/*                      D A T A _ L I N E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/data_lines.c
 *
 * Logic for drawing arbitrary lines not associated with geometry.
 *
 * Phase T1 (drawing_stack_modernization): BSG-first rewrite.  The BSG
 * VIEW_SCOPE object (_tcl_data_lines / _tcl_sdata_lines) is the sole persistent
 * store for both Tcl and non-Tcl views.  gv_tcl is no longer mirrored because
 * commands.c to_data_pick_func and to_data_move_func now read from BSG directly.
 *
 * Usage example (Archer / QGED):
 *
 * Archer> view sdata_lines points {{0 -1000 0} {0 1000 0} {100 -1000 0} {100 1000 0} {-1000 10 0} {1000 10 0}}
 * Archer> view sdata_lines draw 1
 * Archer> view sdata_lines line_width 100
 * Archer> view sdata_lines color 255 0 0
 *
 * Note that gedp->ged_gvp must be set to the correct display manager before
 * calling this command to put the output in the correct display manager.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/field.h"
#include "bsg/draw_source.h"
#include "bsg/scene_object.h"
#include "bsg/hud.h"
#include "bsg/overlay.h"
#include "../ged_private.h"
#include "./ged_view.h"

struct view_dlines_state {
    struct ged *gedp;
    const char *bsg_name;
};

/* Phase T1 (drawing_stack_modernization): BSG-first helper – rebuild the BSG
 * VIEW_SCOPE line object from an explicit point array.
 *
 * Pass draw=1 to create/replace the object; draw=0 to only remove it. */
static void
_rebuild_bsg_dlines(struct bsg_view *v, const char *bsg_name,
		    int draw, point_t *pts, int npts,
		    int *color, int line_width)
{
    if (!v || !bsg_name)
	return;

    bsg_feature_remove(v, bsg_name);

    if (!draw || npts < 2 || !pts)
	return;

    bsg_feature_ref ref = bsg_feature_create_lines(v, bsg_name, 1 /* local */);
    if (bsg_feature_ref_is_null(ref))
	return;
    bsg_feature_overlay_register_owner(ref, NULL,
	    BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_TCL_OVERLAY,
	    BSG_OVERLAY_LC_PER_COMMAND,
	    BSG_OVERLAY_ORDER_POST_TRANSPARENT,
	    NULL, 0);

    int *cmds = (int *)bu_calloc(npts, sizeof(int), "data line cmds");
    for (int i = 0; i + 1 < npts; i += 2) {
	cmds[i] = BSG_GEOMETRY_LINE_MOVE;
	cmds[i+1] = BSG_GEOMETRY_LINE_DRAW;
    }
    bsg_feature_points_replace(ref, BSG_FEATURE_LINES, (const point_t *)pts, cmds, (size_t)npts);
    bu_free(cmds, "data line cmds");

    if (color)
	bsg_feature_set_color(ref, color[0], color[1], color[2]);
    bsg_feature_set_line_width(ref, line_width);
    bsg_feature_set_visible(ref, 1);
}

