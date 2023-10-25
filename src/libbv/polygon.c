/*                     P O L Y G O N  . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2023 United States Government as represented by
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
/** @file polygons.c
 *
 * Utility functions for working with polygons in a view context.
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bn/tol.h"
#include "bv/vlist.h"
#include "bv/defines.h"
#include "bv/util.h"
#include "bg/lseg.h"
#include "bg/plane.h"
#include "bg/polygon.h"
#include "bv/defines.h"
#include "bv/polygon.h"
#include "bv/snap.h"

void
bv_polygon_contour(struct bv_scene_obj *s, struct bg_poly_contour *c, int curr_c, int curr_i, int do_pnt)
{
    if (!s || !c || !s->s_v)
	return;

    if (do_pnt) {
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[0], BV_VLIST_POINT_DRAW);
	return;
    }

    BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[0], BV_VLIST_LINE_MOVE);
    for (size_t i = 0; i < c->num_points; i++) {
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[i], BV_VLIST_LINE_DRAW);
    }
    if (!c->open)
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[0], BV_VLIST_LINE_DRAW);

    if (curr_c && curr_i >= 0) {
	point_t psize;
	VSET(psize, 10, 0, 0);
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[curr_i], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, psize, BV_VLIST_POINT_SIZE);
	BV_ADD_VLIST(s->vlfree, &s->s_vlist, c->point[curr_i], BV_VLIST_POINT_DRAW);
    }
}

void
bv_fill_polygon(struct bv_scene_obj *s)
{
    if (!s)
	return;

    // free old fill, if present
    struct bv_scene_obj *fobj = bv_find_child(s, "*fill*");
    if (fobj)
	bv_obj_put(fobj);

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (!p || !p->polygon.num_contours)
	return;

    if (!p->polygon.contour || p->polygon.contour[0].open)
	return;

    if (p->fill_delta < BN_TOL_DIST)
	return;

    struct bg_polygon *fill = bv_polygon_fill_segments(&p->polygon, &p->vp, p->fill_dir, p->fill_delta);
    if (!fill)
	return;

    // Got fill, create lines
    fobj = bv_obj_get_child(s);
    bu_vls_printf(&fobj->s_name, ":fill");
    fobj->s_os->s_line_width = 1;
    fobj->s_soldash = 0;
    bu_color_to_rgb_chars(&p->fill_color, fobj->s_color);
    for (size_t i = 0; i < fill->num_contours; i++) {
	bv_polygon_contour(fobj, &fill->contour[i], 0, -1, 0);
    }
}

void
bv_polygon_vlist(struct bv_scene_obj *s)
{
    if (!s)
	return;

    // Reset obj drawing data
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	BV_FREE_VLIST(s->vlfree, &s->s_vlist);
    }
    BU_LIST_INIT(&(s->s_vlist));

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    int type = p->type;

    // Clear any old holes
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	bv_obj_put(s_c);
    }

    for (size_t i = 0; i < p->polygon.num_contours; ++i) {
	/* Draw holes using segmented lines.  Since vlists don't have a style
	 * command for that, we make child scene objects for the holes. */
	size_t pcnt = p->polygon.contour[i].num_points;
	int do_pnt = 0;
	if (pcnt == 1)
	    do_pnt = 1;
	if (type == BV_POLYGON_CIRCLE && pcnt == 3)
	    do_pnt = 1;
	if (type == BV_POLYGON_ELLIPSE && pcnt == 4)
	    do_pnt = 1;
	if (type == BV_POLYGON_RECTANGLE) {
	    if (NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[1]), SMALL_FASTF) &&
		    NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[2]), SMALL_FASTF))
		do_pnt = 1;
	}
	if (type == BV_POLYGON_SQUARE) {
	    if (NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[1]), SMALL_FASTF) &&
		    NEAR_ZERO(DIST_PNT_PNT_SQ(p->polygon.contour[0].point[0], p->polygon.contour[0].point[2]), SMALL_FASTF))
		do_pnt = 1;
	}

	if (p->polygon.hole[i]) {
	    struct bv_scene_obj *s_c = bv_obj_get_child(s);
	    s_c->s_soldash = 1;
	    s_c->s_color[0] = s->s_color[0];
	    s_c->s_color[1] = s->s_color[1];
	    s_c->s_color[2] = s->s_color[2];
	    s_c->s_v = s->s_v;
	    bv_polygon_contour(s_c, &p->polygon.contour[i], ((int)i == p->curr_contour_i), p->curr_point_i, do_pnt);
	    bu_ptbl_ins(&s->children, (long *)s_c);
	    continue;
	}

	bv_polygon_contour(s, &p->polygon.contour[i], ((int)i == p->curr_contour_i), p->curr_point_i, do_pnt);
    }

    if (p->fill_flag) {
	bv_fill_polygon(s);
    } else {
	struct bv_scene_obj *fobj = bv_find_child(s, "*fill*");
	if (fobj)
	    bv_obj_put(fobj);

    }
}

