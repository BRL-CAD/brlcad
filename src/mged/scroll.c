/*                        S C R O L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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
/** @file mged/scroll.c
 *
 */

#include "common.h"

#include <stdlib.h>

#include "tcl.h"

#include "vmath.h"
#include "ged.h"
#include "./mged.h"
#include "./mged_dm.h"

#include "./sedit.h"

#define SL_TOL 0.03125  /* size of dead spot - 64/2048 */

/************************************************************************
 *									*
 *	First part:  scroll bar definitions				*
 *									*
 ************************************************************************/

static void sl_tol(struct scroll_item *mptr, double val);
static void sl_atol(struct scroll_item *mptr, double val);
static void sl_rrtol(struct scroll_item *mptr, double val);
static void sl_artol(struct scroll_item *mptr, double val);
static void sl_itol(struct scroll_item *mptr, double val);
static void sl_adctol(struct scroll_item *mptr, double val);

struct scroll_item sl_menu[] = {
    { "xslew",	sl_tol,		0,	"X" },
    { "yslew",	sl_tol,		1,	"Y" },
    { "zslew",	sl_tol,		2,	"Z" },
    { "scale",	sl_tol,		3,	"S" },
    { "xrot",	sl_rrtol,	4,	"x" },
    { "yrot",	sl_rrtol,	5,	"y" },
    { "zrot",	sl_rrtol,	6,	"z" },
    { "",	NULL, 		0,	"" }
};


struct scroll_item sl_abs_menu[] = {
    { "Xslew",	sl_atol,	0,	"aX" },
    { "Yslew",	sl_atol,	1,	"aY" },
    { "Zslew",	sl_atol,	2,	"aZ" },
    { "Scale",	sl_tol,		3,	"aS" },
    { "Xrot",	sl_artol,	4,	"ax" },
    { "Yrot",	sl_artol,	5,	"ay" },
    { "Zrot",	sl_artol,	6,	"az" },
    { "",	NULL, 		0,	"" }
};


struct scroll_item sl_adc_menu[] = {
    { "xadc",	sl_itol,	0,	"xadc" },
    { "yadc",	sl_itol,	1,	"yadc" },
    { "ang 1",	sl_adctol,	2,	"ang1" },
    { "ang 2",	sl_adctol,	3,	"ang2" },
    { "tick",	sl_itol,	4,	"distadc" },
    { "",	NULL, 		0, 	"" }
};


/************************************************************************
 *									*
 *	Second part: Event Handlers called from menu items by buttons.c *
 *									*
 ************************************************************************/


/*
 * Set scroll_array.
 */
void
set_scroll(struct mged_state *s)
{
    if (mged_variables->mv_sliders) {
	if (mged_variables->mv_rateknobs)
	    scroll_array[0] = sl_menu;
	else
	    scroll_array[0] = sl_abs_menu;

	if (adc_state->adc_draw)
	    scroll_array[1] = sl_adc_menu;
	else
	    scroll_array[1] = NULL;

    } else {
	scroll_array[0] = NULL;
	scroll_array[1] = NULL;
    }
}


/*
 * Reset all scroll bars to the zero position.
 */
void
sl_halt_scroll(struct mged_state *s, int UNUSED(a), int UNUSED(b), int UNUSED(c))
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    bu_vls_printf(&vls, "knob zero");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


void
sl_toggle_scroll(struct mged_state *s, int UNUSED(a), int UNUSED(b), int UNUSED(c))
{
    mged_variables->mv_sliders = mged_variables->mv_sliders ? 0 : 1;

    {
	/* need dummy values for func signature--they are unused in the func */
	const struct bu_structparse *sdp = 0;
	const char name[] = "name";
	void *base = 0;
	const char value[] = "value";
	set_scroll_private(sdp, name, base, value, s);
    }
}


/************************************************************************
 *									*
 *	Third part:  event handlers called from tables, above		*
 *									*
 * Where the floating point value pointed to by scroll_val		*
 * in the range -1.0 to +1.0 is the only desired result,		*
 * everything can be handled by sl_tol().				*
 *									*
 ************************************************************************/

static void
sl_tol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


static void
sl_atol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (s->dbip == DBI_NULL)
	return;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


static void
sl_rrtol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val * RATE_ROT_FACTOR);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


static void
sl_artol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*ABS_ROT_FACTOR);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


static void
sl_adctol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, 45.0 - val*45.0);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


