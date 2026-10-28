/**
 *         M E S H _ C O N V E R T . C P P
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
 * @file conversion/mesh_convert.cpp
 *
 * Toolkit-free mesh normalization and fallback output helpers for snapshot
 * part conversion.
 */

#include "common.h"

#include "conversion/mesh_convert.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string.h>

#include "conversion/name_allocator.h"
#include "bg/vert_tree.h"
#include "bu/avs.h"
#include "shared/core_api.h"
#include "shared/mesh_normalization.h"
#include "wdb.h"


static const char CREO_BRL_CORE_SOLID_SUFFIX[] = ".s";
static const char CREO_BRL_CORE_RPP_SUFFIX[] = ".rpp";
const char CREO_BRL_CORE_IMPORTER_ATTRIBUTE[] = "importer";
const char CREO_BRL_CORE_IMPORTER_VALUE[] = "creo-g";
const char CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE[] = "ptc_name";
const char CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
const char CREO_BRL_CORE_MATERIAL_ATTRIBUTE[] = "ptc_material_name";
const char CREO_BRL_CORE_FALLBACK_ATTRIBUTE[] = "tess_fail";
static const char CREO_BRL_CORE_UNDEFINED_MATERIAL[] = "undefined";
static const char CREO_BRL_CORE_DENSITY_ATTRIBUTE[] = "density";
static const char CREO_BRL_CORE_MASS_ATTRIBUTE[] = "mass";
static const char CREO_BRL_CORE_VOLUME_ATTRIBUTE[] = "volume";
static const char CREO_BRL_CORE_SURFACE_AREA_ATTRIBUTE[] = "surface_area";
static const char CREO_BRL_CORE_ORIGINAL_COLOR_ATTRIBUTE[] = "ptc_rgb";
static const char CREO_BRL_CORE_MATERIAL_ID_ATTRIBUTE[] = "material_id";
static const char CREO_BRL_CORE_LOS_ATTRIBUTE[] = "los";
static const char CREO_BRL_CORE_RPP_AREA_ATTRIBUTE[] = "rpp_area";
static const char CREO_BRL_CORE_RPP_DIAGONAL_ATTRIBUTE[] = "rpp_diag";
static const char CREO_BRL_CORE_RPP_VOLUME_ATTRIBUTE[] = "rpp_vol";


struct normalized_mesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
    std::vector<fastf_t> normals;
    std::vector<int> face_normals;
    int is_solid;
};


static int
write_bot(struct rt_wdb *writer,
          const char *name,
          size_t vertex_count,
          size_t face_count,
          fastf_t *vertices,
          int *faces,
          size_t normal_count,
          fastf_t *normals,
          int *face_normals)
{
    if (normal_count > 0) {
        const unsigned char normal_flags = RT_BOT_HAS_SURFACE_NORMALS | RT_BOT_USE_NORMALS;

        return mk_bot_w_normals(writer, name, RT_BOT_SOLID, RT_BOT_CCW, normal_flags,
                                vertex_count, face_count, vertices, faces,
                                NULL, NULL, normal_count, normals, face_normals);
    }

    return mk_bot(writer, name, RT_BOT_SOLID, RT_BOT_CCW, 0,
                  vertex_count, face_count, vertices, faces, NULL, NULL);
}


static std::string
format_double(double value)
{
    std::ostringstream stream;
    std::string text;

    stream << std::setprecision(15) << value;
    text = stream.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text[text.size() - 1] == '0')
            text.resize(text.size() - 1);
        if (!text.empty() && text[text.size() - 1] == '.')
            text.resize(text.size() - 1);
    }
    return text;
}


void
creo_brl_add_snapshot_named_values(
    struct bu_attribute_value_set *attributes,
    const std::vector<struct snapshot_named_value>& values)
{
    if (!attributes)
        return;

    for (size_t index = 0; index < values.size(); ++index)
        bu_avs_add(attributes, values[index].name.c_str(), values[index].value.c_str());
}


