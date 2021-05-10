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
/** @file libbv/snap.c
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
#include "bv.h"

struct bv_cp_info {
    double ctol_sq; // square of the distance that defines "close to a line"

    struct bv_data_line_state *c_lset; // container holding closest line
    point_t cp;  // closest point on closest line
    int c_l;     // index of closest line
    double dsq;  // squared distance to closest line

    struct bv_data_line_state *c_lset2; // container holding 2nd closest line
    point_t cp2;  // closest point on closest line
    int c_l2;   // index of 2nd closest line
    double dsq2; // squared distance to 2nd closest line
};
#define BV_CP_INFO_INIT {BN_TOL_DIST, NULL, VINIT_ZERO, -1, DBL_MAX, NULL, VINIT_ZERO, -1, DBL_MAX}

static
int
_find_closest_point(struct bv_cp_info *s, point_t *p, struct bv_data_line_state *lines)
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
_find_close_isect(struct bv_cp_info *s, point_t *p)
{
    point_t P0, P1, Q0, Q1;
    point_t c1, c2;

    if (!s || !s->c_lset || !p)
	return;

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
line_tol_sq(struct bview *v, struct bv_data_line_state *gdlsp)
{
    if (!v || !gdlsp)
	return 100*100;

    // NOTE - make sure calling applications update these values from
    // the display manager info before command execution.
    int width = v->gv_width;
    int height = v->gv_height;

    if (!width || !height)
	return 100*100;

    double lavg = ((double)width + (double)height) * 0.5;
    double lwidth = (gdlsp->gdls_line_width) ? gdlsp->gdls_line_width : 1;
    double lratio = lwidth/lavg;
    double lrsize = v->gv_size * lratio * v->gv_snap_tol_factor;
    return lrsize*lrsize;
}

int
bv_snap_lines_3d(point_t *out_pt, struct bview *v, point_t *p)
{
    struct bv_cp_info cpinfo = BV_CP_INFO_INIT;

    if (!p || !v) return BRLCAD_ERROR;

    // There are some issues with line snapping that don't come up with grid
    // snapping - in particular, when are we "close enough" to a line to snap,
    // and how do we handle snapping when close enough to multiple lines?  We
    // probably want to prefer intersections between lines to closest line
    // point if we are close to multiple lines...
    int ret = 0;
    cpinfo.ctol_sq = line_tol_sq(v, &v->gv_tcl.gv_data_lines);
    ret += _find_closest_point(&cpinfo, p, &v->gv_tcl.gv_data_lines);
    cpinfo.ctol_sq = line_tol_sq(v, &v->gv_tcl.gv_sdata_lines);
    ret += _find_closest_point(&cpinfo, p, &v->gv_tcl.gv_sdata_lines);

    // Check if we are close enough to two line segments to warrant using the
    // closest approach point.  The intersection may not be close enough to
    // use, but if it is prefer it as it satisfies two lines instead of one.
    if (ret > 1) {
	_find_close_isect(&cpinfo, p);
    }

    // If we found something, we can snap
    if (ret) {
	VMOVE(*out_pt, cpinfo.cp);
	return 1;
    }

    return 0;
}

int
bv_snap_lines_2d(struct bview *v, fastf_t *vx, fastf_t *vy)
{
    if (!v || !vx || !vy) return 0;

    point2d_t p2d = {0.0, 0.0};
    V2SET(p2d, *vx, *vy);
    point_t vp = VINIT_ZERO;
    VSET(vp, p2d[0], p2d[1], 0);
    point_t p = VINIT_ZERO;
    MAT4X3PNT(p, v->gv_view2model, vp);
    point_t out_pt = VINIT_ZERO;
    if (bv_snap_lines_3d(&out_pt, v, &p) == BRLCAD_OK) {
	MAT4X3PNT(vp, v->gv_model2view, out_pt);
	(*vx) = vp[0];
	(*vy) = vp[1];
	return 1;
    }

    return 0;
}

void
bv_view_center_linesnap(struct bview *v)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, v->gv_center);
    MAT4X3PNT(view_pt, v->gv_model2view, model_pt);
    bv_snap_lines_2d(v, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, v->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(v->gv_center, model_pt);
    bv_update(v);
}

int
bv_snap_grid_2d(struct bview *v, fastf_t *vx, fastf_t *vy)
{
    int nh, nv;		/* whole grid units */
    point_t view_pt;
    point_t view_grid_anchor;
    fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
    fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
    fastf_t sf;
    fastf_t inv_sf;

    if (!v || !vx || !vy)
	return 0;

    if (ZERO(v->gv_grid.res_h) ||
	ZERO(v->gv_grid.res_v))
	return 0;

    sf = v->gv_base2local;
    inv_sf = 1 / sf;

    VSET(view_pt, *vx, *vy, 0.0);
    VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

    MAT4X3PNT(view_grid_anchor, v->gv_model2view, v->gv_grid.anchor);
    VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

    grid_units_h = (view_grid_anchor[X] - view_pt[X]) / (v->gv_grid.res_h * v->gv_base2local);
    grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / (v->gv_grid.res_v * v->gv_base2local);
    nh = grid_units_h;
    nv = grid_units_v;

    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */

    if (grid_units_h <= -0.5)
	*vx = view_grid_anchor[X] - ((nh - 1) * v->gv_grid.res_h * v->gv_base2local);
    else if (0.5 <= grid_units_h)
	*vx = view_grid_anchor[X] - ((nh + 1) * v->gv_grid.res_h * v->gv_base2local);
    else
	*vx = view_grid_anchor[X] - (nh * v->gv_grid.res_h * v->gv_base2local);

    if (grid_units_v <= -0.5)
	*vy = view_grid_anchor[Y] - ((nv - 1) * v->gv_grid.res_v * v->gv_base2local);
    else if (0.5 <= grid_units_v)
	*vy = view_grid_anchor[Y] - ((nv + 1) * v->gv_grid.res_v * v->gv_base2local);
    else
	*vy = view_grid_anchor[Y] - (nv * v->gv_grid.res_v * v->gv_base2local);

    *vx *= inv_sf;
    *vy *= inv_sf;

    return 1;
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
