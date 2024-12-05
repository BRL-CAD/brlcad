/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @file mged/dm-generic.c
 *
 * Routines common to MGED's interface to LIBDM.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "tcl.h"
#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "vmath.h"
#include "raytrace.h"
#include "ged.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;

int
common_dm(struct mged_state *s, int argc, const char *argv[])
{
    int status;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (DBIP == DBI_NULL)
	return TCL_OK;

    if (BU_STR_EQUAL(argv[0], "idle")) {

	/* redraw after scaling */
	if (s->GEDP && s->GEDP->ged_gvp &&
	    s->GEDP->ged_gvp->gv_s->adaptive_plot_csg &&
	    s->GEDP->ged_gvp->gv_s->redraw_on_zoom &&
	    (am_mode == AMM_SCALE ||
	     am_mode == AMM_CON_SCALE_X ||
	     am_mode == AMM_CON_SCALE_Y ||
	     am_mode == AMM_CON_SCALE_Z))
	{
	    if (redraw_visible_objects(s) == TCL_ERROR) {
		return TCL_ERROR;
	    }
	}

	am_mode = AMM_IDLE;
	scroll_active = 0;
	if (rubber_band->rb_active) {
	    rubber_band->rb_active = 0;

	    if (mged_variables->mv_mouse_behavior == 'p') {
		/* need dummy values for func signature--they are unused in the func */
		const struct bu_structparse *sdp = 0;
		const char name[] = "name";
		void *base = 0;
		const char value[] = "value";
		rb_set_dirty_flag(sdp, name, base, value, NULL);
	    }
	    else if (mged_variables->mv_mouse_behavior == 'r')
		rt_rect_area();
	    else if (mged_variables->mv_mouse_behavior == 'z')
		zoom_rect_area(s);
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[0], "m")) {
	int x;
	int y;
	int old_orig_gui;
	int stolen = 0;
	fastf_t fx, fy;

	if (argc < 3) {
	    Tcl_AppendResult(INTERP, "dm m: need more parameters\n",
			     "dm m xpos ypos\n", (char *)NULL);
	    return TCL_ERROR;
	}

	old_orig_gui = mged_variables->mv_orig_gui;

	fx = dm_Xx2Normal(DMP, atoi(argv[1]));
	fy = dm_Xy2Normal(DMP, atoi(argv[2]), 0);
	x = fx * GED_MAX;
	y = fy * GED_MAX;

	if (mged_variables->mv_faceplate &&
	    mged_variables->mv_orig_gui) {
#define MENUXLIM (-1250)

	    if (x >= MENUXLIM && scroll_select(x, y, 0)) {
		stolen = 1;
		goto end;
	    }

	    if (x < MENUXLIM && mmenu_select(s, y, 0)) {
		stolen = 1;
		goto end;
	    }
	}

	mged_variables->mv_orig_gui = 0;
	fy = dm_Xy2Normal(DMP, atoi(argv[2]), 1);
	y = fy * GED_MAX;

    end:
	if (mged_variables->mv_mouse_behavior == 'q' && !stolen) {
	    point_t view_pt;
	    point_t model_pt;

	    if (grid_state->snap)
		snap_to_grid(&fx, &fy);

	    if (mged_variables->mv_perspective_mode)
		VSET(view_pt, fx, fy, 0.0);
	    else
		VSET(view_pt, fx, fy, 1.0);

	    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    if (dm_get_zclip(DMP))
		bu_vls_printf(&vls, "qray_nirt %lf %lf %lf",
			      model_pt[X], model_pt[Y], model_pt[Z]);
	    else
		bu_vls_printf(&vls, "qray_nirt -b %lf %lf %lf",
			      model_pt[X], model_pt[Y], model_pt[Z]);
	} else if ((mged_variables->mv_mouse_behavior == 'p' ||
		    mged_variables->mv_mouse_behavior == 'r' ||
		    mged_variables->mv_mouse_behavior == 'z') && !stolen) {

	    if (grid_state->snap)
		snap_to_grid(&fx, &fy);

	    rubber_band->rb_active = 1;
	    rubber_band->rb_x = fx;
	    rubber_band->rb_y = fy;
	    rubber_band->rb_width = 0.0;
	    rubber_band->rb_height = 0.0;
	    rect_view2image();
	    {
		/* need dummy values for func signature--they are unused in the func */
		const struct bu_structparse *sdp = 0;
		const char name[] = "name";
		void *base = 0;
		const char value[] = "value";
		rb_set_dirty_flag(sdp, name, base, value, NULL);
	    }
	} else if (mged_variables->mv_mouse_behavior == 's' && !stolen) {
	    bu_vls_printf(&vls, "mouse_solid_edit_select %d %d", x, y);
	} else if (mged_variables->mv_mouse_behavior == 'm' && !stolen) {
	    bu_vls_printf(&vls, "mouse_matrix_edit_select %d %d", x, y);
	} else if (mged_variables->mv_mouse_behavior == 'c' && !stolen) {
	    bu_vls_printf(&vls, "mouse_comb_edit_select %d %d", x, y);
	} else if (mged_variables->mv_mouse_behavior == 'o' && !stolen) {
	    bu_vls_printf(&vls, "mouse_rt_obj_select %d %d", x, y);
	} else if (adc_state->adc_draw && mged_variables->mv_transform == 'a' && !stolen) {
	    point_t model_pt;
	    point_t view_pt;

	    if (grid_state->snap)
		snap_to_grid(&fx, &fy);

	    VSET(view_pt, fx, fy, 1.0);
	    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
	} else if (grid_state->snap && !stolen &&
		   SEDIT_TRAN && mged_variables->mv_transform == 'e') {
	    point_t view_pt;
	    point_t model_pt;

	    snap_to_grid(&fx, &fy);
	    MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	} else if (grid_state->snap && !stolen &&
		   OEDIT_TRAN && mged_variables->mv_transform == 'e') {
	    point_t view_pt;
	    point_t model_pt;

	    snap_to_grid(&fx, &fy);
	    MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, curr_e_axes_pos);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "translate %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	} else if (grid_state->snap && !stolen &&
		   STATE != ST_S_PICK && STATE != ST_O_PICK &&
		   STATE != ST_O_PATH && !SEDIT_PICK && !EDIT_SCALE) {
	    point_t view_pt;
	    point_t model_pt;
	    point_t vcenter;

	    snap_to_grid(&fx, &fy);
	    MAT_DELTAS_GET_NEG(vcenter, view_state->vs_gvp->gv_center);
	    MAT4X3PNT(view_pt, view_state->vs_gvp->gv_model2view, vcenter);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "center %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	} else
	    bu_vls_printf(&vls, "M 1 %d %d\n", x, y);

	status = Tcl_Eval(INTERP, bu_vls_addr(&vls));
	mged_variables->mv_orig_gui = old_orig_gui;
	bu_vls_free(&vls);

	return status;
    }

    if (BU_STR_EQUAL(argv[0], "am")) {
	if (argc < 4) {
	    Tcl_AppendResult(INTERP, "dm am: need more parameters\n",
			     "dm am <r|t|s> xpos ypos\n", (char *)NULL);
	    return TCL_ERROR;
	}

	dm_omx = atoi(argv[2]);
	dm_omy = atoi(argv[3]);

	switch (*argv[1]) {
	    case 'r':
		am_mode = AMM_ROT;
		break;
	    case 't':
		am_mode = AMM_TRAN;

		if (grid_state->snap) {
		    int save_edflag;

		    if ((STATE == ST_S_EDIT || STATE == ST_O_EDIT) &&
			mged_variables->mv_transform == 'e') {
			if (STATE == ST_S_EDIT) {
			    save_edflag = es_edflag;
			    if (!SEDIT_TRAN)
				es_edflag = STRANS;
			} else {
			    save_edflag = edobj;
			    edobj = BE_O_XY;
			}

			snap_keypoint_to_grid();

			if (STATE == ST_S_EDIT)
			    es_edflag = save_edflag;
			else
			    edobj = save_edflag;
		    } else
			snap_view_center_to_grid();
		}

		break;
	    case 's':
		if (STATE == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
		    ZERO(acc_sc_sol))
		    acc_sc_sol = 1.0;
		else if (STATE == ST_O_EDIT && mged_variables->mv_transform == 'e') {
		    edit_absolute_scale = acc_sc_obj - 1.0;
		    if (edit_absolute_scale > 0.0)
			edit_absolute_scale /= 3.0;
		}

		am_mode = AMM_SCALE;
		break;
	    default:
		Tcl_AppendResult(INTERP, "dm am: need more parameters\n",
				 "dm am <r|t|s> xpos ypos\n", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[0], "adc")) {
	fastf_t fx, fy;
	fastf_t td; /* tick distance */

	if (argc < 4) {
	    Tcl_AppendResult(INTERP, "dm adc: need more parameters\n",
			     "dm adc 1|2|t|d xpos ypos\n", (char *)NULL);
	    return TCL_ERROR;
	}

	dm_omx = atoi(argv[2]);
	dm_omy = atoi(argv[3]);

	switch (*argv[1]) {
	    case '1':
		fx = dm_Xx2Normal(DMP, dm_omx) * GED_MAX - adc_state->adc_dv_x;
		fy = dm_Xy2Normal(DMP, dm_omy, 1) * GED_MAX - adc_state->adc_dv_y;

		bu_vls_printf(&vls, "adc a1 %lf\n", RAD2DEG*atan2(fy, fx));
		Tcl_Eval(INTERP, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		am_mode = AMM_ADC_ANG1;
		break;
	    case '2':
		fx = dm_Xx2Normal(DMP, dm_omx) * GED_MAX - adc_state->adc_dv_x;
		fy = dm_Xy2Normal(DMP, dm_omy, 1) * GED_MAX - adc_state->adc_dv_y;

		bu_vls_printf(&vls, "adc a2 %lf\n", RAD2DEG*atan2(fy, fx));
		Tcl_Eval(INTERP, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		am_mode = AMM_ADC_ANG2;
		break;
	    case 't':
		{
		    point_t model_pt;
		    point_t view_pt;

		    VSET(view_pt, dm_Xx2Normal(DMP, dm_omx), dm_Xy2Normal(DMP, dm_omy, 1), 0.0);

		    if (grid_state->snap)
			snap_to_grid(&view_pt[X], &view_pt[Y]);

		    MAT4X3PNT(model_pt, view_state->vs_gvp->gv_view2model, view_pt);
		    VSCALE(model_pt, model_pt, base2local);

		    bu_vls_printf(&vls, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
		    Tcl_Eval(INTERP, bu_vls_addr(&vls));

		    bu_vls_free(&vls);
		    am_mode = AMM_ADC_TRAN;
		}

		break;
	    case 'd':
		fx = (dm_Xx2Normal(DMP, dm_omx) * GED_MAX -
		      adc_state->adc_dv_x) * view_state->vs_gvp->gv_scale * base2local * INV_GED;
		fy = (dm_Xy2Normal(DMP, dm_omy, 1) * GED_MAX -
		      adc_state->adc_dv_y) * view_state->vs_gvp->gv_scale * base2local * INV_GED;

		td = sqrt(fx * fx + fy * fy);

		bu_vls_printf(&vls, "adc dst %lf\n", td);
		Tcl_Eval(INTERP, bu_vls_addr(&vls));
		bu_vls_free(&vls);

		am_mode = AMM_ADC_DIST;
		break;
	    default:
		Tcl_AppendResult(INTERP, "dm adc: unrecognized parameter - ", argv[1],
				 "\ndm adc 1|2|t|d xpos ypos\n", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[0], "con")) {
	if (argc < 5) {
	    Tcl_AppendResult(INTERP, "dm con: need more parameters\n",
			     "dm con r|t|s x|y|z xpos ypos\n",
			     "dm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
	    return TCL_ERROR;
	}

	dm_omx = atoi(argv[3]);
	dm_omy = atoi(argv[4]);

	switch (*argv[1]) {
	    case 'a':
		switch (*argv[2]) {
		    case 'x':
			am_mode = AMM_CON_XADC;
			break;
		    case 'y':
			am_mode = AMM_CON_YADC;
			break;
		    case '1':
			am_mode = AMM_CON_ANG1;
			break;
		    case '2':
			am_mode = AMM_CON_ANG2;
			break;
		    case 'd':
			am_mode = AMM_CON_DIST;
			break;
		    default:
			Tcl_AppendResult(INTERP, "dm con: unrecognized parameter - ", argv[2],
					 "\ndm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
		}
		break;
	    case 'r':
		switch (*argv[2]) {
		    case 'x':
			am_mode = AMM_CON_ROT_X;
			break;
		    case 'y':
			am_mode = AMM_CON_ROT_Y;
			break;
		    case 'z':
			am_mode = AMM_CON_ROT_Z;
			break;
		    default:
			Tcl_AppendResult(INTERP, "dm con: unrecognized parameter - ", argv[2],
					 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
			return TCL_ERROR;
		}
		break;
	    case 't':
		switch (*argv[2]) {
		    case 'x':
			am_mode = AMM_CON_TRAN_X;
			break;
		    case 'y':
			am_mode = AMM_CON_TRAN_Y;
			break;
		    case 'z':
			am_mode = AMM_CON_TRAN_Z;
			break;
		    default:
			Tcl_AppendResult(INTERP, "dm con: unrecognized parameter - ", argv[2],
					 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
			return TCL_ERROR;
		}
		break;
	    case 's':
		switch (*argv[2]) {
		    case 'x':
			if (STATE == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
			    ZERO(acc_sc_sol))
			    acc_sc_sol = 1.0;
			else if (STATE == ST_O_EDIT && mged_variables->mv_transform == 'e') {
			    edit_absolute_scale = acc_sc[0] - 1.0;
			    if (edit_absolute_scale > 0.0)
				edit_absolute_scale /= 3.0;
			}

			am_mode = AMM_CON_SCALE_X;
			break;
		    case 'y':
			if (STATE == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
			    ZERO(acc_sc_sol))
			    acc_sc_sol = 1.0;
			else if (STATE == ST_O_EDIT && mged_variables->mv_transform == 'e') {
			    edit_absolute_scale = acc_sc[1] - 1.0;
			    if (edit_absolute_scale > 0.0)
				edit_absolute_scale /= 3.0;
			}

			am_mode = AMM_CON_SCALE_Y;
			break;
		    case 'z':
			if (STATE == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
			    ZERO(acc_sc_sol))
			    acc_sc_sol = 1.0;
			else if (STATE == ST_O_EDIT && mged_variables->mv_transform == 'e') {
			    edit_absolute_scale = acc_sc[2] - 1.0;
			    if (edit_absolute_scale > 0.0)
				edit_absolute_scale /= 3.0;
			}

			am_mode = AMM_CON_SCALE_Z;
			break;
		    default:
			Tcl_AppendResult(INTERP, "dm con: unrecognized parameter - ", argv[2],
					 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
			return TCL_ERROR;
		}
		break;
	    default:
		Tcl_AppendResult(INTERP, "dm con: unrecognized parameter - ", argv[1],
				 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[0], "size")) {
	int width, height;

	/* get the window size */
	if (argc == 1) {
	    bu_vls_printf(&vls, "%d %d", dm_get_width(DMP), dm_get_height(DMP));
	    Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_OK;
	}

	/* set the window size */
	if (argc == 3) {
	    width = atoi(argv[1]);
	    height = atoi(argv[2]);

	    dm_set_width(DMP, width);
	    dm_set_height(DMP, height);
	    return TCL_OK;
	}

	Tcl_AppendResult(INTERP, "Usage: dm size [width height]\n", (char *)NULL);
	return TCL_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "getx")) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	if (argc == 1) {
	    /* Bare set command, print out current settings */
	    dm_internal_var(&tmp_vls, DMP, NULL);
	} else if (argc == 2) {
	    dm_internal_var(&tmp_vls, DMP, argv[1]);
	}
	Tcl_AppendResult(INTERP, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);
	return TCL_OK;
    }

    if (BU_STR_EQUAL(argv[0], "bg")) {
	int r, g, b;

	if (argc != 1 && argc != 4) {
	    bu_vls_printf(&vls, "Usage: dm bg [r g b]");
	    Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	/* return background color of current display manager */
	if (argc == 1) {
	    unsigned char *dm_bg;
	    dm_get_bg(&dm_bg, NULL, DMP);
	    bu_vls_printf(&vls, "%d %d %d", dm_bg[0], dm_bg[1], dm_bg[2]);
	    Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_OK;
	}

	if (sscanf(argv[1], "%d", &r) != 1 ||
	    sscanf(argv[2], "%d", &g) != 1 ||
	    sscanf(argv[3], "%d", &b) != 1) {
	    bu_vls_printf(&vls, "Usage: dm bg r g b");
	    Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	    bu_vls_free(&vls);

	    return TCL_ERROR;
	}

	DMP_dirty = 1;
	dm_set_dirty(DMP, 1);
	(void)dm_make_current(DMP);
	return dm_set_bg(DMP, r, g, b, r, g, b);
    }

    Tcl_AppendResult(INTERP, "dm: bad command - ", argv[0], "\n", (char *)NULL);
    return TCL_ERROR;
}

/* common sp_hook functions */

void
view_state_flag_hook(const struct bu_structparse *UNUSED(sdp),
		const char *UNUSED(name),
		void *UNUSED(base),
		const char *UNUSED(value),
                void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    if (hs->vs)
	hs->vs->vs_flag = 1;
}

void
dirty_hook(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    *(hs->dirty_global) = 1;
}

void
zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    hs->vs->vs_gvp->gv_s->gv_zclip = dm_get_zclip(hs->hs_dmp);
    dirty_hook(sdp, name, base, value, data);
}

void *
set_hook_data(struct mged_view_hook_state *hs) {
    hs->hs_dmp = DMP;
    hs->vs = view_state;
    hs->dirty_global = &(DMP_dirty);
    return (void *)hs;
}

struct bu_structparse_map vparse_map[] = {
    {"depthcue",	view_state_flag_hook      },
    {"zclip",		zclip_hook		  },
    {"zbuffer",		view_state_flag_hook      },
    {"lighting",	view_state_flag_hook      },
    {"transparency",	view_state_flag_hook      },
    {"fastfog",		view_state_flag_hook      },
    {"density",		dirty_hook  		  },
    {"bound",		dirty_hook  		  },
    {"useBound",	dirty_hook  	  	  },
    {(char *)0,		BU_STRUCTPARSE_FUNC_NULL  }
};

int
dm_commands(int argc, const char *argv[], void *data)
{
    struct mged_state *s = (struct mged_state *)data;
    struct dm_hook_data mged_dm_hook;
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	struct bu_structparse *dmparse = dm_get_vparse(DMP);
	void *mvars = dm_get_mvars(DMP);
	if (dmparse && mvars) {
	    if (argc < 2) {
		struct bu_vls report_str = BU_VLS_INIT_ZERO;

		bu_vls_sprintf(&report_str, "Display Manager (type %s) internal variables", dm_get_dm_name(DMP));
		/* Bare set command, print out current settings */
		bu_vls_struct_print2(&vls, bu_vls_addr(&report_str), dmparse, (const char *)mvars);
		bu_vls_free(&report_str);
	    } else if (argc == 2) {
		/* TODO - need to add hook func support to this func, since the
		 * one in the libdm structparse isn't enough by itself */
		if (dm_get_mvars(DMP) && dm_get_vparse(DMP)) {
		    bu_vls_struct_item_named(&vls, dmparse, argv[1], (const char *)mvars, COMMA);
		}
	    } else {
		struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
		int ret;
		struct mged_view_hook_state global_hs;
		void *hdata = set_hook_data(&global_hs);

		ret = dm_set_hook(vparse_map, argv[1], hdata, &mged_dm_hook);
		if (ret < 0) {
		    bu_log("Warning - failed to set dm hook - dm-generic.c:%d\n", __LINE__);
		}

		bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
		bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
		bu_vls_putc(&tmp_vls, '\"');
		ret = bu_struct_parse(&tmp_vls, dmparse, (char *)mvars, (void *)(&mged_dm_hook));
		bu_vls_free(&tmp_vls);
		if (ret < 0) {
		    bu_vls_free(&vls);
		    return TCL_ERROR;
		}
	    }
	}

	Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    return common_dm(s, argc, argv);
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
