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
#include "./xinit.h"

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"

extern int dm_pipe[];

static void	label();
static void	draw();
static int	xsetup();
static void     x_var_init();
static void     establish_perspective();
static void     set_perspective();
static void     X_configure_window_shape();
static void     establish_vtb();

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
  int virtual_trackball;
  int debug;
};

struct x_vars {
  struct rt_list l;
  struct dm_list *dm_list;
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  Pixmap pix;
#endif
  int width;
  int height;
  int omx, omy;
  int perspective_angle;
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
  {"%d",  1, "set_perspective",   X_MV_O(dummy_perspective),set_perspective },
  {"%d",  1, "virtual_trackball", X_MV_O(virtual_trackball),establish_vtb },
  {"%d",  1, "debug",             X_MV_O(debug),            FUNC_NULL },
  {"",    0, (char *)0,           0,                        FUNC_NULL }
};

static int perspective_table[] = { 
	30, 45, 60, 90 };

static struct x_vars head_x_vars;
static int XdoMotion = 0;

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
  rt_vls_init(&pathName);

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

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  if(((struct x_vars *)dm_vars)->pix != 0)
     Tk_FreePixmap(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix);
#endif

  if(((struct x_vars *)dm_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct x_vars *)dm_vars)->xtkwin);

  RT_LIST_DEQUEUE(&((struct x_vars *)dm_vars)->l);
  rt_free(dm_vars, "X_close: dm_vars");
  rt_vls_free(&pathName);
#if 0
  Tk_DeleteGenericHandler(Xdoevent, (ClientData)curr_dm_list);
#else
  if(RT_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_DeleteGenericHandler(Xdoevent, (ClientData)NULL);
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  XGCValues       gcv;

  gcv.foreground = ((struct x_vars *)dm_vars)->bg;
  XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix,
		 ((struct x_vars *)dm_vars)->gc, 0, 0, ((struct x_vars *)dm_vars)->width + 1,
		 ((struct x_vars *)dm_vars)->height + 1);
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

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
    XCopyArea(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix,
	      ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc,
	      0, 0, ((struct x_vars *)dm_vars)->width, ((struct x_vars *)dm_vars)->height,
	      0, 0);
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

    gcv.foreground = ((struct x_vars *)dm_vars)->fg;
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#else
		    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#endif
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
			XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#else
			XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#endif
		    }
		    nseg = 0;
		    segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#else
	XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#endif
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#else
	    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
#endif
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

#if 1
  save_dm_list = curr_dm_list;
#endif

#if 1
  curr_dm_list = get_dm_list(eventPtr->xany.window);

  if(curr_dm_list == DM_LIST_NULL)
    goto end;

  if(((struct x_vars *)dm_vars)->mvars.debug)
    rt_log("curr_dm_list: %d\n", (int)curr_dm_list);
#else
  curr_dm_list = (struct dm_list *)clientData;
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
#if 0
    dmaflag = 1;
#else
    dirty = 1;
#endif
    refresh();
    goto end;
  }else if(eventPtr->type == ConfigureNotify){
#if 0
    ((struct x_vars *)dm_vars)->height = eventPtr->xconfigure.height;
    ((struct x_vars *)dm_vars)->width = eventPtr->xconfigure.width;
#endif
    X_configure_window_shape();

#if 0
    dmaflag = 1;
#else
    dirty = 1;
#endif
    refresh();
    goto end;
  } else if( eventPtr->type == MotionNotify ) {
    int mx, my;

    if ( !XdoMotion &&
	 (VIRTUAL_TRACKBALL_NOT_ACTIVE(struct x_vars *, mvars.virtual_trackball)) )
      goto end;

    rt_vls_init(&cmd);
    mx = eventPtr->xmotion.x;
    my = eventPtr->xmotion.y;

    switch(((struct x_vars *)dm_vars)->mvars.virtual_trackball){
    case VIRTUAL_TRACKBALL_OFF:
    case VIRTUAL_TRACKBALL_ON:
      /* trackball not active so do the regular thing */
      mx = (mx/(double)((struct x_vars *)dm_vars)->width - 0.5) * 4095;
      my = (0.5 - my/(double)((struct x_vars *)dm_vars)->height) * 4095;

      /* Constant tracking (e.g. illuminate mode) bound to M mouse */
      rt_vls_printf( &cmd, "M 0 %d %d\n", mx, my );
      break;
    case VIRTUAL_TRACKBALL_ROTATE:
      rt_vls_printf( &cmd, "irot %f %f 0\n",
		     (double)(my - ((struct x_vars *)dm_vars)->omy)/2.0,
		     (double)(mx - ((struct x_vars *)dm_vars)->omx)/2.0);
      break;
    case VIRTUAL_TRACKBALL_TRANSLATE:
      rt_vls_printf( &cmd, "tran %f %f %f",
		     ((double)mx/((struct x_vars *)dm_vars)->width - 0.5) * 2,
		     (0.5 - (double)my/((struct x_vars *)dm_vars)->height) * 2, tran_z);
      break;
    case VIRTUAL_TRACKBALL_ZOOM:
      rt_vls_printf( &cmd, "zoom %lf",
		     ((double)((struct x_vars *)dm_vars)->omy - my)/
		     ((struct x_vars *)dm_vars)->height + 1.0);
      break;
    }

    ((struct x_vars *)dm_vars)->omx = mx;
    ((struct x_vars *)dm_vars)->omy = my;
  } else {
    XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->win, &xwa);
    ((struct x_vars *)dm_vars)->height = xwa.height;
    ((struct x_vars *)dm_vars)->width = xwa.width;

    goto end;
  }

  (void)cmdline(&cmd, FALSE);
  rt_vls_free(&cmd);
