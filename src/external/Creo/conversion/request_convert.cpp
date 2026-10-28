/**
 *       R E Q U E S T _ C O N V E R T . C P P
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
 * @file conversion/request_convert.cpp
 *
 * Toolkit-free conversion request validation for the runtime core.
 */

#include "common.h"

#include <cmath>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#endif

#include "conversion/scene_convert.h"
#include "shared/conversion_snapshot.h"
#include "shared/core_api.h"


static_assert(sizeof(struct creo_brl_snapshot_header) == CREO_BRL_SNAPSHOT_HEADER_SIZE,
              "Snapshot header layout changed");
static_assert(sizeof(struct creo_brl_snapshot_range) == CREO_BRL_SNAPSHOT_RANGE_SIZE,
              "Snapshot range layout changed");
static_assert(sizeof(struct creo_brl_snapshot_vector) == CREO_BRL_SNAPSHOT_VECTOR_SIZE,
              "Snapshot vector layout changed");
static_assert(sizeof(struct creo_brl_snapshot_triangle) == CREO_BRL_SNAPSHOT_TRIANGLE_SIZE,
              "Snapshot triangle layout changed");
static_assert(sizeof(struct creo_brl_snapshot_tgc_subtraction) == CREO_BRL_SNAPSHOT_TGC_SUBTRACTION_SIZE,
              "Snapshot TGC subtraction layout changed");
static_assert(sizeof(struct creo_brl_snapshot_named_value) == CREO_BRL_SNAPSHOT_NAMED_VALUE_SIZE,
              "Snapshot name/value layout changed");
static_assert(sizeof(struct creo_brl_snapshot_settings) == CREO_BRL_SNAPSHOT_SETTINGS_SIZE,
              "Snapshot settings layout changed");
static_assert(sizeof(struct creo_brl_snapshot_part) == CREO_BRL_SNAPSHOT_PART_SIZE,
              "Snapshot part layout changed");
static_assert(sizeof(struct creo_brl_snapshot_matrix) == CREO_BRL_SNAPSHOT_MATRIX_SIZE,
              "Snapshot matrix layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly_member) ==
                  CREO_BRL_SNAPSHOT_ASSEMBLY_MEMBER_SIZE,
              "Snapshot assembly member layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly) == CREO_BRL_SNAPSHOT_ASSEMBLY_SIZE,
              "Snapshot assembly layout changed");
static_assert(sizeof(struct creo_brl_snapshot_scene) == CREO_BRL_SNAPSHOT_SCENE_SIZE,
              "Snapshot scene layout changed");


static FILE *
snapshot_open(const char *snapshot_path)
{
#ifdef _WIN32
    const int wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                     snapshot_path, -1, NULL, 0);
    std::vector<wchar_t> wide_path;

    if (wide_path_length <= 0)
        return NULL;

    wide_path.resize((size_t)wide_path_length);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, snapshot_path, -1,
                            &wide_path[0], wide_path_length) != wide_path_length)
        return NULL;

    return _wfopen(&wide_path[0], L"rb");
#else
    return fopen(snapshot_path, "rb");
#endif
}


static int
snapshot_seek(FILE *snapshot_file, uint64_t offset)
{
    if (offset > INT64_MAX)
        return 0;

#ifdef _WIN32
    return _fseeki64(snapshot_file, (__int64)offset, SEEK_SET) == 0;
#else
    return fseek(snapshot_file, (long)offset, SEEK_SET) == 0;
#endif
}


static int
snapshot_file_size(FILE *snapshot_file, uint64_t *file_size)
{
#ifdef _WIN32
    __int64 end_offset = 0;

    if (_fseeki64(snapshot_file, 0, SEEK_END) != 0)
        return 0;
    end_offset = _ftelli64(snapshot_file);
    if (end_offset < 0 || _fseeki64(snapshot_file, 0, SEEK_SET) != 0)
        return 0;
#else
    long end_offset = 0;

    if (fseek(snapshot_file, 0, SEEK_END) != 0)
        return 0;
    end_offset = ftell(snapshot_file);
    if (end_offset < 0 || fseek(snapshot_file, 0, SEEK_SET) != 0)
        return 0;
#endif

    *file_size = (uint64_t)end_offset;
    return 1;
}


static int
snapshot_read(FILE *snapshot_file, void *buffer, size_t size)
{
    return fread(buffer, 1, size, snapshot_file) == size;
}


static int
snapshot_read_at(FILE *snapshot_file, uint64_t offset, void *buffer, size_t size)
{
    return snapshot_seek(snapshot_file, offset) && snapshot_read(snapshot_file, buffer, size);
}


