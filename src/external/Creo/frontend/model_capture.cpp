/**
 *           M O D E L _ C A P T U R E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file frontend/model_capture.cpp
 *
 * Creo Toolkit model capture for neutral conversion requests.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdio.h>
#include <utility>
#include <windows.h>

#include "frontend/csg_capture.h"
#include "frontend/model_capture.h"
#include "shared/mesh_normalization.h"
#include "shared/name_selection.h"


static const size_t CREO_BRL_TESSELLATION_ATTEMPT_COUNT = 20u;
static const size_t CREO_BRL_MAXIMUM_FACET_COUNT = 100000u;
static const double CREO_BRL_MINIMUM_CHORD_FACTOR = 0.1;
static const double CREO_BRL_MAXIMUM_ANGLE_FACTOR = 1.5;


static int
wide_to_utf8(const wchar_t *wide_text, std::string *text)
{
    int size = 0;

    if (!wide_text || !text)
        return 0;

    size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1, NULL, 0, NULL, NULL);
    if (size <= 0)
        return 0;

    text->resize((size_t)size);
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                            &(*text)[0], size, NULL, NULL) != size)
        return 0;

    text->resize((size_t)size - 1);
    return 1;
}


static ProError
append_parameter_to_array(void *parameter_object, ProError filter_error, ProAppData app_data)
{
    ProArray *parameter_array = NULL;

    (void)filter_error;
    parameter_array = (ProArray *)((void **)app_data)[0];
    return ProArrayObjectAdd(parameter_array, PRO_VALUE_UNUSED, 1, parameter_object);
}


static ProError
collect_model_parameters(ProModelitem *model_item, ProParameter **parameters)
{
    ProError error = PRO_TK_GENERAL_ERROR;

    if (!model_item || !parameters)
        return PRO_TK_BAD_INPUTS;

    error = ProArrayAlloc(0, sizeof(ProParameter), 1, (ProArray *)parameters);
    if (error != PRO_TK_NO_ERROR)
        return error;

    error = ProParameterVisit(model_item,
                              NULL,
                              (ProParameterAction)append_parameter_to_array,
                              (ProAppData)&parameters);
    if (error != PRO_TK_NO_ERROR) {
        (void)ProArrayFree((ProArray *)parameters);
        *parameters = NULL;
    }

    return error;
}


static int
capture_parameter_value(ProMdl model, const std::string& parameter_name, std::string *value)
{
    ProModelitem model_item;
    ProParameter parameter;
    ProParamvalue parameter_value;
    ProParamvalueType parameter_type;
    ProUnititem units;
    wchar_t wide_name[CREO_NAME_MAX] = {L'\0'};

    if (!value || parameter_name.empty() || parameter_name.size() >= CREO_NAME_MAX)
        return 0;

    ProStringToWstring(wide_name, (char *)parameter_name.c_str());
    if (ProMdlToModelitem(model, &model_item) != PRO_TK_NO_ERROR ||
        ProParameterInit(&model_item, wide_name, &parameter) != PRO_TK_NO_ERROR ||
        ProParameterValueWithUnitsGet(&parameter, &parameter_value, &units) != PRO_TK_NO_ERROR ||
        ProParamvalueTypeGet(&parameter_value, &parameter_type) != PRO_TK_NO_ERROR)
        return 0;

    switch (parameter_type) {
        case PRO_PARAM_STRING: {
            wchar_t wide_value[CREO_NAME_MAX] = {L'\0'};

            if (ProParamvalueValueGet(&parameter_value, parameter_type, (void *)wide_value) != PRO_TK_NO_ERROR)
                return 0;
            return wide_to_utf8(wide_value, value) && !value->empty();
        }
        case PRO_PARAM_INTEGER: {
            int integer_value = 0;
            char buffer[64] = {'\0'};

            if (ProParamvalueValueGet(&parameter_value, parameter_type, (void *)&integer_value) != PRO_TK_NO_ERROR)
                return 0;
            snprintf(buffer, sizeof(buffer), "%d", integer_value);
            *value = buffer;
            return 1;
        }
        case PRO_PARAM_DOUBLE: {
            double double_value = 0.0;
            char buffer[64] = {'\0'};

            if (ProParamvalueValueGet(&parameter_value, parameter_type, (void *)&double_value) != PRO_TK_NO_ERROR)
                return 0;
            snprintf(buffer, sizeof(buffer), "%.15g", double_value);
            *value = buffer;
            return 1;
        }
        case PRO_PARAM_BOOLEAN: {
            short boolean_value = 0;

            if (ProParamvalueValueGet(&parameter_value, parameter_type, (void *)&boolean_value) != PRO_TK_NO_ERROR)
                return 0;
            *value = boolean_value ? "yes" : "no";
            return 1;
        }
        default:
            break;
    }

    return 0;
}


static const struct snapshot_named_value *
find_captured_parameter(const std::vector<struct snapshot_named_value>& parameters,
                        const std::string& name)
{
    for (size_t index = 0; index < parameters.size(); ++index) {
        if (parameters[index].name == name)
            return &parameters[index];
    }

    return NULL;
}


static int
capture_parameter(ProMdl model,
                  const std::string& parameter_name,
                  std::vector<struct snapshot_named_value> *parameters)
{
    const std::string normalized_name = creo_brl_name_text_lower(parameter_name);
    std::string value;

    if (!parameters || normalized_name.empty())
        return 0;
    if (find_captured_parameter(*parameters, normalized_name))
        return 1;
    if (!capture_parameter_value(model, normalized_name, &value))
        return 0;

    struct snapshot_named_value parameter = {};
    parameter.name = normalized_name;
    parameter.value = value;
    parameters->push_back(parameter);
    return 1;
}


static void
capture_requested_parameters(ProMdl model,
                             const std::string& names_text,
                             std::vector<struct snapshot_named_value> *parameters)
{
    const std::vector<creo_brl_name_selector> selectors =
        creo_brl_parse_name_selectors(names_text);

    if (!parameters)
        return;

    for (size_t index = 0; index < selectors.size(); ++index)
        (void)capture_parameter(model, selectors[index].exact_name(), parameters);
}


static int
collect_model_parameter_array(ProMdl model,
                              ProParameter **collected_parameters,
                              int *count)
{
    ProModelitem model_item;

    if (!collected_parameters || !count ||
        ProMdlToModelitem(model, &model_item) != PRO_TK_NO_ERROR ||
        collect_model_parameters(&model_item, collected_parameters) != PRO_TK_NO_ERROR)
        return 0;

    if (ProArraySizeGet((ProArray)*collected_parameters, count) != PRO_TK_NO_ERROR) {
        (void)ProArrayFree((ProArray *)collected_parameters);
        *collected_parameters = NULL;
        return 0;
    }

    return 1;
}


static int
parameter_has_string_value(ProParameter *parameter)
{
    ProParamvalue value;
    ProParamvalueType type;
    ProUnititem units;

    return parameter &&
           ProParameterValueWithUnitsGet(parameter, &value, &units) ==
               PRO_TK_NO_ERROR &&
           ProParamvalueTypeGet(&value, &type) == PRO_TK_NO_ERROR &&
           type == PRO_PARAM_STRING;
}


static void
capture_rename_parameters(ProMdl model,
                          const std::string& names_text,
                          std::vector<struct snapshot_named_value> *parameters)
{
    const std::vector<creo_brl_name_selector> selectors =
        creo_brl_parse_name_selectors(names_text);
    ProParameter *collected_parameters = NULL;
    int parameter_count = 0;
    int has_pattern = 0;

    if (!parameters)
        return;

    for (size_t selector_index = 0;
         selector_index < selectors.size(); ++selector_index)
        has_pattern = has_pattern || selectors[selector_index].has_pattern();

    if (has_pattern)
        (void)collect_model_parameter_array(model, &collected_parameters,
                                            &parameter_count);

    /* Regex selection depends on Toolkit visitation order.  Capture the
     * relevant candidates in that order so replay can make the same choice. */
    for (int parameter_index = 0;
         parameter_index < parameter_count; ++parameter_index) {
        char parameter_name[CREO_NAME_MAX] = {'\0'};
        int should_capture = 0;

        ProWstringToString(parameter_name,
                           collected_parameters[parameter_index].id);
        for (size_t selector_index = 0;
             selector_index < selectors.size(); ++selector_index) {
            const creo_brl_name_selector& selector = selectors[selector_index];

            if (selector.matches_exact(parameter_name) ||
                (selector.matches_pattern(parameter_name) &&
                 parameter_has_string_value(&collected_parameters[parameter_index]))) {
                should_capture = 1;
                break;
            }
        }
        if (should_capture)
            (void)capture_parameter(model, parameter_name, parameters);
    }

    if (collected_parameters)
        (void)ProArrayFree((ProArray *)&collected_parameters);

    /* Direct lookup remains authoritative and also covers Toolkit builds
     * where parameter visitation cannot enumerate the requested value. */
    for (size_t selector_index = 0;
         selector_index < selectors.size(); ++selector_index) {
        if (!find_captured_parameter(*parameters,
                                     selectors[selector_index].exact_name())) {
            (void)capture_parameter(model, selectors[selector_index].exact_name(),
                                    parameters);
        }
    }
}


