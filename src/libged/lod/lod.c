/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include "../ged_private.h"


int
ged_lod_core(struct ged *gedp, int argc, const char *argv[])
{
    struct bv *gvp;
    int printUsage = 0;
    static const char *usage = "lod (on|off|enabled)\n"
			       "lod scale (points|curves) <factor>\n"
			       "lod redraw (off|onzoom)\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc >= 2 && BU_STR_EQUAL(argv[1], "-h")) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return GED_HELP;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	return GED_OK;
    }

    /* Print current state if no args are supplied */
    if (argc == 1) {
	if (gvp->adaptive_plot) {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: disabled\n");
	}
	if (gvp->redraw_on_zoom) {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: disabled\n");
	}
	bu_vls_printf(gedp->ged_result_str, "Point scale: %g\n", gvp->point_scale);
	bu_vls_printf(gedp->ged_result_str, "Curve scale: %g\n", gvp->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "BoT face threshold: %zd\n", gvp->bot_threshold);
	return GED_OK;
    }

    /* determine subcommand */
    --argc;
    ++argv;
    printUsage = 0;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "on")) {
	/* lod on */
	gvp->adaptive_plot = 1;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "off")) {
	/* lod off */
	gvp->adaptive_plot = 0;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "enabled")) {
	/* lod enabled - return on state */
	bu_vls_printf(gedp->ged_result_str, "%d", gvp->adaptive_plot);
    } else if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 2 || argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "points")) {
		if (argc == 2) {
		    /* lod scale points - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->point_scale);
		} else {
		    /* lod scale points f - set value */
		    gvp->point_scale = atof(argv[2]);
		}
	    } else if (BU_STR_EQUAL(argv[1], "curves")) {
		if (argc == 2) {
		    /* lod scale curves - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->curve_scale);
		} else {
		    /* lod scale curves f - set value */
		    gvp->curve_scale = atof(argv[2]);
		}
	    } else {
		printUsage = 1;
	    }
	} else {
	    printUsage = 1;
	}
    } else if (BU_STR_EQUAL(argv[0], "redraw")) {
	printUsage = 1;
	if (argc == 1) {
	    /* lod redraw - return current value */
	    if (gvp->redraw_on_zoom) {
		bu_vls_printf(gedp->ged_result_str, "onzoom");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "off");
	    }
	    printUsage = 0;
	} else if (argc == 2) {
	    if (BU_STR_EQUAL(argv[1], "off")) {
		/* lod redraw off */
		gvp->redraw_on_zoom = 0;
		printUsage = 0;
	    } else if (BU_STR_EQUAL(argv[1], "onzoom")) {
		/* lod redraw onzoom */
		gvp->redraw_on_zoom = 1;
		printUsage = 0;
	    }
	}
    } else {
	printUsage = 1;
    }

    if (printUsage) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return GED_ERROR;
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl lod_cmd_impl = {
    "lod",
    ged_lod_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd lod_cmd = { &lod_cmd_impl };
const struct ged_cmd *lod_cmds[] = { &lod_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  lod_cmds, 1 };

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
