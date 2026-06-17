/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file mged/adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bu/vls.h"
#include "vmath.h"
#include "ged.h"
#include "./mged.h"
#include "./mged_dm.h"


static char adc_syntax1[] = "\
 adc			toggle display of angle/distance cursor\n\
 adc vars		print a list of all variables (i.e. var = val)\n\
 adc draw [0|1]		set or get the draw parameter\n\
 adc a1 [#]		set or get angle1\n\
 adc a2 [#]		set or get angle2\n\
 adc dst [#]		set or get radius (distance) of tick\n\
 adc odst [#]		set or get radius (distance) of tick (+-2047)\n\
 adc hv [# #]		set or get position (grid coordinates)\n\
 adc xyz [# # #]	set or get position (model coordinates)\n\
";

static char adc_syntax2[] = "\
 adc x [#]		set or get horizontal position (+-2047)\n\
 adc y [#]		set or get vertical position (+-2047)\n\
 adc dh #		add to horizontal position (grid coordinates)\n\
 adc dv #		add to vertical position (grid coordinates)\n\
 adc dx #		add to X position (model coordinates)\n\
 adc dy #		add to Y position (model coordinates)\n\
 adc dz #		add to Z position (model coordinates)\n\
";

static char adc_syntax3[] = "\
 adc anchor_pos	[0|1]	anchor ADC to current position in model coordinates\n\
 adc anchor_a1	[0|1]	anchor angle1 to go through anchorpoint_a1\n\
 adc anchor_a2	[0|1]	anchor angle2 to go through anchorpoint_a2\n\
 adc anchor_dst	[0|1]	anchor tick distance to go through anchorpoint_dst\n\
 adc anchorpoint_a1 [# # #]	set or get anchor point for angle1\n\
 adc anchorpoint_a2 [# # #]	set or get anchor point for angle2\n\
 adc anchorpoint_dst [# # #]	set or get anchor point for tick distance\n\
";

static char adc_syntax4[] = "\
 adc -i			any of the above appropriate commands will interpret parameters as increments\n\
 adc reset		reset angles, location, and tick distance\n\
 adc help		prints this help message\n\
";


void
adc_set_dirty_flag(struct mged_state *s)
{

    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	if (mged_dm_view_settings_shared(m_dmp, s->mged_curr_dm)) {
	    mged_dm_repaint_request(m_dmp, MGED_REPAINT_DEVICE_SETTING);
	}
    }
}


void
adc_set_scroll(struct mged_state *s)
{
    struct mged_dm *save_m_dmp = s->mged_curr_dm;

    for (size_t i = 0; i < BU_PTBL_LEN(&active_dm_set); i++) {
	struct mged_dm *m_dmp = (struct mged_dm *)BU_PTBL_GET(&active_dm_set, i);
	if (mged_dm_view_settings_shared(m_dmp, save_m_dmp)) {
	    set_curr_dm(s, m_dmp);
	    set_scroll(s);
	    mged_dm_repaint_request(s->mged_curr_dm, MGED_REPAINT_DEVICE_SETTING);
	}
    }

    set_curr_dm(s, save_m_dmp);
}


static void
adc_model_To_adc_view(struct mged_state *s, struct bsg_adc_state *adc)
{
    MAT4X3PNT(adc->pos_view, view_state->vs_gvp->gv_model2view, adc->pos_model);
    adc->dv_x = adc->pos_view[X] * BSG_VIEW_MAX;
    adc->dv_y = adc->pos_view[Y] * BSG_VIEW_MAX;
}


static void
adc_grid_To_adc_view(struct mged_state *s, struct bsg_adc_state *adc)
{
    point_t model_pt = VINIT_ZERO;
    point_t view_pt;

    MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, model_pt);
    VADD2(adc->pos_view, view_pt, adc->pos_grid);
    adc->dv_x = adc->pos_view[X] * BSG_VIEW_MAX;
    adc->dv_y = adc->pos_view[Y] * BSG_VIEW_MAX;
}


static void
adc_view_To_adc_grid(struct mged_state *s, struct bsg_adc_state *adc)
{
    point_t model_pt = VINIT_ZERO;
    point_t view_pt;

    MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, model_pt);
    VSUB2(adc->pos_grid, adc->pos_view, view_pt);
}


