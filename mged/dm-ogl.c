/*
 *			D M - O G L . C
 *
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <termio.h>
#undef VMIN		/* is used in vmath.h, too */
#include <ctype.h>

#include <sys/types.h>
#include <sys/time.h>

#include <X11/X.h>
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "tk.h"
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>

#ifdef USE_MESA_GL
#include <MESA_GL/glx.h>
#include <MESA_GL/gl.h>
#else
#include <GL/glx.h>
#include <GL/gl.h>
#include <gl/device.h>
#endif

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm-ogl.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./mged_solid.h"
#include "./sedit.h"

extern void mged_print_result();
int      Ogl_dm_init();
static void     set_knob_offset();
static void     Ogl_statechange();
static int	Ogl_dm();
static int	Ogl_doevent();
static void     Ogl_colorchange();
static void     establish_zbuffer();
static void     establish_lighting();
static void     establish_perspective();
static void     set_perspective();
static void     refresh_hook();
static void     do_linewidth();
static void     do_fog();

#if IR_KNOBS
void ogl_dbtext();
#endif

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

struct bu_structparse Ogl_vparse[] = {
	{"%d",	1, "depthcue",		Ogl_MV_O(cueing_on),	Ogl_colorchange },
	{"%d",  1, "zclip",		Ogl_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		Ogl_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		Ogl_MV_O(lighting_on),	establish_lighting },
 	{"%d",  1, "perspective",       Ogl_MV_O(perspective_mode), establish_perspective },
	{"%d",  1, "set_perspective",   Ogl_MV_O(dummy_perspective),  set_perspective },
	{"%d",  1, "linewidth",		Ogl_MV_O(linewidth),	do_linewidth },
	{"%d",  1, "fastfog",		Ogl_MV_O(fastfog),	do_fog },
	{"%f",  1, "density",		Ogl_MV_O(fogdensity),	refresh_hook },
	{"%d",  1, "has_zbuf",		Ogl_MV_O(zbuf),		FUNC_NULL },
	{"%d",  1, "has_rgb",		Ogl_MV_O(rgb),		FUNC_NULL },
	{"%d",  1, "has_doublebuffer",	Ogl_MV_O(doublebuffer), FUNC_NULL },
	{"%d",  1, "depth",		Ogl_MV_O(depth),	FUNC_NULL },
	{"%d",  1, "debug",		Ogl_MV_O(debug),	FUNC_NULL },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

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

static int OgldoMotion = 0;

int
Ogl_dm_init()
{
  /* register application provided routines */
  dmp->dm_eventHandler = Ogl_doevent;
  cmd_hook = Ogl_dm;
  state_hook = Ogl_statechange;

  return TCL_OK;
}

static int
Ogl_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  static int button0  = 0;   /*  State of button 0 */
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  struct bu_vls cmd;
  struct ogl_vars *p;
  register struct dm_list *save_dm_list;
  int status = TCL_OK;

  GET_DM(p, ogl_vars, eventPtr->xany.window, &head_ogl_vars.l);
  if(p == (struct ogl_vars *)NULL || eventPtr->type == DestroyNotify)
    return TCL_OK;

  bu_vls_init(&cmd);
  save_dm_list = curr_dm_list;

  GET_DM_LIST(curr_dm_list, ogl_vars, eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(!glXMakeCurrent(((struct ogl_vars *)dmp->dm_vars)->dpy,
     ((struct ogl_vars *)dmp->dm_vars)->win,
     ((struct ogl_vars *)dmp->dm_vars)->glxc))
    goto end;

  /* Forward key events to a command window */
  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[2];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables.hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);

