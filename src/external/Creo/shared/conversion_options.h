/**
 *          C O N V E R S I O N _ O P T I O N S . H
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
 * @file shared/conversion_options.h
 *
 * Shared conversion option tokens and structures between the facade and core.
 */

#ifndef CREO_SHARED_CONVERSION_OPTIONS_H
#define CREO_SHARED_CONVERSION_OPTIONS_H

#include <stdint.h>
#include <bu/str.h>

#ifdef __cplusplus
extern "C" {
#endif

enum creo_brl_conversion_log_type {
    CREO_BRL_CONVERSION_LOG_NONE = -1,
    CREO_BRL_CONVERSION_LOG_FAILURE = 0,
    CREO_BRL_CONVERSION_LOG_SUCCESS = 1,
    CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS = 2,
    CREO_BRL_CONVERSION_LOG_ALL = 3
};

enum creo_brl_conversion_xform_mode {
    CREO_BRL_CONVERSION_XFORM_NONE = 0,
    CREO_BRL_CONVERSION_XFORM_X_TO_Z = 1,
    CREO_BRL_CONVERSION_XFORM_Y_TO_Z = 2
};

enum creo_brl_conversion_chord_mode {
    CREO_BRL_CONVERSION_CHORD_ABSOLUTE = 0,
    CREO_BRL_CONVERSION_CHORD_RELATIVE = 1
};

enum creo_brl_conversion_option_flag {
    CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES = 1u << 0,
    CREO_BRL_CONVERSION_OPTION_FACETS_ONLY = 1u << 1,
    CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY = 1u << 2,
    CREO_BRL_CONVERSION_OPTION_CREATE_BOXES = 1u << 3,
    CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS = 1u << 4
};

#define CREO_BRL_CONVERSION_OPTION_FLAG_MASK \
    (CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES | \
     CREO_BRL_CONVERSION_OPTION_FACETS_ONLY | \
     CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY | \
     CREO_BRL_CONVERSION_OPTION_CREATE_BOXES | \
     CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS)

#define CREO_BRL_OPTION_TOKEN_LOG_FAILURE "failure"
#define CREO_BRL_OPTION_TOKEN_LOG_SUCCESS "success"
#define CREO_BRL_OPTION_TOKEN_LOG_FAILURE_OR_SUCCESS "failure/success"
#define CREO_BRL_OPTION_TOKEN_LOG_ALL "all/(debug)"
#define CREO_BRL_OPTION_TOKEN_XFORM_NONE "none"
#define CREO_BRL_OPTION_TOKEN_XFORM_X_TO_Z "x_to_z"
#define CREO_BRL_OPTION_TOKEN_XFORM_Y_TO_Z "y_to_z"
#define CREO_BRL_OPTION_TOKEN_CHORD_ABSOLUTE "millimeter"
#define CREO_BRL_OPTION_TOKEN_CHORD_RELATIVE "percent"

static inline const char *
creo_brl_log_type_token(int32_t log_type)
{
    switch (log_type) {
        case CREO_BRL_CONVERSION_LOG_FAILURE:
            return CREO_BRL_OPTION_TOKEN_LOG_FAILURE;
        case CREO_BRL_CONVERSION_LOG_SUCCESS:
            return CREO_BRL_OPTION_TOKEN_LOG_SUCCESS;
        case CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS:
            return CREO_BRL_OPTION_TOKEN_LOG_FAILURE_OR_SUCCESS;
        case CREO_BRL_CONVERSION_LOG_ALL:
        default:
            return CREO_BRL_OPTION_TOKEN_LOG_ALL;
    }
}

static inline int
creo_brl_parse_log_type_token(const char *token, int32_t *log_type)
{
    if (!token || !log_type)
        return 0;

    if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_LOG_FAILURE) == 0) 
        *log_type = CREO_BRL_CONVERSION_LOG_FAILURE;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_LOG_SUCCESS) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_SUCCESS;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_LOG_FAILURE_OR_SUCCESS) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_LOG_ALL) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_ALL;
    else
        return 0;

    return 1;
}

static inline const char *
creo_brl_xform_mode_token(int32_t xform_mode)
{
    switch (xform_mode) {
        case CREO_BRL_CONVERSION_XFORM_X_TO_Z:
            return CREO_BRL_OPTION_TOKEN_XFORM_X_TO_Z;
        case CREO_BRL_CONVERSION_XFORM_Y_TO_Z:
            return CREO_BRL_OPTION_TOKEN_XFORM_Y_TO_Z;
        default:
            return CREO_BRL_OPTION_TOKEN_XFORM_NONE;
    }
}

static inline int
creo_brl_parse_xform_mode_token(const char *token, int32_t *xform_mode)
{
    if (!token || !xform_mode)
        return 0;

    if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_XFORM_X_TO_Z) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_X_TO_Z;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_XFORM_Y_TO_Z) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_Y_TO_Z;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_XFORM_NONE) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_NONE;
    else
        return 0;

    return 1;
}

static inline const char *
creo_brl_chord_mode_token(int32_t chord_mode)
{
    return chord_mode == CREO_BRL_CONVERSION_CHORD_ABSOLUTE ?
               CREO_BRL_OPTION_TOKEN_CHORD_ABSOLUTE :
               CREO_BRL_OPTION_TOKEN_CHORD_RELATIVE;
}

static inline int
creo_brl_parse_chord_mode_token(const char *token, int32_t *chord_mode)
{
    if (!token || !chord_mode)
        return 0;

    if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_CHORD_ABSOLUTE) == 0)
        *chord_mode = CREO_BRL_CONVERSION_CHORD_ABSOLUTE;
    else if (bu_strcmp(token, CREO_BRL_OPTION_TOKEN_CHORD_RELATIVE) == 0)
        *chord_mode = CREO_BRL_CONVERSION_CHORD_RELATIVE;
    else
        return 0;

    return 1;
}

/*
 * Profile numeric fields remain text so invalid user input reaches the
 * existing dialog validation path instead of acquiring a new fallback policy.
 */
struct creo_brl_conversion_options {
    int32_t log_type;
    const char *material_path;
    const char *rename_parameters;
    const char *preserve_parameters;
    int32_t xform_mode;
    const char *region_counter;
    const char *minimum_luminance;
    int32_t chord_mode;
    const char *maximum_chord;
    const char *minimum_angle;
    uint32_t flags;
    const char *minimum_hole;
    const char *minimum_chamfer;
    const char *minimum_round;
};

#ifdef __cplusplus
}
#endif

#endif /* CREO_SHARED_CONVERSION_OPTIONS_H */
