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
#include <stdio.h>
#include <limits.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
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
#include "solid.h"

void     X_configure_window_shape();
void     X_establish_perspective();
void     X_set_perspective();
int      X_drawString2D();

static void	label();
static void	draw();
static void     x_var_init();
static int X_set_visual();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*X_open();
static int	X_close();
static int	X_drawBegin(), X_drawEnd();
static int	X_normal(), X_newrot();
static int	X_drawLine2D();
static int      X_drawVertex2D();
static int	X_drawVList();
static int      X_setColor(), X_setLineAttr();
static unsigned X_cvtvecs(), X_load();
static int	X_setWinBounds(), X_debug();

struct dm dm_X = {
  X_close,
  X_drawBegin,
  X_drawEnd,
  X_normal,
  X_newrot,
  X_drawString2D,
  X_drawLine2D,
  X_drawVertex2D,
  X_drawVList,
  X_setColor,
  X_setLineAttr,
  X_cvtvecs,
  X_load,
  X_setWinBounds,
  X_debug,
  Nu_int0,
  0,				/* no displaylist */
  PLOTBOUND,
  "X",
  "X Window System (X11)",
  DM_TYPE_X,
  1,
  0,
  0,
  0,
  0,
  1.0, /* aspect ratio */
  0,
  0,
  0,
  0,
  0,
  0
};

fastf_t min_short = (fastf_t)SHRT_MIN;
fastf_t max_short = (fastf_t)SHRT_MAX;

/* Currently, the application must define these. */
extern Tk_Window tkwin;

struct x_vars head_x_vars;
static int perspective_table[] = { 30, 45, 60, 90 };

