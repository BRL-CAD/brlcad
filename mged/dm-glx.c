/*
   Just a note:

   These particular commands should not be used in
   mixed mode programming.

   qdevice
   blkqread
   qtest
   getbutton
   getvaluator
   setvaluator
   unqdevice
   mapcolor
   gconfig
   doublebuffer
   RGBmode
   winopen
   foreground
   noborder
   keepaspect
   prefposition
*/
/*
 *			D M - G L X . C
 *
 *  This display manager started out with the guts from DM-4D which
 *  was modified to use mixed-mode (i.e. gl and X )
 *
 *  Authors -
 *      Robert G. Parker
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
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <termio.h>
#undef VMIN		/* is used in vmath.h, too */
#include <ctype.h>

#include <X11/X.h>
#include <gl/glws.h>
#include "tk.h"
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>

#include <gl/gl.h>		/* SGI IRIS library */
#include <gl/device.h>		/* SGI IRIS library */
#include <gl/get.h>		/* SGI IRIS library */
#include <gl/cg2vme.h>		/* SGI IRIS, for DE_R1 defn on IRIX 3 */
#include <gl/addrs.h>		/* SGI IRIS, for DER1_STEREO defn on IRIX 3 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/invent.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm-glx.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./mged_solid.h"
#include "./sedit.h"

int Glx_dm_init();
#if 0
int Glx_close();
#endif
static int      Glx_doevent();
static int      Glx_dm();
static void     Glx_colorchange();
static void     Glx_statechange();
static void     Glx_dbtext();
static void     establish_zbuffer();
static void     establish_lighting();
static void     establish_perspective();
static void     set_perspective();
static void     refresh_hook();
static void     set_knob_offset();
#if 0
static struct dm_list *get_dm_list();
#endif

struct bu_structparse Glx_vparse[] = {
	{"%d",	1, "depthcue",		Glx_MV_O(cueing_on),	Glx_colorchange },
	{"%d",  1, "zclip",		Glx_MV_O(zclipping_on),	refresh_hook },
	{"%d",  1, "zbuffer",		Glx_MV_O(zbuffer_on),	establish_zbuffer },
	{"%d",  1, "lighting",		Glx_MV_O(lighting_on),	establish_lighting },
	{"%d",  1, "perspective",       Glx_MV_O(perspective_mode), establish_perspective },
	{"%d",  1, "set_perspective",Glx_MV_O(dummy_perspective),  set_perspective },
	{"%d",  1, "has_zbuf",		Glx_MV_O(zbuf),	refresh_hook },
	{"%d",  1, "has_rgb",		Glx_MV_O(rgb),	Glx_colorchange },
	{"%d",  1, "has_doublebuffer",	Glx_MV_O(doublebuffer), refresh_hook },
	{"%d",  1, "min_scr_z",		Glx_MV_O(min_scr_z),	refresh_hook },
	{"%d",  1, "max_scr_z",		Glx_MV_O(max_scr_z),	refresh_hook },
	{"%d",  1, "debug",		Glx_MV_O(debug),	FUNC_NULL },
	{"%d",  1, "linewidth",		Glx_MV_O(linewidth),	refresh_hook },
	{"",	0,  (char *)0,		0,			FUNC_NULL }
};

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
/* Inverse map for mapping MGED button functions to SGI button numbers */
static unsigned char invbmap[BV_MAXFUNC+1];

/* bit 0 == switchlight 0 */
static unsigned long lights;
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

static int GlxdoMotion = 0;

int
Glx_dm_init(argc, argv)
int argc;
char *argv[];
{
  if(dmp->dmr_init(dmp, argc, argv) == TCL_ERROR)
    return TCL_ERROR;

  /* register application provided routines */
  dmp->dmr_eventhandler = Glx_doevent;
  dmp->dmr_cmd = Glx_dm;
  dmp->dmr_statechange = Glx_statechange;
#if 0
  dmp->dmr_app_close = Glx_close;
#endif

  return dmp->dmr_open(dmp);
}