    bu_vls_free(&cmd);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  if ( eventPtr->type == Expose && eventPtr->xexpose.count == 0 ) {
    glClearColor(0.0, 0.0, 0.0, 0.0);
    if (((struct ogl_vars *)dmp->dm_vars)->mvars.zbuf)
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    else
       glClear(GL_COLOR_BUFFER_BIT);

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == ConfigureNotify ) {
    Ogl_configure_window_shape(curr_dm_list->_dmp);

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct ogl_vars *)dmp->dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Ogl_irisX2ged(dmp, mx), Ogl_irisY2ged(dmp, my));
      else if(OgldoMotion)
	/* do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", Ogl_irisX2ged(dmp, mx), Ogl_irisY2ged(dmp, my));
      else /* not doing motion */
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
      bu_vls_printf( &cmd, "knob -i ax %f ay %f\n",
		     (my - ((struct ogl_vars *)dmp->dm_vars)->omy) * 0.25,
		     (mx - ((struct ogl_vars *)dmp->dm_vars)->omx) * 0.25 );
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if(EDIT_TRAN && mged_variables.edit){
	  fx = (mx/(fastf_t)((struct ogl_vars *)dmp->dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)((struct ogl_vars *)dmp->dm_vars)->height) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy);
	}else{
	  fx = (mx - ((struct ogl_vars *)dmp->dm_vars)->omx)/
		(fastf_t)((struct ogl_vars *)dmp->dm_vars)->width * 2.0;
	  fy = (((struct ogl_vars *)dmp->dm_vars)->omy - my)/
		(fastf_t)((struct ogl_vars *)dmp->dm_vars)->height * 2.0;
	  bu_vls_printf( &cmd, "knob -i aX %f aY %f\n", fx, fy);
	}
      }	     
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "knob -i aS %f\n",
		     (((struct ogl_vars *)dmp->dm_vars)->omy - my)/
		      (fastf_t)((struct ogl_vars *)dmp->dm_vars)->height);
      break;
    }

      ((struct ogl_vars *)dmp->dm_vars)->omx = mx;
      ((struct ogl_vars *)dmp->dm_vars)->omy = my;
  }
#if IR_KNOBS
  else if( eventPtr->type == ((struct ogl_vars *)dmp->dm_vars)->devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;
    fastf_t f;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      ogl_dbtext(
		(mged_variables.adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(mged_variables.adcflag) {
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_1adc )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit(dv_1adc) + M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      if(mged_variables.rateknobs){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !rate_zoom )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	    M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * rate_zoom)) + M->axis_data[0] -
	    knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob S %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !absolute_zoom )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * absolute_zoom)) + M->axis_data[0] -
	    knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aS %f\n",
		       setting / 512.0 );
      }
      break;
    case DIAL2:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_2adc )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] = dm_unlimit(dv_2adc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob ang2 %d\n",
		      setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Z] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * rate_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob z %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Z] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.85 * absolute_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd , "knob az %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL3:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_distadc)
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] = dm_unlimit(dv_distadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob distadc %d\n",
		       setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_slew[Z] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * rate_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob Z %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Z] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * absolute_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob aZ %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL4:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_yadc)
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] = dm_unlimit(dv_yadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob yadc %d\n",
		      setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Y] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] +=
	      M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * rate_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob y %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Y] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.85 * absolute_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  f = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd , "knob ay %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL5:
      if(mged_variables.rateknobs){
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_slew[Y] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * rate_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];
	  
	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob Y %f\n",
			 setting / 512.0 );
      }else{
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Y] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * absolute_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob aY %f\n",
			 setting / 512.0 );
      }
      break;
    case DIAL6:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_xadc)
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] = dm_unlimit(dv_xadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob xadc %d\n",
		       setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[X] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(512.5 * rate_rotate[X])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob x %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	     ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[X] )
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	      knob_values[M->first_axis];
	  else
	    ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	      dm_unlimit((int)(2.85 * absolute_rotate[X])) + M->axis_data[0] -
	      knob_values[M->first_axis];

	  f = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]) / 512.0;
	  bu_vls_printf( &cmd , "knob ax %f\n", dm_wrap(f) * 180.0);
	}
      }
      break;
    case DIAL7:
      if(mged_variables.rateknobs){
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !rate_slew[X] )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * rate_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob X %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] &&
	   ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] < NOISE &&
	   !absolute_slew[X] )
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] += M->axis_data[0] -
	    knob_values[M->first_axis];
	else
	  ((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis] =
	    dm_unlimit((int)(512.5 * absolute_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = dm_limit(((struct ogl_vars *)dmp->dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aX %f\n",
		       setting / 512.0 );
      }
      break;
    default:
      break;
    }

    /* Keep track of the knob values */
    knob_values[M->first_axis] = M->axis_data[0];
  }
#endif
#if IR_BUTTONS
  else if( eventPtr->type == ((struct ogl_vars *)dmp->dm_vars)->devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      ogl_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      bu_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      bu_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == ((struct ogl_vars *)dmp->dm_vars)->devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1)
      button0 = 0;

    goto end;
  }
