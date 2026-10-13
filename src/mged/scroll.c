/*                        S C R O L L . C
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
/** @file mged/scroll.c
 *
 * Scroll bar (slider) handling and display.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "tcl.h"

#include "vmath.h"
#include "ged.h"
#include "dm/faceplate.h"
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
sl_halt_scroll(struct rt_edit *UNUSED(es), int UNUSED(a), int UNUSED(b), int UNUSED(c), void *data)
{
    struct mged_state *s = (struct mged_state *)data;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    bu_vls_printf(&vls, "knob zero");
    (void)Tcl_Eval(s->interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
}


void
sl_toggle_scroll(struct rt_edit *UNUSED(es), int UNUSED(a), int UNUSED(b), int UNUSED(c), void *data)
{
    struct mged_state *s = (struct mged_state *)data;
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
 *	Fourth part:  display helpers 					*
 *									*
 ************************************************************************/

/* Handle second (ADC) menu display. */
static void
second_menu_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    switch (mptr->scroll_val) {
	case 0:
	    *f = (double)adc_state->adc_dv_x * INV_BV;
	    break;
	case 1:
	    *f = (double)adc_state->adc_dv_y * INV_BV;
	    break;
	case 2:
	    *f = (double)adc_state->adc_dv_a1 * INV_BV;
	    break;
	case 3:
	    *f = (double)adc_state->adc_dv_a2 * INV_BV;
	    break;
	case 4:
	    *f = (double)adc_state->adc_dv_dist * INV_BV;
	    break;
	default:
	    Tcl_AppendResult(s->interp,
		    "scroll_display: 2nd scroll menu is hosed\n", (char *)NULL);
	    return;
    }

}

/* Handle edit mode values for a given scroll item.
 * Returns 1 if handled (an edit mode applicable), 0 otherwise.
 */
static int
edit_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    /* Determine which edit transform (if any) is active for this scroll_val */
    switch (mptr->scroll_val) {
	case 0: /* X translation */
	case 1: /* Y translation */
	case 2: /* Z translation */
	    if (!(EDIT_TRAN && mged_variables->mv_transform == 'e'))
		return 0;
	    switch (mged_variables->mv_coords) {
		case 'm':
		case 'o':
		    if (mged_variables->mv_rateknobs)
			*f = MEDIT(s)->k.tra_m[mptr->scroll_val];
		    else
			*f = MEDIT(s)->k.tra_m_abs[mptr->scroll_val];
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			*f = MEDIT(s)->k.tra_v[mptr->scroll_val];
		    else
			*f = MEDIT(s)->k.tra_v_abs[mptr->scroll_val];
		    break;
	    }
	    break;

	case 3: /* scale */
	    if (!(EDIT_SCALE && mged_variables->mv_transform == 'e'))
		return 0;
	    if (mged_variables->mv_rateknobs)
		*f = MEDIT(s)->k.sca;
	    else
		*f = MEDIT(s)->k.sca_abs;
	    break;

	case 4: /* X rotation */
	case 5: /* Y rotation */
	case 6: /* Z rotation */
	    if (!(EDIT_ROTATE && mged_variables->mv_transform == 'e'))
		return 0;
	    switch (mged_variables->mv_coords) {
		case 'm':
		    if (mged_variables->mv_rateknobs)
			*f = MEDIT(s)->k.rot_m[mptr->scroll_val - 4] / RATE_ROT_FACTOR;
		    else
			*f = MEDIT(s)->k.rot_m_abs[mptr->scroll_val - 4] / ABS_ROT_FACTOR;
		    break;
		case 'o':
		    if (mged_variables->mv_rateknobs)
			*f = MEDIT(s)->k.rot_o[mptr->scroll_val - 4] / RATE_ROT_FACTOR;
		    else
			*f = MEDIT(s)->k.rot_o_abs[mptr->scroll_val - 4] / ABS_ROT_FACTOR;
		    break;
		case 'v':
		default:
		    if (mged_variables->mv_rateknobs)
			*f = MEDIT(s)->k.rot_v[mptr->scroll_val - 4] / RATE_ROT_FACTOR;
		    else
			*f = MEDIT(s)->k.rot_v_abs[mptr->scroll_val - 4] / ABS_ROT_FACTOR;
		    break;
	    }
	    break;

	default:
	    /* Not a recognized first menu scroll value */
	    return 0;
    }

    return 1;
}

