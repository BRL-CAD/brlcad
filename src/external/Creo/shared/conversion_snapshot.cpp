/**
 *        C O N V E R S I O N _ S N A P S H O T . C P P
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
 * @file shared/conversion_snapshot.cpp
 *
 * Toolkit-free binary snapshot serialization, validation, and single-pass
 * deserialization.
 */

#include "common.h"

#include "shared/conversion_snapshot.h"

#include <cmath>
#include <cstring>
#include <limits.h>
#include <stdio.h>

#ifdef _WIN32
#  include <windows.h>
#endif


#define CREO_BRL_SNAPSHOT_HEADER_SIZE 16u
#define CREO_BRL_SNAPSHOT_RANGE_SIZE 16u
#define CREO_BRL_SNAPSHOT_VECTOR_SIZE 24u
#define CREO_BRL_SNAPSHOT_TRIANGLE_SIZE 12u
#define CREO_BRL_SNAPSHOT_TGC_SUBTRACTION_SIZE 144u
#define CREO_BRL_SNAPSHOT_NAMED_VALUE_SIZE 32u
#define CREO_BRL_SNAPSHOT_SETTINGS_SIZE 128u
#define CREO_BRL_SNAPSHOT_PART_SIZE 332u
#define CREO_BRL_SNAPSHOT_MATRIX_SIZE 128u
#define CREO_BRL_SNAPSHOT_ASSEMBLY_MEMBER_SIZE 136u
#define CREO_BRL_SNAPSHOT_ASSEMBLY_SIZE 116u
#define CREO_BRL_SNAPSHOT_SCENE_SIZE 184u

static const unsigned char creo_brl_snapshot_magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE] = {
    'C', 'R', 'E', 'O', 'B', 'R', 'L', '\0'
};

#pragma pack(push, 1)

struct snapshot_disk_range {
    uint64_t offset;
    uint64_t size;
};

struct snapshot_disk_named_value {
    struct snapshot_disk_range name;
    struct snapshot_disk_range value;
};

struct snapshot_disk_header {
    unsigned char magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE];
    uint64_t snapshot_size;
};

struct snapshot_disk_settings {
    uint32_t flags;
    int32_t xform_mode;
    int32_t chord_mode;
    int32_t region_id;
    int32_t min_luminance;
    int32_t log_type;
    double min_angle;
    double max_chord;
    double min_hole;
    double min_chamfer;
    double min_round;
    struct snapshot_disk_range output_path;
    struct snapshot_disk_range log_path;
    struct snapshot_disk_range material_path;
    struct snapshot_disk_range rename_parameters;
};

struct snapshot_disk_part {
    uint32_t flags;
    int32_t region_id;
    double bbox_diagonal;
    double bbox_volume;
    double bbox_area;
    unsigned char color[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT];
    unsigned char color_is_set;
    double density;
    double mass;
    double volume;
    double surface_area;
    struct creo_brl_snapshot_vector bbox_min;
    struct creo_brl_snapshot_vector bbox_max;
    struct snapshot_disk_range model_name;
    struct snapshot_disk_range model_version;
    struct snapshot_disk_range material_name;
    struct snapshot_disk_range vertices;
    uint64_t vertex_count;
    struct snapshot_disk_range triangles;
    uint64_t triangle_count;
    struct snapshot_disk_range normals;
    uint64_t normal_count;
    struct snapshot_disk_range normal_indices;
    uint64_t normal_index_count;
    struct snapshot_disk_range tgc_subtractions;
    uint64_t tgc_subtraction_count;
    struct snapshot_disk_range attributes;
    uint64_t attribute_count;
    struct snapshot_disk_range parameters;
    uint64_t parameter_count;
};

struct snapshot_disk_assembly {
    uint32_t flags;
    double density;
    double mass;
    double volume;
    double surface_area;
    struct snapshot_disk_range model_name;
    struct snapshot_disk_range model_version;
    struct snapshot_disk_range members;
    uint64_t member_count;
    struct snapshot_disk_range parameters;
    uint64_t parameter_count;
};

struct snapshot_disk_scene {
    struct snapshot_disk_settings settings;
    struct snapshot_disk_range parts;
    uint64_t part_count;
    struct snapshot_disk_range assemblies;
    uint64_t assembly_count;
    uint32_t root_type;
    uint32_t root_index;
};

#pragma pack(pop)

static_assert(sizeof(struct snapshot_disk_header) == CREO_BRL_SNAPSHOT_HEADER_SIZE,
              "Snapshot header layout changed");
static_assert(sizeof(struct snapshot_disk_range) == CREO_BRL_SNAPSHOT_RANGE_SIZE,
              "Snapshot range layout changed");
static_assert(sizeof(struct creo_brl_snapshot_vector) == CREO_BRL_SNAPSHOT_VECTOR_SIZE,
              "Snapshot vector layout changed");
static_assert(sizeof(struct creo_brl_snapshot_triangle) == CREO_BRL_SNAPSHOT_TRIANGLE_SIZE,
              "Snapshot triangle layout changed");
static_assert(sizeof(struct creo_brl_snapshot_tgc_subtraction) == CREO_BRL_SNAPSHOT_TGC_SUBTRACTION_SIZE,
              "Snapshot TGC subtraction layout changed");
static_assert(sizeof(struct snapshot_disk_named_value) == CREO_BRL_SNAPSHOT_NAMED_VALUE_SIZE,
              "Snapshot name/value layout changed");
