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
#include "dm.h"
#include "dm-X.h"
#include "solid.h"

#define FONTBACK	"-adobe-courier-medium-r-normal--10-100-75-75-m-60-iso8859-1"
#define FONT5	"5x7"
#define FONT6	"6x10"
#define FONT7	"7x13"
#define FONT8	"8x13"
#define FONT9	"9x15"

void     X_configure_window_shape();
void     X_establish_perspective();
void     X_set_perspective();

static void	label();
static void	draw();
static void     x_var_init();
static void X_load_startup();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int     X_init();
static int	X_open();
static void	X_close();
static void	X_input();
static void	X_prolog(), X_epilog();
static void	X_normal(), X_newrot();
static void	X_update();
static void	X_puts(), X_2d_line(), X_light();
static int	X_object();
static unsigned X_cvtvecs(), X_load();
static void	X_viewchange(), X_colorchange();
static void	X_window(), X_debug();

#if TRY_COLOR_CUBE
static unsigned long get_pixel();
static void get_color_slot();
static void allocate_color_cube();
#endif

struct dm dm_X = {
  X_init,
  X_open, X_close,
  X_input,
  X_prolog, X_epilog,
  X_normal, X_newrot,
  X_update,
  X_puts, X_2d_line,
  X_light,
  X_object,	X_cvtvecs, X_load,
  Nu_void,
  X_viewchange,
  X_colorchange,
  X_window, X_debug, Nu_int0, Nu_int0,
  0,				/* no displaylist */
  0,				/* multi-window */
  PLOTBOUND,
  "X", "X Window System (X11)",
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

/* Currently, the application must define these. */
extern Tk_Window tkwin;

struct x_vars head_x_vars;
static int perspective_table[] = { 30, 45, 60, 90 };

static int
X_init(dmp, argc, argv)
struct dm *dmp;
int argc;
char *argv[];
{
  static int count = 0;

  /* Only need to do this once for this display manager */
  if(!count)
    X_load_startup(dmp);

  bu_vls_printf(&dmp->dmr_pathName, ".dm_x%d", count++);

  dmp->dmr_vars = bu_calloc(1, sizeof(struct x_vars), "X_init: x_vars");
  ((struct x_vars *)dmp->dmr_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */
  ((struct x_vars *)dmp->dmr_vars)->mvars.dummy_perspective = 1;

  if(BU_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_CreateGenericHandler(dmp->dmr_eventhandler, (ClientData)NULL);

  BU_LIST_APPEND(&head_x_vars.l, &((struct x_vars *)dmp->dmr_vars)->l);

  if(dmp->dmr_vars)
    return TCL_OK;

  return TCL_ERROR;
}

/*
 *			X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
static int
X_open(dmp)
struct dm *dmp;
{
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  int a_screen;
  Colormap  a_cmap;
  struct bu_vls str;
  Display *tmp_dpy;

  bu_vls_init(&str);

  ((struct x_vars *)dmp->dmr_vars)->fontstruct = NULL;

  /* Make xtkwin a toplevel window */
  ((struct x_vars *)dmp->dmr_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
				       bu_vls_addr(&dmp->dmr_pathName), dmp->dmr_dname);

  /*
   * Create the X drawing window by calling init_x which
   * is defined in xinit.tcl
   */
  bu_vls_strcpy(&str, "init_x ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&dmp->dmr_pathName));

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    return TCL_ERROR;
  }

  bu_vls_free(&str);

  ((struct x_vars *)dmp->dmr_vars)->dpy = Tk_Display(((struct x_vars *)dmp->dmr_vars)->xtkwin);
  ((struct x_vars *)dmp->dmr_vars)->width =
    DisplayWidth(((struct x_vars *)dmp->dmr_vars)->dpy,
		 DefaultScreen(((struct x_vars *)dmp->dmr_vars)->dpy)) - 20;
  ((struct x_vars *)dmp->dmr_vars)->height =
    DisplayHeight(((struct x_vars *)dmp->dmr_vars)->dpy,
		  DefaultScreen(((struct x_vars *)dmp->dmr_vars)->dpy)) - 20;

  /* Make window square */
  if(((struct x_vars *)dmp->dmr_vars)->height < ((struct x_vars *)dmp->dmr_vars)->width)
    ((struct x_vars *)dmp->dmr_vars)->width = ((struct x_vars *)dmp->dmr_vars)->height;
  else
    ((struct x_vars *)dmp->dmr_vars)->height = ((struct x_vars *)dmp->dmr_vars)->width;

  Tk_GeometryRequest(((struct x_vars *)dmp->dmr_vars)->xtkwin,
		     ((struct x_vars *)dmp->dmr_vars)->width, 
		     ((struct x_vars *)dmp->dmr_vars)->height);

#if 0
  /*XXX*/
  XSynchronize(((struct x_vars *)dmp->dmr_vars)->dpy, TRUE);
#endif

  Tk_MakeWindowExist(((struct x_vars *)dmp->dmr_vars)->xtkwin);
  ((struct x_vars *)dmp->dmr_vars)->win =
      Tk_WindowId(((struct x_vars *)dmp->dmr_vars)->xtkwin);

  ((struct x_vars *)dmp->dmr_vars)->pix = Tk_GetPixmap(((struct x_vars *)dmp->dmr_vars)->dpy,
    DefaultRootWindow(((struct x_vars *)dmp->dmr_vars)->dpy), ((struct x_vars *)dmp->dmr_vars)->width,
    ((struct x_vars *)dmp->dmr_vars)->height, Tk_Depth(((struct x_vars *)dmp->dmr_vars)->xtkwin));

  a_screen = Tk_ScreenNumber(((struct x_vars *)dmp->dmr_vars)->xtkwin);
  a_visual = Tk_Visual(((struct x_vars *)dmp->dmr_vars)->xtkwin);

  /* Get color map indices for the colors we use. */
  ((struct x_vars *)dmp->dmr_vars)->black = BlackPixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
  ((struct x_vars *)dmp->dmr_vars)->white = WhitePixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );

#if TRY_COLOR_CUBE
  ((struct x_vars *)dmp->dmr_vars)->cmap = a_cmap = Tk_Colormap(((struct x_vars *)dmp->dmr_vars)->xtkwin);
#else
  a_cmap = Tk_Colormap(((struct x_vars *)dmp->dmr_vars)->xtkwin);
#endif
  a_color.red = 255<<8;
  a_color.green=0;
  a_color.blue=0;
  a_color.flags = DoRed | DoGreen| DoBlue;
  if ( ! XAllocColor(((struct x_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
    Tcl_AppendResult(interp, "dm-X: Can't Allocate red\n", (char *)NULL);
    return TCL_ERROR;
  }
  ((struct x_vars *)dmp->dmr_vars)->red = a_color.pixel;
    if ( ((struct x_vars *)dmp->dmr_vars)->red == ((struct x_vars *)dmp->dmr_vars)->white )
      ((struct x_vars *)dmp->dmr_vars)->red = ((struct x_vars *)dmp->dmr_vars)->black;

    a_color.red = 200<<8;
    a_color.green=200<<8;
    a_color.blue=0<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
      Tcl_AppendResult(interp, "dm-X: Can't Allocate yellow\n", (char *)NULL);
      return TCL_ERROR;
    }
    ((struct x_vars *)dmp->dmr_vars)->yellow = a_color.pixel;
    if ( ((struct x_vars *)dmp->dmr_vars)->yellow == ((struct x_vars *)dmp->dmr_vars)->white )
      ((struct x_vars *)dmp->dmr_vars)->yellow = ((struct x_vars *)dmp->dmr_vars)->black;
    
    a_color.red = 0;
    a_color.green=0;
    a_color.blue=255<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
      Tcl_AppendResult(interp, "dm-X: Can't Allocate blue\n", (char *)NULL);
      return TCL_ERROR;
    }
    ((struct x_vars *)dmp->dmr_vars)->blue = a_color.pixel;
    if ( ((struct x_vars *)dmp->dmr_vars)->blue == ((struct x_vars *)dmp->dmr_vars)->white )
      ((struct x_vars *)dmp->dmr_vars)->blue = ((struct x_vars *)dmp->dmr_vars)->black;

    a_color.red = 128<<8;
    a_color.green=128<<8;
    a_color.blue= 128<<8;
    a_color.flags = DoRed | DoGreen| DoBlue;
    if ( ! XAllocColor(((struct x_vars *)dmp->dmr_vars)->dpy, a_cmap, &a_color)) {
      Tcl_AppendResult(interp, "dm-X: Can't Allocate gray\n", (char *)NULL);
      return TCL_ERROR;
    }
    ((struct x_vars *)dmp->dmr_vars)->gray = a_color.pixel;
    if ( ((struct x_vars *)dmp->dmr_vars)->gray == ((struct x_vars *)dmp->dmr_vars)->white )
      ((struct x_vars *)dmp->dmr_vars)->gray = ((struct x_vars *)dmp->dmr_vars)->black;

    /* Select border, background, foreground colors,
     * and border width.
     */
    if( a_visual->class == GrayScale || a_visual->class == StaticGray ) {
	((struct x_vars *)dmp->dmr_vars)->is_monochrome = 1;
	((struct x_vars *)dmp->dmr_vars)->bd = BlackPixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct x_vars *)dmp->dmr_vars)->bg = WhitePixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct x_vars *)dmp->dmr_vars)->fg = BlackPixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
    } else {
	/* Hey, it's a color server.  Ought to use 'em! */
	((struct x_vars *)dmp->dmr_vars)->is_monochrome = 0;
	((struct x_vars *)dmp->dmr_vars)->bd = WhitePixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct x_vars *)dmp->dmr_vars)->bg = BlackPixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
	((struct x_vars *)dmp->dmr_vars)->fg = WhitePixel( ((struct x_vars *)dmp->dmr_vars)->dpy, a_screen );
    }

    if( !((struct x_vars *)dmp->dmr_vars)->is_monochrome &&
	((struct x_vars *)dmp->dmr_vars)->fg != ((struct x_vars *)dmp->dmr_vars)->red &&
	((struct x_vars *)dmp->dmr_vars)->red != ((struct x_vars *)dmp->dmr_vars)->black )
      ((struct x_vars *)dmp->dmr_vars)->fg = ((struct x_vars *)dmp->dmr_vars)->red;

    gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->fg;
    gcv.background = ((struct x_vars *)dmp->dmr_vars)->bg;
    ((struct x_vars *)dmp->dmr_vars)->gc = XCreateGC(((struct x_vars *)dmp->dmr_vars)->dpy,
					       ((struct x_vars *)dmp->dmr_vars)->win,
					       (GCForeground|GCBackground),
					       &gcv);