static int
_view_dlines_cmd_draw(void *bs, int argc, const char **argv)
{
    struct view_dlines_state *vs = (struct view_dlines_state *)bs;
    struct ged *gedp = vs->gedp;
    struct bsg_view *v = gedp->ged_gvp;

    if (argc == 1) {
	struct bsg_feature_record rec;
	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	bu_vls_printf(gedp->ged_result_str, "%d",
		(!bsg_feature_ref_is_null(ref) && bsg_feature_record_get(ref, &rec) && rec.visible) ? 1 : 0);
	return BRLCAD_OK;
    }

    if (argc == 2) {
	int i;

	if (bu_sscanf(argv[1], "%d", &i) != 1) return BRLCAD_ERROR;

	/* BSG is the sole owner; just remove or hide the object. */
	if (!i)
	    bsg_feature_remove(v, vs->bsg_name);
	/* draw=1 is a no-op here; use "points" to create/re-enable. */

	ged_refresh_cb(gedp);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
_view_dlines_cmd_snap(void *bs, int argc, const char **argv)
{
    struct view_dlines_state *vs = (struct view_dlines_state *)bs;
    struct ged *gedp = vs->gedp;
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%d", bsg_view_snap_lines(gedp->ged_gvp));
	return BRLCAD_OK;
    }

    if (argc == 2) {
	int i;

	if (bu_sscanf(argv[1], "%d", &i) != 1) return BRLCAD_ERROR;

	bsg_view_set_snap_lines(gedp->ged_gvp, i);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
_view_dlines_cmd_color(void *bs, int argc, const char **argv)
{
    struct view_dlines_state *vs = (struct view_dlines_state *)bs;
    struct ged *gedp = vs->gedp;
    struct bsg_view *v = gedp->ged_gvp;

    if (argc == 1) {
	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	struct bsg_feature_record rec;
	if (!bsg_feature_ref_is_null(ref) && bsg_feature_record_get(ref, &rec)) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  (int)rec.color[0], (int)rec.color[1], (int)rec.color[2]);
	} else
	    bu_vls_printf(gedp->ged_result_str, "0 0 0");
	return BRLCAD_OK;
    }

    if (argc == 4) {
	int r, g, b;

	/* set background color */
	if (bu_sscanf(argv[1], "%d", &r) != 1 ||
		bu_sscanf(argv[2], "%d", &g) != 1 ||
		bu_sscanf(argv[3], "%d", &b) != 1)
	    return BRLCAD_ERROR;

	/* validate color */
	if (r < 0 || 255 < r ||
		g < 0 || 255 < g ||
		b < 0 || 255 < b)
	    return BRLCAD_ERROR;

	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	if (!bsg_feature_ref_is_null(ref))
	    bsg_feature_set_color(ref, r, g, b);

	ged_refresh_cb(gedp);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
_view_dlines_cmd_line_width(void *bs, int argc, const char **argv)
{
    struct view_dlines_state *vs = (struct view_dlines_state *)bs;
    struct ged *gedp = vs->gedp;
    struct bsg_view *v = gedp->ged_gvp;

    if (argc == 1) {
	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	struct bsg_feature_record rec;
	if (!bsg_feature_ref_is_null(ref) && bsg_feature_record_get(ref, &rec))
	    bu_vls_printf(gedp->ged_result_str, "%d", rec.line_width);
	else
	    bu_vls_printf(gedp->ged_result_str, "0");
	return BRLCAD_OK;
    }

    if (argc == 2) {
	int line_width;

	if (bu_sscanf(argv[1], "%d", &line_width) != 1)
	    return BRLCAD_ERROR;

	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	if (!bsg_feature_ref_is_null(ref))
	    bsg_feature_set_line_width(ref, line_width);

	ged_refresh_cb(gedp);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

static int
_view_dlines_cmd_points(void *bs, int argc, const char **argv)
{
    struct view_dlines_state *vs = (struct view_dlines_state *)bs;
    struct ged *gedp = vs->gedp;
    struct bsg_view *v = gedp->ged_gvp;
    int i;

    if (argc == 1) {
	bsg_feature_ref ref = bsg_feature_find(v, vs->bsg_name);
	point_t *points = NULL;
	size_t point_count = 0;
	if (!bsg_feature_ref_is_null(ref) &&
		bsg_feature_points_copy(ref, &points, NULL, &point_count)) {
	    for (size_t j = 0; j < point_count; j++)
		bu_vls_printf(gedp->ged_result_str, " {%lf %lf %lf} ", V3ARGS(points[j]));
	    if (points)
		bu_free(points, "bsg feature points copy");
	}
	return BRLCAD_OK;
    }

    if (argc == 2) {
	int ac;
	const char **av;

	if (bu_argv_from_tcl_list(argv[1], &ac, &av)) {
	    bu_vls_printf(gedp->ged_result_str, "failed to parse list");
	    return BRLCAD_ERROR;
	}

	if (ac % 2) {
	    bu_vls_printf(gedp->ged_result_str, "%s: must be an even number of points", argv[0]);
	    bu_free((char *)av, "av");
	    return BRLCAD_ERROR;
	}

	/* BSG is the sole persistent store; preserve style from existing object. */
	int saved_color[3] = {255, 255, 0}; /* default yellow */
	int saved_lw = 0;
	bsg_feature_ref old_ref = bsg_feature_find(v, vs->bsg_name);
	struct bsg_feature_record rec;
	if (!bsg_feature_ref_is_null(old_ref) && bsg_feature_record_get(old_ref, &rec)) {
	    saved_color[0] = (int)rec.color[0];
	    saved_color[1] = (int)rec.color[1];
	    saved_color[2] = (int)rec.color[2];
	    saved_lw = rec.line_width;
	}

	if (ac < 2) {
	    bsg_feature_remove(v, vs->bsg_name);
	    ged_refresh_cb(gedp);
	    bu_free((char *)av, "av");
	    return BRLCAD_OK;
	}

	point_t *pts = (point_t *)bu_calloc(ac, sizeof(point_t), "data points");
	for (i = 0; i < ac; ++i) {
	    double scan[3];

	    if (bu_sscanf(av[i], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
		bu_vls_printf(gedp->ged_result_str, "bad data point - %s\n", av[i]);
		bu_free((void *)pts, "data points");
		ged_refresh_cb(gedp);
		bu_free((char *)av, "av");
		return BRLCAD_ERROR;
	    }
	    VMOVE(pts[i], scan);
	}

	_rebuild_bsg_dlines(v, vs->bsg_name, 1 /* draw=1 on explicit set */,
			   pts, ac, saved_color, saved_lw);
	bu_free((void *)pts, "data points");

	ged_refresh_cb(gedp);
	bu_free((char *)av, "av");
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}

const struct bu_cmdtab _view_dline_cmds[] = {
    { "draw",       _view_dlines_cmd_draw},
    { "color",      _view_dlines_cmd_color},
    { "line_width", _view_dlines_cmd_line_width},
    { "points",     _view_dlines_cmd_points},
    { "snap",       _view_dlines_cmd_snap},
    { (char *)NULL,      NULL}
};

int
ged_view_data_lines(struct ged *gedp, int argc, const char *argv[])
{
    struct view_dlines_state vs;
    const char *usage = "view [s]data_lines [subcommand]";

    vs.gedp = gedp;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (6 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argv[0][0] == 's') {
	vs.bsg_name = "_tcl_sdata_lines";
    } else {
	vs.bsg_name = "_tcl_data_lines";
    }

    argc--;argv++;

    if (bu_cmd_valid(_view_dline_cmds, argv[0]) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "invalid subcommand: %s", argv[1]);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_view_dline_cmds, argc, argv, 0, (void *)&vs, &ret) == BRLCAD_OK) {
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
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
