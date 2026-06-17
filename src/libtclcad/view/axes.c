/*                           A X E S . C
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
/** @file libtclcad/view/axes.c
 *
 */
/** @} */

#include "common.h"
#include "bu/units.h"
#include "bsg/feature.h"
#include "dm.h"
#include "ged.h"
#include "tclcad.h"

/* Private headers */
#include "../tclcad_private.h"
#include "../view/view.h"
#include "../bsg_move_helpers.h"

/* Phase T3 (drawing_stack_modernization): all getters and setters in
 * to_data_axes_func now operate on BSG objects directly.  gv_tcl is no longer
 * read or written by this path; BSG is the sole canonical store.
 * BVDAS_DEFAULT_DM_WIDTH is the pixel-width fallback for dm_width in sf calcs. */
#define BVDAS_DEFAULT_DM_WIDTH 512  /* fallback pixel width when no DM is attached */

int
to_axes(struct ged *gedp,
	struct bsg_view *gdvp,
	struct bsg_axes *gasp,
	int argc,
	const char *argv[],
	const char *usage)
{

    if (BU_STR_EQUAL(argv[2], "draw")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->draw);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->draw = 1;
	    else
		gasp->draw = 0;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_size")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf", gasp->axes_size);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &size) != 1)
		goto bad;

	    gasp->axes_size = size;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_pos")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%lf %lf %lf",
			  V3ARGS(gasp->axes_pos));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    double x, y, z; /* must be double for scanf */

	    if (bu_sscanf(argv[3], "%lf", &x) != 1 ||
		bu_sscanf(argv[4], "%lf", &y) != 1 ||
		bu_sscanf(argv[5], "%lf", &z) != 1)
		goto bad;

	    VSET(gasp->axes_pos, x, y, z);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "axes_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->axes_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->axes_color, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "label_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->label_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->label_color, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "line_width")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->line_width);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int line_width;

	    if (bu_sscanf(argv[3], "%d", &line_width) != 1)
		goto bad;

	    gasp->line_width = line_width;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "pos_only")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->pos_only);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->pos_only = 1;
	    else
		gasp->pos_only = 0;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_color, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_enable")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_enabled);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->tick_enabled = 1;
	    else
		gasp->tick_enabled = 0;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_interval")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%f", gasp->tick_interval);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_interval;

	    if (bu_sscanf(argv[3], "%d", &tick_interval) != 1)
		goto bad;

	    gasp->tick_interval = tick_interval;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_length);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_length;

	    if (bu_sscanf(argv[3], "%d", &tick_length) != 1)
		goto bad;

	    gasp->tick_length = tick_length;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  V3ARGS(gasp->tick_major_color));
	    return BRLCAD_OK;
	}

	if (argc == 6) {
	    int r, g, b;

	    /* set background color */
	    if (bu_sscanf(argv[3], "%d", &r) != 1 ||
		bu_sscanf(argv[4], "%d", &g) != 1 ||
		bu_sscanf(argv[5], "%d", &b) != 1)
		goto bad;

	    /* validate color */
	    if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
		goto bad;

	    VSET(gasp->tick_major_color, r, g, b);

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_major_length")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_major_length);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_major_length;

	    if (bu_sscanf(argv[3], "%d", &tick_major_length) != 1)
		goto bad;

	    gasp->tick_major_length = tick_major_length;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "ticks_per_major")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->ticks_per_major);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int ticks_per_major;

	    if (bu_sscanf(argv[3], "%d", &ticks_per_major) != 1)
		goto bad;

	    gasp->ticks_per_major = ticks_per_major;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "tick_threshold")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->tick_threshold);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int tick_threshold;

	    if (bu_sscanf(argv[3], "%d", &tick_threshold) != 1)
		goto bad;

	    if (tick_threshold < 1)
		tick_threshold = 1;

	    gasp->tick_threshold = tick_threshold;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[2], "triple_color")) {
	if (argc == 3) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gasp->triple_color);
	    return BRLCAD_OK;
	}

	if (argc == 4) {
	    int i;

	    if (bu_sscanf(argv[3], "%d", &i) != 1)
		goto bad;

	    if (i)
		gasp->triple_color = 1;
	    else
		gasp->triple_color = 0;

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}

