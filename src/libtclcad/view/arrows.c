/*                       A R R O W S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2026 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file libtclcad/view/arrows.c
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "bsg/feature.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../tclcad_private.h"
#include "../view/view.h"
#include "../bsg_move_helpers.h"

/* Phase T3 (drawing_stack_modernization): the "view get" introspection path
 * (getters in to_data_arrows_func) now recovers values by reading the BSG
 * object fields rather than gv_tcl directly, making BSG the canonical read
 * source for Tcl introspection.
 *
 * Setters no longer write gv_tcl at all; they mutate the BSG object in-place
 * (color, line_width, tip_length, tip_width, draw) or rebuild it from scratch
 * preserving current style (points).  gv_tcl is no longer mirrored. */

int
go_data_arrows(Tcl_Interp *interp,
	       struct ged *gedp,
	       struct bsg_view *gdvp,
	       int argc,
	       const char *argv[],
	       const char *usage)
{
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    to_refresh_suppress_all_begin(current_top);

    ret = to_data_arrows_func(interp, gedp, gdvp, argc, argv);
    to_refresh_suppress_all_end(current_top);
    if (ret & BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_arrows(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    struct bsg_view *gdvp;
    int ret;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3 || 6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gdvp = bsg_set_find_view(&gedp->ged_views, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* shift the command name to argv[1] before calling to_data_arrows_func */
    argv[1] = argv[0];
    ret = to_data_arrows_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1);
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}


int
to_data_arrows_func(Tcl_Interp *interp,
		    struct ged *gedp,
		    struct bsg_view *gdvp,
		    int argc,
		    const char *argv[])
{
    /* T3: BSG object name is the only per-variant state needed here. */
    const char *bsg_name = (argv[0][0] == 's') ? "_tcl_sdata_arrows" : "_tcl_data_arrows";

    if (BU_STR_EQUAL(argv[1], "draw")) {
	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    bu_vls_printf(gedp->ged_result_str, "%d", bsg_feature_ref_is_null(ref) ? 0 : 1);
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int i;

	    if (bu_sscanf(argv[2], "%d", &i) != 1)
		goto bad;

	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    if (!bsg_feature_ref_is_null(ref))
		bsg_feature_set_visible(ref, i ? 1 : 0);
	    /* If no BSG object exists and draw=1 is requested, nothing to show
	     * yet (no points have been set); silently no-op. */

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "color")) {
	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	    if (bsg_feature_style_get(ref, &style) && style.color_valid) {
		bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			      (int)style.color[0], (int)style.color[1], (int)style.color[2]);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "0 0 0");
	    }
	    return BRLCAD_OK;
	}

	if (argc == 5) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[2], "%d", &r) != 1 ||
		bu_sscanf(argv[3], "%d", &g) != 1 ||
		bu_sscanf(argv[4], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    if (!bsg_feature_ref_is_null(ref))
		bsg_feature_set_color(ref, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "line_width")) {
	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
	    if (bsg_feature_style_get(ref, &style))
		bu_vls_printf(gedp->ged_result_str, "%d", style.line_width);
	    else
		bu_vls_printf(gedp->ged_result_str, "0");
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int line_width;

	    if (bu_sscanf(argv[2], "%d", &line_width) != 1)
		goto bad;

	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    if (!bsg_feature_ref_is_null(ref))
		bsg_feature_set_line_width(ref, line_width);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "points")) {
	register int i;

	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    point_t *pts = NULL;
	    size_t npts = 0;
	    if (bsg_feature_points_copy(ref, &pts, NULL, &npts)) {
		for (size_t _j = 0; _j < npts; _j++)
		    bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ", V3ARGS(pts[_j]));
		bu_free(pts, "bsg feature points copy");
	    }
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int ac;
	    const char **av;

	    if (Tcl_SplitList(interp, argv[2], &ac, &av) != TCL_OK) {
		bu_vls_printf(gedp->ged_result_str, "%s", Tcl_GetStringResult(interp));
		return BRLCAD_ERROR;
	    }

	    if (ac % 2) {
		bu_vls_printf(gedp->ged_result_str, "%s: must be an even number of points", argv[0]);
		Tcl_Free((char *)av);
		return BRLCAD_ERROR;
	    }

	    /* T3: save style from existing BSG object before replacing it. */
	    int saved_color[3]; int saved_lw, saved_tl, saved_tw, saved_vis;
	    bsg_feature_ref old_ref = bsg_feature_find(gdvp, bsg_name);
	    _bsg_read_style(old_ref, saved_color, &saved_lw, &saved_tl, &saved_tw, &saved_vis);

	    /* Clear out: remove old BSG object. */
	    if (ac < 2) {
		bsg_feature_remove(gdvp, bsg_name);
		Tcl_Free((char *)av);
		to_refresh_view(gdvp);
		return BRLCAD_OK;
	    }

	    /* Parse points into temporary local array. */
	    point_t *pts = (point_t *)bu_calloc(ac, sizeof(point_t), "arrow points");
	    for (i = 0; i < ac; ++i) {
		double scan[ELEMENTS_PER_VECT];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);
		    bu_free(pts, "arrow points");
		    Tcl_Free((char *)av);
		    return BRLCAD_ERROR;
		}
		VMOVE(pts[i], scan);
	    }

	    /* T3: rebuild BSG from new points, preserving style (no gv_tcl write). */
	    _bsg_rebuild_arrows(gdvp, bsg_name, pts, ac,
			       saved_color, saved_lw, saved_tl, saved_tw, saved_vis);
	    bu_free(pts, "arrow points");
	    Tcl_Free((char *)av);
	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}
    }

    if (BU_STR_EQUAL(argv[1], "tip_length")) {
	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    fastf_t tip_length = 0.0;
	    if (bsg_feature_arrow_tip_get(ref, &tip_length, NULL)) {
		bu_vls_printf(gedp->ged_result_str, "%d", (int)tip_length);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "0");
	    }
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int tip_length;

	    if (bu_sscanf(argv[2], "%d", &tip_length) != 1)
		goto bad;

	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    fastf_t tip_width = 0.0;
	    if (bsg_feature_arrow_tip_get(ref, NULL, &tip_width))
		bsg_feature_arrow_tip_set(ref, (fastf_t)tip_length, tip_width);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "tip_width")) {
	if (argc == 2) {
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    fastf_t tip_width = 0.0;
	    if (bsg_feature_arrow_tip_get(ref, NULL, &tip_width)) {
		bu_vls_printf(gedp->ged_result_str, "%d", (int)tip_width);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "0");
	    }
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    int tip_width;

	    if (bu_sscanf(argv[2], "%d", &tip_width) != 1)
		goto bad;

	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    fastf_t tip_length = 0.0;
	    if (bsg_feature_arrow_tip_get(ref, &tip_length, NULL))
		bsg_feature_arrow_tip_set(ref, tip_length, (fastf_t)tip_width);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

bad:
    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
