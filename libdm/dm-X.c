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
#include "tk.h"
#include <X11/Xutil.h>

#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

#include "machine.h"
#include "externs.h"
#include "bu.h"

#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-X.h"

void     X_configure_window_shape();
void     X_establish_perspective();
void     X_set_perspective();

static void	label();
static void	draw();
static void     x_var_init();
static int X_load_startup();
static int X_set_visual();
#define CMAP_BASE 32

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
static int	X_init();
static int	X_open();
static int	X_close();
static int	X_drawBegin(), X_drawEnd();
static int	X_normal(), X_newrot();
static int	X_drawString2D(), X_drawLine2D();
static int      X_drawVertex2D();
static int	X_drawVlist();
static int      X_setColor(), X_setLineAttr();
static unsigned X_cvtvecs(), X_load();
static int	X_setWinBounds(), X_debug();

static unsigned long get_pixel();
static void get_color_slot();
static void allocate_color_cube();

struct dm dm_X = {
  X_init,
  X_open,
  X_close,
  X_drawBegin,
  X_drawEnd,
  X_normal,
  X_newrot,
  X_drawString2D,
  X_drawLine2D,
  X_drawVertex2D,
  X_drawVlist,
  X_setColor,
  X_setLineAttr,
  X_cvtvecs,
  X_load,
  X_setWinBounds,
  X_debug,
  Nu_int0,
  0,				/* no displaylist */
  PLOTBOUND,
  "X", "X Window System (X11)",
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
    (void)X_load_startup(dmp);

  bu_vls_printf(&dmp->dm_pathName, ".dm_x%d", count++);

  dmp->dm_vars = bu_calloc(1, sizeof(struct x_vars), "X_init: x_vars");
  ((struct x_vars *)dmp->dm_vars)->perspective_angle = 3;

  /* initialize the modifiable variables */
  ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;

  if(BU_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_CreateGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_X);

  BU_LIST_APPEND(&head_x_vars.l, &((struct x_vars *)dmp->dm_vars)->l);

  if(dmp->dm_vars)
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

  ((struct x_vars *)dmp->dm_vars)->fontstruct = NULL;

  /* Make xtkwin a toplevel window */
  ((struct x_vars *)dmp->dm_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
				       bu_vls_addr(&dmp->dm_pathName), dmp->dm_dname);

  /*
   * Create the X drawing window by calling init_x which
   * is defined in xinit.tcl
   */
  bu_vls_strcpy(&str, "init_x ");
  bu_vls_printf(&str, "%s\n", bu_vls_addr(&dmp->dm_pathName));

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    return TCL_ERROR;
  }

  bu_vls_free(&str);

  ((struct x_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct x_vars *)dmp->dm_vars)->xtkwin);
  ((struct x_vars *)dmp->dm_vars)->width =
    DisplayWidth(((struct x_vars *)dmp->dm_vars)->dpy,
		 DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 20;
  ((struct x_vars *)dmp->dm_vars)->height =
    DisplayHeight(((struct x_vars *)dmp->dm_vars)->dpy,
		  DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 20;

  /* Make window square */
  if(((struct x_vars *)dmp->dm_vars)->height < ((struct x_vars *)dmp->dm_vars)->width)
    ((struct x_vars *)dmp->dm_vars)->width = ((struct x_vars *)dmp->dm_vars)->height;
  else
    ((struct x_vars *)dmp->dm_vars)->height = ((struct x_vars *)dmp->dm_vars)->width;

  Tk_GeometryRequest(((struct x_vars *)dmp->dm_vars)->xtkwin,
		     ((struct x_vars *)dmp->dm_vars)->width, 
		     ((struct x_vars *)dmp->dm_vars)->height);

#if 0
  /*XXX*/
  XSynchronize(((struct x_vars *)dmp->dm_vars)->dpy, TRUE);
#endif

  a_screen = Tk_ScreenNumber(((struct x_vars *)dmp->dm_vars)->xtkwin);

  /* must do this before MakeExist */
  if(X_set_visual(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->xtkwin,
		  &((struct x_vars *)dmp->dm_vars)->cmap,
		  &((struct x_vars *)dmp->dm_vars)->depth) == NULL){
    Tcl_AppendResult(interp, "X_open: Can't get an appropriate visual.\n", (char *)NULL);
    return TCL_ERROR;
  }

  Tk_MakeWindowExist(((struct x_vars *)dmp->dm_vars)->xtkwin);
  ((struct x_vars *)dmp->dm_vars)->win =
      Tk_WindowId(((struct x_vars *)dmp->dm_vars)->xtkwin);

  ((struct x_vars *)dmp->dm_vars)->pix =
    Tk_GetPixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		 DefaultRootWindow(((struct x_vars *)dmp->dm_vars)->dpy),
		 ((struct x_vars *)dmp->dm_vars)->width,
		 ((struct x_vars *)dmp->dm_vars)->height,
		 Tk_Depth(((struct x_vars *)dmp->dm_vars)->xtkwin));

  allocate_color_cube(dmp);
  
  gcv.background = ((struct x_vars *)dmp->dm_vars)->bg;
  gcv.foreground = ((struct x_vars *)dmp->dm_vars)->fg;
  ((struct x_vars *)dmp->dm_vars)->gc = XCreateGC(((struct x_vars *)dmp->dm_vars)->dpy,
						  ((struct x_vars *)dmp->dm_vars)->win,
						  (GCForeground|GCBackground), &gcv);

#ifndef CRAY2
  X_configure_window_shape(dmp);
#endif

  Tk_SetWindowBackground(((struct x_vars *)dmp->dm_vars)->xtkwin,
			 ((struct x_vars *)dmp->dm_vars)->bg);
  Tk_MapWindow(((struct x_vars *)dmp->dm_vars)->xtkwin);

  return TCL_OK;
}

