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
#include "vmath.h"
#include "mater.h"
#include "bu.h"
#include "raytrace.h"
#include "./ged.h"
#include "./dm.h"
#include "./solid.h"
#include "./sedit.h"

extern int dm_pipe[];

static void	label();
static void	draw();
static int	X_setup();
static void     x_var_init();
static void     establish_perspective();
static void     set_perspective();
static void     X_configure_window_shape();
static void     establish_am();

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

static void X_load_startup();
static struct dm_list *get_dm_list();
#ifdef USE_PROTOTYPES
static Tk_GenericProc X_doevent;
#else
static int X_doevent();
#endif

#define TRY_COLOR_CUBE 1
#if TRY_COLOR_CUBE
#define NUM_PIXELS  216
static unsigned long get_pixel();
static void get_color_slot();
static void allocate_color_cube();
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
  int debug;
};

struct x_vars {
  struct rt_list l;
  struct dm_list *dm_list;
  Display *dpy;
  Tk_Window xtkwin;
  Window win;
  Pixmap pix;
  unsigned int mb_mask;
  int width;
  int height;
  int omx, omy;
  int perspective_angle;
  GC gc;
  XFontStruct *fontstruct;
  int is_monochrome;
  unsigned long black,gray,white,yellow,red,blue;
  unsigned long bd, bg, fg;   /* color of border, background, foreground */
#if TRY_COLOR_CUBE
  Colormap cmap;
  unsigned long   pixel[NUM_PIXELS];
#endif
  struct modifiable_x_vars mvars;
};

#define X_MV_O(_m) offsetof(struct modifiable_x_vars, _m)
struct bu_structparse X_vparse[] = {
  {"%d",  1, "perspective",       X_MV_O(perspective_mode), establish_perspective },
  {"%d",  1, "set_perspective",   X_MV_O(dummy_perspective),set_perspective },
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
  x_var_init();

  return X_setup(dname);
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

  if(((struct x_vars *)dm_vars)->pix != 0)
     Tk_FreePixmap(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix);

  if(((struct x_vars *)dm_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct x_vars *)dm_vars)->xtkwin);

  if(((struct x_vars *)dm_vars)->l.forw != RT_LIST_NULL)
    RT_LIST_DEQUEUE(&((struct x_vars *)dm_vars)->l);

  bu_free(dm_vars, "X_close: dm_vars");

  if(RT_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_DeleteGenericHandler(X_doevent, (ClientData)NULL);
}

/*
 *			X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
void
X_prolog()
{
  XGCValues       gcv;

  gcv.foreground = ((struct x_vars *)dm_vars)->bg;
  XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix,
		 ((struct x_vars *)dm_vars)->gc, 0, 0, ((struct x_vars *)dm_vars)->width + 1,
		 ((struct x_vars *)dm_vars)->height + 1);
}

/*
 *			X _ E P I L O G
 */