#if TRY_COLOR_CUBE
    allocate_color_cube(dmp);
#endif

#ifndef CRAY2
    X_configure_window_shape(dmp);
#endif

    Tk_SetWindowBackground(((struct x_vars *)dmp->dmr_vars)->xtkwin, ((struct x_vars *)dmp->dmr_vars)->bg);
    Tk_MapWindow(((struct x_vars *)dmp->dmr_vars)->xtkwin);

    return TCL_OK;
}

/*
 *  			X _ C L O S E
 *  
 *  Gracefully release the display.
 */
static void
X_close(dmp)
struct dm *dmp;
{
  if(((struct x_vars *)dmp->dmr_vars)->gc != 0)
    XFreeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc);

  if(((struct x_vars *)dmp->dmr_vars)->pix != 0)
     Tk_FreePixmap(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix);

  if(((struct x_vars *)dmp->dmr_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct x_vars *)dmp->dmr_vars)->xtkwin);

  if(((struct x_vars *)dmp->dmr_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct x_vars *)dmp->dmr_vars)->l);

  bu_free(dmp->dmr_vars, "X_close: x_vars");

  if(BU_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_DeleteGenericHandler(dmp->dmr_eventhandler, (ClientData)NULL);
}

/*
 *			X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static void
X_prolog(dmp)
struct dm *dmp;
{
  XGCValues       gcv;

  gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->bg;
  XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix,
		 ((struct x_vars *)dmp->dmr_vars)->gc, 0, 0, ((struct x_vars *)dmp->dmr_vars)->width + 1,
		 ((struct x_vars *)dmp->dmr_vars)->height + 1);
}

/*
 *			X _ E P I L O G
 */
