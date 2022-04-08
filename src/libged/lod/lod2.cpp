/*                           L O D . C P P
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


extern "C" int
ged_lod2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "lod (on|off|enabled)\n"
    "lod scale (points|curves) <factor>\n"
    "lod redraw (off|onzoom)\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bview *cv = gedp->ged_gvp;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to change lod setting on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	cv = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    if (!cv) {
	return BRLCAD_OK;
    }

    /* must be wanting help */
    if (argc >= 2 && BU_STR_EQUAL(argv[1], "-h")) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_HELP;
    }

    /* Print current state if no args are supplied */
    if (argc == 1) {
	if (cv->gv_s->adaptive_plot) {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "LoD drawing: disabled\n");
	}
	if (cv->gv_s->redraw_on_zoom) {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: enabled\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Redraw on zoom: disabled\n");
	}
	bu_vls_printf(gedp->ged_result_str, "Point scale: %g\n", cv->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "Curve scale: %g\n", cv->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "BoT face threshold: %zd\n", cv->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    /* determine subcommand */
    --argc; ++argv;
    int printUsage = 0;
    if (argc == 1 && BU_STR_EQUAL(argv[0], "on")) {
	/* lod on */
	if (cv) {
	    if (!cv->gv_s->adaptive_plot) {
		cv->gv_s->adaptive_plot = 1;
		const char *cmd = "redraw";
		ged_exec(gedp, 1, (const char **)&cmd);
	    }
	} else {
	    int delta = 0;
	    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
		if (!v)
		    continue;
		if (!v->gv_s->adaptive_plot) {
		    v->gv_s->adaptive_plot = 1;
		    delta = 1;
		}
	    }
	    if (delta) {
		const char *cmd = "redraw";
		ged_exec(gedp, 1, (const char **)&cmd);
	    }
	}
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "off")) {
	/* lod off */
	if (cv) {
	    if (cv->gv_s->adaptive_plot) {
		cv->gv_s->adaptive_plot = 0;
		const char *cmd = "redraw";
		ged_exec(gedp, 1, (const char **)&cmd);
	    }
	} else {
	    int delta = 0;
	    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
		struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
		if (!v)
		    continue;
		if (v->gv_s->adaptive_plot) {
		    v->gv_s->adaptive_plot = 0;
		    delta = 1;
		}

		if (delta) {
		    const char *cmd = "redraw";
		    ged_exec(gedp, 1, (const char **)&cmd);
		}
	    }

	}
    } else if (argc == 1 && BU_STR_EQUAL(argv[0], "enabled")) {
	/* lod enabled - return on state */
	bu_vls_printf(gedp->ged_result_str, "%d", cv->gv_s->adaptive_plot);
    } else if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 2 || argc == 3) {
	    if (BU_STR_EQUAL(argv[1], "points")) {
		if (argc == 2) {
		    /* lod scale points - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", cv->gv_s->point_scale);
		} else {
		    /* lod scale points f - set value */
		    cv->gv_s->point_scale = atof(argv[2]);
		}
	    } else if (BU_STR_EQUAL(argv[1], "curves")) {
		if (argc == 2) {
		    /* lod scale curves - return current value */
		    bu_vls_printf(gedp->ged_result_str, "%f", cv->gv_s->curve_scale);
		} else {
		    /* lod scale curves f - set value */
		    cv->gv_s->curve_scale = atof(argv[2]);
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
	    if (cv->gv_s->redraw_on_zoom) {
		bu_vls_printf(gedp->ged_result_str, "onzoom");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "off");
	    }
	    printUsage = 0;
	} else if (argc == 2) {
	    if (BU_STR_EQUAL(argv[1], "off")) {
		/* lod redraw off */
		cv->gv_s->redraw_on_zoom = 0;
		printUsage = 0;
	    } else if (BU_STR_EQUAL(argv[1], "onzoom")) {
		/* lod redraw onzoom */
		cv->gv_s->redraw_on_zoom = 1;
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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

