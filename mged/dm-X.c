#ifdef MULTI_ATTACH
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

#define DO_XSELECTINPUT 0
#define TRY_PIPES 1
#define TRY_MULTIBUFFERING 0  /* Leave this off. The Multibuffering extension doesn't work */

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
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

#if TRY_PIPES
extern int ged_pipe[];
#endif

static void	label();
static void	draw();
static int	xsetup();
static void     x_var_init();
static void     establish_perspective();
static void     set_perspective();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	X_open();
void	X_close();
MGED_EXTERN(void	X_input, (fd_set *input, int noblock) );
void	X_prolog(), X_epilog();
void	X_normal(), X_newrot();
void	X_update();
void	X_puts(), X_2d_line(), X_light();
int	X_object();
unsigned X_cvtvecs(), X_load();
void	X_statechange(), X_viewchange(), X_colorchange();
void	X_window(), X_debug(), X_selectargs();
int     X_dm();

static int   X_load_startup();
static struct dm_list *get_dm_list();
#ifdef USE_PROTOTYPES
static Tk_GenericProc Xdoevent;
#else
static int Xdoevent();
#endif

struct dm dm_X = {
	X_open, X_close,
	X_input,
	X_prolog, X_epilog,
	X_normal, X_newrot,
	X_update,
	X_puts, X_2d_line,
	X_light,
	X_object,	X_cvtvecs, X_load,
	X_statechange,
	X_viewchange,
	X_colorchange,
	X_window, X_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	PLOTBOUND,
	"X", "X Window System (X11)",
	0,
	X_dm
};

extern struct device_values dm_values;	/* values read from devices */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

struct modifiable_x_vars {
  int perspective_mode;
  int dummy_perspective;
};

struct x_vars {
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
#if TRY_MULTIBUFFERING
  Multibuffer buffers[2];
  Multibuffer curr_buf;
  int doublebuffer;
#endif
  int width;
  int height;
  GC gc;
  XFontStruct *fontstruct;
  int is_monochrome;
  unsigned long black,gray,white,yellow,red,blue;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
  struct modifiable_x_vars mvars;
};

#define X_MV_O(_m) offsetof(struct modifiable_x_vars, _m)
struct structparse X_vparse[] = {
  {"%d",  1, "perspective",       X_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective", X_MV_O(dummy_perspective),  set_perspective },
  {"",    0,  (char *)0,          0,                      FUNC_NULL }
};

static int perspective_angle = 3;	/* Angle of perspective */
static int perspective_table[] = { 
	30, 45, 60, 90 };

#if !DO_XSELECTINPUT
static int XdoMotion = 0;
#endif

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	((int)((x/4096.0+0.5)*((struct x_vars *)dm_vars)->width))
#define	GED_TO_Xy(x)	((int)((0.5-x/4096.0)*((struct x_vars *)dm_vars)->height))
#define Xx_TO_GED(x)    ((int)((x/(double)((struct x_vars *)dm_vars)->width - 0.5) * 4095))
#define Xy_TO_GED(x)    ((int)((0.5 - x/(double)((struct x_vars *)dm_vars)->height) * 4095))

/*
 *			X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
X_open()
{
  char	line[82];
  char	hostname[80];
  char	display[82];
  char	*envp;

  x_var_init();
  /* get or create the default display */
  if( (envp = getenv("DISPLAY")) == NULL ) {
    /* Env not set, use local host */
    gethostname( hostname, 80 );
    hostname[79] = '\0';
    (void)sprintf( display, "%s:0", hostname );
    envp = display;
  }

  rt_log("X Display [%s]? ", envp );
  (void)fgets( line, sizeof(line), stdin );
  line[strlen(line)-1] = '\0';		/* remove newline */
  if( feof(stdin) )  quit();
  if( line[0] != '\0' ) {
    if( xsetup(line) )
      return(1);		/* BAD */
  } else {
    if( xsetup(envp) )
      return(1);	/* BAD */
  }

  /* Ignore the old scrollbars and menus */
  mged_variables.show_menu = 0;

  return(0);			/* OK */
}

/*
 *  			X _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
X_close()
{
  if(((struct x_vars *)dm_vars)->gc != 0)
    XFreeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc);

  if(((struct x_vars *)dm_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct x_vars *)dm_vars)->xtkwin);

#if 1
  Tk_DeleteGenericHandler(Xdoevent, (ClientData)curr_dm_list);
  rt_free(dm_vars, "X_close: dm_vars");
  rt_vls_free(&pathName);
#else
  /* to prevent events being processed after window destroyed */
  win = -1;
#endif
}

/*
 *			X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
X_prolog()
{
#if TRY_MULTIBUFFERING
  if(!((struct x_vars *)dm_vars)->doublebuffer)
    XClearWindow(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win);
#else
    XClearWindow(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win);
#endif
}

/*
 *			X _ E P I L O G
 */
void
X_epilog()
{
    /* Put the center point up last */
    draw( 0, 0, 0, 0 );

#if TRY_MULTIBUFFERING
    if(((struct x_vars *)dm_vars)->doublebuffer){
      /* dpy, size of buffer array, buffer array, minimum delay, maximum delay */
      XmbufDisplayBuffers(((struct x_vars *)dm_vars)->dpy, 1,
			  &((struct x_vars *)dm_vars)->curr_buf, 0, 0);

      /* swap buffers */
      ((struct x_vars *)dm_vars)->curr_buf =
	((struct x_vars *)dm_vars)->curr_buf == ((struct x_vars *)dm_vars)->buffers[0] ?
	((struct x_vars *)dm_vars)->buffers[1] : ((struct x_vars *)dm_vars)->buffers[0];
    }
#endif
    /* Prevent lag between events and updates */
    XSync(((struct x_vars *)dm_vars)->dpy, 0);
}