struct bv_scene_obj *
bv_create_polygon_obj(struct bview *v, int flags, struct bv_polygon *p)
{
    struct bv_scene_obj *s = bv_obj_get(v, flags);
    s->s_type_flags |= BV_POLYGONS;
    s->s_type_flags |= BV_VIEWONLY;

    // Construct the plane
    bv_view_plane(&p->vp, v);

    s->s_os->s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bv_update_polygon;

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* updated */
    s->s_changed++;

    return s;
}

struct bv_scene_obj *
bv_create_polygon(struct bview *v, int flags, int type, point_t fp)
{

    struct bv_polygon *p;
    BU_GET(p, struct bv_polygon);
    p->type = type;
    p->curr_contour_i = -1;
    p->curr_point_i = -1;

    // Set default fill color to blue
    unsigned char frgb[3] = {0, 0, 255};
    bu_color_from_rgb_chars(&p->fill_color, frgb);

    // Construct the plane
    bv_view_plane(&p->vp, v);

    // Construct closest point to fp on plane
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, p->vp, fp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, p->vp, fx, fy);

    // This is now the origin point
    VMOVE(p->origin_point, m_pt);

    int pcnt = 1;
    if (type == BV_POLYGON_CIRCLE)
	pcnt = 3;
    if (type == BV_POLYGON_ELLIPSE)
	pcnt = 4;
    if (type == BV_POLYGON_RECTANGLE)
	pcnt = 4;
    if (type == BV_POLYGON_SQUARE)
	pcnt = 4;

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = pcnt;
    p->polygon.contour[0].open = 0;
    p->polygon.contour[0].point = (point_t *)bu_calloc(pcnt, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    for (int i = 0; i < pcnt; i++) {
	VMOVE(p->polygon.contour[0].point[i], m_pt);
    }

    // Only the general polygon isn't closed out of the gate
    if (type == BV_POLYGON_GENERAL)
	p->polygon.contour[0].open = 1;

    // Have polygon, now make scene object
    struct bv_scene_obj *s = bv_create_polygon_obj(v, flags, p);
    if (!s)
	BU_PUT(p, struct bv_polygon);
    return s;
}

void
bv_polygon_cpy(struct bv_polygon *dest, struct bv_polygon *src)
{
    if (!src || !dest)
	return;

    dest->type = src->type;
    dest->fill_flag = src->fill_flag;
    V2MOVE(dest->fill_dir, src->fill_dir);
    dest->fill_delta = src->fill_delta;
    BU_COLOR_CPY(&dest->fill_color, &src->fill_color);
    dest->curr_contour_i = src->curr_contour_i;
    dest->curr_point_i = src->curr_point_i;
    VMOVE(dest->origin_point, src->origin_point);
    HMOVE(dest->vp, src->vp);
    dest->vZ = src->vZ;
    bg_polygon_free(&dest->polygon);
    bg_polygon_cpy(&dest->polygon, &src->polygon);
    dest->u_data = src->u_data;
}

int
bv_append_polygon_pt(struct bv_scene_obj *s, point_t np)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    if (p->curr_contour_i < 0)
	return -1;

    // Construct closest point to np on plane
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, p->vp, np);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, p->vp, fx, fy);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    c->num_points++;
    c->point = (point_t *)bu_realloc(c->point,c->num_points * sizeof(point_t), "realloc contour points");
    VMOVE(c->point[c->num_points-1], m_pt);

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

