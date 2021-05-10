/*                          A D C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/str.h"
#include "vmath.h"
#include "bview/adc.h"

void
adc_model_to_adc_view(struct bview_adc_state *adcs, mat_t model2view, fastf_t amax)
{
    MAT4X3PNT(adcs->pos_view, model2view, adcs->pos_model);
    adcs->dv_x = adcs->pos_view[X] * amax;
    adcs->dv_y = adcs->pos_view[Y] * amax;
}


void
adc_grid_to_adc_view(struct bview_adc_state *adcs, mat_t model2view, fastf_t amax)
{
    point_t model_pt = VINIT_ZERO;
    point_t view_pt;

    MAT4X3PNT(view_pt, model2view, model_pt);
    VADD2(adcs->pos_view, view_pt, adcs->pos_grid);
    adcs->dv_x = adcs->pos_view[X] * amax;
    adcs->dv_y = adcs->pos_view[Y] * amax;
}


void
adc_view_to_adc_grid(struct bview_adc_state *adcs, mat_t model2view)
{
    point_t model_pt = VINIT_ZERO;
    point_t view_pt;

    MAT4X3PNT(view_pt, model2view, model_pt);
    VSUB2(adcs->pos_grid, adcs->pos_view, view_pt);
}

/* From include/dm.h */
#define INV_GED 0.00048828125

void
adc_reset(struct bview_adc_state *adcs, mat_t view2model, mat_t model2view)
{
    adcs->dv_x = adcs->dv_y = 0;
    adcs->dv_a1 = adcs->dv_a2 = 0;
    adcs->dv_dist = 0;

    VSETALL(adcs->pos_view, 0.0);
    MAT4X3PNT(adcs->pos_model, view2model, adcs->pos_view);
    adcs->dst = (adcs->dv_dist * INV_GED + 1.0) * M_SQRT1_2;
    adcs->a1 = adcs->a2 = 45.0;
    adc_view_to_adc_grid(adcs, model2view);

    VSETALL(adcs->anchor_pt_a1, 0.0);
    VSETALL(adcs->anchor_pt_a2, 0.0);
    VSETALL(adcs->anchor_pt_dst, 0.0);

    adcs->anchor_pos = 0;
    adcs->anchor_a1 = 0;
    adcs->anchor_a2 = 0;
    adcs->anchor_dst = 0;
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
