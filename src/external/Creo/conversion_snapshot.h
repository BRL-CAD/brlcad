/**
 *          C O N V E R S I O N _ S N A P S H O T . H
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
 * @file conversion_snapshot.h
 *
 * Toolkit-free on-disk request contract between the Creo facade and core.
 *
 * Version 1 stores little-endian IEEE 754 values.  Every variable-size range
 * is an absolute byte range in the snapshot and is length-bounded; no record
 * relies on process pointers or null-terminated text.
 */

#ifndef CREO_CONVERSION_SNAPSHOT_H
#define CREO_CONVERSION_SNAPSHOT_H

#include <stdint.h>

#define CREO_BRL_SNAPSHOT_MAGIC_SIZE 8u
#define CREO_BRL_SNAPSHOT_FORMAT_VERSION 1u

#define CREO_BRL_SNAPSHOT_HEADER_V1_SIZE 40u
#define CREO_BRL_SNAPSHOT_RANGE_SIZE 16u
#define CREO_BRL_SNAPSHOT_VECTOR_SIZE 24u
#define CREO_BRL_SNAPSHOT_TRIANGLE_SIZE 12u
#define CREO_BRL_SNAPSHOT_NAMED_VALUE_SIZE 32u
#define CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE 200u
#define CREO_BRL_SNAPSHOT_PART_V1_SIZE 316u
#define CREO_BRL_SNAPSHOT_SINGLE_PART_V1_SIZE 524u

#define CREO_BRL_SNAPSHOT_COORDINATE_COUNT 3u
#define CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT 3u
#define CREO_BRL_SNAPSHOT_MIN_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_MIN_TRIANGLE_COUNT 1u

static const unsigned char creo_brl_snapshot_magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE] = {
    'C', 'R', 'E', 'O', 'B', 'R', 'L', '\0'
};

enum creo_brl_snapshot_payload_type {
    CREO_BRL_SNAPSHOT_PAYLOAD_SINGLE_PART = 1u
};

enum creo_brl_snapshot_log_type {
    CREO_BRL_SNAPSHOT_LOG_NONE = -1,
    CREO_BRL_SNAPSHOT_LOG_FAILURE = 0,
    CREO_BRL_SNAPSHOT_LOG_SUCCESS = 1,
    CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS = 2,
    CREO_BRL_SNAPSHOT_LOG_ALL = 3
};

enum creo_brl_snapshot_xform_mode {
    CREO_BRL_SNAPSHOT_XFORM_NONE = 0,
    CREO_BRL_SNAPSHOT_XFORM_X_TO_Z = 1,
    CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z = 2
};

enum creo_brl_snapshot_chord_mode {
    CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE = 0,
    CREO_BRL_SNAPSHOT_CHORD_RELATIVE = 1
};

enum creo_brl_snapshot_settings_flag {
    CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES = 1u << 0,
    CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY = 1u << 1,
    CREO_BRL_SNAPSHOT_SETTING_EXPORT_STL = 1u << 2,
    CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY = 1u << 3,
    CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES = 1u << 4,
    CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS = 1u << 5
};

#define CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK \
    (CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES | \
     CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY | \
     CREO_BRL_SNAPSHOT_SETTING_EXPORT_STL | \
     CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY | \
     CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES | \
     CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS)

enum creo_brl_snapshot_part_flag {
    CREO_BRL_SNAPSHOT_PART_HAS_MESH = 1u << 0,
    CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK = 1u << 1,
    CREO_BRL_SNAPSHOT_PART_HAS_NORMALS = 1u << 2,
    CREO_BRL_SNAPSHOT_PART_HAS_COLOR = 1u << 3,
    CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL = 1u << 4,
    CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES = 1u << 5
};

#define CREO_BRL_SNAPSHOT_PART_FLAG_MASK \
    (CREO_BRL_SNAPSHOT_PART_HAS_MESH | \
     CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK | \
     CREO_BRL_SNAPSHOT_PART_HAS_NORMALS | \
     CREO_BRL_SNAPSHOT_PART_HAS_COLOR | \
     CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL | \
     CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES)

#pragma pack(push, 1)

struct creo_brl_snapshot_range {
    uint64_t offset;
    uint64_t size;
};

struct creo_brl_snapshot_vector {
    double coordinates[CREO_BRL_SNAPSHOT_COORDINATE_COUNT];
};

struct creo_brl_snapshot_triangle {
    uint32_t vertices[CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT];
};

struct creo_brl_snapshot_named_value {
    struct creo_brl_snapshot_range name;
    struct creo_brl_snapshot_range value;
};

struct creo_brl_snapshot_header {
    unsigned char magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE];
    uint32_t format_version;
    uint32_t header_size;
    uint64_t snapshot_size;
    uint32_t payload_type;
    uint32_t reserved;
    uint64_t payload_size;
};

struct creo_brl_snapshot_settings {
    uint32_t structure_size;
    uint32_t flags;
    int32_t xform_mode;
    int32_t chord_mode;
    int32_t region_id;
    int32_t min_luminance;
    int32_t max_facets;
    int32_t max_steps;
    int32_t log_type;
    uint32_t reserved;
    double max_angle;
    double min_angle;
    double max_chord;
    double min_chord;
    double min_edge;
    double min_hole;
    double min_chamfer;
    double min_round;
    struct creo_brl_snapshot_range output_path;
    struct creo_brl_snapshot_range log_path;
    struct creo_brl_snapshot_range material_path;
    struct creo_brl_snapshot_range stl_path;
    struct creo_brl_snapshot_range rename_parameters;
    struct creo_brl_snapshot_range preserve_parameters;
};

struct creo_brl_snapshot_part {
    uint32_t structure_size;
    uint32_t flags;
    int32_t region_id;
    uint32_t reserved;
    double model_to_mm;
    double bbox_diagonal;
    double bbox_volume;
    double bbox_area;
    unsigned char color[CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT];
    unsigned char color_is_set;
    double density;
    double mass;
    double volume;
    struct creo_brl_snapshot_vector bbox_min;
    struct creo_brl_snapshot_vector bbox_max;
    struct creo_brl_snapshot_range model_name;
    struct creo_brl_snapshot_range model_version;
    struct creo_brl_snapshot_range material_name;
    struct creo_brl_snapshot_range vertices;
    uint64_t vertex_count;
    struct creo_brl_snapshot_range triangles;
    uint64_t triangle_count;
    struct creo_brl_snapshot_range normals;
    uint64_t normal_count;
    struct creo_brl_snapshot_range normal_indices;
    uint64_t normal_index_count;
    struct creo_brl_snapshot_range attributes;
    uint64_t attribute_count;
    struct creo_brl_snapshot_range parameters;
    uint64_t parameter_count;
};

struct creo_brl_snapshot_single_part {
    uint32_t structure_size;
    uint32_t reserved;
    struct creo_brl_snapshot_settings settings;
    struct creo_brl_snapshot_part part;
};

#pragma pack(pop)

enum creo_brl_core_convert_result {
    CREO_BRL_CORE_CONVERT_SUCCESS = 0,
    CREO_BRL_CORE_CONVERT_INVALID_REQUEST = -1,
    CREO_BRL_CORE_CONVERT_OPEN_FAILED = -2,
    CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT = -3,
    CREO_BRL_CORE_CONVERT_UNSUPPORTED_SNAPSHOT = -4,
    CREO_BRL_CORE_CONVERT_NOT_IMPLEMENTED = -5
};

#endif /* CREO_CONVERSION_SNAPSHOT_H */
