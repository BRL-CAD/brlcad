/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
/** @file lod.c
 *
 * Level of Detail drawing configuration command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/geom.h"
#include "bsg/view_state.h"

#include "../ged_private.h"

int
ged_lod_core(struct ged *gedp, int argc, const char *argv[])
{
    struct bsg_view *gvp;
    int printUsage = 0;
    static const char *usage = "lod (on|off|enabled)\n"
			       "lod scale (points|curves) <factor>\n";

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc >= 2 && BU_STR_EQUAL(argv[1], "-h")) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return GED_HELP;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	return BRLCAD_OK;
    }
    struct bsg_lod_source_policy_settings lod_policy;
    if (!bsg_view_lod_source_policy_get(gvp, &lod_policy))
	return BRLCAD_ERROR;
    /* Print current state if no args are supplied */
    if (argc == 1) {
	if (lod_policy.csg_enabled) {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: disabled\n");
	}
	bu_vls_printf(gedp->ged_result_str, "Point scale: %g\n", lod_policy.point_scale);
	bu_vls_printf(gedp->ged_result_str, "Curve scale: %g\n", lod_policy.curve_scale);
	bu_vls_printf(gedp->ged_result_str, "BoT face threshold: %zu\n", lod_policy.bot_threshold);
	return BRLCAD_OK;
    }

    /* determine subcommand */
    --argc;
    ++argv;
    printUsage = 0;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "on")) {
	/* lod on */
	lod_policy.csg_enabled = 1;
	lod_policy.zoom_refresh = 1;
	bsg_view_lod_source_policy_set(gvp, &lod_policy);
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "off")) {
	/* lod off */
	lod_policy.csg_enabled = 0;
	if (!lod_policy.mesh_enabled)
	    lod_policy.zoom_refresh = 0;
	bsg_view_lod_source_policy_set(gvp, &lod_policy);
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "enabled")) {
	/* lod enabled - return on state */
	bu_vls_printf(gedp->ged_result_str, "%d", lod_policy.csg_enabled);
    } else if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 2 || argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "points")) {
		if (argc == 2) {
		    /* lod scale points - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", lod_policy.point_scale);
		} else {
		    /* lod scale points f - set value */
		    lod_policy.point_scale = atof(argv[2]);
		    bsg_view_lod_source_policy_set(gvp, &lod_policy);
		}
	    } else if (BU_STR_EQUAL(argv[1], "curves")) {
		if (argc == 2) {
		    /* lod scale curves - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", lod_policy.curve_scale);
		} else {
		    /* lod scale curves f - set value */
		    lod_policy.curve_scale = atof(argv[2]);
		    bsg_view_lod_source_policy_set(gvp, &lod_policy);
		}
	    } else {
		printUsage = 1;
	    }
	} else {
	    printUsage = 1;
	}
    } else {
	printUsage = 1;
    }

    if (printUsage) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_LOD_COMMANDS(X, XID) \
    X(lod, ged_lod_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_LOD_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_lod", 1, GED_LOD_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