void
creo_brl_add_snapshot_mass_properties(struct bu_attribute_value_set *attributes,
                                      int available,
                                      double density,
                                      double mass,
                                      double volume,
                                      double surface_area)
{
    if (!attributes || !available)
        return;

    bu_avs_add(attributes, CREO_BRL_CORE_DENSITY_ATTRIBUTE, format_double(density).c_str());
    bu_avs_add(attributes, CREO_BRL_CORE_MASS_ATTRIBUTE, format_double(mass).c_str());
    bu_avs_add(attributes, CREO_BRL_CORE_VOLUME_ATTRIBUTE, format_double(volume).c_str());
    bu_avs_add(attributes, CREO_BRL_CORE_SURFACE_AREA_ATTRIBUTE,
               format_double(surface_area).c_str());
}


static unsigned char
color_component(double value)
{
    const long rounded = std::lround(value * 255.0);

    return static_cast<unsigned char>((std::max)(0L, (std::min)(255L, rounded)));
}


static int
adjust_color_luminance(unsigned char color[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT],
                       int32_t minimum_luminance,
                       std::string *original_color)
{
    const unsigned char maximum_component =
        (std::max)(color[0], (std::max)(color[1], color[2]));
    const unsigned char minimum_component =
        (std::min)(color[0], (std::min)(color[1], color[2]));
    const double luminance =
        (static_cast<double>(maximum_component) + minimum_component) / 510.0;
    const double red = color[0] / 255.0;
    const double green = color[1] / 255.0;
    const double blue = color[2] / 255.0;
    const double maximum = (std::max)(red, (std::max)(green, blue));
    const double minimum = (std::min)(red, (std::min)(green, blue));
    const double delta = maximum - minimum;
    double hue = 0.0;
    double saturation = 0.0;
    double chroma = 0.0;
    double secondary = 0.0;
    double offset = 0.0;
    double adjusted[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT] = {};
    std::ostringstream original;

    if (minimum_luminance <= 0 ||
        std::lround(luminance * CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM) >=
            minimum_luminance)
        return 0;

    original << static_cast<unsigned int>(color[0]) << '/'
             << static_cast<unsigned int>(color[1]) << '/'
             << static_cast<unsigned int>(color[2]);
    if (original_color)
        *original_color = original.str();

    if (delta > 0.0) {
        if (maximum == red)
            hue = 60.0 * std::fmod((green - blue) / delta, 6.0);
        else if (maximum == green)
            hue = 60.0 * ((blue - red) / delta + 2.0);
        else
            hue = 60.0 * ((red - green) / delta + 4.0);
        if (hue < 0.0)
            hue += 360.0;

        const double denominator = 1.0 - std::fabs(2.0 * luminance - 1.0);
        if (denominator > 0.0)
            saturation = delta / denominator;
    }

    const double target_luminance =
        minimum_luminance / CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM;
    chroma = (1.0 - std::fabs(2.0 * target_luminance - 1.0)) * saturation;
    secondary = chroma * (1.0 - std::fabs(std::fmod(hue / 60.0, 2.0) - 1.0));
    offset = target_luminance - chroma / 2.0;

    switch (static_cast<int>(hue / 60.0) % 6) {
        case 0:
            adjusted[0] = chroma; adjusted[1] = secondary; break;
        case 1:
            adjusted[0] = secondary; adjusted[1] = chroma; break;
        case 2:
            adjusted[1] = chroma; adjusted[2] = secondary; break;
        case 3:
            adjusted[1] = secondary; adjusted[2] = chroma; break;
        case 4:
            adjusted[0] = secondary; adjusted[2] = chroma; break;
        default:
            adjusted[0] = chroma; adjusted[2] = secondary; break;
    }

    for (size_t component = 0; component < CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT; ++component)
        color[component] = color_component(adjusted[component] + offset);
    return 1;
}