/*
 *  			X _ C L O S E
 *  
 *  Gracefully release the display.
 */
static int
X_close(dmp)
struct dm *dmp;
{
  if(((struct x_vars *)dmp->dm_vars)->gc != 0)
    XFreeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars)->gc);

  if(((struct x_vars *)dmp->dm_vars)->pix != 0)
     Tk_FreePixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		   ((struct x_vars *)dmp->dm_vars)->pix);

  /*XXX Possibly need to free the colormap */

  if(((struct x_vars *)dmp->dm_vars)->xtkwin != 0)
    Tk_DestroyWindow(((struct x_vars *)dmp->dm_vars)->xtkwin);

  if(((struct x_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct x_vars *)dmp->dm_vars)->l);

  bu_free(dmp->dm_vars, "X_close: x_vars");

  if(BU_LIST_IS_EMPTY(&head_x_vars.l))
    Tk_DeleteGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_X);

  return TCL_OK;
}

/*
 *			X _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static int
X_drawBegin(dmp)
struct dm *dmp;
{
  XGCValues       gcv;

  gcv.foreground = ((struct x_vars *)dmp->dm_vars)->bg;
  XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct x_vars *)dmp->dm_vars)->dpy,
		 ((struct x_vars *)dmp->dm_vars)->pix,
		 ((struct x_vars *)dmp->dm_vars)->gc, 0,
		 0, ((struct x_vars *)dmp->dm_vars)->width + 1,
		 ((struct x_vars *)dmp->dm_vars)->height + 1);

  return TCL_OK;
}

/*
 *			X _ E P I L O G
 */
static int
X_drawEnd(dmp)
struct dm *dmp;
{
  XCopyArea(((struct x_vars *)dmp->dm_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars)->pix,
	    ((struct x_vars *)dmp->dm_vars)->win,
	    ((struct x_vars *)dmp->dm_vars)->gc,
	      0, 0, ((struct x_vars *)dmp->dm_vars)->width,
	    ((struct x_vars *)dmp->dm_vars)->height, 0, 0);

  /* Prevent lag between events and updates */
  XSync(((struct x_vars *)dmp->dm_vars)->dpy, 0);

  return TCL_OK;
}

/*
 *  			X _ N E W R O T
 *  Stub.
 */
/* ARGSUSED */
static int
X_newrot(dmp, mat)
struct dm *dmp;
mat_t mat;
{
  return TCL_OK;
}

/*
 *  			X _ D R A W V L I S T
 *  
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
static int
X_drawVlist( dmp, vp, mat )
struct dm *dmp;
register struct rt_vlist *vp;
mat_t mat;
{
    static vect_t   pnt;
    register struct rt_vlist	*tvp;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    XGCValues gcv;
    int	nseg;			        /* number of segments */
    int	x, y;
    int	lastx = 0;
    int	lasty = 0;

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
		if( nseg == 1024 ) {
		  XDrawSegments( ((struct x_vars *)dmp->dm_vars)->dpy,
				 ((struct x_vars *)dmp->dm_vars)->pix,
				 ((struct x_vars *)dmp->dm_vars)->gc, segbuf, nseg );

		  nseg = 0;
		  segp = segbuf;
		}
		break;
	    }
	}
    }
    if( nseg ) {
      XDrawSegments( ((struct x_vars *)dmp->dm_vars)->dpy,
		     ((struct x_vars *)dmp->dm_vars)->pix,
		     ((struct x_vars *)dmp->dm_vars)->gc, segbuf, nseg );
    }

    return TCL_OK;
}

