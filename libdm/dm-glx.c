/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"
#include "tk.h"

#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif

#include <gl/glws.h>
#include <X11/extensions/XInput.h>
#include <X11/Xutil.h>
#include <sys/invent.h>

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
void    Glx_configure_window_shape();
void    Glx_establish_perspective();
void    Glx_set_perspective();
void	Glx_establish_lighting();
void	Glx_establish_zbuffer();
int Glx_irisX2ged();
int Glx_irisY2ged();

struct glx_vars head_glx_vars;
/* End of functions and variables that are available to applications */


static void set_window();
static XVisualInfo *extract_visual();
static unsigned long extract_value();

static void Glx_var_init();
static void Glx_make_materials();
static void Glx_clear_to_black();

struct dm	*Glx_open();
static int	Glx_close();
static int	Glx_drawBegin(), Glx_drawEnd();
static int	Glx_normal(), Glx_newrot();
static int	Glx_drawString2D(), Glx_drawLine2D();
static int      Glx_drawVertex2D();
static int	Glx_drawVList();
static int      Glx_setColor(), Glx_setLineAttr();
static unsigned Glx_cvtvecs(), Glx_load();
static int	Glx_setWinBounds(), Glx_debug();

static GLXconfig glx_config_wish_list [] = {
  { GLX_NORMAL, GLX_WINDOW, GLX_NONE },
  { GLX_NORMAL, GLX_DOUBLE, TRUE },
  { GLX_NORMAL, GLX_RGB, TRUE },
  { GLX_NORMAL, GLX_ZSIZE, GLX_NOCONFIG },
  { 0, 0, 0 }
};

static int perspective_table[] = { 
	30, 45, 60, 90 };
static double	xlim_view = 1.0;	/* args for ortho() */
static double	ylim_view = 1.0;

struct dm dm_glx = {
  Glx_open,
  Glx_close,
  Glx_drawBegin,
  Glx_drawEnd,
  Glx_normal,
  Glx_newrot,
  Glx_drawString2D,
  Glx_drawLine2D,
  Glx_drawVertex2D,
  Glx_drawVList,
  Glx_setColor,
  Glx_setLineAttr,
  Glx_cvtvecs,
  Glx_load,
  Glx_setWinBounds,
  Glx_debug,
  Nu_int0,
  0,			/* no "displaylist", per. se. */
  IRBOUND,
  "glx", "SGI - mixed mode", 
  1,
  0,
  0,
  1.0, /* aspect ratio */
  0,
  0,
  0,
  0,
  0,
  0,
  0
};

/*
 *  Mouse coordinates are in absolute screen space, not relative to
 *  the window they came from.  Convert to window-relative,
 *  then to MGED-style +/-2048 range.
 */
int
Glx_irisX2ged(dmp, x, use_aspect)
struct dm *dmp;
register int x;
int use_aspect;
{
  if(use_aspect)
    return ((x / (double)dmp->dm_width - 0.5) /
	    dmp->dm_aspect * 4095);
  else
    return ((x / (double)dmp->dm_width - 0.5) * 4095);
}

int
Glx_irisY2ged(dmp, y)
struct dm *dmp;
register int y;
{
  return ((0.5 - y/(double)dmp->dm_height) * 4095);
}

/*
 *			I R _ O P E N
 *
 *  Fire up the display manager, and the display processor. Note that
 *  this brain-damaged version of the MEX display manager gets terribly
 *  confused if you try to close your last window.  Tough. We go ahead
 *  and close the window.  Ignore the "ERR_CLOSEDLASTWINDOW" error
 *  message. It doesn't hurt anything.  Silly MEX.
 */
