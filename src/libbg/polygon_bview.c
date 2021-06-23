/*                P O L Y G O N _ B V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
 * Utility functions for working with polygons in a bv context.
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
#include "bg/polygon.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
    if (BU_LIST_IS_EMPTY(fp)) { \
	BU_ALLOC((p), struct bv_scene_obj); \
    } else { \
	p = BU_LIST_NEXT(bv_scene_obj, fp); \
	BU_LIST_DEQUEUE(&((p)->l)); \
    } \
    BU_LIST_INIT( &((p)->s_vlist) ); }

#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

void
bv_polygon_contour(struct bv_scene_obj *s, struct bg_poly_contour *c, int curr_c, int curr_i, int do_pnt)
{
    if (!s || !c || !s->s_v)
	return;

    if (do_pnt) {
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[0], BV_VLIST_POINT_DRAW);
	return;
    }

    BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[0], BV_VLIST_LINE_MOVE);
    for (size_t i = 0; i < c->num_points; i++) {
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[i], BV_VLIST_LINE_DRAW);
    }
    if (!c->open)
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[0], BV_VLIST_LINE_DRAW);

    if (curr_c && curr_i >= 0) {
	point_t psize;
	VSET(psize, 10, 0, 0);
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[curr_i], BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, psize, BV_VLIST_POINT_SIZE);
	BV_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[curr_i], BV_VLIST_POINT_DRAW);
    }
}

void
bv_fill_polygon(struct bv_scene_obj *s)
{
    if (!s)
	return;

    // free old fill, if present
    struct bv_scene_obj *fobj = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s_c->s_uuid), "fill")) {
	    fobj = s_c;
	    BV_FREE_VLIST(&s->s_v->gv_vlfree, &s_c->s_vlist);
	    break;
	}
    }

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    if (p->polygon.contour[0].open) {
	return;
    }
    if (p->fill_delta < BN_TOL_DIST) {
	return;
    }

    struct bg_polygon *fill = bg_polygon_fill_segments(&p->polygon, p->fill_dir, p->fill_delta);
    if (!fill)
	return;

    // Got fill, create lines
    if (!fobj) {
	GET_BV_SCENE_OBJ(fobj, &s->free_scene_obj->l);
	fobj->free_scene_obj = s->free_scene_obj;
	BU_LIST_INIT(&(fobj->s_vlist));
	bu_vls_init(&fobj->s_uuid);
	bu_vls_sprintf(&fobj->s_uuid, "fill");
	fobj->s_os.s_line_width = 1;
	fobj->s_soldash = 0;
	fobj->s_v = s->s_v;
	bu_ptbl_ins(&s->children, (long *)fobj);
    }
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

    // free old s->s_vlist
    BV_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	BV_FREE_VLIST(&s->s_v->gv_vlfree, &s_c->s_vlist);
	// TODO - free bv_scene_obj itself (ptbls, etc.)
    }


    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    int type = p->type;

    BU_LIST_INIT(&(s->s_vlist));

    for (size_t i = 0; i < p->polygon.num_contours; ++i) {
	/* Draw holes using segmented lines.  Since vlists don't have a style
	 * command for that, we make child scene objects for the holes. */
	int pcnt = p->polygon.contour[i].num_points;
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
	    struct bv_scene_obj *s_c;
	    GET_BV_SCENE_OBJ(s_c, &s->free_scene_obj->l);
	    s_c->free_scene_obj = s->free_scene_obj;
	    BU_LIST_INIT(&(s_c->s_vlist));
	    BU_VLS_INIT(&(s_c->s_uuid));
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
    }

}

struct bv_scene_obj *
bv_create_polygon(struct bview *v, int type, int x, int y, struct bv_scene_obj *free_scene_obj)
{
    struct bv_scene_obj *s;
    GET_BV_SCENE_OBJ(s, &free_scene_obj->l);
    s->free_scene_obj = free_scene_obj;
    s->s_v = v;
    s->s_type_flags |= BV_VIEWONLY;
    s->s_type_flags |= BV_POLYGONS;
    BU_LIST_INIT(&(s->s_vlist));
    BU_PTBL_INIT(&s->children);

    struct bv_polygon *p;
    BU_GET(p, struct bv_polygon);
    p->type = type;
    p->curr_contour_i = -1;
    p->curr_point_i = -1;

    // Save the current view for later processing
    bv_sync(&p->v, s->s_v);

    s->s_os.s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bv_update_polygon;

    // Set default fill color to blue
    unsigned char frgb[3] = {0, 0, 255};
    bu_color_from_rgb_chars(&p->fill_color, frgb);

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    if (bv_screen_to_view(v, &fx, &fy, x, y) < 0) {
	BU_PUT(p, struct bv_polygon);
	BU_PUT(s, struct bv_scene_obj);
	return NULL;
    }
    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(p->prev_point, v_pt);

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

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* updated */
    s->s_changed++;

    return s;
}

