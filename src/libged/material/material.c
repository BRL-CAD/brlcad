/*                         M A T E R I A L . C
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
/** @file libged/material.c
 *
 * The material command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"

typedef enum {
    MATERIAL_CREATE,
    MATERIAL_DESTROY,
    MATERIAL_GET,
    MATERIAL_SET,
    ATTR_UNKNOWN
} material_cmd_t;

static const char *usage = " [create] [destroy] [get] [set] [options] object [args] \n";

HIDDEN material_cmd_t
get_material_cmd(const char* arg)
{
    /* sub-commands */
    const char CREATE[] = "create";
    const char DESTROY[]   = "destroy";
    const char GET[]    = "get";
    const char SET[]    = "set";

    /* alphabetical order */
    if (BU_STR_EQUIV(CREATE, arg))
	return MATERIAL_CREATE;
    else if (BU_STR_EQUIV(DESTROY, arg))
	return MATERIAL_DESTROY;
    else if (BU_STR_EQUIV(SET, arg))
	return MATERIAL_SET;
    else if (BU_STR_EQUIV(GET, arg))
	return MATERIAL_GET;
    else
    return ATTR_UNKNOWN;
}

int
ged_material_core(struct ged *gedp, int argc, const char *argv[]){
    material_cmd_t scmd;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialization */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* incorrect arguments */
    if (argc < 3) {
        bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
        return GED_HELP;
    }

    scmd = get_material_cmd(argv[1]);

    if (scmd == MATERIAL_CREATE){
        // create routine
        bu_vls_printf(gedp->ged_result_str, "Trying: create");
    } else if (scmd == MATERIAL_DESTROY) {
        // destroy routine
        bu_vls_printf(gedp->ged_result_str, "Trying: destroy");
    } else if (scmd == MATERIAL_GET) {
        // get routine
        bu_vls_printf(gedp->ged_result_str, "Trying: get");
    } else if (scmd == MATERIAL_SET) {
        // set routine
        bu_vls_printf(gedp->ged_result_str, "Trying: set");
    } 

    return 0;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl material_cmd_impl = {
    "material",
    ged_material_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd material_cmd = { &material_cmd_impl };
const struct ged_cmd *material_cmds[] = { &material_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  material_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */