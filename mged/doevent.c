/*
 *                        D O E V E N T . C
 *
 * X event handling routines for MGED.
 *
 * Author -
 *     Robert G. Parker
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

#include "tk.h"
#include <X11/Xutil.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/keysym.h>
#if IR_KNOBS
#include <gl/device.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm_xvars.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./sedit.h"

extern int doMotion;	/* defined in dm-generic.c */

static void motion_event_handler();
static void dials_event_handler();
static void buttons_event_handler();
static int forward_key();

#ifdef IR_BUTTONS
/*
 * Map SGI Button numbers to MGED button functions.
 * The layout of this table is suggestive of the actual button box layout.
 */
#define SW_HELP_KEY	SW0
#define SW_ZERO_KEY	SW3
#define HELP_KEY	0
#define ZERO_KNOBS	0
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
 *  Labels for knobs in help mode.
 */
static char	*kn1_knobs[] = {
	/* 0 */ "adc <1",	/* 1 */ "zoom", 
	/* 2 */ "adc <2",	/* 3 */ "adc dist",
	/* 4 */ "adc y",	/* 5 */ "y slew",
	/* 6 */ "adc x",	/* 7 */	"x slew"
};
static char	*kn2_knobs[] = {
	/* 0 */ "unused",	/* 1 */	"zoom",
	/* 2 */ "z rot",	/* 3 */ "z slew",
	/* 4 */ "y rot",	/* 5 */ "y slew",
	/* 6 */ "x rot",	/* 7 */	"x slew"
};
#endif

static int button0 = 0;

int
doEvent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  register struct dm_list *save_dm_list;
  int status;

  if(eventPtr->type == DestroyNotify)
    return TCL_OK;

  save_dm_list = curr_dm_list;
  GET_DM_LIST(curr_dm_list, (unsigned long)eventPtr->xany.window);

  /* it's an event for a window that I'm not handling */
  if(curr_dm_list == DM_LIST_NULL){
    curr_dm_list = save_dm_list;
    return TCL_OK;
  }

  /* calling the display manager specific event handler */
  status = eventHandler(clientData, eventPtr);

  /* no further processing of this event */
  if(status != TCL_OK){
    curr_dm_list = save_dm_list;
    return status;
  }

  /* Continuing to process the event */

  /* Forward key events to a command window */
  if(mged_variables->send_key && eventPtr->type == KeyPress)
    status = forward_key((XKeyEvent *)eventPtr);
  else if(eventPtr->type == ConfigureNotify){
    XConfigureEvent *conf = (XConfigureEvent *)eventPtr;

    dm_configureWindowShape(dmp);
    dirty = 1;

#ifdef USE_FRAMEBUFFER
    if(fbp)
      fb_configureWindow(fbp, conf->width, conf->height);
#endif

    /* no further processing of this event */
    status = TCL_RETURN;
  }else if(eventPtr->type == MapNotify){
    mapped = 1;

    /* no further processing of this event */
    status = TCL_RETURN;
  }else if(eventPtr->type == UnmapNotify){
    mapped = 0;

    /* no further processing of this event */
    status = TCL_RETURN;
  }else if(eventPtr->type == MotionNotify){
    motion_event_handler((XMotionEvent *)eventPtr);

    /* no further processing of this event */
    status = TCL_RETURN;
  }
#if IR_KNOBS
  else if(eventPtr->type == ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devmotionnotify){
    dials_event_handler((XDeviceMotionEvent *)eventPtr);

    /* no further processing of this event */
    status = TCL_RETURN;
  }
#endif
#if IR_BUTTONS
  else if(eventPtr->type == ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress){
    buttons_event_handler((XDeviceButtonEvent *)eventPtr, 1);

    /* no further processing of this event */
    status = TCL_RETURN;
  }else if(eventPtr->type == ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease){
    buttons_event_handler((XDeviceButtonEvent *)eventPtr, 0);

    /* no further processing of this event */
    status = TCL_RETURN;
  }
#endif

  curr_dm_list = save_dm_list;
  return status;
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i)
    dml_knobs[i] = 0;
}

static void
common_dbtext(str)
char *str;
{
  bu_log("common_dbtext: You pressed Help key and '%s'\n", str);
}

static void
motion_event_handler(xmotion)
XMotionEvent *xmotion;
{
  struct bu_vls cmd;
  int save_edflag = -1;
  int mx, my;
  int dx, dy;
  fastf_t f;
  fastf_t fx, fy;
  fastf_t td;

  bu_vls_init(&cmd);

  mx = xmotion->x;
  my = xmotion->y;
  dx = mx - dml_omx;
  dy = my - dml_omy;

  switch(am_mode){
  case AMM_IDLE:
    if(scroll_active)
#ifdef USE_RT_ASPECT
      bu_vls_printf( &cmd, "M 1 %d %d\n",
		     (int)(dm_Xx2Normal(dmp, mx) * 2047.0),
		     (int)(dm_Xy2Normal(dmp, my, 0) * 2047.0) );
#else
      bu_vls_printf( &cmd, "M 1 %d %d\n",
		     (int)(dm_Xx2Normal(dmp, mx, 0) * 2047.0),
		     (int)(dm_Xy2Normal(dmp, my) * 2047.0) );
#endif
    else if(rubber_band_active){
#ifdef USE_RT_ASPECT
      fastf_t x = dm_Xx2Normal(dmp, mx);
      fastf_t y = dm_Xy2Normal(dmp, my, 1);
#else
      fastf_t x = dm_Xx2Normal(dmp, mx, 1);
      fastf_t y = dm_Xy2Normal(dmp, my);
#endif

      rect_width = x - rect_x;
      rect_height = y - rect_y;

      dirty = 1;
      goto handled;
    }else if(doMotion)
      /* do the regular thing */
      /* Constant tracking (e.g. illuminate mode) bound to M mouse */
#ifdef USE_RT_ASPECT
      bu_vls_printf( &cmd, "M 0 %d %d\n",
		     (int)(dm_Xx2Normal(dmp, mx) * 2047.0),
		     (int)(dm_Xy2Normal(dmp, my, 1) * 2047.0) );
#else
      bu_vls_printf( &cmd, "M 0 %d %d\n",
		     (int)(dm_Xx2Normal(dmp, mx, 1) * 2047.0),
		     (int)(dm_Xy2Normal(dmp, my) * 2047.0) );
#endif
    else /* not doing motion */
      goto handled;

    break;
  case AMM_ROT:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      char save_coords;

      save_coords = mged_variables->coords;
      mged_variables->coords = 'v';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }

      if(mged_variables->rateknobs)
	bu_vls_printf(&cmd, "knob -i x %lf y %lf\n",
		      dy / (fastf_t)dmp->dm_height * RATE_ROT_FACTOR * 2.0,
		      dx / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0);
      else
	bu_vls_printf(&cmd, "knob -i ax %lf ay %lf\n",
		      dy * 0.25, dx * 0.25);
      (void)Tcl_Eval(interp, bu_vls_addr(&cmd));
      mged_variables->coords = save_coords;

      goto reset_edflag;
    }

    if(mged_variables->rateknobs)
      bu_vls_printf(&cmd, "knob -i -v x %lf y %lf\n",
		    dy / (fastf_t)dmp->dm_height * RATE_ROT_FACTOR * 2.0,
		    dx / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0);
    else
      bu_vls_printf(&cmd, "knob -i -v ax %lf ay %lf\n",
		    dy * 0.25, dx * 0.25);

    break;
  case AMM_TRAN:
#ifdef USE_RT_ASPECT
    fx = dx / (fastf_t)dmp->dm_width * 2.0;
    fy = -dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0;
#else
    fx = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
    fy = -dy / (fastf_t)dmp->dm_height * 2.0;
#endif
      
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      char save_coords;

      save_coords = mged_variables->coords;
      mged_variables->coords = 'v';

      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else{
	save_edflag = edobj;
	edobj = BE_O_XY;
      }

      if(mged_variables->rateknobs)
	bu_vls_printf(&cmd, "knob -i X %lf Y %lf\n", fx, fy);
      else{
	if(mged_variables->grid_snap){
	  point_t view_pt;
	  point_t model_pt;
	  point_t vcenter, diff;

	  /* accumulate distance mouse moved since starting to translate */
	  dml_mouse_dx += dx;
	  dml_mouse_dy += dy;

#ifdef USE_RT_ASPECT
	  view_pt[X] = dml_mouse_dx / (fastf_t)dmp->dm_width * 2.0;
	  view_pt[Y] = -dml_mouse_dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0;
#else
	  view_pt[X] = dml_mouse_dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
	  view_pt[Y] = -dml_mouse_dy / (fastf_t)dmp->dm_height * 2.0;
#endif
	  view_pt[Z] = 0.0;
	  round_to_grid(&view_pt[X], &view_pt[Y]);

	  MAT4X3PNT(model_pt, view2model, view_pt);
	  MAT_DELTAS_GET_NEG(vcenter, toViewcenter);
	  VSUB2(diff, model_pt, vcenter);
	  VSCALE(diff, diff, base2local);
	  VADD2(model_pt, dml_work_pt, diff);
	  bu_vls_printf(&cmd, "p %lf %lf %lf", model_pt[X], model_pt[Y], model_pt[Z]);
	}else
	  bu_vls_printf(&cmd, "knob -i aX %lf aY %lf\n",
			fx*Viewscale*base2local, fy*Viewscale*base2local);
      }

      (void)Tcl_Eval(interp, bu_vls_addr(&cmd));
      mged_variables->coords = save_coords;

      goto reset_edflag;
    }

    /* otherwise, drag to translate the view */
    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i -v X %lf Y %lf\n", fx, fy );
    else{
      if(mged_variables->grid_snap){
	/* accumulate distance mouse moved since starting to translate */
	dml_mouse_dx += dx;
	dml_mouse_dy += dy;

#ifdef USE_RT_ASPECT
	snap_view_to_grid(dml_mouse_dx / (fastf_t)dmp->dm_width * 2.0,
			  -dml_mouse_dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0);
#else
	snap_view_to_grid(dml_mouse_dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0,
			  -dml_mouse_dy / (fastf_t)dmp->dm_height * 2.0);
#endif

	goto handled;
      }else
	bu_vls_printf( &cmd, "knob -i -v aX %lf aY %lf\n",
		       fx*Viewscale*base2local, fy*Viewscale*base2local );
    }

    break;
  case AMM_SCALE:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT && !SEDIT_SCALE){
	save_edflag = es_edflag;
	es_edflag = SSCALE;
      }else if(state == ST_O_EDIT && !OEDIT_SCALE){
	save_edflag = edobj;
	edobj = BE_O_SCALE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
    else
      bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

    break;
  case AMM_ADC_ANG1:
#ifdef USE_RT_ASPECT
    fx = dm_Xx2Normal(dmp, mx) * 2047.0 - dv_xadc;
    fy = dm_Xy2Normal(dmp, my, 1) * 2047.0 - dv_yadc;
#else
    fx = dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
    fy = dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc;
#endif
    bu_vls_printf(&cmd, "adc a1 %lf\n", RAD2DEG*atan2(fy, fx));

    break;
  case AMM_ADC_ANG2:
#ifdef USE_RT_ASPECT
    fx = dm_Xx2Normal(dmp, mx) * 2047.0 - dv_xadc;
    fy = dm_Xy2Normal(dmp, my, 1) * 2047.0 - dv_yadc;
#else
    fx = dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
    fy = dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc;
#endif
    bu_vls_printf(&cmd, "adc a2 %lf\n", RAD2DEG*atan2(fy, fx));

    break;
  case AMM_ADC_TRAN:
    {
      point_t model_pt;
      point_t view_pt;

      VSET(view_pt, dm_Xx2Normal(dmp, mx), dm_Xy2Normal(dmp, my, 1), 0.0);
      MAT4X3PNT(model_pt, view2model, view_pt);
      VSCALE(model_pt, model_pt, base2local);
      bu_vls_printf(&cmd, "adc xyz %lf %lf %lf\n", model_pt[X], model_pt[Y], model_pt[Z]);
    }

    break;
  case AMM_ADC_DIST:
#ifdef USE_RT_ASPECT
    fx = (dm_Xx2Normal(dmp, mx) * 2047.0 - dv_xadc) * Viewscale * base2local / 2047.0;
    fy = (dm_Xy2Normal(dmp, my, 1) * 2047.0 - dv_yadc) * Viewscale * base2local / 2047.0;
#else
    fx = (dm_Xx2Normal(dmp, mx, 1) * 2047.0 - dv_xadc) * Viewscale * base2local / 2047.0;
    fy = (dm_Xy2Normal(dmp, my) * 2047.0 - dv_yadc) * Viewscale * base2local / 2047.0;
#endif
    td = sqrt(fx * fx + fy * fy);
    bu_vls_printf(&cmd, "adc dst %lf\n", td);

    break;
  case AMM_CON_ROT_X:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i x %f\n",
		     f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
    else
      bu_vls_printf( &cmd, "knob -i ax %f\n", f * 0.25 );

    break;
  case AMM_CON_ROT_Y:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i y %f\n",
		     f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
    else
      bu_vls_printf( &cmd, "knob -i ay %f\n", f * 0.25 );

    break;
  case AMM_CON_ROT_Z:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_ROTATE)
	  es_edflag = SROT;
      }else{
	save_edflag = edobj;
	edobj = BE_O_ROTATE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i z %f\n",
		     f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
    else
      bu_vls_printf( &cmd, "knob -i az %f\n", f * 0.25 );

    break;
  case AMM_CON_TRAN_X:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	save_edflag = edobj;
	edobj = BE_O_X;
      }
    }

#ifdef USE_RT_ASPECT
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0;
#else
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height * 2.0;
#endif

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i X %f\n", f);
    else
      bu_vls_printf( &cmd, "knob -i aX %f\n", f*Viewscale*base2local);

    break;
  case AMM_CON_TRAN_Y:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	save_edflag = edobj;
	edobj = BE_O_Y;
      }
    }

#ifdef USE_RT_ASPECT
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0;
#else
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height * 2.0;
#endif

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i Y %f\n", f);
    else
      bu_vls_printf( &cmd, "knob -i aY %f\n", f*Viewscale*base2local);

    break;
  case AMM_CON_TRAN_Z:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_TRAN)
	  es_edflag = STRANS;
      }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	save_edflag = edobj;
	edobj = BE_O_XY;
      }
    }

#ifdef USE_RT_ASPECT
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height / dmp->dm_aspect * 2.0;
#else
    if(abs(dx) >= abs(dy))
      f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
    else
      f = -dy / (fastf_t)dmp->dm_height * 2.0;
#endif

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i Z %f\n", f);
    else
      bu_vls_printf( &cmd, "knob -i aZ %f\n", f*Viewscale*base2local);

    break;
  case AMM_CON_SCALE_X:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_SCALE)
	  es_edflag = SSCALE;
      }else if(state == ST_O_EDIT && !OEDIT_SCALE){
	save_edflag = edobj;
	edobj = BE_O_XSCALE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
    else
      bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

    break;
  case AMM_CON_SCALE_Y:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_SCALE)
	  es_edflag = SSCALE;
      }else if(state == ST_O_EDIT && !OEDIT_SCALE){
	save_edflag = edobj;
	edobj = BE_O_YSCALE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
    else
      bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

    break;
  case AMM_CON_SCALE_Z:
    if((state == ST_S_EDIT || state == ST_O_EDIT) &&
       mged_variables->transform == 'e'){
      if(state == ST_S_EDIT){
	save_edflag = es_edflag;
	if(!SEDIT_SCALE)
	  es_edflag = SSCALE;
      }else if(state == ST_O_EDIT && !OEDIT_SCALE){
	save_edflag = edobj;
	edobj = BE_O_ZSCALE;
      }
    }

    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    if(mged_variables->rateknobs)
      bu_vls_printf( &cmd, "knob -i S %f\n", f / (fastf_t)dmp->dm_height );
    else
      bu_vls_printf( &cmd, "knob -i aS %f\n", f / (fastf_t)dmp->dm_height );

    break;
  case AMM_CON_XADC:
    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

#ifdef USE_RT_ASPECT
    bu_vls_printf( &cmd, "knob -i xadc %f\n",
		   f / (fastf_t)dmp->dm_width * 4095.0 );
#else
    bu_vls_printf( &cmd, "knob -i xadc %f\n",
		   f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );
#endif
    break;
  case AMM_CON_YADC:
    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    bu_vls_printf( &cmd, "knob -i yadc %f\n",
		   f / (fastf_t)dmp->dm_height * 4095.0 );
    break;
  case AMM_CON_ANG1:
    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    bu_vls_printf( &cmd, "knob -i ang1 %f\n",
		   f / (fastf_t)dmp->dm_width * 90.0 );
    break;
  case AMM_CON_ANG2:
    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

    bu_vls_printf( &cmd, "knob -i ang2 %f\n",
		     f / (fastf_t)dmp->dm_width * 90.0 );
    break;
  case AMM_CON_DIST:
    if(abs(dx) >= abs(dy))
      f = dx;
    else
      f = -dy;

#ifdef USE_RT_ASPECT
    bu_vls_printf( &cmd, "knob -i distadc %f\n",
		   f / (fastf_t)dmp->dm_width * 4095.0 );
#else
    bu_vls_printf( &cmd, "knob -i distadc %f\n",
		   f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );
#endif
    break;
  }

  (void)Tcl_Eval(interp, bu_vls_addr(&cmd));

reset_edflag:
  if(save_edflag != -1){
    if(state == ST_S_EDIT)
      es_edflag = save_edflag;
    else if(state == ST_O_EDIT)
      edobj = save_edflag;
  }

handled:
  bu_vls_free(&cmd);
  dml_omx = mx;
  dml_omy = my;
}

#if IR_KNOBS
static void
dials_event_handler(dmep)
XDeviceMotionEvent *dmep;
{
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  struct bu_vls cmd;
  int save_edflag = -1;
  int setting;
  fastf_t f;

  if(button0){
    common_dbtext((adc_draw ? kn1_knobs:kn2_knobs)[dmep->first_axis]);
    return;
  }

  bu_vls_init(&cmd);

  switch(DIAL0 + dmep->first_axis){
  case DIAL0:
    if(adc_draw){
      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE &&
	 !dv_1adc )
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit(dv_1adc) + dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob ang1 %f\n",
		     45.0 - 45.0*((double)setting)/2047.0);
    }else{
      if(mged_variables->rateknobs){
	f = rate_model_rotate[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf( &cmd, "knob -m z %f\n", setting / 512.0 );
      }else{
	f = absolute_model_rotate[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(2.847 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	f = dm_limit(dml_knobs[dmep->first_axis]) / 512.0;
	bu_vls_printf( &cmd, "knob -m az %f\n", dm_wrap(f) * 180.0);
      }
    }
    break;
  case DIAL1:
    if(mged_variables->rateknobs){
      if(EDIT_SCALE && mged_variables->transform == 'e')
	f = edit_rate_scale;
      else
	f = rate_scale;

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] = dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob S %f\n", setting / 512.0 );
    }else{
      if(EDIT_SCALE && mged_variables->transform == 'e')
	f = edit_absolute_scale;
      else
	f = absolute_scale;

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob aS %f\n", setting / 512.0 );
    }
    break;
  case DIAL2:
    if(adc_draw){
      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE &&
	 !dv_2adc )
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit(dv_2adc) + dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob ang2 %f\n",
		     45.0 - 45.0*((double)setting)/2047.0);
    }else {
      if(mged_variables->rateknobs){
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = rate_model_rotate[Z];
	else
	  f = rate_rotate[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf( &cmd, "knob z %f\n", setting / 512.0 );
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_rotate[Z];
	else
	  f = absolute_rotate[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(2.847 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	f = dm_limit(dml_knobs[dmep->first_axis]) / 512.0;
	bu_vls_printf( &cmd, "knob az %f\n", dm_wrap(f) * 180.0);
      }
    }
    break;
  case DIAL3:
    if(adc_draw){
      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE &&
	 !dv_distadc)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit(dv_distadc) + dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob distadc %d\n", setting );
    }else {
      if(mged_variables->rateknobs){
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
	  case 'm':
	  case 'o':
	    f = edit_rate_model_tran[Z];
	    break;
	  case 'v':
	  default:
	    f = edit_rate_view_tran[Z];
	    break;
	  }

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_TRAN)
	      es_edflag = STRANS;
	  }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }
	}else if(mged_variables->coords == 'm')
	  f = rate_model_tran[Z];
	else
	  f = rate_tran[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf( &cmd, "knob Z %f\n", setting / 512.0 );
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
	  case 'm':
	  case 'o':
	    f = edit_absolute_model_tran[Z];
	    break;
	  case 'v':
	  default:
	    f = edit_absolute_view_tran[Z];
	    break;
	  }

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_TRAN)
	      es_edflag = STRANS;
	  }else if(state == ST_O_EDIT && !OEDIT_TRAN){
	    save_edflag = edobj;
	    edobj = BE_O_XY;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_tran[Z];
	else
	  f = absolute_tran[Z];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE &&
	   !f )
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf(&cmd, "knob aZ %f\n", setting / 512.0 * Viewscale * base2local);
      }
    }
    break;
  case DIAL4:
    if(adc_draw){
      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE &&
	 !dv_yadc)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit(dv_yadc) + dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob yadc %d\n", setting );
    }else{
      if(mged_variables->rateknobs){
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = rate_model_rotate[Y];
	else
	  f = rate_rotate[Y];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf( &cmd, "knob y %f\n", setting / 512.0 );
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_rotate[Y];
	else
	  f = absolute_rotate[Y];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE &&
	   !f )
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(2.847 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	f = dm_limit(dml_knobs[dmep->first_axis]) / 512.0;
	bu_vls_printf( &cmd, "knob ay %f\n", dm_wrap(f) * 180.0);
      }
    }
    break;
  case DIAL5:
    if(mged_variables->rateknobs){
      if((state == ST_S_EDIT || state == ST_O_EDIT)
	 && mged_variables->transform == 'e'){
	switch(mged_variables->coords){
	case 'm':
	case 'o':
	  f = edit_rate_model_tran[Y];
	  break;
	case 'v':
	default:
	  f = edit_rate_view_tran[Y];
	  break;
	}

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_TRAN)
	    es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }else if(mged_variables->coords == 'm')
	f = rate_model_tran[Y];
      else
	f = rate_tran[Y];

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob Y %f\n", setting / 512.0 );
    }else{
      if((state == ST_S_EDIT || state == ST_O_EDIT)
	 && mged_variables->transform == 'e'){
	switch(mged_variables->coords){
	case 'm':
	case 'o':
	  f = edit_absolute_model_tran[Y];
	  break;
	case 'v':
	default:
	  f = edit_absolute_view_tran[Y];
	  break;
	}

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_TRAN)
	    es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }else if(mged_variables->coords == 'm')
	f = absolute_model_tran[Y];
      else
	f = absolute_tran[Y];

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] -
	  knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf(&cmd, "knob aY %f\n", setting / 512.0 * Viewscale * base2local);
    }
    break;
  case DIAL6:
    if(adc_draw){
      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE &&
	 !dv_xadc)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit(dv_xadc) + dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob xadc %d\n", setting );
    }else{
      if(mged_variables->rateknobs){
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = rate_model_rotate[X];
	else
	  f = rate_rotate[X];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(512.5 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	setting = dm_limit(dml_knobs[dmep->first_axis]);
	bu_vls_printf( &cmd, "knob x %f\n", setting / 512.0);
      }else{
	if((state == ST_S_EDIT || state == ST_O_EDIT)
	   && mged_variables->transform == 'e'){
	  switch(mged_variables->coords){
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

	  if(state == ST_S_EDIT){
	    save_edflag = es_edflag;
	    if(!SEDIT_ROTATE)
	      es_edflag = SROT;
	  }else if(state == ST_O_EDIT && !OEDIT_ROTATE){
	    save_edflag = edobj;
	    edobj = BE_O_ROTATE;
	  }
	}else if(mged_variables->coords == 'm')
	  f = absolute_model_rotate[X];
	else
	  f = absolute_rotate[X];

	if(-NOISE <= dml_knobs[dmep->first_axis] &&
	   dml_knobs[dmep->first_axis] <= NOISE && !f)
	  dml_knobs[dmep->first_axis] +=
	    dmep->axis_data[0] - knob_values[dmep->first_axis];
	else
	  dml_knobs[dmep->first_axis] =
	    dm_unlimit((int)(2.847 * f)) +
	    dmep->axis_data[0] - knob_values[dmep->first_axis];

	f = dm_limit(dml_knobs[dmep->first_axis]) / 512.0;
	bu_vls_printf( &cmd, "knob ax %f\n", dm_wrap(f) * 180.0);
      }
    }
    break;
  case DIAL7:
    if(mged_variables->rateknobs){
      if((state == ST_S_EDIT || state == ST_O_EDIT)
	 && mged_variables->transform == 'e'){
	switch(mged_variables->coords){
	case 'm':
	case 'o':
	  f = edit_rate_model_tran[X];
	  break;
	case 'v':
	default:
	  f = edit_rate_view_tran[X];
	  break;
	}

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_TRAN)
	    es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }else if(mged_variables->coords == 'm')
	f = rate_model_tran[X];
      else
	f = rate_tran[X];

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf( &cmd, "knob X %f\n", setting / 512.0 );
    }else{
      if((state == ST_S_EDIT || state == ST_O_EDIT)
	 && mged_variables->transform == 'e'){
	switch(mged_variables->coords){
	case 'm':
	case 'o':
	  f = edit_absolute_model_tran[X];
	  break;
	case 'v':
	default:
	  f = edit_absolute_view_tran[X];
	  break;
	}

	if(state == ST_S_EDIT){
	  save_edflag = es_edflag;
	  if(!SEDIT_TRAN)
	    es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }else if(mged_variables->coords == 'm')
	f = absolute_model_tran[X];
      else
	f = absolute_tran[X];

      if(-NOISE <= dml_knobs[dmep->first_axis] &&
	 dml_knobs[dmep->first_axis] <= NOISE && !f)
	dml_knobs[dmep->first_axis] +=
	  dmep->axis_data[0] - knob_values[dmep->first_axis];
      else
	dml_knobs[dmep->first_axis] =
	  dm_unlimit((int)(512.5 * f)) +
	  dmep->axis_data[0] - knob_values[dmep->first_axis];

      setting = dm_limit(dml_knobs[dmep->first_axis]);
      bu_vls_printf(&cmd, "knob aX %f\n", setting / 512.0 * Viewscale * base2local);
    }
    break;
  default:
    break;
  }

  /* Keep track of the knob values */
  knob_values[dmep->first_axis] = dmep->axis_data[0];

  Tcl_Eval(interp, bu_vls_addr(&cmd));

  if(save_edflag != -1){
    if(state == ST_S_EDIT)
      es_edflag = save_edflag;
    else if(state == ST_O_EDIT)
      edobj = save_edflag;
  }

  bu_vls_free(&cmd);
}
#endif

