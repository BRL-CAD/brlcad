/*                         D I R 2 A E. C
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
/** @file libged/dir2ae.c
 *
 * The dir2ae command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_dir2ae_core(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t az, el;
    vect_t dir;
    double scan[3];
    int iflag;
    static const char *usage = "[-i] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argv[1][0] == '-' && argv[1][1] == 'i' && argv[1][2] == '\0') {
	iflag = 1;
	--argc;
	++argv;
    } else
	iflag = 0;

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[1], "%lf", &scan[X]) != 1 ||
	sscanf(argv[2], "%lf", &scan[Y]) != 1 ||
	sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(dir, scan);

    if (iflag)
	VSCALE(dir, dir, -1);

    AZEL_FROM_V3DIR(az, el, dir);
    bu_vls_printf(gedp->ged_result_str, "%lf %lf", az, el);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl dir2ae_cmd_impl = {
    "dir2ae",
    ged_dir2ae_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd dir2ae_cmd = { &dir2ae_cmd_impl };
const struct ged_cmd *dir2ae_cmds[] = { &dir2ae_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dir2ae_cmds, 1 };

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
