/*                        D M - G L X . C
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
/** @file dm-glx.c
   Just a note:

   These particular commands should not be used in
   mixed mode programming.

   qdevice
   blkqread
   qtest
   getbutton
   getvaluator
   setvaluator
   unqdevice
   mapcolor
   gconfig
   doublebuffer
   RGBmode
   winopen
   foreground
   noborder
   keepaspect
   prefposition
*/
/*
 *			D M - G L X . C
 *
 *  This display manager started out with the guts from DM-4D which
 *  was modified to use mixed-mode (i.e. gl and X )
 *
 *  Authors -
 *      Robert G. Parker
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include "tk.h"

#ifdef HAVE_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif

#include <gl/glws.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <sys/invent.h>

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
#include "dm-glx.h"
#include "solid.h"


#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

extern Tk_Window tkwin;
extern inventory_t	*getinvent();
extern char ogl_ogl_used;
extern char ogl_sgi_used;

/* Display Manager package interface */

#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

/* These functions and variables are available to applications */
void    glx_configureWindowShape();
void	glx_lighting();
void	glx_zbuffer();
void glx_clearToBlack();

struct glx_vars head_glx_vars;
/* End of functions and variables that are available to applications */


static void set_window();
static XVisualInfo *extract_visual();
static unsigned long extract_value();

static void glx_var_init();
static void glx_make_materials();

struct dm	*glx_open();
static int	glx_close();
static int	glx_drawBegin(), glx_drawEnd();
static int	glx_normal(), glx_loadMatrix();
static int	glx_drawString2D(), glx_drawLine2D();
static int      glx_drawPoint2D();
static int	glx_drawVList();
static int      glx_setColor(), glx_setLineAttr();
static int	glx_setWinBounds(), glx_debug();
static int      glx_beginDList(), glx_endDList();
static int      glx_drawDList();
static int      glx_freeDLists();

static GLXconfig glx_config_wish_list [] = {
  { GLX_NORMAL, GLX_WINDOW, GLX_NONE },
  { GLX_NORMAL, GLX_VISUAL, GLX_NONE},
  { GLX_NORMAL, GLX_DOUBLE, TRUE },
  { GLX_NORMAL, GLX_RGB, TRUE },
  { GLX_NORMAL, GLX_RGBSIZE, GLX_NOCONFIG},
  { GLX_NORMAL, GLX_ZSIZE, GLX_NOCONFIG },
  { 0, 0, 0 }
};

static double	xlim_view = 1.0;	/* args for ortho() */
static double	ylim_view = 1.0;