static int
snapshot_write_attributes(struct db_i *database,
                          const char *solid_name,
                          const char *region_name,
                          const struct creo_brl_snapshot_part *part,
                          const std::string& model_name,
                          const std::string& model_version,
                          const std::string& material_name,
                          const struct snapshot_material_translation *material_translation,
                          const std::string& original_color,
                          const std::vector<struct snapshot_named_value>& attributes_in,
                          const std::vector<struct snapshot_named_value>& parameters,
                          int has_mesh)
{
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct directory *solid = db_lookup(database, solid_name, LOOKUP_QUIET);
    struct directory *region = db_lookup(database, region_name, LOOKUP_QUIET);
    int result = 0;

    if (solid == RT_DIR_NULL || region == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, solid) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_IMPORTER_ATTRIBUTE, CREO_BRL_CORE_IMPORTER_VALUE);
    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, model_name.c_str());
    if (!has_mesh) {
        bu_avs_add(&attributes, CREO_BRL_CORE_FALLBACK_ATTRIBUTE, model_name.c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_RPP_AREA_ATTRIBUTE,
                   format_double(part->bbox_area).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_RPP_DIAGONAL_ATTRIBUTE,
                   format_double(part->bbox_diagonal).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_RPP_VOLUME_ATTRIBUTE,
                   format_double(part->bbox_volume).c_str());
    }
    if (db5_update_attributes(solid, &attributes, database) != 0)
        goto cleanup;

    bu_avs_free(&attributes);
    if (db5_get_attributes(database, &attributes, region) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, model_name.c_str());
    if (!model_version.empty())
        bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE, model_version.c_str());
    bu_avs_add(&attributes, CREO_BRL_CORE_MATERIAL_ATTRIBUTE,
               material_name.empty() ? CREO_BRL_CORE_UNDEFINED_MATERIAL : material_name.c_str());
    creo_brl_add_snapshot_named_values(&attributes, attributes_in);
    creo_brl_add_snapshot_named_values(&attributes, parameters);
    if (material_translation && material_translation->material_id >= 0) {
        bu_avs_add(&attributes, CREO_BRL_CORE_MATERIAL_ID_ATTRIBUTE,
                   std::to_string(material_translation->material_id).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_LOS_ATTRIBUTE,
                   std::to_string(material_translation->los).c_str());
    }
    if (!original_color.empty())
        bu_avs_add(&attributes, CREO_BRL_CORE_ORIGINAL_COLOR_ATTRIBUTE,
                   original_color.c_str());
    if ((part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES) != 0) {
        creo_brl_add_snapshot_mass_properties(&attributes, 1, part->density,
                                              part->mass, part->volume,
                                              part->surface_area);
    } else if (std::isfinite(part->density) && part->density > 0.0) {
        bu_avs_add(&attributes, CREO_BRL_CORE_DENSITY_ATTRIBUTE, format_double(part->density).c_str());
    }
    result = db5_update_attributes(region, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
facet_winds_ccw(uint32_t v1, uint32_t v2, uint32_t v3,
                size_t n1, size_t n2, size_t n3,
                const std::vector<fastf_t>& vertices,
                const struct bg_vert_tree *normals)
{
    vect_t point1, point2, point3;
    vect_t normal1, normal2, normal3;
    vect_t edge12, edge13;
    vect_t facet_normal, average_normal;

    VMOVE(point1, &vertices[(size_t)v1 * 3]);
    VMOVE(point2, &vertices[(size_t)v2 * 3]);
    VMOVE(point3, &vertices[(size_t)v3 * 3]);
    VMOVE(normal1, &normals->the_array[n1 * 3]);
    VMOVE(normal2, &normals->the_array[n2 * 3]);
    VMOVE(normal3, &normals->the_array[n3 * 3]);

    VSUB2(edge12, point2, point1);
    VSUB2(edge13, point3, point1);
    VCROSS(facet_normal, edge12, edge13);
    if (VNEAR_ZERO(facet_normal, VUNITIZE_TOL))
        return 0;
    VUNITIZE(facet_normal);

    average_normal[0] = (normal1[0] + normal2[0] + normal3[0]) / 3.0;
    average_normal[1] = (normal1[1] + normal2[1] + normal3[1]) / 3.0;
    average_normal[2] = (normal1[2] + normal2[2] + normal3[2]) / 3.0;
    if (VNEAR_ZERO(average_normal, VUNITIZE_TOL))
        return 0;
    VUNITIZE(average_normal);

    return VDOT(facet_normal, average_normal) > 0.0;
}


static int
normalize_snapshot_mesh(const struct creo_brl_snapshot_part *part,
                        const struct creo_brl_snapshot_settings *settings,
                        std::vector<fastf_t>& source_vertices,
                        std::vector<int>& source_triangles,
                        std::vector<fastf_t>& source_normals,
                        std::vector<int>& source_normal_indices,
                        struct normalized_mesh *mesh)
{
    struct bg_vert_tree *normal_tree = NULL;
    const int has_normals = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    const int use_normals = has_normals && settings &&
                            (settings->flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;
    const double vertex_tolerance_squared = CREO_BRL_MESH_VERTEX_TOLERANCE_MM *
                                            CREO_BRL_MESH_VERTEX_TOLERANCE_MM;
    std::vector<struct creo_brl_snapshot_vector> normalization_vertices;
    std::vector<struct creo_brl_snapshot_triangle> normalization_triangles;
    struct creo_brl_mesh_normalization normalization = {};

    if (!part || !mesh || source_vertices.size() % CREO_BRL_SNAPSHOT_COORDINATE_COUNT != 0 ||
        source_triangles.size() % CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT != 0 ||
        source_vertices.size() / CREO_BRL_SNAPSHOT_COORDINATE_COUNT != part->vertex_count ||
        source_triangles.size() / CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT != part->triangle_count ||
        (use_normals &&
         (source_normals.size() % CREO_BRL_SNAPSHOT_COORDINATE_COUNT != 0 ||
          source_normals.size() / CREO_BRL_SNAPSHOT_COORDINATE_COUNT !=
              part->normal_count ||
          source_normal_indices.size() != source_triangles.size())))
        return 0;

    mesh->vertices.clear();
    mesh->faces.clear();
    mesh->normals.clear();
    mesh->face_normals.clear();
    mesh->is_solid = 0;

    normalization_vertices.resize(source_vertices.size() /
                                  CREO_BRL_SNAPSHOT_COORDINATE_COUNT);
    for (size_t vertex = 0; vertex < normalization_vertices.size(); ++vertex) {
        for (size_t coordinate = 0;
             coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            normalization_vertices[vertex].coordinates[coordinate] =
                source_vertices[vertex * CREO_BRL_SNAPSHOT_COORDINATE_COUNT + coordinate];
        }
    }
    normalization_triangles.resize(source_triangles.size() /
                                   CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);
    for (size_t triangle = 0; triangle < normalization_triangles.size(); ++triangle) {
        for (size_t corner = 0;
             corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
            const int source_vertex = source_triangles[
                triangle * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT + corner];

            if (source_vertex < 0)
                goto cleanup;
            normalization_triangles[triangle].vertices[corner] =
                static_cast<uint32_t>(source_vertex);
        }
    }

    if (!creo_brl_normalize_mesh(normalization_vertices, normalization_triangles,
                                 &normalization))
        goto cleanup;

    if (normalization.vertices.size() > mesh->vertices.max_size() /
                                            CREO_BRL_SNAPSHOT_COORDINATE_COUNT ||
        normalization.triangles.size() > mesh->faces.max_size() /
                                             CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)
        goto cleanup;
    mesh->vertices.reserve(normalization.vertices.size() *
                           CREO_BRL_SNAPSHOT_COORDINATE_COUNT);
    for (size_t vertex = 0; vertex < normalization.vertices.size(); ++vertex) {
        mesh->vertices.insert(mesh->vertices.end(),
                              normalization.vertices[vertex].coordinates,
                              normalization.vertices[vertex].coordinates +
                                  CREO_BRL_SNAPSHOT_COORDINATE_COUNT);
    }
    mesh->faces.reserve(normalization.triangles.size() *
                        CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);

    if (use_normals) {
        normal_tree = bg_vert_tree_create();
        if (!normal_tree)
            goto cleanup;
        mesh->face_normals.reserve(normalization.triangles.size() *
                                   CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT);
    }

    for (size_t triangle_index = 0;
         triangle_index < normalization.triangles.size(); ++triangle_index) {
        const size_t source_triangle_index =
            normalization.source_triangle_indices[triangle_index];
        const size_t face_offset = source_triangle_index *
                                   CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT;
        const struct creo_brl_snapshot_triangle& triangle =
            normalization.triangles[triangle_index];
        const uint32_t v1 = triangle.vertices[0];
        const uint32_t v2 = triangle.vertices[1];
        const uint32_t v3 = triangle.vertices[2];
        size_t n1 = 0, n2 = 0, n3 = 0;

        if (use_normals) {
            if (source_normal_indices[face_offset + 0] < 0 ||
                source_normal_indices[face_offset + 1] < 0 ||
                source_normal_indices[face_offset + 2] < 0)
                goto cleanup;
            const size_t normal_offset1 =
                (size_t)source_normal_indices[face_offset + 0] * 3;
            const size_t normal_offset2 =
                (size_t)source_normal_indices[face_offset + 1] * 3;
            const size_t normal_offset3 =
                (size_t)source_normal_indices[face_offset + 2] * 3;

            if (source_normals.size() < CREO_BRL_SNAPSHOT_COORDINATE_COUNT ||
                normal_offset1 > source_normals.size() - 3u ||
                normal_offset2 > source_normals.size() - 3u ||
                normal_offset3 > source_normals.size() - 3u)
                goto cleanup;

            n1 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_offset1 + 0],
                                  source_normals[normal_offset1 + 1],
                                  source_normals[normal_offset1 + 2],
                                  vertex_tolerance_squared);
            n2 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_offset2 + 0],
                                  source_normals[normal_offset2 + 1],
                                  source_normals[normal_offset2 + 2],
                                  vertex_tolerance_squared);
            n3 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_offset3 + 0],
                                  source_normals[normal_offset3 + 1],
                                  source_normals[normal_offset3 + 2],
                                  vertex_tolerance_squared);
        }

        if (use_normals && facet_winds_ccw(v1, v3, v2, n1, n3, n2,
                                           mesh->vertices, normal_tree)) {
            mesh->faces.push_back((int)v1);
            mesh->faces.push_back((int)v3);
            mesh->faces.push_back((int)v2);
            mesh->face_normals.push_back((int)n1);
            mesh->face_normals.push_back((int)n3);
            mesh->face_normals.push_back((int)n2);
        } else {
            mesh->faces.push_back((int)v1);
            mesh->faces.push_back((int)v2);
            mesh->faces.push_back((int)v3);
            if (use_normals) {
                mesh->face_normals.push_back((int)n1);
                mesh->face_normals.push_back((int)n2);
                mesh->face_normals.push_back((int)n3);
            }
        }
    }

    if (mesh->faces.empty() || mesh->vertices.empty())
        goto cleanup;
    if (use_normals && mesh->face_normals.empty())
        goto cleanup;

    if (use_normals) {
        mesh->normals.assign(normal_tree->the_array,
                             normal_tree->the_array + normal_tree->curr_vert * 3);
    }
    mesh->is_solid = normalization.is_solid;

    if (normal_tree)
        bg_vert_tree_destroy(normal_tree);
    return 1;