// NOTE: This is a naive brute force search for the closest projected edge at
// the moment...  Would be better for repeated sampling of relatively static
// scenes to build an RTree first...
struct bv_scene_obj *
bv_select_polygon(struct bu_ptbl *objs, point_t cp)
{
    if (!objs)
	return NULL;

    struct bv_scene_obj *closest = NULL;
    double dist_min_sq = DBL_MAX;

    for (size_t i = 0; i < BU_PTBL_LEN(objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(objs, i);
	if (s->s_type_flags & BV_POLYGONS) {
	    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
	    // Because we're working in 2D orthogonal when processing polygons,
	    // the specific value of Z for each individual polygon isn't
	    // relevant - we want to find the closest edge in the projected
	    // view plane.  Accordingly, always construct the test point using
	    // whatever the current vZ is for the polygon being tested.
	    plane_t zpln;
	    HMOVE(zpln, p->vp);
	    zpln[3] += p->vZ;
	    fastf_t fx, fy;
	    bg_plane_closest_pt(&fx, &fy, zpln, cp);
	    point_t m_pt;
	    bg_plane_pt_at(&m_pt, zpln, fx, fy);

	    for (size_t j = 0; j < p->polygon.num_contours; j++) {
		struct bg_poly_contour *c = &p->polygon.contour[j];
		for (size_t k = 0; k < c->num_points; k++) {
		    double dcand;
		    if (k < c->num_points - 1) {
			dcand = bg_distsq_lseg3_pt(NULL, c->point[k], c->point[k+1], m_pt);
		    } else {
			dcand = bg_distsq_lseg3_pt(NULL, c->point[k], c->point[0], m_pt);
		    }
		    if (dcand < dist_min_sq) {
			dist_min_sq = dcand;
			closest = s;
		    }
		}
	    }
	}
    }

    return closest;
}

int
bv_select_polygon_pt(struct bv_scene_obj *s, point_t cp)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    fastf_t fx, fy;
    bg_plane_closest_pt(&fx, &fy, zpln, cp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, zpln, fx, fy);

    // If a contour is selected, restrict our closest point candidates to
    // that contour's points
    double dist_min_sq = DBL_MAX;
    long closest_ind = -1;
    long closest_contour = -1;
    if (p->curr_contour_i >= 0) {
	struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
	closest_contour = p->curr_contour_i;
	for (size_t i = 0; i < c->num_points; i++) {
	    double dcand = DIST_PNT_PNT_SQ(c->point[i], m_pt);
	    if (dcand < dist_min_sq) {
		closest_ind = (long)i;
		dist_min_sq = dcand;
	    }
	}
    } else {
	for (size_t j = 0; j < p->polygon.num_contours; j++) {
	    struct bg_poly_contour *c = &p->polygon.contour[j];
	    for (size_t i = 0; i < c->num_points; i++) {
		double dcand = DIST_PNT_PNT_SQ(c->point[i], m_pt);
		if (dcand < dist_min_sq) {
		    closest_ind = (long)i;
		    closest_contour = (long)j;
		    dist_min_sq = dcand;
		}
	    }
	}
    }

    p->curr_point_i = closest_ind;
    p->curr_contour_i = closest_contour;

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}


void
bv_select_clear_polygon_pt(struct bv_scene_obj *s)
{
    if (!s)
	return;

    if (s->s_type_flags & BV_POLYGONS) {
	struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
	p->curr_point_i = -1;
	p->curr_contour_i = -1;
	bv_polygon_vlist(s);
	/* Updated */
	s->s_changed++;
    }
}


int
bv_move_polygon(struct bv_scene_obj *s, point_t cp, point_t prev_point)
{
    fastf_t pfx, pfy, fx, fy;
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, zpln, prev_point);
    bg_plane_closest_pt(&fx, &fy, zpln, cp);
    point_t pm_pt, m_pt;
    bg_plane_pt_at(&pm_pt, p->vp, pfx, pfy);
    bg_plane_pt_at(&m_pt, p->vp, fx, fy);
    vect_t v_mv;
    VSUB2(v_mv, m_pt, pm_pt);

    for (size_t j = 0; j < p->polygon.num_contours; j++) {
	struct bg_poly_contour *c = &p->polygon.contour[j];
	for (size_t i = 0; i < c->num_points; i++) {
	    VADD2(c->point[i], c->point[i], v_mv);
	}
    }

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    // Shift the origin point.
    VADD2(p->origin_point, p->origin_point, v_mv);

    /* Updated */
    s->s_changed++;

    return 0;
}

