/*                       D M - W G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file dm-wgl.c
 *
 *  A Windows OpenGL (wgl) Display Manager.
 *
 *  Authors -
 *	Carl Nuzman
 *	Robert G. Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 */

#include "common.h"

#ifdef DM_WGL

#include "tk.h"
#include "TkWinInt.h"

#undef VMIN		/* is used in vmath.h, too */

#include <GL/gl.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-wgl.h"
#include "dm_xvars.h"
#include "solid.h"

#define VIEWFACTOR      (1.0/(*dmp->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

#define USE_VECTOR_THRESHHOLD 0

#if USE_VECTOR_THRESHHOLD
extern int vectorThreshold;	/* defined in libdm/tcl.c */
#endif

static int wgl_actively_drawing;
HIDDEN PIXELFORMATDESCRIPTOR *wgl_choose_visual();

/* Display Manager package interface */
#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*wgl_open();
HIDDEN int	wgl_close();
HIDDEN int	wgl_drawBegin();
HIDDEN int      wgl_drawEnd();
HIDDEN int	wgl_normal(), wgl_loadMatrix();
HIDDEN int	wgl_drawString2D(), wgl_drawLine2D();
HIDDEN int      wgl_drawPoint2D();
HIDDEN int	wgl_drawVList();
HIDDEN int      wgl_setFGColor(), wgl_setBGColor();
HIDDEN int	wgl_setLineAttr();
HIDDEN int	wgl_configureWin_guts();
HIDDEN int	wgl_configureWin();
HIDDEN int	wgl_setLight();
HIDDEN int	wgl_setTransparency();
HIDDEN int	wgl_setDepthMask();
HIDDEN int	wgl_setZBuffer();
HIDDEN int	wgl_setWinBounds(), wgl_debug();
HIDDEN int      wgl_beginDList(), wgl_endDList();
HIDDEN int      wgl_drawDList();
HIDDEN int      wgl_freeDLists();

struct dm dm_wgl = {
  wgl_close,
  wgl_drawBegin,
  wgl_drawEnd,
  wgl_normal,
  wgl_loadMatrix,
  wgl_drawString2D,
  wgl_drawLine2D,
  wgl_drawPoint2D,
  wgl_drawVList,
  wgl_setFGColor,
  wgl_setBGColor,
  wgl_setLineAttr,
  wgl_configureWin,
  wgl_setWinBounds,
  wgl_setLight,
  wgl_setTransparency,
  wgl_setDepthMask,
  wgl_setZBuffer,
  wgl_debug,
  wgl_beginDList,
  wgl_endDList,
  wgl_drawDList,
  wgl_freeDLists,
  0,
  1,				/* has displaylist */
  0,                            /* no stereo by default */
  1.0,				/* zoom-in limit */
  1,				/* bound flag */
  "wgl",
  "Windows with OpenGL graphics",
  DM_TYPE_WGL,
  1,
  0,
  0,
  1,
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
  1,				/* depth buffer is writable */
  1,				/* zbuffer */
  0,				/* no zclipping */
  1,                            /* clear back buffer after drawing and swap */
  0				/* Tcl interpreter */
};

HIDDEN fastf_t default_viewscale = 1000.0;
HIDDEN double	xlim_view = 1.0;	/* args for glOrtho*/
HIDDEN double	ylim_view = 1.0;

/* lighting parameters */
HIDDEN float amb_three[] = {0.3, 0.3, 0.3, 1.0};

HIDDEN float light0_direction[] = {0.0, 0.0, 1.0, 0.0};
HIDDEN float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
HIDDEN float wireColor[4];
HIDDEN float ambientColor[4];
HIDDEN float specularColor[4];
HIDDEN float diffuseColor[4];
#if 1
HIDDEN float backColor[] = {1.0, 1.0, 0.0, 1.0}; /* yellow */
#else
HIDDEN float backColor[] = {0.1, 0.1, 0.1, 1.0}; /* dark gray */
#endif

void
wgl_fogHint(dmp, fastfog)
struct dm *dmp;
int fastfog;
{
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
  glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}

