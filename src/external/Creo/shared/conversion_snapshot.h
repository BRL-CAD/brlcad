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
 * Toolkit-free scene representation and snapshot codec shared by the Creo
 * facade, runtime conversion core, and tests.
 */

#ifndef CREO_CONVERSION_SNAPSHOT_H
#define CREO_CONVERSION_SNAPSHOT_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "shared/conversion_options.h"

#ifdef __cplusplus
#  include <string>
#  include <vector>
#endif

#define CREO_BRL_SNAPSHOT_MAGIC_SIZE 8u
#define CREO_BRL_SNAPSHOT_COORDINATE_COUNT 3u
#define CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_COLOR_COMPONENT_COUNT 3u
#define CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT 16u
#define CREO_BRL_SNAPSHOT_MIN_VERTEX_COUNT 3u
#define CREO_BRL_SNAPSHOT_MIN_TRIANGLE_COUNT 1u
#define CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM 100.0
#define CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM 1.0
#define CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM 100

enum creo_brl_snapshot_io_result {
    CREO_BRL_SNAPSHOT_IO_SUCCESS = 0,
    CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST = -1,
    CREO_BRL_SNAPSHOT_IO_OPEN_FAILED = -2,
    CREO_BRL_SNAPSHOT_IO_INVALID_SNAPSHOT = -3,
    CREO_BRL_SNAPSHOT_IO_WRITE_FAILED = -4
};

enum creo_brl_snapshot_scene_node_type {
    CREO_BRL_SNAPSHOT_SCENE_NODE_PART = 1u,
    CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY = 2u
};

enum creo_brl_snapshot_log_type {
    CREO_BRL_SNAPSHOT_LOG_NONE = CREO_BRL_CONVERSION_LOG_NONE,
    CREO_BRL_SNAPSHOT_LOG_FAILURE = CREO_BRL_CONVERSION_LOG_FAILURE,
    CREO_BRL_SNAPSHOT_LOG_SUCCESS = CREO_BRL_CONVERSION_LOG_SUCCESS,
    CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS = CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS,
    CREO_BRL_SNAPSHOT_LOG_ALL = CREO_BRL_CONVERSION_LOG_ALL
};

enum creo_brl_snapshot_xform_mode {
    CREO_BRL_SNAPSHOT_XFORM_NONE = CREO_BRL_CONVERSION_XFORM_NONE,
    CREO_BRL_SNAPSHOT_XFORM_X_TO_Z = CREO_BRL_CONVERSION_XFORM_X_TO_Z,
    CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z = CREO_BRL_CONVERSION_XFORM_Y_TO_Z
};

enum creo_brl_snapshot_chord_mode {
    CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE = CREO_BRL_CONVERSION_CHORD_ABSOLUTE,
    CREO_BRL_SNAPSHOT_CHORD_RELATIVE = CREO_BRL_CONVERSION_CHORD_RELATIVE
};

enum creo_brl_snapshot_settings_flag {
    CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES =
        CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES,
    CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY =
        CREO_BRL_CONVERSION_OPTION_FACETS_ONLY,
    CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY =
        CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY,
    CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES =
        CREO_BRL_CONVERSION_OPTION_CREATE_BOXES,
    CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS =
        CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS
};

#define CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK CREO_BRL_CONVERSION_OPTION_FLAG_MASK

enum creo_brl_snapshot_part_flag {
    CREO_BRL_SNAPSHOT_PART_HAS_MESH = 1u << 0,
    CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK = 1u << 1,
    CREO_BRL_SNAPSHOT_PART_HAS_NORMALS = 1u << 2,
    CREO_BRL_SNAPSHOT_PART_HAS_COLOR = 1u << 3,
    CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL = 1u << 4,
    CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES = 1u << 5,
    CREO_BRL_SNAPSHOT_PART_INCOMPLETE_TESS = 1u << 6,
    CREO_BRL_SNAPSHOT_PART_OMITTED_QUILTS = 1u << 7,
    CREO_BRL_SNAPSHOT_PART_NON_SOLID = 1u << 8
};

