/**
 *              P R O F I L E _ P A T H . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_FRONTEND_PROFILE_PATH_H
#define CREO_FRONTEND_PROFILE_PATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*creo_brl_profile_path_exists_fn_t)(const char *path, void *context);

int creo_brl_resolve_profile_path(
    const char *environment_profile,
    const char *working_directory,
    const char *bundled_profile_path,
    creo_brl_profile_path_exists_fn_t path_exists,
    void *context,
    char *profile_path,
    size_t profile_path_size);

#ifdef __cplusplus
}
#endif

#endif /* CREO_FRONTEND_PROFILE_PATH_H */