cleanup:
    if (normal_tree)
        bg_vert_tree_destroy(normal_tree);
    mesh->vertices.clear();
    mesh->faces.clear();
    mesh->normals.clear();
    mesh->face_normals.clear();
    mesh->is_solid = 0;
    return 0;
}


static int
write_part_definition(struct db_i *database,
                      struct rt_wdb *writer,
                      const struct creo_brl_snapshot_part *part,
                      const struct creo_brl_snapshot_settings *settings,
                      const std::string& rename_parameters,
                      const std::string& model_name,
                      const std::string& model_version,
                      const std::string& material_name,
                      const struct snapshot_material_translation *material_translation,
                      const std::vector<struct snapshot_named_value>& attributes,
                      const std::vector<struct snapshot_named_value>& parameters,
                      const std::vector<std::string>& subtraction_names,
                      int has_mesh,
                      size_t vertex_count,
                      size_t face_count,
                      fastf_t *vertices,
                      int *faces,
                      size_t normal_count,
                      fastf_t *normals,
                      int *face_normals,
                      struct snapshot_part_output *output)
{
    std::string solid_name;
    std::string region_name;
    std::string original_color;
    unsigned char output_color[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT] = {};
    const unsigned char *region_color = NULL;
    const std::string name_source =
        creo_brl_snapshot_name_source(rename_parameters, model_name, parameters);
    struct bu_list members;

