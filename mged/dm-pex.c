/* Experimental. */
/*
 *			D M - P E X . C
 *
 *  An X Window System interface for MGED
 *  that uses PEX. This display manager started
 *  out with the innards from DM-X.C
 *
 *  Author -
 *      Robert Parker
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

#define DO_XSELECTINPUT 0
#define SET_COLOR( r, g, b, c ) { \
	(c).rgb.red = (r); \
	(c).rgb.green = (g); \
	(c).rgb.blue = (b);}

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
#include <X11/PEX5/PEXlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/multibuf.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm-pex.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "./mged_solid.h"
#include "./sedit.h"

int Pex_dm_init();
static void	Pex_statechange();
static int     Pex_dm();
static void     establish_perspective();
static void     set_perspective();
#ifdef USE_PROTOTYPES
static Tk_GenericProc Pex_doevent;
#else
static int Pex_doevent();
#endif

extern int dm_pipe[];
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct bu_structparse Pex_vparse[] = {
#if TRY_DEPTHCUE
  {"%d",  1, "depthcue",	Pex_MV_O(cue),	Pex_colorchange },
#endif
  {"%d",  1, "perspective",     Pex_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective", Pex_MV_O(dummy_perspective),  set_perspective },
  {"",    0,  (char *)0,          0,                      FUNC_NULL }
};

#if !DO_XSELECTINPUT
static int XdoMotion = 0;
#endif

int
Pex_dm_init(argc, argv)
int argc;
char *argv[];
{
  /* register application provided routines */
  dmp->dmr_eventhandler = Pex_doevent;
  dmp->dmr_cmd = Pex_dm;
  dmp->dmr_statechange = Pex_statechange;

  if(dmp->dmr_init(dmp, argc, argv) == TCL_ERROR)
    return TCL_ERROR;

  return dmp->dmr_open(dmp);
}

static int
Pex_doevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  KeySym key;
  char keybuf[4];
  int cnt;
  XComposeStatus compose_stat;
  XWindowAttributes xwa;
  struct bu_vls cmd;
  struct pex_vars *p;
  register struct dm_list *save_dm_list;
  int status = CMD_OK;

  GET_DM(p, pex_vars, eventPtr->xany.window, &head_pex_vars.l);
  if(p == (struct pex_vars *)NULL || eventPtr->type == DestroyNotify)
    return TCL_OK;

  save_dm_list = curr_dm_list;
  GET_DM_LIST(curr_dm_list, pex_vars, eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

#if 0
  /* Not interested */
  if (eventPtr->xany.window != ((struct pex_vars *)dm_vars)->win){
    curr_dm_list = save_dm_list;
    return TCL_OK;
  }
#endif

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
#if 1
    Pex_configure_window_shape(dmp);
#else
    ((struct pex_vars *)dm_vars)->height = eventPtr->xconfigure.height;
    ((struct pex_vars *)dm_vars)->width = eventPtr->xconfigure.width;
#endif

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
      if(scroll_active && eventPtr->xmotion.state & ((struct pex_vars *)dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Xx_TO_GED(mx), Xy_TO_GED(my));
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n", Xx_TO_GED(mx), Xy_TO_GED(my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
      bu_vls_printf( &cmd, "iknob ax %f ay %f\n",
		     (my - ((struct pex_vars *)dm_vars)->omy)/512.0,
		     (mx - ((struct pex_vars *)dm_vars)->omx)/512.0);
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      {
	fastf_t fx, fy;

	if((state == ST_S_EDIT || state == ST_O_EDIT) && !EDIT_ROTATE &&
	  (edobj || es_edflag > 0)){
	  fx = (mx/(fastf_t)((struct pex_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - my/(fastf_t)((struct pex_vars *)dm_vars)->height) * 2;
	  bu_vls_printf( &cmd, "knob aX %f aY %f\n", fx, fy );
	}else{
	  fx = (mx - ((struct pex_vars *)dm_vars)->omx) /
	    (fastf_t)((struct pex_vars *)dm_vars)->width * 2.0;
	  fy = (((struct pex_vars *)dm_vars)->omy - my) /
	    (fastf_t)((struct pex_vars *)dm_vars)->height * 2.0;
	  bu_vls_printf( &cmd, "iknob aX %f aY %f\n", fx, fy );
	}
      }	     
      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "iknob aS %f\n",
		     (((struct pex_vars *)dm_vars)->omy - my)/
		     (fastf_t)((struct pex_vars *)dm_vars)->height);
      break;
    }

    ((struct pex_vars *)dm_vars)->omx = mx;
    ((struct pex_vars *)dm_vars)->omy = my;
  } else {
    XGetWindowAttributes( ((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, &xwa);
    ((struct pex_vars *)dm_vars)->height = xwa.height;
    ((struct pex_vars *)dm_vars)->width = xwa.width;

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
	    
void
Pex_statechange( a, b )
int	a, b;
{
	/*
	 *  Based upon new state, possibly do extra stuff,
	 *  including enabling continuous tablet tracking,
	 *  object highlighting
	 */
#if DO_XSELECTINPUT
 	switch( b )  {
	case ST_VIEW:
	  /* constant tracking OFF */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	  /* constant tracking ON */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask|PointerMotionMask);
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  XSelectInput(((struct pex_vars *)dm_vars)->dpy, ((struct pex_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
#else
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
#endif
	default:
	    bu_log("Pex_statechange: unknown state %s\n", state_str[b]);
	    break;
	}

	/*Pex_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
	++dmaflag;
}

int
Pex_dm(argc, argv)
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
      bu_struct_print("dm_X internal variables", Pex_vparse,
		     (CONST char *)&((struct pex_vars *)dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, Pex_vparse, argv[1],
			 (CONST char *)&((struct pex_vars *)dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, Pex_vparse, (char *)&((struct pex_vars *)dm_vars)->mvars);
    }

    bu_vls_free(&vls);

    stop_catching_output(&tmp_vls);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
    return TCL_OK;
  }

  if( !strcmp( argv[0], "m")){
    int up;
    int xpos, ypos;

    scroll_active = 0;

    if( argc < 5){
      Tcl_AppendResult(interp, "dm m: need more parameters\n",
		       "dm m button 1|0 xpos ypos\n", (char *)NULL);
      return TCL_ERROR;
    }

    /* This assumes a 3-button mouse */
    switch(*argv[1]){
    case '1':
      ((struct pex_vars *)dm_vars)->mb_mask = Button1Mask;
      break;
    case '2':
      ((struct pex_vars *)dm_vars)->mb_mask = Button2Mask;
      break;
    case '3':
      ((struct pex_vars *)dm_vars)->mb_mask = Button3Mask;
      break;
    default:
      Tcl_AppendResult(interp, "dm m: bad button value - ", argv[1], "\n", (char *)NULL);
      return TCL_ERROR;
    }

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "M %s %d %d\n", argv[2],
		  Xx_TO_GED(atoi(argv[3])), Xy_TO_GED(atoi(argv[4])));
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
    ((struct pex_vars *)dm_vars)->omx = atoi(argv[3]);
    ((struct pex_vars *)dm_vars)->omy = atoi(argv[4]);

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
	  fx = (((struct pex_vars *)dm_vars)->omx/
		(fastf_t)((struct pex_vars *)dm_vars)->width - 0.5) * 2;
	  fy = (0.5 - ((struct pex_vars *)dm_vars)->omy/
		(fastf_t)((struct pex_vars *)dm_vars)->height) * 2;

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
  Pex_establish_perspective(dmp);
  ++dmaflag;
}

static void
set_perspective()
{
  Pex_set_perspective(dmp);
  ++dmaflag;
}
