/*
 *			D M - O G L . C
 *
 *  An X/ogl Display Manager.
 *  
 *  Authors -
 *	Carl Nuzman
 *	Robert G. Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1988-2004 by the United States Army.
 *	All rights reserved.
 *
 */

#include "conf.h"
#include "tk.h"

#undef VMIN		/* is used in vmath.h, too */

#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef   X_NOT_STDC_ENV
#	undef   X_NOT_POSIX
#endif

#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

#include <X11/extensions/XInput.h>
#include <GL/glx.h>
#include <GL/gl.h>

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-ogl.h"
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

static int ogl_actively_drawing;
HIDDEN XVisualInfo *ogl_choose_visual();

/* Display Manager package interface */
#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*ogl_open();
HIDDEN int	ogl_close();
HIDDEN int	ogl_drawBegin();
HIDDEN int      ogl_drawEnd();
HIDDEN int	ogl_normal(), ogl_loadMatrix();
HIDDEN int	ogl_drawString2D(), ogl_drawLine2D();
HIDDEN int      ogl_drawPoint2D();
HIDDEN int	ogl_drawVList();
HIDDEN int      ogl_setFGColor(), ogl_setBGColor();
HIDDEN int	ogl_setLineAttr();
HIDDEN int	ogl_configureWin_guts();
HIDDEN int	ogl_configureWin();
HIDDEN int	ogl_setLight();
HIDDEN int	ogl_setZBuffer();
HIDDEN int	ogl_setWinBounds(), ogl_debug();
HIDDEN int      ogl_beginDList(), ogl_endDList();
HIDDEN int      ogl_drawDList();
HIDDEN int      ogl_freeDLists();

struct dm dm_ogl = {
  ogl_close,
  ogl_drawBegin,
  ogl_drawEnd,
  ogl_normal,
  ogl_loadMatrix,
  ogl_drawString2D,
  ogl_drawLine2D,
  ogl_drawPoint2D,
  ogl_drawVList,
  ogl_setFGColor,
  ogl_setBGColor,
  ogl_setLineAttr,
  ogl_configureWin,
  ogl_setWinBounds,
  ogl_setLight,
  ogl_setZBuffer,
  ogl_debug,
  ogl_beginDList,
  ogl_endDList,
  ogl_drawDList,
  ogl_freeDLists,
  0,
  1,				/* has displaylist */
  0,                            /* no stereo by default */
  IRBOUND,			/* zoom-in limit */
  1,				/* bound flag */
  "ogl",
  "X Windows with OpenGL graphics",
  DM_TYPE_OGL,
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
  {0.0, 0.0, 0.0},		/* clipmin */
  {0.0, 0.0, 0.0},		/* clipmax */
  0,				/* no debugging */
  0,				/* no perspective */
  0,				/* no lighting */
  1,				/* zbuffer */
  0,				/* no zclipping */
  0				/* Tcl interpreter */
};

HIDDEN fastf_t default_viewscale = 1000.0;
HIDDEN double	xlim_view = 1.0;	/* args for glOrtho*/
HIDDEN double	ylim_view = 1.0;

/* lighting parameters */
HIDDEN float amb_three[] = {0.3, 0.3, 0.3, 1.0};

#if USE_SURVICE_MODS
HIDDEN float light0_direction[] = {0.0, 0.0, 1.0, 0.0};
HIDDEN float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
#else
HIDDEN float light0_position[] = {100.0, 200.0, 100.0, 0.0};
HIDDEN float light1_position[] = {100.0, 30.0, 100.0, 0.0};
HIDDEN float light2_position[] = {-100.0, 20.0, 20.0, 0.0};
HIDDEN float light3_position[] = {0.0, -100.0, -100.0, 0.0};

HIDDEN float light0_diffuse[] = {0.70, 0.70, 0.70, 1.0}; /* white */
HIDDEN float light1_diffuse[] = {0.60, 0.10, 0.10, 1.0}; /* red */
HIDDEN float light2_diffuse[] = {0.10, 0.30, 0.10, 1.0}; /* green */
HIDDEN float light3_diffuse[] = {0.10, 0.10, 0.30, 1.0}; /* blue */
#endif

void
ogl_fogHint(dmp, fastfog)
struct dm *dmp;
int fastfog;
{
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
  glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}