/*
 *  			X _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
X_newrot(mat)
mat_t mat;
{
	return;
}

/*
 *  			X _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
int
X_object( sp, mat, ratio, white_flag )
register struct solid *sp;
mat_t mat;
double ratio;
int white_flag;
{
    static vect_t   pnt;
    register struct rt_vlist	*vp;
    int useful = 0;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    XGCValues gcv;
    int	nseg;			        /* number of segments */
    int	x, y;
    int	lastx = 0;
    int	lasty = 0;

    XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv);

    if( sp->s_soldash )
	XSetLineAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, 1, LineOnOffDash, CapButt, JoinMiter );
    else
	XSetLineAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, 1, LineSolid, CapButt, JoinMiter );

    nseg = 0;
    segp = segbuf;
    for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
	register int	i;
	register int	nused = vp->nused;
	register int	*cmd = vp->cmd;
	register point_t *pt = vp->pt;

	/* Viewing region is from -1.0 to +1.0 */
	/* 2^31 ~= 2e9 -- dynamic range of a long int */
	/* 2^(31-11) = 2^20 ~= 1e6 */
	/* Integerize and let the X server do the clipping */
	for( i = 0; i < nused; i++,cmd++,pt++ )  {
	    switch( *cmd )  {
	    case RT_VLIST_POLY_START:
	    case RT_VLIST_POLY_VERTNORM:
		continue;
	    case RT_VLIST_POLY_MOVE:
	    case RT_VLIST_LINE_MOVE:
		/* Move, not draw */
		MAT4X3PNT( pnt, mat, *pt );
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		   pnt[1] < -1e6 || pnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */
		pnt[0] *= 2047;
		pnt[1] *= 2047;
		x = GED_TO_Xx(pnt[0]);
		y = GED_TO_Xy(pnt[1]);
		lastx = x;
		lasty = y;
		continue;
	    case RT_VLIST_POLY_DRAW:
	    case RT_VLIST_POLY_END:
	    case RT_VLIST_LINE_DRAW:
		/* draw */
		MAT4X3PNT( pnt, mat, *pt );
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		   pnt[1] < -1e6 || pnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */
		/* Integerize and let the X server do the clipping */
		/*XXX Color */
		gcv.foreground = ((struct x_vars *)dm_vars)->fg;
		if( white_flag && !((struct x_vars *)dm_vars)->is_monochrome ){
		    gcv.foreground = ((struct x_vars *)dm_vars)->white;
		}
		XChangeGC( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv );
		
		pnt[0] *= 2047;
		pnt[1] *= 2047;
		x = GED_TO_Xx(pnt[0]);
		y = GED_TO_Xy(pnt[1]);

		segp->x1 = lastx;
		segp->y1 = lasty;
		segp->x2 = x;
		segp->y2 = y;
		nseg++;
		segp++;
		lastx = x;
		lasty = y;
		useful = 1;
		if( nseg == 1024 ) {
		    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
		    /* Thicken the drawing, if monochrome */
		    if( white_flag && ((struct x_vars *)dm_vars)->is_monochrome ){
			int	i;
			/* XXX - width and height don't work on Sun! */
			/* Thus the following gross hack */
			segp = segbuf;
			for( i = 0; i < nseg; i++ ) {
			    segp->x1++;
			    segp->y1++;
			    segp->x2++;
			    segp->y2++;
			    segp++;
			}
			XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
		    }
		    nseg = 0;
		    segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
	XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
	if( white_flag && ((struct x_vars *)dm_vars)->is_monochrome ){
	    int	i;
	    /* XXX - width and height don't work on Sun! */
	    /* Thus the following gross hack */
	    segp = segbuf;
	    for( i = 0; i < nseg; i++ ) {
		segp->x1++;
		segp->y1++;
		segp->x2++;
		segp->y2++;
		segp++;
	    }
	    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
	}
    }

    return(useful);
}

/*
 *			X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
X_normal()
{
	return;
}

/*
 *			X _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
X_update()
{
    XFlush(((struct x_vars *)dm_vars)->dpy);
}

/*
 *			X _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
X_puts( str, x, y, size, color )
register char *str;
{
	XGCValues gcv;
	unsigned long fg;

	switch( color )  {
	case DM_BLACK:
		fg = ((struct x_vars *)dm_vars)->black;
		break;
	case DM_RED:
		fg = ((struct x_vars *)dm_vars)->red;
		break;
	case DM_BLUE:
		fg = ((struct x_vars *)dm_vars)->blue;
		break;
	default:
	case DM_YELLOW:
		fg = ((struct x_vars *)dm_vars)->yellow;
		break;
	case DM_WHITE:
		fg = ((struct x_vars *)dm_vars)->gray;
		break;
	}
	gcv.foreground = fg;
	XChangeGC( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv );
	label( (double)x, (double)y, str );
}

/*
 *			X _ 2 D _ G O T O
 *
 */
void
X_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
    XGCValues gcv;

    gcv.foreground = ((struct x_vars *)dm_vars)->yellow;
    XChangeGC( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv );
    if( dashed ) {
	XSetLineAttributes(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, 1, LineOnOffDash, CapButt, JoinMiter );
    } else {
	XSetLineAttributes(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, 1, LineSolid, CapButt, JoinMiter );
    }
    draw( x1, y1, x2, y2 );
}

static int
Xdoevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  KeySym key;
  char keybuf[4];
  int cnt;
  XComposeStatus compose_stat;
  XWindowAttributes xwa;
  struct rt_vls cmd;
  register struct dm_list *save_dm_list;

  save_dm_list = curr_dm_list;

  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

#if 0
  /* Not interested */
  if (eventPtr->xany.window != ((struct x_vars *)dm_vars)->win){
    curr_dm_list = save_dm_list;
    return TCL_OK;
  }
#endif

#if TRY_PIPES
  if(mged_variables.focus && eventPtr->type == KeyPress){
    char buffer[1];

    XLookupString(&(eventPtr->xkey), buffer, 1,
		  (KeySym *)NULL, (XComposeStatus *)NULL);

    write(ged_pipe[1], buffer, 1);

    curr_dm_list = save_dm_list;

    /* Use this so that these events won't propagate */
    return TCL_RETURN;
  }
#endif

  if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
    XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, &xwa);
    ((struct x_vars *)dm_vars)->height = xwa.height;
    ((struct x_vars *)dm_vars)->width = xwa.width;

    dmaflag = 1;
    refresh();
    goto end;
  }else if(eventPtr->type == ConfigureNotify){
    XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, &xwa);
    ((struct x_vars *)dm_vars)->height = xwa.height;
    ((struct x_vars *)dm_vars)->width = xwa.width;

    dmaflag = 1;
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int	x, y;

#if !DO_XSELECTINPUT
    if ( !XdoMotion )
      return TCL_OK;
#endif

    x = (eventPtr->xmotion.x/(double)((struct x_vars *)dm_vars)->width - 0.5) * 4095;
    y = (0.5 - eventPtr->xmotion.y/(double)((struct x_vars *)dm_vars)->height) * 4095;
    /* Constant tracking (e.g. illuminate mode) bound to M mouse */
    rt_vls_init(&cmd);
    rt_vls_printf( &cmd, "M 0 %d %d\n", x, y );
  } else {
    XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, &xwa);
    ((struct x_vars *)dm_vars)->height = xwa.height;
    ((struct x_vars *)dm_vars)->width = xwa.width;

    goto end;
  }

  (void)cmdline(&cmd, FALSE);
  rt_vls_free(&cmd);
end:
  curr_dm_list = save_dm_list;
  return TCL_OK;
}
	    
/*
 *			X _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 *
 * DEPRECATED 
 *
 */
/* ARGSUSED */
void
X_input( input, noblock )
fd_set		*input;
int		noblock;
{
    return;
}

/* 
 *			X _ L I G H T
 */
/* ARGSUSED */
void
X_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
X_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
X_load( addr, count )
unsigned addr, count;
{
	rt_log("X_load(x%x, %d.)\n", addr, count );
	return( 0 );
}

void
X_statechange( a, b )
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
	  XSelectInput(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	  /* constant tracking ON */
	  XSelectInput(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask|PointerMotionMask);
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  XSelectInput(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ExposureMask|ButtonPressMask|
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
	    rt_log("X_statechange: unknown state %s\n", state_str[b]);
	    break;
	}

	/*X_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
X_viewchange()
{
}

void
X_colorchange()
{
	color_soltab();		/* apply colors to the solid table */
}

/* ARGSUSED */
void
X_debug(lvl)
{
	XFlush(((struct x_vars *)dm_vars)->dpy);
	rt_log("flushed\n");
}

void
X_window(w)
register int w[];
{
#if 0
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
#endif
}

/*********XXX**********/
/*
 *  Called for 2d_line, and dot at center of screen.
 */
static void
draw( x1, y1, x2, y2 )
int	x1, y1;		/* from point */
int	x2, y2;		/* to point */
{
  int	sx1, sy1, sx2, sy2;

  sx1 = GED_TO_Xx( x1 );
  sy1 = GED_TO_Xy( y1 );
  sx2 = GED_TO_Xx( x2 );
  sy2 = GED_TO_Xy( y2 );

  if( sx1 == sx2 && sy1 == sy2 )
    XDrawPoint( ((struct x_vars *)dm_vars)->dpy,
		((struct x_vars *)dm_vars)->win,
		((struct x_vars *)dm_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct x_vars *)dm_vars)->dpy,
	       ((struct x_vars *)dm_vars)->win,
	       ((struct x_vars *)dm_vars)->gc, sx1, sy1, sx2, sy2 );
}

