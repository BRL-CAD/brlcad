/*
 *			D M - X . C
 *
 *  An X Window System interface for MGED.
 *  X11R2.  Color support is yet to be implemented.
 *
 *  Author -
 *	Phillip Dykstra
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

#include <math.h>
#include <sys/time.h>		/* for struct timeval */
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
#include "tk.h"
#include <X11/Xutil.h>
#include <X11/extensions/Xext.h>
#include <X11/extensions/multibuf.h>
#include <X11/keysym.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./sedit.h"
#include "dm-X.h"

extern void dm_var_init();
extern void mged_print_result();
extern point_t e_axes_pos;

static void	X_statechange();
static int     X_dm();
static void     establish_perspective();
static void     set_perspective();
static void     set_knob_offset();

#ifdef USE_PROTOTYPES
static Tk_GenericProc X_doevent;
#else
static int X_doevent();
#endif

extern int dm_pipe[];
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct bu_structparse X_vparse[] = {
  {"%d",  1, "perspective",       X_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective",   X_MV_O(dummy_perspective),set_perspective },
  {"%d",  1, "debug",             X_MV_O(debug),            BU_STRUCTPARSE_FUNC_NULL },
  {"",    0, (char *)0,           0,                        BU_STRUCTPARSE_FUNC_NULL }
};

static int XdoMotion = 0;

int
X_dm_init(o_dm_list, argc, argv)
struct dm_list *o_dm_list;
int argc;
char *argv[];
{
  int i;
  char **av;

  /* register application provided routines */
  cmd_hook = X_dm;
  state_hook = X_statechange;

  /* stuff in a default initialization script */
  av = (char **)bu_malloc(sizeof(char *)*(argc + 2), "X_dm_init: av");
  av[0] = "X_open";
  av[1] = "-i";
  av[2] = "mged_bind_dm";

  /* copy the rest except last */
  for(i = 1; i < argc-1; ++i)
    av[i+2] = argv[i];

  av[i+2] = (char *)NULL;

  dm_var_init(o_dm_list);
  Tk_DeleteGenericHandler(X_doevent, (ClientData)DM_TYPE_X);
  if((dmp = dm_open(DM_TYPE_X, DM_EVENT_HANDLER_NULL, argc+1, av)) == DM_NULL){
    bu_free(av, "X_dm_init: av");
    return TCL_ERROR;
  }

  bu_free(av, "X_dm_init: av");
  dmp->dm_eventHandler = X_doevent;
  curr_dm_list->s_info->opp = &pathName;
  Tk_CreateGenericHandler(X_doevent, (ClientData)DM_TYPE_X);
  X_configure_window_shape(dmp);

  return TCL_OK;
}

static int
X_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  KeySym key;
  char keybuf[4];
  int cnt;
  XComposeStatus compose_stat;
  XWindowAttributes xwa;
  struct bu_vls cmd;
  struct x_vars *p;
  register struct dm_list *save_dm_list;
  int status = TCL_OK;
  int save_edflag = -1;

  GET_DM(p, x_vars, eventPtr->xany.window, &head_x_vars.l);
  if(p == (struct x_vars *)NULL || eventPtr->type == DestroyNotify)
    return TCL_OK;

  bu_vls_init(&cmd);
  save_dm_list = curr_dm_list;

  GET_DM_LIST(curr_dm_list, x_vars, eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(mged_variables->send_key && eventPtr->type == KeyPress){
    char buffer[2];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables->hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);

    bu_vls_free(&cmd);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    dirty = 1;
    goto end;
  }else if(eventPtr->type == ConfigureNotify){
    X_configure_window_shape(dmp);

    dirty = 1;
    goto end;
  } else if( eventPtr->type == MapNotify ){
    mapped = 1;
    goto end;
  } else if( eventPtr->type == UnmapNotify ){
    mapped = 0;
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;
    int dx, dy;
    fastf_t f;
    fastf_t fx, fy;
    fastf_t td;

    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;
    dx = mx - ((struct x_vars *)dmp->dm_vars)->omx;
    dy = my - ((struct x_vars *)dmp->dm_vars)->omy;

    switch(am_mode){
    case AMM_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct x_vars *)dmp->dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n",
		       (int)(dm_X2Normal(dmp, mx, 0) * 2047.0),
		       (int)(dm_Y2Normal(dmp, my) * 2047.0) );
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n",
		       (int)(dm_X2Normal(dmp, mx, 1) * 2047.0),
		       (int)(dm_Y2Normal(dmp, my) * 2047.0) );
      else
	goto end;

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
	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;
	goto end;
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
      fx = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      fy = -dy / (fastf_t)dmp->dm_height * 2.0;
      
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
	else
	  bu_vls_printf(&cmd, "knob -i aX %lf aY %lf\n",
			fx*Viewscale*base2local, fy*Viewscale*base2local);

	(void)Tcl_Eval(interp, bu_vls_addr(&cmd));

	mged_variables->coords = save_coords;
	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;
	goto end;
      }

      /* otherwise, drag to translate the view */
      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i -v X %lf Y %lf\n", fx, fy );
      else
	bu_vls_printf( &cmd, "knob -i -v aX %lf aY %lf\n",
		       fx*Viewscale*base2local, fy*Viewscale*base2local );

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
      fx = dm_X2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
      fy = dm_Y2Normal(dmp, my) * 2047.0 - dv_yadc;
      bu_vls_printf(&cmd, "adc a1 %lf\n", DEGRAD*atan2(fy, fx));

      break;
    case AMM_ADC_ANG2:
      fx = dm_X2Normal(dmp, mx, 1) * 2047.0 - dv_xadc;
      fy = dm_Y2Normal(dmp, my) * 2047.0 - dv_yadc;
      bu_vls_printf(&cmd, "adc a2 %lf\n", DEGRAD*atan2(fy, fx));

      break;
    case AMM_ADC_TRAN:
      bu_vls_printf(&cmd, "adc hv %lf %lf\n",
		    dm_X2Normal(dmp, mx, 1) * Viewscale * base2local,
		    dm_Y2Normal(dmp, my) * Viewscale * base2local);

      break;
    case AMM_ADC_DIST:
      fx = (dm_X2Normal(dmp, mx, 1) * 2047.0 - dv_xadc) * Viewscale * base2local / 2047.0;
      fy = (dm_Y2Normal(dmp, my) * 2047.0 - dv_yadc) * Viewscale * base2local / 2047.0;
      td = sqrt(fx * fx + fy * fy);
      bu_vls_printf(&cmd, "adc dst %lf\n", td);

      break;
    case AMM_CON_ROT_X:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i x %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i ax %f\n", f * 0.25 );

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

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;
	goto end;
      }

      break;
    case AMM_CON_ROT_Y:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i y %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i ay %f\n", f * 0.25 );

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

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;
	goto end;
      }

      break;
    case AMM_CON_ROT_Z:
      if(abs(dx) >= abs(dy))
	f = dx;
      else
	f = -dy;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i z %f\n",
		       f / (fastf_t)dmp->dm_width * RATE_ROT_FACTOR * 2.0 );
      else
	bu_vls_printf( &cmd, "knob -i az %f\n", f * 0.25 );

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

	Tcl_Eval(interp, bu_vls_addr(&cmd));

	if(state == ST_S_EDIT)
	  es_edflag = save_edflag;
	else
	  edobj = save_edflag;

	((struct x_vars *)dmp->dm_vars)->omx = mx;
	((struct x_vars *)dmp->dm_vars)->omy = my;
	goto end;
      }

      break;
    case AMM_CON_TRAN_X:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_X;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i X %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aX %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_TRAN_Y:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_Y;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i Y %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aY %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_TRAN_Z:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_TRAN){
	  save_edflag = es_edflag;
	  es_edflag = STRANS;
	}else if(state == ST_O_EDIT && !OEDIT_TRAN){
	  save_edflag = edobj;
	  edobj = BE_O_XY;
	}
      }

      if(abs(dx) >= abs(dy))
	f = dx / (fastf_t)dmp->dm_width / dmp->dm_aspect * 2.0;
      else
	f = -dy / (fastf_t)dmp->dm_height * 2.0;

      if(mged_variables->rateknobs)
	bu_vls_printf( &cmd, "knob -i Z %f\n", f);
      else
	bu_vls_printf( &cmd, "knob -i aZ %f\n", f*Viewscale*base2local);

      break;
    case AMM_CON_SCALE_X:
      if((state == ST_S_EDIT || state == ST_O_EDIT) &&
	 mged_variables->transform == 'e'){
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
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
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
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
	if(state == ST_S_EDIT && !SEDIT_SCALE){
	  save_edflag = es_edflag;
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

      bu_vls_printf( &cmd, "knob -i xadc %f\n",
		     f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );
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

      bu_vls_printf( &cmd, "knob -i distadc %f\n",
		     f / (fastf_t)dmp->dm_width / dmp->dm_aspect * 4095.0 );

      break;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = mx;
    ((struct x_vars *)dmp->dm_vars)->omy = my;
  } else {
    /*XXX Hack to prevent Tk from choking on certain control sequences */
    if(eventPtr->type == KeyPress && eventPtr->xkey.state & ControlMask){
      char buffer[1];
      KeySym keysym;

      XLookupString(&(eventPtr->xkey), buffer, 1,
		    &keysym, (XComposeStatus *)NULL);

      if(keysym == XK_c || keysym == XK_t || keysym == XK_v ||
	 keysym == XK_w || keysym == XK_x || keysym == XK_y){
	curr_dm_list = save_dm_list;

	return TCL_RETURN;
      }
    }

#if 0
    XGetWindowAttributes( ((struct x_vars *)dmp->dm_vars)->dpy,
			  ((struct x_vars *)dmp->dm_vars)->win, &xwa);
    dmp->dm_height = xwa.height;
    dmp->dm_width = xwa.width;
#endif
    goto end;
  }

  status = Tcl_Eval(interp, bu_vls_addr(&cmd));
  if(save_edflag != -1){
    if(SEDIT_TRAN || SEDIT_ROTATE || SEDIT_SCALE)
      es_edflag = save_edflag;
    else if(OEDIT_TRAN || OEDIT_ROTATE || OEDIT_SCALE)
      edobj = save_edflag;
  }
end:
  bu_vls_free(&cmd);
  curr_dm_list = save_dm_list;

  return status;
}
	    