void
X_epilog()
{
    /* Put the center point up last */
    draw( 0, 0, 0, 0 );

    XCopyArea(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix,
	      ((struct x_vars *)dm_vars)->win, ((struct x_vars *)dm_vars)->gc,
	      0, 0, ((struct x_vars *)dm_vars)->width, ((struct x_vars *)dm_vars)->height,
	      0, 0);

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
    int   line_width = 1;
    int   line_style = LineSolid;

#if TRY_COLOR_CUBE
    if(white_flag){    /* if highlighted */
      gcv.foreground = get_pixel(sp->s_color);

      /* if solid color is already the same as the highlight color use double line width */
      if(gcv.foreground == ((struct x_vars *)dm_vars)->white)
	line_width = 2;
      else
	gcv.foreground = ((struct x_vars *)dm_vars)->white;
    }else
      gcv.foreground = get_pixel(sp->s_color);
#else
    gcv.foreground = ((struct x_vars *)dm_vars)->fg;
#endif
    XChangeGC(((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv);

    if( sp->s_soldash )
      line_style = LineOnOffDash;

    XSetLineAttributes( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc,
			line_width, line_style, CapButt, JoinMiter );

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
#if !TRY_COLOR_CUBE
		gcv.foreground = ((struct x_vars *)dm_vars)->fg;
		if( white_flag && !((struct x_vars *)dm_vars)->is_monochrome ){
		    gcv.foreground = ((struct x_vars *)dm_vars)->white;
		}
		XChangeGC( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->gc, GCForeground, &gcv );
#endif
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
		    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
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
			XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
		    }
		    nseg = 0;
		    segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
	XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
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
	    XDrawSegments( ((struct x_vars *)dm_vars)->dpy, ((struct x_vars *)dm_vars)->pix, ((struct x_vars *)dm_vars)->gc, segbuf, nseg );
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

  save_dm_list = curr_dm_list;
  curr_dm_list = get_dm_list(eventPtr->xany.window);

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
    X_configure_window_shape();

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
  return 0;
}

/*
 * Loads displaylist
 */
unsigned
X_load( addr, count )
unsigned addr, count;
{
  return 0;
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
	  Tcl_AppendResult(interp, "X_statechange: unknown state ",
			   state_str[b], "\n", (char *)NULL);
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
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);
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
		((struct x_vars *)dm_vars)->pix,
		((struct x_vars *)dm_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct x_vars *)dm_vars)->dpy,
	       ((struct x_vars *)dm_vars)->pix,
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
	       ((struct x_vars *)dm_vars)->pix,
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
X_setup( name )
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
  struct bu_vls str;
  Display *tmp_dpy;

  bu_vls_init(&str);

  /* Only need to do this once */
  if(tkwin == NULL)
    gui_setup();

  /* Only need to do this once for this display manager */
  if(!count)
    X_load_startup();

  if(RT_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_CreateGenericHandler(X_doevent, (ClientData)NULL);

  RT_LIST_APPEND(&head_x_vars.l, &((struct x_vars *)curr_dm_list->_dm_vars)->l);

  bu_vls_printf(&pathName, ".dm_x%d", count++);

  /* Make xtkwin a toplevel window */
  ((struct x_vars *)dm_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						       bu_vls_addr(&pathName), name);

  /*
   * Create the X drawing window by calling init_x which
   * is defined in xinit.tcl
   */
  bu_vls_strcpy(&str, "init_x ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&pathName));

  if(cmdline(&str, FALSE) == CMD_BAD){
    bu_vls_free(&str);
    return -1;
  }

  bu_vls_free(&str);

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

#if 0
  /*XXX*/
  XSynchronize(((struct x_vars *)dm_vars)->dpy, TRUE);
#endif

  Tk_MakeWindowExist(((struct x_vars *)dm_vars)->xtkwin);
  ((struct x_vars *)dm_vars)->win =
      Tk_WindowId(((struct x_vars *)dm_vars)->xtkwin);

  ((struct x_vars *)dm_vars)->pix = Tk_GetPixmap(((struct x_vars *)dm_vars)->dpy,
    DefaultRootWindow(((struct x_vars *)dm_vars)->dpy), ((struct x_vars *)dm_vars)->width,
    ((struct x_vars *)dm_vars)->height, Tk_Depth(((struct x_vars *)dm_vars)->xtkwin));

  a_screen = Tk_ScreenNumber(((struct x_vars *)dm_vars)->xtkwin);
  a_visual = Tk_Visual(((struct x_vars *)dm_vars)->xtkwin);

  /* Get color map indices for the colors we use. */
  ((struct x_vars *)dm_vars)->black = BlackPixel( ((struct x_vars *)dm_vars)->dpy, a_screen );
  ((struct x_vars *)dm_vars)->white = WhitePixel( ((struct x_vars *)dm_vars)->dpy, a_screen );

#if TRY_COLOR_CUBE
  ((struct x_vars *)dm_vars)->cmap = a_cmap = Tk_Colormap(((struct x_vars *)dm_vars)->xtkwin);
#else
  a_cmap = Tk_Colormap(((struct x_vars *)dm_vars)->xtkwin);
#endif
  a_color.red = 255<<8;
  a_color.green=0;
  a_color.blue=0;
  a_color.flags = DoRed | DoGreen| DoBlue;
  if ( ! XAllocColor(((struct x_vars *)dm_vars)->dpy, a_cmap, &a_color)) {
    Tcl_AppendResult(interp, "dm-X: Can't Allocate red\n", (char *)NULL);
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
      Tcl_AppendResult(interp, "dm-X: Can't Allocate yellow\n", (char *)NULL);
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
      Tcl_AppendResult(interp, "dm-X: Can't Allocate blue\n", (char *)NULL);
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
      Tcl_AppendResult(interp, "dm-X: Can't Allocate gray\n", (char *)NULL);
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

#if TRY_COLOR_CUBE
    allocate_color_cube();
#endif

#ifndef CRAY2
    X_configure_window_shape();
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
	Tcl_AppendResult(interp, "dm-X: Can't open font '", FONT9,
			 "' or '", FONTBACK, "'\n", (char *)NULL);
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
  bu_vls_printf( &dm_values.dv_string,
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
    bu_vls_printf( &dm_values.dv_string,
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
establish_am()
{
  return;
}

int
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
x_var_init()
{
  dm_vars = (char *)bu_malloc(sizeof(struct x_vars), "x_var_init: x_vars");
  bzero((void *)dm_vars, sizeof(struct x_vars));
  ((struct x_vars *)dm_vars)->dm_list = curr_dm_list;
  ((struct x_vars *)dm_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */
  ((struct x_vars *)dm_vars)->mvars.dummy_perspective = 1;
}

static void
X_load_startup()
{
  char *filename;

  bzero((void *)&head_x_vars, sizeof(struct x_vars));
  RT_LIST_INIT( &head_x_vars.l );

  if((filename = getenv("DM_X_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
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

#if TRY_COLOR_CUBE
/* Return the allocated pixel value that most closely represents
the color requested. */
static unsigned long
get_pixel(s_color)
unsigned char	*s_color;
{
  unsigned char	r, g, b;

  get_color_slot(s_color[0], &r);
  get_color_slot(s_color[1], &g);
  get_color_slot(s_color[2], &b);

  return(((struct x_vars *)dm_vars)->pixel[r * 36 + g * 6 + b]);
}

/* get color component value */
static void
get_color_slot(sc, c)
unsigned char sc;
unsigned char *c;
{
  if(sc < 42)
	*c = 0;
  else if(sc < 85)
	*c = 1;
  else if(sc < 127)
	*c = 2;
  else if(sc < 170)
	*c = 3;
  else if(sc < 212)
	*c = 4;
  else
	*c = 5;
}

/* Try to allocate 216 colors. If a color cannot be
allocated, the default foreground color will be used.*/
static void
allocate_color_cube()
{
  int	i = 0;
  XColor	color;

  int r, g, b;

  for(r = 0; r < 65026; r = r + 13005)
    for(g = 0; g < 65026; g = g + 13005)
      for(b = 0; b < 65026; b = b + 13005){
	color.red = (unsigned short)r;
	color.green = (unsigned short)g;
	color.blue = (unsigned short)b;
	if(XAllocColor(((struct x_vars *)dm_vars)->dpy,
		       ((struct x_vars *)dm_vars)->cmap, &color)){
	  if(color.pixel == ((struct x_vars *)dm_vars)->bg)
	    /* that is, if the allocated color is the same as
	       the background color */
	    ((struct x_vars *)dm_vars)->pixel[i++] = ((struct x_vars *)dm_vars)->fg;	/* default foreground color, which may not be black */
	  else
	    ((struct x_vars *)dm_vars)->pixel[i++] = color.pixel;
	}else	/* could not allocate a color */
	  ((struct x_vars *)dm_vars)->pixel[i++] = ((struct x_vars *)dm_vars)->fg;
      }
}
#endif
