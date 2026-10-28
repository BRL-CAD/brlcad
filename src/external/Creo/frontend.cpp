/**
 *                 F R O N T E N D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/**
 * @file frontend.cpp
 *
 * Registered Creo facade entrypoints.
 */

#include "common.h"

#include "frontend/dialog_controller.h"


extern "C" void __cdecl
creo_brl_frontend_apply_options(const struct creo_brl_conversion_options *options)
{
    creo_brl_frontend_dialog_apply_options(options);
}


extern "C" int
creo_brl_frontend_command(uiCmdCmdId command, uiCmdValue *value,
                          void *push_command_data)
{
    return creo_brl_frontend_dialog_command(command, value, push_command_data);
}


extern "C" uiCmdAccessState
creo_brl_frontend_access(uiCmdAccessMode UNUSED(access_mode))
{
    return ACCESS_AVAILABLE;
}