static void
capture_all_parameters(ProMdl model,
                       std::vector<struct snapshot_named_value> *parameters)
{
    ProParameter *collected_parameters = NULL;
    int count = 0;

    if (!parameters ||
        !collect_model_parameter_array(model, &collected_parameters, &count))
        return;

    for (int index = 0; index < count; ++index) {
        char parameter_name[CREO_NAME_MAX] = {'\0'};

        ProWstringToString(parameter_name, collected_parameters[index].id);
        (void)capture_parameter(model, parameter_name, parameters);
    }

    (void)ProArrayFree((ProArray *)&collected_parameters);
}


static void
capture_selected_parameters(ProMdl model,
                            const struct snapshot_settings_strings *settings,
                            std::vector<struct snapshot_named_value> *parameters)
{
    if (!settings || !parameters)
        return;

    capture_rename_parameters(model, settings->rename_parameters, parameters);
    if (creo_brl_name_text_trim(settings->preserve_parameters).empty())
        capture_all_parameters(model, parameters);
    else
        capture_requested_parameters(model, settings->preserve_parameters, parameters);
}


int
creo_brl_model_scale_to_mm(ProMdl model, double *scale)
{
    ProUnitsystem system;
    ProUnititem model_length_unit;
    ProUnititem millimeter_unit;
    ProUnitConversion conversion = {};
    ProName millimeter_name = L"mm";

    if (!model || !scale ||
        ProMdlPrincipalunitsystemGet(model, &system) != PRO_TK_NO_ERROR ||
        ProUnitsystemUnitGet(&system, PRO_UNITTYPE_LENGTH, &model_length_unit) !=
            PRO_TK_NO_ERROR ||
        ProUnitInit(model, millimeter_name, &millimeter_unit) != PRO_TK_NO_ERROR ||
        ProUnitConversionCalculate(&model_length_unit, &millimeter_unit,
                                   &conversion) != PRO_TK_NO_ERROR ||
        !std::isfinite(conversion.scale) || conversion.scale <= 0.0)
        return 0;

    *scale = conversion.scale;
    return 1;
}