int
bv_move_polygon_pt(struct bv_scene_obj *s, point_t mp)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    // Need to have a point selected before we can move
    if (p->curr_point_i < 0 || p->curr_contour_i < 0)
	return -1;

    fastf_t fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, zpln, mp);
    point_t m_pt;
    bg_plane_pt_at(&m_pt, zpln, fx, fy);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    VMOVE(c->point[p->curr_point_i], m_pt);

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

int
bv_update_polygon_circle(struct bv_scene_obj *s, point_t cp, fastf_t pixel_size)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t curr_fx, curr_fy;
    fastf_t r, arc;
    int nsegs, n;

    r = DIST_PNT_PNT(cp, p->origin_point);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.
     */
    nsegs = M_PI_2 * r / pixel_size;
    if (nsegs < 32)
	nsegs = 32;

    struct bg_polygon gp;
    struct bg_polygon *gpp = &gp;
    gpp->num_contours = 1;
    gpp->hole = (int *)bu_calloc(1, sizeof(int), "hole");;
    gpp->contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    gpp->contour[0].num_points = nsegs;
    gpp->contour[0].open = 0;
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, zpln, cp);
    bg_plane_closest_pt(&pfx, &pfy, zpln, p->origin_point);

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t ang = n * arc;

	curr_fx = cos(ang*DEG2RAD) * r + pfx;
	curr_fy = sin(ang*DEG2RAD) * r + pfy;
	point_t v_pt;
	bg_plane_pt_at(&v_pt, p->vp, curr_fx, curr_fy);
	VMOVE(gpp->contour[0].point[n], v_pt);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_polygon_ellipse(struct bv_scene_obj *s, point_t cp, fastf_t pixel_size)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.  select a chord length that
     * results in segments approximately 4 pixels in length.
     *
     * circumference / 4 = PI * diameter / 4
     *
     */

    fastf_t r = DIST_PNT_PNT(cp, p->origin_point);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.
     */
    int nsegs = M_PI_2 * r / pixel_size;
    if (nsegs < 32)
	nsegs = 32;

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&fx, &fy, zpln, cp);
    bg_plane_closest_pt(&pfx, &pfy, zpln, p->origin_point);

    fastf_t a, b, arc;
    point_t pv_pt;
    point_t ellout;
    point_t A, B;

    VSET(pv_pt, pfx, pfy, 0);
    a = fx - pfx;
    b = fy - pfy;

    /*
     * For angle alpha, compute surface point as
     *
     * V + cos(alpha) * A + sin(alpha) * B
     *
     * note that sin(alpha) is cos(90-alpha).
     */

    VSET(A, a, 0, 0);
    VSET(B, 0, b, 0);

    struct bg_polygon gp;
    struct bg_polygon *gpp = &gp;
    gpp->num_contours = 1;
    gpp->hole = (int *)bu_calloc(1, sizeof(int), "hole");;
    gpp->contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    gpp->contour[0].num_points = nsegs;
    gpp->contour[0].open = 0;
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    arc = 360.0 / nsegs;
    for (int n = 0; n < nsegs; ++n) {
	fastf_t cosa = cos(n * arc * DEG2RAD);
	fastf_t sina = sin(n * arc * DEG2RAD);

	VJOIN2(ellout, pv_pt, cosa, A, sina, B);

	// Use the polygon's plane for actually adding the points
	bg_plane_pt_at(&gpp->contour[0].point[n], zpln, ellout[0], ellout[1]);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_polygon_rectangle(struct bv_scene_obj *s, point_t cp)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, zpln, p->origin_point);
    bg_plane_closest_pt(&fx, &fy, zpln, cp);

    // Use the polygon's plane for actually adjusting the points
    bg_plane_pt_at(&p->polygon.contour[0].point[0], zpln, pfx, pfy);
    bg_plane_pt_at(&p->polygon.contour[0].point[1], zpln, pfx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[2], zpln, fx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[3], zpln, fx, pfy);

    p->polygon.contour[0].open = 0;

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_polygon_square(struct bv_scene_obj *s, point_t cp)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t pfx, pfy, fx, fy;
    plane_t zpln;
    HMOVE(zpln, p->vp);
    zpln[3] += p->vZ;
    bg_plane_closest_pt(&pfx, &pfy, zpln, p->origin_point);
    bg_plane_closest_pt(&fx, &fy, zpln, cp);

    fastf_t dx = fx - pfx;
    fastf_t dy = fy - pfy;

    if (fabs(dx) > fabs(dy)) {
	if (dy < 0.0)
	    fy = pfy - fabs(dx);
	else
	    fy = pfy + fabs(dx);
    } else {
	if (dx < 0.0)
	    fx = pfx - fabs(dy);
	else
	    fx = pfx + fabs(dy);
    }


    // Use the polygon's plane for actually adjusting the points
    bg_plane_pt_at(&p->polygon.contour[0].point[0], zpln, pfx, pfy);
    bg_plane_pt_at(&p->polygon.contour[0].point[1], zpln, pfx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[2], zpln, fx, fy);
    bg_plane_pt_at(&p->polygon.contour[0].point[3], zpln, fx, pfy);

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_general_polygon(struct bv_scene_obj *s, int utype, point_t cp)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return 0;

    if (utype == BV_POLYGON_UPDATE_PT_APPEND) {
	return bv_append_polygon_pt(s, cp);
    }

    if (utype == BV_POLYGON_UPDATE_PT_SELECT) {
	return bv_select_polygon_pt(s, cp);
    }

    if (utype == BV_POLYGON_UPDATE_PT_SELECT_CLEAR) {
	bv_select_clear_polygon_pt(s);
	return 1;
    }

    if (utype == BV_POLYGON_UPDATE_PT_MOVE) {
	return bv_move_polygon_pt(s, cp);
    }

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