int
go_data_axes(Tcl_Interp *interp,
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

    ret = to_data_axes_func(interp, gedp, gdvp, argc, argv);
    to_refresh_suppress_all_end(current_top);
    if (ret & BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}

int
to_data_axes(struct ged *gedp,
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

    /* shift the command name to argv[1] before calling to_data_axes_func */
    argv[1] = argv[0];
    ret = to_data_axes_func(current_top->to_interp, gedp, gdvp, argc-1, argv+1);
    if (ret == BRLCAD_ERROR)
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return ret;
}

int
to_data_axes_func(Tcl_Interp *interp,
		  struct ged *gedp,
		  struct bsg_view *gdvp,
		  int argc,
		  const char *argv[])
{
    /* T3: BSG object name is the only per-variant state needed here. */
    const char *bsg_name = (argv[0][0] == 's') ? "_tcl_sdata_axes" : "_tcl_data_axes";

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

    if (BU_STR_EQUAL(argv[1], "size")) {
	if (argc == 2) {
	    /* T3: recover the encoded half-size from BSG X-axis endpoints and
	     * back-compute size = 2*half / sf.  Returns approximate value. */
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    if (!bsg_feature_ref_is_null(ref)) {
		point_t *_all = NULL;
		int _ntotal = _bsg_extract_pts(ref, &_all);
		if (_ntotal >= 2) {
		    fastf_t _half = (_all[1][X] - _all[0][X]) * 0.5;
		    fastf_t _dm_width = (fastf_t)BVDAS_DEFAULT_DM_WIDTH;
		    if (gdvp->dmp) {
			int _w = dm_get_width((struct dm *)gdvp->dmp);
			if (_w > 0) _dm_width = (fastf_t)_w;
		    }
		    fastf_t _sf = gdvp->gv_size / _dm_width;
		    fastf_t _size = (_sf > 0.0) ? (_half * 2.0 / _sf) : 0.0;
		    bu_vls_printf(gedp->ged_result_str, "%lf", _size);
		} else {
		    bu_vls_printf(gedp->ged_result_str, "0.0");
		}
		bu_free(_all, "bsg pts");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "0.0");
	    }
	    return BRLCAD_OK;
	}

	if (argc == 3) {
	    double size; /* must be double for scanf */

	    if (bu_sscanf(argv[2], "%lf", &size) != 1)
		goto bad;

	    /* T3: extract current centers, rebuild with new halfAxesSize; no gv_tcl write. */
	    bsg_feature_ref old_ref = bsg_feature_find(gdvp, bsg_name);
	    int _color[3]; int _lw, _vis;
	    _bsg_read_style(old_ref, _color, &_lw, NULL, NULL, &_vis);

	    point_t *_cpts = NULL;
	    int _ncpts = !bsg_feature_ref_is_null(old_ref) ? _bsg_extract_axes_centers(old_ref, &_cpts) : 0;

	    fastf_t _dm_width = (fastf_t)BVDAS_DEFAULT_DM_WIDTH;
	    if (gdvp->dmp) {
		int _w = dm_get_width((struct dm *)gdvp->dmp);
		if (_w > 0) _dm_width = (fastf_t)_w;
	    }
	    fastf_t _sf = gdvp->gv_size / _dm_width;
	    fastf_t _half = (fastf_t)size * 0.5f * _sf;

	    _bsg_rebuild_axes(gdvp, bsg_name, _cpts, _ncpts, _half, _color, _lw, _vis);
	    bu_free(_cpts, "bsg axes pts");

	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}

	goto bad;
    }

    if (BU_STR_EQUAL(argv[1], "points")) {
	register int i;

	if (argc == 2) {
	    /* T3: recover center points from BSG vlist. */
	    bsg_feature_ref ref = bsg_feature_find(gdvp, bsg_name);
	    if (!bsg_feature_ref_is_null(ref)) {
		point_t *_cpts = NULL;
		int _ncpts = _bsg_extract_axes_centers(ref, &_cpts);
		for (i = 0; i < _ncpts; ++i)
		    bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ", V3ARGS(_cpts[i]));
		bu_free(_cpts, "bsg axes pts");
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

	    /* T3: save style and size from existing BSG object before replacing it. */
	    bsg_feature_ref old_ref = bsg_feature_find(gdvp, bsg_name);
	    int _color[3]; int _lw, _vis;
	    _bsg_read_style(old_ref, _color, &_lw, NULL, NULL, &_vis);

	    /* Recover halfAxesSize from existing object (use default 1.0 if none). */
	    fastf_t _half = 1.0;
	    if (!bsg_feature_ref_is_null(old_ref)) {
		point_t *_all = NULL;
		int _ntotal = _bsg_extract_pts(old_ref, &_all);
		if (_ntotal >= 2)
		    _half = (_all[1][X] - _all[0][X]) * 0.5;
		bu_free(_all, "bsg pts");
	    }

	    /* Clear out: remove old BSG object. */
	    if (ac < 1) {
		bsg_feature_remove(gdvp, bsg_name);
		to_refresh_view(gdvp);
		Tcl_Free((char *)av);
		return BRLCAD_OK;
	    }

	    /* Parse new center points into temporary local array. */
	    point_t *pts = (point_t *)bu_calloc(ac, sizeof(point_t), "axes points");
	    for (i = 0; i < ac; ++i) {
		double scan[3];

		if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);
		    bu_free(pts, "axes points");
		    Tcl_Free((char *)av);
		    return BRLCAD_ERROR;
		}
		VMOVE(pts[i], scan);
	    }

	    /* T3: rebuild BSG from new centers, preserving style; no gv_tcl write. */
	    _bsg_rebuild_axes(gdvp, bsg_name, pts, ac, _half, _color, _lw, _vis);
	    bu_free(pts, "axes points");
	    Tcl_Free((char *)av);
	    to_refresh_view(gdvp);
	    return BRLCAD_OK;
	}
    }

