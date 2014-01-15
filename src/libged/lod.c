/*                           L O D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
#include "bio.h"

#include "rtgeom.h"

#include "./ged_private.h"


int
ged_lod(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_view *gvp;
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
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return GED_HELP;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	return GED_OK;
    }

    /* determine subcommand */
    --argc;
    ++argv;
    printUsage = 0;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "on")) {
	/* lod on */
	gvp->gv_adaptive_plot = 1;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "off")) {
	/* lod off */
	gvp->gv_adaptive_plot = 0;
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "enabled")) {
	/* lod enabled - return on state */
	bu_vls_printf(gedp->ged_result_str, "%d", gvp->gv_adaptive_plot);
    } else if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 2 || argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "points")) {
		if (argc == 2) {
		    /* lod scale points - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->gv_point_scale);
		} else {
		    /* lod scale points f - set value */
		    gvp->gv_point_scale = atof(argv[2]);
		}
	    } else if (BU_STR_EQUAL(argv[1], "curves")) {
		if (argc == 2) {
		    /* lod scale curves - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", gvp->gv_curve_scale);
		} else {
		    /* lod scale curves f - set value */
		    gvp->gv_curve_scale = atof(argv[2]);
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
	    if (gvp->gv_redraw_on_zoom) {
		bu_vls_printf(gedp->ged_result_str, "onzoom");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "off");
	    }
	    printUsage = 0;
	} else if (argc == 2) {
	    if (BU_STR_EQUAL(argv[1], "off")) {
		/* lod redraw off */
		gvp->gv_redraw_on_zoom = 0;
		printUsage = 0;
	    } else if (BU_STR_EQUAL(argv[1], "onzoom")) {
		/* lod redraw onzoom */
		gvp->gv_redraw_on_zoom = 1;
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


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