static void
label( x, y, str )
double	x, y;
char	*str;
{
  int	sx, sy;

  sx = GED_TO_Xx( x );
  sy = GED_TO_Xy( y );
  /* point is center of text? - seems like what MGED wants... */
  /* The following makes the menu look good, the rest bad */
  /*sy += ((struct x_vars *)dm_vars)->fontstruct->max_bounds.ascent + ((struct x_vars *)dm_vars)->fontstruct->max_bounds.descent/2);*/

  XDrawString( ((struct x_vars *)dm_vars)->dpy,
	       ((struct x_vars *)dm_vars)->win,
	       ((struct x_vars *)dm_vars)->gc, sx, sy, str, strlen(str) );
}

#define	FONT	"6x10"
#define FONT2	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"

static XWMHints xwmh = {
        StateHint,		        /* flags */
	0,				/* input */
	NormalState,			/* initial_state */
	0,				/* icon pixmap */
	0,				/* icon window */
	0, 0,				/* icon location */
	0,				/* icon mask */
	0				/* Window group */
};

static int
xsetup( name )
char	*name;
{
  static int count = 0;
  int first_event, first_error;
  char *cp;
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  int a_screen;
  Colormap  a_cmap;
  struct rt_vls str;
  Display *tmp_dpy;

  rt_vls_init(&str);

  /* Only need to do this once */
  if(tkwin == NULL){
    rt_vls_printf(&str, "loadtk %s\n", name);

    if(cmdline(&str, FALSE) == CMD_BAD){
      rt_vls_free(&str);
      return -1;
    }
  }

  /* Only need to do this once for this display manager */
  if(!count){
    if( X_load_startup() ){
      rt_vls_free(&str);
      return -1;
    }
  }

  rt_vls_init(&pathName);
  rt_vls_printf(&pathName, ".dm_x%d", count++);

  /* initialize the modifiable variables */
  ((struct x_vars *)dm_vars)->mvars.dummy_perspective = 1;
  ((struct x_vars *)dm_vars)->mvars.perspective_mode = 0;

#if 0
  if((tmp_dpy = XOpenDisplay(name)) == NULL){
    rt_vls_free(&str);
    return -1;
  }

  ((struct x_vars *)dm_vars)->width =
      DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
  ((struct x_vars *)dm_vars)->height =
      DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;

  /* Make window square */
  if(((struct x_vars *)dm_vars)->height < ((struct x_vars *)dm_vars)->width)
    ((struct x_vars *)dm_vars)->width = ((struct x_vars *)dm_vars)->height;
  else
    ((struct x_vars *)dm_vars)->height = ((struct x_vars *)dm_vars)->width;

  XCloseDisplay(tmp_dpy);

  /*
   * Create the X drawing window by calling create_x which
   * is defined in xinit.tk
   */
  rt_vls_strcpy(&str, "create_x ");
  rt_vls_printf(&str, "%s %s %d %d\n", name, rt_vls_addr(&pathName),
		((struct x_vars *)dm_vars)->width,
		((struct x_vars *)dm_vars)->height);

  if(cmdline(&str, FALSE) == CMD_BAD){
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);

  ((struct x_vars *)dm_vars)->xtkwin = Tk_NameToWindow(interp,
						       rt_vls_addr(&pathName),
						       tkwin);
  ((struct x_vars *)dm_vars)->dpy = Tk_Display(((struct x_vars *)dm_vars)->xtkwin);

#else
  /* Make xtkwin a toplevel window */
  ((struct x_vars *)dm_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						       rt_vls_addr(&pathName), name);

  rt_vls_strcpy(&str, "init_x ");
#if 0
  rt_vls_printf(&str, "%s %d\n", rt_vls_addr(&pathName), ((struct x_vars *)dm_vars)->width);
#else
  rt_vls_printf(&str, "%s\n", rt_vls_addr(&pathName));
  rt_log("pathname = %s\n", rt_vls_addr(&pathName));
#endif


  if(cmdline(&str, FALSE) == CMD_BAD){
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);

  ((struct x_vars *)dm_vars)->dpy = Tk_Display(((struct x_vars *)dm_vars)->xtkwin);
  ((struct x_vars *)dm_vars)->width =
    DisplayWidth(((struct x_vars *)dm_vars)->dpy,
		 DefaultScreen(((struct x_vars *)dm_vars)->dpy)) - 20;
  ((struct x_vars *)dm_vars)->height =
    DisplayHeight(((struct x_vars *)dm_vars)->dpy,
		  DefaultScreen(((struct x_vars *)dm_vars)->dpy)) - 20;

  /* Make window square */
  if(((struct x_vars *)dm_vars)->height < ((struct x_vars *)dm_vars)->width)
    ((struct x_vars *)dm_vars)->width = ((struct x_vars *)dm_vars)->height;
  else
    ((struct x_vars *)dm_vars)->height = ((struct x_vars *)dm_vars)->width;

  Tk_GeometryRequest(((struct x_vars *)dm_vars)->xtkwin,
		     ((struct x_vars *)dm_vars)->width, 
		     ((struct x_vars *)dm_vars)->height);
#endif

#if 0
  /*XXX*/
  XSynchronize(((struct x_vars *)dm_vars)->dpy, TRUE);
#endif

  Tk_MakeWindowExist(((struct x_vars *)dm_vars)->xtkwin);
  ((struct x_vars *)dm_vars)->win =
      Tk_WindowId(((struct x_vars *)dm_vars)->xtkwin);

#if TRY_MULTIBUFFERING
  if(!XmbufQueryExtension(((struct x_vars *)dm_vars)->dpy,
			  &first_event, &first_error)){
    ((struct x_vars *)dm_vars)->doublebuffer = 0;
    rt_log("xsetup: no multi-buffering extension available on %s\n", name);
  }else{
    int num;

    num = XmbufCreateBuffers(((struct x_vars *)dm_vars)->dpy,
			  ((struct x_vars *)dm_vars)->win, 2,
			  MultibufferUpdateActionBackground,
			  MultibufferUpdateHintFrequent,
			  ((struct x_vars *)dm_vars)->buffers);
    if(num != 2){
      if(num)
	XmbufDestroyBuffers(((struct x_vars *)dm_vars)->dpy,
			    ((struct x_vars *)dm_vars)->win);
      ((struct x_vars *)dm_vars)->doublebuffer = 0;
      rt_log("xsetup: failed to get 2 buffers\n");
    }else{
      ((struct x_vars *)dm_vars)->doublebuffer = 1;
      ((struct x_vars *)dm_vars)->curr_buf = ((struct x_vars *)dm_vars)->buffers[1];
    }
  }
#endif

  a_screen = Tk_ScreenNumber(((struct x_vars *)dm_vars)->xtkwin);
  a_visual = Tk_Visual(((struct x_vars *)dm_vars)->xtkwin);

  /* Get color map indices for the colors we use. */
  ((struct x_vars *)dm_vars)->black = BlackPixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
  ((struct x_vars *)dm_vars)->white = WhitePixel( ((struct x_vars *)dm_vars)->dpy, a_screen );

  a_cmap = Tk_Colormap(((struct x_vars *)dm_vars)->xtkwin);
  a_color.red = 255<<8;
  a_color.green=0;
  a_color.blue=0;
  a_color.flags = DoRed | DoGreen| DoBlue;
  if ( ! XAllocColor(((struct x_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
    rt_log( "dm-X: Can't Allocate red\n");
    return -1;
  }
  ((struct x_vars *)dm_vars)->red = a_color.pixel;
    if ( ((struct x_vars *)dm_vars)->red == ((struct x_vars *)dm_vars)->white )
      ((struct x_vars *)dm_vars)->red = ((struct x_vars *)dm_vars)->black;

    a_color.red = 200<<8;
    a_color.green=200<<8;
    a_color.blue=0<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate yellow\n");
	return -1;
    }
    ((struct x_vars *)dm_vars)->yellow = a_color.pixel;
    if ( ((struct x_vars *)dm_vars)->yellow == ((struct x_vars *)dm_vars)->white )
      ((struct x_vars *)dm_vars)->yellow = ((struct x_vars *)dm_vars)->black;
    
    a_color.red = 0;
    a_color.green=0;
    a_color.blue=255<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate blue\n");
	return -1;
    }
    ((struct x_vars *)dm_vars)->blue = a_color.pixel;
    if ( ((struct x_vars *)dm_vars)->blue == ((struct x_vars *)dm_vars)->white )
      ((struct x_vars *)dm_vars)->blue = ((struct x_vars *)dm_vars)->black;

    a_color.red = 128<<8;
    a_color.green=128<<8;
    a_color.blue= 128<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate gray\n");
	return -1;
    }
    ((struct x_vars *)dm_vars)->gray = a_color.pixel;
    if ( ((struct x_vars *)dm_vars)->gray == ((struct x_vars *)dm_vars)->white )
      ((struct x_vars *)dm_vars)->gray = ((struct x_vars *)dm_vars)->black;

    /* Select border, background, foreground colors,
     * and border width.
     */
    if( a_visual->class == GrayScale || a_visual->class == StaticGray ) {
	((struct x_vars *)dm_vars)->is_monochrome = 1;
	((struct x_vars *)dm_vars)->bd = BlackPixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
	((struct x_vars *)dm_vars)->bg = WhitePixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
	((struct x_vars *)dm_vars)->fg = BlackPixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
    } else {
	/* Hey, it's a color server.  Ought to use 'em! */
	((struct x_vars *)dm_vars)->is_monochrome = 0;
	((struct x_vars *)dm_vars)->bd = WhitePixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
	((struct x_vars *)dm_vars)->bg = BlackPixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
	((struct x_vars *)dm_vars)->fg = WhitePixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
    }

    if( !((struct x_vars *)dm_vars)->is_monochrome &&
	((struct x_vars *)dm_vars)->fg != ((struct x_vars *)dm_vars)->red &&
	((struct x_vars *)dm_vars)->red != ((struct x_vars *)dm_vars)->black )
      ((struct x_vars *)dm_vars)->fg = ((struct x_vars *)dm_vars)->red;

    gcv.foreground = ((struct x_vars *)dm_vars)->fg;
    gcv.background = ((struct x_vars *)dm_vars)->bg;

#ifndef CRAY2
    cp = FONT;
    if ( (((struct x_vars *)dm_vars)->fontstruct =
	 XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, cp)) == NULL ) {
      /* Try hardcoded backup font */
      if ( (((struct x_vars *)dm_vars)->fontstruct =
	    XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT2)) == NULL) {
	rt_log( "dm-X: Can't open font '%s' or '%s'\n", cp, FONT2 );
	return -1;
      }
    }
    gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
    ((struct x_vars *)dm_vars)->gc = XCreateGC(((struct x_vars *)dm_vars)->dpy,
					       ((struct x_vars *)dm_vars)->win,
					       (GCFont|GCForeground|GCBackground),
						&gcv);