#if IR_BUTTONS
static void
buttons_event_handler(dbep, press)
XDeviceButtonEvent *dbep;
int press;
{
  if(press){
    struct bu_vls cmd;

    if(dbep->button == 1){
      button0 = 1;
      return;
    }

    if(button0){
      common_dbtext(label_button(bmap[dbep->button - 1]));
      return;
    }else if(dbep->button == 4){
      set_knob_offset();

      bu_vls_init(&cmd);
      bu_vls_strcat(&cmd, "knob zero\n");
    }else{
      bu_vls_init(&cmd);
      bu_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[dbep->button - 1]));
    }

    (void)Tcl_Eval(interp, bu_vls_addr(&cmd));
    bu_vls_free(&cmd);
  }else{
    if(dbep->button == 1)
      button0 = 0;
  }
}
#endif

/* Forward key events to a command window */
static int
forward_key(xkey)
XKeyEvent *xkey;
{
  char buffer[2];
  KeySym keysym;
  struct dm_char_queue *dcqp;

  XLookupString(xkey, buffer, 1, &keysym, (XComposeStatus *)NULL);

  if(keysym == mged_variables->hot_key)
    return TCL_OK;

  BU_GETSTRUCT(dcqp, dm_char_queue);
  dcqp->dlp = curr_dm_list;
  BU_LIST_INSERT(&head_dm_char_queue.l, &dcqp->l);
  write(dm_pipe[1], buffer, 1);

  /* Use this so that these events won't propagate */
  return TCL_RETURN;
}
