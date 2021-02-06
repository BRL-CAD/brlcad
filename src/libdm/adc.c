/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2021 United States Government as represented by
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
/** @file libdm/adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bn.h"
#include "vmath.h"
#include "dm.h"
#include "dm/bview.h"
#include "./include/private.h"

static void
dm_draw_ticks(struct dm *dmp, struct bview_adc_state *adcp, fastf_t angle)
{
    fastf_t c_tdist;
    fastf_t d1, d2;
    fastf_t t1, t2;
    fastf_t x1, Y1;       /* not "y1", due to conflict with math lib */
    fastf_t x2, y2;

    /*
     * Position tic marks from dial 9.
     */
    /* map -2048 - 2047 into 0 - 2048 * sqrt (2) */
    /* Tick distance */
    c_tdist = ((fastf_t)(adcp->dv_dist) + GED_MAX) * M_SQRT1_2;

    d1 = c_tdist * cos (angle);
    d2 = c_tdist * sin (angle);
    t1 = 20.0 * sin (angle);
    t2 = 20.0 * cos (angle);

    /* Quadrant 1 */
    x1 = adcp->dv_x + d1 + t1;
    Y1 = adcp->dv_y + d2 - t2;
    x2 = adcp->dv_x + d1 -t1;
    y2 = adcp->dv_y + d2 + t2;
    if (bn_lseg_clip(&x1, &Y1, &x2, &y2, DM_MIN, DM_MAX) == 0) {
	dm_draw_line_2d(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    }

    /* Quadrant 2 */
    x1 = adcp->dv_x - d2 + t2;
    Y1 = adcp->dv_y + d1 + t1;
    x2 = adcp->dv_x - d2 - t2;
    y2 = adcp->dv_y + d1 - t1;
    if (bn_lseg_clip(&x1, &Y1, &x2, &y2, DM_MIN, DM_MAX) == 0) {
	dm_draw_line_2d(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    }

    /* Quadrant 3 */
    x1 = adcp->dv_x - d1 - t1;
    Y1 = adcp->dv_y - d2 + t2;
    x2 = adcp->dv_x - d1 + t1;
    y2 = adcp->dv_y - d2 - t2;
    if (bn_lseg_clip(&x1, &Y1, &x2, &y2, DM_MIN, DM_MAX) == 0) {
	dm_draw_line_2d(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    }

    /* Quadrant 4 */
    x1 = adcp->dv_x + d2 - t2;
    Y1 = adcp->dv_y - d1 - t1;
    x2 = adcp->dv_x + d2 + t2;
    y2 = adcp->dv_y - d1 + t1;
    if (bn_lseg_clip(&x1, &Y1, &x2, &y2, DM_MIN, DM_MAX) == 0) {
	dm_draw_line_2d(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    }
}


/**
 * Compute and display the angle/distance cursor.
 */
void
dm_draw_adc(struct dm *dmp, struct bview_adc_state *adcp, mat_t view2model, mat_t model2view)
{
    fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
    fastf_t x2, y2;
    fastf_t x3, y3;
    fastf_t x4, y4;
    fastf_t d1, d2;
    fastf_t angle1, angle2;

    if (adcp->anchor_pos == 1) {
	adc_model_to_adc_view(adcp, model2view, GED_MAX);
	adc_view_to_adc_grid(adcp, model2view);
    } else if (adcp->anchor_pos == 2) {
	adc_grid_to_adc_view(adcp, view2model, GED_MAX);
	MAT4X3PNT(adcp->pos_model, view2model, adcp->pos_view);
    } else {
	adc_view_to_adc_grid(adcp, model2view);
	MAT4X3PNT(adcp->pos_model, view2model, adcp->pos_view);
    }

    if (adcp->anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, model2view, adcp->anchor_pt_a1);
	dx = view_pt[X] * GED_MAX - adcp->dv_x;
	dy = view_pt[Y] * GED_MAX - adcp->dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    adcp->a1 = RAD2DEG*atan2(dy, dx);
	    adcp->dv_a1 = (1.0 - (adcp->a1 / 45.0)) * GED_MAX;
	}
    }

    if (adcp->anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, model2view, adcp->anchor_pt_a2);
	dx = view_pt[X] * GED_MAX - adcp->dv_x;
	dy = view_pt[Y] * GED_MAX - adcp->dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    adcp->a2 = RAD2DEG*atan2(dy, dx);
	    adcp->dv_a2 = (1.0 - (adcp->a2 / 45.0)) * GED_MAX;
	}
    }

    if (adcp->anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, model2view, adcp->anchor_pt_dst);

	dx = view_pt[X] * GED_MAX - adcp->dv_x;
	dy = view_pt[Y] * GED_MAX - adcp->dv_y;
	dist = sqrt(dx * dx + dy * dy);
	adcp->dst = dist * INV_GED;
	adcp->dv_dist = (dist / M_SQRT1_2) - GED_MAX;
    } else
	adcp->dst = (adcp->dv_dist * INV_GED + 1.0) * M_SQRT1_2;

    dm_set_fg(dmp,
		   adcp->line_color[0],
		   adcp->line_color[1],
		   adcp->line_color[2], 1, 1.0);
    dm_set_line_attr(dmp, adcp->line_width, 0);

    /* Horizontal */
    dm_draw_line_2d(dmp,
		    GED_TO_PM1(GED_MIN), GED_TO_PM1(adcp->dv_y) * dmp->i->dm_aspect,
		    GED_TO_PM1(GED_MAX), GED_TO_PM1(adcp->dv_y) * dmp->i->dm_aspect);

    /* Vertical */
    dm_draw_line_2d(dmp,
		    GED_TO_PM1(adcp->dv_x), GED_TO_PM1(GED_MAX),
		    GED_TO_PM1(adcp->dv_x), GED_TO_PM1(GED_MIN));

    angle1 = adcp->a1 * DEG2RAD;
    angle2 = adcp->a2 * DEG2RAD;

    /* sin for X and cos for Y to reverse sense of knob */
    d1 = cos (angle1) * 8000.0;
    d2 = sin (angle1) * 8000.0;
    x1 = adcp->dv_x + d1;
    Y1 = adcp->dv_y + d2;
    x2 = adcp->dv_x - d1;
    y2 = adcp->dv_y - d2;

    x3 = adcp->dv_x + d2;
    y3 = adcp->dv_y - d1;
    x4 = adcp->dv_x - d2;
    y4 = adcp->dv_y + d1;

    dm_draw_line_2d(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    dm_draw_line_2d(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->i->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->i->dm_aspect);

    d1 = cos(angle2) * 8000.0;
    d2 = sin(angle2) * 8000.0;
    x1 = adcp->dv_x + d1;
    Y1 = adcp->dv_y + d2;
    x2 = adcp->dv_x - d1;
    y2 = adcp->dv_y - d2;

    x3 = adcp->dv_x + d2;
    y3 = adcp->dv_y - d1;
    x4 = adcp->dv_x - d2;
    y4 = adcp->dv_y + d1;

    dm_set_line_attr(dmp, adcp->line_width, 1);
    dm_draw_line_2d(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->i->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->i->dm_aspect);
    dm_draw_line_2d(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->i->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->i->dm_aspect);
    dm_set_line_attr(dmp, adcp->line_width, 0);

    dm_set_fg(dmp,
		   adcp->tick_color[0],
		   adcp->tick_color[1],
		   adcp->tick_color[2], 1, 1.0);
    dm_draw_ticks(dmp, adcp, 0.0);
    dm_draw_ticks(dmp, adcp, angle1);
    dm_draw_ticks(dmp, adcp, angle2);
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
