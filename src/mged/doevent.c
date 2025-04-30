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

/* Ew. Global. */
extern int doMotion;			/* defined in buttons.c */

#ifdef HAVE_X11_TYPES
static void motion_event_handler(struct mged_state *, XMotionEvent *);
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


#ifdef HAVE_X11_TYPES
static void
motion_event_handler(struct mged_state *s, XMotionEvent *xmotion)
{
    struct bu_vls cmd = BU_VLS_INIT_ZERO;
    int save_edflag = -1;
    fastf_t f;
    fastf_t fx, fy;
    fastf_t td;
    int edit_mode = ((s->edit_state.global_editing_state == ST_S_EDIT || s->edit_state.global_editing_state == ST_O_EDIT) && mged_variables->mv_transform == 'e') ? 1 : 0;

    if (s->dbip == DBI_NULL)
	return;

    int width = dm_get_width(DMP);
    int height = dm_get_height(DMP);
    int mx = xmotion->x;
    int my = xmotion->y;
    int dx = mx - dm_omx;
    int dy = my - dm_omy;

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

		save_coords = view_state->vs_gvp->gv_coord;
		view_state->vs_gvp->gv_coord = 'v';

		if (edit_mode) {

		    if (s->edit_state.global_editing_state == ST_S_EDIT) {
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
		view_state->vs_gvp->gv_coord = save_coords;

		goto reset_edflag;
	    }
	case AMM_TRAN:
	    {
		char save_coords;

		save_coords = view_state->vs_gvp->gv_coord;
		view_state->vs_gvp->gv_coord = 'v';

		fx = dx / (fastf_t)width * 2.0;
		fy = -dy / (fastf_t)height / dm_get_aspect(DMP) * 2.0;

		if (edit_mode) {

		    if (s->edit_state.global_editing_state == ST_S_EDIT) {
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
			if (s->edit_state.global_editing_state == ST_S_EDIT)
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

			    view_state->vs_gvp->gv_coord = save_coords;
			    goto handled;
			} else
			    bu_vls_printf(&cmd, "knob -i -v aX %lf aY %lf\n",
					  fx*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local, fy*view_state->vs_gvp->gv_scale*s->dbip->dbi_base2local);
		    }
		}

		(void)Tcl_Eval(s->interp, bu_vls_addr(&cmd));
		view_state->vs_gvp->gv_coord = save_coords;

		goto reset_edflag;
	    }
	case AMM_SCALE:
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT && !SEDIT_SCALE) {
		    save_edflag = es_edflag;
		    es_edflag = SSCALE;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_SCALE) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_TRAN) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_TRAN) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_TRAN)
			es_edflag = STRANS;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_TRAN) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_SCALE) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_SCALE) {
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
	    if (edit_mode) {
		if (s->edit_state.global_editing_state == ST_S_EDIT) {
		    save_edflag = es_edflag;
		    if (!SEDIT_SCALE)
			es_edflag = SSCALE;
		} else if (s->edit_state.global_editing_state == ST_O_EDIT && !OEDIT_SCALE) {
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
	if (s->edit_state.global_editing_state == ST_S_EDIT)
	    es_edflag = save_edflag;
	else if (s->edit_state.global_editing_state == ST_O_EDIT)
	    edobj = save_edflag;
    }

 handled:
    bu_vls_free(&cmd);
    dm_omx = mx;
    dm_omy = my;
}
#endif /* HAVE_X11_XLIB_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
