/*
 *			D M - G E N E R I C . C
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>

#include "tk.h"
#include <X11/Xutil.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm_xvars.h"
#include "./ged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern point_t e_axes_pos;
extern int scroll_select();		/* defined in scroll.c */
extern int menu_select();		/* defined in menu.c */

int doMotion = 0;

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

  if(!strcmp(argv[0], "idle")){
    am_mode = AMM_IDLE;
    scroll_active = 0;
#ifdef DO_RUBBER_BAND
    if(rubber_band_active){
      rubber_band_active = 0;
      dirty = 1;

      if(mged_variables->mouse_behavior == 'r')
	rt_rect_area();
      else if(mged_variables->mouse_behavior == 'z')
	zoom_rect_area();
    }
#endif    

    return TCL_OK;
  }

  if( !strcmp( argv[0], "m" )){
    if( argc < 3){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    {
      int x;
      int y;
      int old_orig_gui;
      int stolen = 0;
      fastf_t fx, fy;

      old_orig_gui = mged_variables->orig_gui;

#ifdef USE_RT_ASPECT
      fx = dm_Xx2Normal(dmp, atoi(argv[1]));
      fy = dm_Xy2Normal(dmp, atoi(argv[2]), 0);
#else
      fx = dm_Xx2Normal(dmp, atoi(argv[1]), 0);
      fy = dm_Xy2Normal(dmp, atoi(argv[2]));
#endif
      x = fx * 2047.0;
      y = fy * 2047.0;

      if(mged_variables->faceplate &&
	 mged_variables->orig_gui){
#define        MENUXLIM        (-1250)

	if(x >= MENUXLIM && scroll_select( x, y, 0 )){
	  stolen = 1;
	  goto end;
	}

	if(x < MENUXLIM && mmenu_select( y, 0)){
	  stolen = 1;
	  goto end;
	}
      }

      mged_variables->orig_gui = 0;
#ifdef USE_RT_ASPECT
      fy = dm_Xy2Normal(dmp, atoi(argv[2]), 1);
      y = fy * 2047.0;
#else
      fx = dm_Xx2Normal(dmp, atoi(argv[1]), 1);
      x = fx * 2047.0;
#endif

end:
      bu_vls_init(&vls);
      if(mged_variables->mouse_behavior == 'q' && !stolen){
	point_t view_pt;
	point_t model_pt;

	VSET(view_pt, fx, fy, 1.0);
	MAT4X3PNT(model_pt, view2model, view_pt);
	VSCALE(model_pt, model_pt, base2local);
	if(*zclip_ptr)
	  bu_vls_printf(&vls, "nirt %lf %lf %lf",
			model_pt[X], model_pt[Y], model_pt[Z]);
	else
	  bu_vls_printf(&vls, "nirt -b %lf %lf %lf",
			model_pt[X], model_pt[Y], model_pt[Z]);
#ifdef DO_RUBBER_BAND
      }else if((mged_variables->mouse_behavior == 'p' ||
		mged_variables->mouse_behavior == 'r' ||
		mged_variables->mouse_behavior == 'z') && !stolen){
	rubber_band_active = 1;
	rect_x = fx;
	rect_y = fy;
	rect_width = 0.0;
	rect_height = 0.0;

	dirty = 1;
#endif
#ifdef DO_SNAP_TO_GRID
      }else if(mged_variables->grid_snap && !stolen &&
	       state != ST_S_PICK && state != ST_O_PICK &&
	       state != ST_O_PATH && !SEDIT_PICK){
	point_t view_pt;
	point_t model_pt;

	snap_to_grid(&fx, &fy);

	if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	   mged_variables->transform == 'e'){
	  int save_edflag = -1;

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_TRAN)
	      es_edflag = STRANS;
	  }else{
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }

	  MAT4X3PNT(view_pt, model2view, e_axes_pos);
	  view_pt[X] = fx;
	  view_pt[Y] = fy;
	  MAT4X3PNT(model_pt, view2model, view_pt);
	  VSCALE(model_pt, model_pt, base2local);
	  bu_vls_printf(&vls, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	  status = Tcl_Eval(interp, bu_vls_addr(&vls));

	  if(state == ST_S_EDIT)
	    es_edflag = save_edflag;
	  else
	    edobj = save_edflag;

	  mged_variables->orig_gui = old_orig_gui;
	  bu_vls_free(&vls);
	  return status;
	}else{
	  point_t vcenter;

	  MAT_DELTAS_GET_NEG(vcenter, toViewcenter);
	  MAT4X3PNT(view_pt, model2view, vcenter);
	  view_pt[X] = fx;
	  view_pt[Y] = fy;
	  MAT4X3PNT(model_pt, view2model, view_pt);
	  VSCALE(model_pt, model_pt, base2local);
	  bu_vls_printf(&vls, "center %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	}
#endif
      }else
	bu_vls_printf(&vls, "M 1 %d %d\n", x, y);

      status = Tcl_Eval(interp, bu_vls_addr(&vls));
      mged_variables->orig_gui = old_orig_gui;
      bu_vls_free(&vls);

      return status;
    }
  }

  if(!strcmp(argv[0], "am")){
    if( argc < 4){
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

      if(mged_variables->grid_snap){
	int save_edflag;

	if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	   mged_variables->transform == 'e'){
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
      if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	 NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	acc_sc_sol = 1.0;
      else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
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
#ifdef USE_RT_ASPECT
      fx = dm_Xx2Normal(dmp, dml_omx) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, dml_omy, 1) * 2047.0 - dv_yadc;
#else
      fx = dm_Xx2Normal(dmp, dml_omx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, dml_omy) * 2047.0 - dv_yadc;
#endif
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "adc a1 %lf\n", RAD2DEG*atan2(fy, fx));
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);

      am_mode = AMM_ADC_ANG1;
      break;
    case '2':
#ifdef USE_RT_ASPECT
      fx = dm_Xx2Normal(dmp, dml_omx) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, dml_omy, 1) * 2047.0 - dv_yadc;
