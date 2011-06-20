/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "dm.h"


static void
dm_draw_ticks(struct dm *dmp, struct ged_view *gvp, fastf_t angle)
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
    c_tdist = ((fastf_t)(gvp->gv_adc.gas_dv_dist) + GED_MAX) * M_SQRT1_2;

    d1 = c_tdist * cos (angle);
    d2 = c_tdist * sin (angle);
    t1 = 20.0 * sin (angle);
    t2 = 20.0 * cos (angle);

    /* Quadrant 1 */
    x1 = gvp->gv_adc.gas_dv_x + d1 + t1;
    Y1 = gvp->gv_adc.gas_dv_y + d2 - t2;
    x2 = gvp->gv_adc.gas_dv_x + d1 -t1;
    y2 = gvp->gv_adc.gas_dv_y + d2 + t2;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 2 */
    x1 = gvp->gv_adc.gas_dv_x - d2 + t2;
    Y1 = gvp->gv_adc.gas_dv_y + d1 + t1;
    x2 = gvp->gv_adc.gas_dv_x - d2 - t2;
    y2 = gvp->gv_adc.gas_dv_y + d1 - t1;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 3 */
    x1 = gvp->gv_adc.gas_dv_x - d1 - t1;
    Y1 = gvp->gv_adc.gas_dv_y - d2 + t2;
    x2 = gvp->gv_adc.gas_dv_x - d1 + t1;
    y2 = gvp->gv_adc.gas_dv_y - d2 - t2;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 4 */
    x1 = gvp->gv_adc.gas_dv_x + d2 - t2;
    Y1 = gvp->gv_adc.gas_dv_y - d1 - t1;
    x2 = gvp->gv_adc.gas_dv_x + d2 + t2;
    y2 = gvp->gv_adc.gas_dv_y - d1 + t1;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }
}


/**
 * Compute and display the angle/distance cursor.
 */
void
dm_draw_adc(struct dm *dmp, struct ged_view *gvp)
{
    fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
    fastf_t x2, y2;
    fastf_t x3, y3;
    fastf_t x4, y4;
    fastf_t d1, d2;
    fastf_t angle1, angle2;

    ged_calc_adc_pos(gvp);
    ged_calc_adc_a1(gvp);
    ged_calc_adc_a2(gvp);
    ged_calc_adc_dst(gvp);

    DM_SET_FGCOLOR(dmp,
		   gvp->gv_adc.gas_line_color[0],
		   gvp->gv_adc.gas_line_color[1],
		   gvp->gv_adc.gas_line_color[2], 1, 1.0);
    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 0);

    /* Horizontal */
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(GED_MIN), GED_TO_PM1(gvp->gv_adc.gas_dv_y) * dmp->dm_aspect,
		    GED_TO_PM1(GED_MAX), GED_TO_PM1(gvp->gv_adc.gas_dv_y) * dmp->dm_aspect);

    /* Vertical */
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(gvp->gv_adc.gas_dv_x), GED_TO_PM1(GED_MAX),
		    GED_TO_PM1(gvp->gv_adc.gas_dv_x), GED_TO_PM1(GED_MIN));

    angle1 = gvp->gv_adc.gas_a1 * DEG2RAD;
    angle2 = gvp->gv_adc.gas_a2 * DEG2RAD;

    /* sin for X and cos for Y to reverse sense of knob */
    d1 = cos (angle1) * 8000.0;
    d2 = sin (angle1) * 8000.0;
    x1 = gvp->gv_adc.gas_dv_x + d1;
    Y1 = gvp->gv_adc.gas_dv_y + d2;
    x2 = gvp->gv_adc.gas_dv_x - d1;
    y2 = gvp->gv_adc.gas_dv_y - d2;

    x3 = gvp->gv_adc.gas_dv_x + d2;
    y3 = gvp->gv_adc.gas_dv_y - d1;
    x4 = gvp->gv_adc.gas_dv_x - d2;
    y4 = gvp->gv_adc.gas_dv_y + d1;

    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->dm_aspect);

    d1 = cos(angle2) * 8000.0;
    d2 = sin(angle2) * 8000.0;
    x1 = gvp->gv_adc.gas_dv_x + d1;
    Y1 = gvp->gv_adc.gas_dv_y + d2;
    x2 = gvp->gv_adc.gas_dv_x - d1;
    y2 = gvp->gv_adc.gas_dv_y - d2;

    x3 = gvp->gv_adc.gas_dv_x + d2;
    y3 = gvp->gv_adc.gas_dv_y - d1;
    x4 = gvp->gv_adc.gas_dv_x - d2;
    y4 = gvp->gv_adc.gas_dv_y + d1;

    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 1);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->dm_aspect);
    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 0);

    DM_SET_FGCOLOR(dmp,
		   gvp->gv_adc.gas_tick_color[0],
		   gvp->gv_adc.gas_tick_color[1],
		   gvp->gv_adc.gas_tick_color[2], 1, 1.0);
    dm_draw_ticks(dmp, gvp, 0.0);
    dm_draw_ticks(dmp, gvp, angle1);
    dm_draw_ticks(dmp, gvp, angle2);
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
