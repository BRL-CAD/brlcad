/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-generic.c
 *
 * Routines common to MGED's interface to LIBDM.
 *
 * Author -
 *	Robert G. Parker
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "common.h"



#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>

#ifdef DM_X 
#  include "tk.h"
#  include <X11/Xutil.h>
#  include "dm_xvars.h"
#else
#  include "tcl.h"
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern point_t e_axes_pos;
extern point_t curr_e_axes_pos;
extern int scroll_select();		/* defined in scroll.c */
extern int menu_select();		/* defined in menu.c */
extern void rect_view2image();		/* defined in rect.c */
extern void rb_set_dirty_flag();

int doMotion = 0;

#ifdef DM_X
struct bu_structparse dm_xvars_vparse[] = {
	{"%x",	1,	"dpy",			XVARS_MV_O(dpy),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"win",			XVARS_MV_O(win),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"top",			XVARS_MV_O(top),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"tkwin",		XVARS_MV_O(xtkwin),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1,	"depth",		XVARS_MV_O(depth),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"cmap",			XVARS_MV_O(cmap),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"vip",			XVARS_MV_O(vip),	BU_STRUCTPARSE_FUNC_NULL },
	{"%x",	1,	"fontstruct",		XVARS_MV_O(fontstruct),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1,	"devmotionnotify",	XVARS_MV_O(devmotionnotify),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1,	"devbuttonpress",	XVARS_MV_O(devbuttonpress),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1,	"devbuttonrelease",	XVARS_MV_O(devbuttonrelease),	BU_STRUCTPARSE_FUNC_NULL },
	{"",	0,	(char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};
#endif

/*
 *  Based upon new state, possibly do extra stuff,
 *  including enabling continuous tablet tracking,
 *  object highlighting.
 *
 *  This routine was taken and modified from the original dm-X.c
 *  that was written by Phil Dykstra.
 */
void
stateChange(a, b)
int a, b;
{
  switch( b )  {
  case ST_VIEW:
    /* constant tracking OFF */
    doMotion = 0;
    break;
  case ST_S_PICK:
  case ST_O_PICK:
  case ST_O_PATH:
  case ST_S_VPICK:
    /* constant tracking ON */
    doMotion = 1;
    break;
  case ST_O_EDIT:
  case ST_S_EDIT:
    /* constant tracking OFF */
    doMotion = 0;
    break;
  default:
    bu_log("statechange: unknown state %s\n", state_str[b]);
    break;
  }

  ++update_views;
}

int
common_dm(argc, argv)
int argc;
char **argv;
{
  int status;
  struct bu_vls vls;

  if (dbip == DBI_NULL)
	  return TCL_OK;

  if(!strcmp(argv[0], "idle")){
    am_mode = AMM_IDLE;
    scroll_active = 0;
    if(rubber_band->rb_active){
      rubber_band->rb_active = 0;

      if(mged_variables->mv_mouse_behavior == 'p')
	rb_set_dirty_flag();
      else if(mged_variables->mv_mouse_behavior == 'r')
	rt_rect_area();
      else if(mged_variables->mv_mouse_behavior == 'z')
	zoom_rect_area();
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "m")){
    int x;
    int y;
    int old_orig_gui;
    int stolen = 0;
    fastf_t fx, fy;

    if(argc < 3){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    old_orig_gui = mged_variables->mv_orig_gui;

    fx = dm_Xx2Normal(dmp, atoi(argv[1]));
    fy = dm_Xy2Normal(dmp, atoi(argv[2]), 0);
    x = fx * GED_MAX;
    y = fy * GED_MAX;

    if(mged_variables->mv_faceplate &&
       mged_variables->mv_orig_gui){
#define        MENUXLIM        (-1250)

      if(x >= MENUXLIM && scroll_select(x, y, 0)){
	stolen = 1;
	goto end;
      }

      if(x < MENUXLIM && mmenu_select(y, 0)){
	stolen = 1;
	goto end;
      }
    }

    mged_variables->mv_orig_gui = 0;
    fy = dm_Xy2Normal(dmp, atoi(argv[2]), 1);
    y = fy * GED_MAX;

end:
    bu_vls_init(&vls);
    if(mged_variables->mv_mouse_behavior == 'q' && !stolen){
      point_t view_pt;
      point_t model_pt;

      if(grid_state->gr_snap)
	snap_to_grid(&fx, &fy);

      if( mged_variables->mv_perspective_mode )
            VSET(view_pt, fx, fy, 0.0)
      else
            VSET(view_pt, fx, fy, 1.0)

      MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
      VSCALE(model_pt, model_pt, base2local);
      if(dmp->dm_zclip)
	bu_vls_printf(&vls, "qray_nirt %lf %lf %lf",
		      model_pt[X], model_pt[Y], model_pt[Z]);
      else
	bu_vls_printf(&vls, "qray_nirt -b %lf %lf %lf",
		      model_pt[X], model_pt[Y], model_pt[Z]);
    }else if((mged_variables->mv_mouse_behavior == 'p' ||
	      mged_variables->mv_mouse_behavior == 'r' ||
	      mged_variables->mv_mouse_behavior == 'z') && !stolen){

      if(grid_state->gr_snap)
	snap_to_grid(&fx, &fy);

      rubber_band->rb_active = 1;
      rubber_band->rb_x = fx;
      rubber_band->rb_y = fy;
      rubber_band->rb_width = 0.0;
      rubber_band->rb_height = 0.0;
      rect_view2image();
      rb_set_dirty_flag();
    }else if(mged_variables->mv_mouse_behavior == 's' && !stolen){
#if 0
      if(grid_state->gr_snap){
	snap_to_grid(&fx, &fy);
	x = fx * GED_MAX;
	y = fy * GED_MAX;
      }
#endif
      bu_vls_printf(&vls, "mouse_solid_edit_select %d %d", x, y);
    }else if(mged_variables->mv_mouse_behavior == 'm' && !stolen){
#if 0
      if(grid_state->gr_snap){
	snap_to_grid(&fx, &fy);
	x = fx * GED_MAX;
	y = fy * GED_MAX;
      }
#endif
      bu_vls_printf(&vls, "mouse_matrix_edit_select %d %d", x, y);
    }else if(mged_variables->mv_mouse_behavior == 'c' && !stolen){
#if 0
      if(grid_state->gr_snap){
	snap_to_grid(&fx, &fy);
	x = fx * GED_MAX;
	y = fy * GED_MAX;
      }
#endif
      bu_vls_printf(&vls, "mouse_comb_edit_select %d %d", x, y);
    } else if(mged_variables->mv_mouse_behavior == 'o' && !stolen){
#if 0
      if(grid_state->gr_snap){
	snap_to_grid(&fx, &fy);
	x = fx * GED_MAX;
	y = fy * GED_MAX;
      }
#endif
      bu_vls_printf(&vls, "mouse_rt_obj_select %d %d", x, y);
    }else if(adc_state->adc_draw && mged_variables->mv_transform == 'a' && !stolen) {
      point_t model_pt;
      point_t view_pt;

      if(grid_state->gr_snap)
	snap_to_grid(&fx, &fy);

      VSET(view_pt, fx, fy, 1.0);
      MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
      VSCALE(model_pt, model_pt, base2local);
      bu_vls_printf(&vls, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
    } else if (grid_state->gr_snap && !stolen &&
	       SEDIT_TRAN && mged_variables->mv_transform == 'e') {
	    point_t view_pt;
	    point_t model_pt;

	    snap_to_grid(&fx, &fy);
	    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
    } else if (grid_state->gr_snap && !stolen &&
	       OEDIT_TRAN && mged_variables->mv_transform == 'e') {
	    point_t view_pt;
	    point_t model_pt;

	    snap_to_grid(&fx, &fy);
	    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, curr_e_axes_pos);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "translate %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
    } else if (grid_state->gr_snap && !stolen &&
	       state != ST_S_PICK && state != ST_O_PICK &&
	       state != ST_O_PATH && !SEDIT_PICK && !EDIT_SCALE) {
	    point_t view_pt;
	    point_t model_pt;
	    point_t vcenter;

	    snap_to_grid(&fx, &fy);
	    MAT_DELTAS_GET_NEG(vcenter, view_state->vs_vop->vo_center);
	    MAT4X3PNT(view_pt, view_state->vs_vop->vo_model2view, vcenter);
	    view_pt[X] = fx;
	    view_pt[Y] = fy;
	    MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
	    VSCALE(model_pt, model_pt, base2local);
	    bu_vls_printf(&vls, "center %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
    } else
	    bu_vls_printf(&vls, "M 1 %d %d\n", x, y);

    status = Tcl_Eval(interp, bu_vls_addr(&vls));
    mged_variables->mv_orig_gui = old_orig_gui;
    bu_vls_free(&vls);

    return status;
  }

  if(!strcmp(argv[0], "am")){
    if(argc < 4){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|s> xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    dml_omx = atoi(argv[2]);
    dml_omy = atoi(argv[3]);

    switch(*argv[1]){
    case 'r':
      am_mode = AMM_ROT;
      break;
    case 't':
      am_mode = AMM_TRAN;

      if(grid_state->gr_snap){
	int save_edflag;

	if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	   mged_variables->mv_transform == 'e'){
	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_TRAN)
	      es_edflag = STRANS;
	  }else{
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }

	  snap_keypoint_to_grid();

	  if(state == ST_S_EDIT)
	    es_edflag = save_edflag;
	  else
	    edobj = save_edflag;
	}else
	  snap_view_center_to_grid();
      }

      break;
    case 's':
      if(state == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
	 NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	acc_sc_sol = 1.0;
      else if(state == ST_O_EDIT && mged_variables->mv_transform == 'e'){
	edit_absolute_scale = acc_sc_obj - 1.0;
	if(edit_absolute_scale > 0.0)
	  edit_absolute_scale /= 3.0;
      }

      am_mode = AMM_SCALE;
      break;
    default:
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|s> xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "adc")){
    fastf_t fx, fy;
    fastf_t td; /* tick distance */

    if(argc < 4){
      Tcl_AppendResult(interp, "dm adc: need more parameters\n",
		       "dm adc 1|2|t|d xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    dml_omx = atoi(argv[2]);
    dml_omy = atoi(argv[3]);

    switch(*argv[1]){
    case '1':
      fx = dm_Xx2Normal(dmp, dml_omx) * GED_MAX - adc_state->adc_dv_x;
      fy = dm_Xy2Normal(dmp, dml_omy, 1) * GED_MAX - adc_state->adc_dv_y;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc a1 %lf\n", RAD2DEG*atan2(fy, fx));
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_ANG1;
      break;
    case '2':
      fx = dm_Xx2Normal(dmp, dml_omx) * GED_MAX - adc_state->adc_dv_x;
      fy = dm_Xy2Normal(dmp, dml_omy, 1) * GED_MAX - adc_state->adc_dv_y;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc a2 %lf\n", RAD2DEG*atan2(fy, fx));
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_ANG2;
      break;
    case 't':
      {
	point_t model_pt;
	point_t view_pt;

	bu_vls_init(&vls);

	VSET(view_pt, dm_Xx2Normal(dmp, dml_omx), dm_Xy2Normal(dmp, dml_omy, 1), 0.0);

	if(grid_state->gr_snap)
	  snap_to_grid(&view_pt[X], &view_pt[Y]);

	MAT4X3PNT(model_pt, view_state->vs_vop->vo_view2model, view_pt);
	VSCALE(model_pt, model_pt, base2local);

	bu_vls_printf(&vls, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
	Tcl_Eval(interp, bu_vls_addr(&vls));

	bu_vls_free(&vls);
	am_mode = AMM_ADC_TRAN;
      }

      break;
    case 'd':
      fx = (dm_Xx2Normal(dmp, dml_omx) * GED_MAX -
	    adc_state->adc_dv_x) * view_state->vs_vop->vo_scale * base2local * INV_GED;
      fy = (dm_Xy2Normal(dmp, dml_omy, 1) * GED_MAX -
	    adc_state->adc_dv_y) * view_state->vs_vop->vo_scale * base2local * INV_GED;

      td = sqrt(fx * fx + fy * fy);
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc dst %lf\n", td);
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_DIST;
      break;
      default:
	Tcl_AppendResult(interp, "dm adc: unrecognized parameter - ", argv[1],
			 "\ndm adc 1|2|t|d xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "con")){
    if(argc < 5){
      Tcl_AppendResult(interp, "dm con: need more parameters\n",
		       "dm con r|t|s x|y|z xpos ypos\n",
		       "dm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    dml_omx = atoi(argv[3]);
    dml_omy = atoi(argv[4]);

    switch(*argv[1]){
    case 'a':
      switch(*argv[2]){
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
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con a x|y|1|2|d xpos ypos\n", (char *)NULL);
      }
      break;
    case 'r':
      switch(*argv[2]){
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
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
      break;
    case 't':
      switch(*argv[2]){
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
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
      break;
    case 's':
      switch(*argv[2]){
      case 'x':
	if(state == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->mv_transform == 'e'){
	  edit_absolute_scale = acc_sc[0] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_X;
	break;
      case 'y':
	if(state == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->mv_transform == 'e'){
	  edit_absolute_scale = acc_sc[1] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_Y;
	break;
      case 'z':
	if(state == ST_S_EDIT && mged_variables->mv_transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->mv_transform == 'e'){
	  edit_absolute_scale = acc_sc[2] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_Z;
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[2],
			 "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
      break;
    default:
      Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[1],
		       "\ndm con r|t|s x|y|z xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    return TCL_OK;
  }

  if(!strcmp(argv[0], "size")){
    int width, height;

    /* get the window size */
    if(argc == 1){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d", dmp->dm_width, dmp->dm_height);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_OK;
    }

    /* set the window size */
    if( argc == 3 ){
      width = atoi( argv[1] );
      height = atoi( argv[2] );

#ifdef DM_X
#  if 0
      Tk_ResizeWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, width, height);
#  else
      Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, width, height);
#  endif
#endif

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "Usage: dm size [width height]\n", (char *)NULL);
    return TCL_ERROR;
  }

#ifdef DM_X
  if(!strcmp(argv[0], "getx")){
    if(argc == 1){
      struct bu_vls tmp_vls;

      bu_vls_init(&tmp_vls);
      /* Bare set command, print out current settings */
      bu_vls_struct_print2(&tmp_vls, "dm internal X variables", dm_xvars_vparse,
		      (const char *)dmp->dm_vars.pub_vars);
      Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
      bu_vls_free(&tmp_vls);
    }else if(argc == 2){
      bu_vls_init(&vls);
      bu_vls_struct_item_named(&vls, dm_xvars_vparse, argv[1],
			       (const char *)dmp->dm_vars.pub_vars, ',');
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
    }

    return TCL_OK;
  }
#endif

  if(!strcmp(argv[0], "bg")){
    int r, g, b;

    if(argc != 1 && argc != 4){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "Usage: dm bg [r g b]");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    /* return background color of current display manager */
    if (argc == 1) {
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "%d %d %d",
		    dmp->dm_bg[0],
		    dmp->dm_bg[1],
		    dmp->dm_bg[2]);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
      
      return TCL_OK;
    }

    if(sscanf(argv[1], "%d", &r) != 1 ||
       sscanf(argv[2], "%d", &g) != 1 ||
       sscanf(argv[3], "%d", &b) != 1){
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "Usage: dm bg r g b");
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);

      return TCL_ERROR;
    }

    dirty = 1;
    return DM_SET_BGCOLOR(dmp, r, g, b);
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
