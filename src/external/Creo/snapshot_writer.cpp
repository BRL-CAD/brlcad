/**
 *            S N A P S H O T _ W R I T E R . C P P
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
 * @file snapshot_writer.cpp
 *
 * Creo Toolkit capture for neutral conversion requests.
 */

#include "common.h"

#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "creo-brl.h"
#include "conversion_snapshot.h"
#include "snapshot_writer.h"


struct snapshot_settings_strings {
    std::string output_path;
    std::string log_path;
    std::string material_path;
    std::string rename_parameters;
    std::string preserve_parameters;
};


struct snapshot_part_strings {
    std::string model_name;
    std::string model_version;
    std::string material_name;
};


struct snapshot_part_capture {
    struct creo_brl_snapshot_part part;
    struct snapshot_part_strings strings;
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<struct creo_brl_snapshot_vector> normals;
    std::vector<uint32_t> normal_indices;
};


struct snapshot_assembly_capture {
    struct creo_brl_snapshot_assembly assembly;
    std::string model_name;
    std::string model_version;
    std::vector<struct creo_brl_snapshot_assembly_member> members;
};


struct snapshot_scene_capture {
    struct creo_brl_snapshot_settings settings;
    struct snapshot_settings_strings settings_strings;
    std::vector<struct snapshot_part_capture> parts;
    std::vector<struct snapshot_assembly_capture> assemblies;
};

/* The legacy converter keeps this tolerance internal; the dialog has no min_edge control. */
static const double CREO_BRL_DEFAULT_MIN_EDGE_MM = 0.000254;


static_assert(sizeof(struct creo_brl_snapshot_header) == CREO_BRL_SNAPSHOT_HEADER_V1_SIZE,
              "Snapshot header layout changed");
static_assert(sizeof(struct creo_brl_snapshot_settings) == CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE,
              "Snapshot settings layout changed");
static_assert(sizeof(struct creo_brl_snapshot_part) == CREO_BRL_SNAPSHOT_PART_V1_SIZE,
              "Snapshot part layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly_member) ==
                  CREO_BRL_SNAPSHOT_ASSEMBLY_MEMBER_V2_SIZE,
              "Snapshot assembly member layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly) == CREO_BRL_SNAPSHOT_ASSEMBLY_V2_SIZE,
              "Snapshot assembly layout changed");
static_assert(sizeof(struct creo_brl_snapshot_scene) == CREO_BRL_SNAPSHOT_SCENE_V2_SIZE,
              "Snapshot scene layout changed");


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


static int
dialog_text(const char *name, std::string *text)
{
    wchar_t *wide_text = NULL;
    const ProError err = ProUIInputpanelValueGet(CREO_UI_NAME, (char *)name, &wide_text);
    const int converted = err == PRO_TK_NO_ERROR && wide_to_utf8(wide_text, text);

    if (wide_text)
        (void)ProWstringFree(wide_text);

    return converted;
}


static int
dialog_number(const char *name, double *value)
{
    std::string text;
    char *end = NULL;

    if (!dialog_text(name, &text) || text.empty())
        return 0;

    *value = strtod(text.c_str(), &end);
    return end && *end == '\0' && std::isfinite(*value);
}


static int
dialog_integer(const char *name, int32_t *value)
{
    std::string text;
    char *end = NULL;
    const long parsed = dialog_text(name, &text) ? strtol(text.c_str(), &end, 10) : 0;

    if (!end || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX)
        return 0;

    *value = (int32_t)parsed;
    return 1;
}


static int
dialog_check(const char *name, uint32_t flag, uint32_t *flags)
{
    ProBoolean checked = PRO_B_FALSE;

    if (ProUICheckbuttonGetState(CREO_UI_NAME, (char *)name, &checked) != PRO_TK_NO_ERROR)
        return 0;

    if (checked)
        *flags |= flag;

    return 1;
}


static int
dialog_radio(const char *name, std::string *selection)
{
    char **names = NULL;
    int count = 0;
    const ProError err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, (char *)name, &count, &names);

    if (err != PRO_TK_NO_ERROR || count != 1 || !names || !names[0]) {
        if (names)
            ProStringarrayFree(names, count);
        return 0;
    }

    *selection = names[0];
    ProStringarrayFree(names, count);
    return 1;
}


