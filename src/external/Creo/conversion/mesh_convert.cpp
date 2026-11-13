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
#include <limits>
#include <sstream>
#include <string.h>

#include "vmath.h"
#include "conversion/name_allocator.h"
#include "bg/trimesh.h"
#include "bg/vert_tree.h"
#include "bu/avs.h"
#include "rt/geom.h"
#include "rt/primitives/bot.h"
#include "shared/core_api.h"
#include "shared/mesh_normalization.h"
#include "wdb.h"


static const char CREO_BRL_CORE_SOLID_SUFFIX[] = ".s";
static const char CREO_BRL_CORE_RPP_SUFFIX[] = ".rpp";
static const char CREO_BRL_CORE_IMPORTER_ATTRIBUTE[] = "importer";
static const char CREO_BRL_CORE_IMPORTER_VALUE[] = "creo-g";
static const char CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE[] = "ptc_name";
static const char CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
static const char CREO_BRL_CORE_MATERIAL_ATTRIBUTE[] = "ptc_material_name";
static const char CREO_BRL_CORE_FALLBACK_ATTRIBUTE[] = "tess_fail";
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


static void
add_snapshot_named_values(
    struct bu_attribute_value_set *attributes,
    const std::vector<struct snapshot_named_value>& values)
{
    if (!attributes)
        return;

    for (size_t index = 0; index < values.size(); ++index)
        bu_avs_add(attributes, values[index].name.c_str(), values[index].value.c_str());
}