int
bv_append_polygon_pt(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    if (p->curr_contour_i < 0)
	return -1;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt;
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    c->num_points++;
    c->point = (point_t *)bu_realloc(c->point,c->num_points * sizeof(point_t), "realloc contour points");
    // Use the polygon's view context for actually adding the point
    MAT4X3PNT(c->point[c->num_points-1], p->v.gv_view2model, v_pt);

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

// NOTE: This is a naive brute force search for the closest edge at the
// moment...  Would be better for repeated sampling of relatively static
// scenes to build an RTree first...
struct bv_scene_obj *
bv_select_polygon(struct bu_ptbl *objs, struct bview *v)
{
    if (!objs)
	return NULL;

    fastf_t fx, fy;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);

    struct bv_scene_obj *closest = NULL;
    double dist_min_sq = DBL_MAX;

    for (size_t i = 0; i < BU_PTBL_LEN(objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(objs, i);
	if (s->s_type_flags & BV_POLYGONS) {
	    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
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
bv_select_polygon_pt(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);


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

int
bv_move_polygon(struct bv_scene_obj *s)
{
    fastf_t pfx, pfy, fx, fy;
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &pfx, &pfy, v->gv_prevMouseX, v->gv_prevMouseY) < 0)
	return 0;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;
    fastf_t dx = fx - pfx;
    fastf_t dy = fy - pfy;

    point_t v_pt, m_pt;
    VSET(v_pt, dx, dy, v->gv_s->gv_data_vZ);
    // Use the polygon's view context for actually moving the point
    MAT4X3PNT(m_pt, p->v.gv_view2model, v_pt);

    for (size_t j = 0; j < p->polygon.num_contours; j++) {
	struct bg_poly_contour *c = &p->polygon.contour[j];
	for (size_t i = 0; i < c->num_points; i++) {
	    VADD2(c->point[i], c->point[i], m_pt);
	}
    }

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    // Shift the previous point so updates will start from the
    // correct point.
    p->prev_point[0] = p->prev_point[0]+dx;
    p->prev_point[1] = p->prev_point[1]+dy;

    /* Updated */
    s->s_changed++;

    return 0;
}

int
bv_move_polygon_pt(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return -1;

    // Need to have a point selected before we can move
    if (p->curr_point_i < 0 || p->curr_contour_i < 0)
	return -1;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    // Use the polygon's view context for actually moving the point
    MAT4X3PNT(m_pt, p->v.gv_view2model, v_pt);

    struct bg_poly_contour *c = &p->polygon.contour[p->curr_contour_i];
    VMOVE(c->point[p->curr_point_i], m_pt);

    /* Have new polygon, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

/* Oof.  Followed the logic down the chain to_poly_circ_mode_func ->
 * to_data_polygons_func -> to_extract_contours_av to get what are hopefully
 * all the pieces needed to seed a circle at an XY point. */

int
bv_update_polygon_circle(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t curr_fx, curr_fy;
    fastf_t fx, fy;
    fastf_t r, arc;
    int nsegs, n;
    point_t v_pt;
    vect_t vdiff;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    VSUB2(vdiff, v_pt, p->prev_point);
    r = MAGNITUDE(vdiff);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.  select a chord length that
     * results in segments approximately 4 pixels in length.
     *
     * circumference / 4 = PI * diameter / 4
     */
    nsegs = M_PI_2 * r * v->gv_scale;

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

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t ang = n * arc;

	curr_fx = cos(ang*DEG2RAD) * r + p->prev_point[X];
	curr_fy = sin(ang*DEG2RAD) * r + p->prev_point[Y];
	VSET(v_pt, curr_fx, curr_fy, v->gv_s->gv_data_vZ);
	// Use the polygon's view context for actually adding the points
	MAT4X3PNT(gpp->contour[0].point[n], p->v.gv_view2model, v_pt);
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
bv_update_polygon_ellipse(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }


    fastf_t a, b, arc;
    point_t ellout;
    point_t A, B;
    int nsegs, n;

    a = fx - p->prev_point[X];
    b = fy - p->prev_point[Y];

    /*
     * For angle alpha, compute surface point as
     *
     * V + cos(alpha) * A + sin(alpha) * B
     *
     * note that sin(alpha) is cos(90-alpha).
     */

    VSET(A, a, 0, v->gv_s->gv_data_vZ);
    VSET(B, 0, b, v->gv_s->gv_data_vZ);

    /* use a variable number of segments based on the size of the
     * circle being created so small circles have few segments and
     * large ones are nice and smooth.  select a chord length that
     * results in segments approximately 4 pixels in length.
     *
     * circumference / 4 = PI * diameter / 4
     *
     */
    nsegs = M_PI_2 * FMAX(a, b) * v->gv_scale;

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

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t cosa = cos(n * arc * DEG2RAD);
	fastf_t sina = sin(n * arc * DEG2RAD);

	VJOIN2(ellout, p->prev_point, cosa, A, sina, B);
	// Use the polygon's view context for actually adding the points
	MAT4X3PNT(gpp->contour[0].point[n], p->v.gv_view2model, ellout);
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
bv_update_polygon_rectangle(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    // Use the polygon's view context for actually adjusting the points
    point_t v_pt;
    MAT4X3PNT(p->polygon.contour[0].point[0], p->v.gv_view2model, p->prev_point);
    VSET(v_pt, p->prev_point[X], fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[1], p->v.gv_view2model, v_pt);
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[2], p->v.gv_view2model, v_pt);
    VSET(v_pt, fx, p->prev_point[Y], v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[3], p->v.gv_view2model, v_pt);
    p->polygon.contour[0].open = 0;

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_polygon_square(struct bv_scene_obj *s)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    fastf_t fx, fy;

    // Use the polygon's view context
    struct bview *v = s->s_v;
    if (bv_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_s->gv_snap_lines) {
	snapped = bv_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_s->gv_grid.snap) {
	bv_snap_grid_2d(v, &fx, &fy);
    }

    fastf_t dx = fx - p->prev_point[X];
    fastf_t dy = fy - p->prev_point[Y];

    if (fabs(dx) > fabs(dy)) {
	if (dy < 0.0)
	    fy = p->prev_point[Y] - fabs(dx);
	else
	    fy = p->prev_point[Y] + fabs(dx);
    } else {
	if (dx < 0.0)
	    fx = p->prev_point[X] - fabs(dy);
	else
	    fx = p->prev_point[X] + fabs(dy);
    }

    // Use the polygon's view context for actually adjusting the points
    point_t v_pt;
    MAT4X3PNT(p->polygon.contour[0].point[0], p->v.gv_view2model, p->prev_point);
    VSET(v_pt, p->prev_point[X], fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[1], p->v.gv_view2model, v_pt);
    VSET(v_pt, fx, fy, v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[2], p->v.gv_view2model, v_pt);
    VSET(v_pt, fx, p->prev_point[Y], v->gv_s->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[3], p->v.gv_view2model, v_pt);
    p->polygon.contour[0].open = 0;

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 1;
}

int
bv_update_general_polygon(struct bv_scene_obj *s, int utype)
{
    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;
    if (p->type != BV_POLYGON_GENERAL)
	return 0;

    if (utype == BV_POLYGON_UPDATE_PT_APPEND) {
	return bv_append_polygon_pt(s);
    }

    if (utype == BV_POLYGON_UPDATE_PT_SELECT) {
	return bv_select_polygon_pt(s);
    }

    if (utype == BV_POLYGON_UPDATE_PT_MOVE) {
	return bv_move_polygon_pt(s);
    }

    /* Polygon updated, now update view object vlist */
    bv_polygon_vlist(s);

    /* Updated */
    s->s_changed++;

    return 0;
}

int
bv_update_polygon(struct bv_scene_obj *s, int utype)
{
    if (!s)
	return 0;

    struct bv_polygon *p = (struct bv_polygon *)s->s_i_data;

    // Regardless of type, sync fill color
    struct bv_scene_obj *fobj = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&s_c->s_uuid), "fill")) {
	    fobj = s_c;
	    break;
	}
    }
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
	    // Clear old fill
	    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
		struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
		if (!s_c)
		    continue;
		if (BU_STR_EQUAL(bu_vls_cstr(&s_c->s_uuid), "fill")) {
		    BV_FREE_VLIST(&s->s_v->gv_vlfree, &s_c->s_vlist);
		    break;
		}
	    }
	}

	return 0;
    }

    if (p->type == BV_POLYGON_CIRCLE)
	return bv_update_polygon_circle(s);
    if (p->type == BV_POLYGON_ELLIPSE)
	return bv_update_polygon_ellipse(s);
    if (p->type == BV_POLYGON_RECTANGLE)
	return bv_update_polygon_rectangle(s);
    if (p->type == BV_POLYGON_SQUARE)
	return bv_update_polygon_square(s);
    if (p->type != BV_POLYGON_GENERAL)
	return 0;
    return bv_update_general_polygon(s, utype);
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