static int
capture_settings(struct creo_brl_snapshot_settings *settings, snapshot_settings_strings *strings)
{
    std::string selection;

    memset(settings, 0, sizeof(*settings));
    settings->structure_size = CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE;

    if (!dialog_text("out_fname", &strings->output_path) || strings->output_path.empty() ||
        !dialog_text("log_fname", &strings->log_path) ||
        !dialog_text("mtl_fname", &strings->material_path) ||
        !dialog_text("param_rename", &strings->rename_parameters) ||
        !dialog_text("param_save", &strings->preserve_parameters) ||
        !dialog_integer("region_counter", &settings->region_id) ||
         !dialog_integer("min_luminance", &settings->min_luminance) ||
         !dialog_number("max_chord", &settings->max_chord) ||
         !dialog_number("min_angle", &settings->min_angle) ||
         !dialog_radio("transform", &selection))
        return 0;

    settings->xform_mode = selection == "x_to_z" ? CREO_BRL_SNAPSHOT_XFORM_X_TO_Z :
                           selection == "y_to_z" ? CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z :
                                                   CREO_BRL_SNAPSHOT_XFORM_NONE;
    if (!dialog_radio("chord_mode", &selection))
        return 0;
    settings->chord_mode = selection == "percent" ? CREO_BRL_SNAPSHOT_CHORD_RELATIVE :
                                                       CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    settings->max_angle = settings->min_angle * 1.5;
    settings->max_facets = 0;
    settings->max_steps = 1;
    settings->log_type = CREO_BRL_SNAPSHOT_LOG_FAILURE;
    settings->min_edge = CREO_BRL_DEFAULT_MIN_EDGE_MM;

    if (!dialog_check("elim_small", CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES,
                      &settings->flags))
        return 0;
    if ((settings->flags & CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES) != 0 &&
        (!dialog_number("min_hole", &settings->min_hole) ||
         !dialog_number("min_chamfer", &settings->min_chamfer) ||
         !dialog_number("min_round", &settings->min_round)))
        return 0;

    return dialog_check("facets_only", CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY, &settings->flags) &&
           dialog_check("export_stl", CREO_BRL_SNAPSHOT_SETTING_EXPORT_STL, &settings->flags) &&
           dialog_check("check_solidity", CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY, &settings->flags) &&
           dialog_check("create_boxes", CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES, &settings->flags) &&
           dialog_check("write_normals", CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS, &settings->flags);
}


static int
model_scale_to_mm(ProMdl model, double *scale)
{
    ProUnitsystem system;
    ProUnititem length;
    char unit_name[PRO_NAME_SIZE + 1] = {0};
    double inches = 0.0;

    if (ProMdlPrincipalunitsystemGet(model, &system) != PRO_TK_NO_ERROR ||
        ProUnitsystemUnitGet(&system, PRO_UNITTYPE_LENGTH, &length) != PRO_TK_NO_ERROR)
        return 0;

    ProWstringToString(unit_name, length.name);
    if (strcmp(unit_name, "ft") == 0)
        inches = 12.0;
    else if (strcmp(unit_name, "in") == 0)
        inches = 1.0;
    else if (strcmp(unit_name, "cm") == 0)
        inches = 1.0 / 2.54;
    else if (strcmp(unit_name, "mm") == 0)
        inches = 1.0 / 25.4;
    else if (strcmp(unit_name, "m") == 0)
        inches = 1.0 / 0.0254;
    else
        return 0;

    *scale = inches * 25.4;
    return 1;
}