static void
sl_itol(struct scroll_item *mptr, double val)
{
    struct mged_state *s = MGED_STATE;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (val < -SL_TOL) {
	val += SL_TOL;
    } else if (val > SL_TOL) {
	val -= SL_TOL;
    } else {
	val = 0.0;
    }

    bu_vls_printf(&vls, "knob %s %f", mptr->scroll_cmd, val*BV_MAX);
    Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


/************************************************************************
 *									*
 *	Fourth part:  general-purpose interface mechanism		*
 *									*
 ************************************************************************/

void
second_menu_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    switch (mptr->scroll_val) {
	case 0:
	    (*f) = (double)adc_state->adc_dv_x * INV_BV;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 1:
	    (*f) = (double)adc_state->adc_dv_y * INV_BV;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 2:
	    (*f) = (double)adc_state->adc_dv_a1 * INV_BV;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 3:
	    (*f) = (double)adc_state->adc_dv_a2 * INV_BV;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 4:
	    (*f) = (double)adc_state->adc_dv_dist * INV_BV;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 5:
	    Tcl_AppendResult(s->interp, "scroll_display: 2nd scroll menu is hosed\n",
		    (char *)NULL);
	    break;
	case 6:
	    Tcl_AppendResult(s->interp, "scroll_display: 2nd scroll menu is hosed\n",
		    (char *)NULL);
	    break;
	default:
	    Tcl_AppendResult(s->interp, "scroll_display: 2nd scroll menu is hosed\n",
		    (char *)NULL);
    }
}