#endif
  else
    goto end;

  status = Tcl_Eval(interp, bu_vls_addr(&cmd));
end:
  bu_vls_free(&cmd);
  curr_dm_list = save_dm_list;

  return status;
}

static void
Ogl_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
	switch( b )  {
	case ST_VIEW:
	    /* constant tracking OFF */
	    OgldoMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    OgldoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    OgldoMotion = 0;
	    break;
	default:
	  Tcl_AppendResult(interp, "Ogl_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	/*Ogl_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
	++dmaflag;
}

/*
 *			O G L _ D M
 * 
 *  Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Ogl_dm(argc, argv)
int	argc;
char	**argv;
{
  struct bu_vls	vls;
  int status;
  char *av[6];
  char xstr[32];
  char ystr[32];

  if( !strcmp( argv[0], "set" ) )  {
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_ogl internal variables", Ogl_vparse, (CONST char *)&((struct ogl_vars *)dmp->dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Ogl_vparse, argv[1], (CONST char *)&((struct ogl_vars *)dmp->dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Ogl_vparse, (char *)&((struct ogl_vars *)dmp->dm_vars)->mvars );
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m" )){
    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct ogl_vars *)dmp->dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d", argv[2],
		  Ogl_irisX2ged(dmp, atoi(argv[3])),
		  Ogl_irisY2ged(dmp, atoi(argv[4])));
    status = Tcl_Eval(interp, bu_vls_addr(&vls));
#if 0
    mged_print_result(status);
#endif
    return status;
  }

  status = TCL_OK;

  if( !strcmp( argv[0], "am" )){
    int buttonpress;

    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct ogl_vars *)dmp->dm_vars)->omx = atoi(argv[3]);
    ((struct ogl_vars *)dmp->dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	am_mode = ALT_MOUSE_MODE_ROTATE;
	break;
      case 't':
	am_mode = ALT_MOUSE_MODE_TRANSLATE;
	if(EDIT_TRAN && mged_variables.edit){
	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "knob aX %f aY %f\n",
			(((struct ogl_vars *)dmp->dm_vars)->omx /
			 (fastf_t)((struct ogl_vars *)dmp->dm_vars)->width - 0.5) * 2,
			(0.5 - ((struct ogl_vars *)dmp->dm_vars)->omy /
			 (fastf_t)((struct ogl_vars *)dmp->dm_vars)->height) * 2);
	  status = Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	}

	break;
      case 'z':
	am_mode = ALT_MOUSE_MODE_ZOOM;
	break;
      default:
	Tcl_AppendResult(interp, "dm am: need more parameters\n",
			 "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
    }else{
      am_mode = ALT_MOUSE_MODE_IDLE;
    }

    return status;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

#if IR_KNOBS
void
ogl_dbtext(str)
{
  Tcl_AppendResult(interp, "dm-ogl: You pressed Help key and '",
		   str, "'\n", (char *)NULL);
}
#endif

static void
Ogl_colorchange()
{
  color_soltab();
  if(((struct ogl_vars *)dmp->dm_vars)->mvars.cueing_on) {
    glEnable(GL_FOG);
  }else{
    glDisable(GL_FOG);
  }
#if 0
  dmp->dm_colorchange(dmp);
#endif
  ++dmaflag;
}

static void
establish_zbuffer()
{
  Ogl_establish_zbuffer(dmp);
  ++dmaflag;
}

static void
establish_lighting()
{
  Ogl_establish_lighting(dmp);
  ++dmaflag;
}

static void
establish_perspective()
{
  Ogl_establish_perspective(dmp);
  ++dmaflag;
}

static void
set_perspective()
{
  Ogl_set_perspective(dmp);
  ++dmaflag;
}

static void
do_linewidth()
{
  dmp->dm_setLineAttr(dmp, ((struct ogl_vars *)dmp->dm_vars)->mvars.linewidth, 0);
  ++dmaflag;
}

static void
do_fog()
{
  Ogl_do_fog(dmp);
  ++dmaflag;
}

static void
refresh_hook()
{
  dmaflag = 1;
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i)
    ((struct ogl_vars *)dmp->dm_vars)->knobs[i] = 0;
}