static void
calc_adc_pos(struct mged_state *s, struct bsg_adc_state *adc)
{
    if (adc->anchor_pos == 1) {
	adc_model_To_adc_view(s, adc);
	adc_view_To_adc_grid(s, adc);
    } else if (adc->anchor_pos == 2) {
	adc_grid_To_adc_view(s, adc);
	MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);
    } else {
	adc_view_To_adc_grid(s, adc);
	MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);
    }
}


static void
calc_adc_a1(struct mged_state *s, struct bsg_adc_state *adc)
{
    if (adc->anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, adc->anchor_pt_a1);
	dx = view_pt[X] * BSG_VIEW_MAX - adc->dv_x;
	dy = view_pt[Y] * BSG_VIEW_MAX - adc->dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    adc->a1 = RAD2DEG*atan2(dy, dx);
	    adc->dv_a1 = (1.0 - (adc->a1 / 45.0)) * BSG_VIEW_MAX;
	}
    }
}


static void
calc_adc_a2(struct mged_state *s, struct bsg_adc_state *adc)
{
    if (adc->anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, adc->anchor_pt_a2);
	dx = view_pt[X] * BSG_VIEW_MAX - adc->dv_x;
	dy = view_pt[Y] * BSG_VIEW_MAX - adc->dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    adc->a2 = RAD2DEG*atan2(dy, dx);
	    adc->dv_a2 = (1.0 - (adc->a2 / 45.0)) * BSG_VIEW_MAX;
	}
    }
}


static void
calc_adc_dst(struct mged_state *s, struct bsg_adc_state *adc)
{
    if (adc->anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, adc->anchor_pt_dst);

	dx = view_pt[X] * BSG_VIEW_MAX - adc->dv_x;
	dy = view_pt[Y] * BSG_VIEW_MAX - adc->dv_y;
	dist = sqrt(dx * dx + dy * dy);
	adc->dst = dist * INV_BV;
	adc->dv_dist = (dist / M_SQRT1_2) - BSG_VIEW_MAX;
    } else
	adc->dst = (adc->dv_dist * INV_BV + 1.0) * M_SQRT1_2;
}