static_assert(sizeof(struct snapshot_disk_settings) == CREO_BRL_SNAPSHOT_SETTINGS_SIZE,
              "Snapshot settings layout changed");
static_assert(sizeof(struct snapshot_disk_part) == CREO_BRL_SNAPSHOT_PART_SIZE,
              "Snapshot part layout changed");
static_assert(sizeof(struct creo_brl_snapshot_matrix) == CREO_BRL_SNAPSHOT_MATRIX_SIZE,
              "Snapshot matrix layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly_member) ==
                  CREO_BRL_SNAPSHOT_ASSEMBLY_MEMBER_SIZE,
              "Snapshot assembly member layout changed");
static_assert(sizeof(struct snapshot_disk_assembly) == CREO_BRL_SNAPSHOT_ASSEMBLY_SIZE,
              "Snapshot assembly layout changed");
static_assert(sizeof(struct snapshot_disk_scene) == CREO_BRL_SNAPSHOT_SCENE_SIZE,
              "Snapshot scene layout changed");


static FILE *
snapshot_open_file(const char *snapshot_path, const wchar_t *wide_mode, const char *mode)
{
    if (!snapshot_path || !snapshot_path[0] || !mode)
        return NULL;

#ifdef _WIN32
    {
        const int wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                         snapshot_path, -1, NULL, 0);
        std::vector<wchar_t> wide_path;

        if (!wide_mode || wide_path_length <= 0)
            return NULL;

        wide_path.resize((size_t)wide_path_length);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, snapshot_path, -1,
                                &wide_path[0], wide_path_length) != wide_path_length)
            return NULL;

        return _wfopen(&wide_path[0], wide_mode);
    }
#else
    (void)wide_mode;
    return fopen(snapshot_path, mode);
#endif
}


static int
buffer_append_data(std::vector<unsigned char> *contents,
                   const void *data,
                   size_t data_size,
                   struct snapshot_disk_range *range)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);

    if (!contents || !range)
        return 0;

    if (data_size == 0) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    if (!bytes || data_size > contents->max_size() - contents->size())
        return 0;

    range->offset = (uint64_t)contents->size();
    range->size = (uint64_t)data_size;
    contents->insert(contents->end(), bytes, bytes + data_size);
    return 1;
}


static int
buffer_append_record(std::vector<unsigned char> *contents,
                     const void *record,
                     size_t record_size)
{
    struct snapshot_disk_range unused_range = {};

    return buffer_append_data(contents, record, record_size, &unused_range);
}


static int
buffer_replace_record(std::vector<unsigned char> *contents,
                      size_t offset,
                      const void *record,
                      size_t record_size)
{
    if (!contents || !record || offset > contents->size() ||
        record_size > contents->size() - offset)
        return 0;

    std::memcpy(&(*contents)[offset], record, record_size);
    return 1;
}


static int
buffer_append_text(std::vector<unsigned char> *contents,
                   const std::string& text,
                   struct snapshot_disk_range *range)
{
    return buffer_append_data(contents,
                              text.empty() ? NULL : text.data(),
                              text.size(),
                              range);
}


static int
buffer_append_named_values(std::vector<unsigned char> *contents,
                           const std::vector<struct snapshot_named_value>& values,
                           struct snapshot_disk_range *range,
                           uint64_t *count)
{
    const size_t records_offset = contents ? contents->size() : 0;
    std::vector<struct snapshot_disk_named_value> records(values.size());

    if (!contents || !range || !count)
        return 0;

    *count = values.size();
    if (values.empty()) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    if (!buffer_append_data(contents, records.data(),
                            values.size() * sizeof(records[0]), range))
        return 0;

    for (size_t index = 0; index < values.size(); ++index) {
        if (!buffer_append_text(contents, values[index].name, &records[index].name) ||
            !buffer_append_text(contents, values[index].value, &records[index].value) ||
            !buffer_replace_record(contents, records_offset + index * sizeof(records[index]),
                                   &records[index], sizeof(records[index])))
            return 0;
    }

    return 1;
}


static void
copy_settings_to_disk(const struct creo_brl_snapshot_settings& source,
                      struct snapshot_disk_settings *destination)
{
    destination->flags = source.flags;
    destination->xform_mode = source.xform_mode;
    destination->chord_mode = source.chord_mode;
    destination->region_id = source.region_id;
    destination->min_luminance = source.min_luminance;
    destination->log_type = source.log_type;
    destination->min_angle = source.min_angle;
    destination->max_chord = source.max_chord;
    destination->min_hole = source.min_hole;
    destination->min_chamfer = source.min_chamfer;
    destination->min_round = source.min_round;
}


static void
copy_settings_from_disk(const struct snapshot_disk_settings& source,
                        struct creo_brl_snapshot_settings *destination)
{
    destination->flags = source.flags;
    destination->xform_mode = source.xform_mode;
    destination->chord_mode = source.chord_mode;
    destination->region_id = source.region_id;
    destination->min_luminance = source.min_luminance;
    destination->log_type = source.log_type;
    destination->min_angle = source.min_angle;
    destination->max_chord = source.max_chord;
    destination->min_hole = source.min_hole;
    destination->min_chamfer = source.min_chamfer;
    destination->min_round = source.min_round;
}


