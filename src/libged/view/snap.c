/*                         S N A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/snap.c
 *
 * Logic for snapping points to visual elements.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/vls.h"
#include "bn/plane.h"
#include "../ged_private.h"

int
ged_snap_lines_2d(struct ged *gedp, point2d_t *p)
{
    if (!p || !gedp) return GED_ERROR;
    return GED_ERROR;
}

int
ged_snap_lines_3d(struct ged *gedp, point_t *p)
{
    if (!p || !gedp) return GED_ERROR;
    return GED_ERROR;
}

int
ged_view_snap(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[options] x y [z]";
    int in_dim = 0;
    int print_help = 0;
    int use_grid = 0;
    int use_lines = 0;
    int use_object_keypoints = 0;
    int use_object_lines = 0;
    point2d_t view_pt_2d = {0.0, 0.0};
    point_t view_pt = VINIT_ZERO;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[6];
    BU_OPT(d[0], "h", "help",      "",  NULL,  &print_help,           "Print help and exit");
    BU_OPT(d[1], "g", "grid",      "",  NULL,  &use_grid,             "Snap to the view grid");
    BU_OPT(d[2], "l", "lines",     "",  NULL,  &use_lines,            "Snap to the view lines");
    BU_OPT(d[3], "o", "obj-keypt", "",  NULL,  &use_object_keypoints, "Snap to drawn object keypoints");
    BU_OPT(d[4], "w", "obj-lines", "",  NULL,  &use_object_lines,     "Snap to drawn object lines");
    BU_OPT_NULL(d[5]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    /* must be wanting help */
    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;
    if (argc != 2 && argc != 3) {
        _ged_cmd_help(gedp, usage, d);
        return GED_ERROR;
    }

    /* We may get a 2D screen point or a 3D model space point.  Either
     * should work - whatever we get, set up both points so we have
     * the necessary inputs for any of the options. */
    if (argc == 2) {
	point2d_t p2d = {0.0, 0.0};
	point_t p = VINIT_ZERO;
	point_t vp = VINIT_ZERO;
	if (bu_opt_fastf_t(&msg, 1, (const char **)&argv[0], (void *)&p2d[X]) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "problem reading X value %s: %s\n", argv[0], bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return GED_ERROR;
	}
	if (bu_opt_fastf_t(&msg, 1, (const char **)&argv[1], (void *)&p2d[Y]) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "problem reading Y value %s: %s\n", argv[1], bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return GED_ERROR;
	}
	V2MOVE(view_pt_2d, p2d);
	VSET(vp, p[0], p[1], 0);
	MAT4X3PNT(p, gedp->ged_gvp->gv_view2model, vp);
	VMOVE(view_pt, p);
	in_dim = 2;
    }
    /* We may get a 3D point instead */
    if (argc == 3) {
	point_t p = VINIT_ZERO;
	point_t vp = VINIT_ZERO;
	if (bu_opt_vect_t(&msg, argc, argv, (void *)&p) <= 0) {
	    bu_vls_sprintf(gedp->ged_result_str, "%s", bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	    return GED_ERROR;
	}
	MAT4X3PNT(vp, gedp->ged_gvp->gv_model2view, p);
	V2SET(view_pt_2d, vp[0], vp[1]);
	VMOVE(view_pt, p);
	in_dim = 3;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);


    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (use_grid) {
	// Grid operates on view space points
	ged_snap_to_grid(gedp, &view_pt_2d[X], &view_pt_2d[Y]);
    }

    if (use_lines) {
	if (in_dim == 2) {
	    ged_snap_lines_2d(gedp, &view_pt_2d);
	}

	if (in_dim == 3) {
	    ged_snap_lines_3d(gedp, &view_pt);
	}
    }

    bu_vls_printf(gedp->ged_result_str, "%g %g %g", V3ARGS(view_pt));

    bu_vls_free(&msg);
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