#else
    ((struct x_vars *)dm_vars)->gc = XCreateGC(((struct x_vars *)dm_vars)->dpy,
					       ((struct x_vars *)dm_vars)->win,
					       (GCForeground|GCBackground),
					       &gcv);
#endif
    
    /* Register the file descriptor with the Tk event handler */
    Tk_CreateGenericHandler(Xdoevent, (ClientData)curr_dm_list);

#if DO_XSELECTINPUT
    /* start with constant tracking OFF */
    XSelectInput(((struct x_vars *)dm_vars)->dpy,
		 ((struct x_vars *)dm_vars)->win,
		 ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);
#endif

    Tk_SetWindowBackground(((struct x_vars *)dm_vars)->xtkwin, ((struct x_vars *)dm_vars)->bg);
    Tk_MapWindow(((struct x_vars *)dm_vars)->xtkwin);

    return 0;
}

static void
establish_perspective()
{
  rt_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		((struct x_vars *)dm_vars)->mvars.perspective_mode ?
		perspective_table[perspective_angle] : -1 );
  dmaflag = 1;
}

/*
   This routine will toggle the perspective_angle if the
   dummy_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (dummy_perspective - 1).
*/
static void
set_perspective()
{
  /* set perspective matrix */
  if(((struct x_vars *)dm_vars)->mvars.dummy_perspective > 0)
    perspective_angle = ((struct x_vars *)dm_vars)->mvars.dummy_perspective <= 4 ?
    ((struct x_vars *)dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  if(((struct x_vars *)dm_vars)->mvars.perspective_mode)
    rt_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct x_vars *)dm_vars)->mvars.dummy_perspective = 1;

  dmaflag = 1;
}

int
X_dm(argc, argv)
int argc;
char *argv[];
{
  struct rt_vls   vls;

  if( !strcmp( argv[0], "set" )){
    rt_vls_init(&vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      rt_structprint("dm_X internal variables", X_vparse,
		     (CONST char *)&((struct x_vars *)dm_vars)->mvars );
      rt_log("%s", rt_vls_addr(&vls) );
    } else if( argc == 2 ) {
      rt_vls_name_print( &vls, X_vparse, argv[1],
			 (CONST char *)&((struct x_vars *)dm_vars)->mvars);
      rt_log( "%s\n", rt_vls_addr(&vls) );
    } else {
      rt_vls_printf( &vls, "%s=\"", argv[1] );
      rt_vls_from_argv( &vls, argc-2, argv+2 );
      rt_vls_putc( &vls, '\"' );
      rt_structparse( &vls, X_vparse, (char *)&((struct x_vars *)dm_vars)->mvars);
    }

    rt_vls_free(&vls);
    return CMD_OK;
  }

  if( !strcmp( argv[0], "mouse")){
    int up;
    int xpos, ypos;

    if( argc < 4){
      rt_log("dm: need more parameters\n");
      rt_log("mouse 1|0 xpos ypos\n");
      return CMD_BAD;
    }

    up = atoi(argv[1]);
    xpos = atoi(argv[2]);
    ypos = atoi(argv[3]);

    rt_vls_printf(&dm_values.dv_string, "M %d %d %d\n",
		  up, Xx_TO_GED(xpos), Xy_TO_GED(ypos));
    return CMD_OK;
  }

  rt_log("dm: bad command - %s\n", argv[0]);
  return CMD_BAD;
}

static void
x_var_init()
{
  dm_vars = (char *)rt_malloc(sizeof(struct x_vars), "x_var_init: x_vars");
  bzero((void *)dm_vars, sizeof(struct x_vars));
}