int
creo_brl_component_matrix_to_mm(const ProMatrix creo_matrix,
                                double parent_to_mm,
                                double child_to_mm,
                                struct creo_brl_snapshot_matrix *matrix)
{
    if (!matrix || !std::isfinite(parent_to_mm) || parent_to_mm <= 0.0 ||
        !std::isfinite(child_to_mm) || child_to_mm <= 0.0)
        return 0;

    for (size_t column = 0; column < 4u; ++column) {
        for (size_t row = 0; row < 4u; ++row) {
            double value = creo_matrix[row][column];

            if (!std::isfinite(value))
                return 0;
            if (row < CREO_BRL_SNAPSHOT_COORDINATE_COUNT &&
                column < CREO_BRL_SNAPSHOT_COORDINATE_COUNT)
                value *= parent_to_mm / child_to_mm;
            else if (row == CREO_BRL_SNAPSHOT_COORDINATE_COUNT &&
                     column < CREO_BRL_SNAPSHOT_COORDINATE_COUNT)
                value *= parent_to_mm;

            matrix->elements[column * 4u + row] = value;
        }
    }

    return 1;
}


static unsigned char
snapshot_color_component(double component)
{
    static const unsigned char COLOR_COMPONENT_MAX = 255;

    if (!std::isfinite(component) || component <= 0.0)
        return 0;
    if (component >= 1.0)
        return COLOR_COMPONENT_MAX;

    return (unsigned char)(component * COLOR_COMPONENT_MAX + 0.5);
}


static void
capture_part_appearance(ProMdl model, struct creo_brl_snapshot_part *part)
{
    ProModelitem model_item;
    ProSurfaceAppearanceProps appearance;
    const int surface_side = 0;

    if (ProMdlToModelitem(model, &model_item) != PRO_TK_NO_ERROR ||
        ProSurfaceSideAppearancepropsGet(&model_item, surface_side, &appearance) != PRO_TK_NO_ERROR)
        return;

    for (size_t component = 0; component < CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT; ++component)
        part->color[component] = snapshot_color_component(appearance.color_rgb[component]);

    part->color_is_set = 1;
    part->flags |= CREO_BRL_SNAPSHOT_PART_HAS_COLOR;
}


static int
store_mass_properties(const ProMassProperty *mass_properties,
                      uint32_t mass_properties_flag,
                      uint32_t *flags,
                      double *density,
                      double *mass,
                      double *volume,
                      double *surface_area)
{
    if (!mass_properties || !flags || !density || !mass || !volume || !surface_area ||
        !std::isfinite(mass_properties->density) ||
        !std::isfinite(mass_properties->mass) ||
        !std::isfinite(mass_properties->volume) ||
        !std::isfinite(mass_properties->surface_area))
        return 0;

    *density = mass_properties->density;
    *mass = mass_properties->mass;
    *volume = mass_properties->volume;
    *surface_area = mass_properties->surface_area;
    *flags |= mass_properties_flag;
    return 1;
}


static void
capture_assembly_mass_properties(ProMdl model,
                                 struct snapshot_assembly_capture *capture)
{
    ProMassProperty mass_properties = {};

    if (!capture ||
        ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &mass_properties) != PRO_TK_NO_ERROR)
        return;

    (void)store_mass_properties(
        &mass_properties, CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES,
        &capture->assembly.flags, &capture->assembly.density,
        &capture->assembly.mass, &capture->assembly.volume,
        &capture->assembly.surface_area);
}