static void
copy_part_to_disk(const struct creo_brl_snapshot_part& source,
                  struct snapshot_disk_part *destination)
{
    destination->flags = source.flags;
    destination->region_id = source.region_id;
    destination->bbox_diagonal = source.bbox_diagonal;
    destination->bbox_volume = source.bbox_volume;
    destination->bbox_area = source.bbox_area;
    std::memcpy(destination->color, source.color, sizeof(destination->color));
    destination->color_is_set = source.color_is_set;
    destination->density = source.density;
    destination->mass = source.mass;
    destination->volume = source.volume;
    destination->surface_area = source.surface_area;
    destination->bbox_min = source.bbox_min;
    destination->bbox_max = source.bbox_max;
}


static void
copy_part_from_disk(const struct snapshot_disk_part& source,
                    struct creo_brl_snapshot_part *destination)
{
    destination->flags = source.flags;
    destination->region_id = source.region_id;
    destination->bbox_diagonal = source.bbox_diagonal;
    destination->bbox_volume = source.bbox_volume;
    destination->bbox_area = source.bbox_area;
    std::memcpy(destination->color, source.color, sizeof(destination->color));
    destination->color_is_set = source.color_is_set;
    destination->density = source.density;
    destination->mass = source.mass;
    destination->volume = source.volume;
    destination->surface_area = source.surface_area;
    destination->bbox_min = source.bbox_min;
    destination->bbox_max = source.bbox_max;
}


static void
copy_assembly_to_disk(const struct creo_brl_snapshot_assembly& source,
                      struct snapshot_disk_assembly *destination)
{
    destination->flags = source.flags;
    destination->density = source.density;
    destination->mass = source.mass;
    destination->volume = source.volume;
    destination->surface_area = source.surface_area;
}


static void
copy_assembly_from_disk(const struct snapshot_disk_assembly& source,
                        struct creo_brl_snapshot_assembly *destination)
{
    destination->flags = source.flags;
    destination->density = source.density;
    destination->mass = source.mass;
    destination->volume = source.volume;
    destination->surface_area = source.surface_area;
}


static int
buffer_append_part_capture(std::vector<unsigned char> *contents,
                           const struct snapshot_part_capture& capture,
                           struct snapshot_disk_part *disk_part)
{
    if (!contents || !disk_part)
        return 0;

    disk_part->vertex_count = capture.vertices.size();
    disk_part->triangle_count = capture.triangles.size();
    disk_part->normal_count = capture.normals.size();
    disk_part->normal_index_count = capture.normal_indices.size();
    disk_part->tgc_subtraction_count = capture.tgc_subtractions.size();

    return buffer_append_text(contents, capture.strings.model_name, &disk_part->model_name) &&
           buffer_append_text(contents, capture.strings.model_version, &disk_part->model_version) &&
           buffer_append_text(contents, capture.strings.material_name, &disk_part->material_name) &&
           buffer_append_data(contents,
                              capture.vertices.empty() ? NULL : capture.vertices.data(),
                              capture.vertices.size() * sizeof(capture.vertices[0]),
                              &disk_part->vertices) &&
           buffer_append_data(contents,
                              capture.triangles.empty() ? NULL : capture.triangles.data(),
                              capture.triangles.size() * sizeof(capture.triangles[0]),
                              &disk_part->triangles) &&
           buffer_append_data(contents,
                              capture.normals.empty() ? NULL : capture.normals.data(),
                              capture.normals.size() * sizeof(capture.normals[0]),
                              &disk_part->normals) &&
           buffer_append_data(contents,
                              capture.normal_indices.empty() ? NULL : capture.normal_indices.data(),
                              capture.normal_indices.size() * sizeof(capture.normal_indices[0]),
                              &disk_part->normal_indices) &&
           buffer_append_data(contents,
                              capture.tgc_subtractions.empty() ? NULL : capture.tgc_subtractions.data(),
                              capture.tgc_subtractions.size() * sizeof(capture.tgc_subtractions[0]),
                              &disk_part->tgc_subtractions) &&
           buffer_append_named_values(contents, capture.attributes, &disk_part->attributes,
                                      &disk_part->attribute_count) &&
           buffer_append_named_values(contents, capture.parameters, &disk_part->parameters,
                                      &disk_part->parameter_count);
}