    if (part->color_is_set) {
        memcpy(output_color, part->color, sizeof(output_color));
        if (settings)
            (void)adjust_color_luminance(output_color, settings->min_luminance,
                                         &original_color);
        region_color = output_color;
    }

    if (!creo_brl_snapshot_object_names(database, creo_brl_snapshot_name_root(name_source),
                                        has_mesh ? CREO_BRL_CORE_SOLID_SUFFIX : CREO_BRL_CORE_RPP_SUFFIX,
                                        &solid_name, &region_name) ||
        (has_mesh ?
         write_bot(writer, solid_name.c_str(), vertex_count, face_count,
                   vertices, faces, normal_count, normals, face_normals) :
         mk_rpp(writer, solid_name.c_str(), part->bbox_min.coordinates,
                part->bbox_max.coordinates)) != 0)
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    BU_LIST_INIT(&members);
    if (mk_addmember(solid_name.c_str(), &members, NULL, WMOP_UNION) == WMEMBER_NULL) {
        mk_freemembers(&members);
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
    }
    for (size_t index = 0; index < subtraction_names.size(); ++index) {
        if (mk_addmember(subtraction_names[index].c_str(), &members, NULL, WMOP_SUBTRACT) == WMEMBER_NULL) {
            mk_freemembers(&members);
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }
    }

