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
#include <X11/extensions/XInput.h>
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-X.h"
#include "solid.h"

void     X_configureWindowShape();

static void	label();
static void	draw();
static void     x_var_init();
static int X_set_visual();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*X_open();
static int	X_close();
static int	X_drawBegin(), X_drawEnd();
static int	X_normal(), X_loadMatrix();
static int      X_drawString2D();
static int	X_drawLine2D();
static int      X_drawPoint2D();
static int	X_drawVList();
static int      X_setColor(), X_setLineAttr();
static int	X_setWinBounds(), X_debug();

struct dm dm_X = {
  X_close,
  X_drawBegin,
  X_drawEnd,
  X_normal,
  X_loadMatrix,
  X_drawString2D,
  X_drawLine2D,
  X_drawPoint2D,
  X_drawVList,
  X_setColor,
  X_setLineAttr,
  X_setWinBounds,
  X_debug,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,
  0,				/* no displaylist */
  0,                            /* no stereo */
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
  0
};

fastf_t min_short = (fastf_t)SHRT_MIN;
fastf_t max_short = (fastf_t)SHRT_MAX;

/* Currently, the application must define these. */
extern Tk_Window tkwin;

struct x_vars head_x_vars;
static mat_t xmat;

/*
 *			X _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
X_open(argc, argv)
int argc;
char *argv[];
{
  static int count = 0;
  int i, j, k;
  int a_screen;
  int make_square = -1;
  XGCValues gcv;
  XColor a_color;
  Visual *a_visual;
  Colormap  a_cmap;
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  struct bu_vls str;
  struct bu_vls top_vls;
  struct bu_vls init_proc_vls;
  Display *tmp_dpy;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_X; /* struct copy */

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

  i = dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0)
    bu_vls_printf(&dmp->dm_pathName, ".dm_X%d", count);

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
  ((struct x_vars *)dmp->dm_vars)->devmotionnotify = LASTEvent;
  ((struct x_vars *)dmp->dm_vars)->devbuttonpress = LASTEvent;
  ((struct x_vars *)dmp->dm_vars)->devbuttonrelease = LASTEvent;
  dmp->dm_aspect = 1.0;

  /* initialize modifiable variables */
  ((struct x_vars *)dmp->dm_vars)->mvars.zclip = 1;

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
		   DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 30;
    ++make_square;
  }

  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(((struct x_vars *)dmp->dm_vars)->dpy,
		    DefaultScreen(((struct x_vars *)dmp->dm_vars)->dpy)) - 30;
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
  /*XXX For debugging purposes */
  XSynchronize(((struct x_vars *)dmp->dm_vars)->dpy, TRUE);
#endif

  a_screen = Tk_ScreenNumber(((struct x_vars *)dmp->dm_vars)->top);

  /* must do this before MakeExist */
  if(X_set_visual(dmp) == 0){
    Tcl_AppendResult(interp, "X_open: Can't get an appropriate visual.\n", (char *)NULL);
    (void)X_close(dmp);
    return DM_NULL;
  }

  Tk_MakeWindowExist(((struct x_vars *)dmp->dm_vars)->xtkwin);
  ((struct x_vars *)dmp->dm_vars)->win =
      Tk_WindowId(((struct x_vars *)dmp->dm_vars)->xtkwin);
  dmp->dm_id = ((struct x_vars *)dmp->dm_vars)->win;

  ((struct x_vars *)dmp->dm_vars)->pix =
    Tk_GetPixmap(((struct x_vars *)dmp->dm_vars)->dpy,
		 DefaultRootWindow(((struct x_vars *)dmp->dm_vars)->dpy),
		 dmp->dm_width,
		 dmp->dm_height,
		 Tk_Depth(((struct x_vars *)dmp->dm_vars)->xtkwin));

  if(((struct x_vars *)dmp->dm_vars)->is_trueColor){
    XColor fg, bg;

    fg.red = 255 << 8;
    fg.green = fg.blue = 0;
    XAllocColor(((struct x_vars *)dmp->dm_vars)->dpy,
		((struct x_vars *)dmp->dm_vars)->cmap,
		&fg);
    ((struct x_vars *)dmp->dm_vars)->fg = fg.pixel;

    bg.red = bg.green = bg.blue = 0;
    XAllocColor(((struct x_vars *)dmp->dm_vars)->dpy,
		((struct x_vars *)dmp->dm_vars)->cmap,
		&bg);
    ((struct x_vars *)dmp->dm_vars)->bg = bg.pixel;
  }else{
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
  }  

  gcv.background = ((struct x_vars *)dmp->dm_vars)->bg;
  gcv.foreground = ((struct x_vars *)dmp->dm_vars)->fg;
  ((struct x_vars *)dmp->dm_vars)->gc = XCreateGC(((struct x_vars *)dmp->dm_vars)->dpy,
						  ((struct x_vars *)dmp->dm_vars)->win,
						  (GCForeground|GCBackground), &gcv);

  /* First see if the server supports XInputExtension */
  {
    int return_val;

    if(!XQueryExtension(((struct x_vars *)dmp->dm_vars)->dpy,
		     "XInputExtension", &return_val, &return_val, &return_val))
      goto Skip_dials;
  }

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list =
    (XDeviceInfoPtr)XListInputDevices(((struct x_vars *)dmp->dm_vars)->dpy,
				      &ndevices);

  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct x_vars *)dmp->dm_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp,
			   "X_open: Couldn't open the dials+buttons\n",
			   (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#if IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, ((struct x_vars *)dmp->dm_vars)->devbuttonpress,
			      e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, ((struct x_vars *)dmp->dm_vars)->devbuttonrelease,
				e_class[nclass]);
	    ++nclass;
	    break;