/* Handle non-edit (view) values (first menu only). */
static void
view_scroll_display(fastf_t *f, struct scroll_item *mptr, struct mged_state *s)
{
    switch (mptr->scroll_val) {
	case 0: /* X translation */
	case 1: /* Y translation */
	case 2: /* Z translation */
	    if (mged_variables->mv_rateknobs) {
		if (mged_variables->mv_coords == 'm')
		    *f = view_state->k.tra_m[mptr->scroll_val];
		else
		    *f = view_state->k.tra_v[mptr->scroll_val];
	    } else {
		if (mged_variables->mv_coords == 'm')
		    *f = view_state->k.tra_m_abs[mptr->scroll_val];
		else
		    *f = view_state->k.tra_v_abs[mptr->scroll_val];
	    }
	    break;

	case 3: /* scale */
	    if (mged_variables->mv_rateknobs)
		*f = view_state->k.sca;
	    else
		*f = view_state->k.sca_abs;
	    break;

	case 4: /* X rotation */
	case 5: /* Y rotation */
	case 6: /* Z rotation */
	    if (mged_variables->mv_rateknobs) {
		if (mged_variables->mv_coords == 'm')
		    *f = view_state->k.rot_m[mptr->scroll_val - 4] / RATE_ROT_FACTOR;
		else
		    *f = view_state->k.rot_v[mptr->scroll_val - 4] / RATE_ROT_FACTOR;
	    } else {
		if (mged_variables->mv_coords == 'm')
		    *f = view_state->k.rot_m_abs[mptr->scroll_val - 4] / ABS_ROT_FACTOR;
		else
		    *f = view_state->k.rot_v_abs[mptr->scroll_val - 4] / ABS_ROT_FACTOR;
	    }
	    break;

	default:
	    Tcl_AppendResult(s->interp,
		    "scroll_display: first scroll menu is hosed\n", (char *)NULL);
	    return;
    }

}

/************************************************************************
 *									*
 *	Fifth part:  general-purpose interface mechanism		*
 *									*
 ************************************************************************/

static struct scroll_item *
faceplate_slider_item(struct mged_state *s, size_t row, int *group)
{
    for (*group = 0; scroll_array[*group]; (*group)++) {
        struct scroll_item *item;
        for (item = scroll_array[*group]; item->scroll_string[0]; item++) {
            if (!row) return item;
            row--;
        }
    }
    return NULL;
}

static int
faceplate_slider_get(void *data, size_t row, struct dm_faceplate_item *result)
{
    struct mged_state *s = (struct mged_state *)data;
    int group, editing = 0;
    struct scroll_item *item = faceplate_slider_item(s, row, &group);
    if (!item) return 0;
    result->label = item->scroll_string;
    result->value = 0;
    if (group) {
        second_menu_scroll_display(&result->value, item, s);
    } else {
        if (mged_variables->mv_transform == 'e' &&
            (EDIT_ROTATE || EDIT_TRAN || EDIT_SCALE))
            editing = edit_scroll_display(&result->value, item, s);
        if (!editing) view_scroll_display(&result->value, item, s);
    }
    VMOVE(result->color, editing ? color_scheme->cs_slider_text1 : color_scheme->cs_slider_text2);
    return 1;
}

static void
faceplate_slider_select(void *data, size_t row, fastf_t value, int activate)
{
    struct mged_state *s = (struct mged_state *)data;
    int group;
    struct scroll_item *item = faceplate_slider_item(s, row, &group);
    if (activate && item && item->scroll_func) item->scroll_func(item, value);
}

static void
faceplate_slider_controls(struct mged_state *s, struct dm_faceplate_controls *controls)
{
    dm_faceplate_controls_init(controls, DM_FACEPLATE_SLIDER);
    controls->top = GED2PM1(scroll_top);
    controls->dead_zone = SL_TOL;
    controls->data = s;
    controls->item = faceplate_slider_get;
    controls->select = faceplate_slider_select;
    controls->line_width = mged_variables->mv_linewidth;
    VMOVE(controls->line_color, color_scheme->cs_slider_line);
}

int
scroll_display(struct mged_state *s, int y_top)
{
    struct dm_faceplate_controls controls;
    scroll_top = y_top;
    faceplate_slider_controls(s, &controls);
    return (int)lrint(dm_faceplate_controls_draw(DMP, &controls) / INV_BV);
}

int
scroll_select(struct mged_state *s, int pen_x, int pen_y, int do_func)
{
    struct dm_faceplate_controls controls;
    if (!mged_variables->mv_sliders) return 0;
    if (pen_y > scroll_top) return -1;
    faceplate_slider_controls(s, &controls);
    return dm_faceplate_controls_select(&controls, GED2PM1(pen_x), GED2PM1(pen_y), do_func);
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