int
creo_brl_snapshot_encode(const struct snapshot_scene_capture& scene,
                         std::vector<unsigned char> *buffer)
{
    if (!buffer)
        return CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST;

    try {
        struct snapshot_disk_header header = {};
        struct snapshot_disk_scene disk_scene = {};
        std::vector<struct snapshot_disk_part> disk_parts(scene.parts.size());
        std::vector<struct snapshot_disk_assembly> disk_assemblies(scene.assemblies.size());
        const size_t header_offset = 0;
        const size_t scene_offset = sizeof(header);
        const size_t part_offset = sizeof(header) + sizeof(disk_scene);
        const size_t assembly_offset = part_offset + disk_parts.size() * sizeof(disk_parts[0]);

        buffer->clear();
        std::memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
        copy_settings_to_disk(scene.settings, &disk_scene.settings);
        disk_scene.part_count = disk_parts.size();
        disk_scene.assembly_count = disk_assemblies.size();
        disk_scene.root_type = scene.root_type;
        disk_scene.root_index = scene.root_index;

        for (size_t index = 0; index < scene.parts.size(); ++index)
            copy_part_to_disk(scene.parts[index].part, &disk_parts[index]);
        for (size_t index = 0; index < scene.assemblies.size(); ++index)
            copy_assembly_to_disk(scene.assemblies[index].assembly, &disk_assemblies[index]);

        if (!buffer_append_record(buffer, &header, sizeof(header)) ||
            !buffer_append_record(buffer, &disk_scene, sizeof(disk_scene)) ||
            !buffer_append_data(buffer,
                                disk_parts.empty() ? NULL : disk_parts.data(),
                                disk_parts.size() * sizeof(disk_parts[0]),
                                &disk_scene.parts) ||
            !buffer_append_data(buffer,
                                disk_assemblies.empty() ? NULL : disk_assemblies.data(),
                                disk_assemblies.size() * sizeof(disk_assemblies[0]),
                                &disk_scene.assemblies) ||
            !buffer_append_text(buffer, scene.settings_strings.output_path,
                                &disk_scene.settings.output_path) ||
            !buffer_append_text(buffer, scene.settings_strings.log_path,
                                &disk_scene.settings.log_path) ||
            !buffer_append_text(buffer, scene.settings_strings.material_path,
                                &disk_scene.settings.material_path) ||
            !buffer_append_text(buffer, scene.settings_strings.rename_parameters,
                                &disk_scene.settings.rename_parameters))
            return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;

        for (size_t index = 0; index < scene.parts.size(); ++index) {
            if (!buffer_append_part_capture(buffer, scene.parts[index], &disk_parts[index]) ||
                !buffer_replace_record(buffer, part_offset + index * sizeof(disk_parts[index]),
                                       &disk_parts[index], sizeof(disk_parts[index])))
                return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;
        }

        for (size_t index = 0; index < scene.assemblies.size(); ++index) {
            const struct snapshot_assembly_capture& assembly = scene.assemblies[index];

            disk_assemblies[index].member_count = assembly.members.size();
            if (!buffer_append_text(buffer, assembly.model_name,
                                    &disk_assemblies[index].model_name) ||
                !buffer_append_text(buffer, assembly.model_version,
                                    &disk_assemblies[index].model_version) ||
                !buffer_append_data(buffer,
                                    assembly.members.empty() ? NULL : assembly.members.data(),
                                    assembly.members.size() * sizeof(assembly.members[0]),
                                    &disk_assemblies[index].members) ||
                !buffer_append_named_values(buffer, assembly.parameters,
                                            &disk_assemblies[index].parameters,
                                            &disk_assemblies[index].parameter_count) ||
                !buffer_replace_record(buffer,
                                       assembly_offset + index * sizeof(disk_assemblies[index]),
                                       &disk_assemblies[index], sizeof(disk_assemblies[index])))
                return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;
        }

        header.snapshot_size = (uint64_t)buffer->size();
        if (!buffer_replace_record(buffer, header_offset, &header, sizeof(header)) ||
            !buffer_replace_record(buffer, scene_offset, &disk_scene, sizeof(disk_scene)))
            return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;

        return CREO_BRL_SNAPSHOT_IO_SUCCESS;
    } catch (...) {
        return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;
    }
}


int
creo_brl_snapshot_write(const char *snapshot_path,
                        const struct snapshot_scene_capture& scene)
{
    std::vector<unsigned char> buffer;
    FILE *snapshot_file = NULL;
    int encode_result = CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;
    int wrote_all = 0;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST;

    encode_result = creo_brl_snapshot_encode(scene, &buffer);
    if (encode_result != CREO_BRL_SNAPSHOT_IO_SUCCESS)
        return encode_result;

    snapshot_file = snapshot_open_file(snapshot_path, L"wb", "wb");
    if (!snapshot_file)
        return CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;

    wrote_all = buffer.empty() ||
                fwrite(buffer.data(), 1, buffer.size(), snapshot_file) == buffer.size();
    fclose(snapshot_file);

    return wrote_all ? CREO_BRL_SNAPSHOT_IO_SUCCESS : CREO_BRL_SNAPSHOT_IO_WRITE_FAILED;
}


static int
snapshot_seek(FILE *snapshot_file, uint64_t offset)
{
    if (!snapshot_file || offset > INT64_MAX)
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
snapshot_read_bytes(FILE *snapshot_file, void *buffer, size_t size)
{
    return size == 0 ||
           (snapshot_file && buffer && fread(buffer, 1, size, snapshot_file) == size);
}


static int
snapshot_read_at(FILE *snapshot_file, uint64_t offset, void *buffer, size_t size)
{
    return snapshot_seek(snapshot_file, offset) && snapshot_read_bytes(snapshot_file, buffer, size);
}


static int
snapshot_range_is_empty(const struct snapshot_disk_range *range)
{
    return range->offset == 0 && range->size == 0;
}


static int
snapshot_range_is_valid(const struct snapshot_disk_range *range,
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
snapshot_range_has_records(const struct snapshot_disk_range *range,
                           uint64_t count,
                           size_t record_size,
                           uint64_t snapshot_size,
                           uint64_t data_offset)
{
    if (!snapshot_range_is_valid(range, snapshot_size, data_offset))
        return 0;
    if (count == 0)
        return snapshot_range_is_empty(range);
    if (count > SIZE_MAX / record_size || count > UINT64_MAX / record_size)
        return 0;

    return range->size == count * record_size;
}


static int
snapshot_read_valid_text(FILE *snapshot_file,
                         const struct snapshot_disk_range *range,
                         uint64_t snapshot_size,
                         uint64_t data_offset,
                         int required,
                         std::string *text)
{
    if (!range || !text || !snapshot_range_is_valid(range, snapshot_size, data_offset) ||
        range->size > SIZE_MAX)
        return 0;

    if (range->size == 0) {
        text->clear();
        return !required;
    }

    text->resize((size_t)range->size);
    if (!snapshot_read_at(snapshot_file, range->offset, &(*text)[0], text->size()))
        return 0;

    return text->find('\0') == std::string::npos;
}


static int
snapshot_vector_is_finite(const struct creo_brl_snapshot_vector *vector)
{
    for (unsigned int coordinate = 0;
         coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
        if (!std::isfinite(vector->coordinates[coordinate]))
            return 0;
    }

    return 1;
}


static int
snapshot_matrix_is_finite(const struct creo_brl_snapshot_matrix *matrix)
{
    for (unsigned int element = 0;
         element < CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT; ++element) {
        if (!std::isfinite(matrix->elements[element]))
            return 0;
    }

    return 1;
}


static int
snapshot_read_valid_vectors(FILE *snapshot_file,
                            const struct snapshot_disk_range *range,
                            uint64_t count,
                            uint64_t snapshot_size,
                            uint64_t data_offset,
                            std::vector<struct creo_brl_snapshot_vector> *vectors)
{
    if (!vectors ||
        !snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_vector),
                                    snapshot_size, data_offset) ||
        count > vectors->max_size())
        return 0;

    vectors->resize((size_t)count);
    if (count == 0)
        return 1;
    if (!snapshot_read_at(snapshot_file, range->offset, vectors->data(),
                          (size_t)count * sizeof(struct creo_brl_snapshot_vector)))
        return 0;

    for (size_t index = 0; index < vectors->size(); ++index) {
        if (!snapshot_vector_is_finite(&(*vectors)[index]))
            return 0;
    }

    return 1;
}