static void
capture_part_mass_properties(ProMdl model, struct creo_brl_snapshot_part *part)
{
    ProMassProperty mass_properties = {};

    if (!part ||
        ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &mass_properties) != PRO_TK_NO_ERROR)
        return;

    store_mass_properties(&mass_properties,
                          CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES,
                          &part->flags, &part->density, &part->mass,
                          &part->volume, &part->surface_area);
}


static void
capture_model_version(ProMdl model, std::string *version)
{
    ProWVerstamp stamp;
    char *stamp_text = NULL;

    if (ProMdlVerstampGet(model, &stamp) != PRO_TK_NO_ERROR ||
        ProVerstampStringGet(stamp, &stamp_text) != PRO_TK_NO_ERROR || !stamp_text)
        return;

    *version = stamp_text;
    ProVerstampStringFree(&stamp_text);
}


int
creo_brl_build_tessellation_policy(const struct creo_brl_snapshot_settings *settings,
                                    double model_to_mm,
                                    double bbox_diagonal_mm,
                                    struct snapshot_tessellation_policy *policy)
{
    double maximum_chord = 0.0;
    double minimum_chord = 0.0;
    double maximum_angle = 0.0;

    if (!policy || !creo_brl_validate_snapshot_settings(settings) ||
        !std::isfinite(model_to_mm) || model_to_mm <= 0.0)
        return 0;

    if (settings->chord_mode == CREO_BRL_SNAPSHOT_CHORD_RELATIVE) {
        if (!std::isfinite(bbox_diagonal_mm) || bbox_diagonal_mm <= 0.0)
            return 0;
        maximum_chord = bbox_diagonal_mm * settings->max_chord /
                        (CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM * model_to_mm);
    } else {
        maximum_chord = settings->max_chord / model_to_mm;
    }

    minimum_chord = maximum_chord * CREO_BRL_MINIMUM_CHORD_FACTOR;
    maximum_angle = (std::min)(CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM,
                               settings->min_angle * CREO_BRL_MAXIMUM_ANGLE_FACTOR);
    if (!std::isfinite(maximum_chord) || maximum_chord <= 0.0 ||
        !std::isfinite(minimum_chord) || minimum_chord <= 0.0)
        return 0;

    policy->attempts.clear();
    policy->attempts.reserve(CREO_BRL_TESSELLATION_ATTEMPT_COUNT);
    policy->maximum_facet_count = CREO_BRL_MAXIMUM_FACET_COUNT;
    for (size_t index = 0; index < CREO_BRL_TESSELLATION_ATTEMPT_COUNT; ++index) {
        const double fraction = CREO_BRL_TESSELLATION_ATTEMPT_COUNT > 1u ?
                                    static_cast<double>(index) /
                                        static_cast<double>(CREO_BRL_TESSELLATION_ATTEMPT_COUNT - 1u) :
                                    0.0;
        struct snapshot_tessellation_attempt attempt = {};

        attempt.chord = maximum_chord - (maximum_chord - minimum_chord) * fraction;
        attempt.angle = settings->min_angle + (maximum_angle - settings->min_angle) * fraction;
        policy->attempts.push_back(attempt);
    }

    return 1;
}


int
creo_brl_capture_tessellation_data(const ProSurfaceTessellationData *tessellation,
                                    int surface_count,
                                    double model_to_mm,
                                    int write_normals,
                                    size_t maximum_facet_count,
                                    struct snapshot_mesh_capture *mesh)
{
    std::vector<struct creo_brl_snapshot_vector> source_vertices;
    std::vector<struct creo_brl_snapshot_triangle> source_triangles;
    std::vector<struct creo_brl_snapshot_vector> source_normals;
    struct creo_brl_mesh_normalization normalized = {};
    struct snapshot_mesh_capture captured = {};
    size_t facet_count = 0u;

    if (!mesh)
        return 0;
    *mesh = snapshot_mesh_capture();
    if (!tessellation || surface_count <= 0 || !std::isfinite(model_to_mm) ||
        model_to_mm <= 0.0 || maximum_facet_count == 0u)
        return 0;

    for (int surface = 0; surface < surface_count; ++surface) {
        const ProSurfaceTessellationData& data = tessellation[surface];

        if (data.n_vertices < 0 || data.n_facets < 0 ||
            static_cast<size_t>(data.n_facets) > maximum_facet_count - facet_count ||
            (data.n_facets > 0 && (!data.vertices || !data.facets ||
                                   (write_normals && !data.normals))))
            return 0;
        facet_count += static_cast<size_t>(data.n_facets);
    }
    if (facet_count == 0u)
        return 0;
    if (facet_count > source_triangles.max_size() ||
        facet_count > source_vertices.max_size() /
                          CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT ||
        (write_normals &&
         facet_count > source_normals.max_size() /
                           CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT))
        return 0;

    source_vertices.reserve(facet_count * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);
    source_triangles.reserve(facet_count);
    if (write_normals)
        source_normals.reserve(facet_count * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);