static int
X_load_startup()
{
  FILE    *fp;
  struct rt_vls str;
  char *path;
  char *filename;
  int     found;

#define DM_X_RCFILE "xinit.tk"

  found = 0;
  rt_vls_init( &str );

  if((filename = getenv("DM_X_RCFILE")) == (char *)NULL )
        filename = DM_X_RCFILE;

  if((path = getenv("MGED_LIBRARY")) != (char *)NULL ){
    rt_vls_strcpy( &str, path );
    rt_vls_strcat( &str, "/" );
    rt_vls_strcat( &str, filename );

    if ((fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    if( (path = getenv("HOME")) != (char *)NULL )  {
      rt_vls_strcpy( &str, path );
      rt_vls_strcat( &str, "/" );
      rt_vls_strcat( &str, filename );

      if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
	found = 1;
    }
  }

  if( !found ) {
    if( (fp = fopen( filename, "r" )) != NULL )  {
      rt_vls_strcpy( &str, filename );
      found = 1;
    }
  }

/*XXX Temporary, so things will work without knowledge of the new environment
      variables */
  if( !found ) {
    rt_vls_strcpy( &str, "/m/cad/mged/");
    rt_vls_strcat( &str, filename);

    if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    rt_vls_free(&str);
    return -1;
  }

  fclose( fp );

  if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
    rt_log("Error reading %s: %s\n", filename, interp->result);
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);
  return 0;
}


static struct dm_list *
get_dm_list(window)
Window window;
{
  register struct dm_list *p;
  struct rt_vls str;

  for( RT_LIST_FOR(p, dm_list, &head_dm_list.l) ){
    if(window == ((struct x_vars *)p->_dm_vars)->win)
      return p;
  }

  return DM_LIST_NULL;
}
#else
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

#define DO_XSELECTINPUT 1
#define TRY_PIPES 1

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


#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

#if TRY_PIPES
extern int ged_pipe[];
#endif

static void	label();
static void	draw();
static int	xsetup();
static void     establish_perspective();
static void     set_perspective();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
int	X_open();
void	X_close();
MGED_EXTERN(void	X_input, (fd_set *input, int noblock) );
void	X_prolog(), X_epilog();
void	X_normal(), X_newrot();
void	X_update();
void	X_puts(), X_2d_line(), X_light();
int	X_object();
unsigned X_cvtvecs(), X_load();
void	X_statechange(), X_viewchange(), X_colorchange();
void	X_window(), X_debug(), X_selectargs();
static int     X_dm();
static int   X_load_startup();

struct dm dm_X = {
	X_open, X_close,
	X_input,
	X_prolog, X_epilog,
	X_normal, X_newrot,
	X_update,
	X_puts, X_2d_line,
	X_light,
	X_object,	X_cvtvecs, X_load,
	X_statechange,
	X_viewchange,
	X_colorchange,
	X_window, X_debug,
	0,				/* no displaylist */
	0,				/* multi-window */
	PLOTBOUND,
	"X", "X Window System (X11)",
	0,
	X_dm
};

extern struct device_values dm_values;	/* values read from devices */
extern Tcl_Interp *interp;
extern Tk_Window tkwin;

/*
 * These variables are visible and modifiable via a "dm set" command.
 */
static int      dummy_perspective = 1;
static int      perspective_mode = 0;	/* Perspective flag */
/* End modifiable variables */

static int perspective_angle = 3;	/* Angle of perspective */
static int perspective_table[] = { 
	30, 45, 60, 90 };

static int height, width;
static Tcl_Interp *xinterp;
static Tk_Window xtkwin;

#if !DO_XSELECTINPUT
static int XdoMotion = 0;
#endif

static Display	*dpy;			/* X display pointer */
static Window	win;			/* X window */
static unsigned long black,gray,white,yellow,red,blue;
static unsigned long bd, bg, fg;   /*color of border, background, foreground */

static GC	gc;			/* X Graphics Context */
static int	is_monochrome = 0;
static XFontStruct *fontstruct;		/* X Font */

static int	no_faceplate = 0;

struct structparse X_vparse[] = {
  {"%d",  1, "perspective",       (int)&perspective_mode, establish_perspective },
  {"%d",  1, "set_perspective",(int)&dummy_perspective,  set_perspective },
  {"",    0,  (char *)0,          0,                      FUNC_NULL }
};

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2048,
 *  X is 0..width,0..height
 */
#define	GED_TO_Xx(x)	(((x)/4096.0+0.5)*width)
#define	GED_TO_Xy(x)	((0.5-(x)/4096.0)*height)
#define Xx_TO_GED(x)    ((int)(((x)/(double)width - 0.5) * 4095))
#define Xy_TO_GED(x)    ((int)((0.5 - (x)/(double)height) * 4095))

/*
 *			X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
X_open()
{
        char	line[82];
        char	hostname[80];
	char	display[82];
	char	*envp;

	/* get or create the default display */
	if( (envp = getenv("DISPLAY")) == NULL ) {
		/* Env not set, use local host */
		gethostname( hostname, 80 );
		hostname[79] = '\0';
		(void)sprintf( display, "%s:0", hostname );
		envp = display;
	}

	rt_log("X Display [%s]? ", envp );
	(void)fgets( line, sizeof(line), stdin );
	line[strlen(line)-1] = '\0';		/* remove newline */
	if( feof(stdin) )  quit();
	if( line[0] != '\0' ) {
		if( xsetup(line) ) {
			return(1);		/* BAD */
		}
	} else {
		if( xsetup(envp) ) {
			return(1);	/* BAD */
		}
	}

	/* Ignore the old scrollbars and menus */
	mged_variables.show_menu = 0;

	return(0);			/* OK */
}

/*
 *  			X _ C L O S E
 *  
 *  Gracefully release the display.
 */
void
X_close()
{
    XFreeGC(dpy, gc);
    Tk_DestroyWindow(xtkwin);

#if 0
    Tcl_DeleteInterp(xinterp);
#endif

    /* to prevent events being processed after window destroyed */
    win = -1;
}

/*
 *			X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
X_prolog()
{
    if( !dmaflag )
	return;

    XClearWindow(dpy, win);

    /* Put the center point up */
    draw( 0, 0, 0, 0 );
}

/*
 *			X _ E P I L O G
 */
void
X_epilog()
{
    /* Prevent lag between events and updates */
    XSync(dpy, 0);
    return;
}

