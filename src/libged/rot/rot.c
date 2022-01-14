/*                         R O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

    if ((ret = ged_rot_args(gedp, argc, argv, &coord, rmat)) != GED_OK)
	return ret;

    return _ged_do_rot(gedp, coord, rmat, (int (*)())0);
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rot_cmd_impl = {"rot", ged_rot_core, GED_CMD_DEFAULT};
const struct ged_cmd rot_cmd = { &rot_cmd_impl };

extern int ged_rotate_about_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl rotate_about_cmd_impl = {"rot_about", ged_rotate_about_core, GED_CMD_DEFAULT};
const struct ged_cmd rotate_about_cmd = { &rotate_about_cmd_impl };

const struct ged_cmd *rot_cmds[] = { &rot_cmd, &rotate_about_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rot_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
