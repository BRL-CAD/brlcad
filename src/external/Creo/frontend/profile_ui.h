/**
 *                P R O F I L E _ U I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_FRONTEND_PROFILE_UI_H
#define CREO_FRONTEND_PROFILE_UI_H

#include <string>

#include "shared/conversion_snapshot.h"
#include "shared/conversion_options.h"

struct creo_brl_frontend_control_api {
    void *context;
    int (*set_text)(void *context, const char *control, const char *value);
    int (*set_check)(void *context, const char *control, int checked);
    int (*set_radio)(void *context, const char *control, const char *selection);
    int (*get_text)(void *context, const char *control, std::string *value);
    int (*get_check)(void *context, const char *control, int *checked);
    int (*get_radio)(void *context, const char *control, std::string *selection);
};

int creo_brl_frontend_apply_options_to_controls(
    const struct creo_brl_conversion_options *options,
    const struct creo_brl_frontend_control_api *controls);
int creo_brl_frontend_capture_snapshot_options(
    const struct creo_brl_frontend_control_api *controls,
    struct creo_brl_snapshot_settings *settings,
    struct snapshot_settings_strings *strings);
int creo_brl_frontend_default_output_paths(
    const char *model_name,
    std::string *output_path,
    std::string *log_path);

#endif /* CREO_FRONTEND_PROFILE_UI_H */
