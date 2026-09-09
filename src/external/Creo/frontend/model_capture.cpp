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

#include <cmath>

#include "frontend/model_capture.h"


static const double CREO_BRL_FEET_TO_INCHES = 12.0;
static const double CREO_BRL_CENTIMETERS_PER_INCH = 2.54;
static const double CREO_BRL_MILLIMETERS_PER_INCH = 25.4;
static const double CREO_BRL_METERS_PER_INCH = 0.0254;


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
        inches = CREO_BRL_FEET_TO_INCHES;
    else if (strcmp(unit_name, "in") == 0)
        inches = 1.0;
    else if (strcmp(unit_name, "cm") == 0)
        inches = 1.0 / CREO_BRL_CENTIMETERS_PER_INCH;
    else if (strcmp(unit_name, "mm") == 0)
        inches = 1.0 / CREO_BRL_MILLIMETERS_PER_INCH;
    else if (strcmp(unit_name, "m") == 0)
        inches = 1.0 / CREO_BRL_METERS_PER_INCH;
    else
        return 0;

    *scale = inches * CREO_BRL_MILLIMETERS_PER_INCH;
    return 1;
}


static void
set_vector(struct creo_brl_snapshot_vector *destination, const ProVector source, double scale)
{
    VSCALE(destination->coordinates, source, scale);
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
        capture->part.bbox_diagonal = DIST_PNT_PNT(capture->part.bbox_min.coordinates,
                                                   capture->part.bbox_max.coordinates);
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


static int
capture_component_matrix(ProMdl parent,
                         ProFeature *feature,
                         struct creo_brl_snapshot_matrix *matrix)
{
    ProAsmcomppath component_path;
    ProIdTable component_ids;
    ProMatrix creo_matrix;
    double scale = 0.0;

    MAT_IDN(matrix->elements);
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


int
creo_brl_frontend_capture_model_scene(ProMdl model,
                                      ProMdlType model_type,
                                      const struct creo_brl_snapshot_settings *settings,
                                      const struct snapshot_settings_strings *settings_strings,
                                      struct snapshot_scene_capture *capture,
                                      uint32_t *root_type,
                                      uint32_t *root_index)
{
    if (!model || !settings || !settings_strings || !capture || !root_type || !root_index)
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;

    capture->settings = *settings;
    capture->settings_strings = *settings_strings;

    if (model_type == PRO_MDL_PART) {
        *root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_PART;
        return capture_scene_part(model, capture, root_index);
    }
    if (model_type == PRO_MDL_ASSEMBLY) {
        *root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY;
        return capture_scene_assembly(model, capture, root_index);
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;
}