/*
 *  			X _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
void
X_newrot(mat)
mat_t mat;
{
	return;
}

/*
 *  			X _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
int
X_object( sp, mat, ratio, white_flag )
register struct solid *sp;
mat_t mat;
double ratio;
int white_flag;
{
    static vect_t   pnt;
    register struct rt_vlist	*vp;
    int useful = 0;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    XGCValues gcv;
    int	nseg;			        /* number of segments */
    int	x, y;
    int	lastx = 0;
    int	lasty = 0;

    XChangeGC(dpy, gc, GCForeground, &gcv);

    if( sp->s_soldash )
	XSetLineAttributes( dpy, gc, 1, LineOnOffDash, CapButt, JoinMiter );
    else
	XSetLineAttributes( dpy, gc, 1, LineSolid, CapButt, JoinMiter );

    nseg = 0;
    segp = segbuf;
    for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
	register int	i;
	register int	nused = vp->nused;
	register int	*cmd = vp->cmd;
	register point_t *pt = vp->pt;

	/* Viewing region is from -1.0 to +1.0 */
	/* 2^31 ~= 2e9 -- dynamic range of a long int */
	/* 2^(31-11) = 2^20 ~= 1e6 */
	/* Integerize and let the X server do the clipping */
	for( i = 0; i < nused; i++,cmd++,pt++ )  {
	    switch( *cmd )  {
	    case RT_VLIST_POLY_START:
	    case RT_VLIST_POLY_VERTNORM:
		continue;
	    case RT_VLIST_POLY_MOVE:
	    case RT_VLIST_LINE_MOVE:
		/* Move, not draw */
		MAT4X3PNT( pnt, mat, *pt );
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		   pnt[1] < -1e6 || pnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */
		pnt[0] *= 2047;
		pnt[1] *= 2047;
		x = GED_TO_Xx(pnt[0]);
		y = GED_TO_Xy(pnt[1]);
		lastx = x;
		lasty = y;
		continue;
	    case RT_VLIST_POLY_DRAW:
	    case RT_VLIST_POLY_END:
	    case RT_VLIST_LINE_DRAW:
		/* draw */
		MAT4X3PNT( pnt, mat, *pt );
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		   pnt[1] < -1e6 || pnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */
		/* Integerize and let the X server do the clipping */
		/*XXX Color */
		gcv.foreground = fg;
		if( white_flag && !is_monochrome )  {
		    gcv.foreground = white;
		}
		XChangeGC( dpy, gc, GCForeground, &gcv );
		
		pnt[0] *= 2047;
		pnt[1] *= 2047;
		x = GED_TO_Xx(pnt[0]);
		y = GED_TO_Xy(pnt[1]);

		segp->x1 = lastx;
		segp->y1 = lasty;
		segp->x2 = x;
		segp->y2 = y;
		nseg++;
		segp++;
		lastx = x;
		lasty = y;
		useful = 1;
		if( nseg == 1024 ) {
		    XDrawSegments( dpy, win, gc, segbuf, nseg );
		    /* Thicken the drawing, if monochrome */
		    if( white_flag && is_monochrome ) {
			int	i;
			/* XXX - width and height don't work on Sun! */
			/* Thus the following gross hack */
			segp = segbuf;
			for( i = 0; i < nseg; i++ ) {
			    segp->x1++;
			    segp->y1++;
			    segp->x2++;
			    segp->y2++;
			    segp++;
			}
			XDrawSegments( dpy, win, gc, segbuf, nseg );
		    }
		    nseg = 0;
		    segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
	XDrawSegments( dpy, win, gc, segbuf, nseg );
	if( white_flag && is_monochrome ) {
	    int	i;
	    /* XXX - width and height don't work on Sun! */
	    /* Thus the following gross hack */
	    segp = segbuf;
	    for( i = 0; i < nseg; i++ ) {
		segp->x1++;
		segp->y1++;
		segp->x2++;
		segp->y2++;
		segp++;
	    }
	    XDrawSegments( dpy, win, gc, segbuf, nseg );
	}
    }

    return(useful);
}

/*
 *			X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
void
X_normal()
{
	return;
}

/*
 *			X _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
void
X_update()
{
    XFlush(dpy);
}

/*
 *			X _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
void
X_puts( str, x, y, size, color )
register char *str;
{
	XGCValues gcv;
	unsigned long fg;

	switch( color )  {
	case DM_BLACK:
		fg = black;
		break;
	case DM_RED:
		fg = red;
		break;
	case DM_BLUE:
		fg = blue;
		break;
	default:
	case DM_YELLOW:
		fg = yellow;
		break;
	case DM_WHITE:
		fg = gray;
		break;
	}
	gcv.foreground = fg;
	XChangeGC( dpy, gc, GCForeground, &gcv );
	label( (double)x, (double)y, str );
}

/*
 *			X _ 2 D _ G O T O
 *
 */
void
X_2d_line( x1, y1, x2, y2, dashed )
int x1, y1;
int x2, y2;
int dashed;
{
    XGCValues gcv;

    gcv.foreground = yellow;
    XChangeGC( dpy, gc, GCForeground, &gcv );
    if( dashed ) {
	XSetLineAttributes(dpy, gc, 1, LineOnOffDash, CapButt, JoinMiter );
    } else {
	XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter );
    }
    draw( x1, y1, x2, y2 );
}

int
Xdoevent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
    KeySym key;
    char keybuf[4];
    int cnt;
    XComposeStatus compose_stat;
    XWindowAttributes xwa;

    if (eventPtr->xany.window != win)
	return TCL_OK;

#if TRY_PIPES
    if(mged_variables.focus && eventPtr->type == KeyPress){
      char buffer[1];

      XLookupString(&(eventPtr->xkey), buffer, 1,
		    (KeySym *)NULL, (XComposeStatus *)NULL);

      write(ged_pipe[1], buffer, 1);
      return TCL_RETURN;
    }
#endif

    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0){
      XGetWindowAttributes( dpy, win, &xwa );
      height = xwa.height;
      width = xwa.width;
      rt_vls_printf( &dm_values.dv_string, "refresh\n" );
    }else if(eventPtr->type == ConfigureNotify){
      XGetWindowAttributes( dpy, win, &xwa );
      height = xwa.height;
      width = xwa.width;
      rt_vls_printf( &dm_values.dv_string, "refresh\n" );
    } else if( eventPtr->type == MotionNotify ) {
	int	x, y;

#if !DO_XSELECTINPUT
	if ( !XdoMotion )
	    return TCL_OK;
#endif

	x = (eventPtr->xmotion.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xmotion.y/(double)height) * 4095;
    	/* Constant tracking (e.g. illuminate mode) bound to M mouse */
	rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y );
    }
