/*                       D O E V E N T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file mged/doevent.c
 *
 * X event handling routines for MGED.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>

#ifdef HAVE_GL_DEVICE_H
#  include <gl/device.h>
#endif

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#ifdef HAVE_X11_XLIB_H
#  include <X11/Xutil.h>
#  include <X11/keysym.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
#  include <X11/extensions/XI.h>
#  include <X11/extensions/XInput.h>
#endif

#include "vmath.h"
#include "raytrace.h"
#include "ged.h"

#include "./mged.h"
#include "./mged_dm.h"
#include "./sedit.h"


extern int doMotion;			/* defined in buttons.c */

#ifdef HAVE_X11_TYPES
static void motion_event_handler(struct mged_state *, XMotionEvent *);
#endif

#ifdef IR_KNOBS
static void dials_event_handler(XDeviceMotionEvent *);
#endif

#ifdef IR_KNOBS
#  define NOISE 16              /* Size of dead spot on knob */
#endif

#ifdef IR_BUTTONS
static void buttons_event_handler(XDeviceButtonEvent *);
/*
 * Map SGI Button numbers to MGED button functions.
 * The layout of this table is suggestive of the actual button box layout.
 */
#define SW_HELP_KEY SW0
#define SW_ZERO_KEY SW3
#define HELP_KEY 0
#define ZERO_KNOBS 0
static unsigned char bmap[IR_BUTTONS] = {
    HELP_KEY,    BV_ADCURSOR, BV_RESET,    ZERO_KNOBS,
    BE_O_SCALE,  BE_O_XSCALE, BE_O_YSCALE, BE_O_ZSCALE, 0,           BV_VSAVE,
    BE_O_X,      BE_O_Y,      BE_O_XY,     BE_O_ROTATE, 0,           BV_VRESTORE,
    BE_S_TRANS,  BE_S_ROTATE, BE_S_SCALE,  BE_MENU,     BE_O_ILLUMINATE, BE_S_ILLUMINATE,
    BE_REJECT,   BV_BOTTOM,   BV_TOP,      BV_REAR,     BV_45_45,    BE_ACCEPT,
    BV_RIGHT,    BV_FRONT,    BV_LEFT,     BV_35_25
};
#endif

#ifdef IR_KNOBS
/*
 * Labels for knobs in help mode.
 */
static char *kn1_knobs[] = {
    /* 0 */ "adc <1",	/* 1 */ "zoom",
    /* 2 */ "adc <2",	/* 3 */ "adc dist",
    /* 4 */ "adc y",	/* 5 */ "y slew",
    /* 6 */ "adc x",	/* 7 */	"x slew"
};
static char *kn2_knobs[] = {
    /* 0 */ "unused",	/* 1 */	"zoom",
    /* 2 */ "z rot",	/* 3 */ "z slew",
    /* 4 */ "y rot",	/* 5 */ "y slew",
    /* 6 */ "x rot",	/* 7 */	"x slew"
};
#endif

#if defined(IR_BUTTONS) || defined(IR_KNOBS)
static int button0 = 0;
#endif

