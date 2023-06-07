/*                         S N A P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
    point_t cp;  // closest point on closest line
    double dsq;  // squared distance to closest line
    point_t cp2;  // closest point on closest line
    double dsq2; // squared distance to 2nd closest line
};
#define BV_CP_INFO_INIT {BN_TOL_DIST, VINIT_ZERO, DBL_MAX, VINIT_ZERO, DBL_MAX}

struct bv_cp_info_tcl {
    struct bv_cp_info c;
    struct bv_data_line_state *c_lset; // container holding closest line
    int c_l;     // index of closest line

    struct bv_data_line_state *c_lset2; // container holding 2nd closest line
    int c_l2;   // index of 2nd closest line
};
#define BV_CP_INFO_TCL_INIT {BV_CP_INFO_INIT, NULL, -1, NULL, -1}

static int
_find_closest_tcl_point(struct bv_cp_info_tcl *s, point_t *p, struct bv_data_line_state *lines)
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
	if (dsq > s->c.ctol_sq) {
	    continue;
	}
	// If this is the closest we've seen, record it
	if (s->c.dsq > dsq) {
	    // Closest is now second closest
	    VMOVE(s->c.cp2, s->c.cp);
	    s->c.dsq2 = s->c.dsq;
	    s->c_l2 = s->c_l;
	    s->c_lset2 = s->c_lset;

	    // set new closest
	    VMOVE(s->c.cp, c);
	    s->c.dsq = dsq;
	    s->c_l = i;
	    s->c_lset = lines;
	    ret = 1;
	    continue;
	}
	// Not the closest - is it closer than the second closest?
	if (s->c.dsq2 > dsq) {
	    VMOVE(s->c.cp2, c);
	    s->c.dsq2 = dsq;
	    s->c_l2 = i;
	    s->c_lset2 = lines;
	    ret = 2;
	    continue;
	}
    }

    return ret;
}

static void
_find_close_isect(struct bv_cp_info *s, point_t *p, point_t *P0, point_t *P1, point_t *Q0, point_t *Q1)
{
    point_t c1, c2;

    if (!s || !p)
	return;

    double csdist_sq = bg_distsq_lseg3_lseg3(&c1, &c2, *P0, *P1, *Q0, *Q1);
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

static void
_find_close_isect_tcl(struct bv_cp_info_tcl *s, point_t *p)
{
    point_t P0, P1, Q0, Q1;

    if (!s || !s->c_lset || !p)
	return;

    VMOVE(P0, s->c_lset->gdls_points[s->c_l]);
    VMOVE(P1, s->c_lset->gdls_points[s->c_l+1]);

    VMOVE(Q0, s->c_lset2->gdls_points[s->c_l2]);
    VMOVE(Q1, s->c_lset2->gdls_points[s->c_l2+1]);

    _find_close_isect(&s->c, p, &P0, &P1, &Q0, &Q1);
}

static int
_find_closest_obj_point(struct bv_cp_info *s, point_t *p, struct bv_scene_obj *o)
{
    int ret = 0;
    if (!s || !p || !o)
	return 0;
    if (!bu_list_len(&o->s_vlist))
	return 0;

    struct bv_vlist *tvp;
    for (BU_LIST_FOR(tvp, bv_vlist, &o->s_vlist)) {
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	point_t *pt1 = NULL;
	point_t *pt2 = NULL;
	for (int i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		    pt2 = pt;
		    break;
		case BV_VLIST_LINE_DRAW:
		    pt1 = pt2;
		    pt2 = pt;
		    break;
	    }
	    if (pt1 && pt2) {
		point_t c;
		double dsq = bg_distsq_lseg3_pt(&c, *pt1, *pt2, *p);
		// If we're outside tolerance, continue
		if (dsq > s->ctol_sq) {
		    continue;
		}
		// If this is the closest we've seen, record it
		if (s->dsq > dsq) {
		    // Closest is now second closest
		    VMOVE(s->cp2, s->cp);
		    s->dsq2 = s->dsq;

		    // set new closest
		    VMOVE(s->cp, c);
		    s->dsq = dsq;
		    ret = 1;
		    continue;
		}
		// Not the closest - is it closer than the second closest?
		if (s->dsq2 > dsq) {
		    VMOVE(s->cp2, c);
		    s->dsq2 = dsq;
		    ret = 2;
		    continue;
		}
	    }
	}
    }

    return ret;
}

static double
line_tol_sq(struct bview *v, int lwidth)
{
    if (!v || lwidth <= 0)
	return 100*100;

    // NOTE - make sure calling applications update these values from
    // the display manager info before command execution.
    int width = v->gv_width;
    int height = v->gv_height;

    if (!width || !height)
	return 100*100;

    double lavg = ((double)width + (double)height) * 0.5;
    double lratio = ((double)lwidth)/lavg;

    struct bview_settings *gv_s = (v->gv_s) ? v->gv_s : &v->gv_ls;
    double lrsize = v->gv_size * lratio * gv_s->gv_snap_tol_factor;

    return lrsize*lrsize;
}

int
bv_snap_lines_3d(point_t *out_pt, struct bview *v, point_t *p)
{
    int ret = 0;
    struct bview_settings *gv_s = (v->gv_s) ? v->gv_s : &v->gv_ls;
    struct bv_cp_info_tcl cpinfo = BV_CP_INFO_TCL_INIT;

    if (!p || !v) return 0;

    // If we're not in Tcl mode only, we are looking at objects - either
    // all of them, or a specified subset
    if (gv_s->gv_snap_flags != BV_SNAP_TCL) {
	struct bv_cp_info *s = &cpinfo.c;
	s->ctol_sq = line_tol_sq(v, 1);
	if (BU_PTBL_LEN(&gv_s->gv_snap_objs) > 0) {
	    for (size_t i = 0; i < BU_PTBL_LEN(&gv_s->gv_snap_objs); i++) {
		struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(&gv_s->gv_snap_objs, i);
		if (gv_s->gv_snap_flags) {
		    if (gv_s->gv_snap_flags == BV_SNAP_DB && (!(so->s_type_flags & BV_DB_OBJS)))
			continue;
		    if (gv_s->gv_snap_flags == BV_SNAP_VIEW && (!(so->s_type_flags & BV_VIEW_OBJS)))
			continue;
		}
		struct bv_obj_settings *s_os = (so->s_os) ? so->s_os : &so->s_local_os;
		s->ctol_sq = line_tol_sq(v, (s_os->s_line_width) ? s_os->s_line_width : 1);
		ret += _find_closest_obj_point(s, p, so);
	    }
	} else {
	    if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_DB)) {
		if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_SHARED)) {
		    struct bu_ptbl *sobjs = bv_view_objs(v, BV_DB_OBJS);
		    for (size_t i = 0; i < BU_PTBL_LEN(sobjs); i++) {
			struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(sobjs, i);
			ret += _find_closest_obj_point(s, p, so);
		    }
		}
		if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_LOCAL)) {
		    struct bu_ptbl *sobjs = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
		    for (size_t i = 0; i < BU_PTBL_LEN(sobjs); i++) {
			struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(sobjs, i);
			ret += _find_closest_obj_point(s, p, so);
		    }
		}
	    }
	    if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_VIEW)) {
		if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_SHARED)) {
		    struct bu_ptbl *sobjs = bv_view_objs(v, BV_VIEW_OBJS);
		    for (size_t i = 0; i < BU_PTBL_LEN(sobjs); i++) {
			struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(sobjs, i);
			ret += _find_closest_obj_point(s, p, so);
		    }
		}
		if (!gv_s->gv_snap_flags || (gv_s->gv_snap_flags & BV_SNAP_LOCAL)) {
		    struct bu_ptbl *sobjs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
		    for (size_t i = 0; i < BU_PTBL_LEN(sobjs); i++) {
			struct bv_scene_obj *so = (struct bv_scene_obj *)BU_PTBL_GET(sobjs, i);
			ret += _find_closest_obj_point(s, p, so);
		    }
		}
	    }
	}
    }

    // There are some issues with line snapping that don't come up with grid
    // snapping - in particular, when are we "close enough" to a line to snap,
    // and how do we handle snapping when close enough to multiple lines?  We
    // probably want to prefer intersections between lines to closest line
    // point if we are close to multiple lines...
    if (!gv_s->gv_snap_flags || gv_s->gv_snap_flags & BV_SNAP_TCL) {
	int tret = 0;
	int lwidth;
	lwidth = (v->gv_tcl.gv_data_lines.gdls_line_width) ? v->gv_tcl.gv_data_lines.gdls_line_width : 1;
	cpinfo.c.ctol_sq = line_tol_sq(v, lwidth);
	tret += _find_closest_tcl_point(&cpinfo, p, &v->gv_tcl.gv_data_lines);
	lwidth = (v->gv_tcl.gv_sdata_lines.gdls_line_width) ? v->gv_tcl.gv_sdata_lines.gdls_line_width : 1;
	cpinfo.c.ctol_sq = line_tol_sq(v, lwidth);
	tret += _find_closest_tcl_point(&cpinfo, p, &v->gv_tcl.gv_sdata_lines);

	// Check if we are close enough to two line segments to warrant using the
	// closest approach point.  The intersection may not be close enough to
	// use, but if it is prefer it as it satisfies two lines instead of one.
	//
	// TODO - as implemented this will only prefer the intersection between
	// two Tcl lines, rather than all lines...
	if (tret > 1) {
	    _find_close_isect_tcl(&cpinfo, p);
	}
	ret += tret;
    }

    // If we found something, we can snap
    if (ret) {
	VMOVE(*out_pt, cpinfo.c.cp);
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
    if (bv_snap_lines_3d(&out_pt, v, &p) == 1) {
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

    struct bview_settings *gv_s = (v->gv_s) ? v->gv_s : &v->gv_ls;

    if (ZERO(gv_s->gv_grid.res_h) ||
	ZERO(gv_s->gv_grid.res_v))
	return 0;

    sf = v->gv_base2local;
    inv_sf = 1 / sf;

    VSET(view_pt, *vx, *vy, 0.0);
    VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

    MAT4X3PNT(view_grid_anchor, v->gv_model2view, gv_s->gv_grid.anchor);
    VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

    grid_units_h = (view_grid_anchor[X] - view_pt[X]) / (gv_s->gv_grid.res_h * v->gv_base2local);
    grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / (gv_s->gv_grid.res_v * v->gv_base2local);
    nh = grid_units_h;
    nv = grid_units_v;

    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */

    if (grid_units_h <= -0.5)
	*vx = view_grid_anchor[X] - ((nh - 1) * gv_s->gv_grid.res_h * v->gv_base2local);
    else if (0.5 <= grid_units_h)
	*vx = view_grid_anchor[X] - ((nh + 1) * gv_s->gv_grid.res_h * v->gv_base2local);
    else
	*vx = view_grid_anchor[X] - (nh * gv_s->gv_grid.res_h * v->gv_base2local);

    if (grid_units_v <= -0.5)
	*vy = view_grid_anchor[Y] - ((nv - 1) * gv_s->gv_grid.res_v * v->gv_base2local);
    else if (0.5 <= grid_units_v)
	*vy = view_grid_anchor[Y] - ((nv + 1) * gv_s->gv_grid.res_v * v->gv_base2local);
    else
	*vy = view_grid_anchor[Y] - (nv * gv_s->gv_grid.res_v * v->gv_base2local);

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