struct dm dm_glx = {
  glx_close,
  glx_drawBegin,
  glx_drawEnd,
  glx_normal,
  glx_loadMatrix,
  glx_drawString2D,
  glx_drawLine2D,
  glx_drawPoint2D,
  glx_drawVList,
  glx_setColor,
  glx_setLineAttr,
  glx_setWinBounds,
  glx_debug,
  glx_beginDList,
  glx_endDList,
  glx_drawDList,
  glx_freeDLists,
  0,
  1,			/* has displaylist */
  0,                    /* no stereo by default */
  IRBOUND,
  "glx",
  "SGI - mixed mode",
  DM_TYPE_GLX,
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

/*
 *			G L X _ O P E N
 *
 */
struct dm *
glx_open(argc, argv)
int argc;
char *argv[];
{
  static int count = 0;
  register int	i;
  Matrix		m;
  inventory_t	*inv;
  struct bu_vls str;
  struct bu_vls init_proc_vls;
  int j, k;
  int ndevices;
  int nclass = 0;
  int make_square = -1;
  int unused;
  XDeviceInfoPtr olist = NULL, list = NULL;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  GLXconfig *p, *glx_config;
  XVisualInfo *visual_info;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_glx;  /* struct copy */

  /* Only need to do this once for this display manager */
  if(!count){
    bzero((void *)&head_glx_vars, sizeof(struct glx_vars));
    BU_LIST_INIT( &head_glx_vars.l );
  }

  BU_GETSTRUCT(dmp->dm_vars, glx_vars);
  if(dmp->dm_vars == (genptr_t)NULL){
    bu_free(dmp, "glx_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  i = dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

  if(bu_vls_strlen(&dmp->dm_pathName) == 0)
     bu_vls_printf(&dmp->dm_pathName, ".dm_glx%d", count);

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
  ((struct glx_vars *)dmp->dm_vars)->devmotionnotify = LASTEvent;
  ((struct glx_vars *)dmp->dm_vars)->devbuttonpress = LASTEvent;
  ((struct glx_vars *)dmp->dm_vars)->devbuttonrelease = LASTEvent;

  /* initialize modifiable variables */
  ((struct glx_vars *)dmp->dm_vars)->mvars.zclipping_on = 1;       /* Z Clipping flag */
  ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on = 1;         /* Hardware Z buffer is on */

  BU_LIST_APPEND(&head_glx_vars.l, &((struct glx_vars *)dmp->dm_vars)->l);

#ifdef DM_OGL
  /* This is a hack to handle the fact that the sgi attach crashes
   * if a direct OpenGL context has been previously opened in the
   * current mged session. This stops the attach before it crashes.
   */
  if (ogl_ogl_used){
    Tcl_AppendResult(interp, "Can't attach sgi, because a direct OpenGL context has\n",
		     "previously been opened in this session. To use sgi,\n",
		     "quit this session and reopen it.\n", (char *)NULL);
    bu_vls_free(&init_proc_vls);
    (void)glx_close(dmp);
    return DM_NULL;
  }
  ogl_sgi_used = 1;
#endif /* DM_OGL */

  if(dmp->dm_top){
    ((struct glx_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindowFromPath(interp, tkwin,
			      bu_vls_addr(&dmp->dm_pathName),
			      bu_vls_addr(&dmp->dm_dName));
    ((struct glx_vars *)dmp->dm_vars)->top = ((struct glx_vars *)dmp->dm_vars)->xtkwin;
  }else{
    char *cp;

    cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
    if(cp == bu_vls_addr(&dmp->dm_pathName)){
      ((struct glx_vars *)dmp->dm_vars)->top = tkwin;
    }else{
      struct bu_vls top_vls;

      bu_vls_init(&top_vls);
      bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
		    bu_vls_addr(&dmp->dm_pathName));
      ((struct glx_vars *)dmp->dm_vars)->top =
	Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
      bu_vls_free(&top_vls);
    }

    /* Make xtkwin an embedded window */
    ((struct glx_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindow(interp, ((struct glx_vars *)dmp->dm_vars)->top,
		      cp + 1, (char *)NULL);
  }

  if(((struct glx_vars *)dmp->dm_vars)->xtkwin == NULL){
    Tcl_AppendResult(interp, "dm-X: Failed to open ",
		     bu_vls_addr(&dmp->dm_pathName),
		     "\n", (char *)NULL);
    bu_vls_free(&init_proc_vls);
    (void)glx_close(dmp);
    return DM_NULL;
  }

  {
    int return_val;

    if(!XQueryExtension(Tk_Display(((struct glx_vars *)dmp->dm_vars)->top), "SGI-GLX",
			&return_val, &return_val, &return_val)){
      bu_vls_free(&init_proc_vls);
      (void)glx_close(dmp);
      return DM_NULL;
    }
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct glx_vars *)dmp->dm_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S \n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);
    (void)glx_close(dmp);

    return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct glx_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct glx_vars *)dmp->dm_vars)->top);

  if(dmp->dm_width == 0){
    dmp->dm_width =
      DisplayWidth(((struct glx_vars *)dmp->dm_vars)->dpy,
		   DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 30;
    ++make_square;
  }

  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(((struct glx_vars *)dmp->dm_vars)->dpy,
		    DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 30;
    ++make_square;
  }

  if(make_square > 0){
    /* Make window square */
    if( dmp->dm_height <
	dmp->dm_width )
      dmp->dm_width = dmp->dm_height;
    else /* we have a funky shaped monitor */
      dmp->dm_height = dmp->dm_width;
  }

  Tk_GeometryRequest(((struct glx_vars *)dmp->dm_vars)->xtkwin,
		     dmp->dm_width,
		     dmp->dm_height);

  glx_config = GLXgetconfig(((struct glx_vars *)dmp->dm_vars)->dpy,
			    Tk_ScreenNumber(((struct glx_vars *)dmp->dm_vars)->xtkwin),
			    glx_config_wish_list);
  ((struct glx_vars *)dmp->dm_vars)->vip = extract_visual(dmp, GLX_NORMAL, glx_config);
  ((struct glx_vars *)dmp->dm_vars)->depth = ((struct glx_vars *)dmp->dm_vars)->vip->depth;
  ((struct glx_vars *)dmp->dm_vars)->cmap = extract_value(GLX_NORMAL, GLX_COLORMAP,
							   glx_config);
  if(Tk_SetWindowVisual(((struct glx_vars *)dmp->dm_vars)->xtkwin,
			((struct glx_vars *)dmp->dm_vars)->vip->visual,
			((struct glx_vars *)dmp->dm_vars)->depth,
			((struct glx_vars *)dmp->dm_vars)->cmap) == 0){
    (void)glx_close(dmp);
    return DM_NULL;
  }

  Tk_MakeWindowExist(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  ((struct glx_vars *)dmp->dm_vars)->win =
    Tk_WindowId(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  dmp->dm_id = (unsigned long)((struct glx_vars *)dmp->dm_vars)->win;
  set_window(GLX_NORMAL, ((struct glx_vars *)dmp->dm_vars)->win, glx_config);

  /* Inform the GL that you intend to render GL into an X window */
  if(GLXlink(((struct glx_vars *)dmp->dm_vars)->dpy, glx_config) < 0){
    (void)glx_close(dmp);
    return DM_NULL;
  }

  GLXwinset(((struct glx_vars *)dmp->dm_vars)->dpy,
	    ((struct glx_vars *)dmp->dm_vars)->win);

  /* set configuration variables */
  for(p = glx_config; p->buffer; ++p){
    switch(p->buffer){
    case GLX_NORMAL:
      switch(p->mode){
      case GLX_ZSIZE:
	if(p->arg)
	  ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf = 1;
	else
	  ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf = 0;

	break;
      case GLX_RGB:
	if(p->arg)
	  ((struct glx_vars *)dmp->dm_vars)->mvars.rgb = 1;
	else
	  ((struct glx_vars *)dmp->dm_vars)->mvars.rgb = 0;

	break;
      case GLX_DOUBLE:
	if(p->arg)
	  ((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer = 1;
	else
	  ((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer = 0;

	break;
      case GLX_STEREOBUF:
	((struct glx_vars *)dmp->dm_vars)->stereo_is_on = 1;

	break;
      case GLX_BUFSIZE:
      case GLX_STENSIZE:
      case GLX_ACSIZE:
      case GLX_VISUAL:
      case GLX_COLORMAP:
      case GLX_WINDOW:
      case GLX_MSSAMPLE:
      case GLX_MSZSIZE:
      case GLX_MSSSIZE:
      case GLX_RGBSIZE:
      default:
	break;
      }
    case GLX_OVERLAY:
    case GLX_POPUP:
    case GLX_UNDERLAY:
    default:
      break;
    }
  }

  free((void *)glx_config);

  winconstraints();	/* remove constraints on the window size */

  /*
   * Establish GL library operating modes
   */
  /* Don't draw polygon edges */
  glcompat( GLC_OLDPOLYGON, 0 );

  /* Z-range mapping */
  /* Z range from getgdesc(GD_ZMIN)
   * to getgdesc(GD_ZMAX).
   * Hardware specific.
   */
  glcompat( GLC_ZRANGEMAP, 0 );

  /* Take off a smidgeon for wraparound, as suggested by SGI manual */

  ((struct glx_vars *)dmp->dm_vars)->mvars.min_scr_z = getgdesc(GD_ZMIN)+15;
  ((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z = getgdesc(GD_ZMAX)-15;

  if( ((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer){
    /* Clear out image from windows underneath */
    frontbuffer(1);
    glx_clearToBlack(dmp);
    frontbuffer(0);
    glx_clearToBlack(dmp);
  }

  glx_configureWindowShape(dmp);

  /* Line style 0 is solid.  Program line style 1 as dot-dashed */
  deflinestyle( 1, 0xCF33 );
  setlinestyle( 0 );

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  if (XQueryExtension(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, "XInputExtension", &unused, &unused, &unused)) {
      olist = list = (XDeviceInfoPtr)XListInputDevices(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, &ndevices);
  }

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct glx_vars *)dmp->dm_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp, "glx_open: Couldn't open the dials+buttons\n",
			   (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#ifdef IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, ((struct glx_vars *)dmp->dm_vars)->devbuttonpress,
			      e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, ((struct glx_vars *)dmp->dm_vars)->devbuttonrelease,
				e_class[nclass]);
	    ++nclass;
#endif
	    break;
#ifdef IR_KNOBS
	  case ValuatorClass:
	    DeviceMotionNotify(dev, ((struct glx_vars *)dmp->dm_vars)->devmotionnotify,
			       e_class[nclass]);
	    ++nclass;
	    break;
#endif
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(((struct glx_vars *)dmp->dm_vars)->dpy,
			      ((struct glx_vars *)dmp->dm_vars)->win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);

  Tk_MapWindow(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  return dmp;
}


/*
 *			G L X _ C O N F I G U R E W I N D O W S H A P E
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 */
void
glx_configureWindowShape(dmp)
struct dm *dmp;
{
  int		npix;
  XWindowAttributes xwa;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_configureWindowShape()\n", (char *)NULL);

  XGetWindowAttributes( ((struct glx_vars *)dmp->dm_vars)->dpy,
			((struct glx_vars *)dmp->dm_vars)->win, &xwa );
  dmp->dm_width = xwa.width;
  dmp->dm_height = xwa.height;

  /* Write enable all the bloody bits after resize! */
  viewport(0, dmp->dm_width, 0,
	   dmp->dm_height);

  if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf )
    glx_zbuffer(dmp);

  glx_lighting(dmp);
  glx_clearToBlack(dmp);

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
  dmp->dm_aspect =
    (fastf_t)dmp->dm_height/
    (fastf_t)dmp->dm_width;
}

/*
 *  			G L X _ C L O S E
 *
 *  Gracefully release the display.  Well, mostly gracefully -- see
 *  the comments in the open routine.
 */
static int
glx_close(dmp)
struct dm *dmp;
{
  if(((struct glx_vars *)dmp->dm_vars)->dpy){
    if(((struct glx_vars *)dmp->dm_vars)->mvars.cueing_on)
      depthcue(0);

    lampoff( 0xf );

    /* avoids error messages when reattaching */
    mmode(MVIEWING);
    lmbind(LIGHT2,0);
    lmbind(LIGHT3,0);
    lmbind(LIGHT4,0);
    lmbind(LIGHT5,0);

    frontbuffer(1);
    glx_clearToBlack(dmp);
    frontbuffer(0);

    GLXunlink(((struct glx_vars *)dmp->dm_vars)->dpy,
	      ((struct glx_vars *)dmp->dm_vars)->win);
  }

  if(((struct glx_vars *)dmp->dm_vars)->xtkwin != NULL)
    Tk_DestroyWindow(((struct glx_vars *)dmp->dm_vars)->xtkwin);

  if(((struct glx_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct glx_vars *)dmp->dm_vars)->l);

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_free(dmp->dm_vars, "glx_close: glx_vars");
  bu_free(dmp, "glx_close: dmp");

  return TCL_OK;
}

/*
 *			G L X _ D R A W B E G I N
 *
 * Define the world, and include in it instances of all the
 * important things.
 */
static int
glx_drawBegin(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawBegin()\n", (char *)NULL);

  if(GLXwinset(((struct glx_vars *)dmp->dm_vars)->dpy,
	       ((struct glx_vars *)dmp->dm_vars)->win) < 0){
    Tcl_AppendResult(interp, "glx_drawBegin: GLXwinset() failed\n", (char *)NULL);
    return TCL_ERROR;
  }

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );

  if( !((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer )
    glx_clearToBlack(dmp);

  return TCL_OK;
}

/*
 *			G L X _ D R A W E N D
 *
 * End the drawing sequence.
 */
static int
glx_drawEnd(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawEnd()\n", (char *)NULL);

  /*
   * A Point, in the Center of the Screen.
   * This is drawn last, to always come out on top.
   */
  glx_drawLine2D( dmp, 0, 0, 0, 0, 0 );
  /* End of faceplate */

  if(((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer){
    swapbuffers();
    /* give Graphics pipe time to work */
    glx_clearToBlack(dmp);
  }

  return TCL_OK;
}

/*
 *			G L X _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
static int
glx_normal(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_normal()\n", (char *)NULL);

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );

  return TCL_OK;
}

/*
 *  			G L X _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to glx_drawVList().
 *
 *  IMPORTANT COORDINATE SYSTEM NOTE:
 *
 *  MGED uses a right handed coordinate system where +Z points OUT of
 *  the screen.  The Silicon Graphics uses a left handed coordinate
 *  system where +Z points INTO the screen.
 *  This difference in convention is handled here.
 *  The conversion is accomplished by concatenating a matrix multiply
 *  by
 *            (  1    0    0   0  )
 *            (  0    1    0   0  )
 *            (  0    0   -1   0  )
 *            (  0    0    0   1  )
 *
 *  However, this is actually implemented by straight-line code which
 *  flips the sign on the entire third row.
 *
 *  Note that through BRL-CAD Release 3.7 this was handled by flipping
 *  the direction of the shade ramps.  Now, with the Z-buffer being used,
 *  the correct solution is important.
 */
static int
glx_loadMatrix(dmp, mat, which_eye)
struct dm *dmp;
mat_t	mat;
int which_eye;
{
  register fastf_t *mptr;
  Matrix	gtmat;
  mat_t	newm;
  int	i;

  if(((struct glx_vars *)dmp->dm_vars)->mvars.debug){
    struct bu_vls tmp_vls;

    Tcl_AppendResult(interp, "glx_loadMatrix()\n", (char *)NULL);

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

  switch(which_eye)  {
  case 0:
    /* Non-stereo */
    break;
  case 1:
    /* R eye */
    viewport(0, XMAXSCREEN, 0, YSTEREO);
    glx_drawString2D( dmp, "R", 2020, 0, 0, DM_RED );
    break;
  case 2:
    /* L eye */
    viewport(0, XMAXSCREEN, 0+YOFFSET_LEFT, YSTEREO+YOFFSET_LEFT);
    break;
  }

  if( ! ((struct glx_vars *)dmp->dm_vars)->mvars.zclipping_on ) {
    mat_t	nozclip;

    MAT_IDN( nozclip );
    nozclip[10] = 1.0e-20;
    bn_mat_mul( newm, nozclip, mat );
    mptr = newm;
  } else {
    mptr = mat;
  }

  gtmat[0][0] = *(mptr++) * dmp->dm_aspect;
  gtmat[1][0] = *(mptr++) * dmp->dm_aspect;
  gtmat[2][0] = *(mptr++) * dmp->dm_aspect;
  gtmat[3][0] = *(mptr++) * dmp->dm_aspect;

  gtmat[0][1] = *(mptr++);
  gtmat[1][1] = *(mptr++);
  gtmat[2][1] = *(mptr++);
  gtmat[3][1] = *(mptr++);

  gtmat[0][2] = *(mptr++);
  gtmat[1][2] = *(mptr++);
  gtmat[2][2] = *(mptr++);
  gtmat[3][2] = *(mptr++);

  gtmat[0][3] = *(mptr++);
  gtmat[1][3] = *(mptr++);
  gtmat[2][3] = *(mptr++);
  gtmat[3][3] = *(mptr++);

  /*
   *  Convert between MGED's right handed coordinate system
   *  where +Z comes out of the screen to the Silicon Graphics's
   *  left handed coordinate system, where +Z goes INTO the screen.
   */
  gtmat[0][2] = -gtmat[0][2];
  gtmat[1][2] = -gtmat[1][2];
  gtmat[2][2] = -gtmat[2][2];
  gtmat[3][2] = -gtmat[3][2];

  loadmatrix( gtmat );
  return TCL_OK;
}

static float material_objdef[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,	/* 0.4 in rt */
	DIFFUSE,	0.6, 0.6, 0.6,
	SPECULAR,	0.2, 0.2, 0.2,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	10.0,
	LMNULL   };

/*
 *  			G L X _ O B J E C T
 *
 */
static int
glx_drawVList( dmp, vp )
struct dm *dmp;
register struct rt_vlist *vp;
{
  register struct rt_vlist	*tvp;
  register int nvec;
  register float	*gtvec;
  char	gtbuf[16+3*sizeof(double)];
  int first;
  int i,j;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawVList()\n", (char *)NULL);

  /*
   *  It is claimed that the "dancing vector disease" of the
   *  4D GT processors is due to the array being passed to v3f()
   *  not being quad-word aligned (16-byte boundary).
   *  This hack ensures that the buffer has this alignment.
   *  Note that this requires gtbuf to be 16 bytes longer than needed.
   */
  gtvec = (float *)((((int)gtbuf)+15) & (~0xF));

  /* Viewing region is from -1.0 to +1.0 */
  first = 1;
  for( BU_LIST_FOR( tvp, rt_vlist, &vp->l ) )  {
    register int	i;
    register int	nused = tvp->nused;
    register int	*cmd = tvp->cmd;
    register point_t *pt = tvp->pt;
    for( i = 0; i < nused; i++,cmd++,pt++ )  {
      switch( *cmd )  {
      case RT_VLIST_LINE_MOVE:
	/* Move, start line */
	  if( first == 0 )
	    endline();
	  first = 0;
	  bgnline();
	  v3d( *pt );
	  break;
      case RT_VLIST_LINE_DRAW:
	/* Draw line */
	v3d( *pt );
	break;
      case RT_VLIST_POLY_START:
	/* Start poly marker & normal */
	if( first == 0 )
	  endline();
	/* concave(TRUE); */
	bgnpolygon();
	/* Set surface normal (vl_pnt points outward) */
	VMOVE( gtvec, *pt );
	n3f(gtvec);
	break;
      case RT_VLIST_POLY_MOVE:
	/* Polygon Move */
	v3d( *pt );
	break;
      case RT_VLIST_POLY_DRAW:
	/* Polygon Draw */
	v3d( *pt );
	break;
      case RT_VLIST_POLY_END:
	/* Draw, End Polygon */
	v3d( *pt );
	endpolygon();
	first = 1;
	break;
      case RT_VLIST_POLY_VERTNORM:
	/* Set per-vertex normal.  Given before vert. */
	VMOVE( gtvec, *pt );
	n3f(gtvec);
	break;
      }
    }
  }

  if( first == 0 )
    endline();

  return TCL_OK;
}


/*
 *			G L X _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
glx_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x,y;
int size;
int use_aspect;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawString2D()\n", (char *)NULL);

  if(use_aspect)
    cmov2( GED2IRIS(x) * dmp->dm_aspect, GED2IRIS(y));
  else
    cmov2( GED2IRIS(x), GED2IRIS(y));

  charstr( str );

  return TCL_OK;
}

/*
 *			G L X _ D R A W L I N E 2 D
 *
 */
static int
glx_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  register int nvec;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawLine2D()\n", (char *)NULL);

  move2( GED2IRIS(x1), GED2IRIS(y1));
  draw2( GED2IRIS(x2), GED2IRIS(y2));

  return TCL_OK;
}

/*
 *                      G L X _ D R A W P O I N T 2 D
 *
 */
static int
glx_drawPoint2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawPoint2D()\n", (char *)NULL);

  return glx_drawLine2D(dmp, x, y, x, y);
}

/*
 *                      G L X _ S E T C O L O R
 *
 */
static int
glx_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_setColor()\n", (char *)NULL);

  /*
   * IMPORTANT DEPTHCUEING NOTE
   *
   * Also note that the depthcueing shaderange() routine wanders
   * outside it's allotted range due to roundoff errors.  A buffer
   * entry is kept on each end of the shading curves, and the
   * highlight mode uses the *next* to the brightest entry --
   * otherwise it can (and does) fall off the shading ramp.
   */

  if(strict){
    if(((struct glx_vars *)dmp->dm_vars)->mvars.cueing_on) {
      lRGBrange(r, g, b, r, g, b,
		((struct glx_vars *)dmp->dm_vars)->mvars.min_scr_z,
		((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z );
    }

    RGBcolor( r, g, b );
  }else{
    if(((struct glx_vars *)dmp->dm_vars)->mvars.cueing_on)  {
      lRGBrange(r/10, g/10, b/10, r, g, b,
		((struct glx_vars *)dmp->dm_vars)->mvars.min_scr_z,
		((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z );
    } else if(((struct glx_vars *)dmp->dm_vars)->mvars.lighting_on) {
      /* Ambient = .2, Diffuse = .6, Specular = .2 */

      /* Ambient */
      material_objdef[3] = 	.2 * ( r / 255.0);
      material_objdef[4] = 	.2 * ( g / 255.0);
      material_objdef[5] = 	.2 * ( b / 255.0);

      /* diffuse */
      material_objdef[7] = 	.6 * ( r / 255.0);
      material_objdef[8] = 	.6 * ( g / 255.0);
      material_objdef[9] = 	.6 * ( b / 255.0);

      /* Specular */
      material_objdef[11] = 	.2 * ( r / 255.0);
      material_objdef[12] = 	.2 * ( g / 255.0);
      material_objdef[13] = 	.2 * ( b / 255.0);

      lmdef(DEFMATERIAL, 21, 0, material_objdef);
      lmbind(MATERIAL, 21);
    } else
      RGBcolor( r, g, b );
  }

  return TCL_OK;
}

/*
 *                         G L X _ S E T L I N E A T T R
 *
 */
static int
glx_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_setLineAttr()\n", (char *)NULL);

  dmp->dm_lineWidth = width;
  dmp->dm_lineStyle = style;

  linewidth(width);

  if(style == DM_DASHED_LINE)
    setlinestyle(1);
  else
    setlinestyle(0);

  return TCL_OK;
}

/*
 *                         G L X _ D E B U G
 *
 */
static int
glx_debug(dmp, lvl)
struct dm *dmp;
int lvl;
{
  ((struct glx_vars *)dmp->dm_vars)->mvars.debug = lvl;

  return TCL_OK;
}

/*
 *                         G L X _ S E T W I N B O U N D S
 *
 */
static int
glx_setWinBounds(dmp, w)
struct dm *dmp;
int w[6];
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_setWinBounds()\n", (char *)NULL);

  return TCL_OK;
}

/*
 *                         G L X _ Z B U F F E R
 *
 */
void
glx_zbuffer(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_zbuffer()\n", (char *)NULL);

  if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf == 0 )  {
    Tcl_AppendResult(interp, "dm-4d: This machine has no Zbuffer to enable\n",
		     (char *)NULL);
    ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on = 0;
  }

  zbuffer( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on );

  if(((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on)  {
    /* Set screen coords of near and far clipping planes */
    lsetdepth(((struct glx_vars *)dmp->dm_vars)->mvars.min_scr_z,
	      ((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z);
  }
}

void
glx_clearToBlack(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_clearToBlack()\n", (char *)NULL);

  /* Re-enable the full viewport */
  viewport(0, dmp->dm_width, 0,
	   dmp->dm_height);

  if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on )  {
    zfunction( ZF_LEQUAL );
    if( ((struct glx_vars *)dmp->dm_vars)->mvars.rgb )  {
      czclear( 0x000000, ((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z );
    } else {
      czclear( BLACK, ((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z );
    }
  } else {
    if( ((struct glx_vars *)dmp->dm_vars)->mvars.rgb )  {
      RGBcolor( (short)0, (short)0, (short)0 );
    } else {
      color(BLACK);
    }
    clear();
  }
}

#if 0
/* Handy fakeouts when we don't want to link with -lmpc */
usinit()	{
	bu_log("usinit\n");
}
usnewlock()	{
	bu_log("usnewlock\n");
}
taskcreate()	{
	bu_log("taskcreate\n");
}
#endif

/*
 *                            G L X _ L I G H T I N G
 *
 *  The struct_parse will change the value of the variable.
 *  Just implement it, here.
 */
void
glx_lighting(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_lighting()\n", (char *)NULL);

  if( !((struct glx_vars *)dmp->dm_vars)->mvars.lighting_on )  {
    /* Turn it off */
    mmode(MVIEWING);
    lmbind(MATERIAL,0);
    lmbind(LMODEL,0);
    mmode(MSINGLE);
  } else {
    /* Turn it on */
    if( ((struct glx_vars *)dmp->dm_vars)->mvars.cueing_on )  {
      /* Has to be off for lighting */
      ((struct glx_vars *)dmp->dm_vars)->mvars.cueing_on = 0;
#if 0
      glx_colorchange(dmp);
#else
      depthcue(0);
#endif
    }

    mmode(MVIEWING);

    glx_make_materials();	/* Define material properties */

    lmbind(LMODEL, 2);	/* infinite */
    lmbind(LIGHT2,2);
    lmbind(LIGHT3,3);
    lmbind(LIGHT4,4);
    lmbind(LIGHT5,5);

    /* RGB color commands & lighting */
    lmcolor( LMC_COLOR );

    mmode(MSINGLE);
  }
}

/*
 *  Some initial lighting model stuff
 *  Really, MGED needs to derrive it's lighting from the database,
 *  but for now, this hack will suffice.
 *
 *  For materials, the definitions are:
 *	ALPHA		opacity.  1.0=opaque
 *	AMBIENT		ambient reflectance of the material  0..1
 *	DIFFUSE		diffuse reflectance of the material  0..1
 *	SPECULAR	specular reflectance of the material  0..1
 *	EMISSION	emission color ???
 *	SHININESS	specular scattering exponent, integer 0..128
 */
static float material_default[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,
	DIFFUSE,	0.8, 0.8, 0.8,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	0.0,
	SPECULAR,	0.0, 0.0, 0.0,
	LMNULL   };

/* Something like the RT default phong material */
static float material_rtdefault[] = {
	ALPHA,		1.0,
	AMBIENT,	0.2, 0.2, 0.2,	/* 0.4 in rt */
	DIFFUSE,	0.6, 0.6, 0.6,
	SPECULAR,	0.2, 0.2, 0.2,
	EMISSION,	0.0, 0.0, 0.0,
	SHININESS,	10.0,
	LMNULL   };

/* This was the "default" material in the demo */
static float material_xdefault[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_brass[] = {
	AMBIENT, 0.35, 0.25,  0.1,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 5.0,
	LMNULL   };

static float mat_shinybrass[] = {
	AMBIENT, 0.25, 0.15, 0.0,
	DIFFUSE, 0.65, 0.5, 0.35,
	SPECULAR, 0.9, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_pewter[] = {
	AMBIENT, 0.0, 0.0,  0.0,
	DIFFUSE, 0.6, 0.55 , 0.65,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 10.0,
	LMNULL   };

static float mat_silver[] = {
	AMBIENT, 0.4, 0.4,  0.4,
	DIFFUSE, 0.3, 0.3, 0.3,
	SPECULAR, 0.9, 0.9, 0.95,
	SHININESS, 30.0,
	LMNULL   };

static float mat_gold[] = {
	AMBIENT, 0.4, 0.2, 0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.7, 0.7, 0.0,
	SHININESS, 10.0,
	LMNULL   };

static float mat_shinygold[] = {
	AMBIENT, 0.4, 0.2,  0.0,
	DIFFUSE, 0.9, 0.5, 0.0,
	SPECULAR, 0.9, 0.9, 0.0,
	SHININESS, 20.0,
	LMNULL   };

static float mat_plaster[] = {
	AMBIENT, 0.2, 0.2,  0.2,
	DIFFUSE, 0.95, 0.95, 0.95,
	SPECULAR, 0.0, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL   };

static float mat_redplastic[] = {
	AMBIENT, 0.3, 0.1, 0.1,
	DIFFUSE, 0.5, 0.1, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenplastic[] = {
	AMBIENT, 0.1, 0.3, 0.1,
	DIFFUSE, 0.1, 0.5, 0.1,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_blueplastic[] = {
	AMBIENT, 0.1, 0.1, 0.3,
	DIFFUSE, 0.1, 0.1, 0.5,
	SPECULAR, 0.45, 0.45, 0.45,
	SHININESS, 30.0,
	LMNULL   };

static float mat_greenflat[] = {
	EMISSION,   0.0, 0.4, 0.0,
	AMBIENT,    0.0, 0.0, 0.0,
	DIFFUSE,    0.0, 0.0, 0.0,
	SPECULAR,   0.0, 0.6, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_greenshiny[]= {
	EMISSION, 0.0, 0.4, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.25, 0.9, 0.25,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueflat[] = {
	EMISSION, 0.0, 0.0, 0.4,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.0, 0.5, 0.5,
	SPECULAR,  0.0, 0.0, 0.9,
	SHININESS, 10.0,
	LMNULL
};

static float mat_blueshiny[] = {
	EMISSION, 0.0, 0.0, 0.6,
	AMBIENT,  0.1, 0.25, 0.5,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_redflat[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 1.0,
	LMNULL
};

static float mat_redshiny[] = {
	EMISSION, 0.60, 0.0, 0.0,
	AMBIENT,  0.1, 0.25, 0.1,
	DIFFUSE,  0.5, 0.5, 0.5,
	SPECULAR,  0.5, 0.0, 0.0,
	SHININESS, 10.0,
	LMNULL
};

static float mat_beigeshiny[] = {
	EMISSION, 0.5, 0.5, 0.6,
	AMBIENT,  0.35, 0.35, 0.0,
	DIFFUSE,  0.5, 0.5, 0.0,
	SPECULAR,  0.5, 0.5, 0.0,
	SHININESS, 10.0,
	LMNULL
};

/*
 *  Meanings of the parameters:
 *	AMBIENT		ambient light associated with this source ???, 0..1
 *	LCOLOR		light color, 0..1
 *	POSITION	position of light.  w=0 for infinite lights
 */
static float default_light[] = {
	AMBIENT,	0.0, 0.0, 0.0,
	LCOLOR,		1.0, 1.0, 1.0,
	POSITION,	0.0, 0.0, 1.0, 0.0,
	LMNULL};


#if 1
# if 0
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.70, 0.70, 0.70,
	POSITION, 100.0, -200.0, 100.0, 0.0,
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.6, 0.1, 0.1,
	POSITION, -100.0, -30.0, 100.0, 0.0,
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.3, 0.1,
	POSITION, 100.0, -20.0, 20.0, 0.0,
	LMNULL};


static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.1, 0.3,
	POSITION, 0.0, 100.0, -100.0, 0.0,
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 1.0, 0.0,
	LCOLOR,   0.75, 0.75, 0.75,
	POSITION, 0.0, 10.0, 10.0, 5.0,
	LMNULL};
# else
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.70, 0.70, 0.70,
	POSITION, 100.0, 200.0, 100.0, 0.0,
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.6, 0.1, 0.1,
	POSITION, 100.0, 30.0, 100.0, 0.0,
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.3, 0.1,
	POSITION, -100.0, 20.0, 20.0, 0.0,
	LMNULL};


static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.1, 0.3,
	POSITION, 0.0, -100.0, -100.0, 0.0,
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 1.0, 0.0,
	LCOLOR,   0.75, 0.75, 0.75,
	POSITION, 0.0, 10.0, 10.0, 5.0,
	LMNULL};
# endif

#else
static float white_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.70, 0.70, 0.70,
	POSITION, 10.0, 50.0, 50.0, 0.0,
	LMNULL};


static float red_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.5, 0.1, 0.1,
	POSITION, -100.0, 0.0, 0.0, 0.0,
	LMNULL};

static float green_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.5, 0.1,
	POSITION, 100.0, 50.0, 0.0, 0.0,
	LMNULL};

static float blue_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.1, 0.1, 0.5,
	POSITION, 0.0, -50.0, 0.0, 0.0,
	LMNULL};

static float orange_inf_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,	0.35, 0.175, 0.0,
	POSITION, -50.0, 50.0, 10.0, 0.0,
	LMNULL};

static float white_local_light[] = {
	AMBIENT, 0.0, 0.0, 0.0,
	LCOLOR,   0.75, 0.75, 0.75,
	POSITION, 0.0, 10.0, 10.0, 5.0,
	LMNULL};


#endif




/*
 *  Lighting model parameters
 *	AMBIENT		amount of ambient light present in the scene, 0..1
 *	ATTENUATION	fixed and variable attenuation factor, 0..1
 *	LOCALVIEWER	1=eye at (0,0,0), 0=eye at (0,0,+inf)
 */
static float	default_lmodel[] = {
	AMBIENT,	0.2,  0.2,  0.2,
	ATTENUATION,	1.0, 0.0,
	LOCALVIEWER,	0.0,
	LMNULL};

static float infinite[] = {
	AMBIENT, 0.3,  0.3, 0.3,
	LOCALVIEWER, 0.0,
	LMNULL};

static float local[] = {
	AMBIENT, 0.3,  0.3, 0.3,
	LOCALVIEWER, 1.0,
	ATTENUATION, 1.0, 0.0,
	LMNULL};

static void
glx_make_materials()
{
	/* define material properties */
	lmdef (DEFMATERIAL, 1, 0, material_default);

	lmdef (DEFMATERIAL, 2, 0, mat_brass);
	lmdef (DEFMATERIAL, 3, 0, mat_shinybrass);
	lmdef (DEFMATERIAL, 4, 0, mat_pewter);
	lmdef (DEFMATERIAL, 5, 0, mat_silver);
	lmdef (DEFMATERIAL, 6, 0, mat_gold);
	lmdef (DEFMATERIAL, 7, 0, mat_shinygold);
	lmdef (DEFMATERIAL, 8, 0, mat_plaster);
	lmdef (DEFMATERIAL, 9, 0, mat_redplastic);
	lmdef (DEFMATERIAL, 10, 0, mat_greenplastic);
	lmdef (DEFMATERIAL, 11, 0, mat_blueplastic);

	lmdef (DEFMATERIAL, 12, 0, mat_greenflat);
	lmdef (DEFMATERIAL, 13, 0, mat_greenshiny);

	lmdef (DEFMATERIAL, 14, 0, mat_blueflat);
	lmdef (DEFMATERIAL, 15, 0, mat_blueshiny);

	lmdef (DEFMATERIAL, 16, 0, mat_redflat);
	lmdef (DEFMATERIAL, 17, 0, mat_redshiny);

	lmdef (DEFMATERIAL, 18, 0, mat_beigeshiny);

	lmdef( DEFMATERIAL, 19, 0, material_xdefault );
	lmdef( DEFMATERIAL, 20, 0, material_rtdefault );

	/*    lmdef (DEFLIGHT, 1, 0, default_light); */
	lmdef (DEFLIGHT, 4, 0, green_inf_light);
	lmdef (DEFLIGHT, 2, 0, white_inf_light);
	lmdef (DEFLIGHT, 3, 0, red_inf_light);
	lmdef (DEFLIGHT, 4, 0, green_inf_light);
	lmdef (DEFLIGHT, 5, 0, blue_inf_light);
	/*    lmdef (DEFLIGHT, 6, 0, orange_inf_light); */
	lmdef (DEFLIGHT, 7, 0, white_local_light);

	lmdef (DEFLMODEL, 1, 0, default_lmodel);
	lmdef (DEFLMODEL, 2, 0, infinite);
	lmdef (DEFLMODEL, 3, 0, local);
}

/*
 *  Check to see if setmonitor(STR_RECT) will work.
 *  Returns -
 *	> 0	If stereo is available
 *	0	If not
 */
static int
glx_has_stereo()
{
#if !defined(__sgi) && !defined(__mips)
	/* IRIX 3 systems, test to see if DER1_STEREO bit is
	 * read/write (no hardware underneath), or
	 * read only (hardware underneath, which can't be read back.
	 */
	int	rw_orig, rw1, rw2;

	rw_orig = getvideo(DE_R1);
	rw1 = rw_orig ^ DER1_STEREO;	/* Toggle the bit */
	setvideo(DE_R1, rw1);
	rw2 = getvideo(DE_R1);
	if( rw1 != rw2 )  {
		setvideo(DE_R1, rw_orig);/* Restore original state */
		return 1;		/* Has stereo */
	}
	rw1 = rw1 ^ DER1_STEREO;	/* Toggle the bit, again */
	setvideo(DE_R1, rw1);
	rw2 = getvideo(DE_R1);
	if( rw1 != rw2 )  {
		setvideo(DE_R1, rw_orig);/* Restore original state */
		return 1;		/* Has stereo */
	}
	setvideo(DE_R1, rw_orig);	/* Restore original state */
	return 0;			/* Does not have stereo */
#else
	/* IRIX 4 systems */
	return getgdesc(GD_STEREO);
#endif
}

static unsigned long
extract_value(buffer, mode, conf)
int buffer;
int mode;
GLXconfig *conf;
{
  int i;

  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == mode)
      return conf[i].arg;

  return 0;
}

/* Extract X visual information */
static XVisualInfo*
extract_visual(dmp, buffer, conf)
struct dm *dmp;
int buffer;
GLXconfig *conf;
{
  XVisualInfo template, *v;
  int n;

  template.screen = Tk_ScreenNumber(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  template.visualid = extract_value(buffer, GLX_VISUAL, conf);

  return XGetVisualInfo(((struct glx_vars *)dmp->dm_vars)->dpy,
			VisualScreenMask|VisualIDMask, &template, &n);
}

/*
 * Fill the configuration structure with the appropriately
 * created window
 */
static void
set_window(buffer, _win, conf)
int buffer;
Window _win;
GLXconfig *conf;
{
  int i;

  for (i = 0; conf[i].buffer; i++)
    if (conf[i].buffer == buffer && conf[i].mode == GLX_WINDOW)
      conf[i].arg = _win;
}

/*
 *                  G L X _ B E G I N D L I S T
 *
 */
int
glx_beginDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_beginDList()\n", (char *)NULL);

  GLXwinset(((struct glx_vars *)dmp->dm_vars)->dpy,
	    ((struct glx_vars *)dmp->dm_vars)->win);
  makeobj((Object)list);

  return TCL_OK;
}

/*
 *                  G L X _ E N D D L I S T
 *
 */
int
glx_endDList(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_endDList()\n", (char *)NULL);

  closeobj();
  return TCL_OK;
}

/*
 *                  G L X _ D R A W D L I S T
 *
 */
int
glx_drawDList(dmp, list)
struct dm *dmp;
unsigned int list;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_drawDList()\n", (char *)NULL);

  callobj((Object)list);
  return TCL_OK;
}

/*
 *                  G L X _ F R E E D L I S T
 *
 */
int
glx_freeDLists(dmp, list, range)
struct dm *dmp;
unsigned int list;
int range;
{
  int i;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "glx_freeDLists()\n", (char *)NULL);

  for(i = 0; i < range; ++i)
    delobj((Object)(list + i));

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