    for (int surface = 0; surface < surface_count; ++surface) {
        const ProSurfaceTessellationData& data = tessellation[surface];

        for (int facet = 0; facet < data.n_facets; ++facet) {
            struct creo_brl_snapshot_triangle triangle = {};

            for (size_t corner = 0; corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
                const int source_index = data.facets[facet][corner];
                struct creo_brl_snapshot_vector vertex = {};

                if (source_index < 0 || source_index >= data.n_vertices)
                    return 0;
                for (size_t coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT;
                     ++coordinate) {
                    vertex.coordinates[coordinate] =
                        data.vertices[source_index][coordinate] * model_to_mm;
                    if (!std::isfinite(vertex.coordinates[coordinate]))
                        return 0;
                }

                if (source_vertices.size() >= UINT32_MAX)
                    return 0;
                triangle.vertices[corner] =
                    static_cast<uint32_t>(source_vertices.size());
                source_vertices.push_back(vertex);

                if (write_normals) {
                    struct creo_brl_snapshot_vector normal = {};

                    for (size_t coordinate = 0;
                         coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
                        normal.coordinates[coordinate] = data.normals[source_index][coordinate];
                        if (!std::isfinite(normal.coordinates[coordinate]))
                            return 0;
                    }
                    source_normals.push_back(normal);
                }
            }
            source_triangles.push_back(triangle);
        }
    }

    if (!creo_brl_normalize_mesh(source_vertices, source_triangles, &normalized))
        return 0;

    captured.vertices = std::move(normalized.vertices);
    captured.triangles = std::move(normalized.triangles);
    captured.is_solid = normalized.is_solid;
    if (write_normals) {
        if (normalized.source_triangle_indices.size() >
            captured.normals.max_size() /
                CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)
            return 0;
        captured.normals.reserve(normalized.source_triangle_indices.size() *
                                 CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);
        captured.normal_indices.reserve(normalized.source_triangle_indices.size() *
                                        CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);
        for (size_t accepted_index = 0;
             accepted_index < normalized.source_triangle_indices.size(); ++accepted_index) {
            const struct creo_brl_snapshot_triangle& source_triangle =
                source_triangles[normalized.source_triangle_indices[accepted_index]];

            for (size_t corner = 0;
                 corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
                if (captured.normals.size() >= UINT32_MAX)
                    return 0;
                captured.normal_indices.push_back(
                    static_cast<uint32_t>(captured.normals.size()));
                captured.normals.push_back(source_normals[source_triangle.vertices[corner]]);
            }
        }
    }

    *mesh = std::move(captured);
    return 1;
}


int
creo_brl_run_tessellation_policy(const struct snapshot_tessellation_policy *policy,
                                  int require_solidity,
                                  snapshot_tessellation_attempt_fn capture_attempt,
                                  void *context,
                                  struct snapshot_mesh_capture *mesh)
{
    if (!mesh)
        return 0;
    *mesh = snapshot_mesh_capture();
    if (!policy || policy->attempts.empty() || policy->maximum_facet_count == 0u ||
        !capture_attempt)
        return 0;

    for (size_t index = 0; index < policy->attempts.size(); ++index) {
        struct snapshot_mesh_capture attempt_mesh = {};

        if (!capture_attempt(context, &policy->attempts[index],
                             policy->maximum_facet_count, &attempt_mesh) ||
            attempt_mesh.vertices.empty() || attempt_mesh.triangles.empty() ||
            attempt_mesh.triangles.size() > policy->maximum_facet_count ||
            (require_solidity && !attempt_mesh.is_solid))
            continue;

        *mesh = std::move(attempt_mesh);
        return 1;
    }

    return 0;
}


struct toolkit_tessellation_context {
    ProMdl model;
    double model_to_mm;
    int write_normals;
};


static int
capture_toolkit_tessellation_attempt(void *context,
                                     const struct snapshot_tessellation_attempt *attempt,
                                     size_t maximum_facet_count,
                                     struct snapshot_mesh_capture *mesh)
{
    struct toolkit_tessellation_context *toolkit =
        static_cast<struct toolkit_tessellation_context *>(context);
    ProSurfaceTessellationData *tessellation = NULL;
    int surface_count = 0;
    int result = 0;

    if (!toolkit || !toolkit->model || !attempt || !mesh)
        return 0;

    if (ProPartTessellate(ProMdlToPart(toolkit->model), attempt->chord,
                          attempt->angle, PRO_B_TRUE, &tessellation) == PRO_TK_NO_ERROR &&
        tessellation &&
        ProArraySizeGet((ProArray)tessellation, &surface_count) == PRO_TK_NO_ERROR) {
        result = creo_brl_capture_tessellation_data(
            tessellation, surface_count, toolkit->model_to_mm,
            toolkit->write_normals, maximum_facet_count, mesh);
    }

    if (tessellation)
        (void)ProPartTessellationFree(&tessellation);
    return result;
}


static int
capture_tessellation(ProMdl model,
                     double model_to_mm,
                     double bbox_diagonal_mm,
                     const struct creo_brl_snapshot_settings *settings,
                     struct snapshot_part_capture *capture)
{
    struct snapshot_tessellation_policy policy;
    struct snapshot_mesh_capture mesh = {};
    struct toolkit_tessellation_context context = {};

