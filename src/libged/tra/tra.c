/*                         T R A . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/tra.c
 *
 * The tra command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_tra_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    char coord;
    vect_t tvec;

    if ((ret = ged_tra_args(gedp, argc, argv, &coord, tvec)) != GED_OK)
	return ret;

    return _ged_do_tra(gedp, coord, tvec, (int (*)())0);
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl tra_cmd_impl = {
    "tra",
    ged_tra_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd tra_cmd = { &tra_cmd_impl };
const struct ged_cmd *tra_cmds[] = { &tra_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  tra_cmds, 1 };

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