int
bv_update_polygon(struct bv_scene_obj *s, struct bview *v, int utype)
{
    if (!s)
	return 0;

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    // Regardless of type, sync fill color
    struct bv_scene_obj *fobj = bv_find_child(s, "*fill*");
    if (fobj) {
	bu_color_to_rgb_chars(&p->fill_color, fobj->s_color);
    }

    if (utype == BV_POLYGON_UPDATE_PROPS_ONLY) {

	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    if (!s_c)
		continue;
	    s_c->s_color[0] = s->s_color[0];
	    s_c->s_color[1] = s->s_color[1];
	    s_c->s_color[2] = s->s_color[2];
	}

	if (p->fill_flag) {
	    bv_fill_polygon(s);
	} else {
	    if (fobj)
		bv_obj_put(fobj);
	}

	return 0;
    }

    /* Need pixel dimension for calculating segment approximations on these
     * shapes - based on view info */
    if (p->type == BV_POLYGON_CIRCLE || p->type == BV_POLYGON_ELLIPSE) {

	// Need the length of the diagonal of a pixel
	vect_t c1 = VINIT_ZERO;
	vect_t c2 = VINIT_ZERO;
	bv_screen_to_view(v, &c1[0], &c1[1], 0, 0);
	bv_screen_to_view(v, &c2[0], &c2[1], 1, 1);
	point_t p1, p2;
	MAT4X3PNT(p1, v->gv_view2model, c1);
	MAT4X3PNT(p2, v->gv_view2model, c2);
	fastf_t d = DIST_PNT_PNT(p1, p2);

	if (p->type == BV_POLYGON_CIRCLE)
	    return bv_update_polygon_circle(s, v->gv_point, d);
	if (p->type == BV_POLYGON_ELLIPSE)
	    return bv_update_polygon_ellipse(s, v->gv_point, d);
    }

    if (p->type == BV_POLYGON_RECTANGLE)
	return bv_update_polygon_rectangle(s, v->gv_point);
    if (p->type == BV_POLYGON_SQUARE)
	return bv_update_polygon_square(s, v->gv_point);
    if (p->type != BV_POLYGON_GENERAL)
	return 0;
    return bv_update_general_polygon(s, utype, v->gv_point);
}

struct bv_scene_obj *
bv_dup_view_polygon(const char *nname, struct bv_scene_obj *s)
{
    if (!nname || !s)
	return NULL;

    struct bv_polygon *ip = (struct bv_polygon *)s->s_i_data;

    struct bv_polygon *p;
    BU_GET(p, struct bv_polygon);
    bv_polygon_cpy(p, ip);

    struct bv_scene_obj *np = bv_create_polygon_obj(s->s_v, s->s_type_flags, p);

    // Have geometry, now copy visual settings
    VMOVE(np->s_color, s->s_color);

    // Update scene obj vlist
    bv_polygon_vlist(np);

    // Set new name
    bu_vls_init(&np->s_name);
    bu_vls_sprintf(&np->s_name, "%s", nname);

    // Return new object
    return np;
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