static void
set_vector(struct creo_brl_snapshot_vector *destination, const ProVector source, double scale)
{
    destination->coordinates[0] = source[0] * scale;
    destination->coordinates[1] = source[1] * scale;
    destination->coordinates[2] = source[2] * scale;
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


static void
capture_mass_properties(ProMdl model, struct creo_brl_snapshot_part *part)
{
    ProMassProperty mass_properties = {};

    if (ProSolidMassPropertyGet(ProMdlToSolid(model), NULL, &mass_properties) != PRO_TK_NO_ERROR)
        return;

    part->density = mass_properties.density;
    part->mass = mass_properties.mass;
    part->volume = mass_properties.volume;
    part->flags |= CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES;
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


static int
capture_tessellation(ProMdl model, double scale, const struct creo_brl_snapshot_settings *settings,
                     std::vector<struct creo_brl_snapshot_vector> *vertices,
                     std::vector<struct creo_brl_snapshot_triangle> *triangles,
                     std::vector<struct creo_brl_snapshot_vector> *normals,
                     std::vector<uint32_t> *normal_indices)
{
    ProSurfaceTessellationData *tessellation = NULL;
    int surface_count = 0;
    const double chord = settings->max_chord / scale;
    const double angle = settings->min_angle;
    const int write_normals = (settings->flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;

    if (chord <= 0.0 || angle <= 0.0 ||
        ProPartTessellate(ProMdlToPart(model), chord, angle, PRO_B_TRUE, &tessellation) != PRO_TK_NO_ERROR ||
        ProArraySizeGet((ProArray)tessellation, &surface_count) != PRO_TK_NO_ERROR)
        goto failure;

    for (int surface = 0; surface < surface_count; ++surface) {
        const ProSurfaceTessellationData *data = &tessellation[surface];
        for (int facet = 0; facet < data->n_facets; ++facet) {
            struct creo_brl_snapshot_triangle triangle = {};
            const size_t first_vertex = vertices->size();

            if (first_vertex > UINT32_MAX - CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)
                goto failure;

            for (unsigned int corner = 0; corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
                struct creo_brl_snapshot_vector vertex = {};
                const int source_index = data->facets[facet][corner];
                if (source_index < 0 || source_index >= data->n_vertices)
                    goto failure;
                set_vector(&vertex, data->vertices[source_index], scale);
                vertices->push_back(vertex);
                triangle.vertices[corner] = (uint32_t)(first_vertex + corner);

                if (write_normals) {
                    struct creo_brl_snapshot_vector normal = {};
                    set_vector(&normal, data->normals[source_index], 1.0);
                    normals->push_back(normal);
                    normal_indices->push_back((uint32_t)(normals->size() - 1));
                }
            }

            triangles->push_back(triangle);
        }
    }

    (void)ProPartTessellationFree(&tessellation);
    return !vertices->empty();

failure:
    if (tessellation)
        (void)ProPartTessellationFree(&tessellation);
    return 0;
}


static int
snapshot_open_for_write(const char *snapshot_path, FILE **snapshot_file)
{
    const int wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                     snapshot_path, -1, NULL, 0);
    std::vector<wchar_t> wide_path;

    if (wide_path_length <= 0)
        return 0;

    wide_path.resize((size_t)wide_path_length);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, snapshot_path, -1,
                            &wide_path[0], wide_path_length) != wide_path_length)
        return 0;

    *snapshot_file = _wfopen(&wide_path[0], L"wb");
    return *snapshot_file != NULL;
}


static int
write_range(FILE *snapshot_file, const void *data, uint64_t size,
            struct creo_brl_snapshot_range *range)
{
    const __int64 offset = _ftelli64(snapshot_file);

    if (!range)
        return 0;

    if (size == 0) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    if (offset < 0 || fwrite(data, 1, (size_t)size, snapshot_file) != size)
        return 0;

    range->offset = (uint64_t)offset;
    range->size = size;
    return 1;
}


static int
write_text(FILE *snapshot_file, const std::string& text, struct creo_brl_snapshot_range *range)
{
    return write_range(snapshot_file, text.data(), (uint64_t)text.size(), range);
}


static int
capture_model_name(ProMdl model, std::string *model_name)
{
    ProName name;

    return ProMdlMdlnameGet(model, name) == PRO_TK_NO_ERROR &&
           wide_to_utf8(name, model_name) && !model_name->empty();
}


static int
capture_part_definition(ProMdl model,
                        const struct creo_brl_snapshot_settings *settings,
                        struct snapshot_part_capture *capture)
{
    ProName material_name;
    Pro3dPnt bbox[2];
    double scale = 0.0;
    int have_bbox = 0;