struct dm *
#if DO_NEW_LIBDM_OPEN
Glx_open(eventHandler, argc, argv)
int (*eventHandler)();
#else
Glx_open(dmp, argc, argv)
struct dm *dmp;
#endif
int argc;
char *argv[];
{
  static int count = 0;
  register int	i;
  Matrix		m;
  inventory_t	*inv;
  struct bu_vls str;
  struct bu_vls top_vls;
  int j, k;
  int ndevices;
  int nclass = 0;
  int make_square = -1;
  XDeviceInfoPtr olist, list;
  XDevice *dev;
  XEventClass e_class[15];
  XInputClassInfo *cip;
  GLXconfig *p, *glx_config;
  XVisualInfo *visual_info;
#if DO_NEW_LIBDM_OPEN
  struct dm *dmp;

  dmp = BU_GETSTRUCT(dmp, dm);
  *dmp = dm_glx;
  dmp->dm_eventHandler = eventHandler;
#endif

  /* Only need to do this once for this display manager */
  if(!count){
    bzero((void *)&head_glx_vars, sizeof(struct glx_vars));
    BU_LIST_INIT( &head_glx_vars.l );
  }

  dmp->dm_vars = bu_calloc(1, sizeof(struct glx_vars), "Glx_init: struct glx_vars");
  if(!dmp->dm_vars){
    bu_free(dmp, "Glx_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&dmp->dm_initWinProc);

  i = dm_process_options(dmp,
			 &dmp->dm_width,
			 &dmp->dm_height,
			 argc,
			 argv);

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
  if(bu_vls_strlen(&dmp->dm_initWinProc) == 0)
    bu_vls_strcpy(&dmp->dm_initWinProc, "bind_dm");

  /* initialize dm specific variables */
  ((struct glx_vars *)dmp->dm_vars)->devmotionnotify = LASTEvent;
  ((struct glx_vars *)dmp->dm_vars)->devbuttonpress = LASTEvent;
  ((struct glx_vars *)dmp->dm_vars)->devbuttonrelease = LASTEvent;
  ((struct glx_vars *)dmp->dm_vars)->perspective_angle = 3;

  /* initialize modifiable variables */
  ((struct glx_vars *)dmp->dm_vars)->mvars.zclipping_on = 1;       /* Z Clipping flag */
  ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on = 1;         /* Hardware Z buffer is on */
  ((struct glx_vars *)dmp->dm_vars)->mvars.linewidth = 1;      /* Line drawing width */
  ((struct glx_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;

#if 0
  if(BU_LIST_IS_EMPTY(&head_glx_vars.l))
#else
  if(dmp->dm_eventHandler != DM_EVENT_HANDLER_NULL)
#endif
    Tk_CreateGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_GLX);

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
    (void)Glx_close(dmp);
    return DM_NULL;
  }
  ogl_sgi_used = 1;
#endif /* DM_OGL */

  bu_vls_init(&top_vls);
  if(dmp->dm_top){
    ((struct glx_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindowFromPath(interp, tkwin,
			      bu_vls_addr(&dmp->dm_pathName),
			      bu_vls_addr(&dmp->dm_dName));
    ((struct glx_vars *)dmp->dm_vars)->top = ((struct glx_vars *)dmp->dm_vars)->xtkwin;
    bu_vls_printf(&top_vls, "%S", &dmp->dm_pathName);
  }else{
    char *cp;

    cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
    if(cp == bu_vls_addr(&dmp->dm_pathName)){
      ((struct glx_vars *)dmp->dm_vars)->top = tkwin;
      bu_vls_strcpy(&top_vls, ".");
    }else{
      bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
		    bu_vls_addr(&dmp->dm_pathName));
      ((struct glx_vars *)dmp->dm_vars)->top =
	Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
    }

    bu_vls_free(&top_vls);

    /* Make xtkwin an embedded window */
    ((struct glx_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindow(interp, ((struct glx_vars *)dmp->dm_vars)->top,
		      cp + 1, (char *)NULL);
  }

  if(((struct glx_vars *)dmp->dm_vars)->xtkwin == NULL){
    Tcl_AppendResult(interp, "dm-X: Failed to open ",
		     bu_vls_addr(&dmp->dm_pathName),
		     "\n", (char *)NULL);
    (void)Glx_close(dmp);
    return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct glx_vars *)dmp->dm_vars)->xtkwin));

#if 1
  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S \n",
		&dmp->dm_initWinProc,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    (void)Glx_close(dmp);
    return DM_NULL;
  }

  bu_vls_free(&str);

  ((struct glx_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct glx_vars *)dmp->dm_vars)->top);

  if(dmp->dm_width == 0){
    dmp->dm_width =
      DisplayWidth(((struct glx_vars *)dmp->dm_vars)->dpy,
		   DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 20;
    ++make_square;
  }

  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(((struct glx_vars *)dmp->dm_vars)->dpy,
		    DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 20;
    ++make_square;
  }
#else
  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %s %s\n",
		bu_vls_addr(&dmp->dm_initWinProc),
		bu_vls_addr(&dmp->dm_pathName));

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
        bu_vls_free(&str);
	return TCL_ERROR;
  }

  bu_vls_free(&str);

  ((struct glx_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct glx_vars *)dmp->dm_vars)->xtkwin);