static int
snapshot_range_is_empty(const struct creo_brl_snapshot_range *range)
{
    return range->offset == 0 && range->size == 0;
}


static int
snapshot_range_is_valid(const struct creo_brl_snapshot_range *range,
                        uint64_t snapshot_size,
                        uint64_t data_offset)
{
    if (range->size == 0)
        return range->offset == 0;

    return range->offset >= data_offset &&
           range->offset <= snapshot_size &&
           range->size <= snapshot_size - range->offset;
}


static int
snapshot_range_has_records(const struct creo_brl_snapshot_range *range,
                           uint64_t count,
                           size_t record_size,
                           uint64_t snapshot_size,
                           uint64_t data_offset)
{
    if (!snapshot_range_is_valid(range, snapshot_size, data_offset))
        return 0;
    if (count == 0)
        return snapshot_range_is_empty(range);
    if (count > UINT64_MAX / record_size)
        return 0;

    return range->size == count * record_size;
}


static int
snapshot_text_is_valid(FILE *snapshot_file,
                       const struct creo_brl_snapshot_range *range,
                       int required)
{
    uint64_t remaining = range->size;

    if (remaining == 0)
        return !required;
    if (!snapshot_seek(snapshot_file, range->offset))
        return 0;

    while (remaining > 0) {
        unsigned char byte = 0;

        if (!snapshot_read(snapshot_file, &byte, sizeof(byte)) || byte == '\0')
            return 0;
        --remaining;
    }

    return 1;
}


static int
snapshot_vector_is_finite(const struct creo_brl_snapshot_vector *vector)
{
    unsigned int coordinate = 0;

    for (coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
        if (!std::isfinite(vector->coordinates[coordinate]))
            return 0;
    }

    return 1;
}


static int
snapshot_matrix_is_finite(const struct creo_brl_snapshot_matrix *matrix)
{
    unsigned int element = 0;

    for (element = 0; element < CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT; ++element) {
        if (!std::isfinite(matrix->elements[element]))
            return 0;
    }

    return 1;
}


static int
snapshot_vectors_are_valid(FILE *snapshot_file,
                           const struct creo_brl_snapshot_range *range,
                           uint64_t count,
                           uint64_t snapshot_size,
                           uint64_t data_offset)
{
    uint64_t index = 0;

    if (!snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_vector),
                                    snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_vector vector = {};

        if (!snapshot_read(snapshot_file, &vector, sizeof(vector)) ||
            !snapshot_vector_is_finite(&vector))
            return 0;
    }

    return 1;
}


static int
snapshot_triangles_are_valid(FILE *snapshot_file,
                             const struct creo_brl_snapshot_range *range,
                             uint64_t count,
                             uint64_t vertex_count,
                             uint64_t snapshot_size,
                             uint64_t data_offset)
{
    uint64_t index = 0;

    if (!snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_triangle),
                                    snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_triangle triangle = {};
        unsigned int vertex = 0;

        if (!snapshot_read(snapshot_file, &triangle, sizeof(triangle)))
            return 0;
        for (vertex = 0; vertex < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++vertex) {
            if ((uint64_t)triangle.vertices[vertex] >= vertex_count)
                return 0;
        }
    }

    return 1;
}