#if 0
static int
Glx_close(p)
genptr_t *p;
{
  bu_free(p, "mged_glx_vars");
  return TCL_OK;
}
#endif

/*
   This routine does not handle mouse button or key events. The key
   events are being processed via the TCL/TK bind command or are being
   piped to ged.c/stdin_input(). Eventually, I'd also like to have the
   dials+buttons bindable. That would leave this routine to handle only
   events like Expose and ConfigureNotify.
*/
static int
Glx_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  static int button0  = 0;   /*  State of button 0 */
  static int knobs_during_help[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  static int knob_values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  register struct dm_list *save_dm_list;
  register struct dm_list *p;
  struct bu_vls cmd;
  int status = CMD_OK;

  if(eventPtr->type == DestroyNotify)
    return TCL_OK;

  bu_vls_init(&cmd);
  save_dm_list = curr_dm_list;

#if 0
  curr_dm_list = get_dm_list(eventPtr->xany.window);
#else
  GET_DM_LIST(curr_dm_list, glx_vars, eventPtr->xany.window);
#endif

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  GLXwinset(eventPtr->xany.display, eventPtr->xany.window);

  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[1];
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

  /* Now getting X events */
  if(eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    /* Window may have moved */
    Glx_configure_window_shape(dmp);

    dirty = 1;
    refresh();
    goto end;
  }else if( eventPtr->type == ConfigureNotify ){
      /* Window may have moved */
      Glx_configure_window_shape(dmp);

      dirty = 1;
      refresh();
      goto end;
  }else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct glx_vars *)dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Glx_irisX2ged(dmp, mx), Glx_irisY2ged(dmp, my));
      else if(GlxdoMotion)
	/* do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", Glx_irisX2ged(dmp, mx), Glx_irisY2ged(dmp, my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
      bu_vls_printf( &cmd, "iknob ax %f ay %f\n",
		     (my - ((struct glx_vars *)dm_vars)->omy)/512.0, (mx - ((struct glx_vars *)dm_vars)->omx)/512.0 );
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	  (edobj || es_edflag > 0)){
	  fx = (mx/(fastf_t)((struct glx_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)((struct glx_vars *)dm_vars)->height) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy );
	}else{
	  fx = (mx - ((struct glx_vars *)dm_vars)->omx)/(fastf_t)((struct glx_vars *)dm_vars)->width * 2.0;
	  fy = (((struct glx_vars *)dm_vars)->omy - my)/(fastf_t)((struct glx_vars *)dm_vars)->height * 2.0;
	  bu_vls_printf( &cmd, "iknob aX %f aY %f\n", fx, fy );
	}
      }	     
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "iknob aS %f\n", (((struct glx_vars *)dm_vars)->omy - my)/(fastf_t)((struct glx_vars *)dm_vars)->height);
      break;
    }

    ((struct glx_vars *)dm_vars)->omx = mx;
    ((struct glx_vars *)dm_vars)->omy = my;
  }
#if IR_KNOBS
  else if( eventPtr->type == ((struct glx_vars *)dm_vars)->devmotionnotify ){
    XDeviceMotionEvent *M;
    int setting;

    M = (XDeviceMotionEvent * ) eventPtr;

    if(button0){
      Glx_dbtext(
		(mged_variables.adcflag ? kn1_knobs:kn2_knobs)[M->first_axis]);
      goto end;
    }

    switch(DIAL0 + M->first_axis){
    case DIAL0:
      if(mged_variables.adcflag) {
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	                !dv_1adc )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol(dv_1adc) +
	                  M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd, "knob ang1 %d\n",
		      setting );
      }
      break;
    case DIAL1:
      if(mged_variables.rateknobs){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !rate_zoom )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_zoom)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob S %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !absolute_zoom )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_zoom)) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aS %f\n",
		       setting / 512.0 );
      }
      break;
    case DIAL2:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	                !dv_2adc )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol(dv_2adc) +
	                  M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob ang2 %d\n",
		      setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Z] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob z %f\n",
		      setting / 512.0 );
	}else{
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Z] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob az %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL3:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_distadc)
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol(dv_distadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob distadc %d\n",
		      setting );
      }else {
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_slew[Z] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob Z %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Z] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[Z])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob aZ %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL4:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_yadc)
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol(dv_yadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob yadc %d\n",
		      setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[Y] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob y %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[Y] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob ay %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL5:
      if(mged_variables.rateknobs){
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_slew[Y] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob Y %f\n",
		       setting / 512.0 );
      }else{
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_slew[Y] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[Y])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob aY %f\n",
		       setting / 512.0 );
      }
      break;
    case DIAL6:
      if(mged_variables.adcflag){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !dv_xadc)
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol(dv_xadc) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob xadc %d\n",
		      setting );
      }else{
	if(mged_variables.rateknobs){
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !rate_rotate[X] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_rotate[X])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob x %f\n",
			 setting / 512.0 );
	}else{
	  if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	     !absolute_rotate[X] )
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	  else
	    ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_rotate[X])) +
	      M->axis_data[0] - knob_values[M->first_axis];

	  setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	  bu_vls_printf( &cmd , "knob ax %f\n",
			 setting / 512.0 );
	}
      }
      break;
    case DIAL7:
      if(mged_variables.rateknobs){
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !rate_slew[X] )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * rate_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
	bu_vls_printf( &cmd , "knob X %f\n",
		       setting / 512.0 );
      }else{
	if(-NOISE < ((struct glx_vars *)dm_vars)->knobs[M->first_axis] && ((struct glx_vars *)dm_vars)->knobs[M->first_axis] < NOISE &&
	   !absolute_slew[X] )
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] += M->axis_data[0] - knob_values[M->first_axis];
	else
	  ((struct glx_vars *)dm_vars)->knobs[M->first_axis] = Glx_add_tol((int)(512.5 * absolute_slew[X])) +
	    M->axis_data[0] - knob_values[M->first_axis];

	setting = Glx_irlimit(((struct glx_vars *)dm_vars)->knobs[M->first_axis]);
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
  else if( eventPtr->type == ((struct glx_vars *)dm_vars)->devbuttonpress ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1){
      button0 = 1;
      goto end;
    }

    if(button0){
      Glx_dbtext(label_button(bmap[B->button - 1]));
    }else if(B->button == 4){
      bu_vls_strcat(&cmd, "knob zero\n");
      set_knob_offset();
    }else
      bu_vls_printf(&cmd, "press %s\n",
		    label_button(bmap[B->button - 1]));
  }else if( eventPtr->type == ((struct glx_vars *)dm_vars)->devbuttonrelease ){
    XDeviceButtonEvent *B;

    B = (XDeviceButtonEvent * ) eventPtr;

    if(B->button == 1)
      button0 = 0;

    goto end;
  }
#endif
  else
    goto end;

  status = cmdline(&cmd, FALSE);
end:
  bu_vls_free(&cmd);
  curr_dm_list = save_dm_list;

  if(status == CMD_OK)
    return TCL_OK;

  return TCL_ERROR;
}


static void
Glx_statechange( a, b )
{
  if( ((struct glx_vars *)dm_vars)->mvars.debug ){
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "statechange %d %d\n", a, b );
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  /*
   *  Based upon new state, possibly do extra stuff,
   *  including enabling continuous tablet tracking,
   *  object highlighting
   */
 	switch( b )  {
	case ST_VIEW:
	  /* constant tracking OFF */
	  GlxdoMotion = 0;
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	  /* constant tracking ON */
	  GlxdoMotion = 1;
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  GlxdoMotion = 0;
	  break;
	default:
	  Tcl_AppendResult(interp, "Glx_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	Glx_viewchange( dmp, DM_CHGV_REDO, SOLID_NULL );
}

/*
 *			G L X _ D M
 * 
 *  Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Glx_dm(argc, argv)
int	argc;
char	**argv;
{
  struct bu_vls	vls;
  int status;
  char *av[4];
  char xstr[32];
  char ystr[32];
  char zstr[32];

  if( !strcmp( argv[0], "set" )){
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_4d internal variables", Glx_vparse, (CONST char *)&((struct glx_vars *)dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Glx_vparse, argv[1], (CONST char *)&((struct glx_vars *)dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Glx_vparse, (char *)&((struct glx_vars *)dm_vars)->mvars);
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
		       "m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct glx_vars *)dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct glx_vars *)dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct glx_vars *)dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d\n", argv[2],
		  Glx_irisX2ged(dmp, atoi(argv[3])), Glx_irisY2ged(dmp, atoi(argv[4])));
    status = cmdline(&vls, FALSE);
    bu_vls_free(&vls);

    if(status == CMD_OK)
      return TCL_OK;

    return TCL_ERROR;
  }

  status = TCL_OK;
  if( !strcmp( argv[0], "am" )){
    int buttonpress;

    scroll_active = 0;
    
    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct glx_vars *)dm_vars)->omx = atoi(argv[3]);
    ((struct glx_vars *)dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	am_mode = ALT_MOUSE_MODE_ROTATE;
	break;
      case 't':
	am_mode = ALT_MOUSE_MODE_TRANSLATE;
	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	   (edobj || es_edflag > 0)){
	  fastf_t fx, fy;

	  bu_vls_init(&vls);
	  fx = (((struct glx_vars *)dm_vars)->omx/(fastf_t)((struct glx_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - ((struct glx_vars *)dm_vars)->omy/(fastf_t)((struct glx_vars *)dm_vars)->height) * 2;
	  bu_vls_printf( &vls, "knob aX %f aY %f\n", fx, fy);
	  (void)cmdline(&vls, FALSE);
	  bu_vls_free(&vls);
	}

	break;
      case 'z':
	am_mode = ALT_MOUSE_MODE_ZOOM;
	break;
      default:
	Tcl_AppendResult(interp, "dm am: need more parameters\n",
			 "am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }
    }else
      am_mode = ALT_MOUSE_MODE_IDLE;

    return status;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

/*
 *  I R _ D B T E X T
 *
 *  Used to call dbtext to print cute messages on the button box,
 *  if you have one.  Has to shift everythign to upper case
 *  since the box goes off the deep end with lower case.
 *
 *  Because not all SGI button boxes have text displays,
 *  this now needs to go to stdout in order to be useful.
 */

static void
Glx_dbtext(str)
register char *str;
{
#if IR_BUTTONS
	register i;
	char	buf[9];
	register char *cp;

	Tcl_AppendResult(interp, "dm-glx: You pressed Help key and '",
			 str, "'\n", (char *)NULL);
#else
	return;
#endif
}

static void
set_knob_offset()
{
  int i;

  for(i = 0; i < 8; ++i)
    ((struct glx_vars *)dm_vars)->knobs[i] = 0;
}

static void
Glx_colorchange()
{
  dmp->dmr_colorchange(dmp);
}

static void
establish_zbuffer()
{
  Glx_establish_zbuffer(dmp);
}

static void
establish_lighting()
{
  Glx_establish_lighting(dmp);
}

static void
establish_perspective()
{
  Glx_establish_perspective(dmp);
}

static void
set_perspective()
{
  Glx_set_perspective(dmp);
}

static void
refresh_hook()
{
  dmaflag = 1;
}

#if 0
static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct glx_vars *p;

  for( BU_LIST_FOR(p, glx_vars, &head_glx_vars.l) ){
    if(window == p->win){
      GLXwinset(p->dpy, p->win);
      return ((struct mged_glx_vars *)p->app_vars)->dm_list;
    }
  }

  return DM_LIST_NULL;
}
#endif
