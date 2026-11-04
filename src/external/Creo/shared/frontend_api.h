/**
 *              F R O N T E N D _ A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_BRL_FRONTEND_API_H
#define CREO_BRL_FRONTEND_API_H

#include "shared/conversion_options.h"

#ifdef _WIN32
#  define CREO_BRL_FRONTEND_CALL __cdecl
#else
#  define CREO_BRL_FRONTEND_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (CREO_BRL_FRONTEND_CALL *creo_brl_status_fn_t)(const char *message);
typedef void (CREO_BRL_FRONTEND_CALL *creo_brl_popup_fn_t)(const char *title, const char *message);
typedef void (CREO_BRL_FRONTEND_CALL *creo_brl_apply_options_fn_t)(
    const struct creo_brl_conversion_options *options);

struct creo_brl_frontend_api {
    creo_brl_status_fn_t show_status;
    creo_brl_popup_fn_t show_popup;
    creo_brl_apply_options_fn_t apply_options;
};

#ifdef __cplusplus
}
#endif

#endif
