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

/* Oof.  Followed the logic down the chain to_poly_circ_mode_func ->
 * to_data_polygons_func -> to_extract_contours_av to get what are hopefully
 * all the pieces needed to seed a circle at an XY point. */
struct bview_scene_obj *
bview_create_circle(struct bview *v, int x, int y)
{
    struct bview_scene_obj *s;
    BU_GET(s, struct bview_scene_obj);
    s->s_v = v;

    struct bview_polygon *p;
    BU_GET(p, struct bview_polygon);
    p->type = BVIEW_POLYGON_CIRCLE;
    s->s_i_data = (void *)p;
    s->s_update_callback = &bview_update_polygon;

    // Let the view know these are now the previous x,y points
    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;

    // If snapping is active, handle it
    fastf_t fx, fy;
    bview_screen_to_view(v, &fx, &fy, x, y);
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
bview_update_circle(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;

    fastf_t curr_fx, curr_fy;
    fastf_t fx, fy;
    fastf_t r, arc;
    int nsegs, n;
    point_t v_pt;
    vect_t vdiff;

    struct bview *v = s->s_v;
    bview_screen_to_view(v, &fx, &fy, v->gv_mouse_x, v->gv_mouse_y);

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

    /* Not doing a struct copy to avoid overwriting the color, line width and line style. */
    p->polygon.num_contours = gp.num_contours;
    p->polygon.hole = gp.hole;
    p->polygon.contour = gp.contour;

    /* Updated */
    s->s_changed = 1;
    return 1;
}

int
bview_update_polygon(struct bview_scene_obj *s)
{
    struct bview_polygon *p = (struct bview_polygon *)s->s_i_data;
    if (p->type == BVIEW_POLYGON_CIRCLE)
	return bview_update_circle(s);

    return 0;
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
