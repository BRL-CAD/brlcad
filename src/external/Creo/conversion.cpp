/**
 *                C O N V E R S I O N . C P P
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
 * @file conversion.cpp
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

#include "bu/file.h"
#include "bu/avs.h"
#include "conversion_snapshot.h"
#include "part_writer.h"
#include "wdb.h"


static const char CREO_BRL_CORE_SOLID_SUFFIX[] = ".s";
static const char CREO_BRL_CORE_RPP_SUFFIX[] = ".rpp";
static const char CREO_BRL_CORE_REGION_SUFFIX[] = ".r";
static const char CREO_BRL_CORE_UNKNOWN_NAME[] = "unknown";
static const char CREO_BRL_CORE_NAME_KEEP_CHARACTERS[] = "+-.=_";
static const char CREO_BRL_CORE_IMPORTER_ATTRIBUTE[] = "importer";
static const char CREO_BRL_CORE_IMPORTER_VALUE[] = "creo-g";
static const char CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE[] = "ptc_name";
static const char CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
static const char CREO_BRL_CORE_MATERIAL_ATTRIBUTE[] = "ptc_material_name";
static const char CREO_BRL_CORE_FALLBACK_ATTRIBUTE[] = "tess_fail";
static const char CREO_BRL_CORE_UNDEFINED_MATERIAL[] = "undefined";


static_assert(sizeof(struct creo_brl_snapshot_header) == CREO_BRL_SNAPSHOT_HEADER_V1_SIZE,
              "Snapshot header layout changed");
static_assert(sizeof(struct creo_brl_snapshot_range) == CREO_BRL_SNAPSHOT_RANGE_SIZE,
              "Snapshot range layout changed");
static_assert(sizeof(struct creo_brl_snapshot_vector) == CREO_BRL_SNAPSHOT_VECTOR_SIZE,
              "Snapshot vector layout changed");
static_assert(sizeof(struct creo_brl_snapshot_triangle) == CREO_BRL_SNAPSHOT_TRIANGLE_SIZE,
              "Snapshot triangle layout changed");
static_assert(sizeof(struct creo_brl_snapshot_named_value) == CREO_BRL_SNAPSHOT_NAMED_VALUE_SIZE,
              "Snapshot name/value layout changed");
static_assert(sizeof(struct creo_brl_snapshot_settings) == CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE,
              "Snapshot settings layout changed");
static_assert(sizeof(struct creo_brl_snapshot_part) == CREO_BRL_SNAPSHOT_PART_V1_SIZE,
              "Snapshot part layout changed");
static_assert(sizeof(struct creo_brl_snapshot_single_part) == CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE,
              "Snapshot payload layout changed");


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
snapshot_settings_are_valid(FILE *snapshot_file,
                            const struct creo_brl_snapshot_settings *settings,
                            uint64_t snapshot_size,
                            uint64_t data_offset)
{
    if (settings->structure_size != CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE ||
        settings->reserved != 0 ||
        (settings->flags & ~CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK) != 0 ||
        settings->xform_mode < CREO_BRL_SNAPSHOT_XFORM_NONE ||
        settings->xform_mode > CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z ||
        settings->chord_mode < CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE ||
        settings->chord_mode > CREO_BRL_SNAPSHOT_CHORD_RELATIVE ||
        settings->log_type < CREO_BRL_SNAPSHOT_LOG_NONE ||
        settings->log_type > CREO_BRL_SNAPSHOT_LOG_ALL ||
        !std::isfinite(settings->max_angle) || !std::isfinite(settings->min_angle) ||
        !std::isfinite(settings->max_chord) || !std::isfinite(settings->min_chord) ||
        !std::isfinite(settings->min_edge) || !std::isfinite(settings->min_hole) ||
        !std::isfinite(settings->min_chamfer) || !std::isfinite(settings->min_round) ||
        !snapshot_range_is_valid(&settings->output_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->log_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->material_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->stl_path, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->rename_parameters, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&settings->preserve_parameters, snapshot_size, data_offset))
        return 0;

    return snapshot_text_is_valid(snapshot_file, &settings->output_path, 1) &&
           snapshot_text_is_valid(snapshot_file, &settings->log_path, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->material_path, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->stl_path, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->rename_parameters, 0) &&
           snapshot_text_is_valid(snapshot_file, &settings->preserve_parameters, 0);
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

    if (part->structure_size != CREO_BRL_SNAPSHOT_PART_V1_SIZE ||
        part->reserved != 0 ||
        (part->flags & ~CREO_BRL_SNAPSHOT_PART_FLAG_MASK) != 0 ||
        has_mesh == has_bbox || part->color_is_set !=
            ((part->flags & CREO_BRL_SNAPSHOT_PART_HAS_COLOR) != 0) ||
        !std::isfinite(part->model_to_mm) || part->model_to_mm <= 0.0 ||
        !std::isfinite(part->bbox_diagonal) || !std::isfinite(part->bbox_volume) ||
        !std::isfinite(part->bbox_area) || !snapshot_bbox_is_valid(part) ||
        !snapshot_range_is_valid(&part->model_name, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&part->model_version, snapshot_size, data_offset) ||
        !snapshot_range_is_valid(&part->material_name, snapshot_size, data_offset) ||
        !snapshot_range_has_records(&part->attributes, part->attribute_count,
                                    sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_range_has_records(&part->parameters, part->parameter_count,
                                    sizeof(struct creo_brl_snapshot_named_value),
                                    snapshot_size, data_offset) ||
        !snapshot_text_is_valid(snapshot_file, &part->model_name, 1) ||
        !snapshot_text_is_valid(snapshot_file, &part->model_version, 0) ||
        !snapshot_text_is_valid(snapshot_file, &part->material_name, has_material))
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
snapshot_is_valid(FILE *snapshot_file,
                  struct creo_brl_snapshot_single_part *single_part_out)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_single_part single_part = {};
    uint64_t file_size = 0;
    const uint64_t data_offset = CREO_BRL_SNAPSHOT_HEADER_V1_SIZE +
                                 CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE;

    if (!snapshot_file_size(snapshot_file, &file_size) ||
        !snapshot_read_at(snapshot_file, 0, &header, sizeof(header)) ||
        memcmp(header.magic, creo_brl_snapshot_magic, sizeof(header.magic)) != 0)
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if (header.format_version != CREO_BRL_SNAPSHOT_FORMAT_VERSION ||
        header.header_size != CREO_BRL_SNAPSHOT_HEADER_V1_SIZE ||
        header.payload_type != CREO_BRL_SNAPSHOT_PAYLOAD_SINGLE_PART)
        return CREO_BRL_CORE_CONVERT_UNSUPPORTED_SNAPSHOT;

    if (header.reserved != 0 || header.snapshot_size != file_size ||
        header.snapshot_size < data_offset ||
        header.payload_size != header.snapshot_size - header.header_size ||
        header.payload_size < CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE ||
        !snapshot_read_at(snapshot_file, header.header_size, &single_part, sizeof(single_part)) ||
        single_part.structure_size != CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE ||
        single_part.reserved != 0 ||
        !snapshot_settings_are_valid(snapshot_file, &single_part.settings, file_size, data_offset) ||
        !snapshot_part_is_valid(snapshot_file, &single_part.part, file_size, data_offset))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if (single_part_out)
        *single_part_out = single_part;

    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static int
snapshot_count_as_size(uint64_t count, size_t multiplier, size_t *size)
{
    if (multiplier == 0 || count > SIZE_MAX / multiplier)
        return 0;

    *size = (size_t)count * multiplier;
    return 1;
}


static int
snapshot_read_text(FILE *snapshot_file,
                   const struct creo_brl_snapshot_range *range,
                   std::string *text)
{
    if (range->size > SIZE_MAX)
        return 0;

    text->resize((size_t)range->size);
    return (range->size == 0 ||
            snapshot_read_at(snapshot_file, range->offset, &(*text)[0], text->size())) &&
           text->find('\0') == std::string::npos;
}


static int
snapshot_read_vectors(FILE *snapshot_file,
                      const struct creo_brl_snapshot_range *range,
                      uint64_t count,
                      std::vector<fastf_t> *vectors)
{
    size_t coordinate_count = 0;
    uint64_t index = 0;

    if (!snapshot_count_as_size(count, CREO_BRL_SNAPSHOT_COORDINATE_COUNT, &coordinate_count) ||
        coordinate_count > vectors->max_size() ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    vectors->resize(coordinate_count);
    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_vector vector = {};
        size_t coordinate = 0;

        if (!snapshot_read(snapshot_file, &vector, sizeof(vector)) ||
            !snapshot_vector_is_finite(&vector))
            return 0;

        for (coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate)
            (*vectors)[(size_t)index * CREO_BRL_SNAPSHOT_COORDINATE_COUNT + coordinate] =
                (fastf_t)vector.coordinates[coordinate];
    }

    return 1;
}


static int
snapshot_read_triangles(FILE *snapshot_file,
                        const struct creo_brl_snapshot_range *range,
                        uint64_t count,
                        uint64_t vertex_count,
                        std::vector<int> *triangles)
{
    size_t index_count = 0;
    uint64_t index = 0;

    if (!snapshot_count_as_size(count, CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT, &index_count) ||
        index_count > triangles->max_size() ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    triangles->resize(index_count);
    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_triangle triangle = {};
        size_t vertex = 0;

        if (!snapshot_read(snapshot_file, &triangle, sizeof(triangle)))
            return 0;

        for (vertex = 0; vertex < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++vertex) {
            if ((uint64_t)triangle.vertices[vertex] >= vertex_count ||
                triangle.vertices[vertex] > INT_MAX)
                return 0;
            (*triangles)[(size_t)index * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT + vertex] =
                (int)triangle.vertices[vertex];
        }
    }

    return 1;
}


static int
snapshot_read_indices(FILE *snapshot_file,
                      const struct creo_brl_snapshot_range *range,
                      uint64_t count,
                      uint64_t maximum,
                      std::vector<int> *indices)
{
    size_t index_count = 0;
    uint64_t index = 0;

    if (!snapshot_count_as_size(count, 1, &index_count) ||
        index_count > indices->max_size() ||
        !snapshot_seek(snapshot_file, range->offset))
        return 0;

    indices->resize(index_count);
    for (index = 0; index < count; ++index) {
        uint32_t value = 0;

        if (!snapshot_read(snapshot_file, &value, sizeof(value)) ||
            (uint64_t)value >= maximum || value > INT_MAX)
            return 0;
        (*indices)[(size_t)index] = (int)value;
    }

    return 1;
}


static std::string
snapshot_name_root(const std::string& model_name)
{
    std::string name;
    size_t index = 0;

    name.reserve(model_name.size());
    for (index = 0; index < model_name.size(); ++index) {
        const unsigned char character = (unsigned char)model_name[index];
        const int is_uppercase = character >= 'A' && character <= 'Z';
        const int is_lowercase = character >= 'a' && character <= 'z';
        const int is_digit = character >= '0' && character <= '9';

        if (is_uppercase)
            name.push_back((char)(character - 'A' + 'a'));
        else if (is_lowercase || is_digit ||
                 strchr(CREO_BRL_CORE_NAME_KEEP_CHARACTERS, character))
            name.push_back((char)character);
        else if (name.empty() || name[name.size() - 1] != '_')
            name.push_back('_');
    }

    if (name.empty())
        name.assign(CREO_BRL_CORE_UNKNOWN_NAME);

    return name;
}


static int
snapshot_object_names(struct db_i *database,
                      const std::string& name_root,
                      const char *solid_suffix,
                      std::string *solid_name,
                      std::string *region_name)
{
    uint32_t suffix = 0;

    for (;;) {
        std::string object_root = name_root;

        if (suffix > 0)
            object_root += "_" + std::to_string(suffix);

        *solid_name = object_root + solid_suffix;
        *region_name = object_root + CREO_BRL_CORE_REGION_SUFFIX;
        if (db_lookup(database, solid_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL &&
            db_lookup(database, region_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
            return 1;

        if (suffix == UINT32_MAX)
            return 0;
        ++suffix;
    }
}


static int
snapshot_open_output(const char *output_path, struct db_i **database)
{
    if (bu_file_exists(output_path, NULL))
        *database = db_open(output_path, DB_OPEN_READWRITE);
    else
        *database = db_create(output_path, BRLCAD_DB_FORMAT_LATEST);

    return *database != DBI_NULL;
}


static int
snapshot_write_attributes(struct db_i *database,
                          const char *solid_name,
                          const char *region_name,
                          const std::string& model_name,
                          const std::string& model_version,
                          const std::string& material_name,
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
    if (!has_mesh)
        bu_avs_add(&attributes, CREO_BRL_CORE_FALLBACK_ATTRIBUTE, model_name.c_str());
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
    result = db5_update_attributes(region, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
snapshot_write_part(FILE *snapshot_file,
                    const struct creo_brl_snapshot_single_part *single_part)
{
    const struct creo_brl_snapshot_part *part = &single_part->part;
    const int has_mesh = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_normals = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    std::string output_path;
    std::string model_name;
    std::string model_version;
    std::string material_name;
    std::string solid_name;
    std::string region_name;
    std::vector<fastf_t> vertices;
    std::vector<int> triangles;
    std::vector<fastf_t> normals;
    std::vector<int> normal_indices;
    struct db_i *database = DBI_NULL;
    struct rt_wdb *writer = RT_WDB_NULL;
    struct bu_list members;
    int result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    try {
        if (!snapshot_read_text(snapshot_file, &single_part->settings.output_path, &output_path) ||
            !snapshot_read_text(snapshot_file, &part->model_name, &model_name) ||
            !snapshot_read_text(snapshot_file, &part->model_version, &model_version) ||
            !snapshot_read_text(snapshot_file, &part->material_name, &material_name) ||
            (has_mesh &&
             (!snapshot_read_vectors(snapshot_file, &part->vertices, part->vertex_count, &vertices) ||
              !snapshot_read_triangles(snapshot_file, &part->triangles, part->triangle_count,
                                       part->vertex_count, &triangles) ||
              (has_normals &&
             (!snapshot_read_vectors(snapshot_file, &part->normals, part->normal_count, &normals) ||
              !snapshot_read_indices(snapshot_file, &part->normal_indices,
                                     part->normal_index_count, part->normal_count,
                                     &normal_indices)))))) {
            result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
        } else if (!snapshot_open_output(output_path.c_str(), &database) ||
                   !(writer = wdb_dbopen(database, RT_WDB_TYPE_DB_DISK)) ||
                   !snapshot_object_names(database, snapshot_name_root(model_name),
                                          has_mesh ? CREO_BRL_CORE_SOLID_SUFFIX : CREO_BRL_CORE_RPP_SUFFIX,
                                          &solid_name, &region_name) ||
                   (has_mesh ?
                    creo_brl_write_bot(writer, solid_name.c_str(), has_normals,
                                       (size_t)part->vertex_count, (size_t)part->triangle_count,
                                       vertices.data(), triangles.data(),
                                       has_normals ? (size_t)part->normal_count : 0,
                                       has_normals ? normals.data() : NULL,
                                       has_normals ? normal_indices.data() : NULL) :
                    mk_rpp(writer, solid_name.c_str(), part->bbox_min.coordinates,
                           part->bbox_max.coordinates)) != 0) {
            result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        } else {
            BU_LIST_INIT(&members);
            if (mk_addmember(solid_name.c_str(), &members, NULL, WMOP_UNION) == WMEMBER_NULL) {
                mk_freemembers(&members);
            } else if (mk_comb(writer, region_name.c_str(), &members, 1, NULL, NULL,
                               part->color_is_set ? part->color : NULL,
                               (int)part->region_id, 0, 0, 0, 0, 0, 0) == 0 &&
                       snapshot_write_attributes(database, solid_name.c_str(), region_name.c_str(),
                                                 model_name, model_version, material_name, has_mesh)) {
                result = CREO_BRL_CORE_CONVERT_SUCCESS;
            }
        }
    } catch (...) {
        /* The exported C ABI cannot propagate C++ exceptions across the DLL boundary. */
        result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
    }

    if (database != DBI_NULL)
        db_close(database);

    return result;
}


extern "C" __declspec(dllexport) int
creo_brl_core_convert(const char *snapshot_path)
{
    FILE *snapshot_file = NULL;
    struct creo_brl_snapshot_single_part single_part = {};
    int result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    snapshot_file = snapshot_open(snapshot_path);
    if (!snapshot_file)
        return CREO_BRL_CORE_CONVERT_OPEN_FAILED;

    result = snapshot_is_valid(snapshot_file, &single_part);
    if (result == CREO_BRL_CORE_CONVERT_SUCCESS) {
        result = snapshot_write_part(snapshot_file, &single_part);
    }
    fclose(snapshot_file);

    return result;
}