/*
 *			O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
ogl_open(interp, argc, argv)
     Tcl_Interp *interp;
     int argc;
     char *argv[];
{
  static int count = 0;
  GLfloat backgnd[4];
  int j, k;
  int make_square = -1;
  int ndevices;
  int nclass = 0;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  struct bu_vls str;
  struct bu_vls init_proc_vls;
  Display *tmp_dpy;
  struct dm *dmp;
  Tk_Window tkwin;

  if((tkwin = Tk_MainWindow(interp)) == NULL){
	  return DM_NULL;
  }

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_ogl; /* struct copy */
  dmp->dm_interp = interp;

  dmp->dm_vars.pub_vars = (genptr_t)bu_calloc(1, sizeof(struct dm_xvars), "ogl_open: dm_xvars");
  if(dmp->dm_vars.pub_vars == (genptr_t)NULL){
    bu_free(dmp, "ogl_open: dmp");
    return DM_NULL;
  }

  dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct ogl_vars), "ogl_open: ogl_vars");
  if(dmp->dm_vars.priv_vars == (genptr_t)NULL){
    bu_free(dmp->dm_vars.pub_vars, "ogl_open: dmp->dm_vars.pub_vars");
    bu_free(dmp, "ogl_open: dmp");
    return DM_NULL;
  }

  dmp->dm_vp = &default_viewscale;

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0)
     bu_vls_printf(&dmp->dm_pathName, ".dm_ogl%d", count);
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
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.rgb = 1;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer = 1;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = 1;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity = 1.0;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on = dmp->dm_zclip;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.debug = dmp->dm_debugLevel;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.bound = dmp->dm_bound;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag = dmp->dm_boundFlag;

  /* this is important so that ogl_configureWin knows to set the font */
  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = NULL;

  if((tmp_dpy = XOpenDisplay(bu_vls_addr(&dmp->dm_dName))) == NULL){
    bu_vls_free(&init_proc_vls);
    (void)ogl_close(dmp);
    return DM_NULL;
  }
#ifndef USE_MESA_GL
  {
    int return_val;

    if(!XQueryExtension(tmp_dpy, "GLX", &return_val, &return_val, &return_val)){
      bu_vls_free(&init_proc_vls);
      (void)ogl_close(dmp);
      return DM_NULL;
    }
  }