static void
add_snapshot_mass_properties(struct bu_attribute_value_set *attributes,
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
        if (EQUAL(maximum, red))
            hue = 60.0 * std::fmod((green - blue) / delta, 6.0);
        else if (EQUAL(maximum, green))
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
                          const struct snapshot_part_capture& part_capture,
                          const struct snapshot_material_translation *material_translation,
                          const std::string& original_color,
                          int has_mesh)
{
    const struct creo_brl_snapshot_part& part = part_capture.part;
    const std::string& model_name = part_capture.strings.model_name;
    const std::string& model_version = part_capture.strings.model_version;
    const std::string& material_name = part_capture.strings.material_name;
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
                   format_double(part.bbox_area).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_RPP_DIAGONAL_ATTRIBUTE,
                   format_double(part.bbox_diagonal).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_RPP_VOLUME_ATTRIBUTE,
                   format_double(part.bbox_volume).c_str());
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
    add_snapshot_named_values(&attributes, part_capture.attributes);
    add_snapshot_named_values(&attributes, part_capture.parameters);
    if (material_translation && material_translation->material_id >= 0) {
        bu_avs_add(&attributes, CREO_BRL_CORE_MATERIAL_ID_ATTRIBUTE,
                    std::to_string(material_translation->material_id).c_str());
        bu_avs_add(&attributes, CREO_BRL_CORE_LOS_ATTRIBUTE,
                    std::to_string(material_translation->los).c_str());
    }
    if (!original_color.empty())
        bu_avs_add(&attributes, CREO_BRL_CORE_ORIGINAL_COLOR_ATTRIBUTE,
                   original_color.c_str());
    if ((part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES) != 0) {
        add_snapshot_mass_properties(&attributes, 1, part.density,
                                     part.mass, part.volume,
                                     part.surface_area);
    } else if (std::isfinite(part.density) && part.density > 0.0) {
        bu_avs_add(&attributes, CREO_BRL_CORE_DENSITY_ATTRIBUTE, format_double(part.density).c_str());
    }
    result = db5_update_attributes(region, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


int
creo_brl_write_snapshot_assembly_attributes(
    struct db_i *database,
    const char *assembly_name,
    const struct snapshot_assembly_capture& assembly_capture)
{
    const struct creo_brl_snapshot_assembly& assembly_record = assembly_capture.assembly;
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct directory *assembly = db_lookup(database, assembly_name, LOOKUP_QUIET);
    int result = 0;

    if (assembly == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, assembly) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_IMPORTER_ATTRIBUTE, CREO_BRL_CORE_IMPORTER_VALUE);
    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, assembly_capture.model_name.c_str());
    if (!assembly_capture.model_version.empty()) {
        bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE,
                   assembly_capture.model_version.c_str());
    }
    add_snapshot_named_values(&attributes, assembly_capture.parameters);
    add_snapshot_mass_properties(
        &attributes,
        (assembly_record.flags & CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) != 0,
        assembly_record.density, assembly_record.mass,
        assembly_record.volume, assembly_record.surface_area);
    result = db5_update_attributes(assembly, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
snapshot_write_tgc_subtractions(const struct snapshot_part_capture& part_capture,
                                struct db_i *database,
                                struct rt_wdb *writer,
                                std::vector<std::string> *subtraction_names)
{
    const std::vector<struct creo_brl_snapshot_tgc_subtraction>& subtractions =
        part_capture.tgc_subtractions;
    const std::string hole_name_root =
        creo_brl_snapshot_name_root(part_capture.strings.model_name) + "_hole";

    if (!database || !writer || !subtraction_names)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    subtraction_names->clear();
    if (subtractions.empty())
        return CREO_BRL_CORE_CONVERT_SUCCESS;

    subtraction_names->reserve(subtractions.size());
    for (size_t index = 0; index < subtractions.size(); ++index) {
        const struct creo_brl_snapshot_tgc_subtraction& subtraction = subtractions[index];
        std::string solid_name;
        std::string unused_region_name;
        std::string hole_name = hole_name_root;

        if (subtractions.size() > 1)
            hole_name += "_" + std::to_string(index + 1u);
        if (!creo_brl_snapshot_object_names(database, hole_name, CREO_BRL_CORE_SOLID_SUFFIX,
                                            &solid_name, &unused_region_name) ||
            mk_tgc(writer, solid_name.c_str(),
                   subtraction.base.coordinates,
                   subtraction.height.coordinates,
                   subtraction.a.coordinates,
                   subtraction.b.coordinates,
                   subtraction.c.coordinates,
                   subtraction.d.coordinates) != 0)
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

        subtraction_names->push_back(solid_name);
    }

    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static int
normalize_snapshot_mesh(const struct snapshot_part_capture& part_capture,
                        const struct creo_brl_snapshot_settings& settings,
                        struct normalized_mesh *mesh)
{
    struct bg_vert_tree *normal_tree = NULL;
    const struct creo_brl_snapshot_part& part = part_capture.part;
    const std::vector<struct creo_brl_snapshot_vector>& source_vertices = part_capture.vertices;
    const std::vector<struct creo_brl_snapshot_triangle>& source_triangles = part_capture.triangles;
    const std::vector<struct creo_brl_snapshot_vector>& source_normals = part_capture.normals;
    const std::vector<uint32_t>& source_normal_indices = part_capture.normal_indices;
    const int has_normals = (part.flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    const int use_normals = has_normals &&
                            (settings.flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;
    const double vertex_tolerance_squared = CREO_BRL_MESH_VERTEX_TOLERANCE_MM *
                                            CREO_BRL_MESH_VERTEX_TOLERANCE_MM;
    struct creo_brl_mesh_normalization normalization = {};
    struct rt_bot_internal bot = {};

    if (!mesh ||
        (use_normals &&
         (source_normals.empty() ||
          source_normal_indices.size() !=
              source_triangles.size() * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)))
        return 0;

    mesh->vertices.clear();
    mesh->faces.clear();
    mesh->normals.clear();
    mesh->face_normals.clear();
    mesh->is_solid = 0;

    if (!creo_brl_normalize_mesh(source_vertices, source_triangles, &normalization))
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
            const uint32_t normal_index1 = source_normal_indices[face_offset + 0];
            const uint32_t normal_index2 = source_normal_indices[face_offset + 1];
            const uint32_t normal_index3 = source_normal_indices[face_offset + 2];

            if (normal_index1 >= source_normals.size() ||
                normal_index2 >= source_normals.size() ||
                normal_index3 >= source_normals.size())
                goto cleanup;

            n1 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_index1].coordinates[0],
                                  source_normals[normal_index1].coordinates[1],
                                  source_normals[normal_index1].coordinates[2],
                                  vertex_tolerance_squared);
            n2 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_index2].coordinates[0],
                                  source_normals[normal_index2].coordinates[1],
                                  source_normals[normal_index2].coordinates[2],
                                  vertex_tolerance_squared);
            n3 = bg_vert_tree_add(normal_tree,
                                  source_normals[normal_index3].coordinates[0],
                                  source_normals[normal_index3].coordinates[1],
                                  source_normals[normal_index3].coordinates[2],
                                  vertex_tolerance_squared);
        }

        mesh->faces.push_back((int)v1);
        mesh->faces.push_back((int)v2);
        mesh->faces.push_back((int)v3);
        if (use_normals) {
            mesh->face_normals.push_back((int)n1);
            mesh->face_normals.push_back((int)n2);
            mesh->face_normals.push_back((int)n3);
        }
    }

    if (mesh->faces.empty() || mesh->vertices.empty())
        goto cleanup;
    if (use_normals && mesh->face_normals.empty())
        goto cleanup;

    bot.magic = RT_BOT_INTERNAL_MAGIC;
    bot.mode = RT_BOT_SOLID;
    bot.orientation = RT_BOT_CCW;
    bot.num_vertices = normalization.vertices.size();
    bot.num_faces = normalization.triangles.size();
    bot.vertices = mesh->vertices.data();
    bot.faces = mesh->faces.data();

    if (bot.num_faces <= static_cast<size_t>((std::numeric_limits<int>::max)()) &&
        bg_trimesh_sync(bot.faces, bot.faces, static_cast<int>(bot.num_faces)) > 0 &&
        use_normals) {
        for (size_t triangle_index = 0;
             triangle_index < normalization.triangles.size(); ++triangle_index) {
            if (mesh->faces[triangle_index * 3] !=
                static_cast<int>(normalization.triangles[triangle_index].vertices[0])) {
                std::swap(mesh->face_normals[triangle_index * 3],
                          mesh->face_normals[triangle_index * 3 + 1]);
            }
        }
    }
    if (rt_bot_inside_out(&bot) > 0) {
        (void)rt_bot_flip(&bot);
        if (use_normals) {
            for (size_t triangle_index = 0;
                 triangle_index < normalization.triangles.size(); ++triangle_index) {
                std::swap(mesh->face_normals[triangle_index * 3 + 1],
                          mesh->face_normals[triangle_index * 3 + 2]);
            }
        }
    }

    if (use_normals) {
        mesh->normals.assign(normal_tree->the_array,
                             normal_tree->the_array + normal_tree->curr_vert * 3);
    }

    mesh->is_solid =
        (bot.num_vertices <= static_cast<size_t>((std::numeric_limits<int>::max)()) &&
         bot.num_faces <= static_cast<size_t>((std::numeric_limits<int>::max)())) ?
            !bg_trimesh_solid2(static_cast<int>(bot.num_vertices),
                               static_cast<int>(bot.num_faces),
                               bot.vertices, bot.faces, NULL) :
            normalization.is_solid;

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
                      const struct snapshot_part_capture& part_capture,
                      const struct snapshot_scene_capture& scene,
                      const struct snapshot_material_translation *material_translation,
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
    const struct creo_brl_snapshot_part& part = part_capture.part;
    const struct creo_brl_snapshot_settings& settings = scene.settings;
    const std::string& rename_parameters = scene.settings_strings.rename_parameters;
    const std::string& model_name = part_capture.strings.model_name;
    std::string solid_name;
    std::string region_name;
    std::string original_color;
    unsigned char output_color[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT] = {};
    const unsigned char *region_color = NULL;
    const std::string name_source =
        creo_brl_snapshot_name_source(rename_parameters, model_name, part_capture.parameters);
    struct bu_list members;

    if (part.color_is_set) {
        memcpy(output_color, part.color, sizeof(output_color));
        (void)adjust_color_luminance(output_color, settings.min_luminance,
                                     &original_color);
        region_color = output_color;
    }

    if (!creo_brl_snapshot_object_names(database, creo_brl_snapshot_name_root(name_source),
                                        has_mesh ? CREO_BRL_CORE_SOLID_SUFFIX : CREO_BRL_CORE_RPP_SUFFIX,
                                        &solid_name, &region_name) ||
        (has_mesh ?
         write_bot(writer, solid_name.c_str(), vertex_count, face_count,
                   vertices, faces, normal_count, normals, face_normals) :
         mk_rpp(writer, solid_name.c_str(), part.bbox_min.coordinates,
                part.bbox_max.coordinates)) != 0)
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
                (int)part.region_id, 0, 0, 0, 0, 0, 0) != 0 ||
        !snapshot_write_attributes(database, solid_name.c_str(), region_name.c_str(),
                                   part_capture, material_translation, original_color,
                                   has_mesh))
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    output->region_name = region_name;
    output->used_bbox_fallback = !has_mesh;
    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static void
