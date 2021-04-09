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
#include "bview/defines.h"
#include "bview/util.h"
#include "bview/polygons.h"

/* Oof.  Followed the logic down the chain to_poly_circ_mode_func ->
 * to_data_polygons_func -> to_extract_contours_av to get what are hopefully
 * all the pieces needed to seed a circle at an XY point. */
int
bview_add_circle(struct bview *v, bview_data_polygon_state *ps, int x, int y)
{
    fastf_t fx, fy;
    point_t v_pt, m_pt;

    ps->gdps_scale = v->gv_scale;
    VMOVE(ps->gdps_origin, v->gv_center);
    MAT_COPY(ps->gdps_rotation, v->gv_rotation);
    MAT_COPY(ps->gdps_model2view, v->gv_model2view);
    MAT_COPY(ps->gdps_view2model, v->gv_view2model);

    v->gv_prevMouseX = x;
    v->gv_prevMouseY = y;
    // TODO - setting this because libtclcad does, but wouldn't it be
    // better to have the polygon itself retain the knowledge of what
    // type of shape it is?
    v->gv_mode = BVIEW_POLY_CIRCLE_MODE;

    bview_screen_to_view(v, &fx, &fy, x, y);


#if 0
    // TODO - translate these routines down to bview...
    int snapped = 0;
    if (v->gv_snap_lines) {
	snapped = ged_snap_to_lines(gedp, &fx, &fy);
    }
    if (!snapped && v->gv_grid.snap) {
	ged_snap_to_grid(gedp, &fx, &fy);
    }
#endif

    VSET(v_pt, fx, fy, v->gv_data_vZ);

    MAT4X3PNT(m_pt, v->gv_view2model, v_pt);
    VMOVE(ps->gdps_prev_point, v_pt);

    size_t i = ps->gdps_polygons.num_polygons;
    ++ps->gdps_polygons.num_polygons;
    ps->gdps_polygons.polygon = (struct bg_polygon *)bu_realloc(ps->gdps_polygons.polygon, ps->gdps_polygons.num_polygons * sizeof(struct bg_polygon), "realloc polygon");
    struct bg_polygon *gpp = &ps->gdps_polygons.polygon[i];
    gpp->num_contours = 1;
    gpp->hole = (int *)bu_calloc(1, sizeof(int), "hole");
    gpp->contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "contour");
    gpp->contour[0].num_points = 3;
    gpp->contour[0].point = (point_t *)bu_calloc(3, sizeof(point_t), "point");
    gpp->hole[0] = 0;
    for (int j = 0; j < 3; j++) {
	VMOVE(gpp->contour[0].point[j], m_pt);
    }
    VMOVE(ps->gdps_polygons.polygon[i].gp_color, ps->gdps_color);
    ps->gdps_polygons.polygon[i].gp_line_style = ps->gdps_line_style;
    ps->gdps_polygons.polygon[i].gp_line_width = ps->gdps_line_width;

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