static void
X_epilog(dmp)
struct dm *dmp;
{
    /* Put the center point up last */
    draw( dmp, 0, 0, 0, 0 );

    XCopyArea(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix,
	      ((struct x_vars *)dmp->dmr_vars)->win, ((struct x_vars *)dmp->dmr_vars)->gc,
	      0, 0, ((struct x_vars *)dmp->dmr_vars)->width, ((struct x_vars *)dmp->dmr_vars)->height,
	      0, 0);

    /* Prevent lag between events and updates */
    XSync(((struct x_vars *)dmp->dmr_vars)->dpy, 0);
}

/*
 *  			X _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
static void
X_newrot(dmp, mat)
struct dm *dmp;
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
static int
X_object( dmp, vp, mat, illum, linestyle, r, g, b, index )
struct dm *dmp;
register struct rt_vlist *vp;
mat_t mat;
int illum;
int linestyle;
register short r, g, b;
short index;
{
    static vect_t   pnt;
    register struct rt_vlist	*tvp;
    int useful = TCL_ERROR;
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
    if(illum){    /* if highlighted */
      gcv.foreground = get_pixel(dmp, r, g, b);

      /* if solid color is already the same as the highlight color use double line width */
      if(gcv.foreground == ((struct x_vars *)dmp->dmr_vars)->white)
	line_width = 2;
      else
	gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->white;
    }else
      gcv.foreground = get_pixel(dmp, r, g, b);