#endif
#if IR_KNOBS
	  case ValuatorClass:
	    DeviceMotionNotify(dev, ((struct x_vars *)dmp->dm_vars)->devmotionnotify,
			       e_class[nclass]);
	    ++nclass;
	    break;
#endif
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(((struct x_vars *)dmp->dm_vars)->dpy,
			      ((struct x_vars *)dmp->dm_vars)->win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);

Skip_dials:
#ifndef CRAY2
  X_configureWindowShape(dmp);
#endif

  Tk_SetWindowBackground(((struct x_vars *)dmp->dm_vars)->xtkwin,
			 ((struct x_vars *)dmp->dm_vars)->bg);
  Tk_MapWindow(((struct x_vars *)dmp->dm_vars)->xtkwin);

  bn_mat_idn(xmat);

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

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_drawBegin()\n", (char *)NULL);

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
  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_drawEnd()\n", (char *)NULL);

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
 *  			X _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to X_drawVList().
 */
/* ARGSUSED */
static int
X_loadMatrix(dmp, mat, which_eye)
struct dm *dmp;
mat_t mat;
int which_eye;
{
  if(((struct x_vars *)dmp->dm_vars)->mvars.debug){
    struct bu_vls tmp_vls;

    Tcl_AppendResult(interp, "X_loadMatrix()\n", (char *)NULL);

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
    bu_vls_printf(&tmp_vls, "transformation matrix = \n");
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8],mat[12]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9],mat[13]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10],mat[14]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11],mat[15]);

    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  bn_mat_copy(xmat, mat);
  return TCL_OK;
}

/*
 *  			X _ D R A W V L I S T
 *  
 */