/*
 *			X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
X_normal(dmp)
struct dm *dmp;
{
  return TCL_OK;
}

/*
 *			X _ D R A W S T R I N G 2 D
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
static int
X_drawString2D( dmp, str, x, y, size )
struct dm *dmp;
register char *str;
int x, y;
int size;
{
  int	sx, sy;

  sx = GED_TO_Xx(dmp, x );
  sy = GED_TO_Xy(dmp, y );

  XDrawString( ((struct x_vars *)dmp->dm_vars)->dpy,
	       ((struct x_vars *)dmp->dm_vars)->pix,
	       ((struct x_vars *)dmp->dm_vars)->gc,
	       sx, sy, str, strlen(str) );

  return TCL_OK;
}

static int
X_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  int	sx1, sy1, sx2, sy2;

  sx1 = GED_TO_Xx(dmp, x1 );
  sy1 = GED_TO_Xy(dmp, y1 );
  sx2 = GED_TO_Xx(dmp, x2 );
  sy2 = GED_TO_Xy(dmp, y2 );

  XDrawLine( ((struct x_vars *)dmp->dm_vars)->dpy,
	     ((struct x_vars *)dmp->dm_vars)->pix,
	     ((struct x_vars *)dmp->dm_vars)->gc,
	     sx1, sy1, sx2, sy2 );

  return TCL_OK;
}

static int
X_drawVertex2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  int   sx, sy;

  sx = GED_TO_Xx(dmp, x );
  sy = GED_TO_Xy(dmp, y );

  XDrawPoint( ((struct x_vars *)dmp->dm_vars)->dpy,
	      ((struct x_vars *)dmp->dm_vars)->pix,
	      ((struct x_vars *)dmp->dm_vars)->gc, sx, sy );

  return TCL_OK;
}


static int
X_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  XGCValues gcv;

  gcv.foreground = get_pixel(dmp, r, g, b);
  XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars)->gc,
	    GCForeground, &gcv);

  return TCL_OK;
}

static int
X_setLineAttr(dmp, width, dashed)
struct dm *dmp;
int width;
int dashed;
{
  int linestyle = LineSolid;

  if(dashed)
    linestyle = LineOnOffDash;

  if(width < 1)
    width = 1;

  XSetLineAttributes( ((struct x_vars *)dmp->dm_vars)->dpy,
		      ((struct x_vars *)dmp->dm_vars)->gc,
		      width, linestyle, CapButt, JoinMiter );

  return TCL_OK;
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

/* ARGSUSED */
static int
X_debug(dmp, lvl)
struct dm *dmp;
{
  XFlush(((struct x_vars *)dmp->dm_vars)->dpy);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);

  return TCL_OK;
}

static int
X_setWinBounds(dmp, w)
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

  return TCL_OK;
}

void
X_configure_window_shape(dmp)
struct dm *dmp;
{
  XWindowAttributes xwa;
  XFontStruct     *newfontstruct;
  XGCValues       gcv;

  XGetWindowAttributes( ((struct x_vars *)dmp->dm_vars)->dpy,
			((struct x_vars *)dmp->dm_vars)->win, &xwa );
  ((struct x_vars *)dmp->dm_vars)->height = xwa.height;
  ((struct x_vars *)dmp->dm_vars)->width = xwa.width;

  /* First time through, load a font or quit */
  if (((struct x_vars *)dmp->dm_vars)->fontstruct == NULL) {
    if ((((struct x_vars *)dmp->dm_vars)->fontstruct =
	 XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy, FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct x_vars *)dmp->dm_vars)->fontstruct =
	   XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy, FONTBACK)) == NULL) {
	Tcl_AppendResult(interp, "dm-X: Can't open font '", FONT9,
			 "' or '", FONTBACK, "'\n", (char *)NULL);
	return;
      }
    }

    gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
    XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	      ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
  }

  /* Always try to choose a the font that best fits the window size.
   */

  if (((struct x_vars *)dmp->dm_vars)->width < 582) {
    if (((struct x_vars *)dmp->dm_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy,
					  FONT5)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->fontstruct);
	((struct x_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dm_vars)->width < 679) {
    if (((struct x_vars *)dmp->dm_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy,
					  FONT6)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->fontstruct);
	((struct x_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dm_vars)->width < 776) {
    if (((struct x_vars *)dmp->dm_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy,
					  FONT7)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->fontstruct);
	((struct x_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
      }
    }
  } else if (((struct x_vars *)dmp->dm_vars)->width < 873) {
    if (((struct x_vars *)dmp->dm_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy,
					  FONT8)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->fontstruct);
	((struct x_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
      }
    }
  } else {
    if (((struct x_vars *)dmp->dm_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct x_vars *)dmp->dm_vars)->dpy,
					  FONT9)) != NULL ) {
	XFreeFont(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->fontstruct);
	((struct x_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	gcv.font = ((struct x_vars *)dmp->dm_vars)->fontstruct->fid;
	XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->gc, GCFont, &gcv);
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
		 ((struct x_vars *)dmp->dm_vars)->mvars.perspective_mode ?
		 perspective_table[((struct x_vars *)dmp->dm_vars)->perspective_angle] :
		 -1 );

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
  if(((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective > 0)
    ((struct x_vars *)dmp->dm_vars)->perspective_angle =
      ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective <= 4 ?
      ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct x_vars *)dmp->dm_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct x_vars *)dmp->dm_vars)->perspective_angle = 3;

  if(((struct x_vars *)dmp->dm_vars)->mvars.perspective_mode){
    struct bu_vls vls;

    bu_vls_init(&vls);
     bu_vls_printf( &vls, "set perspective %d\n",
		    perspective_table[((struct x_vars *)dmp->dm_vars)->perspective_angle] );

    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;

#if 0
  dmaflag = 1;
#endif
}


