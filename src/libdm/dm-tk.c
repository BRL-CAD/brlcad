/*                          D M - T K . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-tk.c
 *
 *  A Display Manager that should work wherever tk does.
 *
 *  Author -
 *	Phillip Dykstra
 *	Robert G. Parker
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

/* Even on a platform that has no real X, I should be able to use the
 * Xutil that comes with Tk
 */
#include <tk.h>
#include <X11/Xutil.h>
#include <X11/X.h>

#define USE_DIALS_AND_BUTTONS 0

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif
#if USE_DIALS_AND_BUTTONS
#  include <X11/extensions/XInput.h>
#endif
#if defined(linux)
#  undef   X_NOT_STDC_ENV
#  undef   X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-Tk.h"
#include "dm-X.h"
#include "dm_xvars.h"
#include "solid.h"


struct dm	*tk_open(Tcl_Interp *interp, int argc, char **argv);

HIDDEN void	label();
HIDDEN void	draw();
HIDDEN void     x_var_init();

/* Display Manager package interface */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
HIDDEN int	Tk_close(struct dm *dmp);
HIDDEN int	Tk_drawBegin(struct dm *dmp), Tk_drawEnd(struct dm *dmp);
HIDDEN int	Tk_normal(struct dm *dmp), Tk_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int      Tk_drawString2D(struct dm *dmp, register char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int	Tk_drawLine2D(struct dm *dmp, fastf_t x1, fastf_t y1, fastf_t x2, fastf_t y2);
HIDDEN int      Tk_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int	Tk_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int      Tk_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int	Tk_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int	Tk_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int	Tk_configureWin_guts(struct dm *dmp, int force);
HIDDEN int	Tk_configureWin(struct dm *dmp);
HIDDEN int	Tk_setLight(struct dm *dmp, int light_on);
HIDDEN int	Tk_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int	Tk_setWinBounds(struct dm *dmp, register int *w), Tk_debug(struct dm *dmp, int lvl);

struct dm dm_Tk = {
  Tk_close,
  Tk_drawBegin,
  Tk_drawEnd,
  Tk_normal,
  Tk_loadMatrix,
  Tk_drawString2D,
  Tk_drawLine2D,
  Tk_drawPoint2D,
  Tk_drawVList,
  Tk_setFGColor,
  Tk_setBGColor,
  Tk_setLineAttr,
  Tk_configureWin,
  Tk_setWinBounds,
  Tk_setLight,
  Nu_int0,
  Nu_int0,
  Tk_setZBuffer,
  Tk_debug,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,
  0,				/* no displaylist */
  0,                            /* no stereo */
  PLOTBOUND,			/* zoom-in limit */
  1,				/* bound flag */
  "Tk",
  "Tk Abstraction Layer",
  DM_TYPE_TK,
  1,
  0,
  0,
  0,
  0,
  1.0, /* aspect ratio */
  0,
  {0, 0},
  {0, 0, 0, 0, 0},		/* bu_vls path name*/
  {0, 0, 0, 0, 0},		/* bu_vls full name drawing window */
  {0, 0, 0, 0, 0},		/* bu_vls short name drawing window */
  {0, 0, 0},			/* bg color */
  {0, 0, 0},			/* fg color */
  {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
  {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
  0,				/* no debugging */
  0,				/* no perspective */
  0,				/* no lighting */
  0,				/* no transparency */
  0,				/* depth buffer is not writable */
  0,				/* no zbuffer */
  0,				/* no zclipping */
  1,                            /* clear back buffer after drawing and swap */
  0				/* Tcl interpreter */
};

static fastf_t min_short = (fastf_t)SHRT_MIN;
static fastf_t max_short = (fastf_t)SHRT_MAX;

extern int vectorThreshold;	/* defined in libdm/tcl.c */ 

static void 
get_color(Display *dpy, Colormap cmap, XColor *color)
{
	Status st;
	XColor rgb;

#define CSCK   0xf800

	rgb = *color;

	st = XAllocColor(dpy, cmap, color);
	switch (st) {
	case 1: 
#if 0
		if ( (color->red & CSCK) != (rgb.red & CSCK) ||
		     (color->green & CSCK) != (rgb.green & CSCK) ||
		     (color->blue & CSCK) != (rgb.blue & CSCK) ) {
			bu_log("\nlooking for fg    (%3d,%3d,%3d) %04x,%04x,%04x got \n                  (%3d,%3d,%3d) %04x,%04x,%04x\n",
			       (rgb.red >> 8), (rgb.green >> 8), (rgb.blue >> 8),
			       rgb.red, rgb.green, rgb.blue,

			       (color->red >> 8), (color->green >> 8), (color->blue >> 8),
			       color->red, color->green, color->blue);
		}
#endif
		break;
	case BadColor:
		bu_log("XAllocColor failed (BadColor) for (%3d,%3d,%3d) %04x,%04x,%04x\n", 
		       (rgb.red >> 8), (rgb.green >> 8), (rgb.blue >> 8),
		       rgb.red, rgb.green, rgb.blue);
		break;

	default:
		bu_log("XAllocColor error for (%3d,%3d,%3d) %04x,%04x,%04x\n", 
		       (rgb.red >> 8), (rgb.green >> 8), (rgb.blue >> 8),
		       rgb.red, rgb.green, rgb.blue);
		break;
	}
#undef CSCK

}

/*
 *			T k _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
tk_open(Tcl_Interp *interp, int argc, char **argv)
{
  static int count = 0;
  int make_square = -1;
  XGCValues gcv;

  struct bu_vls str;
  struct bu_vls init_proc_vls;
  struct dm *dmp;
  Tk_Window tkwin;
  Display *dpy;

  if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	  return DM_NULL;
  }

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_Tk; /* struct copy */
  dmp->dm_interp = interp;

  dmp->dm_vars.pub_vars = (genptr_t)bu_calloc(1, sizeof(struct dm_xvars), "tk_open: dm_xvars");
  if(dmp->dm_vars.pub_vars == (genptr_t)NULL){
    bu_free(dmp, "tk_open: dmp");
    return DM_NULL;
  }

  dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct Tk_vars), "tk_open: Tk_vars");
  if(dmp->dm_vars.priv_vars == (genptr_t)NULL){
    bu_free(dmp->dm_vars.pub_vars, "tk_open: dmp->dm_vars.pub_vars");
    bu_free(dmp, "tk_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0) {
      bu_vls_printf(&dmp->dm_pathName, ".dm_Tk%d", count);
  }

  ++count;
  if(bu_vls_strlen(&dmp->dm_dName) == 0){
    char *dp;

    dp = DisplayString(Tk_Display(tkwin));

    if(dp)
      bu_vls_strcpy(&dmp->dm_dName, dp);
    else
      bu_vls_strcpy(&dmp->dm_dName, ":0.0");
  }
  if(bu_vls_strlen(&init_proc_vls) == 0)
    bu_vls_strcpy(&init_proc_vls, "bind_dm");

  /* initialize dm specific variables */
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devmotionnotify = LASTEvent;
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress = LASTEvent;
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease = LASTEvent;
  dmp->dm_aspect = 1.0;

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct = NULL;

  if(dmp->dm_top){
    /* Make xtkwin a toplevel window */
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin = Tk_CreateWindowFromPath(interp, tkwin,
						      bu_vls_addr(&dmp->dm_pathName),
						      bu_vls_addr(&dmp->dm_dName));
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin;
  }else{
    char *cp;

    cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
    if(cp == bu_vls_addr(&dmp->dm_pathName)){
      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = tkwin;
    }else{
      struct bu_vls top_vls;

      bu_vls_init(&top_vls);
      bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
		    bu_vls_addr(&dmp->dm_pathName));
      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top =
	Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
      bu_vls_free(&top_vls);
    }

    /* Make xtkwin an embedded window */
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin =
      Tk_CreateWindow(interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top,
		      cp + 1, (char *)NULL);
  }

  if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin == NULL){
    bu_log("tk_open: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
    (void)Tk_close(dmp);
    return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S\n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    (void)Tk_close(dmp);

    return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy =
    Tk_Display(((struct dm_xvars *)dmp->dm_vars.pub_vars)->top);
  dpy = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy;

  /* make sure there really is a display before proceeding. */
  if (!dpy || !Tk_IsMapped(dpy)) {
      (void)Tk_close(dmp);
      return DM_NULL;
  }

  if(dmp->dm_width == 0){
    dmp->dm_width =
      WidthOfScreen(Tk_Screen((
        (struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)) - 30;
    ++make_square;
  }

  if(dmp->dm_height == 0){
    dmp->dm_height =
      HeightOfScreen(Tk_Screen((
        (struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)) - 30;
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

  Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
		     dmp->dm_width, 
		     dmp->dm_height);

#if 0
  /*XXX For debugging purposes */
  XSynchronize(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, 1);

  /* must do this before MakeExist */
  if((((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip = Tk_choose_visual(dmp)) == NULL){
    bu_log("Tk_open: Can't get an appropriate visual.\n");
    (void)Tk_close(dmp);
    return DM_NULL;
  }

#endif

  Tk_MakeWindowExist(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win =
      Tk_WindowId(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  dmp->dm_id = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win;

  ((struct x_vars *)dmp->dm_vars.priv_vars)->pix =
    Tk_GetPixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		 DefaultRootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy),
		 dmp->dm_width,
		 dmp->dm_height,
		 Tk_Depth(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

    XColor fg, bg;

    fg.red = 65535;
    fg.green = fg.blue = 0;

    ((struct x_vars *)dmp->dm_vars.priv_vars)->fg = 
	Tk_GetColorByValue(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
        &fg)->pixel;

    bg.red = bg.green = bg.blue = 3277;

    ((struct x_vars *)dmp->dm_vars.priv_vars)->bg =
	Tk_GetColorByValue(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
        &bg)->pixel;

  gcv.background = ((struct x_vars *)dmp->dm_vars.priv_vars)->bg;
  gcv.foreground = ((struct x_vars *)dmp->dm_vars.priv_vars)->fg;

  ((struct x_vars *)dmp->dm_vars.priv_vars)->gc =
    Tk_GetGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, 
    (GCForeground|GCBackground), &gcv);


#if HAVE_X
#ifdef HAVE_XQUERYEXTENSION
  /* First see if the server supports XInputExtension */
  {
    int return_val;

    if(!XQueryExtension(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		     "XInputExtension", &return_val, &return_val, &return_val))
      goto Skip_dials;
  }
#endif

#if USE_DIALS_AND_BUTTONS
  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  if (XQueryExtension(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, "XInputExtension", &unused, &unused, &unused)) {
      olist = list = (XDeviceInfoPtr)XListInputDevices(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, &ndevices);
  }

  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  bu_log("Tk_open: Couldn't open the dials+buttons\n");
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#if IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress,
			      e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease,
				e_class[nclass]);
	    ++nclass;
	    break;
#endif
#if IR_KNOBS
	  case ValuatorClass:
	    DeviceMotionNotify(dev, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devmotionnotify,
			       e_class[nclass]);
	    ++nclass;
	    break;
#endif /*USE_DIALS_AND_BUTTONS*/
#endif /*HAVE_X*/
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);
#endif

Skip_dials:
#ifndef CRAY2
  (void)Tk_configureWin_guts(dmp, 1);
#endif
/*
  Tk_SetWindowBackground(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
			 ((struct x_vars *)dmp->dm_vars.priv_vars)->bg);
*/
  Tk_MapWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

  MAT_IDN(((struct x_vars *)dmp->dm_vars.priv_vars)->xmat);

  return dmp;
}

/**
 *  @proc Tk_close
 *  
 *  Gracefully release the display.
 */
HIDDEN int
Tk_close(struct dm *dmp)
{
  if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy){
    if(((struct x_vars *)dmp->dm_vars.priv_vars)->gc)
      Tk_FreeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct x_vars *)dmp->dm_vars.priv_vars)->gc);

    if(((struct x_vars *)dmp->dm_vars.priv_vars)->pix)
      Tk_FreePixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		    ((struct x_vars *)dmp->dm_vars.priv_vars)->pix);

    /*XXX Possibly need to free the colormap */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)
      XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);

    if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
      Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

  }

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_free(dmp->dm_vars.priv_vars, "Tk_close: Tk_vars");
  bu_free(dmp->dm_vars.pub_vars, "Tk_close: dm_Tkvars");
  bu_free(dmp, "Tk_close: dmp");

  return TCL_OK;
}