#ifdef HAVE_X11_TYPES
int
doEvent(ClientData clientData, XEvent *eventPtr)
{
    struct mged_state *s = (struct mged_state *)clientData;
    struct mged_dm *save_dm_list;
    int status;

    if (eventPtr->type == DestroyNotify || (unsigned long)eventPtr->xany.window == 0 || !MGED_STATE)
	return TCL_OK;

    // The set_curr_dm logic here appears to be important for Multipane mode -
    // it doesn't do much if only one dm is up, but if the set_curr_dm calls
    // are removed MGED doesn't update the multipane views correctly.
    save_dm_list = s->mged_curr_dm;
    GET_MGED_DM(s->mged_curr_dm, (unsigned long)eventPtr->xany.window);

    /* it's an event for a window that I'm not handling */
    if (s->mged_curr_dm == MGED_DM_NULL) {
	if (save_dm_list)
	    MGED_CK_STATE(s);
	set_curr_dm(s, save_dm_list);
	return TCL_OK;
    }

    /* calling the display manager specific event handler */
    status = dm_doevent(DMP, clientData, eventPtr);
    DMP_dirty = dm_get_dirty(DMP);

    /* no further processing of this event */
    if (status != TCL_OK) {
	if (save_dm_list)
	    MGED_CK_STATE(s);
	set_curr_dm(s, save_dm_list);
	return status;
    }

    /* Continuing to process the event */

    if (eventPtr->type == ConfigureNotify) {
	XConfigureEvent *conf = (XConfigureEvent *)eventPtr;

	dm_configure_win(DMP, 0);
	rect_image2view(s);
	DMP_dirty = 1;
	dm_set_dirty(DMP, 1);

	if (fbp)
	    (void)fb_configure_window(fbp, conf->width, conf->height);

	/* no further processing of this event */
	status = TCL_RETURN;
    } else if (eventPtr->type == MapNotify) {
	mapped = 1;
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    } else if (eventPtr->type == UnmapNotify) {
	mapped = 0;
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    } else if (eventPtr->type == MotionNotify) {
	motion_event_handler(s, (XMotionEvent *)eventPtr);
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    }
#ifdef IR_KNOBS
    else if (dm_event_cmp(DMP, DM_MOTION_NOTIFY, eventPtr->type) == 1) {
	dials_event_handler((XDeviceMotionEvent *)eventPtr);
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    }
#endif
#ifdef IR_BUTTONS
    else if (dm_event_cmp(DMP, DM_BUTTON_PRESS, eventPtr->type) == 1) {
	buttons_event_handler((XDeviceButtonEvent *)eventPtr, 1);
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    } else if (dm_event_cmp(DMP, DM_BUTTON_RELEASE, eventPtr->type) == 1) {
	buttons_event_handler((XDeviceButtonEvent *)eventPtr, 0);
	dm_set_dirty(DMP, 1);

	/* no further processing of this event */
	status = TCL_RETURN;
    }
#endif

    if (save_dm_list)
	MGED_CK_STATE(s);
    set_curr_dm(s, save_dm_list);
    return status;
}
#else
int
doEvent(ClientData UNUSED(clientData), void *UNUSED(eventPtr)) {
    return TCL_OK;
}
#endif /* HAVE_X11_XLIB_H */


#if defined(IR_BUTTONS) || defined(IR_KNOBS)
static void
set_knob_offset()
{
    int i;

    for (i = 0; i < 8; ++i)
	dm_knobs[i] = 0;
}
#endif

#if defined(IR_BUTTONS) || defined(IR_KNOBS)
static void
common_dbtext(char *str)
{
    bu_log("common_dbtext: You pressed Help key and '%s'\n", str);
}
#endif