    if (!capture)
        return 0;
    capture->vertices.clear();
    capture->triangles.clear();
    capture->normals.clear();
    capture->normal_indices.clear();
    if (!creo_brl_build_tessellation_policy(settings, model_to_mm,
                                             bbox_diagonal_mm, &policy))
        return 0;

    context.model = model;
    context.model_to_mm = model_to_mm;
    context.write_normals =
        (settings->flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;
    if (!creo_brl_run_tessellation_policy(
            &policy,
            (settings->flags & CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY) != 0,
            capture_toolkit_tessellation_attempt, &context, &mesh))
        return 0;

    capture->vertices.swap(mesh.vertices);
    capture->triangles.swap(mesh.triangles);
    capture->normals.swap(mesh.normals);
    capture->normal_indices.swap(mesh.normal_indices);
    return 1;
}


static void
mark_bbox_fallback(struct snapshot_part_capture *capture)
{
    if (!capture)
        return;

    capture->vertices.clear();
    capture->triangles.clear();
    capture->normals.clear();
    capture->normal_indices.clear();
    capture->part.flags &= ~(CREO_BRL_SNAPSHOT_PART_HAS_MESH |
                             CREO_BRL_SNAPSHOT_PART_HAS_NORMALS);
    capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK;
}


static int
capture_model_name(ProMdl model, std::string *model_name)
{
    ProName name;

    return ProMdlMdlnameGet(model, name) == PRO_TK_NO_ERROR &&
           wide_to_utf8(name, model_name) && !model_name->empty();
}


int
creo_brl_capture_part_definition(ProMdl model,
                                 const struct creo_brl_snapshot_settings *settings,
                                 const struct snapshot_settings_strings *settings_strings,
                                 struct snapshot_part_capture *capture)
{
    ProName material_name;
    Pro3dPnt bbox[2];
    double scale = 0.0;
    int have_bbox = 0;
    int suppression_result = CREO_BRL_FEATURE_SUPPRESSION_CAPTURE_FAILED;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    creo_brl_scoped_feature_suppression suppression;

    if (!settings || !capture)
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;

    memset(&capture->part, 0, sizeof(capture->part));
    capture->vertices.clear();
    capture->triangles.clear();
    capture->normals.clear();
    capture->normal_indices.clear();
    capture->attributes.clear();
    capture->parameters.clear();
    capture->tgc_subtractions.clear();
    capture->strings = snapshot_part_strings();

    if (!capture_model_name(model, &capture->strings.model_name) ||
        !creo_brl_model_scale_to_mm(model, &scale))
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    capture_model_version(model, &capture->strings.model_version);
    capture_selected_parameters(model, settings_strings, &capture->parameters);

    capture_part_appearance(model, &capture->part);
    capture_part_mass_properties(model, &capture->part);

    if (ProPartMaterialNameGet(ProMdlToPart(model), material_name) == PRO_TK_NO_ERROR &&
        wide_to_utf8(material_name, &capture->strings.material_name) &&
        !capture->strings.material_name.empty())
        capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL;
    if ((capture->part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES) == 0 &&
        ProPartDensityGet(ProMdlToPart(model), &capture->part.density) != PRO_TK_NO_ERROR)
        capture->part.density = 0.0;

    have_bbox = ProSolidOutlineGet(ProMdlToSolid(model), bbox) == PRO_TK_NO_ERROR;
    if (have_bbox) {
        for (unsigned int coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            capture->part.bbox_min.coordinates[coordinate] = bbox[0][coordinate] * scale;
            capture->part.bbox_max.coordinates[coordinate] = bbox[1][coordinate] * scale;
        }
        const double dx = capture->part.bbox_max.coordinates[0] - capture->part.bbox_min.coordinates[0];
        const double dy = capture->part.bbox_max.coordinates[1] - capture->part.bbox_min.coordinates[1];
        const double dz = capture->part.bbox_max.coordinates[2] - capture->part.bbox_min.coordinates[2];

        capture->part.bbox_area = 2.0 * (dx * dy + dx * dz + dy * dz);
        capture->part.bbox_diagonal = DIST_PNT_PNT(capture->part.bbox_min.coordinates,
                                                   capture->part.bbox_max.coordinates);
        capture->part.bbox_volume = dx * dy * dz;
    }

    suppression_result = suppression.begin(model, scale, settings, capture);
    if (suppression_result == CREO_BRL_FEATURE_SUPPRESSION_RESTORE_FAILED)
        return CREO_BRL_SNAPSHOT_CAPTURE_RESTORATION_FAILURE;
    if (suppression_result == CREO_BRL_FEATURE_SUPPRESSION_CAPTURE_FAILED)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    try {
        if (capture_tessellation(model, scale, capture->part.bbox_diagonal,
                                 settings, capture)) {
            capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MESH;
            if (!capture->normals.empty())
                capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
            result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
        } else if (have_bbox &&
                   (settings->flags & CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES)) {
            mark_bbox_fallback(capture);
            result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
        } else {
            result = CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED;
        }
    } catch (...) {
        result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    }