int
edit_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    switch (mptr->scroll_val) {
	case 0:
	    if (!(EDIT_TRAN && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_m[X];
		    else
			(*f) = s->edit_state.k.tra_m_abs[X];
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_v[X];
		    else
			(*f) = s->edit_state.k.tra_v_abs[X];
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 1:
	    if (!(EDIT_TRAN && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_m[Y];
		    else
			(*f) = s->edit_state.k.tra_m_abs[Y];
		    break;
		case 'v':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_v[Y];
		    else
			(*f) = s->edit_state.k.tra_v_abs[Y];
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 2:
	    if (!(EDIT_TRAN && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_m[Z];
		    else
			(*f) = s->edit_state.k.tra_m_abs[Z];
		    break;
		case 'v':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.tra_v[Z];
		    else
			(*f) = s->edit_state.k.tra_v_abs[Z];
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 3:
	    if (!(EDIT_SCALE && mged_variables->mv_transform == 'e'))
		return 0;

	    if (mged_variables->mv_rateknobs)
		(*f) = s->edit_state.k.sca;
	    else
		(*f) = s->edit_state.k.sca_abs;

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 4:
	    if (!(EDIT_ROTATE && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_m[X] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_m_abs[X] / ABS_ROT_FACTOR;
		    break;
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_o[X] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_o_abs[X] / ABS_ROT_FACTOR;
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_v[X] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_v_abs[X] / ABS_ROT_FACTOR;
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 5:
	    if (!(EDIT_ROTATE && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_m[Y] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_m_abs[Y] / ABS_ROT_FACTOR;
		    break;
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_o[Y] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_o_abs[Y] / ABS_ROT_FACTOR;
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_v[Y] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_v_abs[Y] / ABS_ROT_FACTOR;
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	case 6:
	    if (!(EDIT_ROTATE && mged_variables->mv_transform == 'e'))
		return 0;

	    switch (view_state->vs_gvp->gv_coord) {
		case 'm':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_m[Z] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_m_abs[Z] / ABS_ROT_FACTOR;
		    break;
		case 'o':
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_o[Z] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_o_abs[Z] / ABS_ROT_FACTOR;
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			(*f) = s->edit_state.k.rot_v[Z] / RATE_ROT_FACTOR;
		    else
			(*f) = s->edit_state.k.rot_v_abs[Z] / ABS_ROT_FACTOR;
		    break;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text1[0],
		    color_scheme->cs_slider_text1[1],
		    color_scheme->cs_slider_text1[2], 1, 1.0);
	    break;

	default:
	    Tcl_AppendResult(s->interp, "scroll_display: first scroll menu is hosed\n", (char *)NULL);
	    return 0;
    }

    // Success
    return 1;
}

void
view_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    switch (mptr->scroll_val) {
	case 0:
	    if (mged_variables->mv_rateknobs) {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m[X];
		else
		    (*f) = view_state->vs_gvp->k.tra_v[X];
	    } else {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m_abs[X];
		else
		    (*f) = view_state->vs_gvp->k.tra_v_abs[X];
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 1:
	    if (mged_variables->mv_rateknobs) {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m[Y];
		else
		    (*f) = view_state->vs_gvp->k.tra_v[Y];
	    } else {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m_abs[Y];
		else
		    (*f) = view_state->vs_gvp->k.tra_v_abs[Y];
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 2:
	    if (mged_variables->mv_rateknobs) {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m[Z];
		else
		    (*f) = view_state->vs_gvp->k.tra_v[Z];
	    } else {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.tra_m_abs[Z];
		else
		    (*f) = view_state->vs_gvp->k.tra_v_abs[Z];
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 3:
	    if (view_state->vs_gvp) {
		if (mged_variables->mv_rateknobs)
		    (*f) = view_state->vs_gvp->k.sca;
		else
		    (*f) = view_state->vs_gvp->k.sca_abs;
	    }
	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	case 4:
	    if (mged_variables->mv_rateknobs) {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.rot_m[X] / RATE_ROT_FACTOR;
		else
		    (*f) = view_state->vs_gvp->k.rot_v[X] / RATE_ROT_FACTOR;
	    } else {
		if (view_state->vs_gvp->gv_coord == 'm')
		    (*f) = view_state->vs_gvp->k.rot_m_abs[X] / ABS_ROT_FACTOR;
		else
		    (*f) = view_state->vs_gvp->k.rot_v_abs[X] / ABS_ROT_FACTOR;
	    }

	    dm_set_fg(DMP,
		    color_scheme->cs_slider_text2[0],
		    color_scheme->cs_slider_text2[1],
		    color_scheme->cs_slider_text2[2], 1, 1.0);
	    break;
	default:
	    Tcl_AppendResult(s->interp, "scroll_display: first scroll menu is hosed\n",
		    (char *)NULL);
    }
}

/*
 * The parameter is the Y pixel address of the starting
 * screen Y to be used, and the return value is the last screen Y
 * position used.
 */
int
scroll_display(struct mged_state *s, int y_top)
{
    int y;
    struct scroll_item *mptr;
    struct scroll_item **m;
    int xpos;
    int second_menu = -1;
    fastf_t f = 0;

    scroll_top = y_top;
    y = y_top;

    // If we are in an editing state, there are some combinations of scroll_val
    // and editing flags that will trigger different logic.  Check if any of
    // the potentially relevant edit states are currently active.
    int edit_flag = 0;
    edit_flag += (EDIT_ROTATE && mged_variables->mv_transform == 'e') ? 1 : 0;
    edit_flag += (EDIT_TRAN && mged_variables->mv_transform == 'e') ? 1 : 0;
    edit_flag += (EDIT_SCALE && mged_variables->mv_transform == 'e') ? 1 : 0;

    dm_set_line_attr(DMP, mged_variables->mv_linewidth, 0);

    for (m = &scroll_array[0]; *m != NULL; m++) {
	++second_menu;
	for (mptr = *m; mptr->scroll_string[0] != '\0'; mptr++) {
	    y += SCROLL_DY;		/* y is now bottom line pos */

	    int did_op = 0;
	    if (second_menu) {
		// Second menu has highest priority
		second_menu_scroll_display(&f, mptr, s);
		did_op = 1;
	    } else if (edit_flag) {
		// Potentially active edit state - check for a match
		did_op += edit_scroll_display(&f, mptr, s);
	    }
	    if (!did_op) {
		// Not second menu or edit case - do the baseline logic
		view_scroll_display(&f, mptr, s);
	    }

	    if (f > 0)
		xpos = (f + SL_TOL) * BV_MAX;
	    else if (f < 0)
		xpos = (f - SL_TOL) * -MENUXLIM;
	    else
		xpos = 0;

	    dm_draw_string_2d(DMP, mptr->scroll_string,
			      GED2PM1(xpos), GED2PM1(y-SCROLL_DY/2), 0, 0);
	    dm_set_fg(DMP,
			   color_scheme->cs_slider_line[0],
			   color_scheme->cs_slider_line[1],
			   color_scheme->cs_slider_line[2], 1, 1.0);
	    dm_draw_line_2d(DMP,
			    GED2PM1((int)BV_MAX), GED2PM1(y),
			    GED2PM1(MENUXLIM), GED2PM1(y));
	}
    }

    if (y != y_top) {
	/* Sliders were drawn, so make left vert edge */
	dm_set_fg(DMP,
		       color_scheme->cs_slider_line[0],
		       color_scheme->cs_slider_line[1],
		       color_scheme->cs_slider_line[2], 1, 1.0);
	dm_draw_line_2d(DMP,
			GED2PM1(MENUXLIM), GED2PM1(scroll_top-1),
			GED2PM1(MENUXLIM), GED2PM1(y));
    }
    return y;
}


/*
 * Called with Y coordinate of pen in menu area.
 *
 * Returns:
 * 1 if menu claims these pen coordinates,
 * 0 if pen is BELOW scroll
 * -1 if pen is ABOVE scroll (error)
 */
int
scroll_select(struct mged_state *s, int pen_x, int pen_y, int do_func)
{
    int yy;
    struct scroll_item **m;
    struct scroll_item *mptr;

    if (!mged_variables->mv_sliders) return 0;	/* not enabled */

    if (pen_y > scroll_top)
	return -1;	/* pen above menu area */

    /*
     * Start at the top of the list and see if the pen is
     * above here.
     */
    yy = scroll_top;
    for (m = &scroll_array[0]; *m != NULL; m++) {
	for (mptr = *m; mptr->scroll_string[0] != '\0'; mptr++) {
	    fastf_t val;
	    yy += SCROLL_DY;	/* bottom line pos */
	    if (pen_y < yy)
		continue;	/* pen below this item */

	    /*
	     * Record the location of scroll marker.
	     * Note that the left side has less width than
	     * the right side, due to the presence of the
	     * menu text area on the left.
	     */
	    if (pen_x >= 0) {
		val = pen_x * INV_BV;
	    } else {
		val = pen_x/(double)(-MENUXLIM);
	    }

	    /* See if hooked function has been specified */
	    if (!mptr->scroll_func) continue;

	    if (do_func)
		(*(mptr->scroll_func))(mptr, val);

	    return 1;		/* scroll claims pen value */
	}
    }
    return 0;		/* pen below scroll area */
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
