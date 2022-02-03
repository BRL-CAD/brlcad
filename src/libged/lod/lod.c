/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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

extern int ged_lod2_core(struct ged *gedp, int argc, const char *argv[]);
int
ged_lod_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
	return ged_lod2_core(gedp, argc, argv);

    struct bview *gvp;
    int printUsage = 0;
    static const char *usage = "lod (on|off|enabled)\n"
			       "lod scale (points|curves) <factor>\n"
			       "lod redraw (off|onzoom)\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc >= 2 && BU_STR_EQUAL(argv[1], "-h")) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_HELP;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	return BRLCAD_OK;
    }

    /* Print current state if no args are supplied */
    if (argc == 1) {
	if (gvp->gv_s->adaptive_plot) {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: disabled\n");
	}
	if (gvp->gv_s->redraw_on_zoom) {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: disabled\n");
	}
	bu_vls_printf(gedp->ged_result_str, "Point scale: %g\n", gvp->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "Curve scale: %g\n", gvp->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "BoT face threshold: %zd\n", gvp->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    /* determine subcommand */
    --argc;
    ++argv;
    printUsage = 0;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "on")) {
	/* lod on */
	gvp->gv_s->adaptive_plot = 1;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "off")) {
	/* lod off */
	gvp->gv_s->adaptive_plot = 0;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "enabled")) {
	/* lod enabled - return on state */
	bu_vls_printf(gedp->ged_result_str, "%d", gvp->gv_s->adaptive_plot);
    } else if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 2 || argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "points")) {
		if (argc == 2) {
		    /* lod scale points - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->gv_s->point_scale);
		} else {
		    /* lod scale points f - set value */
		    gvp->gv_s->point_scale = atof(argv[2]);
		}
	    } else if (BU_STR_EQUAL(argv[1], "curves")) {
		if (argc == 2) {
		    /* lod scale curves - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->gv_s->curve_scale);
		} else {
		    /* lod scale curves f - set value */
		    gvp->gv_s->curve_scale = atof(argv[2]);
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
	    if (gvp->gv_s->redraw_on_zoom) {
		bu_vls_printf(gedp->ged_result_str, "onzoom");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "off");
	    }
	    printUsage = 0;
	} else if (argc == 2) {
	    if (BU_STR_EQUAL(argv[1], "off")) {
		/* lod redraw off */
		gvp->gv_s->redraw_on_zoom = 0;
		printUsage = 0;
	    } else if (BU_STR_EQUAL(argv[1], "onzoom")) {
		/* lod redraw onzoom */
		gvp->gv_s->redraw_on_zoom = 1;
		printUsage = 0;
	    }
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