static int
snapshot_read_valid_triangles(FILE *snapshot_file,
                              const struct snapshot_disk_range *range,
                              uint64_t count,
                              uint64_t vertex_count,
                              uint64_t snapshot_size,
                              uint64_t data_offset,
                              std::vector<struct creo_brl_snapshot_triangle> *triangles)
{
    if (!triangles ||
        !snapshot_range_has_records(range, count, sizeof(struct creo_brl_snapshot_triangle),
                                    snapshot_size, data_offset) ||
        count > triangles->max_size())
        return 0;

    triangles->resize((size_t)count);
    if (count == 0)
        return 1;
    if (!snapshot_read_at(snapshot_file, range->offset, triangles->data(),
                          (size_t)count * sizeof(struct creo_brl_snapshot_triangle)))
        return 0;

    for (size_t index = 0; index < triangles->size(); ++index) {
        for (unsigned int corner = 0;
             corner < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++corner) {
            const uint32_t vertex_index = (*triangles)[index].vertices[corner];

            if ((uint64_t)vertex_index >= vertex_count || vertex_index > INT_MAX)
                return 0;
        }
    }

    return 1;
}


static int
snapshot_read_valid_indices(FILE *snapshot_file,
                            const struct snapshot_disk_range *range,
                            uint64_t count,
                            uint64_t maximum,
                            uint64_t snapshot_size,
                            uint64_t data_offset,
                            std::vector<uint32_t> *indices)
{
    if (!indices ||
        !snapshot_range_has_records(range, count, sizeof(uint32_t),
                                    snapshot_size, data_offset) ||
        count > indices->max_size())
        return 0;

    indices->resize((size_t)count);
    if (count == 0)
        return 1;
    if (!snapshot_read_at(snapshot_file, range->offset, indices->data(),
                          (size_t)count * sizeof(uint32_t)))
        return 0;

    for (size_t index = 0; index < indices->size(); ++index) {
        if ((uint64_t)(*indices)[index] >= maximum || (*indices)[index] > INT_MAX)
            return 0;
    }

    return 1;
}


static int
snapshot_read_valid_named_values(FILE *snapshot_file,
                                 const struct snapshot_disk_range *range,
                                 uint64_t count,
                                 uint64_t snapshot_size,
                                 uint64_t data_offset,
                                 std::vector<struct snapshot_named_value> *values)
{
    if (!values ||
        !snapshot_range_has_records(range, count, sizeof(struct snapshot_disk_named_value),
                                    snapshot_size, data_offset) ||
        count > values->max_size())
        return 0;

    values->resize((size_t)count);
    for (size_t index = 0; index < (size_t)count; ++index) {
        struct snapshot_disk_named_value record = {};
        const uint64_t offset = range->offset + index * sizeof(record);

        if (!snapshot_read_at(snapshot_file, offset, &record, sizeof(record)) ||
            !snapshot_read_valid_text(snapshot_file, &record.name, snapshot_size,
                                      data_offset, 1, &(*values)[index].name) ||
            !snapshot_read_valid_text(snapshot_file, &record.value, snapshot_size,
                                      data_offset, 0, &(*values)[index].value))
            return 0;
    }

    return 1;
}


static int
snapshot_read_valid_tgc_subtractions(
    FILE *snapshot_file,
    const struct snapshot_disk_range *range,
    uint64_t count,
    uint64_t snapshot_size,
    uint64_t data_offset,
    std::vector<struct creo_brl_snapshot_tgc_subtraction> *subtractions)
{
    if (!subtractions ||
        !snapshot_range_has_records(range, count,
                                    sizeof(struct creo_brl_snapshot_tgc_subtraction),
                                    snapshot_size, data_offset) ||
        count > subtractions->max_size())
        return 0;

    subtractions->resize((size_t)count);
    if (count == 0)
        return 1;
    if (!snapshot_read_at(snapshot_file, range->offset, subtractions->data(),
                          (size_t)count * sizeof(struct creo_brl_snapshot_tgc_subtraction)))
        return 0;

    for (size_t index = 0; index < subtractions->size(); ++index) {
        const struct creo_brl_snapshot_tgc_subtraction& subtraction = (*subtractions)[index];
        const struct creo_brl_snapshot_vector *vectors[] = {
            &subtraction.base,
            &subtraction.height,
            &subtraction.a,
            &subtraction.b,
            &subtraction.c,
            &subtraction.d
        };

        for (size_t vector_index = 0;
             vector_index < sizeof(vectors) / sizeof(vectors[0]); ++vector_index) {
            if (!snapshot_vector_is_finite(vectors[vector_index]))
                return 0;
        }
    }

    return 1;
}