#endif
  if(dmp->dm_width == 0){
    dmp->dm_width =
      DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 30;
     ++make_square;
  }
  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 30;
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

  XCloseDisplay(tmp_dpy);

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
    bu_log("dm-Ogl: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
    bu_vls_free(&init_proc_vls);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S\n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy =
    Tk_Display(((struct dm_xvars *)dmp->dm_vars.pub_vars)->top);

  Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
		     dmp->dm_width,
		     dmp->dm_height);

  /* must do this before MakeExist */
  if((((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip=ogl_choose_visual(dmp,
				    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)) == NULL){
    bu_log("ogl_open: Can't get an appropriate visual.\n");
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->depth = ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.depth;

  Tk_MakeWindowExist(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win =
    Tk_WindowId(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  dmp->dm_id = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win;

  /* open GLX context */
  if ((((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc =
       glXCreateContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
			(GLXContext)NULL, GL_TRUE))==NULL) {
    bu_log("ogl_open: couldn't create glXContext.\n");
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  /* If we used an indirect context, then as far as sgi is concerned,
   * gl hasn't been used.
   */
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->is_direct =
    (char) glXIsDirect(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc);

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list =
    (XDeviceInfoPtr)XListInputDevices(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				      &ndevices);

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  bu_log("ogl_open: Couldn't open the dials+buttons\n");
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
#endif
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

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_open: Couldn't make context current\n");
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  /* display list (fontOffset + char) will display a given ASCII char */
  if ((((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0){
    bu_log("dm-ogl: Can't make display lists for font.\n");
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  /* This is the applications display list offset */
  dmp->dm_displaylist = ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset + 128;

  ogl_setBGColor(dmp, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer)
    glDrawBuffer(GL_BACK);
  else
    glDrawBuffer(GL_FRONT);

  /* do viewport, ortho commands and initialize font */
  (void)ogl_configureWin_guts(dmp, 1);

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
  glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity(); 
  glTranslatef(0.0, 0.0, -1.0); 
  glPushMatrix();
  glLoadIdentity();
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */

  Tk_MapWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
  return dmp;
}

/*
 */
int
ogl_share_dlist(dmp1, dmp2)
struct dm *dmp1;
struct dm *dmp2;
{
  GLfloat backgnd[4];
  GLfloat vf;
  GLXContext old_glxContext;

  if (dmp1 == (struct dm *)NULL)
    return TCL_ERROR;

  if (dmp2 == (struct dm *)NULL) {
    /* create a new graphics context for dmp1 with private display lists */

    old_glxContext = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc;

    if ((((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc =
	 glXCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->vip,
			  (GLXContext)NULL, GL_TRUE))==NULL) {
      bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
      ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    if (!glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp1->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc)){
      bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
      ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0){
      bu_log("dm-ogl: Can't make display lists for font.\nUsing old context\n.");
      ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    /* This is the applications display list offset */
    dmp1->dm_displaylist = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset + 128;

    ogl_setBGColor(dmp1, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (((struct ogl_vars *)dmp1->dm_vars.priv_vars)->mvars.doublebuffer)
      glDrawBuffer(GL_BACK);
    else
      glDrawBuffer(GL_FRONT);

    /* this is important so that ogl_configureWin knows to set the font */
    ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->fontstruct = NULL;

    /* do viewport, ortho commands and initialize font */
    (void)ogl_configureWin_guts(dmp1, 1);

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
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); 
    glTranslatef(0.0, 0.0, -1.0); 
    glPushMatrix();
    glLoadIdentity();
    ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

    /* destroy old context */
    glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, None, NULL);
    glXDestroyContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, old_glxContext);
  } else {
    /* dmp1 will share it's display lists with dmp2 */

    old_glxContext = ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc;

    if ((((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc =
	 glXCreateContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->vip,
			  ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->glxc,
			  GL_TRUE))==NULL) {
      bu_log("ogl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
      ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    if (!glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp2->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc)){
      bu_log("ogl_share_dlist: Couldn't make context current\nUsing old context\n.");
      ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

      return TCL_ERROR;
    }

    ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->fontOffset = ((struct ogl_vars *)dmp1->dm_vars.priv_vars)->fontOffset;
    dmp2->dm_displaylist = dmp1->dm_displaylist;

    ogl_setBGColor(dmp2, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (((struct ogl_vars *)dmp2->dm_vars.priv_vars)->mvars.doublebuffer)
      glDrawBuffer(GL_BACK);
    else
      glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)ogl_configureWin_guts(dmp2, 1);

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
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); 
    glTranslatef(0.0, 0.0, -1.0); 
    glPushMatrix();
    glLoadIdentity();
    ((struct ogl_vars *)dmp2->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

    /* destroy old context */
    glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, None, NULL);
    glXDestroyContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, old_glxContext);
  }

  return TCL_OK;
}

/*
 *  			O G L _ C L O S E
 *  
 *  Gracefully release the display.
 */
HIDDEN int
ogl_close(dmp)
struct dm *dmp;
{
  if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy){
    if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc){
      glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, None, NULL);
      glXDestroyContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc);
    }

    if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)
      XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);

    if(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
      Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

#if 0
    XCloseDisplay(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy);
#endif
  }

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_free(dmp->dm_vars.priv_vars, "ogl_close: ogl_vars");
  bu_free(dmp->dm_vars.pub_vars, "ogl_close: dm_xvars");
  bu_free(dmp, "ogl_close: dmp");

  return TCL_OK;
}

/*
 *			O G L _ D R A W B E G I N
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
ogl_drawBegin(dmp)
struct dm *dmp;
{
  GLfloat fogdepth;

  if (dmp->dm_debugLevel) {
    bu_log("ogl_drawBegin\n");

    if (ogl_actively_drawing)
	    bu_log("ogl_drawBegin: already actively drawing\n");
  }

  ogl_actively_drawing = 1;

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_drawBegin: Couldn't make context current\n");
    return TCL_ERROR;
  }

  if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
    if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on){
      glEnable(GL_FOG);
      /*XXX Need to do something with Viewscale */
      fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
      glFogf(GL_FOG_END, fogdepth);
      fogdepth = (GLfloat) (0.5*((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity/
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
 *			O G L _ D R A W E N D
 */
HIDDEN int
ogl_drawEnd(dmp)
struct dm *dmp;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_drawEnd\n");


#if USE_SURVICE_MODS
  if (dmp->dm_light) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLightfv(GL_LIGHT0, GL_POSITION, light0_direction);
  }
#endif

  if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer ){
    glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);
    /* give Graphics pipe time to work */
    glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  if(dmp->dm_debugLevel){
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

  ogl_actively_drawing = 0;
  return TCL_OK;
}

/*
 *  			O G L _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to ogl_drawVList().
 */
HIDDEN int
ogl_loadMatrix(dmp, mat, which_eye)
struct dm *dmp;
mat_t mat;
int which_eye;
{
  register fastf_t *mptr;
  GLfloat gtmat[16];
  mat_t	newm;
	
  if(dmp->dm_debugLevel){
    struct bu_vls tmp_vls;

    bu_log("ogl_loadMatrix()\n");

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
    ogl_drawString2D( dmp, "R", 0.986, 0.0, 0, 1 );
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
    mptr = mat;
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

#if !USE_SURVICE_MODS
  /* Make sure that new matrix is applied to the lights */
  if (dmp->dm_light) {
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    glLightfv(GL_LIGHT3, GL_POSITION, light3_position);
  }
#endif

  return TCL_OK;
}

/*
 *  			O G L _ D R A W V L I S T
 *  
 */
HIDDEN int
ogl_drawVList(dmp, vp)
     struct dm			*dmp;
     register struct rt_vlist	*vp;
{
	register struct rt_vlist	*tvp;
	int				first;
#if USE_VECTOR_THRESHHOLD
	static int			nvectors = 0;
#endif

	if (dmp->dm_debugLevel)
		bu_log("ogl_drawVList()\n");

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
				glBegin(GL_LINE_STRIP);
				glVertex3dv(*pt);
				break;
			case RT_VLIST_POLY_START:
				/* Start poly marker & normal */
				if (first == 0)
					glEnd();
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
				bu_log("ogl_drawVList(): handle Tcl events\n");

			nvectors = 0;

			/* Handle events in the queue */
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

			if (dmp->dm_debugLevel)
				bu_log("ogl_drawVList(): handled Tcl events successfully\n");
		}
#endif
	}

	if (first == 0)
		glEnd();

	return TCL_OK;
}

/*
 *			O G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
HIDDEN int
ogl_normal(dmp)
struct dm *dmp;
{

  if (dmp->dm_debugLevel)
    bu_log("ogl_normal\n");

  if (!((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd( ((struct ogl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat );
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
    if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on)
      glDisable(GL_FOG);
    if (dmp->dm_light)
      glDisable(GL_LIGHTING);
  }

  return TCL_OK;
}

/*
 *			O G L _ D R A W S T R I N G 2 D
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
ogl_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
fastf_t x, y;
int size;
int use_aspect;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_drawString2D()\n");

  if(use_aspect)
    glRasterPos2f(x, y * dmp->dm_aspect);
  else
    glRasterPos2f(x, y);

  glListBase(((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
  glCallLists(strlen( str ), GL_UNSIGNED_BYTE,  str );

  return TCL_OK;
}


/*
 *			O G L _ D R A W L I N E 2 D
 *
 */
HIDDEN int
ogl_drawLine2D( dmp, x1, y1, x2, y2)
struct dm *dmp;
fastf_t x1, y1;
fastf_t x2, y2;
{
  
  if (dmp->dm_debugLevel)
    bu_log("ogl_drawLine2D()\n");

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
ogl_drawPoint2D(dmp, x, y)
struct dm *dmp;
fastf_t x, y;
{
  if (dmp->dm_debugLevel){
    bu_log("ogl_drawPoint2D():\n");
    bu_log("\tdmp: %ul\tx - %lf\ty - %lf\n", (unsigned long)dmp, x, y);
  }

  glBegin(GL_POINTS);
  glVertex2f(x, y);
  glEnd();

  return TCL_OK;
}


HIDDEN int
ogl_setFGColor(dmp, r, g, b, strict)
struct dm *dmp;
unsigned char r, g, b;
int strict;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_setFGColor()\n");

  dmp->dm_fg[0] = r;
  dmp->dm_fg[1] = g;
  dmp->dm_fg[2] = b;

  if(strict){
    glColor3ub( (GLubyte)r, (GLubyte)g, (GLubyte)b );
  }else{
    float material[4];
  
    if (dmp->dm_light) {
      /* Ambient = .2, Diffuse = .6, Specular = .2 */

      material[0] = ( r / 255.0) * .2;
      material[1] = ( g / 255.0) * .2;
      material[2] = ( b / 255.0) * .2;
      material[3] = 1.0;

#if USE_SURVICE_MODS
      glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material);
      glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material);
#else
      glMaterialfv(GL_FRONT, GL_AMBIENT, material);
      glMaterialfv(GL_FRONT, GL_SPECULAR, material);
#endif

      material[0] *= 3.0;
      material[1] *= 3.0;
      material[2] *= 3.0;

#if USE_SURVICE_MODS
      glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material);
#else
      glMaterialfv(GL_FRONT, GL_DIFFUSE, material);
#endif
    }else{
      glColor3ub( (GLubyte)r,  (GLubyte)g,  (GLubyte)b );
    }
  }

  return TCL_OK;
}

HIDDEN int
ogl_setBGColor(dmp, r, g, b)
struct dm *dmp;
unsigned char r, g, b;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_setBGColor()\n");

  dmp->dm_bg[0] = r;
  dmp->dm_bg[1] = g;
  dmp->dm_bg[2] = b;

  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

  if(((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer){
    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
      bu_log("ogl_setBGColor: Couldn't make context current\n");
      return TCL_ERROR;
    }

    glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);
    glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  return TCL_OK;
}

HIDDEN int
ogl_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_setLineAttr()\n");

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
ogl_debug(dmp, lvl)
struct dm *dmp;
int lvl;
{
  dmp->dm_debugLevel = lvl;

  return TCL_OK;
}

HIDDEN int
ogl_setWinBounds(dmp, w)
struct dm *dmp;
int w[6];
{
  return TCL_OK;
}

#define OGL_DO_STEREO 1
/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
HIDDEN XVisualInfo *
ogl_choose_visual(dmp, tkwin)
struct dm *dmp;
Tk_Window tkwin;
{
  XVisualInfo *vip, vitemp, *vibase, *maxvip;
#define NGOOD 256
  int good[NGOOD];
  int tries, baddepth;
  int num, i, j;
  int fail;

  /* requirements */
  int use;
  int rgba;
  int dbfr;

  /* desires */
  int m_zbuffer = 1; /* m_zbuffer - try to get zbuffer */
  int zbuffer;
#if OGL_DO_STEREO
  int m_stereo; /* m_stereo - try to get stereo */
  int stereo;

  /*XXX Need to do something with this */
  if( dmp->dm_stereo )  {
    m_stereo = 1;
  } else {
    m_stereo = 0;
  }
#endif

  bzero((void *)&vitemp, sizeof(XVisualInfo));
  /* Try to satisfy the above desires with a color visual of the
   * greatest depth */

  vibase = XGetVisualInfo(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  0, &vitemp, &num);

  while (1) {
    for (i=0, j=0, vip=vibase; i<num; i++, vip++){
      /* requirements */
      fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		   vip, GLX_USE_GL, &use);
      if (fail || !use)
	continue;

      fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		     vip, GLX_RGBA, &rgba);
      if (fail || !rgba)
	continue;

      fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		     vip, GLX_DOUBLEBUFFER,&dbfr);
      if (fail || !dbfr)
	continue;

      /* desires */
      if ( m_zbuffer ) {
	fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    vip, GLX_DEPTH_SIZE,&zbuffer);
	if (fail || !zbuffer)
	  continue;
      }

#if OGL_DO_STEREO
      if ( m_stereo ) {
	fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		     vip, GLX_STEREO, &stereo);
	if (fail || !stereo){
	  bu_log("ogl_choose_visual: failed visual - GLX_STEREO\n");
	  continue;
	}
      }