#else
    gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->fg;
#endif
    XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy,
	      ((struct x_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv);

    if( linestyle )
      line_style = LineOnOffDash;

    XSetLineAttributes( ((struct x_vars *)dmp->dmr_vars)->dpy,
			((struct x_vars *)dmp->dmr_vars)->gc,
			line_width, line_style, CapButt, JoinMiter );

    nseg = 0;
    segp = segbuf;
    for( BU_LIST_FOR( tvp, rt_vlist, &vp->l ) )  {
	register int	i;
	register int	nused = tvp->nused;
	register int	*cmd = tvp->cmd;
	register point_t *pt = tvp->pt;

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
		x = GED_TO_Xx(dmp, pnt[0]);
		y = GED_TO_Xy(dmp, pnt[1]);
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
		gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->fg;
		if( illum && !((struct x_vars *)dmp->dmr_vars)->is_monochrome ){
		    gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->white;
		}
		XChangeGC( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv );
#endif
		pnt[0] *= 2047;
		pnt[1] *= 2047;
		x = GED_TO_Xx(dmp, pnt[0]);
		y = GED_TO_Xy(dmp, pnt[1]);

		segp->x1 = lastx;
		segp->y1 = lasty;
		segp->x2 = x;
		segp->y2 = y;
		nseg++;
		segp++;
		lastx = x;
		lasty = y;
		useful = TCL_OK;
		if( nseg == 1024 ) {
		    XDrawSegments( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix, ((struct x_vars *)dmp->dmr_vars)->gc, segbuf, nseg );
		    /* Thicken the drawing, if monochrome */
		    if( illum && ((struct x_vars *)dmp->dmr_vars)->is_monochrome ){
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
			XDrawSegments( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix, ((struct x_vars *)dmp->dmr_vars)->gc, segbuf, nseg );
		    }
		    nseg = 0;
		    segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
	XDrawSegments( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix, ((struct x_vars *)dmp->dmr_vars)->gc, segbuf, nseg );
	if( illum && ((struct x_vars *)dmp->dmr_vars)->is_monochrome ){
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
	    XDrawSegments( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->pix, ((struct x_vars *)dmp->dmr_vars)->gc, segbuf, nseg );
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
static void
X_normal(dmp)
struct dm *dmp;
{
	return;
}

/*
 *			X _ U P D A T E
 *
 * Transmit accumulated displaylist to the display processor.
 */
static void
X_update(dmp)
struct dm *dmp;
{
    XFlush(((struct x_vars *)dmp->dmr_vars)->dpy);
}

/*
 *			X _ P U T S
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static void
X_puts( dmp, str, x, y, size, color )
struct dm *dmp;
register char *str;
{
	XGCValues gcv;
	unsigned long fg;

	switch( color )  {
	case DM_BLACK:
		fg = ((struct x_vars *)dmp->dmr_vars)->black;
		break;
	case DM_RED:
		fg = ((struct x_vars *)dmp->dmr_vars)->red;
		break;
	case DM_BLUE:
		fg = ((struct x_vars *)dmp->dmr_vars)->blue;
		break;
	default:
	case DM_YELLOW:
		fg = ((struct x_vars *)dmp->dmr_vars)->yellow;
		break;
	case DM_WHITE:
		fg = ((struct x_vars *)dmp->dmr_vars)->gray;
		break;
	}
	gcv.foreground = fg;
	XChangeGC( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv );
	label( dmp, (double)x, (double)y, str );
}

/*
 *			X _ 2 D _ G O T O
 *
 */
static void
X_2d_line( dmp, x1, y1, x2, y2, dashed )
struct dm *dmp;
int x1, y1;
int x2, y2;
int dashed;
{
    XGCValues gcv;

    gcv.foreground = ((struct x_vars *)dmp->dmr_vars)->yellow;
    XChangeGC( ((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc, GCForeground, &gcv );
    if( dashed ) {
	XSetLineAttributes(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc, 1, LineOnOffDash, CapButt, JoinMiter );
    } else {
	XSetLineAttributes(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc, 1, LineSolid, CapButt, JoinMiter );
    }
    draw( dmp, x1, y1, x2, y2 );
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
static void
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
static void
X_light( dmp, cmd, func )
struct dm *dmp;
int cmd;
int func;			/* BE_ or BV_ function */
{
  return;
}

/* ARGSUSED */
static unsigned
X_cvtvecs( dmp, sp )
struct dm *dmp;
struct solid *sp;
{
  return 0;
}

/*
 * Loads displaylist
 */
static unsigned
X_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
  return 0;
}

static void
X_viewchange(dmp)
struct dm *dmp;
{
}

static void
X_colorchange(dmp)
struct dm *dmp;
{
  /* apply colors to the solid table */
  dmp->dmr_cfunc();
}

/* ARGSUSED */
static void
X_debug(dmp, lvl)
struct dm *dmp;
{
  XFlush(((struct x_vars *)dmp->dmr_vars)->dpy);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);
}

static void
X_window(dmp, w)
struct dm *dmp;
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
draw( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int	x1, y1;		/* from point */
int	x2, y2;		/* to point */
{
  int	sx1, sy1, sx2, sy2;

  sx1 = GED_TO_Xx(dmp, x1 );
  sy1 = GED_TO_Xy(dmp, y1 );
  sx2 = GED_TO_Xx(dmp, x2 );
  sy2 = GED_TO_Xy(dmp, y2 );

  if( sx1 == sx2 && sy1 == sy2 )
    XDrawPoint( ((struct x_vars *)dmp->dmr_vars)->dpy,
		((struct x_vars *)dmp->dmr_vars)->pix,
		((struct x_vars *)dmp->dmr_vars)->gc, sx1, sy1 );
  else
    XDrawLine( ((struct x_vars *)dmp->dmr_vars)->dpy,
	       ((struct x_vars *)dmp->dmr_vars)->pix,
	       ((struct x_vars *)dmp->dmr_vars)->gc, sx1, sy1, sx2, sy2 );
}

static void
label( dmp, x, y, str )
struct dm *dmp;
double	x, y;
char	*str;
{
  int	sx, sy;

  sx = GED_TO_Xx(dmp, x );
  sy = GED_TO_Xy(dmp, y );
  /* point is center of text? - seems like what MGED wants... */
  /* The following makes the menu look good, the rest bad */
  /*sy += ((struct x_vars *)dmp->dmr_vars)->fontstruct->max_bounds.ascent + ((struct x_vars *)dmp->dmr_vars)->fontstruct->max_bounds.descent/2);*/

  XDrawString( ((struct x_vars *)dmp->dmr_vars)->dpy,
	       ((struct x_vars *)dmp->dmr_vars)->pix,
	       ((struct x_vars *)dmp->dmr_vars)->gc, sx, sy, str, strlen(str) );
}

void
X_configure_window_shape(dmp)
struct dm *dmp;
{
  XWindowAttributes xwa;
  XFontStruct     *newfontstruct;
  XGCValues       gcv;

  XGetWindowAttributes( ((struct x_vars *)dmp->dmr_vars)->dpy,
			((struct x_vars *)dmp->dmr_vars)->win, &xwa );
  ((struct x_vars *)dmp->dmr_vars)->height = xwa.height;
  ((struct x_vars *)dmp->dmr_vars)->width = xwa.width;

  /* First time through, load a font or quit */
  if (((struct x_vars *)dmp->dmr_vars)->fontstruct == NULL) {
    if ((((struct x_vars *)dmp->dmr_vars)->fontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct x_vars *)dmp->dmr_vars)->fontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONTBACK)) == NULL) {
	Tcl_AppendResult(interp, "dm-X: Can't open font '", FONT9,
			 "' or '", FONTBACK, "'\n", (char *)NULL);
	return;
      }
    }

    gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
    XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
	      GCFont, &gcv);
  }

  /* Always try to choose a the font that best fits the window size.
   */

  if (((struct x_vars *)dmp->dmr_vars)->width < 582) {
    if (((struct x_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT5)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->fontstruct);
	((struct x_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dmr_vars)->width < 679) {
    if (((struct x_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT6)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->fontstruct);
	((struct x_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dmr_vars)->width < 776) {
    if (((struct x_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT7)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->fontstruct);
	((struct x_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dmr_vars)->width < 873) {
    if (((struct x_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT8)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->fontstruct);
	((struct x_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  } else {
    if (((struct x_vars *)dmp->dmr_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dmr_vars)->dpy, FONT9)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->fontstruct);
	((struct x_vars *)dmp->dmr_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dmr_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dmr_vars)->dpy, ((struct x_vars *)dmp->dmr_vars)->gc,
		  GCFont, &gcv);
      }
    }
  }
}

void
X_establish_perspective(dmp)
struct dm *dmp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf( &vls, "set perspective %d\n",
		((struct x_vars *)dmp->dmr_vars)->mvars.perspective_mode ?
		perspective_table[((struct x_vars *)dmp->dmr_vars)->perspective_angle] : -1 );

  Tcl_Eval(interp, bu_vls_addr(&vls));
  bu_vls_free(&vls);
#if 0
  dmaflag = 1;
#endif
}

/*
   This routine will toggle the perspective_angle if the
   dummy_perspective value is 0 or less. Otherwise, the
   perspective_angle is set to the value of (dummy_perspective - 1).
*/
void
X_set_perspective(dmp)
struct dm *dmp;
{
  /* set perspective matrix */
  if(((struct x_vars *)dmp->dmr_vars)->mvars.dummy_perspective > 0)
    ((struct x_vars *)dmp->dmr_vars)->perspective_angle =
      ((struct x_vars *)dmp->dmr_vars)->mvars.dummy_perspective <= 4 ?
      ((struct x_vars *)dmp->dmr_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct x_vars *)dmp->dmr_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct x_vars *)dmp->dmr_vars)->perspective_angle = 3;

  if(((struct x_vars *)dmp->dmr_vars)->mvars.perspective_mode){
    struct bu_vls vls;

    bu_vls_init(&vls);
     bu_vls_printf( &vls, "set perspective %d\n",
		    perspective_table[((struct x_vars *)dmp->dmr_vars)->perspective_angle] );

    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct x_vars *)dmp->dmr_vars)->mvars.dummy_perspective = 1;

#if 0
  dmaflag = 1;
#endif
}


static void
X_load_startup(dmp)
struct dm *dmp;
{
  char *filename;

  bzero((void *)&head_x_vars, sizeof(struct x_vars));
  BU_LIST_INIT( &head_x_vars.l );

  if((filename = getenv("DM_X_RCFILE")) != (char *)NULL )
    Tcl_EvalFile(interp, filename);
}


#if TRY_COLOR_CUBE
/* Return the allocated pixel value that most closely represents
the color requested. */
static unsigned long
get_pixel(dmp, r, g, b)
struct dm *dmp;
short r, g, b;
{
  short rr, rg, rb;

  get_color_slot(r, &rr);
  get_color_slot(g, &rg);
  get_color_slot(b, &rb);

  return(((struct x_vars *)dmp->dmr_vars)->pixel[rr * 36 + rg * 6 + rb]);
}

/* get color component value */
static void
get_color_slot(sc, c)
short sc;
short *c;
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
allocate_color_cube(dmp)
struct dm *dmp;
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
	if(XAllocColor(((struct x_vars *)dmp->dmr_vars)->dpy,
		       ((struct x_vars *)dmp->dmr_vars)->cmap, &color)){
	  if(color.pixel == ((struct x_vars *)dmp->dmr_vars)->bg)
	    /* that is, if the allocated color is the same as
	       the background color */
	    ((struct x_vars *)dmp->dmr_vars)->pixel[i++] = ((struct x_vars *)dmp->dmr_vars)->fg;	/* default foreground color, which may not be black */
	  else
	    ((struct x_vars *)dmp->dmr_vars)->pixel[i++] = color.pixel;
	}else	/* could not allocate a color */
	  ((struct x_vars *)dmp->dmr_vars)->pixel[i++] = ((struct x_vars *)dmp->dmr_vars)->fg;
      }
}
#endif
