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

static void	X_statechange();
static int     X_dm();
static void     establish_perspective();
static void     set_perspective();
static void     set_linewidth();
static void     set_linestyle();
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
  {"%d",  1, "linewidth",	  X_MV_O(linewidth),	set_linewidth },
  {"%d",  1, "linestyle",	  X_MV_O(linestyle),	set_linestyle },
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

  GET_DM(p, x_vars, eventPtr->xany.window, &head_x_vars.l);
  if(p == (struct x_vars *)NULL || eventPtr->type == DestroyNotify)
    return TCL_OK;

  save_dm_list = curr_dm_list;
  GET_DM_LIST(curr_dm_list, x_vars, eventPtr->xany.window);

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

    bu_vls_init(&cmd);
    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(am_mode){
    case ALT_MOUSE_MODE_IDLE:
      if(scroll_active && eventPtr->xmotion.state & ((struct x_vars *)dmp->dm_vars)->mb_mask)
	bu_vls_printf( &cmd, "M 1 %d %d\n", Xx_TO_GED(dmp, mx), Xy_TO_GED(dmp, my));
      else if(XdoMotion)
	/* trackball not active so do the regular thing */
	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	bu_vls_printf( &cmd, "M 0 %d %d\n",
		       (int)(Xx_TO_GED(dmp, mx) / dmp->dm_aspect),
		       Xy_TO_GED(dmp, my));
      else
	goto end;

      break;
    case ALT_MOUSE_MODE_ROTATE:
       bu_vls_printf( &cmd, "knob -i ax %f ay %f\n",
		      (my - ((struct x_vars *)dmp->dm_vars)->omy) * 0.25,
		      (mx - ((struct x_vars *)dmp->dm_vars)->omx) * 0.25 );
      break;
    case ALT_MOUSE_MODE_TRANSLATE:
      if(EDIT_TRAN && mged_variables.edit){
	vect_t view_pos;
#if 0
	view_pos[X] = (mx/(fastf_t)dmp->dm_width
		       - 0.5) * 2.0;
#else
	view_pos[X] = (mx/(fastf_t)dmp->dm_width - 0.5) /
	              dmp->dm_aspect * 2.0;
#endif
	view_pos[Y] = (0.5 - my/
		       (fastf_t)dmp->dm_height) * 2.0;
	view_pos[Z] = 0.0;

	if(state == ST_S_EDIT)
	  sedit_mouse(view_pos);
	else
	  objedit_mouse(view_pos);

	goto end;
      }else{
	fastf_t fx, fy;

#if 0
	fx = (mx - ((struct x_vars *)dmp->dm_vars)->omx) /
	  (fastf_t)dmp->dm_width * 2.0;
#else
	fx = (mx - ((struct x_vars *)dmp->dm_vars)->omx) /
	  (fastf_t)dmp->dm_width /
	  dmp->dm_aspect * 2.0;
#endif
	fy = (((struct x_vars *)dmp->dm_vars)->omy - my) /
	  (fastf_t)dmp->dm_height * 2.0;
	bu_vls_printf( &cmd, "knob -i aX %f aY %f\n", fx, fy );
      }

      break;
    case ALT_MOUSE_MODE_ZOOM:
      bu_vls_printf( &cmd, "knob -i aS %f\n",
		     (((struct x_vars *)dmp->dm_vars)->omy - my)/
		     (fastf_t)dmp->dm_height);
      break;
    }

    ((struct x_vars *)dmp->dm_vars)->omx = mx;
    ((struct x_vars *)dmp->dm_vars)->omy = my;
  } else {
    /*XXX Hack to prevent Tk from choking on Ctrl-c */
    if(eventPtr->type == KeyPress && eventPtr->xkey.state & ControlMask){
      char buffer[1];
      KeySym keysym;

      XLookupString(&(eventPtr->xkey), buffer, 1,
		    &keysym, (XComposeStatus *)NULL);

      if(keysym == XK_c){
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
  bu_vls_free(&cmd);
end:
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
  struct bu_vls   vls;
  int status;
  char *av[6];
  char xstr[32];
  char ystr[32];

  if( !strcmp( argv[0], "set" )){
    struct bu_vls tmp_vls;

    bu_vls_init(&vls);
    bu_vls_init(&tmp_vls);
    start_catching_output(&tmp_vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      bu_struct_print("dm_X internal variables", X_vparse,
		     (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars );
    } else if( argc == 2 ) {
      bu_vls_struct_item_named( &vls, X_vparse, argv[1],
			 (CONST char *)&((struct x_vars *)dmp->dm_vars)->mvars, ',');
      bu_log( "%s\n", bu_vls_addr(&vls) );
    } else {
      bu_vls_printf( &vls, "%s=\"", argv[1] );
      bu_vls_from_argv( &vls, argc-2, argv+2 );
      bu_vls_putc( &vls, '\"' );
      bu_struct_parse( &vls, X_vparse, (char *)&((struct x_vars *)dmp->dm_vars)->mvars);
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
      int old_show_menu;

      old_show_menu = mged_variables.show_menu;
      x = Xx_TO_GED(dmp, atoi(argv[3]));
      y = Xy_TO_GED(dmp, atoi(argv[4]));

      if(mged_variables.faceplate &&
	 mged_variables.show_menu &&
	 *argv[2] == '1'){
#define        MENUXLIM        (-1250)
	if(scroll_active)
	  goto end;

	if(x >= MENUXLIM && scroll_select( x, y, 0 ))
	  goto end;

	if(x < MENUXLIM && mmenu_select( y, 0))
	  goto end;
      }

      x = (int)(x / dmp->dm_aspect);
      mged_variables.show_menu = 0;
end:
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "M %s %d %d", argv[2], x, y);
      status = Tcl_Eval(interp, bu_vls_addr(&vls));
      mged_variables.show_menu = old_show_menu;
      bu_vls_free(&vls);

      return status;
    }
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
    ((struct x_vars *)dmp->dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dmp->dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	am_mode = ALT_MOUSE_MODE_ROTATE;
	break;
      case 't':
	am_mode = ALT_MOUSE_MODE_TRANSLATE;

	if(EDIT_TRAN && mged_variables.edit){
	  vect_t view_pos;

#if 0
	  view_pos[X] = (((struct x_vars *)dmp->dm_vars)->omx /
			 (fastf_t)dmp->dm_width -
			 0.5) * 2.0;
#else
	  view_pos[X] = (((struct x_vars *)dmp->dm_vars)->omx /
			(fastf_t)dmp->dm_width - 0.5) /
	                dmp->dm_aspect * 2.0; 
#endif
	  view_pos[Y] = (0.5 - ((struct x_vars *)dmp->dm_vars)->omy /
			 (fastf_t)dmp->dm_height) * 2.0;
	  view_pos[Z] = 0.0;

	  if(state == ST_S_EDIT)
	    sedit_mouse(view_pos);
	  else
	    objedit_mouse(view_pos);
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

static void
set_linewidth()
{
  dmp->dm_setLineAttr(dmp,
		      ((struct x_vars *)dmp->dm_vars)->mvars.linewidth,
		      dmp->dm_lineStyle);
  ++dmaflag;
}

static void
set_linestyle()
{
  dmp->dm_setLineAttr(dmp, dmp->dm_lineWidth,
		      ((struct x_vars *)dmp->dm_vars)->mvars.linestyle);
  ++dmaflag;
}