end:
#if 1
  curr_dm_list = save_dm_list;
#endif
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
		((struct x_vars *)dm_vars)->pix,
#else
		((struct x_vars *)dm_vars)->win,
#endif
		((struct x_vars *)dm_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct x_vars *)dm_vars)->dpy,
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct x_vars *)dm_vars)->pix,
#else
	       ((struct x_vars *)dm_vars)->win,
#endif
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
#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
	       ((struct x_vars *)dm_vars)->pix,
#else
	       ((struct x_vars *)dm_vars)->win,
#endif
	       ((struct x_vars *)dm_vars)->gc, sx, sy, str, strlen(str) );
}

#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

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

  if(RT_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_CreateGenericHandler(Xdoevent, (ClientData)NULL);

  RT_LIST_APPEND(&head_x_vars.l, &((struct x_vars *)curr_dm_list->_dm_vars)->l);

  rt_vls_printf(&pathName, ".dm_x%d", count++);

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

#ifdef DOUBLE_BUFFERING_WITH_PIXMAPS
  ((struct x_vars *)dm_vars)->pix = Tk_GetPixmap(((struct x_vars *)dm_vars)->dpy,
    DefaultRootWindow(((struct x_vars *)dm_vars)->dpy), ((struct x_vars *)dm_vars)->width,
    ((struct x_vars *)dm_vars)->height, Tk_Depth(((struct x_vars *)dm_vars)->xtkwin));
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
    ((struct x_vars *)dm_vars)->gc = XCreateGC(((struct x_vars *)dm_vars)->dpy,
					       ((struct x_vars *)dm_vars)->win,
					       (GCForeground|GCBackground),
					       &gcv);

#ifndef CRAY2
    X_configure_window_shape();
#endif

#if 0    
    /* Register the file descriptor with the Tk event handler */
    Tk_CreateGenericHandler(Xdoevent, (ClientData)curr_dm_list);
#endif

    Tk_SetWindowBackground(((struct x_vars *)dm_vars)->xtkwin, ((struct x_vars *)dm_vars)->bg);
    Tk_MapWindow(((struct x_vars *)dm_vars)->xtkwin);

    return 0;
}

static void
X_configure_window_shape()
{
  XWindowAttributes xwa;
  XFontStruct     *newfontstruct;
  XGCValues       gcv;

  XGetWindowAttributes( ((struct x_vars *)dm_vars)->dpy,
			((struct x_vars *)dm_vars)->win, &xwa );
  ((struct x_vars *)dm_vars)->height = xwa.height;
  ((struct x_vars *)dm_vars)->width = xwa.width;

  /* First time through, load a font or quit */
  if (((struct x_vars *)dm_vars)->fontstruct == NULL) {
    if ((((struct x_vars *)dm_vars)->fontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct x_vars *)dm_vars)->fontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONTBACK)) == NULL) {
	rt_log( "dm-X: Can't open font '%s' or '%s'\n", FONT9, FONTBACK );
	return;
      }
    }

    gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
    XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
	      GCFont, &gcv);
  }

  /* Always try to choose a the font that best fits the window size.
   */

  if (((struct x_vars *)dm_vars)->width < 582) {
    if (((struct x_vars *)dm_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT5)) != NULL ) {
	XFreeFont(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->fontstruct);
	((struct x_vars *)dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dm_vars)->width < 679) {
    if (((struct x_vars *)dm_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT6)) != NULL ) {
	XFreeFont(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->fontstruct);
	((struct x_vars *)dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dm_vars)->width < 776) {
    if (((struct x_vars *)dm_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT7)) != NULL ) {
	XFreeFont(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->fontstruct);
	((struct x_vars *)dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dm_vars)->width < 873) {
    if (((struct x_vars *)dm_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT8)) != NULL ) {
	XFreeFont(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->fontstruct);
	((struct x_vars *)dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else {
    if (((struct x_vars *)dm_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dm_vars)->dpy, FONT9)) != NULL ) {
	XFreeFont(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->fontstruct);
	((struct x_vars *)dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
		  GCFont, &gcv);
      }
    }
  }
}

static void
establish_perspective()
{
  rt_vls_printf( &dm_values.dv_string,
		"set perspective %d\n",
		((struct x_vars *)dm_vars)->mvars.perspective_mode ?
		perspective_table[((struct x_vars *)dm_vars)->perspective_angle] : -1 );
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
    ((struct x_vars *)dm_vars)->perspective_angle = ((struct x_vars *)dm_vars)->mvars.dummy_perspective <= 4 ?
    ((struct x_vars *)dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct x_vars *)dm_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct x_vars *)dm_vars)->perspective_angle = 3;

  if(((struct x_vars *)dm_vars)->mvars.perspective_mode)
    rt_vls_printf( &dm_values.dv_string,
		  "set perspective %d\n",
		  perspective_table[((struct x_vars *)dm_vars)->perspective_angle] );

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct x_vars *)dm_vars)->mvars.dummy_perspective = 1;

  dmaflag = 1;
}

static void
establish_vtb()
{
  return;
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

  if(((struct x_vars *)dm_vars)->mvars.virtual_trackball){
  if( !strcmp( argv[0], "vtb" )){
    int buttonpress;

    if( argc < 5){
      rt_log("dm: need more parameters\n");
      rt_log("vtb <r|t|z> 1|0 xpos ypos\n");
      return CMD_BAD;
    }


    buttonpress = atoi(argv[2]);
    ((struct x_vars *)dm_vars)->omx = atoi(argv[3]);
    ((struct x_vars *)dm_vars)->omy = atoi(argv[4]);

    if(buttonpress){
      switch(*argv[1]){
      case 'r':
	((struct x_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ROTATE;
	break;
      case 't':
	{
	  struct rt_vls cmd;
	  rt_vls_init(&cmd);

	  ((struct x_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_TRANSLATE;
	  rt_vls_printf( &cmd, "tran %f %f %f",
			 ((double)((struct x_vars *)dm_vars)->omx/
			  ((struct x_vars *)dm_vars)->width - 0.5) * 2,
			 (0.5 - (double)((struct x_vars *)dm_vars)->omy/
			  ((struct x_vars *)dm_vars)->height) * 2, tran_z);

	  (void)cmdline(&cmd, FALSE);
	}
	break;
      case 'z':
	((struct x_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ZOOM;
	break;
      default:
	rt_log("dm: need more parameters\n");
	rt_log("vtb <r|t|z> 1|0 xpos ypos\n");
	return CMD_BAD;
      }
    }else{
      ((struct x_vars *)dm_vars)->mvars.virtual_trackball = VIRTUAL_TRACKBALL_ON;
    }

    return CMD_OK;
  }
  }else{
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
  ((struct x_vars *)dm_vars)->dm_list = curr_dm_list;
  ((struct x_vars *)dm_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */
  ((struct x_vars *)dm_vars)->mvars.dummy_perspective = 1;
  ((struct x_vars *)dm_vars)->mvars.virtual_trackball = 1;
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

  bzero((void *)&head_x_vars, sizeof(struct x_vars));
  RT_LIST_INIT( &head_x_vars.l );

  found = 0;
  rt_vls_init( &str );

  if((filename = getenv("DM_X_RCFILE")) == (char *)NULL )
    /* Use default file name */
    filename = DM_X_RCFILE;

  if((path = getenv("MGED_LIBRARY")) != (char *)NULL ){
    /* Use MGED_LIBRARY path */
    rt_vls_strcpy( &str, path );
    rt_vls_strcat( &str, "/" );
    rt_vls_strcat( &str, filename );

    if ((fp = fopen(rt_vls_addr(&str), "r")) != NULL )
      found = 1;
  }

  if(!found){
    if( (path = getenv("HOME")) != (char *)NULL )  {
      /* Use HOME path */
      rt_vls_strcpy( &str, path );
      rt_vls_strcat( &str, "/" );
      rt_vls_strcat( &str, filename );

      if( (fp = fopen(rt_vls_addr(&str), "r")) != NULL )
	found = 1;
    }
  }

  if( !found ) {
    /* Check current directory */
    if( (fp = fopen( filename, "r" )) != NULL )  {
      rt_vls_strcpy( &str, filename );
      found = 1;
    }
  }

  if(!found){
    rt_vls_free(&str);

    /* Using default */
    if(Tcl_Eval( interp, x_init_str ) == TCL_ERROR){
      rt_log("X_load_startup: Error interpreting x_init_str.\n");
      rt_log("%s\n", interp->result);
      return -1;
    }

    return 0;
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
  register struct x_vars *p;

  for( RT_LIST_FOR(p, x_vars, &head_x_vars.l) ){
    if(window == p->win)
      return p->dm_list;
  }

  return DM_LIST_NULL;
}