#endif

      /* this visual meets criteria */
      if(j >= NGOOD){
	bu_log("ogl_choose_visual: More than %d candidate visuals!\n", NGOOD);
	break;
      }
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

	((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap =
	  XCreateColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  RootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				     maxvip->screen), maxvip->visual, AllocNone);

	if (Tk_SetWindowVisual(tkwin,
			       maxvip->visual, maxvip->depth,
			       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)){

	  glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       maxvip, GLX_DEPTH_SIZE,
		       &((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.depth);
	  if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.depth > 0)
	    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf = 1;

	  return (maxvip); /* success */
	} else { 
	  /* retry with lesser depth */
	  baddepth = maxvip->depth;
	  XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);
	}
      }
    }

    /* if no success at this point, relax a desire and try again */

#if OGL_DO_STEREO
    if ( m_stereo ) {
      m_stereo = 0;
      bu_log("Stereo not available.\n");
      continue;
    }
#endif

    if ( m_zbuffer ) {
      m_zbuffer = 0;
      continue;
    }

    return (XVisualInfo *)NULL; /* failure */
  }
}


/* 
 *			O G L _ C O N F I G U R E W I N
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
ogl_configureWin_guts(dmp, force)
struct dm *dmp;
int force;
{
  GLint mm; 
  XWindowAttributes xwa;
  XFontStruct	*newfontstruct;

  if (dmp->dm_debugLevel)
    bu_log("ogl_configureWin_guts()\n");

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_configureWin_guts: Couldn't make context current\n");
    return TCL_ERROR;
  }

  XGetWindowAttributes( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win, &xwa );

  /* nothing to do */
  if (!force &&
      dmp->dm_height == xwa.height &&
      dmp->dm_width == xwa.width)
    return TCL_OK;
    
  dmp->dm_height = xwa.height;
  dmp->dm_width = xwa.width;
  dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

  if (dmp->dm_debugLevel) {
    bu_log("ogl_configureWin_guts()\n");
    bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
  }

  glViewport(0, 0, dmp->dm_width, dmp->dm_height);