static int
snapshot_read_valid_settings(FILE *snapshot_file,
                             const struct snapshot_disk_settings& disk_settings,
                             uint64_t snapshot_size,
                             uint64_t data_offset,
                             struct creo_brl_snapshot_settings *settings,
                             struct snapshot_settings_strings *strings)
{
    if ((disk_settings.flags & ~CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK) != 0 ||
        disk_settings.xform_mode < CREO_BRL_SNAPSHOT_XFORM_NONE ||
        disk_settings.xform_mode > CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z ||
        disk_settings.chord_mode < CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE ||
        disk_settings.chord_mode > CREO_BRL_SNAPSHOT_CHORD_RELATIVE ||
        disk_settings.log_type < CREO_BRL_SNAPSHOT_LOG_NONE ||
        disk_settings.log_type > CREO_BRL_SNAPSHOT_LOG_ALL ||
        disk_settings.min_luminance < 0 ||
        disk_settings.min_luminance > CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM ||
        !std::isfinite(disk_settings.min_angle) || disk_settings.min_angle <= 0.0 ||
        disk_settings.min_angle > CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM ||
        !std::isfinite(disk_settings.max_chord) || disk_settings.max_chord <= 0.0 ||
        (disk_settings.chord_mode == CREO_BRL_SNAPSHOT_CHORD_RELATIVE &&
         disk_settings.max_chord > CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM) ||
        !std::isfinite(disk_settings.min_hole) || disk_settings.min_hole < 0.0 ||
        !std::isfinite(disk_settings.min_chamfer) || disk_settings.min_chamfer < 0.0 ||
        !std::isfinite(disk_settings.min_round) || disk_settings.min_round < 0.0)
        return 0;

    copy_settings_from_disk(disk_settings, settings);
    strings->preserve_parameters.clear();
    return snapshot_read_valid_text(snapshot_file, &disk_settings.output_path,
                                    snapshot_size, data_offset, 1, &strings->output_path) &&
           snapshot_read_valid_text(snapshot_file, &disk_settings.log_path,
                                    snapshot_size, data_offset, 0, &strings->log_path) &&
           snapshot_read_valid_text(snapshot_file, &disk_settings.material_path,
                                    snapshot_size, data_offset, 0, &strings->material_path) &&
           snapshot_read_valid_text(snapshot_file, &disk_settings.rename_parameters,
                                    snapshot_size, data_offset, 0, &strings->rename_parameters);
}


static int
snapshot_bbox_is_valid(const struct snapshot_disk_part& part)
{
    if (!snapshot_vector_is_finite(&part.bbox_min) ||
        !snapshot_vector_is_finite(&part.bbox_max))
        return 0;

    for (unsigned int coordinate = 0;
         coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
        if (part.bbox_min.coordinates[coordinate] > part.bbox_max.coordinates[coordinate])
            return 0;
    }

    return 1;
}