#ifdef HAVE_X11_TYPES
static void
motion_event_handler(struct mged_state *s, XMotionEvent *xmotion)
{
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    int save_edflag = -1;
    int mx, my;
    int dx, dy;
    int width, height;
    fastf_t f;
    fastf_t fx, fy;
    fastf_t td;

    if (s->dbip == DBI_NULL)
	return;

    width = dm_get_width(DMP);
    height = dm_get_height(DMP);
    mx = xmotion->x;
    my = xmotion->y;
    dx = mx - dm_omx;
    dy = my - dm_omy;

    switch (am_mode) {
	case AMM_IDLE:
	    if (scroll_active)
		bu_vls_printf(&cmd, "M 1 %d %d\n",
			      (int)(dm_Xx2Normal(DMP, mx) * BV_MAX),
			      (int)(dm_Xy2Normal(DMP, my, 0) * BV_MAX));
	    else if (rubber_band->rb_active) {
		fastf_t x = dm_Xx2Normal(DMP, mx);
		fastf_t y = dm_Xy2Normal(DMP, my, 1);

		if (grid_state->snap)
		    snap_to_grid(s, &x, &y);

		rubber_band->rb_width = x - rubber_band->rb_x;
		rubber_band->rb_height = y - rubber_band->rb_y;

		rect_view2image(s);
		/*
		 * Now go back the other way to reconcile
		 * differences caused by floating point fuzz.
		 */
		rect_image2view(s);

		{
		    /* need dummy values for func signature--they are unused in the func */
		    const struct bu_structparse *sdp = 0;
		    const char name[] = "name";
		    void *base = 0;
		    const char value[] = "value";
		    rb_set_dirty_flag(sdp, name, base, value, NULL);
		}

		goto handled;
	    } else if (doMotion)
		/* do the regular thing */
		/* Constant tracking (e.g. illuminate mode) bound to M mouse */
		bu_vls_printf(&cmd, "M 0 %d %d\n",
			      (int)(dm_Xx2Normal(DMP, mx) * BV_MAX),
			      (int)(dm_Xy2Normal(DMP, my, 0) * BV_MAX));
	    else /* not doing motion */
		goto handled;

	    break;
	case AMM_ROT:
	    {
		char save_coords;

		save_coords = mged_variables->mv_coords;
		mged_variables->mv_coords = 'v';

		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		    mged_variables->mv_transform == 'e') {

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_ROTATE)
			    es_edflag = SROT;
		    } else {
			save_edflag = edobj;
			edobj = BE_O_ROTATE;
		    }

		    if (mged_variables->mv_rateknobs)
			bu_vls_printf(&cmd, "knob -i x %lf y %lf\n",
				      dy / (fastf_t)height * RATE_ROT_FACTOR * 2.0,
				      dx / (fastf_t)width * RATE_ROT_FACTOR * 2.0);
		    else
			bu_vls_printf(&cmd, "knob -i ax %lf ay %lf\n",
				      dy * 0.25, dx * 0.25);
		} else {
		    if (mged_variables->mv_rateknobs)
			bu_vls_printf(&cmd, "knob -i -v x %lf y %lf\n",
				      dy / (fastf_t)height * RATE_ROT_FACTOR * 2.0,
				      dx / (fastf_t)width * RATE_ROT_FACTOR * 2.0);
		    else
			bu_vls_printf(&cmd, "knob -i -v ax %lf ay %lf\n",
				      dy * 0.25, dx * 0.25);
		}

		(void)Tcl_Eval(s->interp, bu_vls_addr(&cmd));
		mged_variables->mv_coords = save_coords;

		goto reset_edflag;
	    }
	case AMM_TRAN:
	    {
		char save_coords;

		save_coords = mged_variables->mv_coords;
		mged_variables->mv_coords = 'v';

		fx = dx / (fastf_t)width * 2.0;
		fy = -dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0;

		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		    mged_variables->mv_transform == 'e') {

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_TRAN)
			    es_edflag = STRANS;
		    } else {
			save_edflag = edobj;
			edobj = BE_O_XY;
		    }

		    if (mged_variables->mv_rateknobs)
			bu_vls_printf(&cmd, "knob -i X %lf Y %lf\n", fx, fy);
		    else if (grid_state->snap) {
			point_t view_pt;
			point_t model_pt;
			point_t vcenter, diff;

			/* accumulate distance mouse moved since starting to translate */
			dm_mouse_dx += dx;
			dm_mouse_dy += dy;

			view_pt[X] = dm_mouse_dx / (fastf_t)width * 2.0;
			view_pt[Y] = -dm_mouse_dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0;
			view_pt[Z] = 0.0;
			round_to_grid(s, &view_pt[X], &view_pt[Y]);

			MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
			MAT_DELTAS_GET_NEG(vcenter, view_state->vs_gvp->gv_center);
			VSUB2(diff, model_pt, vcenter);
			VSCALE(diff, diff, s->dbip->dbi_base2local);
			VADD2(model_pt, dm_work_pt, diff);
			if (GEOM_EDIT_STATE == ST_S_EDIT)
			    bu_vls_printf(&cmd, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
			else
			    bu_vls_printf(&cmd, "translate %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
		    } else
			bu_vls_printf(&cmd, "knob -i aX %lf aY %lf\n",
				      fx*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local, fy*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);
		} else {
		    if (mged_variables->mv_rateknobs)      /* otherwise, drag to translate the view */
			bu_vls_printf(&cmd, "knob -i -v X %lf Y %lf\n", fx, fy);
		    else {
			if (grid_state->snap) {
			    /* accumulate distance mouse moved since starting to translate */
			    dm_mouse_dx += dx;
			    dm_mouse_dy += dy;

			    snap_view_to_grid(s, dm_mouse_dx / (fastf_t)width * 2.0,
					      -dm_mouse_dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0);

			    mged_variables->mv_coords = save_coords;
			    goto handled;
			} else
			    bu_vls_printf(&cmd, "knob -i -v aX %lf aY %lf\n",
					  fx*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local, fy*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);
		    }
		}

		(void)Tcl_Eval(s->interp, bu_vls_addr(&cmd));
		mged_variables->mv_coords = save_coords;

		goto reset_edflag;
	    }
	case AMM_SCALE:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT && !SEDIT_SCALE) {
		    save_edflag = es_edflag;
		    es_edflag = SSCALE;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_SCALE) {
		    save_edflag = edobj;
		    edobj = BE_O_SCALE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i S %f\n", f / height);
	    else
		bu_vls_printf(&cmd, "knob -i aS %f\n", f / height);

	    break;
	case AMM_ADC_ANG1:
	    fx = dm_Xx2Normal(DMP, mx) * BV_MAX - adc_state->adc_dv_x;
	    fy = dm_Xy2Normal(DMP, my, 1) * BV_MAX - adc_state->adc_dv_y;
	    bu_vls_printf(&cmd, "adc a1 %lf\n", RAD2DEG*atan2(fy, fx));

	    break;
	case AMM_ADC_ANG2:
	    fx = dm_Xx2Normal(DMP, mx) * BV_MAX - adc_state->adc_dv_x;
	    fy = dm_Xy2Normal(DMP, my, 1) * BV_MAX - adc_state->adc_dv_y;
	    bu_vls_printf(&cmd, "adc a2 %lf\n", RAD2DEG*atan2(fy, fx));

	    break;
	case AMM_ADC_TRAN:
	    {
		point_t model_pt;
		point_t view_pt;

		VSET(view_pt, dm_Xx2Normal(DMP, mx), dm_Xy2Normal(DMP, my, 1), 0.0);

		if (grid_state->snap)
		    snap_to_grid(s, &view_pt[X], &view_pt[Y]);

		MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
		VSCALE(model_pt, model_pt, s->dbip->dbi_base2local);
		bu_vls_printf(&cmd, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
	    }

	    break;
	case AMM_ADC_DIST:
	    fx = (dm_Xx2Normal(DMP, mx) * BV_MAX - adc_state->adc_dv_x) * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local * INV_BV;
	    fy = (dm_Xy2Normal(DMP, my, 1) * BV_MAX - adc_state->adc_dv_y) * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local * INV_BV;
	    td = sqrt(fx * fx + fy * fy);
	    bu_vls_printf(&cmd, "adc dst %lf\n", td);

	    break;
	case AMM_CON_ROT_X:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_ROTATE)
			es_edflag = SROT;
		} else {
		    save_edflag = edobj;
		    edobj = BE_O_ROTATE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i x %f\n",
			      f / (fastf_t)width * RATE_ROT_FACTOR * 2.0);
	    else
		bu_vls_printf(&cmd, "knob -i ax %f\n", f * 0.25);

	    break;
	case AMM_CON_ROT_Y:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_ROTATE)
			es_edflag = SROT;
		} else {
		    save_edflag = edobj;
		    edobj = BE_O_ROTATE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i y %f\n",
			      f / (fastf_t)width * RATE_ROT_FACTOR * 2.0);
	    else
		bu_vls_printf(&cmd, "knob -i ay %f\n", f * 0.25);

	    break;
	case AMM_CON_ROT_Z:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_ROTATE)
			es_edflag = SROT;
		} else {
		    save_edflag = edobj;
		    edobj = BE_O_ROTATE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i z %f\n",
			      f / (fastf_t)width * RATE_ROT_FACTOR * 2.0);
	    else
		bu_vls_printf(&cmd, "knob -i az %f\n", f * 0.25);

	    break;
	case AMM_CON_TRAN_X:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
		    save_edflag = edobj;
		    edobj = BE_O_X;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx / (fastf_t)width * 2.0;
	    else
		f = -dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i X %f\n", f);
	    else
		bu_vls_printf(&cmd, "knob -i aX %f\n", f*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);

	    break;
	case AMM_CON_TRAN_Y:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
		    save_edflag = edobj;
		    edobj = BE_O_Y;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx / (fastf_t)width * 2.0;
	    else
		f = -dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i Y %f\n", f);
	    else
		bu_vls_printf(&cmd, "knob -i aY %f\n", f*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);

	    break;
	case AMM_CON_TRAN_Z:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
		    save_edflag = edobj;
		    edobj = BE_O_XY;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx / (fastf_t)width * 2.0;
	    else
		f = -dy / height / dm_get_aspect(DMP) * 2.0;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i Z %f\n", f);
	    else
		bu_vls_printf(&cmd, "knob -i aZ %f\n", f*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);

	    break;
	case AMM_CON_SCALE_X:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_SCALE) {
		    save_edflag = edobj;
		    edobj = BE_O_XSCALE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i S %f\n", f / height);
	    else
		bu_vls_printf(&cmd, "knob -i aS %f\n", f / height);

	    break;
	case AMM_CON_SCALE_Y:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_SCALE) {
		    save_edflag = edobj;
		    edobj = BE_O_YSCALE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i S %f\n", f / height);
	    else
		bu_vls_printf(&cmd, "knob -i aS %f\n", f / height);

	    break;
	case AMM_CON_SCALE_Z:
	    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT) &&
		mged_variables->mv_transform == 'e') {
		if (GEOM_EDIT_STATE == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_SCALE) {
		    save_edflag = edobj;
		    edobj = BE_O_ZSCALE;
		}
	    }

	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    if (mged_variables->mv_rateknobs)
		bu_vls_printf(&cmd, "knob -i S %f\n", f / height);
	    else
		bu_vls_printf(&cmd, "knob -i aS %f\n", f / height);

	    break;
	case AMM_CON_XADC:
	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    bu_vls_printf(&cmd, "knob -i xadc %f\n",
			  f / (fastf_t)width * BV_RANGE);
	    break;
	case AMM_CON_YADC:
	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    bu_vls_printf(&cmd, "knob -i yadc %f\n",
			  f / (fastf_t)height * BV_RANGE);
	    break;
	case AMM_CON_ANG1:
	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    bu_vls_printf(&cmd, "knob -i ang1 %f\n",
			  f / (fastf_t)width * 90.0);
	    break;
	case AMM_CON_ANG2:
	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    bu_vls_printf(&cmd, "knob -i ang2 %f\n",
			  f / (fastf_t)width * 90.0);
	    break;
	case AMM_CON_DIST:
	    if (abs(dx) >= abs(dy))
		f = dx;
	    else
		f = -dy;

	    bu_vls_printf(&cmd, "knob -i distadc %f\n",
			  f / (fastf_t)width * BV_RANGE);
	    break;
    }

    (void)Tcl_Eval(s->interp, bu_vls_addr(&cmd));

 reset_edflag:
    if (save_edflag != -1) {
	if (GEOM_EDIT_STATE == ST_S_EDIT)
	    es_edflag = save_edflag;
	else if (GEOM_EDIT_STATE == ST_O_EDIT)
	    edobj = save_edflag;
    }

 handled:
    bu_vls_free(&cmd);
    dm_omx = mx;
    dm_omy = my;
}
#endif /* HAVE_X11_XLIB_H */