/**
 * @proc Tk_drawBegin
 * This white-washes the dm's pixmap with the background color. 
 */
HIDDEN int
Tk_drawBegin(struct dm *dmp)
{
  XGCValues       gcv;

  if (dmp->dm_debugLevel)
    bu_log("Tk_drawBegin()\n");

  /* clear pixmap */
  gcv.foreground = ((struct x_vars *)dmp->dm_vars.priv_vars)->bg;
  XChangeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	    GCForeground, &gcv);
  XFillRectangle(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		 ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
		 ((struct x_vars *)dmp->dm_vars.priv_vars)->gc, 0,
		 0, dmp->dm_width + 1,
		 dmp->dm_height + 1);

  /* reset foreground */

    gcv.foreground = ((struct x_vars *)dmp->dm_vars.priv_vars)->fg;
    XChangeGC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	GCForeground, &gcv);

  return TCL_OK;
}

/**
 *  Tk_drawEnd
 *  This copies the pixmap into the window.
 */
HIDDEN int
Tk_drawEnd(struct dm *dmp)
{
  if (dmp->dm_debugLevel)
    bu_log("Tk_drawEnd()\n");

  XCopyArea(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	    ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
	    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
	    ((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	      0, 0, dmp->dm_width,
 	    dmp->dm_height, 0, 0);


  /* Prevent lag between events and updates */
  XSync(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, 0);

  return TCL_OK;
}

/**
 * @proc Tk_loadMatrix
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to Tk_drawVList().
 */
/* ARGSUSED */
HIDDEN int
Tk_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
  if(dmp->dm_debugLevel){
    bu_log("Tk_loadMatrix()\n");

    bu_log("which eye = %d\t", which_eye);
    bu_log("transformation matrix = \n");
#if 1
    bu_log("%g %g %g %g\n", mat[0], mat[1], mat[2], mat[3]);
    bu_log("%g %g %g %g\n", mat[4], mat[5], mat[6], mat[7]);
    bu_log("%g %g %g %g\n", mat[8], mat[9], mat[10], mat[11]);
    bu_log("%g %g %g %g\n", mat[12], mat[13], mat[14], mat[15]);
#else
    bu_log("%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_log("%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_log("%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_log("%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
#endif
  }

  MAT_COPY(((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, mat);
  return TCL_OK;
}

/**
 * Tk_drawVList
 *  
 */

HIDDEN int
Tk_drawVList(struct dm *dmp, register struct bn_vlist *vp)
{
#if 1
	static vect_t			spnt, lpnt, pnt;
	register struct rt_vlist	*tvp;
	XSegment			segbuf[1024];		/* XDrawSegments list */
	XSegment			*segp;			/* current segment */
	int				nseg;		        /* number of segments */
	fastf_t				delta;
	register point_t		*pt_prev = NULL;
	fastf_t				dist_prev=1.0;
	static int			nvectors = 0;

	if (dmp->dm_debugLevel) {
		bu_log("Tk_drawVList()\n");
		bu_log("vp - %lu, perspective - %d\n", vp, dmp->dm_perspective);
	}

	/* delta is used in clipping to insure clipped endpoint is slightly
	 * in front of eye plane (perspective mode only).
	 * This value is a SWAG that seems to work OK.
	 */
	delta = ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat[15]*0.0001;
	if (delta < 0.0)
		delta = -delta;
	if (delta < SQRT_SMALL_FASTF)
		delta = SQRT_SMALL_FASTF;

	nseg = 0;
	segp = segbuf;
	for (BU_LIST_FOR(tvp, rt_vlist, &vp->l)) {
		register int	i;
		register int	nused = tvp->nused;
		register int	*cmd = tvp->cmd;
		register point_t *pt = tvp->pt;
		fastf_t 	 dist;

		/* Viewing region is from -1.0 to +1.0 */
		/* 2^31 ~= 2e9 -- dynamic range of a long int */
		/* 2^(31-11) = 2^20 ~= 1e6 */
		/* Integerize and let the X server do the clipping */
		for (i = 0; i < nused; i++,cmd++,pt++) {
			switch (*cmd) {
			case RT_VLIST_POLY_START:

			case RT_VLIST_POLY_VERTNORM:
				continue;
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_LINE_MOVE:
				/* Move, not draw */
				if (dmp->dm_debugLevel > 2) {
					bu_log("before transformation:\n");
					bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
				}

				if (dmp->dm_perspective > 0) {
					/* cannot apply perspective transformation to
					 * points behind eye plane!!!!
					 */
					dist = VDOT(*pt, &((struct x_vars *)dmp->dm_vars.priv_vars)->xmat[12]) + ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat[15];
					if (dist <= 0.0) {
						pt_prev = pt;
						dist_prev = dist;
						continue;
					} else {
						MAT4X3PNT(lpnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, *pt);
						dist_prev = dist;
						pt_prev = pt;
					}
				} else {
					MAT4X3PNT(lpnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, *pt);
				}

				lpnt[0] *= 2047;
				lpnt[1] *= 2047 * dmp->dm_aspect;
				lpnt[2] *= 2047;
				continue;
			case RT_VLIST_POLY_DRAW:
			case RT_VLIST_POLY_END:
			case RT_VLIST_LINE_DRAW:
				/* draw */
				if (dmp->dm_debugLevel > 2) {
					bu_log("before transformation:\n");
					bu_log("pt - %lf %lf %lf\n", V3ARGS(*pt));
				}

				if (dmp->dm_perspective > 0) {
					/* cannot apply perspective transformation to
					 * points behind eye plane!!!!
					 */
					dist = VDOT( *pt, &((struct x_vars *)dmp->dm_vars.priv_vars)->xmat[12] ) + ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat[15];
					if (dmp->dm_debugLevel > 2)
						bu_log( "dist=%g, dist_prev=%g\n", dist, dist_prev );
					if (dist <= 0.0) {
						if (dist_prev <= 0.0) {
							/* nothing to plot */
							dist_prev = dist;
							pt_prev = pt;
							continue;
						} else {
							fastf_t alpha;
							vect_t diff;
							point_t tmp_pt;

							/* clip this end */
							VSUB2(diff, *pt, *pt_prev);
							alpha = (dist_prev - delta) / (dist_prev - dist);
							VJOIN1(tmp_pt, *pt_prev, alpha, diff);
							MAT4X3PNT(pnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, tmp_pt);
						}
					} else {
						if (dist_prev <= 0.0) {
							fastf_t alpha;
							vect_t diff;
							point_t tmp_pt;

							/* clip other end */
							VSUB2(diff, *pt, *pt_prev);
							alpha = (-dist_prev + delta) / (dist - dist_prev);
							VJOIN1(tmp_pt, *pt_prev, alpha, diff);
							MAT4X3PNT(lpnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, tmp_pt);
							lpnt[0] *= 2047;
							lpnt[1] *= 2047 * dmp->dm_aspect;
							lpnt[2] *= 2047;
							MAT4X3PNT(pnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, *pt);
						} else {
							MAT4X3PNT(pnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, *pt);
						}
					}
					dist_prev = dist;
				} else {
					MAT4X3PNT(pnt, ((struct x_vars *)dmp->dm_vars.priv_vars)->xmat, *pt);
				}

				pnt[0] *= 2047;
				pnt[1] *= 2047 * dmp->dm_aspect;
				pnt[2] *= 2047;

				/* save pnt --- it might get changed by clip() */
				VMOVE(spnt, pnt);
				pt_prev = pt;

				if (dmp->dm_debugLevel > 2) {
					bu_log("before clipping:\n");
					bu_log("clipmin - %lf %lf %lf\n",
					       dmp->dm_clipmin[X],
					       dmp->dm_clipmin[Y],
					       dmp->dm_clipmin[Z]);
					bu_log("clipmax - %lf %lf %lf\n",
					       dmp->dm_clipmax[X],
					       dmp->dm_clipmax[Y],
					       dmp->dm_clipmax[Z]);
					bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
					bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
				}

				if (dmp->dm_zclip) {
					if (vclip(lpnt, pnt,
						  dmp->dm_clipmin,
						  dmp->dm_clipmax) == 0) {
						VMOVE(lpnt, spnt);
						continue;
					}
				} else {
					/* Check to see if lpnt or pnt contain values that exceed
					   the capacity of a short (segbuf is an array of XSegments which
					   contain shorts). If so, do clipping now. Otherwise, let the
					   X server do the clipping */
					if (lpnt[0] < min_short || max_short < lpnt[0] ||
					    lpnt[1] < min_short || max_short < lpnt[1] ||
					    pnt[0] < min_short || max_short < pnt[0] ||
					    pnt[1] < min_short || max_short < pnt[1]) {
						/* if the entire line segment will not be visible then ignore it */
						if (clip(&lpnt[0], &lpnt[1], &pnt[0], &pnt[1]) == -1) {
							VMOVE(lpnt, spnt);
							continue;
						}
					}
				}

				if (dmp->dm_debugLevel > 2) {
					bu_log("after clipping:\n");
					bu_log("pt1 - %lf %lf %lf\n", lpnt[X], lpnt[Y], lpnt[Z]);
					bu_log("pt2 - %lf %lf %lf\n", pnt[X], pnt[Y], pnt[Z]);
				}

				/* convert to X window coordinates */
				segp->x1 = (short)GED_TO_Xx(dmp, lpnt[0]);
				segp->y1 = (short)GED_TO_Xy(dmp, lpnt[1]);
				segp->x2 = (short)GED_TO_Xx(dmp, pnt[0]);
				segp->y2 = (short)GED_TO_Xy(dmp, pnt[1]);

				nseg++;
				segp++;
				VMOVE(lpnt, spnt);

				if (nseg == 1024) {
					XDrawSegments(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						      ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
						      ((struct x_vars *)dmp->dm_vars.priv_vars)->gc, segbuf, nseg);

					nseg = 0;
					segp = segbuf;
				}
				break;
			}
		}

		nvectors += nused;
		if (nvectors >= vectorThreshold) {
			if (dmp->dm_debugLevel)
				bu_log("Tk_drawVList(): handle Tcl events\n");

			nvectors = 0;

			/* Handle events in the queue */
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
		}
	}

	if (nseg) {
		XDrawSegments( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			       ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
			       ((struct x_vars *)dmp->dm_vars.priv_vars)->gc, segbuf, nseg );
	}

#endif
	return TCL_OK;
}

/*
 *			X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
HIDDEN int
Tk_normal(struct dm *dmp)
{
  if (dmp->dm_debugLevel)
    bu_log("Tk_normal()\n");

  return TCL_OK;
}

/*
 *			X _ D R A W S T R I N G 2 D
 *
 * Output a string into the displaylist.
 * The starting position of the beam is as specified.
 */
/* ARGSUSED */
HIDDEN int
Tk_drawString2D(struct dm *dmp, register char *str, fastf_t x, fastf_t y, int size, int use_aspect)
{
  int sx, sy;

  if (dmp->dm_debugLevel){
    bu_log("Tk_drawString2D():\n");
    bu_log("\tstr - %s\n", str);
    bu_log("\tx - %g\n", x);
    bu_log("\ty - %g\n", y);
    bu_log("\tsize - %d\n", size);

  bu_log("color = %d\n", ((struct x_vars *)dmp->dm_vars.priv_vars)->fg);
  /*  bu_log("real_color = %d\n", ((struct x_vars *)dmp->dm_vars.priv_vars)->gc->foreground); */

    if(use_aspect){
      bu_log("\tuse_aspect - %d\t\taspect ratio - %g\n", use_aspect, dmp->dm_aspect);
    }else
      bu_log("\tuse_aspect - 0");
  }

  sx = dm_Normal2Xx(dmp, x);
  sy = dm_Normal2Xy(dmp, y, use_aspect);


  Tk_DrawChars( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	       ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
	       ((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct,
	       str, strlen(str), sx, sy);

  return TCL_OK;
}

HIDDEN int
Tk_drawLine2D(struct dm *dmp, fastf_t x1, fastf_t y1, fastf_t x2, fastf_t y2)
{
  int	sx1, sy1, sx2, sy2;

  sx1 = dm_Normal2Xx(dmp, x1);
  sx2 = dm_Normal2Xx(dmp, x2);
  sy1 = dm_Normal2Xy(dmp, y1, 0);
  sy2 = dm_Normal2Xy(dmp, y2, 0);

  if (dmp->dm_debugLevel) {
    bu_log("Tk_drawLine2D()\n");
    bu_log("x1 = %g, y1 = %g\n", x1, y1);
    bu_log("x2 = %g, y2 = %g\n", x2, y2);
    bu_log("sx1 = %d, sy1 = %d\n", sx1, sy1);
    bu_log("sx2 = %d, sy2 = %d\n", sx2, sy2);
    bu_log("color = %d\n", ((struct x_vars *)dmp->dm_vars.priv_vars)->fg);
  }

  XDrawLine( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	     ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
	     ((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	     sx1, sy1, sx2, sy2 );

  return TCL_OK;
}

HIDDEN int
Tk_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
  int   sx, sy;

  sx = dm_Normal2Xx(dmp, x);
  sy = dm_Normal2Xy(dmp, y, 0);

  if (dmp->dm_debugLevel) {
    bu_log("Tk_drawPoint2D()\n");
    bu_log("x = %g, y = %g\n", x, y);
    bu_log("sx = %d, sy = %d\n", sx, sy);
  }

  XDrawPoint( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	      ((struct x_vars *)dmp->dm_vars.priv_vars)->pix,
	      ((struct x_vars *)dmp->dm_vars.priv_vars)->gc, sx, sy );

  return TCL_OK;
}

HIDDEN int
Tk_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    XGCValues gcv;

    if (dmp->dm_debugLevel)
	bu_log("Tk_setFGColor( %d %d %d)\n", r, g, b);

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    XColor color;

    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;

    ((struct x_vars *)dmp->dm_vars.priv_vars)->fg = Tk_GetColorByValue
	(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, &color)->pixel;

    XSetForeground(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	((struct x_vars *)dmp->dm_vars.priv_vars)->fg);

    return TCL_OK;
}

HIDDEN int
Tk_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (dmp->dm_debugLevel)
	bu_log("Tk_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;


    XColor color;

    color.red = r << 8;
    color.green = g << 8;
    color.blue = b << 8;

    XSetBackground(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
	((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
	Tk_GetColorByValue(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
        &color)->pixel);

  return TCL_OK;
}

HIDDEN int
Tk_setLineAttr(struct dm *dmp, int width, int style)
{
  int linestyle;

  if (dmp->dm_debugLevel)
    bu_log("Tk_setLineAttr( width: %d, style: %d)\n", width, style);

  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  if(width < 1)
    width = 1;

  if(style == DM_DASHED_LINE)
    linestyle = LineOnOffDash;
  else
    linestyle = LineSolid;

  XSetLineAttributes( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct x_vars *)dmp->dm_vars.priv_vars)->gc,
		      width, linestyle, CapButt, JoinMiter );

  return TCL_OK;
}

/* ARGSUSED */
HIDDEN int
Tk_debug(struct dm *dmp, int lvl)
{
  dmp->dm_debugLevel = lvl;

  return TCL_OK;
}

HIDDEN int
Tk_setWinBounds(struct dm *dmp, register int *w)
{
  if (dmp->dm_debugLevel)
    bu_log("Tk_setWinBounds()\n");

  dmp->dm_clipmin[0] = w[0];
  dmp->dm_clipmin[1] = w[2];
  dmp->dm_clipmin[2] = w[4];
  dmp->dm_clipmax[0] = w[1];
  dmp->dm_clipmax[1] = w[3];
  dmp->dm_clipmax[2] = w[5];

  return TCL_OK;
}

HIDDEN int
Tk_configureWin_guts(struct dm *dmp, int force)
{
  XFontStruct     *newfontstruct;
  XGCValues       gcv;
  int h, w;

  /* nothing to do */
  h = Tk_Height(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  w = Tk_Width(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

  if (!force && dmp->dm_width==w && dmp->dm_height == h)
    return TCL_OK;

  dmp->dm_width=w;
  dmp->dm_width=h;

  dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

  if (dmp->dm_debugLevel) {
    bu_log("Tk_configureWin_guts()\n");
    bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
  }

  Tk_FreePixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		((struct x_vars *)dmp->dm_vars.priv_vars)->pix);
  ((struct x_vars *)dmp->dm_vars.priv_vars)->pix =
    Tk_GetPixmap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		 DefaultRootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy),
		 dmp->dm_width,
		 dmp->dm_height,
		 Tk_Depth(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

  /* First time through, load a font or quit */
  if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct == NULL) {
    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct =
        Tk_GetFont(dmp->dm_interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, 
        FONT9)) == NULL ) {
     
      /* Try hardcoded backup font */
      if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->tkfontstruct =
          Tk_GetFont(dmp->dm_interp, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin, 
          FONTBACK)) == NULL ) {
	bu_log("dm-Tk: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
	return TCL_ERROR;
      }
    }
  }

  /* XXX:  I removed the font-sizing routine from dm-X from here.  Something
           should be devised to replace it.  --TJM*/

  return TCL_OK;
}

HIDDEN int
Tk_configureWin(struct dm *dmp)
{
  /* don't force */
  return Tk_configureWin_guts(dmp, 0);
}

HIDDEN int
Tk_setLight(struct dm *dmp, int light_on)
{
  if (dmp->dm_debugLevel)
    bu_log("Tk_setLight:\n");

  dmp->dm_light = light_on;

  return TCL_OK;
}

HIDDEN int
Tk_setZBuffer(struct dm *dmp, int zbuffer_on)
{
  if (dmp->dm_debugLevel)
    bu_log("Tk_setZBuffer:\n");

  dmp->dm_zbuffer = zbuffer_on;

  return TCL_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