static int
snapshot_read_valid_part(FILE *snapshot_file,
                         const struct snapshot_disk_part& disk_part,
                         uint64_t snapshot_size,
                         uint64_t data_offset,
                         struct snapshot_part_capture *part_capture)
{
    const int has_mesh = (disk_part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_bbox = (disk_part.flags & CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK) != 0;
    const int has_normals = (disk_part.flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    const int has_material = (disk_part.flags & CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL) != 0;

    if ((disk_part.flags & ~CREO_BRL_SNAPSHOT_PART_FLAG_MASK) != 0 ||
        disk_part.region_id < 1 ||
        has_mesh == has_bbox ||
        disk_part.color_is_set !=
            ((disk_part.flags & CREO_BRL_SNAPSHOT_PART_HAS_COLOR) != 0) ||
        !std::isfinite(disk_part.bbox_diagonal) || disk_part.bbox_diagonal < 0.0 ||
        !std::isfinite(disk_part.bbox_volume) || disk_part.bbox_volume < 0.0 ||
        !std::isfinite(disk_part.bbox_area) || disk_part.bbox_area < 0.0 ||
        !std::isfinite(disk_part.density) ||
        !std::isfinite(disk_part.mass) || !std::isfinite(disk_part.volume) ||
        !std::isfinite(disk_part.surface_area) ||
        !snapshot_bbox_is_valid(disk_part) ||
        (!has_material && !snapshot_range_is_empty(&disk_part.material_name)) ||
        !snapshot_read_valid_text(snapshot_file, &disk_part.model_name,
                                  snapshot_size, data_offset, 1,
                                  &part_capture->strings.model_name) ||
        !snapshot_read_valid_text(snapshot_file, &disk_part.model_version,
                                  snapshot_size, data_offset, 0,
                                  &part_capture->strings.model_version) ||
        !snapshot_read_valid_text(snapshot_file, &disk_part.material_name,
                                  snapshot_size, data_offset, has_material,
                                  &part_capture->strings.material_name) ||
        !snapshot_read_valid_tgc_subtractions(snapshot_file, &disk_part.tgc_subtractions,
                                              disk_part.tgc_subtraction_count,
                                              snapshot_size, data_offset,
                                              &part_capture->tgc_subtractions) ||
        !snapshot_read_valid_named_values(snapshot_file, &disk_part.attributes,
                                          disk_part.attribute_count,
                                          snapshot_size, data_offset,
                                          &part_capture->attributes) ||
        !snapshot_read_valid_named_values(snapshot_file, &disk_part.parameters,
                                          disk_part.parameter_count,
                                          snapshot_size, data_offset,
                                          &part_capture->parameters))
        return 0;

    copy_part_from_disk(disk_part, &part_capture->part);

    if (has_bbox) {
        part_capture->vertices.clear();
        part_capture->triangles.clear();
        part_capture->normals.clear();
        part_capture->normal_indices.clear();
        return !has_normals && disk_part.vertex_count == 0 && disk_part.triangle_count == 0 &&
               disk_part.normal_count == 0 && disk_part.normal_index_count == 0 &&
               snapshot_range_is_empty(&disk_part.vertices) &&
               snapshot_range_is_empty(&disk_part.triangles) &&
               snapshot_range_is_empty(&disk_part.normals) &&
               snapshot_range_is_empty(&disk_part.normal_indices);
    }

    if (disk_part.vertex_count < CREO_BRL_SNAPSHOT_MIN_VERTEX_COUNT ||
        disk_part.triangle_count < CREO_BRL_SNAPSHOT_MIN_TRIANGLE_COUNT ||
        !snapshot_read_valid_vectors(snapshot_file, &disk_part.vertices,
                                     disk_part.vertex_count,
                                     snapshot_size, data_offset,
                                     &part_capture->vertices) ||
        !snapshot_read_valid_triangles(snapshot_file, &disk_part.triangles,
                                       disk_part.triangle_count,
                                       disk_part.vertex_count,
                                       snapshot_size, data_offset,
                                       &part_capture->triangles))
        return 0;

    if (!has_normals) {
        part_capture->normals.clear();
        part_capture->normal_indices.clear();
        return disk_part.normal_count == 0 && disk_part.normal_index_count == 0 &&
               snapshot_range_is_empty(&disk_part.normals) &&
               snapshot_range_is_empty(&disk_part.normal_indices);
    }

    if (disk_part.normal_count == 0 ||
        disk_part.triangle_count > UINT64_MAX / CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT ||
        disk_part.normal_index_count !=
            disk_part.triangle_count * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT)
        return 0;

    return snapshot_read_valid_vectors(snapshot_file, &disk_part.normals,
                                       disk_part.normal_count,
                                       snapshot_size, data_offset,
                                       &part_capture->normals) &&
           snapshot_read_valid_indices(snapshot_file, &disk_part.normal_indices,
                                       disk_part.normal_index_count,
                                       disk_part.normal_count,
                                       snapshot_size, data_offset,
                                       &part_capture->normal_indices);
}


static int
snapshot_read_valid_assembly(FILE *snapshot_file,
                             const struct snapshot_disk_assembly& disk_assembly,
                             uint64_t part_count,
                             uint64_t assembly_count,
                             uint64_t snapshot_size,
                             uint64_t data_offset,
                             struct snapshot_assembly_capture *assembly_capture)
{
    if ((disk_assembly.flags & ~CREO_BRL_SNAPSHOT_ASSEMBLY_FLAG_MASK) != 0 ||
        !std::isfinite(disk_assembly.density) || !std::isfinite(disk_assembly.mass) ||
        !std::isfinite(disk_assembly.volume) || !std::isfinite(disk_assembly.surface_area) ||
        disk_assembly.member_count == 0 ||
        !snapshot_range_has_records(&disk_assembly.members, disk_assembly.member_count,
                                    sizeof(struct creo_brl_snapshot_assembly_member),
                                    snapshot_size, data_offset) ||
        disk_assembly.member_count > assembly_capture->members.max_size() ||
        !snapshot_read_valid_text(snapshot_file, &disk_assembly.model_name,
                                  snapshot_size, data_offset, 1,
                                  &assembly_capture->model_name) ||
        !snapshot_read_valid_text(snapshot_file, &disk_assembly.model_version,
                                  snapshot_size, data_offset, 0,
                                  &assembly_capture->model_version) ||
        !snapshot_read_valid_named_values(snapshot_file, &disk_assembly.parameters,
                                          disk_assembly.parameter_count,
                                          snapshot_size, data_offset,
                                          &assembly_capture->parameters))
        return 0;

    copy_assembly_from_disk(disk_assembly, &assembly_capture->assembly);
    assembly_capture->members.resize((size_t)disk_assembly.member_count);
    if (!snapshot_read_at(snapshot_file, disk_assembly.members.offset,
                          assembly_capture->members.data(),
                          (size_t)disk_assembly.member_count *
                              sizeof(struct creo_brl_snapshot_assembly_member)))
        return 0;

    for (size_t index = 0; index < assembly_capture->members.size(); ++index) {
        const struct creo_brl_snapshot_assembly_member& member =
            assembly_capture->members[index];

        if (!snapshot_matrix_is_finite(&member.matrix))
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


int
creo_brl_snapshot_read(const char *snapshot_path,
                       struct snapshot_scene_capture *scene_out)
{
    FILE *snapshot_file = NULL;
    int result = CREO_BRL_SNAPSHOT_IO_INVALID_SNAPSHOT;

    if (!snapshot_path || !snapshot_path[0] || !scene_out)
        return CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST;

    snapshot_file = snapshot_open_file(snapshot_path, L"rb", "rb");
    if (!snapshot_file)
        return CREO_BRL_SNAPSHOT_IO_OPEN_FAILED;

    try {
        struct snapshot_disk_header header = {};
        struct snapshot_disk_scene disk_scene = {};
        struct snapshot_scene_capture decoded = {};
        uint64_t file_size = 0;
        uint64_t part_records_offset = 0;
        uint64_t assembly_records_offset = 0;
        uint64_t data_offset = 0;

        if (!snapshot_file_size(snapshot_file, &file_size) ||
            !snapshot_read_at(snapshot_file, 0, &header, sizeof(header)) ||
            std::memcmp(header.magic, creo_brl_snapshot_magic, sizeof(header.magic)) != 0 ||
            header.snapshot_size != file_size ||
            header.snapshot_size < sizeof(header) + sizeof(disk_scene) ||
            !snapshot_read_at(snapshot_file, sizeof(header), &disk_scene, sizeof(disk_scene)))
            goto cleanup;

        part_records_offset = sizeof(header) + sizeof(disk_scene);
        if (disk_scene.part_count > SIZE_MAX / sizeof(struct snapshot_disk_part) ||
            disk_scene.part_count > UINT64_MAX / sizeof(struct snapshot_disk_part) ||
            disk_scene.part_count > (UINT64_MAX - part_records_offset) /
                                        sizeof(struct snapshot_disk_part) ||
            disk_scene.part_count > decoded.parts.max_size() ||
            (disk_scene.part_count == 0 ?
                 !snapshot_range_is_empty(&disk_scene.parts) :
                 (disk_scene.parts.offset != part_records_offset ||
                  disk_scene.parts.size !=
                      disk_scene.part_count * sizeof(struct snapshot_disk_part))))
            goto cleanup;

        assembly_records_offset = part_records_offset +
                                  disk_scene.part_count * sizeof(struct snapshot_disk_part);
        if (assembly_records_offset > file_size ||
            disk_scene.assembly_count > SIZE_MAX / sizeof(struct snapshot_disk_assembly) ||
            disk_scene.assembly_count > UINT64_MAX / sizeof(struct snapshot_disk_assembly) ||
            disk_scene.assembly_count > (UINT64_MAX - assembly_records_offset) /
                                            sizeof(struct snapshot_disk_assembly) ||
            disk_scene.assembly_count > decoded.assemblies.max_size() ||
            (disk_scene.assembly_count == 0 ?
                 !snapshot_range_is_empty(&disk_scene.assemblies) :
                 (disk_scene.assemblies.offset != assembly_records_offset ||
                  disk_scene.assemblies.size !=
                      disk_scene.assembly_count * sizeof(struct snapshot_disk_assembly))))
            goto cleanup;

        data_offset = assembly_records_offset +
                      disk_scene.assembly_count * sizeof(struct snapshot_disk_assembly);
        if (data_offset > file_size ||
            !snapshot_read_valid_settings(snapshot_file, disk_scene.settings,
                                          file_size, data_offset,
                                          &decoded.settings, &decoded.settings_strings))
            goto cleanup;

        if ((disk_scene.root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART &&
             (uint64_t)disk_scene.root_index >= disk_scene.part_count) ||
            (disk_scene.root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY &&
             (uint64_t)disk_scene.root_index >= disk_scene.assembly_count) ||
            (disk_scene.root_type != CREO_BRL_SNAPSHOT_SCENE_NODE_PART &&
             disk_scene.root_type != CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY))
            goto cleanup;

        decoded.root_type = disk_scene.root_type;
        decoded.root_index = disk_scene.root_index;
        decoded.parts.resize((size_t)disk_scene.part_count);
        for (size_t index = 0; index < decoded.parts.size(); ++index) {
            struct snapshot_disk_part disk_part = {};
            const uint64_t offset = disk_scene.parts.offset +
                                    index * sizeof(struct snapshot_disk_part);

            if (!snapshot_read_at(snapshot_file, offset, &disk_part, sizeof(disk_part)) ||
                !snapshot_read_valid_part(snapshot_file, disk_part,
                                          file_size, data_offset,
                                          &decoded.parts[index]))
                goto cleanup;
        }

        decoded.assemblies.resize((size_t)disk_scene.assembly_count);
        for (size_t index = 0; index < decoded.assemblies.size(); ++index) {
            struct snapshot_disk_assembly disk_assembly = {};
            const uint64_t offset = disk_scene.assemblies.offset +
                                    index * sizeof(struct snapshot_disk_assembly);

            if (!snapshot_read_at(snapshot_file, offset, &disk_assembly, sizeof(disk_assembly)) ||
                !snapshot_read_valid_assembly(snapshot_file, disk_assembly,
                                              disk_scene.part_count,
                                              disk_scene.assembly_count,
                                              file_size, data_offset,
                                              &decoded.assemblies[index]))
                goto cleanup;
        }

        *scene_out = decoded;
        result = CREO_BRL_SNAPSHOT_IO_SUCCESS;
    } catch (...) {
        result = CREO_BRL_SNAPSHOT_IO_INVALID_SNAPSHOT;
    }

cleanup:
    fclose(snapshot_file);
    return result;
}
