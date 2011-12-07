/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libdm/grid.c
 *
 * Routines to implement MGED's snap to grid capability.
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "dm.h"

void
dm_draw_grid(struct dm *dmp, struct ged_grid_state *ggsp, struct ged_view *gvp, fastf_t base2local)
{
    int	i, j;
    int	nh, nv;
    int	nv_dots, nh_dots;
    fastf_t	fx, fy;
    fastf_t	sf;
    fastf_t	inv_sf;
    point_t		view_grid_anchor;
    point_t		view_lleft_corner;
    point_t 		view_grid_anchor_local;
    point_t 		view_lleft_corner_local;
    point_t 		view_grid_start_pt_local;
    fastf_t 		inv_grid_res_h;
    fastf_t 		inv_grid_res_v;
    fastf_t 		inv_aspect;

    if (ZERO(ggsp->ggs_res_h) ||
	ZERO(ggsp->ggs_res_v))
	return;

    inv_grid_res_h= 1.0 / (ggsp->ggs_res_h * base2local);
    inv_grid_res_v= 1.0 / (ggsp->ggs_res_v * base2local);

    sf = gvp->gv_scale*base2local;

    /* sanity - don't draw the grid if it would fill the screen */
    {
	fastf_t pixel_size = 2.0 * sf / dmp->dm_width;
	if ( (ggsp->ggs_res_h*base2local) < pixel_size || (ggsp->ggs_res_v*base2local) < pixel_size )
	    return;
    }

    inv_sf = 1.0 / sf;
    inv_aspect = 1.0 / dmp->dm_aspect;

    nv_dots = 2.0 * inv_aspect * sf * inv_grid_res_v + (2 * ggsp->ggs_res_major_v);
    nh_dots = 2.0 * sf * inv_grid_res_h + (2 * ggsp->ggs_res_major_h);
    MAT4X3PNT(view_grid_anchor, gvp->gv_model2view, ggsp->ggs_anchor);
    VSCALE(view_grid_anchor_local, view_grid_anchor, sf);

    VSET(view_lleft_corner, -1.0, -inv_aspect, 0.0);
    VSCALE(view_lleft_corner_local, view_lleft_corner, sf);
    nh = (view_grid_anchor_local[X] - view_lleft_corner_local[X]) * inv_grid_res_h;
    nv = (view_grid_anchor_local[Y] - view_lleft_corner_local[Y]) * inv_grid_res_v;

    {
	int nmh, nmv;

	nmh = nh / ggsp->ggs_res_major_h + 1;
	nmv = nv / ggsp->ggs_res_major_v + 1;
	VSET(view_grid_start_pt_local,
	     view_grid_anchor_local[X] - (nmh * ggsp->ggs_res_h * ggsp->ggs_res_major_h * base2local),
	     view_grid_anchor_local[Y] - (nmv * ggsp->ggs_res_v * ggsp->ggs_res_major_v * base2local),
	     0.0);
    }

    DM_SET_FGCOLOR(dmp,
		   ggsp->ggs_color[0],
		   ggsp->ggs_color[1],
		   ggsp->ggs_color[2], 1, 1.0);
    DM_SET_LINE_ATTR(dmp, 1, 0);		/* solid lines */

    /* draw horizontal dots */
    for (i = 0; i < nv_dots; i += ggsp->ggs_res_major_v) {
	fy = (view_grid_start_pt_local[Y] + (i * ggsp->ggs_res_v * base2local)) * inv_sf;

	for (j = 0; j < nh_dots; ++j) {
	    fx = (view_grid_start_pt_local[X] + (j * ggsp->ggs_res_h * base2local)) * inv_sf;
	    DM_DRAW_POINT_2D(dmp, fx, fy * dmp->dm_aspect);
	}
    }

    /* draw vertical dots */
    if (ggsp->ggs_res_major_v != 1) {
	for (i = 0; i < nh_dots; i += ggsp->ggs_res_major_h) {
	    fx = (view_grid_start_pt_local[X] + (i * ggsp->ggs_res_h * base2local)) * inv_sf;

	    for (j = 0; j < nv_dots; ++j) {
		fy = (view_grid_start_pt_local[Y] + (j * ggsp->ggs_res_v * base2local)) * inv_sf;
		DM_DRAW_POINT_2D(dmp, fx, fy * dmp->dm_aspect);
	    }
	}
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
