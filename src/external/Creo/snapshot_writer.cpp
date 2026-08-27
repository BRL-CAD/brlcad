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
 * Creo Toolkit capture for the first neutral, single-part request.
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


struct snapshot_strings {
    std::string output_path;
    std::string log_path;
    std::string material_path;
    std::string rename_parameters;
    std::string preserve_parameters;
    std::string model_name;
    std::string model_version;
    std::string material_name;
};


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
capture_settings(struct creo_brl_snapshot_settings *settings, snapshot_strings *strings)
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
        !dialog_number("min_edge", &settings->min_edge) ||
        !dialog_number("min_hole", &settings->min_hole) ||
        !dialog_number("min_chamfer", &settings->min_chamfer) ||
        !dialog_number("min_round", &settings->min_round) ||
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

    return dialog_check("elim_small", CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES, &settings->flags) &&
           dialog_check("facets_only", CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY, &settings->flags) &&
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


extern "C" int
creo_brl_frontend_capture_single_part_snapshot(const char *snapshot_path)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_single_part request = {};
    snapshot_strings strings;
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<struct creo_brl_snapshot_vector> normals;
    std::vector<uint32_t> normal_indices;
    ProMdl model = NULL;
    ProMdlType model_type;
    ProName model_name;
    ProName material_name;
    Pro3dPnt bbox[2];
    double scale = 0.0;
    int have_bbox = 0;
    FILE *snapshot_file = NULL;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;

    if (!capture_settings(&request.settings, &strings))
        return CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE;

    if (ProMdlCurrentGet(&model) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_NO_ACTIVE_MODEL;
    if (ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;
    if (model_type != PRO_MDL_PART)
        return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;

    if (
        ProMdlMdlnameGet(model, model_name) != PRO_TK_NO_ERROR ||
        !wide_to_utf8(model_name, &strings.model_name) ||
        strings.model_name.empty() ||
        !model_scale_to_mm(model, &scale))
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    request.structure_size = CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE;
    request.settings.structure_size = CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE;
    request.part.structure_size = CREO_BRL_SNAPSHOT_PART_V1_SIZE;
    request.part.region_id = request.settings.region_id;
    request.part.model_to_mm = scale;

    capture_model_version(model, &strings.model_version);
    capture_part_appearance(model, &request.part);
    capture_mass_properties(model, &request.part);

    if (ProPartMaterialNameGet(ProMdlToPart(model), material_name) == PRO_TK_NO_ERROR &&
        wide_to_utf8(material_name, &strings.material_name) && !strings.material_name.empty())
        request.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL;
    if ((request.part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES) == 0 &&
        ProPartDensityGet(ProMdlToPart(model), &request.part.density) != PRO_TK_NO_ERROR)
        request.part.density = 0.0;

    have_bbox = ProSolidOutlineGet(ProMdlToSolid(model), bbox) == PRO_TK_NO_ERROR;
    if (have_bbox) {
        for (unsigned int coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            request.part.bbox_min.coordinates[coordinate] = bbox[0][coordinate] * scale;
            request.part.bbox_max.coordinates[coordinate] = bbox[1][coordinate] * scale;
        }
        const double dx = request.part.bbox_max.coordinates[0] - request.part.bbox_min.coordinates[0];
        const double dy = request.part.bbox_max.coordinates[1] - request.part.bbox_min.coordinates[1];
        const double dz = request.part.bbox_max.coordinates[2] - request.part.bbox_min.coordinates[2];

        request.part.bbox_area = 2.0 * (dx * dy + dx * dz + dy * dz);
        request.part.bbox_diagonal = sqrt(dx * dx + dy * dy + dz * dz);
        request.part.bbox_volume = dx * dy * dz;
    }

    if (capture_tessellation(model, scale, &request.settings, &vertices, &triangles,
                             &normals, &normal_indices)) {
        request.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MESH;
        if (!normals.empty())
            request.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
    } else if (have_bbox && (request.settings.flags & CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES)) {
        request.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK;
    } else {
        return CREO_BRL_SNAPSHOT_CAPTURE_TESSELLATION_FAILURE;
    }

    if (!snapshot_open_for_write(snapshot_path, &snapshot_file))
        return CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;

    memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
    header.format_version = CREO_BRL_SNAPSHOT_FORMAT_VERSION;
    header.header_size = CREO_BRL_SNAPSHOT_HEADER_V1_SIZE;
    header.payload_type = CREO_BRL_SNAPSHOT_PAYLOAD_SINGLE_PART;
    if (fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&request, 1, sizeof(request), snapshot_file) != sizeof(request) ||
        !write_text(snapshot_file, strings.output_path, &request.settings.output_path) ||
        !write_text(snapshot_file, strings.log_path, &request.settings.log_path) ||
        !write_text(snapshot_file, strings.material_path, &request.settings.material_path) ||
        !write_text(snapshot_file, strings.rename_parameters, &request.settings.rename_parameters) ||
        !write_text(snapshot_file, strings.preserve_parameters, &request.settings.preserve_parameters) ||
        !write_text(snapshot_file, strings.model_name, &request.part.model_name) ||
        !write_text(snapshot_file, strings.model_version, &request.part.model_version) ||
        !write_text(snapshot_file, strings.material_name, &request.part.material_name) ||
        !write_range(snapshot_file, vertices.data(), vertices.size() * sizeof(vertices[0]), &request.part.vertices) ||
        !write_range(snapshot_file, triangles.data(), triangles.size() * sizeof(triangles[0]), &request.part.triangles) ||
        !write_range(snapshot_file, normals.data(), normals.size() * sizeof(normals[0]), &request.part.normals) ||
        !write_range(snapshot_file, normal_indices.data(), normal_indices.size() * sizeof(normal_indices[0]), &request.part.normal_indices)) {
        fclose(snapshot_file);
        return CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;
    }

    request.part.vertex_count = vertices.size();
    request.part.triangle_count = triangles.size();
    request.part.normal_count = normals.size();
    request.part.normal_index_count = normal_indices.size();
    header.snapshot_size = (uint64_t)_ftelli64(snapshot_file);
    header.payload_size = header.snapshot_size - header.header_size;
    if (header.snapshot_size < header.header_size || _fseeki64(snapshot_file, 0, SEEK_SET) != 0 ||
        fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&request, 1, sizeof(request), snapshot_file) != sizeof(request)) {
        fclose(snapshot_file);
        return CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;
    }

    fclose(snapshot_file);
    return CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;
}
