/*                         T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2021 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file libged/track.c
 *
 * Adds "tracks" to the data file given the required info
 *
 * Acknowledgements:
 * Modifications by Bob Parker (SURVICE Engineering):
 * *- adapt for use in LIBRT's database object
 * *- removed prompting for input
 * *- removed signal catching
 * *- added basename parameter
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

/*
 *
 * Adds track given "wheel" info.
 *
 */
int
ged_track_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "basename rX1 rX2 rZ rR dX dZ dR iX iZ iR minX minY th";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 15) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    return ged_track2(gedp->ged_result_str, gedp->ged_wdbp, argv);
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl track_cmd_impl = {
    "track",
    ged_track_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd track_cmd = { &track_cmd_impl };
const struct ged_cmd *track_cmds[] = { &track_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  track_cmds, 1 };

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
