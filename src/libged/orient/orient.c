/*                         O R I E N T . C
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
/** @file libged/orient.c
 *
 * The orient command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_orient_core(struct ged *gedp, int argc, const char *argv[])
{
    quat_t quat;
    static const char *usage = "quat";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* set view orientation */
    if (argc == 2) {
	if (bn_decode_quat(quat, argv[1]) != 4) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	int i;

	for (i = 1; i < 5; ++i) {
	    double scan;
	    if (sscanf(argv[i], "%lf", &scan) != 1) {
		bu_vls_printf(gedp->ged_result_str, "ged_orient_core: bad value - %s\n", argv[i-1]);
		return GED_ERROR;
	    }
	    /* convert from double to fastf_t */
	    quat[i-1] = scan;
	}
    }

    quat_quat2mat(gedp->ged_gvp->gv_rotation, quat);
    bv_update(gedp->ged_gvp);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"

struct ged_cmd_impl orient_cmd_impl = {"orient", ged_orient_core, GED_CMD_DEFAULT};
const struct ged_cmd orient_cmd = { &orient_cmd_impl };

struct ged_cmd_impl orientation_cmd_impl = {"orientation", ged_orient_core, GED_CMD_DEFAULT};
const struct ged_cmd orientation_cmd = { &orientation_cmd_impl };


const struct ged_cmd *orient_cmds[] = { &orient_cmd, &orientation_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  orient_cmds, 2 };

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