    memset(&capture->part, 0, sizeof(capture->part));
    capture->vertices.clear();
    capture->triangles.clear();
    capture->normals.clear();
    capture->normal_indices.clear();
    capture->strings = snapshot_part_strings();

    if (!capture_model_name(model, &capture->strings.model_name) ||
        !model_scale_to_mm(model, &scale))
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    capture->part.structure_size = CREO_BRL_SNAPSHOT_PART_V1_SIZE;
    capture->part.region_id = settings->region_id;
    capture->part.model_to_mm = scale;
    capture_model_version(model, &capture->strings.model_version);
    capture_part_appearance(model, &capture->part);
    capture_mass_properties(model, &capture->part);

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
        capture->part.bbox_diagonal = sqrt(dx * dx + dy * dy + dz * dz);
        capture->part.bbox_volume = dx * dy * dz;
    }

    if (capture_tessellation(model, scale, settings, &capture->vertices, &capture->triangles,
                             &capture->normals, &capture->normal_indices)) {
        capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MESH;
        if (!capture->normals.empty())
            capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
        return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    }
    if (have_bbox && (settings->flags & CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES)) {
        capture->part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK;
        return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_TESSELLATION_FAILURE;
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
    if (scene->parts.size() >= UINT32_MAX)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    result = capture_part_definition(model, &scene->settings, &capture);
    if (result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return result;

    *index_out = (uint32_t)scene->parts.size();
    scene->parts.push_back(capture);
    return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
}


static void
snapshot_identity_matrix(struct creo_brl_snapshot_matrix *matrix)
{
    memset(matrix, 0, sizeof(*matrix));
    matrix->elements[0] = 1.0;
    matrix->elements[5] = 1.0;
    matrix->elements[10] = 1.0;
    matrix->elements[15] = 1.0;
}


static int
capture_component_matrix(ProMdl parent,
                         ProFeature *feature,
                         struct creo_brl_snapshot_matrix *matrix)
{
    ProAsmcomppath component_path;
    ProIdTable component_ids;
    ProMatrix creo_matrix;
    double scale = 0.0;

    snapshot_identity_matrix(matrix);
    if (!model_scale_to_mm(parent, &scale))
        return 0;

    component_ids[0] = feature->id;
    if (ProAsmcomppathInit(ProMdlToSolid(parent), component_ids, 1, &component_path) !=
            PRO_TK_NO_ERROR ||
        ProAsmcomppathTrfGet(&component_path, PRO_B_TRUE, creo_matrix) != PRO_TK_NO_ERROR)
        return 1;

    for (unsigned int column = 0; column < 4; ++column) {
        for (unsigned int row = 0; row < 4; ++row) {
            double value = creo_matrix[row][column];

            if (row == 3 && column < CREO_BRL_SNAPSHOT_COORDINATE_COUNT)
                value *= scale;
            matrix->elements[column * 4 + row] = value;
        }
    }

    return 1;
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


static ProError
snapshot_component_filter(ProFeature *feature, ProAppData *UNUSED(data))
{
    ProFeattype feature_type;
    ProFeatStatus feature_status;

    if (ProFeatureTypeGet(feature, &feature_type) != PRO_TK_NO_ERROR ||
        feature_type != PRO_FEAT_COMPONENT ||
        ProFeatureStatusGet(feature, &feature_status) != PRO_TK_NO_ERROR ||
        feature_status != PRO_FEAT_ACTIVE)
        return PRO_TK_CONTINUE;

    return PRO_TK_NO_ERROR;
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
    int result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (context->result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
        return PRO_TK_NO_ERROR;
    if (ProAsmcompMdlGet(feature, &model) != PRO_TK_NO_ERROR ||
        ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR) {
        context->result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
        return PRO_TK_NO_ERROR;
    }

    ProMdlIsSkeleton(model, &is_skeleton);
    if (is_skeleton)
        return PRO_TK_NO_ERROR;
    if (!capture_component_matrix(context->parent, feature, &member.matrix)) {
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

    if (result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS) {
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

    capture.assembly.structure_size = CREO_BRL_SNAPSHOT_ASSEMBLY_V2_SIZE;
    capture_model_version(model, &capture.model_version);
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

    return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
}


static int
write_settings_strings(FILE *snapshot_file,
                       struct creo_brl_snapshot_settings *settings,
                       const struct snapshot_settings_strings& strings)
{
    return write_text(snapshot_file, strings.output_path, &settings->output_path) &&
           write_text(snapshot_file, strings.log_path, &settings->log_path) &&
           write_text(snapshot_file, strings.material_path, &settings->material_path) &&
           write_text(snapshot_file, strings.rename_parameters, &settings->rename_parameters) &&
           write_text(snapshot_file, strings.preserve_parameters, &settings->preserve_parameters);
}


static int
write_part_capture(FILE *snapshot_file, struct snapshot_part_capture *capture)
{
    capture->part.vertex_count = capture->vertices.size();
    capture->part.triangle_count = capture->triangles.size();
    capture->part.normal_count = capture->normals.size();
    capture->part.normal_index_count = capture->normal_indices.size();

    return write_text(snapshot_file, capture->strings.model_name, &capture->part.model_name) &&
           write_text(snapshot_file, capture->strings.model_version, &capture->part.model_version) &&
           write_text(snapshot_file, capture->strings.material_name, &capture->part.material_name) &&
           write_range(snapshot_file, capture->vertices.data(),
                       capture->vertices.size() * sizeof(capture->vertices[0]),
                       &capture->part.vertices) &&
           write_range(snapshot_file, capture->triangles.data(),
                       capture->triangles.size() * sizeof(capture->triangles[0]),
                       &capture->part.triangles) &&
           write_range(snapshot_file, capture->normals.data(),
                       capture->normals.size() * sizeof(capture->normals[0]),
                       &capture->part.normals) &&
           write_range(snapshot_file, capture->normal_indices.data(),
                       capture->normal_indices.size() * sizeof(capture->normal_indices[0]),
                       &capture->part.normal_indices);
}


static int
finish_snapshot(FILE *snapshot_file, struct creo_brl_snapshot_header *header)
{
    const __int64 end_offset = _ftelli64(snapshot_file);

    if (end_offset < 0)
        return 0;
    header->snapshot_size = (uint64_t)end_offset;
    header->payload_size = header->snapshot_size - header->header_size;
    return header->snapshot_size >= header->header_size;
}


static int
write_single_part_snapshot(const char *snapshot_path,
                           const struct creo_brl_snapshot_settings *settings,
                           const struct snapshot_settings_strings& settings_strings,
                           struct snapshot_part_capture *capture)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_single_part request = {};
    FILE *snapshot_file = NULL;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;

    if (!snapshot_open_for_write(snapshot_path, &snapshot_file))
        return result;

    memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
    header.format_version = CREO_BRL_SNAPSHOT_FORMAT_VERSION_V1;
    header.header_size = CREO_BRL_SNAPSHOT_HEADER_V1_SIZE;
    header.payload_type = CREO_BRL_SNAPSHOT_PAYLOAD_SINGLE_PART;
    request.structure_size = CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE;
    request.settings = *settings;
    request.part = capture->part;

    if (fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&request, 1, sizeof(request), snapshot_file) != sizeof(request) ||
        !write_settings_strings(snapshot_file, &request.settings, settings_strings) ||
        !write_part_capture(snapshot_file, capture) ||
        !finish_snapshot(snapshot_file, &header))
        goto cleanup;

    request.part = capture->part;
    if (_fseeki64(snapshot_file, 0, SEEK_SET) != 0 ||
        fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&request, 1, sizeof(request), snapshot_file) != sizeof(request))
        goto cleanup;

    result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;

cleanup:
    fclose(snapshot_file);
    return result;
}


static int
write_scene_snapshot(const char *snapshot_path, struct snapshot_scene_capture *capture,
                     uint32_t root_type, uint32_t root_index)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_scene scene = {};
    FILE *snapshot_file = NULL;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;

    if (!snapshot_open_for_write(snapshot_path, &snapshot_file))
        return result;

    memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
    header.format_version = CREO_BRL_SNAPSHOT_FORMAT_VERSION_V2;
    header.header_size = CREO_BRL_SNAPSHOT_HEADER_V1_SIZE;
    header.payload_type = CREO_BRL_SNAPSHOT_PAYLOAD_SCENE;
    scene.structure_size = CREO_BRL_SNAPSHOT_SCENE_V2_SIZE;
    scene.settings = capture->settings;
    scene.part_count = capture->parts.size();
    scene.assembly_count = capture->assemblies.size();
    scene.root_type = root_type;
    scene.root_index = root_index;
    if (scene.part_count > 0) {
        scene.parts.offset = header.header_size + sizeof(scene);
        scene.parts.size = scene.part_count * sizeof(struct creo_brl_snapshot_part);
    }
    if (scene.assembly_count > 0) {
        scene.assemblies.offset = header.header_size + sizeof(scene) + scene.parts.size;
        scene.assemblies.size = scene.assembly_count * sizeof(struct creo_brl_snapshot_assembly);
    }

    if (fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&scene, 1, sizeof(scene), snapshot_file) != sizeof(scene))
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (fwrite(&capture->parts[index].part, 1, sizeof(capture->parts[index].part), snapshot_file) !=
            sizeof(capture->parts[index].part))
            goto cleanup;
    }
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        if (fwrite(&capture->assemblies[index].assembly, 1,
                   sizeof(capture->assemblies[index].assembly), snapshot_file) !=
            sizeof(capture->assemblies[index].assembly))
            goto cleanup;
    }
    if (!write_settings_strings(snapshot_file, &scene.settings, capture->settings_strings))
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (!write_part_capture(snapshot_file, &capture->parts[index]))
            goto cleanup;
    }
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        struct snapshot_assembly_capture *assembly = &capture->assemblies[index];

        assembly->assembly.member_count = assembly->members.size();
        if (!write_text(snapshot_file, assembly->model_name, &assembly->assembly.model_name) ||
            !write_text(snapshot_file, assembly->model_version, &assembly->assembly.model_version) ||
            !write_range(snapshot_file, assembly->members.data(),
                         assembly->members.size() * sizeof(assembly->members[0]),
                         &assembly->assembly.members))
            goto cleanup;
    }
    if (!finish_snapshot(snapshot_file, &header) ||
        _fseeki64(snapshot_file, 0, SEEK_SET) != 0 ||
        fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&scene, 1, sizeof(scene), snapshot_file) != sizeof(scene))
        goto cleanup;
    if (scene.part_count > 0 &&
        _fseeki64(snapshot_file, (__int64)scene.parts.offset, SEEK_SET) != 0)
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (fwrite(&capture->parts[index].part, 1, sizeof(capture->parts[index].part), snapshot_file) !=
            sizeof(capture->parts[index].part))
            goto cleanup;
    }
    if (scene.assembly_count > 0 &&
        _fseeki64(snapshot_file, (__int64)scene.assemblies.offset, SEEK_SET) != 0)
        goto cleanup;
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        if (fwrite(&capture->assemblies[index].assembly, 1,
                   sizeof(capture->assemblies[index].assembly), snapshot_file) !=
            sizeof(capture->assemblies[index].assembly))
            goto cleanup;
    }

    result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;

cleanup:
    fclose(snapshot_file);
    return result;
}


extern "C" int
creo_brl_frontend_capture_snapshot(const char *snapshot_path)
{
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    ProMdl model = NULL;
    ProMdlType model_type;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;
    if (!capture_settings(&settings, &settings_strings))
        return CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE;
    if (ProMdlCurrentGet(&model) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_NO_ACTIVE_MODEL;
    if (ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (model_type == PRO_MDL_PART) {
        struct snapshot_part_capture capture;
        const int result = capture_part_definition(model, &settings, &capture);

        return result == CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS ?
                   write_single_part_snapshot(snapshot_path, &settings, settings_strings, &capture) : result;
    }
    if (model_type == PRO_MDL_ASSEMBLY) {
        struct snapshot_scene_capture capture = {};
        uint32_t root_index = 0;
        int result = CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

        capture.settings = settings;
        capture.settings_strings = settings_strings;
        result = capture_scene_assembly(model, &capture, &root_index);
        return result == CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS ?
                   write_scene_snapshot(snapshot_path, &capture,
                                        CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY, root_index) : result;
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;
}
