/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2008 United States Government as represented by
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
/** @file grid.c
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
#include "ged_private.h"


void
ged_grid_vls_print(struct ged_grid_state *ggsp, struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", ggsp->ggs_draw);
    bu_vls_printf(out_vp, "snap = %d\n", ggsp->ggs_snap);
    bu_vls_printf(out_vp, "anchor = %g %g %g\n",
		  ggsp->ggs_anchor[0] * base2local,
		  ggsp->ggs_anchor[1] * base2local,
		  ggsp->ggs_anchor[2] * base2local);
    bu_vls_printf(out_vp, "rh = %g\n", ggsp->ggs_res_h * base2local);
    bu_vls_printf(out_vp, "rv = %g\n", ggsp->ggs_res_v * base2local);
    bu_vls_printf(out_vp, "mrh = %d\n", ggsp->ggs_res_major_h);
    bu_vls_printf(out_vp, "mrv = %d\n", ggsp->ggs_res_major_v);
    bu_vls_printf(out_vp, "color = %d %d %d\n",
		  ggsp->ggs_color[0],
		  ggsp->ggs_color[1],
		  ggsp->ggs_color[2]);
}

void
ged_snap_to_grid(struct ged_grid_state *ggsp, struct ged_view *gvp, fastf_t base2local, fastf_t *mx, fastf_t *my)
{
    register int nh, nv;		/* whole grid units */
    point_t view_pt;
    point_t view_grid_anchor;
    fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
    fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
    register fastf_t sf;
    register fastf_t inv_sf;
    fastf_t local2base = 1.0 / base2local;

    if (NEAR_ZERO(ggsp->ggs_res_h, (fastf_t)SMALL_FASTF) ||
	NEAR_ZERO(ggsp->ggs_res_v, (fastf_t)SMALL_FASTF))
	return;

    sf = gvp->gv_scale*base2local;
    inv_sf = 1 / sf;

    VSET(view_pt, *mx, *my, 0.0);
    VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

    MAT4X3PNT(view_grid_anchor, gvp->gv_model2view, ggsp->ggs_anchor);
    VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

    grid_units_h = (view_grid_anchor[X] - view_pt[X]) / (ggsp->ggs_res_h * base2local);
    grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / (ggsp->ggs_res_v * base2local);
    nh = grid_units_h;
    nv = grid_units_v;

    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */

    if (grid_units_h <= -0.5)
	*mx = view_grid_anchor[X] - ((nh - 1) * ggsp->ggs_res_h * base2local);
    else if (0.5 <= grid_units_h)
	*mx = view_grid_anchor[X] - ((nh + 1) * ggsp->ggs_res_h * base2local);
    else
	*mx = view_grid_anchor[X] - (nh * ggsp->ggs_res_h * base2local);

    if (grid_units_v <= -0.5)
	*my = view_grid_anchor[Y] - ((nv - 1) * ggsp->ggs_res_v * base2local);
    else if (0.5 <= grid_units_v)
	*my = view_grid_anchor[Y] - ((nv + 1) * ggsp->ggs_res_v * base2local);
    else
	*my = view_grid_anchor[Y] - (nv  * ggsp->ggs_res_v * base2local);

    *mx *= inv_sf;
    *my *= inv_sf;
}

void
ged_snap_view_center_to_grid(struct ged_grid_state *ggsp, struct ged_view *gvp, fastf_t base2local)
{
    point_t view_pt;
    point_t model_pt;

    MAT_DELTAS_GET_NEG(model_pt, gvp->gv_center);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    ged_snap_to_grid(ggsp, gvp, base2local, &view_pt[X], &view_pt[Y]);
    MAT4X3PNT(model_pt, gvp->gv_view2model, view_pt);
    MAT_DELTAS_VEC_NEG(gvp->gv_center, model_pt);
    ged_view_update(gvp);
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