#if 0
  glScissor(0,  0, (dmp->dm_width)+1,
	    (dmp->dm_height)+1);
#endif

  if(dmp->dm_zbuffer)
    ogl_setZBuffer(dmp, dmp->dm_zbuffer);

  ogl_setLight(dmp, dmp->dm_light);

  glClearColor(((struct ogl_vars *)dmp->dm_vars.priv_vars)->r,
	       ((struct ogl_vars *)dmp->dm_vars.priv_vars)->g,
	       ((struct ogl_vars *)dmp->dm_vars.priv_vars)->b,
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
    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	 XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	   XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  FONTBACK)) == NULL) {
	bu_log("ogl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
	return TCL_ERROR;
      }
    }
    glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		 0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
  }
		

  /* Always try to choose a the font that best fits the window size.
   */

  if (dmp->dm_width < 582) {
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					  FONT5)) != NULL ) {
	XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 679) {
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					  FONT6)) != NULL ) {
	XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 776) {
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					  FONT7)) != NULL ) {
	XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 873) {
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					  FONT8)) != NULL ) {
	XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
      }
    }
  } else {
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					  FONT9)) != NULL ) {
	XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
      }
    }
  }

  return TCL_OK;
}

HIDDEN int
ogl_configureWin(dmp)
struct dm *dmp;
{
  return ogl_configureWin_guts(dmp, 0);
}

