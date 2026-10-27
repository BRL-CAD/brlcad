#ifndef CREO_SHARED_CONVERSION_OPTIONS_H
#define CREO_SHARED_CONVERSION_OPTIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum creo_brl_conversion_log_type {
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