static void
X_statechange( a, b )
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
	    XdoMotion = 0;
	    break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	case ST_S_VPICK:
	    /* constant tracking ON */
	    XdoMotion = 1;
	    break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	    /* constant tracking OFF */
	    XdoMotion = 0;
	    break;
	default:
	  Tcl_AppendResult(interp, "X_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
	  break;
	}

	/*X_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
	++dmaflag;
}

static int
X_dm(argc, argv)
int argc;
char *argv[];
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
      bu_struct_print("dm_X internal variables", X_vparse, (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, X_vparse, argv[1], (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, X_vparse, (char *)&((struct x_vars *)dmp->dm_vars)->mvars );
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
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct x_vars *)dmp->dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    {
      int x;
      int y;
      int old_orig_gui;

      old_orig_gui = mged_variables->orig_gui;

      x = dm_X2Normal(dmp, atoi(argv[3]), 0) * 2047.0;
      y = dm_Y2Normal(dmp, atoi(argv[4])) * 2047.0;

      if(mged_variables->faceplate &&
	 mged_variables->orig_gui &&
	 *argv[2] == '1'){
#define        MENUXLIM        (-1250)
	if(scroll_active)
	   goto end;

	if(x >= MENUXLIM && scroll_select( x, y, 0 ))
	  goto end;

	if(x < MENUXLIM && mmenu_select( y, 0))
	  goto end;
      }

      mged_variables->orig_gui = 0;
      x = dm_X2Normal(dmp, atoi(argv[3]), 1) * 2047.0;

end:
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "M %s %d %d\n", argv[2], x, y);
      status = Tcl_Eval(interp, bu_vls_addr(&vls));
      mged_variables->orig_gui = old_orig_gui;
      bu_vls_free(&vls);

      return status;
    }
  }

  if(!strcmp(argv[0], "am")){
    int buttonpress;

    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm am: need more parameters\n",
		       "dm am <r|t|s> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	am_mode = AMM_ROT;
	break;
      case 't':
	am_mode = AMM_TRAN;

	if(EDIT_TRAN && mged_variables->transform == 'e'){
	  char save_coords;
	  point_t mouse_view_pos;
	  point_t ea_view_pos;
	  point_t diff;

	  save_coords = mged_variables->coords;
	  mged_variables->coords = 'v';

	  MAT4X3PNT(ea_view_pos, model2view, e_axes_pos);
	  mouse_view_pos[X] = (((struct x_vars *)dmp->dm_vars)->omx /
			       (fastf_t)dmp->dm_width - 0.5) / dmp->dm_aspect * 2.0;
	  mouse_view_pos[Y] = (0.5 - ((struct x_vars *)dmp->dm_vars)->omy /
			       (fastf_t)dmp->dm_height) * 2.0;
	  mouse_view_pos[Z] = ea_view_pos[Z];
	  VSUB2(diff, mouse_view_pos, ea_view_pos);
	  VSCALE(diff, diff, Viewscale * base2local);

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "knob aX %lf aY %lf\n", diff[X], diff[Y]);
	  (void)Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  mged_variables->coords = save_coords;
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
			 "dm am <r|t|s> 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }

      return TCL_OK;
    }

    am_mode = AMM_IDLE;
    return TCL_OK;
  }

  if(!strcmp(argv[0], "adc")){
    int buttonpress;
    fastf_t fx, fy;
    fastf_t td; /* tick distance */

    scroll_active = 0;

    if(argc < 5){
      Tcl_AppendResult(interp, "dm adc: need more parameters\n",
		       "dm adc 1|2|t|d 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case '1':
	fx = dm_X2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 - dv_xadc;
	fy = dm_Y2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 - dv_yadc;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "adc a1 %lf\n", DEGRAD*atan2(fy, fx));
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	am_mode = AMM_ADC_ANG1;
	break;
      case '2':
	fx = dm_X2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 - dv_xadc;
	fy = dm_Y2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 - dv_yadc;
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "adc a2 %lf\n", DEGRAD*atan2(fy, fx));
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	am_mode = AMM_ADC_ANG2;
	break;
      case 't':
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "adc hv %lf %lf\n",
		      dm_X2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) *
		      Viewscale * base2local,
		      dm_Y2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) *
		      Viewscale * base2local);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	am_mode = AMM_ADC_TRAN;
	break;
      case 'd':
	fx = (dm_X2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omx, 1) * 2047.0 -
	      dv_xadc) * Viewscale * base2local / 2047.0;
	fy = (dm_Y2Normal(dmp, ((struct x_vars *)dmp->dm_vars)->omy) * 2047.0 -
	      dv_yadc) * Viewscale * base2local / 2047.0;

	td = sqrt(fx * fx + fy * fy);
	bu_vls_init(&vls);
	bu_vls_printf(&vls, "adc dst %lf\n", td);
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	am_mode = AMM_ADC_DIST;
	break;
      default:
	Tcl_AppendResult(interp, "dm adc: unrecognized parameter - ", argv[1],
			  "\ndm adc 1|2|t|d 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }

      return TCL_OK;
    }

    am_mode = AMM_IDLE;
    return TCL_OK;
  }

  if(!strcmp(argv[0], "con")){
    int buttonpress;

    scroll_active = 0;

    if(argc < 6){
      Tcl_AppendResult(interp, "dm con: need more parameters\n",
		       "dm con r|t|s x|y|z 1|0 xpos ypos\n",
		       "dm con a x|y|1|2|d 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[3]);
    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[4]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[5]);

    if(buttonpress){
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
			   "\ndm con a x|y|1|2|d 1|0 xpos ypos\n", (char *)NULL);
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
			 "\ndm con r|t|s x|y|z 1|0 xpos ypos\n", (char *)NULL);
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
			 "\ndm con r|t|s x|y|z 1|0 xpos ypos\n", (char *)NULL);
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
			 "\ndm con r|t|s x|y|z 1|0 xpos ypos\n", (char *)NULL);
	  return TCL_ERROR;
	}
	break;
      default:
	Tcl_AppendResult(interp, "dm con: unrecognized parameter - ", argv[1],
			 "\ndm con r|t|s x|y|z 1|0 xpos ypos\n", (char *)NULL);
	return TCL_ERROR;
      }

      return TCL_OK;
    }

    am_mode = AMM_IDLE;
    return TCL_OK;
  }

  if( !strcmp( argv[0], "size" )){
    int width, height;

    if( argc < 3 ){
      Tcl_AppendResult(interp, "Usage: dm size width height\n", (char *)NULL);
      return TCL_ERROR;
    }

    width = atoi( argv[1] );
    height = atoi( argv[2] );

    Tk_GeometryRequest(((struct x_vars *)dmp->dm_vars)->xtkwin, width, height);

    return TCL_OK;
  }

  Tcl_AppendResult(interp, "dm: bad command - ", argv[0], "\n", (char *)NULL);
  return TCL_ERROR;
}

static void
establish_perspective()
{
  X_establish_perspective(dmp);
  ++dmaflag;
}

static void
set_perspective()
{
  X_set_perspective(dmp);
  ++dmaflag;
}