    if (mk_comb(writer, region_name.c_str(), &members, 1, NULL, NULL,
                region_color,
                (int)part->region_id, 0, 0, 0, 0, 0, 0) != 0 ||
        !snapshot_write_attributes(database, solid_name.c_str(), region_name.c_str(), part,
                                   model_name, model_version, material_name,
                                   material_translation, original_color,
                                   attributes, parameters, has_mesh))
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    output->region_name = region_name;
    output->used_bbox_fallback = !has_mesh;
    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static void
snapshot_log_part_result(FILE *log_file,
                         const struct creo_brl_snapshot_settings *settings,
                         const std::string& model_name,
                         int result,
                         const struct snapshot_part_output *output)
{
    const int32_t log_type = settings ? settings->log_type : CREO_BRL_SNAPSHOT_LOG_NONE;

    if (!log_file || !output || log_type == CREO_BRL_SNAPSHOT_LOG_NONE)
        return;

    if (result == CREO_BRL_CORE_CONVERT_SUCCESS) {
        if (output->used_bbox_fallback) {
            if (log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE ||
                log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS ||
                log_type == CREO_BRL_SNAPSHOT_LOG_ALL) {
                fprintf(log_file, "FAILURE: Part \"%s\" replaced with bounding box\n",
                        model_name.c_str());
                fflush(log_file);
            }
        } else if (log_type == CREO_BRL_SNAPSHOT_LOG_SUCCESS ||
                   log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS ||
                   log_type == CREO_BRL_SNAPSHOT_LOG_ALL) {
            fprintf(log_file, "SUCCESS: Part \"%s\" converted\n", model_name.c_str());
            fflush(log_file);
        }

        return;
    }

    if (result == CREO_BRL_CORE_CONVERT_OUTPUT_FAILED &&
        (log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE ||
         log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS ||
         log_type == CREO_BRL_SNAPSHOT_LOG_ALL)) {
        fprintf(log_file, "FAILURE: Part \"%s\" not converted\n", model_name.c_str());
        fflush(log_file);
    }
}


int
creo_brl_convert_snapshot_part(const struct creo_brl_snapshot_part *part,
                               const struct creo_brl_snapshot_settings *settings,
                               const std::string& rename_parameters,
                               const std::string& model_name,
                               const std::string& model_version,
                               const std::string& material_name,
                               const struct snapshot_material_translation *material_translation,
                               const std::vector<struct snapshot_named_value>& attributes,
                               const std::vector<struct snapshot_named_value>& parameters,
                               const std::vector<std::string>& subtraction_names,
                               std::vector<fastf_t>& vertices,
                               std::vector<int>& triangles,
                               std::vector<fastf_t>& normals,
                               std::vector<int>& normal_indices,
                               FILE *log_file,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               struct snapshot_part_output *output)
{
    const int has_mesh = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_bbox_fallback = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0;
    const int require_solidity = settings &&
                                 (settings->flags & CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY) != 0;
    const int allow_box_fallback = settings &&
                                   (settings->flags & CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES) != 0;
    const int write_normals = settings &&
                              (settings->flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;
    struct normalized_mesh mesh = {};
    int result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    if (!part || !database || !writer || !output)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    if (!has_mesh || has_bbox_fallback) {
        if (!has_bbox_fallback) {
            snapshot_log_part_result(log_file, settings, model_name,
                                     CREO_BRL_CORE_CONVERT_OUTPUT_FAILED, output);
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }
        result = write_part_definition(database, writer, part, settings, rename_parameters,
                                       model_name, model_version, material_name,
                                       material_translation,
                                       attributes, parameters, subtraction_names,
                                       0, 0, 0, NULL, NULL, 0, NULL, NULL,
                                       output);
        snapshot_log_part_result(log_file, settings, model_name, result, output);
        return result;
    }

    if (!normalize_snapshot_mesh(part, settings, vertices, triangles, normals, normal_indices, &mesh) ||
        (require_solidity && !mesh.is_solid)) {
        if (!allow_box_fallback) {
            snapshot_log_part_result(log_file, settings, model_name,
                                     CREO_BRL_CORE_CONVERT_OUTPUT_FAILED, output);
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }

        result = write_part_definition(database, writer, part, settings, rename_parameters,
                                       model_name, model_version, material_name,
                                       material_translation,
                                       attributes, parameters, subtraction_names,
                                       0, 0, 0, NULL, NULL, 0, NULL, NULL,
                                       output);
        snapshot_log_part_result(log_file, settings, model_name, result, output);
        return result;
    }

    result = write_part_definition(database, writer, part, settings, rename_parameters,
                                   model_name, model_version, material_name,
                                   material_translation,
                                   attributes, parameters, subtraction_names, 1,
                                   mesh.vertices.size() / 3,
                                   mesh.faces.size() / 3,
                                   mesh.vertices.data(),
                                   mesh.faces.data(),
                                   write_normals ? mesh.normals.size() / 3 : 0,
                                   write_normals ? mesh.normals.data() : NULL,
                                   write_normals ? mesh.face_normals.data() : NULL,
                                   output);
    snapshot_log_part_result(log_file, settings, model_name, result, output);
    return result;
}