#if 0
  XSynchronize(((struct glx_vars *)dmp->dm_vars)->dpy, True);
#endif

  dmp->dm_width =
    DisplayWidth(((struct glx_vars *)dmp->dm_vars)->dpy,
		 DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 20;
  dmp->dm_height =
    DisplayHeight(((struct glx_vars *)dmp->dm_vars)->dpy,
		  DefaultScreen(((struct glx_vars *)dmp->dm_vars)->dpy)) - 20;

#endif
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
  visual_info = extract_visual(dmp, GLX_NORMAL, glx_config);
  ((struct glx_vars *)dmp->dm_vars)->vis = visual_info->visual;
  ((struct glx_vars *)dmp->dm_vars)->depth = visual_info->depth;
  ((struct glx_vars *)dmp->dm_vars)->cmap = extract_value(GLX_NORMAL, GLX_COLORMAP,
							   glx_config);
  Tk_SetWindowVisual(((struct glx_vars *)dmp->dm_vars)->xtkwin,
		     ((struct glx_vars *)dmp->dm_vars)->vis,
		     ((struct glx_vars *)dmp->dm_vars)->depth,
		     ((struct glx_vars *)dmp->dm_vars)->cmap);

  Tk_MakeWindowExist(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  ((struct glx_vars *)dmp->dm_vars)->win =
    Tk_WindowId(((struct glx_vars *)dmp->dm_vars)->xtkwin);
  set_window(GLX_NORMAL, ((struct glx_vars *)dmp->dm_vars)->win, glx_config);

  /* Inform the GL that you intend to render GL into an X window */
  if(GLXlink(((struct glx_vars *)dmp->dm_vars)->dpy, glx_config) < 0){
    (void)Glx_close(dmp);
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

  Glx_configure_window_shape(dmp);

  /* Line style 0 is solid.  Program line style 1 as dot-dashed */
  deflinestyle( 1, 0xCF33 );
  setlinestyle( 0 );

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list =
    (XDeviceInfoPtr)XListInputDevices(((struct glx_vars *)dmp->dm_vars)->dpy, &ndevices);

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct glx_vars *)dmp->dm_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp, "Glx_open: Couldn't open the dials+buttons\n",
			   (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#if IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, ((struct glx_vars *)dmp->dm_vars)->devbuttonpress,
			      e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, ((struct glx_vars *)dmp->dm_vars)->devbuttonrelease,
				e_class[nclass]);
	    ++nclass;
#endif
	    break;
#if IR_KNOBS
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
 *			I R _ C O N F I G U R E _ W I N D O W _ S H A P E
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 */
void
Glx_configure_window_shape(dmp)
struct dm *dmp;
{
  int		npix;
  XWindowAttributes xwa;

  XGetWindowAttributes( ((struct glx_vars *)dmp->dm_vars)->dpy,
			((struct glx_vars *)dmp->dm_vars)->win, &xwa );
  dmp->dm_width = xwa.width;
  dmp->dm_height = xwa.height;

  /* Write enable all the bloody bits after resize! */
  viewport(0, dmp->dm_width, 0,
	   dmp->dm_height);

  if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf )
    Glx_establish_zbuffer(dmp);

  Glx_establish_lighting(dmp);
	
  if( ((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer){
    /* Clear out image from windows underneath */
    frontbuffer(1);
    Glx_clear_to_black(dmp);
    frontbuffer(0);
    Glx_clear_to_black(dmp);
  } else
    Glx_clear_to_black(dmp);

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );
  dmp->dm_aspect =
    (fastf_t)dmp->dm_height/
    (fastf_t)dmp->dm_width;
}

/*
 *  			I R _ C L O S E
 *  
 *  Gracefully release the display.  Well, mostly gracefully -- see
 *  the comments in the open routine.
 */
static int
Glx_close(dmp)
struct dm *dmp;
{
  if(((struct glx_vars *)dmp->dm_vars)->dpy){
    if(((struct glx_vars *)dmp->dm_vars)->xtkwin != NULL){
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
      Glx_clear_to_black(dmp);
      frontbuffer(0);

      GLXunlink(((struct glx_vars *)dmp->dm_vars)->dpy,
		((struct glx_vars *)dmp->dm_vars)->win);
      Tk_DestroyWindow(((struct glx_vars *)dmp->dm_vars)->xtkwin);
    }

    if(BU_LIST_IS_EMPTY(&head_glx_vars.l))
      Tk_DeleteGenericHandler(dmp->dm_eventHandler, (ClientData)DM_TYPE_GLX);
  }

  if(((struct glx_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct glx_vars *)dmp->dm_vars)->l);

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_vls_free(&dmp->dm_initWinProc);
  bu_free(dmp->dm_vars, "Glx_close: glx_vars");
  bu_free(dmp, "Glx_close: dmp");

  return TCL_OK;
}

/*
 *			G L X _ P R O L O G
 *
 * Define the world, and include in it instances of all the
 * important things.  Most important of all is the object "faceplate",
 * which is built between dm_normal() and dm_epilog()
 * by dm_puts and dm_2d_line calls from adcursor() and dotitles().
 */
static int
Glx_drawBegin(dmp)
struct dm *dmp;
{
  GLXwinset(((struct glx_vars *)dmp->dm_vars)->dpy,
	    ((struct glx_vars *)dmp->dm_vars)->win);

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_drawBegin\n", (char *)NULL);

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );

  if( !((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer )
    Glx_clear_to_black(dmp);

#if 0
  linewidth(((struct glx_vars *)dmp->dm_vars)->mvars.linewidth);
#endif

  return TCL_OK;
}

/*
 *			I R _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
Glx_normal(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_normal\n", (char *)NULL);

#if 0
  RGBcolor( (short)0, (short)0, (short)0 );
#endif

  ortho( -xlim_view, xlim_view, -ylim_view, ylim_view, -1.0, 1.0 );

  return TCL_OK;
}

/*
 *			I R _ E P I L O G
 */
static int
Glx_drawEnd(dmp)
struct dm *dmp;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_drawEnd\n", (char *)NULL);

  /*
   * A Point, in the Center of the Screen.
   * This is drawn last, to always come out on top.
   */
  Glx_drawLine2D( dmp, 0, 0, 0, 0, 0 );
  /* End of faceplate */

  if(((struct glx_vars *)dmp->dm_vars)->mvars.doublebuffer){
    swapbuffers();
    /* give Graphics pipe time to work */
    Glx_clear_to_black(dmp);
  }

  return TCL_OK;
}

/*
 *  			I R _ N E W R O T
 *
 *  Load a new rotation matrix.  This will be followed by
 *  many calls to Glx_drawVList().
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
Glx_newrot(dmp, mat, which_eye)
struct dm *dmp;
mat_t	mat;
int which_eye;
{
  register fastf_t *mptr;
  Matrix	gtmat;
  mat_t	newm;
  int	i;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_newrot()\n", (char *)NULL);

  switch(which_eye)  {
  case 0:
    /* Non-stereo */
    break;
  case 1:
    /* R eye */
    viewport(0, XMAXSCREEN, 0, YSTEREO);
    Glx_drawString2D( dmp, "R", 2020, 0, 0, DM_RED );
    break;
  case 2:
    /* L eye */
    viewport(0, XMAXSCREEN, 0+YOFFSET_LEFT, YSTEREO+YOFFSET_LEFT);
    break;
  }

  if( ! ((struct glx_vars *)dmp->dm_vars)->mvars.zclipping_on ) {
    mat_t	nozclip;

    bn_mat_idn( nozclip );
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
 *  			I R _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.  Mat is model2view.
 *
 */
static int
Glx_drawVList( dmp, vp, m )
struct dm *dmp;
register struct rt_vlist *vp;
fastf_t *m;
{
  register struct rt_vlist	*tvp;
  register int nvec;
  register float	*gtvec;
  char	gtbuf[16+3*sizeof(double)];
  int first;
  int i,j;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_drawVList()\n", (char *)NULL);

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
 *			I R _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
Glx_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x,y;
int size;
int use_aspect;
{
  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_drawString2D()\n", (char *)NULL);

  if(use_aspect)
    cmov2( GED2IRIS(x) * dmp->dm_aspect, GED2IRIS(y));
  else
    cmov2( GED2IRIS(x), GED2IRIS(y));

  charstr( str );

  return TCL_OK;
}

/*
 *			I R _ 2 D _ L I N E
 *
 */
static int
Glx_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  register int nvec;

  if (((struct glx_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "Glx_drawLine2D()\n", (char *)NULL);

  move2( GED2IRIS(x1), GED2IRIS(y1));
  draw2( GED2IRIS(x2), GED2IRIS(y2));

  return TCL_OK;
}

static int
Glx_drawVertex2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  return Glx_drawLine2D(dmp, x, y, x, y);
}

static int
Glx_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
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


static int
Glx_setLineAttr(dmp, width, dashed)
struct dm *dmp;
int width;
int dashed;
{
  ((struct glx_vars *)dmp->dm_vars)->mvars.linewidth = width;
  linewidth(width);

  if(dashed)
    setlinestyle(1);    /* into dot-dash */
  else
    setlinestyle(0);

  return TCL_OK;
}


/*
 *			I R _ C V T V E C S
 *
 */
static unsigned
Glx_cvtvecs( dmp, sp )
struct dm *dmp;
register struct solid *sp;
{
  return( 0 );	/* No "displaylist" consumed */
}

/*
 * Loads displaylist from storage[]
 */
static unsigned
Glx_load( dmp, addr, count )
struct dm *dmp;
unsigned addr, count;
{
  return( 0 );		/* FLAG:  error */
}


static int
Glx_debug(dmp, lvl)
struct dm *dmp;
{
  ((struct glx_vars *)dmp->dm_vars)->mvars.debug = lvl;

  return TCL_OK;
}

static int
Glx_setWinBounds(dmp, w)
struct dm *dmp;
int w[];
{
  return TCL_OK;
}


void
Glx_establish_perspective(dmp)
struct dm *dmp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf( &vls, "set perspective %d\n",
		((struct glx_vars *)dmp->dm_vars)->mvars.perspective_mode ?
		perspective_table[((struct glx_vars *)dmp->dm_vars)->perspective_angle] :
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
Glx_set_perspective(dmp)
struct dm *dmp;
{
  /* set perspective matrix */
  if(((struct glx_vars *)dmp->dm_vars)->mvars.dummy_perspective > 0)
    ((struct glx_vars *)dmp->dm_vars)->perspective_angle =
      ((struct glx_vars *)dmp->dm_vars)->mvars.dummy_perspective <= 4 ?
      ((struct glx_vars *)dmp->dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct glx_vars *)dmp->dm_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct glx_vars *)dmp->dm_vars)->perspective_angle = 3;

  if(((struct glx_vars *)dmp->dm_vars)->mvars.perspective_mode){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf( &vls, "set perspective %d\n",
		  perspective_table[((struct glx_vars *)dmp->dm_vars)->perspective_angle] );

    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct glx_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;

#if 0
  dmaflag = 1;
#endif
}

void
Glx_establish_zbuffer(dmp)
struct dm *dmp;
{
	if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuf == 0 )  {
	  Tcl_AppendResult(interp, "dm-4d: This machine has no Zbuffer to enable\n",
			   (char *)NULL);
	  ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on = 0;
	}
	zbuffer( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on );
	if( ((struct glx_vars *)dmp->dm_vars)->mvars.zbuffer_on)  {
	  /* Set screen coords of near and far clipping planes */
	  lsetdepth(((struct glx_vars *)dmp->dm_vars)->mvars.min_scr_z,
		    ((struct glx_vars *)dmp->dm_vars)->mvars.max_scr_z);
	}
#if 0
	dmaflag = 1;
#endif
}

static void
Glx_clear_to_black(dmp)
struct dm *dmp;
{
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
 *  The struct_parse will change the value of the variable.
 *  Just implement it, here.
 */
void
Glx_establish_lighting(dmp)
struct dm *dmp;
{
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
      Glx_colorchange(dmp);
#else
      depthcue(0);
#endif
    }

    mmode(MVIEWING);

    Glx_make_materials();	/* Define material properties */

    lmbind(LMODEL, 2);	/* infinite */
    lmbind(LIGHT2,2);
    lmbind(LIGHT3,3);
    lmbind(LIGHT4,4);
    lmbind(LIGHT5,5);

    /* RGB color commands & lighting */
    lmcolor( LMC_COLOR );

    mmode(MSINGLE);
  }

#if 0
  dmaflag = 1;
#endif
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
Glx_make_materials()
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
Glx_has_stereo()
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