bad:
    return BRLCAD_ERROR;
}

int
to_model_axes(struct ged *gedp,
	      int argc,
	      const char *argv[],
	      ged_func_ptr UNUSED(func),
	      const char *usage,
	      int UNUSED(maxargs))
{
    struct bsg_view *gdvp;

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

    struct bsg_axes axes;
    if (!bsg_view_model_axes_get(gdvp, &axes))
	return BRLCAD_ERROR;
    int ret = to_axes(gedp, gdvp, &axes, argc, argv, usage);
    if (ret == BRLCAD_OK)
	bsg_view_model_axes_set(gdvp, &axes);
    return ret;
}

int
go_view_axes(struct ged *gedp,
	     struct bsg_view *gdvp,
	     int argc,
	     const char *argv[],
	     const char *usage)
{
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

    struct bsg_axes axes;
    if (!bsg_view_view_axes_get(gdvp, &axes))
	return BRLCAD_ERROR;
    int ret = to_axes(gedp, gdvp, &axes, argc, argv, usage);
    if (ret == BRLCAD_OK)
	bsg_view_view_axes_set(gdvp, &axes);
    return ret;
}


int
to_view_axes(struct ged *gedp,
	     int argc,
	     const char *argv[],
	     ged_func_ptr UNUSED(func),
	     const char *usage,
	     int UNUSED(maxargs))
{
    struct bsg_view *gdvp;

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

    struct bsg_axes axes;
    if (!bsg_view_view_axes_get(gdvp, &axes))
	return BRLCAD_ERROR;
    int ret = to_axes(gedp, gdvp, &axes, argc, argv, usage);
    if (ret == BRLCAD_OK)
	bsg_view_view_axes_set(gdvp, &axes);
    return ret;
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