/*
 *			X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
X_open(eventHandler, argc, argv)
int (*eventHandler)();
int argc;
char *argv[];
{
  static int count = 0;
  int i;
  int a_screen;
  int make_square = -1;
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  Colormap  a_cmap;
  struct bu_vls str;
  struct bu_vls top_vls;
  struct bu_vls init_proc_vls;
  Display *tmp_dpy;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_X; /* struct copy */
  dmp->dm_eventHandler = eventHandler;

  /* Only need to do this once for this display manager */
  if(!count){
    bzero((void *)&head_x_vars, sizeof(struct x_vars));
    BU_LIST_INIT( &head_x_vars.l );
  }

  dmp->dm_vars = (genptr_t)bu_calloc(1, sizeof(struct x_vars), "X_open: x_vars");
  if(dmp->dm_vars == (genptr_t)NULL){
    bu_free(dmp, "X_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  i = dm_process_options(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0)
    bu_vls_printf(&dmp->dm_pathName, ".dm_x%d", count);

  ++count;
  if(bu_vls_strlen(&dmp->dm_dName) == 0){
    char *dp;

    dp = getenv("DISPLAY");
    if(dp)
      bu_vls_strcpy(&dmp->dm_dName, dp);
    else
      bu_vls_strcpy(&dmp->dm_dName, ":0.0");
  }
  if(bu_vls_strlen(&init_proc_vls) == 0)
    bu_vls_strcpy(&init_proc_vls, "bind_dm");

  /* initialize dm specific variables */
  ((struct x_vars *)dmp->dm_vars)->perspective_angle = 3;
  dmp->dm_aspect = 1.0;

  /* initialize modifiable variables */
  ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;

#if 0  
  if(BU_LIST_IS_EMPTY(&head_x_vars.l))
#else
  if(dmp->dm_eventHandler != DM_EVENT_HANDLER_NULL)
#endif
    Tk_CreateGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_X);

  BU_LIST_APPEND(&head_x_vars.l, &((struct x_vars *)dmp->dm_vars)->l);

  ((struct x_vars *)dmp->dm_vars)->fontstruct = NULL;

  bu_vls_init(&top_vls);
  if(dmp->dm_top){
    /* Make xtkwin a toplevel window */
    ((struct x_vars *)dmp->dm_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						      bu_vls_addr(&dmp->dm_pathName),
						      bu_vls_addr(&dmp->dm_dName));
    ((struct x_vars *)dmp->dm_vars)->top = ((struct x_vars *)dmp->dm_vars)->xtkwin;
    bu_vls_printf(&top_vls, "%S", &dmp->dm_pathName);
  }else{
    char *cp;

    cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
    if(cp == bu_vls_addr(&dmp->dm_pathName)){
      ((struct x_vars *)dmp->dm_vars)->top = tkwin;
      bu_vls_strcpy(&top_vls, ".");
    }else{
      bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
		    bu_vls_addr(&dmp->dm_pathName));
      ((struct x_vars *)dmp->dm_vars)->top =
	Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
    }

    bu_vls_free(&top_vls);

    /* Make xtkwin an embedded window */
    ((struct x_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindow(interp, ((struct x_vars *)dmp->dm_vars)->top,
		      cp + 1, (char *)NULL);
  }

  if(((struct x_vars *)dmp->dm_vars)->xtkwin == NULL){
    Tcl_AppendResult(interp, "dm-X: Failed to open ",
		     bu_vls_addr(&dmp->dm_pathName),
		     "\n", (char *)NULL);
    (void)X_close(dmp);
    return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct x_vars *)dmp->dm_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S\n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    (void)X_close(dmp);

    return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct x_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct x_vars *)dmp->dm_vars)->top);

  if(dmp->dm_width == 0){
    dmp->dm_width =
      DisplayWidth(((struct x_vars *)dmp->dm_vars)->dpy,
		   DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 20;
    ++make_square;
  }

  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(((struct x_vars *)dmp->dm_vars)->dpy,
		    DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 20;
    ++make_square;
  }

  if(make_square > 0){
    /* Make window square */
    if(dmp->dm_height <
       dmp->dm_width)
      dmp->dm_width = dmp->dm_height;
    else
      dmp->dm_height = dmp->dm_width;
  }

  Tk_GeometryRequest(((struct x_vars *)dmp->dm_vars)->xtkwin,
		     dmp->dm_width, 
		     dmp->dm_height);

#if 0
  /*XXX*/
  XSynchronize(((struct x_vars *)dmp->dm_vars)->dpy, TRUE);
#endif

  a_screen = Tk_ScreenNumber(((struct x_vars *)dmp->dm_vars)->top);

  /* must do this before MakeExist */
  if(X_set_visual(((struct x_vars *)dmp->dm_vars)->dpy,
		  ((struct x_vars *)dmp->dm_vars)->xtkwin,
		  &((struct x_vars *)dmp->dm_vars)->cmap,
		  &((struct x_vars *)dmp->dm_vars)->depth) == 0){
    Tcl_AppendResult(interp, "X_open: Can't get an appropriate visual.\n", (char *)NULL);
    (void)X_close(dmp);
    return DM_NULL;
  }

  Tk_MakeWindowExist(((struct x_vars *)dmp->dm_vars)->xtkwin);
  ((struct x_vars *)dmp->dm_vars)->win =
      Tk_WindowId(((struct x_vars *)dmp->dm_vars)->xtkwin);

  ((struct x_vars *)dmp->dm_vars)->pix =
    Tk_GetPixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		 DefaultRootWindow(((struct x_vars *)dmp->dm_vars)->dpy),
		 dmp->dm_width,
		 dmp->dm_height,
		 Tk_Depth(((struct x_vars *)dmp->dm_vars)->xtkwin));

  dm_allocate_color_cube( ((struct x_vars *)dmp->dm_vars)->dpy,
			  ((struct x_vars *)dmp->dm_vars)->cmap,
			  ((struct x_vars *)dmp->dm_vars)->pixels,
			  /* cube dimension, uses XStoreColor */
			  6, CMAP_BASE, 1 );

  ((struct x_vars *)dmp->dm_vars)->bg = dm_get_pixel(DM_BLACK,
				     ((struct x_vars *)dmp->dm_vars)->pixels,
						     CUBE_DIMENSION);
  ((struct x_vars *)dmp->dm_vars)->fg = dm_get_pixel(DM_RED,
				     ((struct x_vars *)dmp->dm_vars)->pixels,
						     CUBE_DIMENSION);
  
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

  return dmp;
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
  if(((struct x_vars *)dmp->dm_vars)->dpy){
    if(((struct x_vars *)dmp->dm_vars)->gc)
      XFreeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	      ((struct x_vars *)dmp->dm_vars)->gc);

    if(((struct x_vars *)dmp->dm_vars)->pix)
      Tk_FreePixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		    ((struct x_vars *)dmp->dm_vars)->pix);

    /*XXX Possibly need to free the colormap */
    if(((struct x_vars *)dmp->dm_vars)->cmap)
      XFreeColormap(((struct x_vars *)dmp->dm_vars)->dpy,
		    ((struct x_vars *)dmp->dm_vars)->cmap);

    if(((struct x_vars *)dmp->dm_vars)->xtkwin)
      Tk_DestroyWindow(((struct x_vars *)dmp->dm_vars)->xtkwin);

    if(BU_LIST_IS_EMPTY(&head_x_vars.l))
      Tk_DeleteGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_X);
  }

  if(((struct x_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct x_vars *)dmp->dm_vars)->l);

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_free(dmp->dm_vars, "X_close: x_vars");
  bu_free(dmp, "X_close: dmp");
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
		 0, dmp->dm_width + 1,
		 dmp->dm_height + 1);

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
	      0, 0, dmp->dm_width,
	    dmp->dm_height, 0, 0);

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
X_drawVList( dmp, vp, mat )
struct dm *dmp;
register struct rt_vlist *vp;
mat_t mat;
{
    static vect_t spnt, lpnt, pnt;
    register struct rt_vlist *tvp;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    XGCValues gcv;
    int	nseg;			        /* number of segments */

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
		MAT4X3PNT( lpnt, mat, *pt );
		if( lpnt[0] < -1e6 || lpnt[0] > 1e6 ||
		   lpnt[1] < -1e6 || lpnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */
		lpnt[0] *= 2047 * dmp->dm_aspect;
		lpnt[1] *= 2047;
		continue;
	    case RT_VLIST_POLY_DRAW:
	    case RT_VLIST_POLY_END:
	    case RT_VLIST_LINE_DRAW:
		/* draw */
		MAT4X3PNT( pnt, mat, *pt );
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		   pnt[1] < -1e6 || pnt[1] > 1e6 )
		    continue; /* omit this point (ugh) */

		pnt[0] *= 2047 * dmp->dm_aspect;
		pnt[1] *= 2047;

		/* save pnt --- it might get changed by clip() */
		VMOVE(spnt, pnt);

		/* Check to see if lpnt or pnt contain values that exceed
		   the capacity of a short (segbuf is an array of XSegments which
		   contain shorts). If so, do clipping now. Otherwise, let the
		   X server do the clipping */
		if(lpnt[0] < min_short || max_short < lpnt[0] ||
		   lpnt[1] < min_short || max_short < lpnt[1] ||
		   pnt[0] < min_short || max_short < pnt[0] ||
		     pnt[1] < min_short || max_short < pnt[1]){

		  /* if the entire line segment will not be visible then ignore it */
		  if(clip(&lpnt[0], &lpnt[1], &pnt[0], &pnt[1]) == -1){
		    VMOVE(lpnt, spnt);
		    continue;
		  }
		}

		/* convert to X window coordinates */
		segp->x1 = (short)GED_TO_Xx(dmp, lpnt[0]);
		segp->y1 = (short)GED_TO_Xy(dmp, lpnt[1]);
		segp->x2 = (short)GED_TO_Xx(dmp, pnt[0]);
		segp->y2 = (short)GED_TO_Xy(dmp, pnt[1]);

		nseg++;
		segp++;
		VMOVE(lpnt, spnt);

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
int
X_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x, y;
int size;
int use_aspect;
{
  int	sx, sy;

  if(use_aspect)
    sx = GED_TO_Xx(dmp, x * dmp->dm_aspect);
  else
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

  sx1 = GED_TO_Xx(dmp, x1);
  sy1 = GED_TO_Xy(dmp, y1);
  sx2 = GED_TO_Xx(dmp, x2);
  sy2 = GED_TO_Xy(dmp, y2);

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

  gcv.foreground = dm_get_pixel(r, g, b, ((struct x_vars *)dmp->dm_vars)->pixels,
				CUBE_DIMENSION);
  XChangeGC(((struct x_vars *)dmp->dm_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars)->gc,
	    GCForeground, &gcv);

  return TCL_OK;
}

static int
X_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  int linestyle;

  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  if(width <= 1)
    width = 0;

  if(style == DM_DASHED_LINE)
    linestyle = LineOnOffDash;
  else
    linestyle = LineSolid;

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
  dmp->dm_height = xwa.height;
  dmp->dm_width = xwa.width;
  dmp->dm_aspect =
    (fastf_t)dmp->dm_height /
    (fastf_t)dmp->dm_width;

  Tk_FreePixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		((struct x_vars *)dmp->dm_vars)->pix);
  ((struct x_vars *)dmp->dm_vars)->pix =
    Tk_GetPixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		 DefaultRootWindow(((struct x_vars *)dmp->dm_vars)->dpy),
		 dmp->dm_width,
		 dmp->dm_height,
		 Tk_Depth(((struct x_vars *)dmp->dm_vars)->xtkwin));

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

  if (dmp->dm_width < 582) {
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
  } else if (dmp->dm_width < 679) {
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
  } else if (dmp->dm_width < 776) {
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
  } else if (dmp->dm_width < 873) {
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
    bu_vls_printf(&vls, "set perspective %d\n",
		  perspective_table[((struct x_vars *)dmp->dm_vars)->perspective_angle]);

    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct x_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;
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

  vibase = XGetVisualInfo(dpy, 0, &vitemp, &num);

  for (i=0, j=0, vip=vibase; i<num; i++, vip++){
#if 1
    /* requirements */
    if (vip->class != PseudoColor) {
      /* if index mode, accept only read/write*/
      continue;
    }
#endif
			
    /* this visual meets criteria */
    good[j++] = i;
  }

  /* j = number of acceptable visuals under consideration */
  if(j < 1)
    return(0); /* failure */

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

  return(0); /* failure */
}




