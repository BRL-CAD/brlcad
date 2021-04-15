/*                    P O L Y G O N S . C
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
 * Utility functions for working with libbview polygons
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bn/vlist.h"
#include "bg/polygon.h"
#include "bview/defines.h"
#include "bview/util.h"
#include "bview/polygons.h"

void
bview_polygon_contour(struct bview_scene_obj *s, struct bg_poly_contour *c, int close)
{
    if (!s || !c || !s->s_v || c->num_points < 2)
	return;

    BN_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[0], BN_VLIST_LINE_MOVE);
    for (size_t i = 0; i < c->num_points; i++) {
	BN_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[i], BN_VLIST_LINE_DRAW);
    }
    if (close)
	BN_ADD_VLIST(&s->s_v->gv_vlfree, &s->s_vlist, c->point[0], BN_VLIST_LINE_DRAW);
}

void
bview_polygon_vlist(struct bview_scene_obj *s)
{
    if (!s)
	return;

    // free old s->s_vlist
    BN_FREE_VLIST(&s->s_v->gv_vlfree, &s->s_vlist);
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bview_scene_obj *s_c = (struct bview_scene_obj *)BU_PTBL_GET(&s->children, i);
	BN_FREE_VLIST(&s->s_v->gv_vlfree, &s_c->s_vlist);
	// TODO - free bview_scene_obj itself (ptbls, etc.)
    }


    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    BU_LIST_INIT(&(s->s_vlist));

    for (size_t i = 0; i < p->polygon.num_contours; ++i) {
	/* Draw holes using segmented lines.  Since vlists don't have a style
	 * command for that, we make child scene objects for the holes. */
	if (p->polygon.hole[i]) {
	    struct bview_scene_obj *s_c;
	    BU_GET(s_c, struct bview_scene_obj);
	    BU_LIST_INIT(&(s_c->s_vlist));
	    s_c->s_soldash = 1;
	    s_c->s_color[0] = s->s_color[0];
	    s_c->s_color[1] = s->s_color[1];
	    s_c->s_color[2] = s->s_color[2];
	    bview_polygon_contour(s_c, &p->polygon.contour[i], 1);
	    bu_ptbl_ins(&s->children, (long *)s_c);
	    continue;
	}

	bview_polygon_contour(s, &p->polygon.contour[i], !p->aflag);
    }
}