/*
 *			W G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
wgl_open(Tcl_Interp *interp, int argc, char *argv[])
{
  static int count = 0;
  GLfloat backgnd[4];
  int make_square = -1;
  int nclass = 0;
  struct bu_vls str;
  struct bu_vls init_proc_vls;
  struct dm *dmp = (struct dm *)NULL;
  Tk_Window tkwin;
  HWND hwnd;
  HDC hdc;

  if ((tkwin = Tk_MainWindow(interp)) == NULL) {
      return DM_NULL;
  }

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL) {
      return DM_NULL;
  }

  *dmp = dm_wgl; /* struct copy */
  dmp->dm_interp = interp;

  dmp->dm_vars.pub_vars = (genptr_t)bu_calloc(1, sizeof(struct dm_xvars), "wgl_open: dm_xvars");
  if(dmp->dm_vars.pub_vars == (genptr_t)NULL){
    bu_free(dmp, "wgl_open: dmp");
    return DM_NULL;
  }

  dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct wgl_vars), "wgl_open: wgl_vars");
  if(dmp->dm_vars.priv_vars == (genptr_t)NULL){
    bu_free(dmp->dm_vars.pub_vars, "wgl_open: dmp->dm_vars.pub_vars");
    bu_free(dmp, "wgl_open: dmp");
    return DM_NULL;
  }

  dmp->dm_vp = &default_viewscale;

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0)
     bu_vls_printf(&dmp->dm_pathName, ".dm_wgl%d", count);
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
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devmotionnotify = LASTEvent;
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress = LASTEvent;
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease = LASTEvent;
  dmp->dm_aspect = 1.0;

  /* initialize modifiable variables */
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.rgb = 1;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer = 1;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = 1;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity = 1.0;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on = dmp->dm_zclip;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.debug = dmp->dm_debugLevel;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.bound = dmp->dm_bound;
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag = dmp->dm_boundFlag;

  /* this is important so that wgl_configureWin knows to set the font */
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = NULL;

  if(dmp->dm_width == 0){
    dmp->dm_width = GetSystemMetrics(SM_CXSCREEN)- 30;
     ++make_square;
  }
  if(dmp->dm_height == 0){
    dmp->dm_height = GetSystemMetrics(SM_CYSCREEN) - 30;
     ++make_square;
  }

  if(make_square > 0){
    /* Make window square */
    if(dmp->dm_height <
       dmp->dm_width)
      dmp->dm_width =
	dmp->dm_height;
    else
      dmp->dm_height =
	dmp->dm_width;
  }

  if(dmp->dm_top){
    /* Make xtkwin a toplevel window */
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin =
      Tk_CreateWindowFromPath(interp,
			      tkwin,
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

  if( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin == NULL ) {
      bu_log("open_gl: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
      bu_vls_free(&init_proc_vls);
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S\n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
      bu_log("open_wgl: _init_dm failed\n");
      bu_vls_free(&init_proc_vls);
      bu_vls_free(&str);
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy =
    Tk_Display(((struct dm_xvars *)dmp->dm_vars.pub_vars)->top);

  /* make sure there really is a display before proceeding. */
  if (!((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy) {
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
		     dmp->dm_width,
		     dmp->dm_height);

  Tk_MakeWindowExist(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win =
    Tk_WindowId(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  dmp->dm_id = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win;

  hwnd = TkWinGetHWND(((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);
  hdc = GetDC(hwnd);
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc = hdc;

  if((((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip =
      wgl_choose_visual(dmp,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)) == NULL) {
      bu_log("wgl_open: Can't get an appropriate visual.\n");
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->depth = ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.depth;

  /* open GLX context */
  if (( ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc =
	  wglCreateContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc))==NULL){
      bu_log("wgl_open: couldn't create glXContext.\n");
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  if (!wglMakeCurrent( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
		      ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)){
      bu_log("wgl_open: couldn't make context current\n");
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  /* display list (fontOffset + char) will display a given ASCII char */
  if ((((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0){
      bu_log("wgl_open: couldn't make display lists for font.\n");
      (void)wgl_close(dmp);
      return DM_NULL;
  }

  /* This is the applications display list offset */
  dmp->dm_displaylist = ((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset + 128;

  wgl_setBGColor(dmp, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer)
    glDrawBuffer(GL_BACK);
  else
    glDrawBuffer(GL_FRONT);

  /* do viewport, ortho commands and initialize font */
  (void)wgl_configureWin_guts(dmp, 1);

  /* Lines will be solid when stippling disabled, dashed when enabled*/
  glLineStipple( 1, 0xCF33);
  glDisable(GL_LINE_STIPPLE);

  backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
  glFogi(GL_FOG_MODE, GL_LINEAR);
  glFogf(GL_FOG_START, 0.0);
  glFogf(GL_FOG_END, 2.0);
  glFogfv(GL_FOG_COLOR, backgnd);

  /*XXX Need to do something about VIEWFACTOR */
  glFogf(GL_FOG_DENSITY, VIEWFACTOR);

  /* Initialize matrices */
  /* Leave it in model_view mode normally */
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
  glGetDoublev(GL_PROJECTION_MATRIX, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.0, 0.0, -1.0);
  glPushMatrix();
  glLoadIdentity();
  ((struct wgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */

    if (!wglMakeCurrent((HDC)NULL, (HGLRC)NULL)) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR)&buf,
		      0,
		      NULL);
	bu_log("wgl_drawBegin: Couldn't release the current context.\n");
	bu_log("wgl_drawBegin: %s", buf);
	LocalFree(buf);

	return DM_NULL;
    }

  Tk_MapWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  return dmp;
}

/*
 */
int
wgl_share_dlist(dmp1, dmp2)
struct dm *dmp1;
struct dm *dmp2;
{
  GLfloat backgnd[4];
  GLfloat vf;
  HGLRC old_glxContext;

  if (dmp1 == (struct dm *)NULL)
    return TCL_ERROR;

  if (dmp2 == (struct dm *)NULL) {
    /* create a new graphics context for dmp1 with private display lists */

    old_glxContext = ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc;

    if ((((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc =
	 wglCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->hdc))==NULL){
	bu_log("wgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;
	return TCL_ERROR;
    }

    if (!wglMakeCurrent( ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->hdc,
			 ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc)){
	bu_log("wgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	return TCL_ERROR;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((((struct wgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0){
      bu_log("dm-wgl: Can't make display lists for font.\nUsing old context\n.");
      ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    /* This is the applications display list offset */
    dmp1->dm_displaylist = ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset + 128;

    wgl_setBGColor(dmp1, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (((struct wgl_vars *)dmp1->dm_vars.priv_vars)->mvars.doublebuffer)
      glDrawBuffer(GL_BACK);
    else
      glDrawBuffer(GL_FRONT);

    /* this is important so that wgl_configureWin knows to set the font */
    ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->fontstruct = NULL;

    /* do viewport, ortho commands and initialize font */
    (void)wgl_configureWin_guts(dmp1, 1);

    /* Lines will be solid when stippling disabled, dashed when enabled*/
    glLineStipple( 1, 0xCF33);
    glDisable(GL_LINE_STIPPLE);

    backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 2.0);
    glFogfv(GL_FOG_COLOR, backgnd);

    /*XXX Need to do something about VIEWFACTOR */
    vf = 1.0/(*dmp1->dm_vp);
    glFogf(GL_FOG_DENSITY, vf);

    /* Initialize matrices */
    /* Leave it in model_view mode normally */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -1.0);
    glPushMatrix();
    glLoadIdentity();
    ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

    /* destroy old context */
    wglMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->hdc,
		   ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->glxc);
    wglDeleteContext(old_glxContext);
  } else {
    /* dmp1 will share it's display lists with dmp2 */

    old_glxContext = ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc;

    if ((((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc =
	 wglCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->hdc))==NULL){
	bu_log("wgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	return TCL_ERROR;
    }

    if (!wglMakeCurrent( ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->hdc,
			 ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc)){
	bu_log("wgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	return TCL_ERROR;
    }

    ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->fontOffset = ((struct wgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset;
    dmp2->dm_displaylist = dmp1->dm_displaylist;

    wgl_setBGColor(dmp2, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (((struct wgl_vars *)dmp2->dm_vars.priv_vars)->mvars.doublebuffer)
      glDrawBuffer(GL_BACK);
    else
      glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)wgl_configureWin_guts(dmp2, 1);

    /* Lines will be solid when stippling disabled, dashed when enabled*/
    glLineStipple( 1, 0xCF33);
    glDisable(GL_LINE_STIPPLE);

    backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 2.0);
    glFogfv(GL_FOG_COLOR, backgnd);

    /*XXX Need to do something about VIEWFACTOR */
    vf = 1.0/(*dmp2->dm_vp);
    glFogf(GL_FOG_DENSITY, vf);

    /* Initialize matrices */
    /* Leave it in model_view mode normally */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -1.0);
    glPushMatrix();
    glLoadIdentity();
    ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

    /* destroy old context */
    wglMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->hdc,
		   ((struct wgl_vars *)dmp2->dm_vars.priv_vars)->glxc);
    wglDeleteContext(old_glxContext);
  }

  return TCL_OK;
}

/*
 *  			W G L _ C L O S E
 *
 *  Gracefully release the display.
 */
HIDDEN int
wgl_close(dmp)
struct dm *dmp;
{
    if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy){
	if(((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc){
	    wglMakeCurrent( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc);
	    wglDeleteContext(((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc);
	}

	if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)
	    XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);

	if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
	    Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
    }

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "wgl_close: wgl_vars");
    bu_free(dmp->dm_vars.pub_vars, "wgl_close: dm_xvars");
    bu_free(dmp, "wgl_close: dmp");

    return TCL_OK;
}

/*
 *			W G L _ D R A W B E G I N
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
wgl_drawBegin(dmp)
struct dm *dmp;
{
    GLfloat fogdepth;

    if (dmp->dm_debugLevel) {
	bu_log("wgl_drawBegin\n");

	if (wgl_actively_drawing)
	    bu_log("wgl_drawBegin: already actively drawing\n");
    }

    wgl_actively_drawing = 1;

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR)&buf,
		      0,
		      NULL);
	bu_log("wgl_drawBegin: Couldn't make context current.\n");
	bu_log("wgl_drawBegin: %s", buf);
	LocalFree(buf);

	return TCL_ERROR;
    }

  /* clear back buffer */
  if (!dmp->dm_clearBufferAfter &&
      ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
    glClearColor(((struct wgl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on){
      glEnable(GL_FOG);
      /*XXX Need to do something with Viewscale */
      fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
      glFogf(GL_FOG_END, fogdepth);
      fogdepth = (GLfloat) (0.5*((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity/
			    (*dmp->dm_vp));
      glFogf(GL_FOG_DENSITY, fogdepth);
      glFogi(GL_FOG_MODE, dmp->dm_perspective ? GL_EXP : GL_LINEAR);
    }
    if (dmp->dm_light) {
      glEnable(GL_LIGHTING);
    }
  }

  return TCL_OK;
}

/*
 *			W G L _ D R A W E N D
 */
HIDDEN int
wgl_drawEnd(dmp)
struct dm *dmp;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_drawEnd\n");


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_direction);
    }

    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer){
	SwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc);

	if (dmp->dm_clearBufferAfter) {
	    /* give Graphics pipe time to work */
	    glClearColor(((struct wgl_vars *)dmp->dm_vars.priv_vars)->r,
			 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->g,
			 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->b,
			 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
    }

    if (dmp->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while((error = glGetError())!=0){
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

/*XXX Keep this off unless testing */
#if 0
    glFinish();
#endif

    if (!wglMakeCurrent((HDC)NULL, (HGLRC)NULL)) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR)&buf,
		      0,
		      NULL);
	bu_log("wgl_drawBegin: Couldn't release the current context.\n");
	bu_log("wgl_drawBegin: %s", buf);
	LocalFree(buf);

	return TCL_ERROR;
    }

    wgl_actively_drawing = 0;
    return TCL_OK;
}

/*
 *  			W G L _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to wgl_drawVList().
 */
HIDDEN int
wgl_loadMatrix(dmp, mat, which_eye)
struct dm *dmp;
mat_t mat;
int which_eye;
{
  register fastf_t *mptr;
  GLfloat gtmat[16];
  mat_t	newm;

  if(dmp->dm_debugLevel){
    struct bu_vls tmp_vls;

    bu_log("wgl_loadMatrix()\n");

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
    bu_vls_printf(&tmp_vls, "transformation matrix = \n");
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8],mat[12]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9],mat[13]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10],mat[14]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11],mat[15]);

    bu_log("%s", bu_vls_addr(&tmp_vls));
    bu_vls_free(&tmp_vls);
  }

  switch(which_eye)  {
  case 0:
    /* Non-stereo */
    break;
  case 1:
    /* R eye */
    glViewport(0,  0, (XMAXSCREEN)+1, ( YSTEREO)+1);
    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
    wgl_drawString2D( dmp, "R", 0.986, 0.0, 0, 1 );
    break;
  case 2:
    /* L eye */
    glViewport(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
	       ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
    glScissor(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
	      ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
    break;
  }

  if (!dmp->dm_zclip) {
    mat_t       nozclip;

    MAT_IDN( nozclip );
    nozclip[10] = 1.0e-20;
    bn_mat_mul( newm, nozclip, mat );
    mptr = newm;
  } else {
    mat_t       nozclip;

    MAT_IDN(nozclip);
    nozclip[10] = dmp->dm_bound;
    bn_mat_mul(newm, nozclip, mat);
    mptr = newm;
  }

  gtmat[0] = *(mptr++);
  gtmat[4] = *(mptr++);
  gtmat[8] = *(mptr++);
  gtmat[12] = *(mptr++);

  gtmat[1] = *(mptr++) * dmp->dm_aspect;
  gtmat[5] = *(mptr++) * dmp->dm_aspect;
  gtmat[9] = *(mptr++) * dmp->dm_aspect;
  gtmat[13] = *(mptr++) * dmp->dm_aspect;

  gtmat[2] = *(mptr++);
  gtmat[6] = *(mptr++);
  gtmat[10] = *(mptr++);
  gtmat[14] = *(mptr++);

  gtmat[3] = *(mptr++);
  gtmat[7] = *(mptr++);
  gtmat[11] = *(mptr++);
  gtmat[15] = *(mptr++);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef( 0.0, 0.0, -1.0 );
  glMultMatrixf( gtmat );

  return TCL_OK;
}

/*
 *  			W G L _ D R A W V L I S T
 *
 */
HIDDEN int
wgl_drawVList(dmp, vp)
     struct dm			*dmp;
     register struct rt_vlist	*vp;
{
	register struct rt_vlist	*tvp;
	int				first;
#if USE_VECTOR_THRESHHOLD
	static int			nvectors = 0;
#endif
	int mflag = 1;
	float black[4] = {0.0, 0.0, 0.0, 0.0};

	if (dmp->dm_debugLevel)
		bu_log("wgl_drawVList()\n");

	/* Viewing region is from -1.0 to +1.0 */
	first = 1;
	for (BU_LIST_FOR(tvp, rt_vlist, &vp->l)) {
		register int	i;
		register int	nused = tvp->nused;
		register int	*cmd = tvp->cmd;
		register point_t *pt = tvp->pt;
		for (i = 0; i < nused; i++,cmd++,pt++) {
			if (dmp->dm_debugLevel > 2)
				bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(pt));
			switch (*cmd) {
			case RT_VLIST_LINE_MOVE:
				/* Move, start line */
				if (first == 0)
					glEnd();
				first = 0;

				if (dmp->dm_light && mflag) {
				    mflag = 0;
				    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
				    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
				    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
				    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

				    if (dmp->dm_transparency)
					glDisable(GL_BLEND);
				}

				glBegin(GL_LINE_STRIP);
				glVertex3dv(*pt);
				break;
			case RT_VLIST_POLY_START:
				/* Start poly marker & normal */
				if (first == 0)
					glEnd();

				if (dmp->dm_light && mflag) {
				    mflag = 0;
				    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
				    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
				    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
#if 0
				    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
#else
				    if (1 < dmp->dm_light)
					glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
				    else {
					glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);
					glMaterialfv(GL_BACK, GL_DIFFUSE, backColor);
				    }
#endif

				    if (dmp->dm_transparency)
					glEnable(GL_BLEND);
				}

				glBegin(GL_POLYGON);
				/* Set surface normal (vl_pnt points outward) */
				glNormal3dv(*pt);
				break;
			case RT_VLIST_LINE_DRAW:
			case RT_VLIST_POLY_MOVE:
			case RT_VLIST_POLY_DRAW:
				glVertex3dv(*pt);
				break;
			case RT_VLIST_POLY_END:
				/* Draw, End Polygon */
				glVertex3dv(*pt);
				glEnd();
				first = 1;
				break;
			case RT_VLIST_POLY_VERTNORM:
				/* Set per-vertex normal.  Given before vert. */
				glNormal3dv(*pt);
				break;
			}
		}

#if USE_VECTOR_THRESHHOLD
/*XXX The Tcl_DoOneEvent below causes the following error:
X Error of failed request:  GLXBadContextState
*/

		nvectors += nused;

		if (nvectors >= vectorThreshold) {
			if (dmp->dm_debugLevel)
				bu_log("wgl_drawVList(): handle Tcl events\n");

			nvectors = 0;

			/* Handle events in the queue */
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

			if (dmp->dm_debugLevel)
				bu_log("wgl_drawVList(): handled Tcl events successfully\n");
		}
#endif
	}

	if (first == 0)
		glEnd();

	return TCL_OK;
}

/*
 *			W G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
HIDDEN int
wgl_normal(dmp)
struct dm *dmp;
{

  if (dmp->dm_debugLevel)
    bu_log("wgl_normal\n");

  if (!((struct wgl_vars *)dmp->dm_vars.priv_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd( ((struct wgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat );
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
    if(((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on)
      glDisable(GL_FOG);
    if (dmp->dm_light)
      glDisable(GL_LIGHTING);
  }

  return TCL_OK;
}

/*
 *			W G L _ D R A W S T R I N G 2 D
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
wgl_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
fastf_t x, y;
int size;
int use_aspect;
{
  if (dmp->dm_debugLevel)
    bu_log("wgl_drawString2D()\n");

  if(use_aspect)
    glRasterPos2f(x, y * dmp->dm_aspect);
  else
    glRasterPos2f(x, y);

  glListBase(((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
  glCallLists(strlen( str ), GL_UNSIGNED_BYTE,  str );

  return TCL_OK;
}


/*
 *			W G L _ D R A W L I N E 2 D
 *
 */
HIDDEN int
wgl_drawLine2D( dmp, x1, y1, x2, y2)
struct dm *dmp;
fastf_t x1, y1;
fastf_t x2, y2;
{

  if (dmp->dm_debugLevel)
    bu_log("wgl_drawLine2D()\n");

  if(dmp->dm_debugLevel){
    GLfloat pmat[16];

    glGetFloatv(GL_PROJECTION_MATRIX, pmat);
    bu_log("projection matrix:\n");
    bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
    bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
    bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
    bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
    glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
    bu_log("modelview matrix:\n");
    bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
    bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
    bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
    bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
  }

  glBegin(GL_LINES);
  glVertex2f(x1, y1);
  glVertex2f(x2, y2);
  glEnd();

  return TCL_OK;
}

HIDDEN int
wgl_drawPoint2D(dmp, x, y)
struct dm *dmp;
fastf_t x, y;
{
  if (dmp->dm_debugLevel){
    bu_log("wgl_drawPoint2D():\n");
    bu_log("\tdmp: %lu\tx - %lf\ty - %lf\n", (unsigned long)dmp, x, y);
  }

  glBegin(GL_POINTS);
  glVertex2f(x, y);
  glEnd();

  return TCL_OK;
}


HIDDEN int
wgl_setFGColor(dmp, r, g, b, strict, transparency)
struct dm *dmp;
unsigned char r, g, b;
int strict;
fastf_t transparency;
{
  if (dmp->dm_debugLevel)
    bu_log("wgl_setFGColor()\n");

  dmp->dm_fg[0] = r;
  dmp->dm_fg[1] = g;
  dmp->dm_fg[2] = b;

  if(strict){
    glColor3ub( (GLubyte)r, (GLubyte)g, (GLubyte)b );
  }else{
    if (dmp->dm_light) {
      /* Ambient = .2, Diffuse = .6, Specular = .2 */

      /* wireColor gets the full rgb */
      wireColor[0] = r / 255.0;
      wireColor[1] = g / 255.0;
      wireColor[2] = b / 255.0;
      wireColor[3] = transparency;

      ambientColor[0] = wireColor[0] * 0.2;
      ambientColor[1] = wireColor[1] * 0.2;
      ambientColor[2] = wireColor[2] * 0.2;
      ambientColor[3] = wireColor[3];

      specularColor[0] = ambientColor[0];
      specularColor[1] = ambientColor[1];
      specularColor[2] = ambientColor[2];
      specularColor[3] = ambientColor[3];

      diffuseColor[0] = wireColor[0] * 0.6;
      diffuseColor[1] = wireColor[1] * 0.6;
      diffuseColor[2] = wireColor[2] * 0.6;
      diffuseColor[3] = wireColor[3];
    }else{
      glColor3ub( (GLubyte)r,  (GLubyte)g,  (GLubyte)b );
    }
  }

  return TCL_OK;
}

HIDDEN int
wgl_setBGColor(struct dm *dmp,
	       unsigned char r,
	       unsigned char g,
	       unsigned char b)
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	    bu_log("wgl_setBGColor: Couldn't make context current\n");
	    return TCL_ERROR;
	}

	SwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc);
	glClearColor(((struct wgl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct wgl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct wgl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    return TCL_OK;
}

HIDDEN int
wgl_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  if (dmp->dm_debugLevel)
    bu_log("wgl_setLineAttr()\n");

  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  glLineWidth((GLfloat) width);

  if(style == DM_DASHED_LINE)
    glEnable(GL_LINE_STIPPLE);
  else
    glDisable(GL_LINE_STIPPLE);

  return TCL_OK;
}

/* ARGSUSED */
HIDDEN int
wgl_debug(dmp, lvl)
struct dm *dmp;
int lvl;
{
  dmp->dm_debugLevel = lvl;

  return TCL_OK;
}

HIDDEN int
wgl_setWinBounds(dmp, w)
struct dm *dmp;
int w[6];
{
  if (dmp->dm_debugLevel)
    bu_log("wgl_setWinBounds()\n");

  dmp->dm_clipmin[0] = w[0];
  dmp->dm_clipmin[1] = w[2];
  dmp->dm_clipmin[2] = w[4];
  dmp->dm_clipmax[0] = w[1];
  dmp->dm_clipmax[1] = w[3];
  dmp->dm_clipmax[2] = w[5];

  if (dmp->dm_clipmax[2] <= GED_MAX)
      dmp->dm_bound = 1.0;
  else
      dmp->dm_bound = GED_MAX / dmp->dm_clipmax[2];

  return TCL_OK;
}

#define WGL_DO_STEREO 1
/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
HIDDEN PIXELFORMATDESCRIPTOR *
wgl_choose_visual(struct dm *dmp,
		  Tk_Window tkwin)
{
    int iPixelFormat ;
    PIXELFORMATDESCRIPTOR *ppfd, pfd;
    BOOL good;

    /* Try to satisfy the above desires with a color visual of the
     * greatest depth */

    ppfd = &pfd;
    memset(ppfd, 0, sizeof(PIXELFORMATDESCRIPTOR));

    if ((iPixelFormat =
	 GetPixelFormat(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc))) {
	LPVOID buf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM |
		      FORMAT_MESSAGE_IGNORE_INSERTS,
		      NULL,
		      GetLastError(),
		      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR)&buf,
		      0,
		      NULL);
	bu_log("wgl_choose_visual: failed to get the pixel format\n");
	bu_log("wgl_choose_visual: %s", buf);
	LocalFree(buf);

	return (PIXELFORMATDESCRIPTOR *)NULL;
    }

    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    ppfd->nVersion = 1;
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    ppfd->iPixelType = PFD_TYPE_RGBA;
    ppfd->cColorBits = 24;
    ppfd->cDepthBits = 32;
    ppfd->iLayerType = PFD_MAIN_PLANE;

    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf = 1;
    iPixelFormat = ChoosePixelFormat(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc, ppfd);
    good = SetPixelFormat(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc, iPixelFormat, ppfd);

    if (good)
	return ppfd;

    return (PIXELFORMATDESCRIPTOR *)NULL;
}


/*
 *			W G L _ C O N F I G U R E W I N
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
wgl_configureWin_guts(struct dm *dmp,
		      int force)
{
    GLint mm;
    HFONT newfontstruct, oldfont;
    LOGFONT logfont;
    HWND hwnd;
    RECT xwa;

    if (dmp->dm_debugLevel)
	bu_log("wgl_configureWin_guts()\n");

    if (!wglMakeCurrent( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)){
	bu_log("wgl_configureWin_guts: Couldn't make context current\n");
	return TCL_ERROR;
    }

    hwnd = WindowFromDC(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc);
    GetWindowRect(hwnd,&xwa);

    /* nothing to do */
    if (!force &&
	dmp->dm_height == (xwa.bottom-xwa.top) &&
	dmp->dm_width == (xwa.right-xwa.left))
	return TCL_OK;

    dmp->dm_height = xwa.bottom-xwa.top;
    dmp->dm_width = xwa.right-xwa.left;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	bu_log("wgl_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);

    if(dmp->dm_zbuffer)
	wgl_setZBuffer(dmp, dmp->dm_zbuffer);

    wgl_setLight(dmp, dmp->dm_light);

    glClearColor(((struct wgl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct wgl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*CJXX this might cause problems in perspective mode? */
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
    glMatrixMode(mm);

    /* First time through, load a font or quit */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct == NULL) {
	logfont.lfHeight = 18;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 10;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfItalic = FALSE;
	logfont.lfUnderline = FALSE;
	logfont.lfStrikeOut = FALSE;
	logfont.lfCharSet = ANSI_CHARSET;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	logfont.lfFaceName[0] = (TCHAR)0;

	if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	     CreateFontIndirect(&logfont)) == NULL ) {
	    /* ????? add backup later */
	    /* Try hardcoded backup font */
	    /*     if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
		   (HFONT *)CreateFontIndirect(&logfont)) == NULL) */
	    {
		bu_log("wgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return TCL_ERROR;
	    }
	}

	oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

	if (oldfont != NULL)
	    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
    }

    /* Always try to choose a the font that best fits the window size.
     */

    if(!GetObject( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct, sizeof(LOGFONT), &logfont)) {
	logfont.lfHeight = 18;
	logfont.lfWidth = 0;
	logfont.lfEscapement = 0;
	logfont.lfOrientation = 10;
	logfont.lfWeight = FW_NORMAL;
	logfont.lfItalic = FALSE;
	logfont.lfUnderline = FALSE;
	logfont.lfStrikeOut = FALSE;
	logfont.lfCharSet = ANSI_CHARSET ;
	logfont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logfont.lfClipPrecision =  CLIP_DEFAULT_PRECIS ;
	logfont.lfQuality = DEFAULT_QUALITY;
	logfont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	logfont.lfFaceName[0] = (TCHAR) 0;

	if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	     CreateFontIndirect(&logfont)) == NULL ) {
	    bu_log("wgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
	    return TCL_ERROR;
	}

	oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

	if (oldfont != NULL)
	    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
    }


    if (dmp->dm_width < 582) {
	if (logfont.lfHeight != 10) {
	    logfont.lfHeight = 10;
	    logfont.lfWidth = 0;
	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {

		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

		if (oldfont != NULL)
		    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
	    }
	}
    } else if (dmp->dm_width < 679) {
	if (logfont.lfHeight != 12){
	    logfont.lfHeight = 12;
	    logfont.lfWidth = 0;

	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

		if (oldfont != NULL)
		    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
	    }
	}
    } else if (dmp->dm_width < 776) {
	if (logfont.lfHeight != 14){
	    logfont.lfHeight = 14;
	    logfont.lfWidth = 0;

	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
	    }
	}
    } else if (dmp->dm_width < 873) {
	if (logfont.lfHeight != 15){
	    logfont.lfHeight = 15;
	    logfont.lfWidth = 0;
	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

		if (oldfont != NULL)
		    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
	    }
	}
    } else {
	if (logfont.lfWidth != 16){
	    logfont.lfHeight = 16;
	    logfont.lfWidth = 0;
	    if ((newfontstruct = CreateFontIndirect(&logfont)) != NULL) {
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		oldfont = SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		wglUseFontBitmaps(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,0,256,((struct wgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);

		if (oldfont != NULL)
		    DeleteObject(SelectObject(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,oldfont));
	    }
	}
    }

    return TCL_OK;
}


HIDDEN int
wgl_configureWin(dmp)
struct dm *dmp;
{
  return wgl_configureWin_guts(dmp, 0);
}

HIDDEN int
wgl_setLight(dmp, lighting_on)
struct dm *dmp;
int lighting_on;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_setLight()\n");

    dmp->dm_light = lighting_on;
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_setLight: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (!dmp->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return TCL_OK;
}

HIDDEN int
wgl_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_setTransparency()\n");

    dmp->dm_transparency = transparency_on;
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on = dmp->dm_transparency;

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_setTransparency: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (transparency_on) {
	/* Turn it on */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
	/* Turn it off */
	glDisable(GL_BLEND);
    }

    return TCL_OK;
}

HIDDEN int
wgl_setDepthMask(struct dm *dmp,
		 int enable) {
    if (dmp->dm_debugLevel)
	bu_log("wgl_setDepthMask()\n");

    dmp->dm_depthMask = enable;

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_setDepthMask: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}

HIDDEN int
wgl_setZBuffer(dmp, zbuffer_on)
struct dm *dmp;
int zbuffer_on;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;
    ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_setZBuffer: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf == 0) {
	dmp->dm_zbuffer = 0;
	((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
    }

    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}

int
wgl_beginDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_beginDList()\n");

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_beginDList: Couldn't make context current\n");
	return TCL_ERROR;
    }

    glNewList(dmp->dm_displaylist + list, GL_COMPILE);
    return TCL_OK;
}

int
wgl_endDList(dmp)
struct dm *dmp;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_endDList()\n");

    glEndList();
    return TCL_OK;
}

int
wgl_drawDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_drawDList()\n");

    glCallList(dmp->dm_displaylist + list);
    return TCL_OK;
}

int
wgl_freeDLists(dmp, list, range)
struct dm *dmp;
unsigned int list;
int range;
{
    if (dmp->dm_debugLevel)
	bu_log("wgl_freeDLists()\n");

    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("wgl_freeDLists: Couldn't make context current\n");
	return TCL_ERROR;
    }

    glDeleteLists(dmp->dm_displaylist + list, (GLsizei)range);
    return TCL_OK;
}

#endif /* DM_WGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