static int
snapshot_indices_are_valid(FILE *snapshot_file,
                           const struct creo_brl_snapshot_range *range,
                           uint64_t count,
                           uint64_t maximum,
                           uint64_t snapshot_size,
                           uint64_t data_offset)
{
    uint64_t index = 0;

    if (!snapshot_range_has_records(range, count, sizeof(uint32_t),
                                    snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    for (index = 0; index < count; ++index) {
        uint32_t value = 0;

        if (!snapshot_read(snapshot_file, &value, sizeof(value)) || (uint64_t)value >= maximum)
            return 0;
    }

    return 1;
}


static int
snapshot_named_values_are_valid(FILE *snapshot_file,
                                const struct creo_brl_snapshot_range *range,
                                uint64_t count,
                                uint64_t snapshot_size,
                                uint64_t data_offset)
{
    uint64_t index = 0;

    if (!snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_named_value value = {};
        const uint64_t offset = range->offset + index * sizeof(value);

        if (!snapshot_read_at(snapshot_file, offset, &value, sizeof(value)) ||
            !snapshot_range_is_valid(&value.name, snapshot_size, data_offset) ||
            !snapshot_range_is_valid(&value.value, snapshot_size, data_offset) ||
            !snapshot_text_is_valid(snapshot_file, &value.name, 1) ||
            !snapshot_text_is_valid(snapshot_file, &value.value, 0))
            return 0;
    }

    return 1;
}


static int
snapshot_tgc_subtractions_are_valid(FILE *snapshot_file,
                                    const struct creo_brl_snapshot_range *range,
                                    uint64_t count,
                                    uint64_t snapshot_size,
                                    uint64_t data_offset)
{
    uint64_t index = 0;

    if (!snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_tgc_subtraction),
                                    snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_tgc_subtraction subtraction = {};
        const struct creo_brl_snapshot_vector *vectors[] = {
            &subtraction.base,
            &subtraction.height,
            &subtraction.a,
            &subtraction.b,
            &subtraction.c,
            &subtraction.d
        };

        if (!snapshot_read(snapshot_file, &subtraction, sizeof(subtraction)))
            return 0;
        for (size_t vector_index = 0; vector_index < sizeof(vectors) / sizeof(vectors[0]); ++vector_index) {
            if (!snapshot_vector_is_finite(vectors[vector_index]))
                return 0;
        }
    }

    return 1;
}


static int
snapshot_settings_are_valid(FILE *snapshot_file,
                            const struct creo_brl_snapshot_settings *settings,
                            uint64_t snapshot_size,
                            uint64_t data_offset)
{
    if ((settings->flags & ~CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK) != 0 ||
        settings->xform_mode < CREO_BRL_SNAPSHOT_XFORM_NONE ||
        settings->xform_mode > CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z ||
        settings->chord_mode < CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE ||
        settings->chord_mode > CREO_BRL_SNAPSHOT_CHORD_RELATIVE ||
        settings->log_type < CREO_BRL_SNAPSHOT_LOG_NONE ||
        settings->log_type > CREO_BRL_SNAPSHOT_LOG_ALL ||
        settings->min_luminance < 0 ||
        settings->min_luminance > CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM ||
        !std::isfinite(settings->min_angle) || settings->min_angle <= 0.0 ||
        settings->min_angle > CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM ||
        !std::isfinite(settings->max_chord) || settings->max_chord <= 0.0 ||
        (settings->chord_mode == CREO_BRL_SNAPSHOT_CHORD_RELATIVE &&
         settings->max_chord > CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM) ||
        !std::isfinite(settings->min_hole) || settings->min_hole < 0.0 ||
        !std::isfinite(settings->min_chamfer) || settings->min_chamfer < 0.0 ||
        !std::isfinite(settings->min_round) || settings->min_round < 0.0 ||
        !snapshot_range_is_valid(&settings->output_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->log_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->material_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->rename_parameters, snapshot_size, data_offset))
        return 0;

    return snapshot_text_is_valid(snapshot_file, &settings->output_path, 1) &&
           snapshot_text_is_valid(snapshot_file, &settings->log_path, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->material_path, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->rename_parameters, 0);
}


static int
snapshot_bbox_is_valid(const struct creo_brl_snapshot_part *part)
{
    unsigned int coordinate = 0;

    if (!snapshot_vector_is_finite(&part->bbox_min) ||
        !snapshot_vector_is_finite(&part->bbox_max))
        return 0;

    for (coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
        if (part->bbox_min.coordinates[coordinate] > part->bbox_max.coordinates[coordinate])
            return 0;
    }

    return 1;
}


static int
snapshot_part_is_valid(FILE *snapshot_file,
                       const struct creo_brl_snapshot_part *part,
                       uint64_t snapshot_size,
                       uint64_t data_offset)
{
    const int has_mesh = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_bbox = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0;
    const int has_normals = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    const int has_material = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL) != 0;

    if ((part->flags & ~CREO_BRL_SNAPSHOT_PART_FLAG_MASK) != 0 || part->region_id < 1 ||
        has_mesh == has_bbox || part->color_is_set !=
            ((part->flags & CREO_BRL_SNAPSHOT_PART_HAS_COLOR) != 0) ||
        !std::isfinite(part->bbox_diagonal) || part->bbox_diagonal < 0.0 ||
        !std::isfinite(part->bbox_volume) || part->bbox_volume < 0.0 ||
        !std::isfinite(part->bbox_area) || part->bbox_area < 0.0 ||
        !std::isfinite(part->density) ||
        !std::isfinite(part->mass) || !std::isfinite(part->volume) ||
        !std::isfinite(part->surface_area) ||
        !snapshot_bbox_is_valid(part) ||
        !snapshot_range_is_valid(&part->model_name, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&part->model_version, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&part->material_name, snapshot_size, data_offset) ||
        !snapshot_range_has_records(&part->tgc_subtractions, part->tgc_subtraction_count,
                                    sizeof(struct creo_brl_snapshot_tgc_subtraction),
                                    snapshot_size, data_offset) ||
        !snapshot_range_has_records(&part->attributes, part->attribute_count,
                                    sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_range_has_records(&part->parameters, part->parameter_count,
                                    sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_text_is_valid(snapshot_file, &part->model_name, 1) ||
        !snapshot_text_is_valid(snapshot_file, &part->model_version, 0) ||
        !snapshot_text_is_valid(snapshot_file, &part->material_name, has_material) ||
        !snapshot_tgc_subtractions_are_valid(snapshot_file, &part->tgc_subtractions,
                                             part->tgc_subtraction_count,
                                             snapshot_size, data_offset) ||
        !snapshot_named_values_are_valid(snapshot_file, &part->attributes, part->attribute_count,
                                         snapshot_size, data_offset) ||
        !snapshot_named_values_are_valid(snapshot_file, &part->parameters, part->parameter_count,
                                         snapshot_size, data_offset))
        return 0;

    if (!has_material && !snapshot_range_is_empty(&part->material_name))
        return 0;

    if (has_bbox)
        return !has_normals && part->vertex_count == 0 && part->triangle_count == 0 &&
               part->normal_count == 0 && part->normal_index_count == 0 &&
               snapshot_range_is_empty(&part->vertices) &&
               snapshot_range_is_empty(&part->triangles) &&
               snapshot_range_is_empty(&part->normals) &&
               snapshot_range_is_empty(&part->normal_indices);

    if (part->vertex_count < CREO_BRL_SNAPSHOT_MIN_VERTEX_COUNT ||
        part->triangle_count < CREO_BRL_SNAPSHOT_MIN_TRIANGLE_COUNT ||
        !snapshot_vectors_are_valid(snapshot_file, &part->vertices, part->vertex_count,
                                    snapshot_size, data_offset) ||
        !snapshot_triangles_are_valid(snapshot_file, &part->triangles, part->triangle_count,
                                      part->vertex_count, snapshot_size, data_offset))
        return 0;

    if (!has_normals)
        return part->normal_count == 0 && part->normal_index_count == 0 &&
               snapshot_range_is_empty(&part->normals) &&
               snapshot_range_is_empty(&part->normal_indices);

    if (part->normal_count == 0 ||
        part->triangle_count > UINT64_MAX / CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT ||
        part->normal_index_count !=
            part->triangle_count * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)
        return 0;

    return snapshot_vectors_are_valid(snapshot_file, &part->normals, part->normal_count,
                                       snapshot_size, data_offset) &&
           snapshot_indices_are_valid(snapshot_file, &part->normal_indices,
                                       part->normal_index_count, part->normal_count,
                                       snapshot_size, data_offset);
}


static int
snapshot_assembly_is_valid(FILE *snapshot_file,
                           const struct creo_brl_snapshot_assembly *assembly,
                           uint64_t part_count,
                           uint64_t assembly_count,
                           uint64_t snapshot_size,
                           uint64_t data_offset)
{
    uint64_t member_index = 0;

    if ((assembly->flags & ~CREO_BRL_SNAPSHOT_ASSEMBLY_FLAG_MASK) != 0 ||
        !std::isfinite(assembly->density) || !std::isfinite(assembly->mass) ||
        !std::isfinite(assembly->volume) || !std::isfinite(assembly->surface_area) ||
        !snapshot_range_is_valid(&assembly->model_name, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&assembly->model_version, snapshot_size, data_offset) ||
        assembly->member_count == 0 ||
        !snapshot_range_has_records(&assembly->members, assembly->member_count,
                                    sizeof(struct creo_brl_snapshot_assembly_member),
                                    snapshot_size, data_offset) ||
        !snapshot_range_has_records(&assembly->parameters, assembly->parameter_count,
                                    sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_text_is_valid(snapshot_file, &assembly->model_name, 1) ||
        !snapshot_text_is_valid(snapshot_file, &assembly->model_version, 0) ||
        !snapshot_named_values_are_valid(snapshot_file, &assembly->parameters,
                                         assembly->parameter_count,
                                         snapshot_size, data_offset) ||
        !snapshot_seek(snapshot_file, assembly->members.offset))
        return 0;

    for (member_index = 0; member_index < assembly->member_count; ++member_index) {
        struct creo_brl_snapshot_assembly_member member = {};

        if (!snapshot_read(snapshot_file, &member, sizeof(member)) ||
            !snapshot_matrix_is_finite(&member.matrix))
            return 0;

        if (member.target_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART) {
            if ((uint64_t)member.target_index >= part_count)
                return 0;
        } else if (member.target_type == CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY) {
            if ((uint64_t)member.target_index >= assembly_count)
                return 0;
        } else {
            return 0;
        }
    }

    return 1;
}


static int
snapshot_scene_is_valid(FILE *snapshot_file,
                        struct creo_brl_snapshot_scene *scene_out)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_scene scene = {};
    uint64_t file_size = 0;
    uint64_t part_records_offset = 0;
    uint64_t assembly_records_offset = 0;
    uint64_t data_offset = 0;
    uint64_t index = 0;

    if (!snapshot_file_size(snapshot_file, &file_size) ||
        !snapshot_read_at(snapshot_file, 0, &header, sizeof(header)) ||
        memcmp(header.magic, creo_brl_snapshot_magic, sizeof(header.magic)) != 0)
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if (header.snapshot_size != file_size ||
        header.snapshot_size < sizeof(header) + sizeof(scene) ||
        !snapshot_read_at(snapshot_file, sizeof(header), &scene, sizeof(scene)))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    part_records_offset = sizeof(header) + sizeof(scene);
    if (scene.part_count > UINT64_MAX / sizeof(struct creo_brl_snapshot_part) ||
        scene.part_count > (UINT64_MAX - part_records_offset) /
                           sizeof(struct creo_brl_snapshot_part) ||
        (scene.part_count == 0 ? !snapshot_range_is_empty(&scene.parts) :
         (scene.parts.offset != part_records_offset ||
          scene.parts.size != scene.part_count * sizeof(struct creo_brl_snapshot_part))))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    assembly_records_offset = part_records_offset +
                              scene.part_count * sizeof(struct creo_brl_snapshot_part);
    if (assembly_records_offset > file_size ||
        scene.assembly_count > UINT64_MAX / sizeof(struct creo_brl_snapshot_assembly) ||
        scene.assembly_count > (UINT64_MAX - assembly_records_offset) /
                               sizeof(struct creo_brl_snapshot_assembly) ||
        (scene.assembly_count == 0 ? !snapshot_range_is_empty(&scene.assemblies) :
         (scene.assemblies.offset != assembly_records_offset ||
          scene.assemblies.size != scene.assembly_count * sizeof(struct creo_brl_snapshot_assembly))))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    data_offset = assembly_records_offset +
                  scene.assembly_count * sizeof(struct creo_brl_snapshot_assembly);
    if (data_offset > file_size ||
        !snapshot_settings_are_valid(snapshot_file, &scene.settings, file_size, data_offset))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if ((scene.root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART &&
         (uint64_t)scene.root_index >= scene.part_count) ||
        (scene.root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY &&
         (uint64_t)scene.root_index >= scene.assembly_count) ||
        (scene.root_type != CREO_BRL_SNAPSHOT_SCENE_NODE_PART &&
         scene.root_type != CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    for (index = 0; index < scene.part_count; ++index) {
        struct creo_brl_snapshot_part part = {};
        const uint64_t offset = scene.parts.offset +
                                index * sizeof(struct creo_brl_snapshot_part);

        if (!snapshot_read_at(snapshot_file, offset, &part, sizeof(part)) ||
            !snapshot_part_is_valid(snapshot_file, &part, file_size, data_offset))
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
    }

    for (index = 0; index < scene.assembly_count; ++index) {
        struct creo_brl_snapshot_assembly assembly = {};
        const uint64_t offset = scene.assemblies.offset +
                                index * sizeof(struct creo_brl_snapshot_assembly);

        if (!snapshot_read_at(snapshot_file, offset, &assembly, sizeof(assembly)) ||
            !snapshot_assembly_is_valid(snapshot_file, &assembly, scene.part_count,
                                        scene.assembly_count, file_size, data_offset))
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
    }

    if (scene_out)
        *scene_out = scene;

    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


extern "C" CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL
creo_brl_core_convert(const char *snapshot_path)
{
    FILE *snapshot_file = NULL;
    struct creo_brl_snapshot_scene scene = {};
    int result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    snapshot_file = snapshot_open(snapshot_path);
    if (!snapshot_file)
        return CREO_BRL_CORE_CONVERT_OPEN_FAILED;

    result = snapshot_scene_is_valid(snapshot_file, &scene);
    if (result == CREO_BRL_CORE_CONVERT_SUCCESS)
        result = creo_brl_convert_snapshot_scene(snapshot_file, &scene);
    fclose(snapshot_file);

    return result;
}
