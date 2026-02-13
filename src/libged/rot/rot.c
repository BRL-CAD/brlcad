/*                         R O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/rot.c
 *
 * The rot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_rot_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char coord;
    mat_t rmat;

    if ((ret = ged_rot_args(gedp, argc, argv, &coord, rmat)) != BRLCAD_OK)
	return ret;

    return _ged_do_rot(gedp, coord, rmat, NULL);
}


#include "../include/plugin.h"

extern int ged_rotate_about_core(struct ged *gedp, int argc, const char *argv[]);

#define GED_ROT_COMMANDS(X, XID) \
    X(rot, ged_rot_core, GED_CMD_DEFAULT) \
    X(rot_about, ged_rotate_about_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ROT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_rot", 1, GED_ROT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