snapshot_log_part_result(FILE *log_file,
                         const struct creo_brl_snapshot_settings *settings,
                         const struct snapshot_part_capture& part_capture,
                         int result,
                         const struct snapshot_part_output *output,
                         int is_solid,
                         const char *failure_reason)
{
    const int32_t log_type = settings ? settings->log_type : CREO_BRL_SNAPSHOT_LOG_NONE;
    const std::string& model_name = part_capture.strings.model_name;
    const std::string& diagnostic_note = part_capture.strings.diagnostic_note;
    const uint32_t part_flags = part_capture.part.flags;
    const char *reason = (failure_reason && failure_reason[0] != '\0') ?
                             failure_reason :
                             (diagnostic_note.empty() ? NULL : diagnostic_note.c_str());
    const int log_failures =
        log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE ||
        log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS ||
        log_type == CREO_BRL_SNAPSHOT_LOG_ALL;
    const int log_successes =
        log_type == CREO_BRL_SNAPSHOT_LOG_SUCCESS ||
        log_type == CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS ||
        log_type == CREO_BRL_SNAPSHOT_LOG_ALL;

    if (!log_file || !output || log_type == CREO_BRL_SNAPSHOT_LOG_NONE)
        return;

    if (result == CREO_BRL_CORE_CONVERT_SUCCESS) {
        if (output->used_bbox_fallback) {
            if (log_failures) {
                if (reason) {
                    fprintf(log_file,
                            "FAILURE: Part \"%s\" replaced with bounding box (%s)\n",
                            model_name.c_str(), reason);
                } else {
                    fprintf(log_file,
                            "FAILURE: Part \"%s\" replaced with bounding box\n",
                            model_name.c_str());
                }
                fflush(log_file);
            }
            return;
        }

        if ((part_flags & CREO_BRL_SNAPSHOT_PART_OMITTED_QUILTS) != 0) {
            fprintf(log_file,
                    "WARNING: Part \"%s\" converted with limitations (%s)\n",
                    model_name.c_str(),
                    !diagnostic_note.empty() ?
                        diagnostic_note.c_str() :
                        "open quilt surfaces were omitted; converted passing solid geometry only");
        }
        if ((part_flags & CREO_BRL_SNAPSHOT_PART_INCOMPLETE_TESS) != 0) {
            const char *incomplete_note =
                (!diagnostic_note.empty() &&
                 (part_flags & CREO_BRL_SNAPSHOT_PART_OMITTED_QUILTS) == 0) ?
                    diagnostic_note.c_str() :
                    "some surfaces have missing or overlapping triangles";
            fprintf(log_file,
                    "WARNING: Part \"%s\" tessellation is incomplete (%s)\n",
                    model_name.c_str(), incomplete_note);
        }
        if (!is_solid || (part_flags & CREO_BRL_SNAPSHOT_PART_NON_SOLID) != 0) {
            fprintf(log_file,
                    "WARNING: Part \"%s\" BoT is not a closed solid (contains open surface or quilt geometry)\n",
                    model_name.c_str());
        }
        if (log_successes)
            fprintf(log_file, "SUCCESS: Part \"%s\" converted\n", model_name.c_str());
        fflush(log_file);
        return;
    }

    if (result == CREO_BRL_CORE_CONVERT_OUTPUT_FAILED && log_failures) {
        if (reason) {
            fprintf(log_file, "FAILURE: Part \"%s\" not converted (%s)\n",
                    model_name.c_str(), reason);
        } else {
            fprintf(log_file, "FAILURE: Part \"%s\" not converted\n",
                    model_name.c_str());
        }
        fflush(log_file);
    }
}