static int
X_load_startup(dmp)
struct dm *dmp;
{
  char *filename;

  bzero((void *)&head_x_vars, sizeof(struct x_vars));
  BU_LIST_INIT( &head_x_vars.l );

  if((filename = getenv("DM_X_RCFILE")) != (char *)NULL )
    return Tcl_EvalFile(interp, filename);

  return TCL_OK;
}


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

  return(((struct x_vars *)dmp->dm_vars)->pixel[rr * 36 + rg * 6 + rb]);
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

static void
allocate_color_cube(dmp)
struct dm *dmp;
{
  XColor color;
  XColor colors[CMAP_BASE];
  Colormap cmap;
  int i;
  int r, g, b;

  /* store default colors below CMAP_BASE */
  cmap = DefaultColormap(((struct x_vars *)dmp->dm_vars)->dpy,
			 DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy));
  for(i = 0; i < CMAP_BASE; ++i)
    colors[i].pixel = i;
  XQueryColors(((struct x_vars *)dmp->dm_vars)->dpy, cmap, colors, CMAP_BASE);
  for(i = 0; i < CMAP_BASE; ++i)
    XStoreColor(((struct x_vars *)dmp->dm_vars)->dpy,
		((struct x_vars *)dmp->dm_vars)->cmap,
		&colors[i]);

  /* store color cube above CMAP_BASE */
  for(i = r = 0; r < 65026; r = r + 13005)
    for(g = 0; g < 65026; g = g + 13005)
      for(b = 0; b < 65026; b = b + 13005){
	color.red = (unsigned short)r;
	color.green = (unsigned short)g;
	color.blue = (unsigned short)b;
	((struct x_vars *)dmp->dm_vars)->pixel[i] = color.pixel = i++ + CMAP_BASE;
	color.flags = DoRed|DoGreen|DoBlue;
	XStoreColor(((struct x_vars *)dmp->dm_vars)->dpy,
		    ((struct x_vars *)dmp->dm_vars)->cmap,
		    &color);
      }

  /* black */
  ((struct x_vars *)dmp->dm_vars)->bg = ((struct x_vars *)dmp->dm_vars)->pixel[0];

  /* red */
  ((struct x_vars *)dmp->dm_vars)->fg = ((struct x_vars *)dmp->dm_vars)->pixel[180];
}

static int
X_set_visual(dpy, tkwin, cmap, depth)
Display *dpy;
Tk_Window tkwin;
Colormap *cmap;
int *depth;
{
  XVisualInfo *vip, vitemp, *vibase, *maxvip;
  int good[40];
  int num, i, j;
  int tries, baddepth;

  /* Try to satisfy the above desires with a color visual of the
   * greatest depth */

  vibase = XGetVisualInfo(dpy, 0, &vitemp, &num);

  while (1) {
    for (i=0, j=0, vip=vibase; i<num; i++, vip++){
      /* requirements */
      if (vip->class != PseudoColor) {
	/* if index mode, accept only read/write*/
	continue;
      }
			
      /* this visual meets criteria */
      good[j++] = i;
    }

    /* j = number of acceptable visuals under consideration */
    if (j >= 1){
      baddepth = 1000;
      for(tries = 0; tries < j; ++tries) {
	maxvip = vibase + good[0];
	for (i=1; i<j; i++) {
	  vip = vibase + good[i];
	  if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)){
	    maxvip = vip;
	  }
	}

	/* make sure Tk handles it */
	*cmap = XCreateColormap(dpy, RootWindow(dpy, maxvip->screen),
				maxvip->visual, AllocAll);

	if (Tk_SetWindowVisual(tkwin, maxvip->visual, maxvip->depth, *cmap)){
	  *depth = maxvip->depth;
	  return 1; /* success */
	} else { 
	  /* retry with lesser depth */
	  baddepth = maxvip->depth;
	  XFreeColormap(dpy, *cmap);
	}
      }
    }

    return(NULL); /* failure */
  }
}