/* ARGSUSED */
static int
X_drawVList( dmp, vp )
struct dm *dmp;
register struct rt_vlist *vp;
{
    static vect_t spnt, lpnt, pnt;
    register struct rt_vlist *tvp;
    XSegment segbuf[1024];		/* XDrawSegments list */
    XSegment *segp;			/* current segment */
    XGCValues gcv;
    int	nseg;			        /* number of segments */

    if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
      Tcl_AppendResult(interp, "X_drawVList()\n", (char *)NULL);

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
		MAT4X3PNT( lpnt, xmat, *pt );
#if 0
		if( lpnt[0] < -1e6 || lpnt[0] > 1e6 ||
		    lpnt[1] < -1e6 || lpnt[1] > 1e6 )
		  continue; /* omit this point (ugh) */
#endif

		lpnt[0] *= 2047 * dmp->dm_aspect;
		lpnt[1] *= 2047;
		lpnt[2] *= 2047;
		continue;
	    case RT_VLIST_POLY_DRAW:
	    case RT_VLIST_POLY_END:
	    case RT_VLIST_LINE_DRAW:
		/* draw */
		MAT4X3PNT( pnt, xmat, *pt );
#if 0
		if( pnt[0] < -1e6 || pnt[0] > 1e6 ||
		    pnt[1] < -1e6 || pnt[1] > 1e6 )
		  continue; /* omit this point (ugh) */
#endif

		pnt[0] *= 2047 * dmp->dm_aspect;
		pnt[1] *= 2047;
		pnt[2] *= 2047;

		/* save pnt --- it might get changed by clip() */
		VMOVE(spnt, pnt);

		if(((struct x_vars *)dmp->dm_vars)->mvars.zclip){
		  if(vclip(lpnt, pnt,
			   ((struct x_vars *)dmp->dm_vars)->clipmin,
			   ((struct x_vars *)dmp->dm_vars)->clipmax) == 0){
		    VMOVE(lpnt, spnt);
		    continue;
		  }
		}else{
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
 */
static int
X_normal(dmp)
struct dm *dmp;
{
  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_normal()\n", (char *)NULL);

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
X_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x, y;
int size;
int use_aspect;
{
  int	sx, sy;

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_drawString2D()\n", (char *)NULL);

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

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_drawLine2D()\n", (char *)NULL);

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
X_drawPoint2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  int   sx, sy;

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_drawPoint2D()\n", (char *)NULL);

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

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_setColor()\n", (char *)NULL);

  if(((struct x_vars *)dmp->dm_vars)->is_trueColor){
    XColor color;

    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;
    XAllocColor(((struct x_vars *)dmp->dm_vars)->dpy,
		((struct x_vars *)dmp->dm_vars)->cmap,
		&color);
    gcv.foreground = color.pixel;
  }else
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

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_setLineAttr()\n", (char *)NULL);

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
static int
X_debug(dmp, lvl)
struct dm *dmp;
int lvl;
{
  ((struct x_vars *)dmp->dm_vars)->mvars.debug = lvl;
  XFlush(((struct x_vars *)dmp->dm_vars)->dpy);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);

  return TCL_OK;
}

static int
X_setWinBounds(dmp, w)
struct dm *dmp;
register int w[6];
{
  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_setWinBounds()\n", (char *)NULL);

  ((struct x_vars *)dmp->dm_vars)->clipmin[0] = w[0];
  ((struct x_vars *)dmp->dm_vars)->clipmin[1] = w[2];
  ((struct x_vars *)dmp->dm_vars)->clipmin[2] = w[4];
  ((struct x_vars *)dmp->dm_vars)->clipmax[0] = w[1];
  ((struct x_vars *)dmp->dm_vars)->clipmax[1] = w[3];
  ((struct x_vars *)dmp->dm_vars)->clipmax[2] = w[5];

  return TCL_OK;
}

void
X_configureWindowShape(dmp)
struct dm *dmp;
{
  XWindowAttributes xwa;
  XFontStruct     *newfontstruct;
  XGCValues       gcv;

  if (((struct x_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "X_configureWindowShape()\n", (char *)NULL);

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

static int
X_set_visual(dmp)
struct dm *dmp;
{
  XVisualInfo *vip, vitemp, *vibase, *maxvip;
  int good[40];
  int num, i, j;
  int tries, baddepth;
  int desire_trueColor = 1;

  vibase = XGetVisualInfo(((struct x_vars *)dmp->dm_vars)->dpy, 0, &vitemp, &num);

  while(1){
    for (i=0, j=0, vip=vibase; i<num; i++, vip++){
      /* requirements */
      if(desire_trueColor && vip->class != TrueColor)
	continue;
      else if (vip->class != PseudoColor)
	continue;
			
      /* this visual meets criteria */
      good[j++] = i;
    }

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
      if(desire_trueColor){
	((struct x_vars *)dmp->dm_vars)->cmap = XCreateColormap(((struct x_vars *)dmp->dm_vars)->dpy, RootWindow(((struct x_vars *)dmp->dm_vars)->dpy, maxvip->screen),
								maxvip->visual, AllocNone);
	((struct x_vars *)dmp->dm_vars)->is_trueColor = 1;
      }else{
	((struct x_vars *)dmp->dm_vars)->cmap = XCreateColormap(((struct x_vars *)dmp->dm_vars)->dpy, RootWindow(((struct x_vars *)dmp->dm_vars)->dpy, maxvip->screen),
								maxvip->visual, AllocAll);
	((struct x_vars *)dmp->dm_vars)->is_trueColor = 0;
      }

      if (Tk_SetWindowVisual(((struct x_vars *)dmp->dm_vars)->xtkwin, maxvip->visual,
			     maxvip->depth, ((struct x_vars *)dmp->dm_vars)->cmap)){
	((struct x_vars *)dmp->dm_vars)->depth = maxvip->depth;
	return 1; /* success */
      } else { 
	/* retry with lesser depth */
	baddepth = maxvip->depth;
	XFreeColormap(((struct x_vars *)dmp->dm_vars)->dpy, ((struct x_vars *)dmp->dm_vars)->cmap);
      }
    }

    if(desire_trueColor)
      desire_trueColor = 0;
    else
      return(0); /* failure */
  }
}