#if 0
    else if( eventPtr->type == ButtonPress ) {
	/* There may also be ButtonRelease events */
	int	x, y;
	/* In MGED this is a "penpress" */
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    /* Left mouse */
	    rt_vls_printf( &dm_values.dv_string, "L 1 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, up to down transition */
	    rt_vls_printf( &dm_values.dv_string, "M 1 %d %d\n", x, y);
	    break;
	case Button3:
	    /* Right mouse */
	    rt_vls_printf( &dm_values.dv_string, "R 1 %d %d\n", x, y);
	    break;
	}
    } else if( eventPtr->type == ButtonRelease ) {
	int	x, y;
	x = (eventPtr->xbutton.x/(double)width - 0.5) * 4095;
	y = (0.5 - eventPtr->xbutton.y/(double)height) * 4095;
	switch( eventPtr->xbutton.button ) {
	case Button1:
	    rt_vls_printf( &dm_values.dv_string, "L 0 %d %d\n", x, y);
	    break;
	case Button2:
	    /* Middle mouse, down to up transition */
	    rt_vls_printf( &dm_values.dv_string, "M 0 %d %d\n", x, y);
	    break;
	case Button3:
	    rt_vls_printf( &dm_values.dv_string, "R 0 %d %d\n", x, y);
	    break;
	}
    } else if( eventPtr->type == KeyPress ) {
	register int i;
	/* Turn these into MGED "buttonpress" or knob functions */
	
	cnt = XLookupString(&eventPtr->xkey, keybuf, sizeof(keybuf),
			    &key, &compose_stat);

	for(i=0 ; i < cnt ; i++){

	    switch( *keybuf ) {
	    case '?':
		rt_log( "\nKey Help Menu:\n\
0	Zero 'knobs'\n\
x	Increase xrot\n\
y	Increase yrot\n\
z	Increase zrot\n\
X	Increase Xslew\n\
Y	Increase Yslew\n\
Z	Increase Zslew\n\
f	Front view\n\
t	Top view\n\
b	Bottom view\n\
l	Left view\n\
r	Right view\n\
R	Rear view\n\
3	35,25 view\n\
4	45,45 view\n\
F	Toggle faceplate\n\
" );
		break;
	    case 'F':
		/* Toggle faceplate on/off */
		no_faceplate = !no_faceplate;
		rt_vls_strcat( &dm_values.dv_string,
			      no_faceplate ?
			      "set faceplate 0\n" :
			      "set faceplate 1\n" );
		break;
	    case '0':
		rt_vls_printf( &dm_values.dv_string, "knob zero\n" );
		break;
	    case 'x':
		/* 6 degrees per unit */
		rt_vls_printf( &dm_values.dv_string, "knob +x 0.1\n" );
		break;
	    case 'y':
		rt_vls_printf( &dm_values.dv_string, "knob +y 0.1\n" );
		break;
	    case 'z':
		rt_vls_printf( &dm_values.dv_string, "knob +z 0.1\n" );
		break;
	    case 'X':
		/* viewsize per unit */
		rt_vls_printf( &dm_values.dv_string, "knob +X 0.1\n" );
		break;
	    case 'Y':
		rt_vls_printf( &dm_values.dv_string, "knob +Y 0.1\n" );
		break;
	    case 'Z':
		rt_vls_printf( &dm_values.dv_string, "knob +Z 0.1\n" );
		break;
	    case 'f':
		rt_vls_strcat( &dm_values.dv_string, "press front\n");
		break;
	    case 't':
		rt_vls_strcat( &dm_values.dv_string, "press top\n");
		break;
	    case 'b':
		rt_vls_strcat( &dm_values.dv_string, "press bottom\n");
		break;
	    case 'l':
		rt_vls_strcat( &dm_values.dv_string, "press left\n");
		break;
	    case 'r':
		rt_vls_strcat( &dm_values.dv_string, "press right\n");
		break;
	    case 'R':
		rt_vls_strcat( &dm_values.dv_string, "press rear\n");
		break;
	    case '3':
		rt_vls_strcat( &dm_values.dv_string, "press 35,25\n");
		break;
	    case '4':
		rt_vls_strcat( &dm_values.dv_string, "press 45,45\n");
		break;
	    default:
		rt_log("dm-X: The key '%c' is not defined\n", key);
		break;
	    }
	}
    }
#endif
    else {
	XGetWindowAttributes( dpy, win, &xwa );
	height = xwa.height;
	width = xwa.width;
    }

    return TCL_OK;
}
	    
/*
 *			X _ I N P U T
 *
 * Execution must suspend in this routine until a significant event
 * has occured on either the command stream,
 * unless "noblock" is set.
 *
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 *
 * DEPRECATED 
 *
 */
/* ARGSUSED */
void
X_input( input, noblock )
fd_set		*input;
int		noblock;
{
    return;
}

/* 
 *			X _ L I G H T
 */
/* ARGSUSED */
void
X_light( cmd, func )
int cmd;
int func;			/* BE_ or BV_ function */
{
	return;
}

/* ARGSUSED */
unsigned
X_cvtvecs( sp )
struct solid *sp;
{
	return( 0 );
}

/*
 * Loads displaylist
 */
unsigned
X_load( addr, count )
unsigned addr, count;
{
	rt_log("X_load(x%x, %d.)\n", addr, count );
	return( 0 );
}

void
X_statechange( a, b )
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
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask);
	  break;
	case ST_S_PICK:
	case ST_O_PICK:
	case ST_O_PATH:
	  /* constant tracking ON */
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		       KeyPressMask|StructureNotifyMask|PointerMotionMask);
	  break;
	case ST_O_EDIT:
	case ST_S_EDIT:
	  /* constant tracking OFF */
	  XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
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
	    rt_log("X_statechange: unknown state %s\n", state_str[b]);
	    break;
	}

	/*X_viewchange( DM_CHGV_REDO, SOLID_NULL );*/
}

void
X_viewchange()
{
}

void
X_colorchange()
{
	color_soltab();		/* apply colors to the solid table */
}

/* ARGSUSED */
void
X_debug(lvl)
{
	XFlush(dpy);
	rt_log("flushed\n");
}

void
X_window(w)
register int w[];
{
#if 0
	/* Compute the clipping bounds */
	clipmin[0] = w[1] / 2048.;
	clipmin[1] = w[3] / 2048.;
	clipmin[2] = w[5] / 2048.;
	clipmax[0] = w[0] / 2047.;
	clipmax[1] = w[2] / 2047.;
	clipmax[2] = w[4] / 2047.;
#endif
}

/*********XXX**********/
/*
 *  Called for 2d_line, and dot at center of screen.
 */
static void
draw( x1, y1, x2, y2 )
int	x1, y1;		/* from point */
int	x2, y2;		/* to point */
{
	int	sx1, sy1, sx2, sy2;

	sx1 = GED_TO_Xx( x1 );
	sy1 = GED_TO_Xy( y1 );
	sx2 = GED_TO_Xx( x2 );
	sy2 = GED_TO_Xy( y2 );

	if( sx1 == sx2 && sy1 == sy2 )
		XDrawPoint( dpy, win, gc, sx1, sy1 );
	else
		XDrawLine( dpy, win, gc, sx1, sy1, sx2, sy2 );
}

static void
label( x, y, str )
double	x, y;
char	*str;
{
	int	sx, sy;

	sx = GED_TO_Xx( x );
	sy = GED_TO_Xy( y );
	/* point is center of text? - seems like what MGED wants... */
	/* The following makes the menu look good, the rest bad */
	/*sy += (fontstruct->max_bounds.ascent + fontstruct->max_bounds.descent)/2;*/

	XDrawString( dpy, win, gc, sx, sy, str, strlen(str) );
}

#define	FONT	"6x10"
#define FONT2	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"

static XWMHints xwmh = {
        StateHint,		        /* flags */
	0,				/* input */
	NormalState,			/* initial_state */
	0,				/* icon pixmap */
	0,				/* icon window */
	0, 0,				/* icon location */
	0,				/* icon mask */
	0				/* Window group */
};