HIDDEN int
ogl_setLight(dmp, lighting_on)
struct dm *dmp;
int lighting_on;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_lighting()\n");

  dmp->dm_light = lighting_on;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_setLight: Couldn't make context current\n");
    return TCL_ERROR;
  }

  if (!dmp->dm_light) {
    /* Turn it off */
    glDisable(GL_LIGHTING);
  } else {
    /* Turn it on */

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

#if USE_SURVICE_MODS
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
#else
    /* light positions specified in ogl_newrot */
    glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT3, GL_SPECULAR, light3_diffuse);
    glLightfv(GL_LIGHT3, GL_DIFFUSE, light3_diffuse);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_LIGHT3);
#endif
  }

  return TCL_OK;
}	

HIDDEN int
ogl_setZBuffer(dmp, zbuffer_on)
struct dm *dmp;
int zbuffer_on;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_setZBuffer:\n");

  dmp->dm_zbuffer = zbuffer_on;
  ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_setZBuffer: Couldn't make context current\n");
    return TCL_ERROR;
  }

  if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf == 0) {
    dmp->dm_zbuffer = 0;
    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
  }

  if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on) {
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }

  return TCL_OK;
}

int
ogl_beginDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_beginDList()\n");

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_beginDList: Couldn't make context current\n");
    return TCL_ERROR;
  }

  glNewList(dmp->dm_displaylist + list, GL_COMPILE);
  return TCL_OK;
}

int
ogl_endDList(dmp)
struct dm *dmp;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_endDList()\n");

  glEndList();
  return TCL_OK;
}

int
ogl_drawDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_drawDList()\n");

  glCallList(dmp->dm_displaylist + list);
  return TCL_OK;
}

int
ogl_freeDLists(dmp, list, range)
struct dm *dmp;
unsigned int list;
int range;
{
  if (dmp->dm_debugLevel)
    bu_log("ogl_freeDLists()\n");

  if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc)){
    bu_log("ogl_freeDLists: Couldn't make context current\n");
    return TCL_ERROR;
  }

  glDeleteLists(dmp->dm_displaylist + list, (GLsizei)range);
  return TCL_OK;
}
