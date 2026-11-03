/**
 *                     C O R E . C P P
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
 * @file core.cpp
 *
 * Toolkit-free C ABI exports for the runtime-loaded conversion core.
 */

#include "common.h"

#include "conversion/profile_reader.h"
#include "conversion/scene_convert.h"
#include "shared/conversion_snapshot.h"
#include "shared/core_api.h"

extern "C" CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL
creo_brl_core_initialize(void)
{
    return 0;
}

extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_terminate(void)
{
}

extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_set_frontend_api(const struct creo_brl_frontend_api *api)
{
    creo_brl_set_profile_frontend_api(api);
}

extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_load_profile(const char *profile_path, const char *material_directory)
{
    creo_brl_load_profile_options(profile_path, material_directory);
}

extern "C" CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL
creo_brl_core_convert(const char *snapshot_path)
{
    struct snapshot_scene_capture scene = {};
    const int io_result = creo_brl_snapshot_read(snapshot_path, &scene);

    switch (io_result) {
        case CREO_BRL_SNAPSHOT_IO_SUCCESS:
            return creo_brl_convert_snapshot_scene(scene);
        case CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST:
            return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;
        case CREO_BRL_SNAPSHOT_IO_OPEN_FAILED:
            return CREO_BRL_CORE_CONVERT_OPEN_FAILED;
        default:
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
    }
}