static void
draw_ticks(struct mged_state *s, struct bsg_adc_state *adc, fastf_t angle)
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
    c_tdist = ((fastf_t)(adc->dv_dist) + BSG_VIEW_MAX) * M_SQRT1_2;

    d1 = c_tdist * cos (angle);
    d2 = c_tdist * sin (angle);
    t1 = 20.0 * sin (angle);
    t2 = 20.0 * cos (angle);

    /* Quadrant 1 */
    x1 = adc->dv_x + d1 + t1;
    Y1 = adc->dv_y + d2 - t2;
    x2 = adc->dv_x + d1 -t1;
    y2 = adc->dv_y + d2 + t2;
    if (clip(&x1, &Y1, &x2, &y2) == 0) {
	dm_draw_line_2d(DMP,
			GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
			GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    }

    /* Quadrant 2 */
    x1 = adc->dv_x - d2 + t2;
    Y1 = adc->dv_y + d1 + t1;
    x2 = adc->dv_x - d2 - t2;
    y2 = adc->dv_y + d1 - t1;
    if (clip (&x1, &Y1, &x2, &y2) == 0) {
	dm_draw_line_2d(DMP,
			GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
			GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    }

    /* Quadrant 3 */
    x1 = adc->dv_x - d1 - t1;
    Y1 = adc->dv_y - d2 + t2;
    x2 = adc->dv_x - d1 + t1;
    y2 = adc->dv_y - d2 - t2;
    if (clip (&x1, &Y1, &x2, &y2) == 0) {
	dm_draw_line_2d(DMP,
			GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
			GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    }

    /* Quadrant 4 */
    x1 = adc->dv_x + d2 - t2;
    Y1 = adc->dv_y - d1 - t1;
    x2 = adc->dv_x + d2 + t2;
    y2 = adc->dv_y - d1 + t1;
    if (clip (&x1, &Y1, &x2, &y2) == 0) {
	dm_draw_line_2d(DMP,
			GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
			GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    }
}


/**
 * Compute and display the angle/distance cursor.
 */
void
adcursor(struct mged_state *s)
{
    struct bsg_adc_state adc_record;
    struct bsg_adc_state *adc = &adc_record;
    fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
    fastf_t x2, y2;
    fastf_t x3, y3;
    fastf_t x4, y4;
    fastf_t d1, d2;
    fastf_t angle1, angle2;

    if (!mged_dm_adc_state_get(s->mged_curr_dm, adc))
	return;

    calc_adc_pos(s, adc);
    calc_adc_a1(s, adc);
    calc_adc_a2(s, adc);
    calc_adc_dst(s, adc);

    dm_set_fg(DMP,
		   color_scheme->cs_adc_line[0],
		   color_scheme->cs_adc_line[1],
		   color_scheme->cs_adc_line[2], 1, 1.0);
    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    /* Horizontal */
    dm_draw_line_2d(DMP,
		    GED2PM1(BSG_VIEW_MIN), GED2PM1(adc->dv_y) * dm_get_aspect(DMP),
		    GED2PM1(BSG_VIEW_MAX), GED2PM1(adc->dv_y) * dm_get_aspect(DMP));

    /* Vertical */
    dm_draw_line_2d(DMP,
		    GED2PM1(adc->dv_x), GED2PM1(BSG_VIEW_MAX),
		    GED2PM1(adc->dv_x), GED2PM1(BSG_VIEW_MIN));

    angle1 = adc->a1 * DEG2RAD;
    angle2 = adc->a2 * DEG2RAD;

    /* sin for X and cos for Y to reverse sense of knob */
    d1 = cos (angle1) * 8000.0;
    d2 = sin (angle1) * 8000.0;
    x1 = adc->dv_x + d1;
    Y1 = adc->dv_y + d2;
    x2 = adc->dv_x - d1;
    y2 = adc->dv_y - d2;

    x3 = adc->dv_x + d2;
    y3 = adc->dv_y - d1;
    x4 = adc->dv_x - d2;
    y4 = adc->dv_y + d1;

    dm_draw_line_2d(DMP,
		    GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
		    GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    dm_draw_line_2d(DMP,
		    GED2PM1(x3), GED2PM1(y3) * dm_get_aspect(DMP),
		    GED2PM1(x4), GED2PM1(y4) * dm_get_aspect(DMP));

    d1 = cos(angle2) * 8000.0;
    d2 = sin(angle2) * 8000.0;
    x1 = adc->dv_x + d1;
    Y1 = adc->dv_y + d2;
    x2 = adc->dv_x - d1;
    y2 = adc->dv_y - d2;

    x3 = adc->dv_x + d2;
    y3 = adc->dv_y - d1;
    x4 = adc->dv_x - d2;
    y4 = adc->dv_y + d1;

    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 1);
    dm_draw_line_2d(DMP,
		    GED2PM1(x1), GED2PM1(Y1) * dm_get_aspect(DMP),
		    GED2PM1(x2), GED2PM1(y2) * dm_get_aspect(DMP));
    dm_draw_line_2d(DMP,
		    GED2PM1(x3), GED2PM1(y3) * dm_get_aspect(DMP),
		    GED2PM1(x4), GED2PM1(y4) * dm_get_aspect(DMP));
    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    dm_set_fg(DMP,
		   color_scheme->cs_adc_tick[0],
		   color_scheme->cs_adc_tick[1],
		   color_scheme->cs_adc_tick[2], 1, 1.0);
    draw_ticks(s, adc, 0.0);
    draw_ticks(s, adc, angle1);
    draw_ticks(s, adc, angle2);
    mged_dm_adc_state_set(s->mged_curr_dm, adc);
}


static void
mged_adc_reset(struct mged_state *s, struct bsg_adc_state *adc)
{
    adc->dv_x = adc->dv_y = 0;
    adc->dv_a1 = adc->dv_a2 = 0;
    adc->dv_dist = 0;

    VSETALL(adc->pos_view, 0.0);
    MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);
    adc->dst = (adc->dv_dist * INV_BV + 1.0) * M_SQRT1_2;
    adc->a1 = adc->a2 = 45.0;
    adc_view_To_adc_grid(s, adc);

    VSETALL(adc->anchor_pt_a1, 0.0);
    VSETALL(adc->anchor_pt_a2, 0.0);
    VSETALL(adc->anchor_pt_dst, 0.0);

    adc->anchor_pos = 0;
    adc->anchor_a1 = 0;
    adc->anchor_a2 = 0;
    adc->anchor_dst = 0;
}


static void
adc_print_vars(struct mged_state *s, struct bsg_adc_state *adc)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    bu_vls_printf(&vls, "draw = %d\n", adc->draw);
    bu_vls_printf(&vls, "a1 = %.15e\n", adc->a1);
    bu_vls_printf(&vls, "a2 = %.15e\n", adc->a2);
    bu_vls_printf(&vls, "dst = %.15e\n", adc->dst * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
    bu_vls_printf(&vls, "odst = %d\n", adc->dv_dist);
    bu_vls_printf(&vls, "hv = %.15e %.15e\n",
		  adc->pos_grid[X] * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
		  adc->pos_grid[Y] * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
    bu_vls_printf(&vls, "xyz = %.15e %.15e %.15e\n",
		  adc->pos_model[X] * s->dbip->dbi_base2local,
		  adc->pos_model[Y] * s->dbip->dbi_base2local,
		  adc->pos_model[Z] * s->dbip->dbi_base2local);
    bu_vls_printf(&vls, "x = %d\n", adc->dv_x);
    bu_vls_printf(&vls, "y = %d\n", adc->dv_y);
    bu_vls_printf(&vls, "anchor_pos = %d\n", adc->anchor_pos);
    bu_vls_printf(&vls, "anchor_a1 = %d\n", adc->anchor_a1);
    bu_vls_printf(&vls, "anchor_a2 = %d\n", adc->anchor_a2);
    bu_vls_printf(&vls, "anchor_dst = %d\n", adc->anchor_dst);
    bu_vls_printf(&vls, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		  adc->anchor_pt_a1[X] * s->dbip->dbi_base2local,
		  adc->anchor_pt_a1[Y] * s->dbip->dbi_base2local,
		  adc->anchor_pt_a1[Z] * s->dbip->dbi_base2local);
    bu_vls_printf(&vls, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		  adc->anchor_pt_a2[X] * s->dbip->dbi_base2local,
		  adc->anchor_pt_a2[Y] * s->dbip->dbi_base2local,
		  adc->anchor_pt_a2[Z] * s->dbip->dbi_base2local);
    bu_vls_printf(&vls, "anchorpoint_dst = %.15e %.15e %.15e\n",
		  adc->anchor_pt_dst[X] * s->dbip->dbi_base2local,
		  adc->anchor_pt_dst[Y] * s->dbip->dbi_base2local,
		  adc->anchor_pt_dst[Z] * s->dbip->dbi_base2local);
    Tcl_AppendResult(s->interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);
}


int
f_adc (
    ClientData clientData,
    Tcl_Interp *interp,
    int argc,
    const char *argv[])
{
    struct cmdtab *ctp = (struct cmdtab *)clientData;
    MGED_CK_CMD(ctp);
    struct mged_state *s = ctp->s;

    struct bu_vls vls = BU_VLS_INIT_ZERO;
    const char *parameter;
    const char **argp = argv;
    struct bsg_adc_state adc_record;
    struct bsg_adc_state *adc = &adc_record;
    point_t user_pt;		/* Value(s) provided by user */
    point_t scaled_pos;
    int incr_flag;
    int i;

    CHECK_DBI_NULL;

#define ADC_RETURN(_ret) do { \
	mged_dm_adc_state_set(s->mged_curr_dm, adc); \
	return (_ret); \
    } while (0)

    if (!mged_dm_adc_state_get(s->mged_curr_dm, adc))
	return TCL_ERROR;

    if (6 < argc) {
	bu_vls_printf(&vls, "help adc");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	ADC_RETURN(TCL_ERROR);
    }

    if (argc == 1) {
	if (adc->draw)
	    adc->draw = 0;
	else
	    adc->draw = 1;

	if (adc_auto) {
	    mged_adc_reset(s, adc);
	    adc_auto = 0;
	}

	adc_set_scroll(s);

	ADC_RETURN(TCL_OK);
    }

    if (BU_STR_EQUAL(argv[1], "-i")) {
	if (argc < 4) {
	    bu_vls_printf(&vls, "adc: -i option specified without an op-val pair");
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_ERROR);
	}

	incr_flag = 1;
	parameter = argv[2];
	argc -= 3;
	argp += 3;
    } else {
	incr_flag = 0;
	parameter = argv[1];
	argc -= 2;
	argp += 2;
    }

    for (i = 0; i < argc; ++i)
	user_pt[i] = atof(argp[i]);

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->draw);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		adc->draw = 1;
	    else
		adc->draw = 0;

	    adc_set_scroll(s);

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc draw' command accepts 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "a1")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%.15e", adc->a1);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_a1) {
		if (incr_flag)
		    adc->a1 += user_pt[0];
		else
		    adc->a1 = user_pt[0];

		adc->dv_a1 = (1.0 - (adc->a1 / 45.0)) * BSG_VIEW_MAX;
		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc a1' command accepts only 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "a2")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%.15e", adc->a2);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_a2) {
		if (incr_flag)
		    adc->a2 += user_pt[0];
		else
		    adc->a2 = user_pt[0];

		adc->dv_a2 = (1.0 - (adc->a2 / 45.0)) * BSG_VIEW_MAX;
		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc a2' command accepts 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dst")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%.15e", adc->dst * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_dst) {
		if (incr_flag)
		    adc->dst += user_pt[0] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		else
		    adc->dst = user_pt[0] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);

		adc->dv_dist = (adc->dst / M_SQRT1_2 - 1.0) * BSG_VIEW_MAX;

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dst' command accepts 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "odst")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->dv_dist);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_dst) {
		if (incr_flag)
		    adc->dv_dist += user_pt[0];
		else
		    adc->dv_dist = user_pt[0];

		adc->dst = (adc->dv_dist * INV_BV + 1.0) * M_SQRT1_2;
		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc odst' command accepts 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dh")) {
	if (argc == 1) {
	    if (!adc->anchor_pos) {
		adc->pos_grid[X] += user_pt[0] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		adc_grid_To_adc_view(s, adc);
		MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dh' command requires 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dv")) {
	if (argc == 1) {
	    if (!adc->anchor_pos) {
		adc->pos_grid[Y] += user_pt[0] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		adc_grid_To_adc_view(s, adc);
		MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dv' command requires 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "hv")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%.15e %.15e",
			  adc->pos_grid[X] * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local,
			  adc->pos_grid[Y] * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 2) {
	    if (!adc->anchor_pos) {
		if (incr_flag) {
		    adc->pos_grid[X] += user_pt[X] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		    adc->pos_grid[Y] += user_pt[Y] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		} else {
		    adc->pos_grid[X] = user_pt[X] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		    adc->pos_grid[Y] = user_pt[Y] / (view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		}

		adc->pos_grid[Z] = 0.0;
		adc_grid_To_adc_view(s, adc);
		MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_model);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc hv' command requires 0 or 2 arguments\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dx")) {
	if (argc == 1) {
	    if (!adc->anchor_pos) {
		adc->pos_model[X] += user_pt[0] * s->dbip->dbi_local2base;
		adc_model_To_adc_view(s, adc);
		adc_view_To_adc_grid(s, adc);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dx' command requires 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dy")) {
	if (argc == 1) {
	    if (!adc->anchor_pos) {
		adc->pos_model[Y] += user_pt[0] * s->dbip->dbi_local2base;
		adc_model_To_adc_view(s, adc);
		adc_view_To_adc_grid(s, adc);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dy' command requires 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "dz")) {
	if (argc == 1) {
	    if (!adc->anchor_pos) {
		adc->pos_model[Z] += user_pt[0] * s->dbip->dbi_local2base;
		adc_model_To_adc_view(s, adc);
		adc_view_To_adc_grid(s, adc);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc dz' command requires 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "xyz")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, adc->pos_model, s->dbip->dbi_base2local);

	    bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, s->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(adc->pos_model, adc->pos_model, user_pt);
	    } else {
		VMOVE(adc->pos_model, user_pt);
	    }

	    adc_model_To_adc_view(s, adc);
	    adc_view_To_adc_grid(s, adc);

	    adc_set_dirty_flag(s);

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc xyz' command requires 0 or 3 arguments\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "x")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->dv_x);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_pos) {
		if (incr_flag) {
		    adc->dv_x += user_pt[0];
		} else {
		    adc->dv_x = user_pt[0];
		}

		adc->pos_view[X] = adc->dv_x * INV_BV;
		adc->pos_view[Y] = adc->dv_y * INV_BV;
		adc_view_To_adc_grid(s, adc);
		MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc x' command requires 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "y")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->dv_y);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    if (!adc->anchor_pos) {
		if (incr_flag) {
		    adc->dv_y += user_pt[0];
		} else {
		    adc->dv_y = user_pt[0];
		}

		adc->pos_view[X] = adc->dv_x * INV_BV;
		adc->pos_view[Y] = adc->dv_y * INV_BV;
		adc_view_To_adc_grid(s, adc);
		MAT4X3PNT(adc->pos_model, view_state->vs_gvp->gv_view2model, adc->pos_view);

		adc_set_dirty_flag(s);
	    }

	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc y' command requires 0 or 1 argument\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchor_pos")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->anchor_pos);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i < 0 || 2 < i) {
		Tcl_AppendResult(interp, "The 'adc anchor_pos parameter accepts values of 0, 1, or 2.",
				 (char *)NULL);
		ADC_RETURN(TCL_ERROR);
	    }

	    adc->anchor_pos = i;

	    calc_adc_pos(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchor_pos' command accepts 0 or 1 argument\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchor_a1")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->anchor_a1);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		adc->anchor_a1 = 1;
	    else
		adc->anchor_a1 = 0;

	    calc_adc_a1(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchor_a1' command accepts 0 or 1 argument\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a1")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, adc->anchor_pt_a1, s->dbip->dbi_base2local);

	    bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, s->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(adc->anchor_pt_a1, adc->anchor_pt_a1, user_pt);
	    } else {
		VMOVE(adc->anchor_pt_a1, user_pt);
	    }

	    calc_adc_a1(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchorpoint_a1' command accepts 0 or 3 arguments\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchor_a2")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->anchor_a2);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		adc->anchor_a2 = 1;
	    else
		adc->anchor_a2 = 0;

	    calc_adc_a2(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchor_a2' command accepts 0 or 1 argument\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a2")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, adc->anchor_pt_a2, s->dbip->dbi_base2local);

	    bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, s->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(adc->anchor_pt_a2, adc->anchor_pt_a2, user_pt);
	    } else {
		VMOVE(adc->anchor_pt_a2, user_pt);
	    }

	    calc_adc_a2(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchorpoint_a2' command accepts 0 or 3 arguments\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchor_dst")) {
	if (argc == 0) {
	    bu_vls_printf(&vls, "%d", adc->anchor_dst);
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i) {
		adc->anchor_dst = 1;
	    } else
		adc->anchor_dst = 0;

	    calc_adc_dst(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchor_dst' command accepts 0 or 1 argument\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_dst")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, adc->anchor_pt_dst, s->dbip->dbi_base2local);

	    bu_vls_printf(&vls, "%.15e %.15e %.15e", V3ARGS(scaled_pos));
	    Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    ADC_RETURN(TCL_OK);
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, s->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(adc->anchor_pt_dst, adc->anchor_pt_dst, user_pt);
	    } else {
		VMOVE(adc->anchor_pt_dst, user_pt);
	    }

	    calc_adc_dst(s, adc);
	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc anchorpoint_dst' command accepts 0 or 3 arguments\n",
			 (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "reset")) {
	if (argc == 0) {
	    mged_adc_reset(s, adc);

	    adc_set_dirty_flag(s);
	    ADC_RETURN(TCL_OK);
	}

	Tcl_AppendResult(interp, "The 'adc reset' command accepts no arguments\n", (char *)NULL);
	ADC_RETURN(TCL_ERROR);
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	adc_print_vars(s, adc);
	ADC_RETURN(TCL_OK);
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	Tcl_AppendResult(interp, "Usage:\n", adc_syntax1, adc_syntax2, adc_syntax3, adc_syntax4, (char *)NULL);
	ADC_RETURN(TCL_OK);
    }

    Tcl_AppendResult(interp, "ADC: unrecognized command: '",
		     argv[1], "'\nUsage:\n", adc_syntax1, adc_syntax2, adc_syntax3, adc_syntax4, (char *)NULL);
    ADC_RETURN(TCL_ERROR);
#undef ADC_RETURN
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
