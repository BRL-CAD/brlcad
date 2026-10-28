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
 * @file shared/conversion_snapshot.h
 *
 * Toolkit-free on-disk request contract between the Creo facade and core.
 *
 * Snapshots store little-endian IEEE 754 values.  Every variable-size
 * range is an absolute byte range in the snapshot and is length-bounded; no
 * record relies on process pointers or null-terminated text.
 */

#ifndef CREO_CONVERSION_SNAPSHOT_H
#define CREO_CONVERSION_SNAPSHOT_H

#include <stdint.h>

#define CREO_BRL_SNAPSHOT_MAGIC_SIZE 8u
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

#define CREO_BRL_SNAPSHOT_COORDINATE_COUNT 3u
#define CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT 3u
#define CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT 16u
#define CREO_BRL_SNAPSHOT_MIN_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_MIN_TRIANGLE_COUNT 1u
#define CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM 100.0
#define CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM 1.0
#define CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM 100

static const unsigned char creo_brl_snapshot_magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE] = {
    'C', 'R', 'E', 'O', 'B', 'R', 'L', '\0'
};

enum creo_brl_snapshot_scene_node_type {
    CREO_BRL_SNAPSHOT_SCENE_NODE_PART = 1u,
    CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY = 2u
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
    CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY = 1u << 2,
    CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES = 1u << 3,
    CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS = 1u << 4
};

#define CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK \
    (CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES | \
     CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY | \
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

enum creo_brl_snapshot_assembly_flag {
    CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES = 1u << 0
};

#define CREO_BRL_SNAPSHOT_ASSEMBLY_FLAG_MASK \
    CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES

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

struct creo_brl_snapshot_tgc_subtraction {
    struct creo_brl_snapshot_vector base;
    struct creo_brl_snapshot_vector height;
    struct creo_brl_snapshot_vector a;
    struct creo_brl_snapshot_vector b;
    struct creo_brl_snapshot_vector c;
    struct creo_brl_snapshot_vector d;
};

struct creo_brl_snapshot_matrix {
    double elements[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT];
};

struct creo_brl_snapshot_named_value {
    struct creo_brl_snapshot_range name;
    struct creo_brl_snapshot_range value;
};

struct creo_brl_snapshot_header {
    unsigned char magic[CREO_BRL_SNAPSHOT_MAGIC_SIZE];
    uint64_t snapshot_size;
};

struct creo_brl_snapshot_settings {
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
    struct creo_brl_snapshot_range output_path;
    struct creo_brl_snapshot_range log_path;
    struct creo_brl_snapshot_range material_path;
    struct creo_brl_snapshot_range rename_parameters;
};

struct creo_brl_snapshot_part {
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
    struct creo_brl_snapshot_range tgc_subtractions;
    uint64_t tgc_subtraction_count;
    struct creo_brl_snapshot_range attributes;
    uint64_t attribute_count;
    struct creo_brl_snapshot_range parameters;
    uint64_t parameter_count;
};

/*
 * Scenes reuse the same part record for each unique part definition. The
 * member matrix is serialized in BRL-CAD mat_t order, with translation
 * already converted to millimeters by the facade.
 */
struct creo_brl_snapshot_assembly_member {
    uint32_t target_type;
    uint32_t target_index;
    struct creo_brl_snapshot_matrix matrix;
};

struct creo_brl_snapshot_assembly {
    uint32_t flags;
    double density;
    double mass;
    double volume;
    double surface_area;
    struct creo_brl_snapshot_range model_name;
    struct creo_brl_snapshot_range model_version;
    struct creo_brl_snapshot_range members;
    uint64_t member_count;
    struct creo_brl_snapshot_range parameters;
    uint64_t parameter_count;
};

struct creo_brl_snapshot_scene {
    struct creo_brl_snapshot_settings settings;
    struct creo_brl_snapshot_range parts;
    uint64_t part_count;
    struct creo_brl_snapshot_range assemblies;
    uint64_t assembly_count;
    uint32_t root_type;
    uint32_t root_index;
};

#pragma pack(pop)

#endif /* CREO_CONVERSION_SNAPSHOT_H */