int
creo_brl_convert_snapshot_part(const struct snapshot_part_capture& part_capture,
                               const struct snapshot_scene_capture& scene,
                               const struct snapshot_material_translation *material_translation,
                               FILE *log_file,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               struct snapshot_part_output *output)
{
    const struct creo_brl_snapshot_part& part = part_capture.part;
    const struct creo_brl_snapshot_settings& settings = scene.settings;
    const int has_mesh = (part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_bbox_fallback = (part.flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0;
    const int require_solidity = (settings.flags & CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY) != 0;
    const int allow_box_fallback = (settings.flags & CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES) != 0;
    const int write_normals = (settings.flags & CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS) != 0;
    std::vector<std::string> subtraction_names;
    struct normalized_mesh mesh = {};
    int normalized_ok = 0;
    int result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    if (!database || !writer || !output)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    if (!part_capture.tgc_subtractions.empty()) {
        const int csg_result = snapshot_write_tgc_subtractions(
            part_capture, database, writer, &subtraction_names);

        if (csg_result != CREO_BRL_CORE_CONVERT_SUCCESS)
            return csg_result;
    }

    if (!has_mesh || has_bbox_fallback) {
        if (!has_bbox_fallback) {
            snapshot_log_part_result(
                log_file, &settings, part_capture,
                CREO_BRL_CORE_CONVERT_OUTPUT_FAILED, output, 0,
                "part contains no mesh or bounding-box fallback");
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }
        result = write_part_definition(database, writer, part_capture, scene,
                                       material_translation, subtraction_names,
                                       0, 0, 0, NULL, NULL, 0, NULL, NULL,
                                       output);
        snapshot_log_part_result(log_file, &settings, part_capture, result,
                                 output, 0, NULL);
        return result;
    }

    normalized_ok = normalize_snapshot_mesh(part_capture, settings, &mesh);
    if (!normalized_ok || (require_solidity && !mesh.is_solid)) {
        const char *mesh_failure_reason =
            !normalized_ok ?
                "mesh normalization failed (no valid non-degenerate facets)" :
                "mesh failed solidity check (non-solid BoT rejected)";

        if (!allow_box_fallback) {
            snapshot_log_part_result(log_file, &settings, part_capture,
                                     CREO_BRL_CORE_CONVERT_OUTPUT_FAILED,
                                     output, 0, mesh_failure_reason);
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }

        result = write_part_definition(database, writer, part_capture, scene,
                                       material_translation, subtraction_names,
                                       0, 0, 0, NULL, NULL, 0, NULL, NULL,
                                       output);
        snapshot_log_part_result(log_file, &settings, part_capture, result,
                                 output, 0, mesh_failure_reason);
        return result;
    }

    result = write_part_definition(database, writer, part_capture, scene,
                                   material_translation, subtraction_names, 1,
                                   mesh.vertices.size() / 3,
                                   mesh.faces.size() / 3,
                                   mesh.vertices.data(),
                                   mesh.faces.data(),
                                   write_normals ? mesh.normals.size() / 3 : 0,
                                   write_normals ? mesh.normals.data() : NULL,
                                   write_normals ? mesh.face_normals.data() : NULL,
                                   output);
    snapshot_log_part_result(log_file, &settings, part_capture, result, output,
                             mesh.is_solid, NULL);
    return result;
}