static int
xsetup( name )
char	*name;
{
    char *cp;
    XGCValues gcv;
    XColor a_color;
    Visual *a_visual;
    int a_screen;
    Colormap  a_cmap;
    struct rt_vls str;

    width = height = 512;

#if 0
    xinterp = Tcl_CreateInterp(); /* Dummy interpreter */
    xtkwin = Tk_CreateMainWindow(xinterp, name, "MGED", "MGED");
#else
    rt_vls_init(&str);
    rt_vls_printf(&str, "loadtk %s\n", name);

    if(tkwin == NULL){
      if(cmdline(&str, FALSE) == CMD_BAD){
	rt_vls_free(&str);
	return -1;
      }
    }

    rt_vls_free(&str);

    /* Use interp with all its registered commands. */
#if 1
    /* Make xtkwin a toplevel window */
    xtkwin = Tk_CreateWindow(interp, tkwin, "mged", name);
#else
    /* Make xtkwin an internal window */
    xtkwin = Tk_CreateWindow(interp, tkwin, "mged", NULL);
#endif

#if 1
    if( X_load_startup() ){
      rt_log( "xsetup: Error loading startup file\n");
      return -1;
    }
#else
/*XXX* Temporary */
    Tcl_EvalFile(interp, "/m/cad/mged/sample_bindings");
#endif
#endif

    /* Open the display - XXX see what NULL does now */
    if( xtkwin == NULL ) {
	rt_log( "dm-X: Can't open X display\n" );
	return -1;
    }

    dpy = Tk_Display(xtkwin);
    
    Tk_GeometryRequest(xtkwin, width+10, height+10);
    Tk_MakeWindowExist(xtkwin);

    win = Tk_WindowId(xtkwin);

    /* Get colormap indices for the colors we use. */

    a_screen = Tk_ScreenNumber(xtkwin);
    a_visual = Tk_Visual(xtkwin);

    /* Get color map inddices for the colors we use. */
    black = BlackPixel( dpy, a_screen );
    white = WhitePixel( dpy, a_screen );

    a_cmap = Tk_Colormap(xtkwin);
    a_color.red = 255<<8;
    a_color.green=0;
    a_color.blue=0;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate red\n");
	return -1;
    }
    red = a_color.pixel;
    if ( red == white ) red = black;

    a_color.red = 200<<8;
    a_color.green=200<<8;
    a_color.blue=0<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate yellow\n");
	return -1;
    }
    yellow = a_color.pixel;
    if (yellow == white) yellow = black;
    
    a_color.red = 0;
    a_color.green=0;
    a_color.blue=255<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate blue\n");
	return -1;
    }
    blue = a_color.pixel;
    if (blue == white) blue = black;

    a_color.red = 128<<8;
    a_color.green=128<<8;
    a_color.blue= 128<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(dpy, a_cmap, &a_color)) {
	rt_log( "dm-X: Can't Allocate gray\n");
	return -1;
    }
    gray = a_color.pixel;
    if (gray == white) gray = black;

    /* Select border, background, foreground colors,
     * and border width.
     */
    if( a_visual->class == GrayScale || a_visual->class == StaticGray )  {
	is_monochrome = 1;
	bd = BlackPixel( dpy, a_screen );
	bg = WhitePixel( dpy, a_screen );
	fg = BlackPixel( dpy, a_screen );
    } else {
	/* Hey, it's a color server.  Ought to use 'em! */
	is_monochrome = 0;
	bd = WhitePixel( dpy, a_screen );
	bg = BlackPixel( dpy, a_screen );
	fg = WhitePixel( dpy, a_screen );
    }

    if( !is_monochrome && fg != red && red != black )  fg = red;

    gcv.foreground = fg;
    gcv.background = bg;

#ifndef CRAY2
    cp = FONT;
    if ((fontstruct = XLoadQueryFont(dpy, cp)) == NULL ) {
	/* Try hardcoded backup font */
	if ((fontstruct = XLoadQueryFont(dpy, FONT2)) == NULL) {
	    rt_log( "dm-X: Can't open font '%s' or '%s'\n", cp, FONT2 );
	    return -1;
	}
    }
    gcv.font = fontstruct->fid;
    gc = XCreateGC(dpy, win, (GCFont|GCForeground|GCBackground), &gcv);
#else
    gc = XCreateGC(dpy, win, (GCForeground|GCBackground), &gcv);
#endif
    
    /* Register the file descriptor with the Tk event handler */
#if 0
    Tk_CreateEventHandler(xtkwin, ExposureMask|ButtonPressMask|KeyPressMask|
			  PointerMotionMask
/*			  |StructureNotifyMask|FocusChangeMask */,
			  (void (*)())Xdoevent, (ClientData)NULL);

#else
    Tk_CreateGenericHandler(Xdoevent, (ClientData)NULL);

#if DO_XSELECTINPUT
    /* start with constant tracking OFF */
    XSelectInput(dpy, win, ExposureMask|ButtonPressMask|
		 KeyPressMask|StructureNotifyMask);
#endif

#endif

    Tk_SetWindowBackground(xtkwin, bg);
    Tk_MapWindow(xtkwin);
    return 0;
}

static void
establish_perspective()
{
  rt_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		perspective_mode ?
		perspective_table[perspective_angle] :
		-1 );
  dmaflag = 1;
}

/*
   This routine will toggle the perspective_angle if the
   dummy_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (dummy_perspective - 1).
*/
static void
set_perspective()
{
  /* set perspective matrix */
  if(dummy_perspective > 0)
    perspective_angle = dummy_perspective <= 4 ? dummy_perspective - 1: 3;
  else if (--perspective_angle < 0) /* toggle perspective matrix */
    perspective_angle = 3;

  if(perspective_mode)
    rt_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  dummy_perspective = 1;

  dmaflag = 1;
}

static int
X_dm(argc, argv)
int argc;
char *argv[];
{
  struct rt_vls   vls;

  if( !strcmp( argv[0], "set" )){
    rt_vls_init(&vls);

    if( argc < 2 )  {
      /* Bare set command, print out current settings */
      rt_structprint("dm_X internal variables", X_vparse, (char *)0 );
      rt_log("%s", rt_vls_addr(&vls) );
    } else if( argc == 2 ) {
      rt_vls_name_print( &vls, X_vparse, argv[1], (char *)0 );
      rt_log( "%s\n", rt_vls_addr(&vls) );
    } else {
      rt_vls_printf( &vls, "%s=\"", argv[1] );
      rt_vls_from_argv( &vls, argc-2, argv+2 );
      rt_vls_putc( &vls, '\"' );
      rt_structparse( &vls, X_vparse, (char *)0 );
    }

    rt_vls_free(&vls);
    return CMD_OK;
  }

  if( !strcmp( argv[0], "mouse")){
    int up;
    int xpos, ypos;

    if( argc < 4){
      rt_log("dm: need more parameters\n");
      rt_log("mouse 1|0 xpos ypos\n");
      return CMD_BAD;
    }

    up = atoi(argv[1]);
    xpos = atoi(argv[2]);
    ypos = atoi(argv[3]);

    rt_vls_printf(&dm_values.dv_string, "M %d %d %d\n",
		  up, Xx_TO_GED(xpos), Xy_TO_GED(ypos));
    return CMD_OK;
  }

  rt_log("dm: bad command - %s\n", argv[0]);
  return CMD_BAD;
}

static int
X_load_startup()
{
  FILE    *fp;
  struct rt_vls str;
  char *path;
  char *filename;
  int     found;

#define DM_X_RCFILE "sample_bindings"

  found = 0;
  rt_vls_init( &str );

  if((filename = getenv("DM_X_RCFILE")) == (char *)NULL )
        filename = DM_X_RCFILE;

  if((path = getenv("MGED_LIBRARY")) != (char *)NULL ){
    rt_vls_strcpy( &str, path );
    rt_vls_strcat( &str, "/" );
    rt_vls_strcat( &str, filename );

    if ((fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    if( (path = getenv("HOME")) != (char *)NULL )  {
      rt_vls_strcpy( &str, path );
      rt_vls_strcat( &str, "/" );
      rt_vls_strcat( &str, filename );

      if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
	found = 1;
    }
  }

  if( !found ) {
    if( (fp = fopen( filename, "r" )) != NULL )  {
      rt_vls_strcpy( &str, filename );
      found = 1;
    }
  }

/*XXX Temporary, so things will work without knowledge of the new environment
      variables */
  if( !found ) {
    rt_vls_strcpy( &str, "/m/cad/mged/");
    rt_vls_strcat( &str, filename);

    if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    rt_vls_free(&str);
    return -1;
  }

  fclose( fp );

  if (Tcl_EvalFile( interp, rt_vls_addr(&str) ) == TCL_ERROR) {
    rt_log("Error reading %s: %s\n", filename, interp->result);
    rt_vls_free(&str);
    return -1;
  }

  rt_vls_free(&str);
  return 0;
}
#endif
