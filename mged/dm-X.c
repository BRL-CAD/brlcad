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

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm-X.h"

#include "./ged.h"
#include "./mged_dm.h"
#include "./mged_solid.h"
#include "./sedit.h"

int     X_dm_init();
#if 0
struct dm_list *get_dm_list();
#endif

static void	X_statechange();
static int     X_dm();
static void     establish_perspective();
static void     set_perspective();
#ifdef USE_PROTOTYPES
static Tk_GenericProc X_doevent;
#else
static int X_doevent();
#endif

extern int dm_pipe[];
extern struct device_values dm_values;	/* values read from devices */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct bu_structparse X_vparse[] = {
  {"%d",  1, "perspective",       X_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective",   X_MV_O(dummy_perspective),set_perspective },
  {"%d",  1, "debug",             X_MV_O(debug),            FUNC_NULL },
  {"",    0, (char *)0,           0,                        FUNC_NULL }
};

static int XdoMotion = 0;

int
X_dm_init(argc, argv)
int argc;
char *argv[];
{
  /* register application provided routines */
  dmp->dmr_eventhandler = X_doevent;
  dmp->dmr_cmd = X_dm;
  dmp->dmr_statechange = X_statechange;

  if(dmp->dmr_init(dmp, argc, argv) == TCL_ERROR)
    return TCL_ERROR;

  return dmp->dmr_open(dmp);
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
  register struct dm_list *save_dm_list;
  int status = CMD_OK;

  if(eventPtr->type == DestroyNotify)
    return TCL_OK;

  save_dm_list = curr_dm_list;
#if 0
  curr_dm_list = get_dm_list(eventPtr->xany.window);
#else
  GET_DM_LIST(curr_dm_list, x_vars, eventPtr->xany.window);
#endif

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(mged_variables.send_key && eventPtr->type == KeyPress){
    char buffer[1];
    KeySym keysym;

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  &keysym, (XComposeStatus *)NULL);

    if(keysym == mged_variables.hot_key)
      goto end;

    write(dm_pipe[1], buffer, 1);
    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }

  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    dirty = 1;
    refresh();
    goto end;
  }else if(eventPtr->type == ConfigureNotify){
    X_configure_window_shape(dmp);

    dirty = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    bu_vls_init(&cmd);
    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct x_vars *)dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Xx_TO_GED(dmp, mx), Xy_TO_GED(dmp, my));
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", Xx_TO_GED(dmp, mx), Xy_TO_GED(dmp, my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
       bu_vls_printf( &cmd, "iknob ax %f ay %f\n",
		      (my - ((struct x_vars *)dm_vars)->omy)/512.0,
		      (mx - ((struct x_vars *)dm_vars)->omx)/512.0 );
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	   (edobj || es_edflag > 0)){
	  fx = (mx/(fastf_t)((struct x_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)((struct x_vars *)dm_vars)->height) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy );
	}else{
	  fx = (mx - ((struct x_vars *)dm_vars)->omx) /
	    (fastf_t)((struct x_vars *)dm_vars)->width * 2.0;
	  fy = (((struct x_vars *)dm_vars)->omy - my) /
	    (fastf_t)((struct x_vars *)dm_vars)->height * 2.0;
	  bu_vls_printf( &cmd, "iknob aX %f aY %f\n", fx, fy );
	}
      }
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "iknob aS %f\n",
		     (((struct x_vars *)dm_vars)->omy - my)/
		     (fastf_t)((struct x_vars *)dm_vars)->height);
      break;
    }

    ((struct x_vars *)dm_vars)->omx = mx;
    ((struct x_vars *)dm_vars)->omy = my;
  } else {
    XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy,
			  ((struct x_vars *)dm_vars)->win, &xwa);
    ((struct x_vars *)dm_vars)->height = xwa.height;
    ((struct x_vars *)dm_vars)->width = xwa.width;

    goto end;
  }

  status = cmdline(&cmd, FALSE);
  bu_vls_free(&cmd);
end:
  curr_dm_list = save_dm_list;

  if(status == CMD_OK)
    return TCL_OK;

  return TCL_ERROR;
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
}

static int
X_dm(argc, argv)
int argc;
char *argv[];
{
  struct bu_vls   vls;
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
      bu_struct_print("dm_X internal variables", X_vparse,
		     (CONST char *)&((struct x_vars *)dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, X_vparse, argv[1],
			 (CONST char *)&((struct x_vars *)dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, X_vparse, (char *)&((struct x_vars *)dm_vars)->mvars);
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m")){
    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct x_vars *)dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct x_vars *)dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct x_vars *)dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d\n", argv[2],
		  Xx_TO_GED(dmp, atoi(argv[3])), Xy_TO_GED(dmp, atoi(argv[4])));
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
		       "dm am <r|t|z> 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    buttonpress = atoi(argv[2]);
    ((struct x_vars *)dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dm_vars)->omy = atoi(argv[4]);

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
	  fx = (((struct x_vars *)dm_vars)->omx/
		(fastf_t)((struct x_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - ((struct x_vars *)dm_vars)->omy/
		(fastf_t)((struct x_vars *)dm_vars)->height) * 2;
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

static void
establish_perspective()
{
  X_establish_perspective(dmp);
}

static void
set_perspective()
{
  X_set_perspective(dmp);
}

#if 0
static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct dm_list *p;

/*XXXX*/
  for(BU_LIST_FOR(p, dm_list, &head_dm_list.l)){
    if(window == p->win)
	return ((struct mged_x_vars *)p->app_vars)->dm_list;
  }

  return DM_LIST_NULL;
}
#endif