#define CREO_BRL_SNAPSHOT_PART_FLAG_MASK \
    (CREO_BRL_SNAPSHOT_PART_HAS_MESH | \
     CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK | \
     CREO_BRL_SNAPSHOT_PART_HAS_NORMALS | \
     CREO_BRL_SNAPSHOT_PART_HAS_COLOR | \
     CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL | \
     CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES | \
     CREO_BRL_SNAPSHOT_PART_INCOMPLETE_TESS | \
     CREO_BRL_SNAPSHOT_PART_OMITTED_QUILTS | \
     CREO_BRL_SNAPSHOT_PART_NON_SOLID)

enum creo_brl_snapshot_assembly_flag {
    CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES = 1u << 0
};

#define CREO_BRL_SNAPSHOT_ASSEMBLY_FLAG_MASK \
    CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES

#pragma pack(push, 1)

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

#pragma pack(pop)

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
};

static inline int
creo_brl_validate_snapshot_settings(const struct creo_brl_snapshot_settings *settings)
{
    return settings &&
           (settings->flags & ~CREO_BRL_SNAPSHOT_SETTINGS_FLAG_MASK) == 0 &&
           settings->xform_mode >= CREO_BRL_SNAPSHOT_XFORM_NONE &&
           settings->xform_mode <= CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z &&
           settings->chord_mode >= CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE &&
           settings->chord_mode <= CREO_BRL_SNAPSHOT_CHORD_RELATIVE &&
           settings->log_type >= CREO_BRL_SNAPSHOT_LOG_NONE &&
           settings->log_type <= CREO_BRL_SNAPSHOT_LOG_ALL &&
           settings->min_luminance >= 0 &&
           settings->min_luminance <= CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM &&
           isfinite(settings->min_angle) && settings->min_angle > 0.0 &&
           settings->min_angle <= CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM &&
           isfinite(settings->max_chord) && settings->max_chord > 0.0 &&
           (settings->chord_mode != CREO_BRL_SNAPSHOT_CHORD_RELATIVE ||
            settings->max_chord <= CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM) &&
           isfinite(settings->min_hole) && settings->min_hole >= 0.0 &&
           isfinite(settings->min_chamfer) && settings->min_chamfer >= 0.0 &&
           isfinite(settings->min_round) && settings->min_round >= 0.0;
}

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
};

struct creo_brl_snapshot_assembly {
    uint32_t flags;
    double density;
    double mass;
    double volume;
    double surface_area;
};

#ifdef __cplusplus

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
    std::string diagnostic_note;
};

struct snapshot_named_value {
    std::string name;
    std::string value;
};

struct snapshot_part_capture {
    struct creo_brl_snapshot_part part;
    struct snapshot_part_strings strings;
    std::vector<struct snapshot_named_value> attributes;
    std::vector<struct snapshot_named_value> parameters;
    std::vector<struct creo_brl_snapshot_tgc_subtraction> tgc_subtractions;
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
    std::vector<struct snapshot_named_value> parameters;
};

struct snapshot_scene_capture {
    struct creo_brl_snapshot_settings settings;
    struct snapshot_settings_strings settings_strings;
    std::vector<struct snapshot_part_capture> parts;
    std::vector<struct snapshot_assembly_capture> assemblies;
    std::vector<struct snapshot_named_value> skipped_parts;
    std::vector<struct snapshot_named_value> skipped_assemblies;
    uint32_t root_type;
    uint32_t root_index;
    int64_t next_region_id;
};

FILE *creo_brl_open_utf8_file(const char *path,
                              const wchar_t *wide_mode,
                              const char *mode);

void creo_brl_snapshot_log_skipped_items(
    FILE *log_file,
    const struct snapshot_scene_capture& scene);

int creo_brl_snapshot_encode(const struct snapshot_scene_capture& scene,
                             std::vector<unsigned char> *buffer);

int creo_brl_snapshot_write(const char *snapshot_path,
                            const struct snapshot_scene_capture& scene);

int creo_brl_snapshot_read(const char *snapshot_path,
                           struct snapshot_scene_capture *scene);

#endif /* __cplusplus */

#endif /* CREO_CONVERSION_SNAPSHOT_H */