#else
      fx = dm_Xx2Normal(dmp, dml_omx, 1) * 2047.0 - dv_xadc;
      fy = dm_Xy2Normal(dmp, dml_omy) * 2047.0 - dv_yadc;
#endif
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
	MAT4X3PNT(model_pt, view2model, view_pt);
	VSCALE(model_pt, model_pt, base2local);

	bu_vls_printf(&vls, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
	Tcl_Eval(interp, bu_vls_addr(&vls));

	bu_vls_free(&vls);
	am_mode = AMM_ADC_TRAN;
      }

      break;
    case 'd':
#ifdef USE_RT_ASPECT
      fx = (dm_Xx2Normal(dmp, dml_omx) * 2047.0 -
	    dv_xadc) * Viewscale * base2local / 2047.0;
      fy = (dm_Xy2Normal(dmp, dml_omy, 1) * 2047.0 -
	    dv_yadc) * Viewscale * base2local / 2047.0;
#else
      fx = (dm_Xx2Normal(dmp, dml_omx, 1) * 2047.0 -
	    dv_xadc) * Viewscale * base2local / 2047.0;
      fy = (dm_Xy2Normal(dmp, dml_omy) * 2047.0 -
	    dv_yadc) * Viewscale * base2local / 2047.0;
#endif

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
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	  edit_absolute_scale = acc_sc[0] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_X;
	break;
      case 'y':
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
	  edit_absolute_scale = acc_sc[1] - 1.0;
	  if(edit_absolute_scale > 0.0)
	    edit_absolute_scale /= 3.0;
	}

	am_mode = AMM_CON_SCALE_Y;
	break;
      case 'z':
	if(state == ST_S_EDIT && mged_variables->transform == 'e' &&
	   NEAR_ZERO(acc_sc_sol, (fastf_t)SMALL_FASTF))
	  acc_sc_sol = 1.0;
	else if(state == ST_O_EDIT && mged_variables->transform == 'e'){
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

  if( !strcmp( argv[0], "size" )){
    int width, height;

    /* get the window size */
    if( argc == 1 ){
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

      Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, width, height);

      return TCL_OK;
    }

    Tcl_AppendResult(interp, "Usage: dm size [width height]\n", (char *)NULL);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}