struct bview_scene_obj *
bview_create_polygon(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    s->s_v = v;
    BU_LIST_INIT(&(s->s_vlist));

    struct bview_polygon *p;
    BU_GET(p, struct bview_polygon);
    p->type = BVIEW_POLYGON_GENERAL;
    s->s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bview_update_polygon;

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    if (bview_screen_to_view(v, &fx, &fy, x, y) < 0) {
	BU_PUT(p, struct bview_polygon);
	BU_PUT(s, struct bview_scene_obj);
	return NULL;
    }
    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(p->prev_point, v_pt);

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = 1;
    p->polygon.contour[0].point = (point_t *)bu_calloc(1, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    VMOVE(p->polygon.contour[0].point[0], m_pt);

    return s;
}

/* Oof.  Followed the logic down the chain to_poly_circ_mode_func ->
 * to_data_polygons_func -> to_extract_contours_av to get what are hopefully
 * all the pieces needed to seed a circle at an XY point. */
struct bview_scene_obj *
bview_create_polygon_circle(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    s->s_v = v;
    BU_LIST_INIT(&(s->s_vlist));

    struct bview_polygon *p;
    BU_GET(p, struct bview_polygon);
    p->type = BVIEW_POLYGON_CIRCLE;
    s->s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bview_update_polygon;

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    if (bview_screen_to_view(v, &fx, &fy, x, y) < 0) {
	BU_PUT(p, struct bview_polygon);
	BU_PUT(s, struct bview_scene_obj);
	return NULL;
    }
    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(p->prev_point, v_pt);

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = 3;
    p->polygon.contour[0].point = (point_t *)bu_calloc(3, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    for (int j = 0; j < 3; j++) {
	VMOVE(p->polygon.contour[0].point[j], m_pt);
    }

    return s;
}

int
bview_update_polygon_circle(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    fastf_t curr_fx, curr_fy;
    fastf_t fx, fy;
    fastf_t r, arc;
    int nsegs, n;
    point_t v_pt;
    vect_t vdiff;

    struct bview *v = s->s_v;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    VSET(v_pt, fx, fy, v->gv_data_vZ);
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
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t ang = n * arc;

	curr_fx = cos(ang*DEG2RAD) * r + p->prev_point[X];
	curr_fy = sin(ang*DEG2RAD) * r + p->prev_point[Y];
	VSET(v_pt, curr_fx, curr_fy, v->gv_data_vZ);
	MAT4X3PNT(gpp->contour[0].point[n], v->gv_view2model, v_pt);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Have new polygon, now update view object vlist */
    bview_polygon_vlist(s);

    /* Updated */
    s->s_changed++;
    return 1;
}

struct bview_scene_obj *
bview_create_polygon_ellipse(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    s->s_v = v;
    BU_LIST_INIT(&(s->s_vlist));

    struct bview_polygon *p;
    BU_GET(p, struct bview_polygon);
    p->type = BVIEW_POLYGON_ELLIPSE;
    s->s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bview_update_polygon;

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(p->prev_point, v_pt);

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = 4;
    p->polygon.contour[0].point = (point_t *)bu_calloc(4, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    for (int j = 0; j < 4; j++) {
	VMOVE(p->polygon.contour[0].point[j], m_pt);
    }

    return s;
}

int
bview_update_polygon_ellipse(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
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

    VSET(A, a, 0, v->gv_data_vZ);
    VSET(B, 0, b, v->gv_data_vZ);

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
    gpp->contour[0].point = (point_t *)bu_calloc(nsegs, sizeof(point_t), "point");

    arc = 360.0 / nsegs;
    for (n = 0; n < nsegs; ++n) {
	fastf_t cosa = cos(n * arc * DEG2RAD);
	fastf_t sina = sin(n * arc * DEG2RAD);

	VJOIN2(ellout, p->prev_point, cosa, A, sina, B);
	MAT4X3PNT(gpp->contour[0].point[n], v->gv_view2model, ellout);
    }

    bg_polygon_free(&p->polygon);

    /* Not doing a struct copy to avoid overwriting other properties. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Have new polygon, now update view object vlist */
    bview_polygon_vlist(s);

    /* Updated */
    s->s_changed++;
    return 1;
}

struct bview_scene_obj *
bview_create_polygon_rectangle(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    s->s_v = v;
    BU_LIST_INIT(&(s->s_vlist));

    struct bview_polygon *p;
    BU_GET(p, struct bview_polygon);
    p->type = BVIEW_POLYGON_RECTANGLE;
    s->s_line_width = 1;
    s->s_color[0] = 255;
    s->s_color[1] = 255;
    s->s_color[2] = 0;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bview_update_polygon;

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt, m_pt;
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(p->prev_point, v_pt);

    p->polygon.num_contours = 1;
    p->polygon.hole = (int *)bu_calloc(1, sizeof(int), "hole");
    p->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    p->polygon.contour[0].num_points = 4;
    p->polygon.contour[0].point = (point_t *)bu_calloc(4, sizeof(point_t), "point");
    p->polygon.hole[0] = 0;
    for (int j = 0; j < 4; j++) {
	VMOVE(p->polygon.contour[0].point[j], m_pt);
    }

    return s;
}

int
bview_update_polygon_rectangle(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
    }

    point_t v_pt;
    MAT4X3PNT(p->polygon.contour[0].point[0], v->gv_view2model, p->prev_point);
    VSET(v_pt, p->prev_point[X], fy, v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[1], v->gv_view2model, v_pt);
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[2], v->gv_view2model, v_pt);
    VSET(v_pt, fx, p->prev_point[Y], v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[3], v->gv_view2model, v_pt);

    /* Polygon updated, now update view object vlist */
    bview_polygon_vlist(s);

    /* Updated */
    s->s_changed++;
    return 1;
}

struct bview_scene_obj *
bview_create_polygon_square(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s = bview_create_polygon_rectangle(v, x, y);
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    p->type = BVIEW_POLYGON_SQUARE;
    return s;
}

int
bview_update_polygon_square(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    fastf_t fx, fy;

    struct bview *v = s->s_v;
    if (bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y) < 0)
	return 0;

    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = bview_snap_lines_2d(v, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	bview_snap_grid_2d(v, &fx, &fy);
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

    point_t v_pt;
    MAT4X3PNT(p->polygon.contour[0].point[0], v->gv_view2model, p->prev_point);
    VSET(v_pt, p->prev_point[X], fy, v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[1], v->gv_view2model, v_pt);
    VSET(v_pt, fx, fy, v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[2], v->gv_view2model, v_pt);
    VSET(v_pt, fx, p->prev_point[Y], v->gv_data_vZ);
    MAT4X3PNT(p->polygon.contour[0].point[3], v->gv_view2model, v_pt);

    /* Polygon updated, now update view object vlist */
    bview_polygon_vlist(s);

    /* Updated */
    s->s_changed++;
    return 1;
}

int
bview_update_general_polygon(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type != BVIEW_POLYGON_GENERAL)
	return 0;

    return 0;
}

int
bview_update_polygon(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type == BVIEW_POLYGON_CIRCLE)
	return bview_update_polygon_circle(s);
    if (p->type == BVIEW_POLYGON_ELLIPSE)
	return bview_update_polygon_ellipse(s);
    if (p->type == BVIEW_POLYGON_RECTANGLE)
	return bview_update_polygon_rectangle(s);
    if (p->type == BVIEW_POLYGON_SQUARE)
	return bview_update_polygon_square(s);
    if (p->type != BVIEW_POLYGON_GENERAL)
	return 0;
    return bview_update_general_polygon(s);
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