#ifdef IR_KNOBS
static void
dials_event_handler(XDeviceMotionEvent *dmep)
{
    static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    int save_edflag = -1;
    int setting;
    fastf_t f;

    if (s->dbip == DBI_NULL)
	return;

    if (button0) {
	common_dbtext((adc_state->adc_draw ? kn1_knobs:kn2_knobs)[dmep->first_axis]);
	return;
    }

    switch (DIAL0 + dmep->first_axis) {
	case DIAL0:
	    if (adc_state->adc_draw) {
		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE &&
		    !adc_state->adc_dv_a1)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit(adc_state->adc_dv_a1) + dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob ang1 %f\n",
			      45.0 - 45.0*((double)setting) * INV_BV);
	    } else {
		if (mged_variables->mv_rateknobs) {
		    f = view_state->vs_rate_model_rotate[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob -m z %f\n", setting / 512.0);
		} else {
		    f = view_state->vs_absolute_model_rotate[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(2.847 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    f = dm_limit(dm_knobs[dmep->first_axis]) / 512.0;
		    bu_vls_printf(&cmd, "knob -m az %f\n", dm_wrap(f) * 180.0);
		}
	    }
	    break;
	case DIAL1:
	    if (mged_variables->mv_rateknobs) {
		if (EDIT_SCALE && mged_variables->mv_transform == 'e')
		    f = edit_rate_scale;
		else
		    f = view_state->vs_rate_scale;

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] = dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob S %f\n", setting / 512.0);
	    } else {
		if (EDIT_SCALE && mged_variables->mv_transform == 'e')
		    f = edit_absolute_scale;
		else
		    f = view_state->vs_absolute_scale;

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob aS %f\n", setting / 512.0);
	    }
	    break;
	case DIAL2:
	    if (adc_state->adc_draw) {
		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE &&
		    !adc_state->adc_dv_a2)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit(adc_state->adc_dv_a2) + dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob ang2 %f\n",
			      45.0 - 45.0*((double)setting) * INV_BV);
	    } else {
		if (mged_variables->mv_rateknobs) {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_rate_model_rotate[Z];
				break;
			    case 'o':
				f = edit_rate_object_rotate[Z];
				break;
			    case 'v':
			    default:
				f = edit_rate_view_rotate[Z];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_rate_model_rotate[Z];
		    else
			f = view_state->vs_rate_rotate[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob z %f\n", setting / 512.0);
		} else {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_absolute_model_rotate[Z];
				break;
			    case 'o':
				f = edit_absolute_object_rotate[Z];
				break;
			    case 'v':
			    default:
				f = edit_absolute_view_rotate[Z];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_absolute_model_rotate[Z];
		    else
			f = view_state->vs_absolute_rotate[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(2.847 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    f = dm_limit(dm_knobs[dmep->first_axis]) / 512.0;
		    bu_vls_printf(&cmd, "knob az %f\n", dm_wrap(f) * 180.0);
		}
	    }
	    break;
	case DIAL3:
	    if (adc_state->adc_draw) {
		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE &&
		    !adc_state->adc_dv_dist)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit(adc_state->adc_dv_dist) + dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob distadc %d\n", setting);
	    } else {
		if (mged_variables->mv_rateknobs) {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
			    case 'o':
				f = edit_rate_model_tran[Z];
				break;
			    case 'v':
			    default:
				f = edit_rate_view_tran[Z];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_TRAN)
				es_edflag = STRANS;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			    save_edflag = edobj;
			    edobj = BE_O_XY;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_rate_model_tran[Z];
		    else
			f = view_state->vs_rate_tran[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob Z %f\n", setting / 512.0);
		} else {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
			    case 'o':
				f = edit_absolute_model_tran[Z];
				break;
			    case 'v':
			    default:
				f = edit_absolute_view_tran[Z];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_TRAN)
				es_edflag = STRANS;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			    save_edflag = edobj;
			    edobj = BE_O_XY;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_absolute_model_tran[Z];
		    else
			f = view_state->vs_absolute_tran[Z];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE &&
			!f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob aZ %f\n", setting / 512.0 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
		}
	    }
	    break;
	case DIAL4:
	    if (adc_state->adc_draw) {
		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE &&
		    !adc_state->adc_dv_y)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit(adc_state->adc_dv_y) + dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob yadc %d\n", setting);
	    } else {
		if (mged_variables->mv_rateknobs) {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_rate_model_rotate[Y];
				break;
			    case 'o':
				f = edit_rate_object_rotate[Y];
				break;
			    case 'v':
			    default:
				f = edit_rate_view_rotate[Y];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_rate_model_rotate[Y];
		    else
			f = view_state->vs_rate_rotate[Y];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob y %f\n", setting / 512.0);
		} else {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_absolute_model_rotate[Y];
				break;
			    case 'o':
				f = edit_absolute_object_rotate[Y];
				break;
			    case 'v':
			    default:
				f = edit_absolute_view_rotate[Y];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_absolute_model_rotate[Y];
		    else
			f = view_state->vs_absolute_rotate[Y];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE &&
			!f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(2.847 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    f = dm_limit(dm_knobs[dmep->first_axis]) / 512.0;
		    bu_vls_printf(&cmd, "knob ay %f\n", dm_wrap(f) * 180.0);
		}
	    }
	    break;
	case DIAL5:
	    if (mged_variables->mv_rateknobs) {
		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
		    && mged_variables->mv_transform == 'e') {
		    switch (mged_variables->mv_coords) {
			case 'm':
			case 'o':
			    f = edit_rate_model_tran[Y];
			    break;
			case 'v':
			default:
			    f = edit_rate_view_tran[Y];
			    break;
		    }

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_TRAN)
			    es_edflag = STRANS;
		    } else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			save_edflag = edobj;
			edobj = BE_O_XY;
		    }
		} else if (mged_variables->mv_coords == 'm')
		    f = view_state->vs_rate_model_tran[Y];
		else
		    f = view_state->vs_rate_tran[Y];

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob Y %f\n", setting / 512.0);
	    } else {
		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
		    && mged_variables->mv_transform == 'e') {
		    switch (mged_variables->mv_coords) {
			case 'm':
			case 'o':
			    f = edit_absolute_model_tran[Y];
			    break;
			case 'v':
			default:
			    f = edit_absolute_view_tran[Y];
			    break;
		    }

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_TRAN)
			    es_edflag = STRANS;
		    } else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			save_edflag = edobj;
			edobj = BE_O_XY;
		    }
		} else if (mged_variables->mv_coords == 'm')
		    f = view_state->vs_absolute_model_tran[Y];
		else
		    f = view_state->vs_absolute_tran[Y];

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] -
			knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob aY %f\n", setting / 512.0 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
	    }
	    break;
	case DIAL6:
	    if (adc_state->adc_draw) {
		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE &&
		    !adc_state->adc_dv_x)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit(adc_state->adc_dv_x) + dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob xadc %d\n", setting);
	    } else {
		if (mged_variables->mv_rateknobs) {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_rate_model_rotate[X];
				break;
			    case 'o':
				f = edit_rate_object_rotate[X];
				break;
			    case 'v':
			    default:
				f = edit_rate_view_rotate[X];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_rate_model_rotate[X];
		    else
			f = view_state->vs_rate_rotate[X];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(512.5 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    setting = dm_limit(dm_knobs[dmep->first_axis]);
		    bu_vls_printf(&cmd, "knob x %f\n", setting / 512.0);
		} else {
		    if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
			&& mged_variables->mv_transform == 'e') {
			switch (mged_variables->mv_coords) {
			    case 'm':
				f = edit_absolute_model_rotate[X];
				break;
			    case 'o':
				f = edit_absolute_object_rotate[X];
				break;
			    case 'v':
			    default:
				f = edit_absolute_view_rotate[X];
				break;
			}

			if (GEOM_EDIT_STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_ROTATE)
				es_edflag = SROT;
			} else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_ROTATE) {
			    save_edflag = edobj;
			    edobj = BE_O_ROTATE;
			}
		    } else if (mged_variables->mv_coords == 'm')
			f = view_state->vs_absolute_model_rotate[X];
		    else
			f = view_state->vs_absolute_rotate[X];

		    if (-NOISE <= dm_knobs[dmep->first_axis] &&
			dm_knobs[dmep->first_axis] <= NOISE && !f)
			dm_knobs[dmep->first_axis] +=
			    dmep->axis_data[0] - knob_values[dmep->first_axis];
		    else
			dm_knobs[dmep->first_axis] =
			    dm_unlimit((int)(2.847 * f)) +
			    dmep->axis_data[0] - knob_values[dmep->first_axis];

		    f = dm_limit(dm_knobs[dmep->first_axis]) / 512.0;
		    bu_vls_printf(&cmd, "knob ax %f\n", dm_wrap(f) * 180.0);
		}
	    }
	    break;
	case DIAL7:
	    if (mged_variables->mv_rateknobs) {
		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
		    && mged_variables->mv_transform == 'e') {
		    switch (mged_variables->mv_coords) {
			case 'm':
			case 'o':
			    f = edit_rate_model_tran[X];
			    break;
			case 'v':
			default:
			    f = edit_rate_view_tran[X];
			    break;
		    }

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_TRAN)
			    es_edflag = STRANS;
		    } else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			save_edflag = edobj;
			edobj = BE_O_XY;
		    }
		} else if (mged_variables->mv_coords == 'm')
		    f = view_state->vs_rate_model_tran[X];
		else
		    f = view_state->vs_rate_tran[X];

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob X %f\n", setting / 512.0);
	    } else {
		if ((GEOM_EDIT_STATE == ST_S_EDIT || GEOM_EDIT_STATE == ST_O_EDIT)
		    && mged_variables->mv_transform == 'e') {
		    switch (mged_variables->mv_coords) {
			case 'm':
			case 'o':
			    f = edit_absolute_model_tran[X];
			    break;
			case 'v':
			default:
			    f = edit_absolute_view_tran[X];
			    break;
		    }

		    if (GEOM_EDIT_STATE == ST_S_EDIT) {
			save_edflag = es_edflag;
			if (!SEDIT_TRAN)
			    es_edflag = STRANS;
		    } else if (GEOM_EDIT_STATE == ST_O_EDIT && !OEDIT_TRAN) {
			save_edflag = edobj;
			edobj = BE_O_XY;
		    }
		} else if (mged_variables->mv_coords == 'm')
		    f = view_state->vs_absolute_model_tran[X];
		else
		    f = view_state->vs_absolute_tran[X];

		if (-NOISE <= dm_knobs[dmep->first_axis] &&
		    dm_knobs[dmep->first_axis] <= NOISE && !f)
		    dm_knobs[dmep->first_axis] +=
			dmep->axis_data[0] - knob_values[dmep->first_axis];
		else
		    dm_knobs[dmep->first_axis] =
			dm_unlimit((int)(512.5 * f)) +
			dmep->axis_data[0] - knob_values[dmep->first_axis];

		setting = dm_limit(dm_knobs[dmep->first_axis]);
		bu_vls_printf(&cmd, "knob aX %f\n", setting / 512.0 * view_state->vs_gvp->gv_scale * s->dbip->dbi_base2local);
	    }
	    break;
	default:
	    break;
    }

    /* Keep track of the knob values */
    knob_values[dmep->first_axis] = dmep->axis_data[0];

    Tcl_Eval(s->interp, bu_vls_addr(&cmd));

    if (save_edflag != -1) {
	if (GEOM_EDIT_STATE == ST_S_EDIT)
	    es_edflag = save_edflag;
	else if (GEOM_EDIT_STATE == ST_O_EDIT)
	    edobj = save_edflag;
    }

    bu_vls_free(&cmd);
}
#endif

#ifdef IR_BUTTONS
static void
buttons_event_handler(XDeviceButtonEvent *dbep, int press)
{
    if (press) {
	struct bu_vls cmd = BU_VLS_INIT_ZERO;

	if (dbep->button == 1) {
	    button0 = 1;
	    return;
	}

	if (button0) {
	    common_dbtext(label_button(bmap[dbep->button - 1]));
	    return;
	} else if (dbep->button == 4) {
	    set_knob_offset();

	    bu_vls_strcat(&cmd, "knob zero\n");
	} else {
	    bu_vls_printf(&cmd, "press %s\n",
			  label_button(bmap[dbep->button - 1]));
	}

	(void)Tcl_Eval(s->interp, bu_vls_addr(&cmd));
	bu_vls_free(&cmd);
    } else {
	if (dbep->button == 1)
	    button0 = 0;
    }
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
