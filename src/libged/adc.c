/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2008 United States Government as represented by
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
/** @file adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "dm.h"


void
ged_adc_model_To_adc_view(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    MAT4X3PNT(gasp->gas_pos_view, gvp->gv_model2view, gasp->gas_pos_model);
    gasp->gas_dv_x = gasp->gas_pos_view[X] * GED_MAX;
    gasp->gas_dv_y = gasp->gas_pos_view[Y] * GED_MAX;
}

void
ged_adc_grid_To_adc_view(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VADD2(gasp->gas_pos_view, view_pt, gasp->gas_pos_grid);
    gasp->gas_dv_x = gasp->gas_pos_view[X] * GED_MAX;
    gasp->gas_dv_y = gasp->gas_pos_view[Y] * GED_MAX;
}

void
ged_adc_view_To_adc_grid(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VSUB2(gasp->gas_pos_grid, gasp->gas_pos_view, view_pt);
}

void
ged_calc_adc_pos(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    if (gasp->gas_anchor_pos == 1) {
	ged_adc_model_To_adc_view(gasp, gvp);
	ged_adc_view_To_adc_grid(gasp, gvp);
    } else if (gasp->gas_anchor_pos == 2) {
	ged_adc_grid_To_adc_view(gasp, gvp);
	MAT4X3PNT(gasp->gas_pos_model, gvp->gv_view2model, gasp->gas_pos_view);
    } else {
	ged_adc_view_To_adc_grid(gasp, gvp);
	MAT4X3PNT(gasp->gas_pos_model, gvp->gv_view2model, gasp->gas_pos_view);
    }
}

void
ged_calc_adc_a1(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    if (gasp->gas_anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gasp->gas_anchor_pt_a1);
	dx = view_pt[X] * GED_MAX - gasp->gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gasp->gas_dv_y;

	if (dx != 0.0 || dy != 0.0) {
	    gasp->gas_a1 = RAD2DEG*atan2(dy, dx);
	    gasp->gas_dv_a1 = (1.0 - (gasp->gas_a1 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_a2(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    if (gasp->gas_anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gasp->gas_anchor_pt_a2);
	dx = view_pt[X] * GED_MAX - gasp->gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gasp->gas_dv_y;

	if (dx != 0.0 || dy != 0.0) {
	    gasp->gas_a2 = RAD2DEG*atan2(dy, dx);
	    gasp->gas_dv_a2 = (1.0 - (gasp->gas_a2 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_dst(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    if (gasp->gas_anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gasp->gas_anchor_pt_dst);

	dx = view_pt[X] * GED_MAX - gasp->gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gasp->gas_dv_y;
	dist = sqrt(dx * dx + dy * dy);
	gasp->gas_dst = dist * INV_GED;
	gasp->gas_dv_dist = (dist / M_SQRT1_2) - GED_MAX;
    } else
	gasp->gas_dst = (gasp->gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
}

void
ged_adc_reset(struct ged_adc_state *gasp, struct ged_view *gvp)
{
    gasp->gas_dv_x = gasp->gas_dv_y = 0;
    gasp->gas_dv_a1 = gasp->gas_dv_a2 = 0;
    gasp->gas_dv_dist = 0;

    VSETALL(gasp->gas_pos_view, 0.0);
    MAT4X3PNT(gasp->gas_pos_model, gvp->gv_view2model, gasp->gas_pos_view);
    gasp->gas_dst = (gasp->gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
    gasp->gas_a1 = gasp->gas_a2 = 45.0;
    ged_adc_view_To_adc_grid(gasp, gvp);

    VSETALL(gasp->gas_anchor_pt_a1, 0.0);
    VSETALL(gasp->gas_anchor_pt_a2, 0.0);
    VSETALL(gasp->gas_anchor_pt_dst, 0.0);

    gasp->gas_anchor_pos = 0;
    gasp->gas_anchor_a1 = 0;
    gasp->gas_anchor_a2 = 0;
    gasp->gas_anchor_dst = 0;
}

void
ged_adc_vls_print(struct ged_adc_state *gasp, struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", gasp->gas_draw);
    bu_vls_printf(out_vp, "a1 = %.15e\n", gasp->gas_a1);
    bu_vls_printf(out_vp, "a2 = %.15e\n", gasp->gas_a2);
    bu_vls_printf(out_vp, "dst = %.15e\n", gasp->gas_dst * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "odst = %d\n", gasp->gas_dv_dist);
    bu_vls_printf(out_vp, "hv = %.15e %.15e\n",
		  gasp->gas_pos_grid[X] * gvp->gv_scale * base2local,
		  gasp->gas_pos_grid[Y] * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "xyz = %.15e %.15e %.15e\n",
		  gasp->gas_pos_model[X] * base2local,
		  gasp->gas_pos_model[Y] * base2local,
		  gasp->gas_pos_model[Z] * base2local);
    bu_vls_printf(out_vp, "x = %d\n", gasp->gas_dv_x);
    bu_vls_printf(out_vp, "y = %d\n", gasp->gas_dv_y);
    bu_vls_printf(out_vp, "anchor_pos = %d\n", gasp->gas_anchor_pos);
    bu_vls_printf(out_vp, "anchor_a1 = %d\n", gasp->gas_anchor_a1);
    bu_vls_printf(out_vp, "anchor_a2 = %d\n", gasp->gas_anchor_a2);
    bu_vls_printf(out_vp, "anchor_dst = %d\n", gasp->gas_anchor_dst);
    bu_vls_printf(out_vp, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		  gasp->gas_anchor_pt_a1[X] * base2local,
		  gasp->gas_anchor_pt_a1[Y] * base2local,
		  gasp->gas_anchor_pt_a1[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		  gasp->gas_anchor_pt_a2[X] * base2local,
		  gasp->gas_anchor_pt_a2[Y] * base2local,
		  gasp->gas_anchor_pt_a2[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_dst = %.15e %.15e %.15e\n",
		  gasp->gas_anchor_pt_dst[X] * base2local,
		  gasp->gas_anchor_pt_dst[Y] * base2local,
		  gasp->gas_anchor_pt_dst[Z] * base2local);
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
