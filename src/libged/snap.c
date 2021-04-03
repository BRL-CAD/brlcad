/*                         S N A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
#include <errno.h>

#include "bu/opt.h"
#include "bu/vls.h"
#include "bg/lseg.h"
#include "dm.h"
#include "./ged_private.h"

struct ged_cp_info {
    double ctol_sq; // square of the distance that defines "close to a line"

    struct bview_data_line_state *c_lset; // container holding closest line
    point_t cp;  // closest point on closest line
    int c_l;     // index of closest line
    double dsq;  // squared distance to closest line

    struct bview_data_line_state *c_lset2; // container holding 2nd closest line
    point_t cp2;  // closest point on closest line
    int c_l2;   // index of 2nd closest line
    double dsq2; // squared distance to 2nd closest line
};
#define GED_CP_INFO_INIT {BN_TOL_DIST, NULL, VINIT_ZERO, -1, DBL_MAX, NULL, VINIT_ZERO, -1, DBL_MAX}

static
int
_find_closest_point(struct ged_cp_info *s, point_t *p, struct bview_data_line_state *lines)
{
    int ret = 0;
    point_t P0, P1;

    if (lines->gdls_num_points < 2) {
	return ret;
    }

    // TODO - if we have a large number of lines drawn, we could really benefit
    // from an acceleration structure such as the RTree to localize these tests
    // rather than checking everything...
    for (int i = 0; i < lines->gdls_num_points; i+=2) {
	if (s->c_l == i && s->c_lset == lines) {
	    continue;
	}
	point_t c;
	VMOVE(P0, lines->gdls_points[i]);
	VMOVE(P1, lines->gdls_points[i+1]);
	double dsq = bg_distsq_lseg3_pt(&c, P0, P1, *p);
	// If we're outside tolerance, continue
	if (dsq > s->ctol_sq) {
	    continue;
	}
	// If this is the closest we've seen, record it
	if (s->dsq > dsq) {
	    // Closest is now second closest
	    VMOVE(s->cp2, s->cp);
	    s->dsq2 = s->dsq;
	    s->c_l2 = s->c_l;
	    s->c_lset2 = s->c_lset;

	    // set new closest
	    VMOVE(s->cp, c);
	    s->dsq = dsq;
	    s->c_l = i;
	    s->c_lset = lines;
	    ret = 1;
	    continue;
	}
	// Not the closest - is it closer than the second closest?
	if (s->dsq2 > dsq) {
	    VMOVE(s->cp2, c);
	    s->dsq2 = dsq;
	    s->c_l2 = i;
	    s->c_lset2 = lines;
	    ret = 2;
	    continue;
	}
    }

    return ret;
}

void
_find_close_isect(struct ged_cp_info *s, point_t *p)
{
    point_t P0, P1, Q0, Q1;
    point_t c1, c2;

    VMOVE(P0, s->c_lset->gdls_points[s->c_l]);
    VMOVE(P1, s->c_lset->gdls_points[s->c_l+1]);

    VMOVE(Q0, s->c_lset2->gdls_points[s->c_l2]);
    VMOVE(Q1, s->c_lset2->gdls_points[s->c_l2+1]);

    double csdist_sq = bg_distsq_lseg3_lseg3(&c1, &c2, P0, P1, Q0, Q1);
    if (csdist_sq > s->ctol_sq) {
	// Line segments are too far away to use both of them to override the
	// original answer
	return;
    }

    // If either closest segment point is too far from the test point, go with
    // the original answer rather than changing it
    double d1_sq = DIST_PNT_PNT_SQ(*p, c1);
    if (d1_sq > s->ctol_sq) {
	// Too far away to work
	return;
    }

    double d2_sq = DIST_PNT_PNT_SQ(*p, c2);
     if (d2_sq > s->ctol_sq) {
	// Too far away to work
	return;
    }

    // Go with the closest segment point to the original point.  If
    // the segments intersect the two points should be the same and
    // it won't matter which is chosen, but if they don't then the
    // intuitive behavior is to prefer the closest point that attempts
    // to satisfy both line segments
    if (d1_sq < d2_sq) {
	VMOVE(s->cp, c1);
    } else {
	VMOVE(s->cp, c2);
    }
}

static double
line_tol_sq(struct ged *gedp, struct bview_data_line_state *gdlsp)
{
    if (!gedp->ged_gvp)
	return 100*100;

    // TODO - make sure calling applications update these values from
    // the display manager info before command execution...
    int width = gedp->ged_gvp->gv_width;
    int height = gedp->ged_gvp->gv_height;

    if (!width || !height)
	return 100*100;

    double lavg = ((double)width + (double)height) * 0.5;
    double lwidth = (gdlsp->gdls_line_width) ? gdlsp->gdls_line_width : 1;
    double lratio = lwidth/lavg;
    double lrsize = gedp->ged_gvp->gv_size * lratio * gedp->ged_gvp->gv_snap_tol_factor;
    return lrsize*lrsize;
}

int
ged_snap_lines(point_t *out_pt, struct ged *gedp, point_t *p)
{
    struct ged_cp_info cpinfo = GED_CP_INFO_INIT;

    if (!p || !gedp) return GED_ERROR;

    // There are some issues with line snapping that don't come up with grid
    // snapping - in particular, when are we "close enough" to a line to snap,
    // and how do we handle snapping when close enough to multiple lines?  We
    // probably want to prefer intersections between lines to closest line
    // point if we are close to multiple lines...
    int ret = 0;
    cpinfo.ctol_sq = line_tol_sq(gedp, &gedp->ged_gvp->gv_data_lines);
    ret += _find_closest_point(&cpinfo, p, &gedp->ged_gvp->gv_data_lines);
    cpinfo.ctol_sq = line_tol_sq(gedp, &gedp->ged_gvp->gv_sdata_lines);
    ret += _find_closest_point(&cpinfo, p, &gedp->ged_gvp->gv_sdata_lines);

    // Check if we are close enough to two line segments to warrant using the
    // closest approach point.  The intersection may not be close enough to
    // use, but if it is prefer it as it satisfies two lines instead of one.
    if (ret > 1) {
	_find_close_isect(&cpinfo, p);
    }

    // If we found something, we can snap
    if (ret) {
	VMOVE(*out_pt, cpinfo.cp);
	return GED_OK;
    }

    return GED_ERROR;
}

int
ged_snap_to_lines(struct ged *gedp, fastf_t *vx, fastf_t *vy)
{
    if (!gedp || !vx || !vy) return 0;
    if (!gedp->ged_gvp) return 0;

    point2d_t p2d = {0.0, 0.0};
    point_t p = VINIT_ZERO;
    point_t vp = VINIT_ZERO;
    point_t out_pt = VINIT_ZERO;

    V2SET(p2d, *vx, *vy);
    VSET(vp, p2d[0], p2d[1], 0);
    MAT4X3PNT(p, gedp->ged_gvp->gv_view2model, vp);
    if (ged_snap_lines(&out_pt, gedp, &p) == GED_OK) {
	MAT4X3PNT(vp, gedp->ged_gvp->gv_model2view, out_pt);
	(*vx) = vp[0];
	(*vy) = vp[1];
	return 1;
    }

    return 0;
}

// TODO - this is another function that belongs in libdm...
void
ged_view_center_linesnap(struct ged *gedp)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, gedp->ged_gvp->gv_center);
    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    ged_snap_to_lines(gedp, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, model_pt);
    bview_update(gedp->ged_gvp);
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