    if (!suppression.finish())
        return CREO_BRL_SNAPSHOT_CAPTURE_RESTORATION_FAILURE;

    return result;
}


static int
scene_part_index(const struct snapshot_scene_capture *scene,
                 const std::string& model_name,
                 uint32_t *index_out)
{
    for (size_t index = 0; index < scene->parts.size(); ++index) {
        if (scene->parts[index].strings.model_name == model_name) {
            *index_out = (uint32_t)index;
            return 1;
        }
    }

    return 0;
}


int
creo_brl_commit_part_capture(struct snapshot_scene_capture *scene,
                             struct snapshot_part_capture *capture,
                             uint32_t *index_out)
{
    if (!scene || !capture || !index_out || capture->strings.model_name.empty())
        return CREO_BRL_SCENE_PART_COMMIT_ERROR;
    if (scene_part_index(scene, capture->strings.model_name, index_out))
        return CREO_BRL_SCENE_PART_REUSED;
    if (scene->parts.size() >= UINT32_MAX)
        return CREO_BRL_SCENE_PART_COMMIT_ERROR;

    if (scene->next_region_id < 1)
        scene->next_region_id = 1;
    if (scene->next_region_id > (std::numeric_limits<int32_t>::max)())
        return CREO_BRL_SCENE_PART_COMMIT_ERROR;

    capture->part.region_id = static_cast<int32_t>(scene->next_region_id);
    ++scene->next_region_id;
    *index_out = static_cast<uint32_t>(scene->parts.size());
    scene->parts.push_back(std::move(*capture));
    return CREO_BRL_SCENE_PART_ADDED;
}


static int
scene_assembly_index(const struct snapshot_scene_capture *scene,
                     const std::string& model_name,
                     uint32_t *index_out)
{
    for (size_t index = 0; index < scene->assemblies.size(); ++index) {
        if (scene->assemblies[index].model_name == model_name) {
            *index_out = (uint32_t)index;
            return 1;
        }
    }

    return 0;
}


static int
capture_scene_part(ProMdl model, struct snapshot_scene_capture *scene, uint32_t *index_out)
{
    std::string model_name;
    struct snapshot_part_capture capture;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (!capture_model_name(model, &model_name))
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    if (scene_part_index(scene, model_name, index_out))
        return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    result = creo_brl_capture_part_definition(model, &scene->settings, &scene->settings_strings, &capture);
    if (result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return result;

    return creo_brl_commit_part_capture(scene, &capture, index_out) ==
                   CREO_BRL_SCENE_PART_ADDED ?
               CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS :
               CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
}


static int
capture_component_matrix(ProMdl parent,
                         ProMdl child,
                         ProFeature *feature,
                         struct creo_brl_snapshot_matrix *matrix)
{
    ProAsmcomppath component_path;
    ProIdTable component_ids;
    ProMatrix creo_matrix;
    double parent_to_mm = 0.0;
    double child_to_mm = 0.0;

    if (!parent || !child || !feature || !matrix ||
        !creo_brl_model_scale_to_mm(parent, &parent_to_mm) ||
        !creo_brl_model_scale_to_mm(child, &child_to_mm))
        return 0;

    component_ids[0] = feature->id;
    if (ProAsmcomppathInit(ProMdlToSolid(parent), component_ids, 1, &component_path) !=
            PRO_TK_NO_ERROR ||
        ProAsmcomppathTrfGet(&component_path, PRO_B_TRUE, creo_matrix) != PRO_TK_NO_ERROR)
        return 0;

    return creo_brl_component_matrix_to_mm(creo_matrix, parent_to_mm,
                                            child_to_mm, matrix);
}


static int capture_scene_assembly(ProMdl model,
                                  struct snapshot_scene_capture *scene,
                                  uint32_t *index_out);


struct snapshot_component_context {
    struct snapshot_scene_capture *scene;
    ProMdl parent;
    uint32_t assembly_index;
    int result;
};


enum capture_child_disposition {
    CAPTURE_CHILD_FATAL = -1,
    CAPTURE_CHILD_SKIP = 0,
    CAPTURE_CHILD_APPEND = 1
};


static int
capture_child_result(int capture_result)
{
    if (capture_result == CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return CAPTURE_CHILD_APPEND;
    if (capture_result == CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED)
        return CAPTURE_CHILD_SKIP;

    return CAPTURE_CHILD_FATAL;
}


static int
finalize_assembly_capture(struct snapshot_scene_capture *scene,
                          uint32_t assembly_index)
{
    if (!scene || assembly_index >= scene->assemblies.size())
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    if (!scene->assemblies[assembly_index].members.empty())
        return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    if (assembly_index + 1u != scene->assemblies.size())
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    scene->assemblies.pop_back();
    return CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED;
}


static ProError
snapshot_component_filter(ProFeature *feature, ProAppData UNUSED(data))
{
    ProFeattype feature_type;
    ProFeatStatus feature_status;
    ProError error = PRO_TK_GENERAL_ERROR;

    if (!feature)
        return PRO_TK_BAD_INPUTS;
    error = ProFeatureTypeGet(feature, &feature_type);
    if (error != PRO_TK_NO_ERROR)
        return error;
    if (feature_type != PRO_FEAT_COMPONENT)
        return PRO_TK_CONTINUE;
    error = ProFeatureStatusGet(feature, &feature_status);
    if (error != PRO_TK_NO_ERROR)
        return error;

    return feature_status == PRO_FEAT_ACTIVE ? PRO_TK_NO_ERROR : PRO_TK_CONTINUE;
}


static ProError
capture_scene_component(ProFeature *feature, ProError UNUSED(status), ProAppData app_data)
{
    struct snapshot_component_context *context =
        (struct snapshot_component_context *)app_data;
    ProMdl model = NULL;
    ProMdlType model_type;
    ProBoolean is_skeleton = PRO_B_FALSE;
    struct creo_brl_snapshot_assembly_member member = {};
    int disposition = CAPTURE_CHILD_FATAL;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (context->result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return PRO_TK_NO_ERROR;
    if (ProAsmcompMdlGet(feature, &model) != PRO_TK_NO_ERROR ||
        ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR) {
        context->result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
        return PRO_TK_NO_ERROR;
    }

    ProMdlIsSkeleton(model, &is_skeleton);
    if (is_skeleton != PRO_B_FALSE)
        return PRO_TK_NO_ERROR;
    if (!capture_component_matrix(context->parent, model, feature, &member.matrix)) {
        context->result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
        return PRO_TK_NO_ERROR;
    }

    if (model_type == PRO_MDL_PART) {
        member.target_type = CREO_BRL_SNAPSHOT_SCENE_NODE_PART;
        result = capture_scene_part(model, context->scene, &member.target_index);
    } else if (model_type == PRO_MDL_ASSEMBLY) {
        member.target_type = CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY;
        result = capture_scene_assembly(model, context->scene, &member.target_index);
    } else {
        return PRO_TK_NO_ERROR;
    }

    disposition = capture_child_result(result);
    if (disposition == CAPTURE_CHILD_SKIP)
        return PRO_TK_NO_ERROR;
    if (disposition == CAPTURE_CHILD_FATAL) {
        context->result = result;
        return PRO_TK_NO_ERROR;
    }

    context->scene->assemblies[context->assembly_index].members.push_back(member);
    return PRO_TK_NO_ERROR;
}


static int
capture_scene_assembly(ProMdl model,
                       struct snapshot_scene_capture *scene,
                       uint32_t *index_out)
{
    struct snapshot_assembly_capture capture = {};
    struct snapshot_component_context context = {};

    if (!capture_model_name(model, &capture.model_name))
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    if (scene_assembly_index(scene, capture.model_name, index_out))
        return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    if (scene->assemblies.size() >= UINT32_MAX)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    capture_model_version(model, &capture.model_version);
    capture_selected_parameters(model, &scene->settings_strings, &capture.parameters);
    capture_assembly_mass_properties(model, &capture);
    *index_out = (uint32_t)scene->assemblies.size();
    scene->assemblies.push_back(capture);

    context.scene = scene;
    context.parent = model;
    context.assembly_index = *index_out;
    context.result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    if (ProSolidFeatVisit(ProMdlToPart(model), capture_scene_component,
                          (ProFeatureFilterAction)snapshot_component_filter,
                          (ProAppData)&context) != PRO_TK_NO_ERROR ||
        context.result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return context.result == CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS ?
                   CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE : context.result;

    return finalize_assembly_capture(scene, *index_out);
}


int
creo_brl_frontend_capture_model_scene(ProMdl model,
                                      ProMdlType model_type,
                                      const struct creo_brl_snapshot_settings *settings,
                                      const struct snapshot_settings_strings *settings_strings,
                                      struct snapshot_scene_capture *capture)
{
    if (!model || !settings || !settings_strings || !capture)
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;

    capture->settings = *settings;
    capture->settings_strings = *settings_strings;
    capture->next_region_id = settings->region_id > 0 ? settings->region_id : 1;

    if (model_type == PRO_MDL_PART) {
        capture->root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_PART;
        const int result = capture_scene_part(model, capture, &capture->root_index);

        return result == CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED ?
                   CREO_BRL_SNAPSHOT_CAPTURE_EMPTY_MODEL : result;
    }
    if (model_type == PRO_MDL_ASSEMBLY) {
        capture->root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY;
        const int result = capture_scene_assembly(model, capture, &capture->root_index);

        return result == CREO_BRL_SNAPSHOT_CAPTURE_SKIPPED ?
                   CREO_BRL_SNAPSHOT_CAPTURE_EMPTY_MODEL : result;
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;
}
