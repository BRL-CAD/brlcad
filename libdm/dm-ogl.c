/*
 *			D M - O G L . C
 *
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
#ifdef USE_MESA_GL
#include <MESA_GL/glx.h>
#include <MESA_GL/gl.h>
#else
#include <GL/glx.h>
#include <GL/gl.h>
#endif

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
#include "solid.h"

#define VIEWFACTOR      (1/(*dmp->dm_vp))
#define VIEWSIZE        (2*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

extern Tk_Window tkwin;

void	ogl_configure_window_shape();
void    ogl_establish_perspective();
void    ogl_set_perspective();
void	ogl_establish_lighting();
void	ogl_establish_zbuffer();

static XVisualInfo *ogl_set_visual();

/* Flags indicating whether the ogl and sgi display managers have been
 * attached.
 * These are necessary to decide whether or not to use direct rendering
 * with gl.
 */
char  ogl_ogl_used = 0;
char  ogl_sgi_used = 0;

/* Display Manager package interface */
#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*ogl_open();
static int	ogl_close();
static int	ogl_drawBegin();
static int      ogl_drawEnd();
static int	ogl_normal(), ogl_newrot();
static int	ogl_drawString2D(), ogl_drawLine2D();
static int      ogl_drawVertex2D();
static int	ogl_drawVList();
static int      ogl_setColor(), ogl_setLineAttr();
static int	ogl_setWinBounds(), ogl_debug();
static int      ogl_beginDList(), ogl_endDList();
static int      ogl_drawDList();
static int      ogl_freeDLists();

struct dm dm_ogl = {
  ogl_close,
  ogl_drawBegin,
  ogl_drawEnd,
  ogl_normal,
  ogl_newrot,
  ogl_drawString2D,
  ogl_drawLine2D,
  ogl_drawVertex2D,
  ogl_drawVList,
  ogl_setColor,
  ogl_setLineAttr,
  ogl_setWinBounds,
  ogl_debug,
  ogl_beginDList,
  ogl_endDList,
  ogl_drawDList,
  ogl_freeDLists,
  0,
  1,				/* has displaylist */
  0,                            /* no stereo by default */
  IRBOUND,
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
  0,
  0,
  0,
  0
};


/* ogl stuff */
struct ogl_vars head_ogl_vars;
int perspective_table[] = {
	30, 45, 60, 90 };

static fastf_t default_viewscale = 1000.0;
static double	xlim_view = 1.0;	/* args for glOrtho*/
static double	ylim_view = 1.0;

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};

static float light0_position[] = {100.0, 200.0, 100.0, 0.0};
static float light1_position[] = {100.0, 30.0, 100.0, 0.0};
static float light2_position[] = {-100.0, 20.0, 20.0, 0.0};
static float light3_position[] = {0.0, -100.0, -100.0, 0.0};

static float light0_diffuse[] = {0.70, 0.70, 0.70, 1.0}; /* white */
static float light1_diffuse[] = {0.60, 0.10, 0.10, 1.0}; /* red */
static float light2_diffuse[] = {0.10, 0.30, 0.10, 1.0}; /* green */
static float light3_diffuse[] = {0.10, 0.10, 0.30, 1.0}; /* blue */

void
ogl_do_fog(dmp)
struct dm *dmp;
{
  glHint(GL_FOG_HINT,
	 ((struct ogl_vars *)dmp->dm_vars)->mvars.fastfog ? GL_FASTEST : GL_NICEST);
}

/*
 *			O G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
ogl_open(argc, argv)
int argc;
char *argv[];
{
  static int count = 0;
  int a_screen;
  XVisualInfo *vip;
  GLfloat backgnd[4];
  int i, j, k;
  int make_square = -1;
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
  fastf_t tmp_vp;
  struct dm *dmp;

  BU_GETSTRUCT(dmp, dm);
  if(dmp == DM_NULL)
    return DM_NULL;

  *dmp = dm_ogl; /* struct copy */
  dmp->dm_vp = &tmp_vp;

  /* Only need to do this once for this display manager */
  if(!count){
    bzero((void *)&head_ogl_vars, sizeof(struct ogl_vars));
    BU_LIST_INIT( &head_ogl_vars.l );
  }

  dmp->dm_vars = (genptr_t)bu_calloc(1, sizeof(struct ogl_vars), "ogl_open: ogl_vars");
  if(dmp->dm_vars == (genptr_t)NULL){
    bu_free(dmp, "ogl_open: dmp");
    return DM_NULL;
  }

  bu_vls_init(&dmp->dm_pathName);
  bu_vls_init(&dmp->dm_tkName);
  bu_vls_init(&dmp->dm_dName);
  bu_vls_init(&init_proc_vls);

  i = dm_process_options(dmp, &init_proc_vls, --argc, ++argv);

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
  ((struct ogl_vars *)dmp->dm_vars)->devmotionnotify = LASTEvent;
  ((struct ogl_vars *)dmp->dm_vars)->devbuttonpress = LASTEvent;
  ((struct ogl_vars *)dmp->dm_vars)->devbuttonrelease = LASTEvent;
  ((struct ogl_vars *)dmp->dm_vars)->perspective_angle = 3;
  dmp->dm_aspect = 1.0;

  /* initialize modifiable variables */
  ((struct ogl_vars *)dmp->dm_vars)->mvars.rgb = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.doublebuffer = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.zclipping_on = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.zbuffer_on = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.linewidth = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.fastfog = 1;
  ((struct ogl_vars *)dmp->dm_vars)->mvars.fogdensity = 1.0;

  BU_LIST_APPEND(&head_ogl_vars.l, &((struct ogl_vars *)dmp->dm_vars)->l);

  /* this is important so that ogl_configure_notify knows to set
   * the font */
  ((struct ogl_vars *)dmp->dm_vars)->fontstruct = NULL;

  if((tmp_dpy = XOpenDisplay(bu_vls_addr(&dmp->dm_dName))) == NULL){
    bu_vls_free(&str);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  if(dmp->dm_width == 0){
    dmp->dm_width =
      DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
     ++make_square;
  }
  if(dmp->dm_height == 0){
    dmp->dm_height =
      DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 20;
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

  bu_vls_init(&top_vls);
  if(dmp->dm_top){
    /* Make xtkwin a toplevel window */
    ((struct ogl_vars *)dmp->dm_vars)->xtkwin =
      Tk_CreateWindowFromPath(interp,
			      tkwin,
			      bu_vls_addr(&dmp->dm_pathName),
			      bu_vls_addr(&dmp->dm_dName));
    ((struct ogl_vars *)dmp->dm_vars)->top = ((struct ogl_vars *)dmp->dm_vars)->xtkwin;
    bu_vls_printf(&top_vls, "%S", &dmp->dm_pathName);
  }else{
     char *cp;

     cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
     if(cp == bu_vls_addr(&dmp->dm_pathName)){
       ((struct ogl_vars *)dmp->dm_vars)->top = tkwin;
       bu_vls_strcpy(&top_vls, ".");
     }else{
       bu_vls_printf(&top_vls, "%*s", cp - bu_vls_addr(&dmp->dm_pathName),
		     bu_vls_addr(&dmp->dm_pathName));
       ((struct ogl_vars *)dmp->dm_vars)->top =
	 Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
     }

     bu_vls_free(&top_vls);

     /* Make xtkwin an embedded window */
     ((struct ogl_vars *)dmp->dm_vars)->xtkwin =
       Tk_CreateWindow(interp, ((struct ogl_vars *)dmp->dm_vars)->top,
		       cp + 1, (char *)NULL);
  }

  if( ((struct ogl_vars *)dmp->dm_vars)->xtkwin == NULL ) {
    Tcl_AppendResult(interp, "dm-Ogl: Failed to open ",
		     bu_vls_addr(&dmp->dm_pathName),
		     "\n", (char *)NULL);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  bu_vls_printf(&dmp->dm_tkName, "%s",
		(char *)Tk_Name(((struct ogl_vars *)dmp->dm_vars)->xtkwin));

  bu_vls_init(&str);
  bu_vls_printf(&str, "_init_dm %S %S\n",
		&init_proc_vls,
		&dmp->dm_pathName);

  if(Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR){
    bu_vls_free(&str);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  bu_vls_free(&init_proc_vls);
  bu_vls_free(&str);

  ((struct ogl_vars *)dmp->dm_vars)->dpy =
    Tk_Display(((struct ogl_vars *)dmp->dm_vars)->top);

  Tk_GeometryRequest(((struct ogl_vars *)dmp->dm_vars)->xtkwin,
		     dmp->dm_width,
		     dmp->dm_height);

  /* must do this before MakeExist */
  if((vip=ogl_set_visual(dmp,
			 ((struct ogl_vars *)dmp->dm_vars)->xtkwin))==NULL){
    Tcl_AppendResult(interp, "ogl_open: Can't get an appropriate visual.\n",
		     (char *)NULL);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

#if 0
  Tk_MoveToplevelWindow(((struct ogl_vars *)dmp->dm_vars)->xtkwin,
			1276 - 976, 0);
#endif
  Tk_MakeWindowExist(((struct ogl_vars *)dmp->dm_vars)->xtkwin);

  ((struct ogl_vars *)dmp->dm_vars)->win =
    Tk_WindowId(((struct ogl_vars *)dmp->dm_vars)->xtkwin);
  dmp->dm_id = ((struct ogl_vars *)dmp->dm_vars)->win;

  /* open GLX context */
  /* If the sgi display manager has been used, then we must use
   * an indirect context. Otherwise use direct, since it is usually
   * faster.
   */
  if ((((struct ogl_vars *)dmp->dm_vars)->glxc =
       glXCreateContext(((struct ogl_vars *)dmp->dm_vars)->dpy, vip, 0,
			ogl_sgi_used ? GL_FALSE : GL_TRUE))==NULL) {
    Tcl_AppendResult(interp, "ogl_open: couldn't create glXContext.\n",
		     (char *)NULL);
    (void)ogl_close(dmp);
    return DM_NULL;
  }
  /* If we used an indirect context, then as far as sgi is concerned,
   * gl hasn't been used.
   */
  ((struct ogl_vars *)dmp->dm_vars)->is_direct =
    (char) glXIsDirect(((struct ogl_vars *)dmp->dm_vars)->dpy,
		       ((struct ogl_vars *)dmp->dm_vars)->glxc);
  Tcl_AppendResult(interp, "Using ",
		   ((struct ogl_vars *)dmp->dm_vars)->is_direct ?
		   "a direct" : "an indirect",
		   " OpenGL rendering context.\n", (char *)NULL);
  /* set ogl_ogl_used if the context was ever direct */
  ogl_ogl_used = (((struct ogl_vars *)dmp->dm_vars)->is_direct ||
		  ogl_ogl_used);

  /*
   * Take a look at the available input devices. We're looking
   * for "dial+buttons".
   */
  olist = list =
    (XDeviceInfoPtr)XListInputDevices(((struct ogl_vars *)dmp->dm_vars)->dpy,
				      &ndevices);

  /* IRIX 4.0.5 bug workaround */
  if( list == (XDeviceInfoPtr)NULL ||
      list == (XDeviceInfoPtr)1 )  goto Done;

  for(j = 0; j < ndevices; ++j, list++){
    if(list->use == IsXExtensionDevice){
      if(!strcmp(list->name, "dial+buttons")){
	if((dev = XOpenDevice(((struct ogl_vars *)dmp->dm_vars)->dpy,
			      list->id)) == (XDevice *)NULL){
	  Tcl_AppendResult(interp,
			   "Glx_open: Couldn't open the dials+buttons\n",
			   (char *)NULL);
	  goto Done;
	}

	for(cip = dev->classes, k = 0; k < dev->num_classes;
	    ++k, ++cip){
	  switch(cip->input_class){
#if IR_BUTTONS
	  case ButtonClass:
	    DeviceButtonPress(dev, ((struct ogl_vars *)dmp->dm_vars)->devbuttonpress,
			      e_class[nclass]);
	    ++nclass;
	    DeviceButtonRelease(dev, ((struct ogl_vars *)dmp->dm_vars)->devbuttonrelease,
				e_class[nclass]);
	    ++nclass;
	    break;
#endif
#if IR_KNOBS
	  case ValuatorClass:
	    DeviceMotionNotify(dev, ((struct ogl_vars *)dmp->dm_vars)->devmotionnotify,
			       e_class[nclass]);
	    ++nclass;
	    break;
#endif
	  default:
	    break;
	  }
	}

	XSelectExtensionEvent(((struct ogl_vars *)dmp->dm_vars)->dpy,
			      ((struct ogl_vars *)dmp->dm_vars)->win, e_class, nclass);
	goto Done;
      }
    }
  }
Done:
  XFreeDeviceList(olist);

  if (!glXMakeCurrent(((struct ogl_vars *)dmp->dm_vars)->dpy,
		      ((struct ogl_vars *)dmp->dm_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars)->glxc)){
    Tcl_AppendResult(interp, "ogl_open: Couldn't make context current\n", (char *)NULL);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  /* display list (fontOffset + char) will displays a given ASCII char */
  if ((((struct ogl_vars *)dmp->dm_vars)->fontOffset = glGenLists(128))==0){
    Tcl_AppendResult(interp, "dm-ogl: Can't make display lists for font.\n", (char *)NULL);
    (void)ogl_close(dmp);
    return DM_NULL;
  }

  /* This is the applications display list offset */
  dmp->dm_displaylist = ((struct ogl_vars *)dmp->dm_vars)->fontOffset + 127;

  glDrawBuffer(GL_FRONT_AND_BACK);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.doublebuffer)
    glDrawBuffer(GL_BACK);
  else
    glDrawBuffer(GL_FRONT);

  /* do viewport, ortho commands and initialize font */
  ogl_configure_window_shape(dmp);

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
  glGetDoublev(GL_PROJECTION_MATRIX, ((struct ogl_vars *)dmp->dm_vars)->faceplate_mat);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity(); 
  glTranslatef( 0.0, 0.0, -1.0); 
  glPushMatrix();
  glLoadIdentity();
  ((struct ogl_vars *)dmp->dm_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */

  Tk_MapWindow(((struct ogl_vars *)dmp->dm_vars)->xtkwin);
  return dmp;
}


/*
 *  			O G L _ C L O S E
 *  
 *  Gracefully release the display.
 */
static int
ogl_close(dmp)
struct dm *dmp;
{
  if(((struct ogl_vars *)dmp->dm_vars)->dpy){
    if(((struct ogl_vars *)dmp->dm_vars)->glxc){
#if 0
      glDrawBuffer(GL_FRONT);
      glClearColor(0.0, 0.0, 0.0, 0.0);
      /*	glClearDepth(0.0);*/
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glDrawBuffer(GL_BACK);
#endif
      glXDestroyContext(((struct ogl_vars *)dmp->dm_vars)->dpy,
			((struct ogl_vars *)dmp->dm_vars)->glxc);
    }

    if(((struct ogl_vars *)dmp->dm_vars)->cmap)
      XFreeColormap(((struct ogl_vars *)dmp->dm_vars)->dpy,
		    ((struct ogl_vars *)dmp->dm_vars)->cmap);

    if(((struct ogl_vars *)dmp->dm_vars)->xtkwin)
      Tk_DestroyWindow(((struct ogl_vars *)dmp->dm_vars)->xtkwin);
  }

  if(((struct ogl_vars *)dmp->dm_vars)->l.forw != BU_LIST_NULL)
    BU_LIST_DEQUEUE(&((struct ogl_vars *)dmp->dm_vars)->l);

  bu_vls_free(&dmp->dm_pathName);
  bu_vls_free(&dmp->dm_tkName);
  bu_vls_free(&dmp->dm_dName);
  bu_free(dmp->dm_vars, "ogl_close: ogl_vars");
  bu_free(dmp, "ogl_close: dmp");

  return TCL_OK;
}

/*
 *			O G L _ P R O L O G
 *
 * There are global variables which are parameters to this routine.
 */
static int
ogl_drawBegin(dmp)
struct dm *dmp;
{
  GLint mm; 
  char i;
  char *str = "a";
  GLfloat fogdepth;

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_drawBegin\n", (char *)NULL);

  if (!glXMakeCurrent(((struct ogl_vars *)dmp->dm_vars)->dpy,
		      ((struct ogl_vars *)dmp->dm_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars)->glxc)){
    Tcl_AppendResult(interp,
 		     "ogl_drawBegin: Couldn't make context current\n", (char *)NULL);
    return TCL_ERROR;
  }

#if 0
  if (!((struct ogl_vars *)dmp->dm_vars)->mvars.doublebuffer){
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
#endif

  if (((struct ogl_vars *)dmp->dm_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    ((struct ogl_vars *)dmp->dm_vars)->face_flag = 0;
    if (((struct ogl_vars *)dmp->dm_vars)->mvars.cueing_on){
      glEnable(GL_FOG);
      /*XXX Need to do something with Viewscale */
      fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
      glFogf(GL_FOG_END, fogdepth);
      fogdepth = (GLfloat) (0.5*((struct ogl_vars *)dmp->dm_vars)->mvars.fogdensity/
			    (*dmp->dm_vp));
      glFogf(GL_FOG_DENSITY, fogdepth);
      glFogi(GL_FOG_MODE, ((struct ogl_vars *)dmp->dm_vars)->mvars.perspective_mode ?
	     GL_EXP : GL_LINEAR);
    }
    if (((struct ogl_vars *)dmp->dm_vars)->mvars.lighting_on){
      glEnable(GL_LIGHTING);
    }
  }

  return TCL_OK;
}

/*
 *			O G L _ E P I L O G
 */
static int
ogl_drawEnd(dmp)
struct dm *dmp;
{
  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_drawEnd\n", (char *)NULL);

  if(((struct ogl_vars *)dmp->dm_vars)->mvars.doublebuffer ){
    glXSwapBuffers(((struct ogl_vars *)dmp->dm_vars)->dpy,
		   ((struct ogl_vars *)dmp->dm_vars)->win);
    /* give Graphics pipe time to work */
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  if(((struct ogl_vars *)dmp->dm_vars)->mvars.debug){
    int error;
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

    while((error = glGetError())!=0){
      bu_vls_printf(&tmp_vls, "Error: %x\n", error);
    }

    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

/*XXX Keep this off unless testing */
#if 0
  glFinish();
#endif

  return TCL_OK;
}

/*
 *  			O G L _ N E W R O T
 *  load new rotation matrix onto top of stack
 */
static int
ogl_newrot(dmp, mat, which_eye)
struct dm *dmp;
mat_t mat;
int which_eye;
{
  register fastf_t *mptr;
  GLfloat gtmat[16], view[16];
  GLfloat *gtmatp;
  mat_t	newm;
  int	i;
	
  if(((struct ogl_vars *)dmp->dm_vars)->mvars.debug){
    struct bu_vls tmp_vls;

    Tcl_AppendResult(interp, "ogl_newrot()\n", (char *)NULL);

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
    bu_vls_printf(&tmp_vls, "newrot matrix = \n");
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
    glViewport(0,  0, (XMAXSCREEN)+1, ( YSTEREO)+1); 
    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
    ogl_drawString2D( dmp, "R", 2020, 0, 0, DM_RED );
    break;
  case 2:
    /* L eye */
    glViewport(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
	       ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1); 
    glScissor(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
	      ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
    break;
  }

  if( ! ((struct ogl_vars*)dmp->dm_vars)->mvars.zclipping_on ) {
    mat_t       nozclip;

    bn_mat_idn( nozclip );
    nozclip[10] = 1.0e-20;
    bn_mat_mul( newm, nozclip, mat );
    mptr = newm;
  } else {
    mptr = mat;
  }

  gtmat[0] = *(mptr++) * dmp->dm_aspect;
  gtmat[4] = *(mptr++) * dmp->dm_aspect;
  gtmat[8] = *(mptr++) * dmp->dm_aspect;
  gtmat[12] = *(mptr++) * dmp->dm_aspect;

#if 0
  gtmat[1] = *(mptr++) * dmp->dm_aspect;
  gtmat[5] = *(mptr++) * dmp->dm_aspect;
  gtmat[9] = *(mptr++) * dmp->dm_aspect;
  gtmat[13] = *(mptr++) * dmp->dm_aspect;
#else
  gtmat[1] = *(mptr++);
  gtmat[5] = *(mptr++);
  gtmat[9] = *(mptr++);
  gtmat[13] = *(mptr++);
#endif

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

  /* Make sure that new matrix is applied to the lights */
  if (((struct ogl_vars *)dmp->dm_vars)->mvars.lighting_on ){
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    glLightfv(GL_LIGHT3, GL_POSITION, light3_position);
  }

  return TCL_OK;
}



/*
 *  			O G L _ O B J E C T
 *  
 *  Set up for an object, transformed as indicated, and with an
 *  object center as specified.  The ratio of object to screen size
 *  is passed in as a convienience.
 *
 *  Returns 0 if object could be drawn, !0 if object was omitted.
 */

/* ARGSUSED */
static int
ogl_drawVList( dmp, vp )
struct dm *dmp;
register struct rt_vlist *vp;
{
  register struct rt_vlist *tvp;
  int first;
  int i,j;

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_drawVList()\n", (char *)NULL);

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
	  glEnd();
	first = 0;
	glBegin(GL_LINE_STRIP);
	glVertex3dv( *pt );
	break;
      case RT_VLIST_POLY_START:
	/* Start poly marker & normal */
	if( first == 0 )
	  glEnd();
	glBegin(GL_POLYGON);
	/* Set surface normal (vl_pnt points outward) */
	glNormal3dv( *pt );
	break;
      case RT_VLIST_LINE_DRAW:
      case RT_VLIST_POLY_MOVE:
      case RT_VLIST_POLY_DRAW:
	glVertex3dv( *pt );
	break;
      case RT_VLIST_POLY_END:
	/* Draw, End Polygon */
	glVertex3dv( *pt );
	glEnd();
	first = 1;
	break;
      case RT_VLIST_POLY_VERTNORM:
	/* Set per-vertex normal.  Given before vert. */
	glNormal3dv( *pt );
	break;
      }
    }
  }
  if( first == 0 )
    glEnd();

  return TCL_OK;
}

/*
 *			O G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 * Turns off windowing.
 */
static int
ogl_normal(dmp)
struct dm *dmp;
{
  GLint mm; 

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_normal\n", (char *)NULL);

  if (!((struct ogl_vars *)dmp->dm_vars)->face_flag){
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixd( ((struct ogl_vars *)dmp->dm_vars)->faceplate_mat );
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    ((struct ogl_vars *)dmp->dm_vars)->face_flag = 1;
    if(((struct ogl_vars *)dmp->dm_vars)->mvars.cueing_on)
      glDisable(GL_FOG);
    if (((struct ogl_vars *)dmp->dm_vars)->mvars.lighting_on)
      glDisable(GL_LIGHTING);
  }

  return TCL_OK;
}

/*
 *			O G L _ P U T S
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
ogl_drawString2D( dmp, str, x, y, size, use_aspect )
struct dm *dmp;
register char *str;
int x, y;
int size;
int use_aspect;
{
  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_drawString2D()\n", (char *)NULL);

  if(use_aspect)	
    glRasterPos2f(GED2IRIS(x) * dmp->dm_aspect,  GED2IRIS(y));
  else
    glRasterPos2f(GED2IRIS(x),  GED2IRIS(y));

  glListBase(((struct ogl_vars *)dmp->dm_vars)->fontOffset);
  glCallLists(strlen( str ), GL_UNSIGNED_BYTE,  str );

  return TCL_OK;
}


/*
 *			O G L _ 2 D _ L I N E
 *
 */
static int
ogl_drawLine2D( dmp, x1, y1, x2, y2 )
struct dm *dmp;
int x1, y1;
int x2, y2;
{
  register int nvec;
  
  if (((struct ogl_vars *)dmp->dm_vars)->mvars.debug)
    Tcl_AppendResult(interp, "ogl_drawLine2D()\n", (char *)NULL);

  if(((struct ogl_vars *)dmp->dm_vars)->mvars.debug){
    GLfloat pmat[16];
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    glGetFloatv(GL_PROJECTION_MATRIX, pmat);
    bu_vls_printf(&tmp_vls, "projection matrix:\n");
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);
    glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
    bu_vls_printf(&tmp_vls, "modelview matrix:\n");
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[0], pmat[4], pmat[8],pmat[12]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[1], pmat[5], pmat[9],pmat[13]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[2], pmat[6], pmat[10],pmat[14]);
    bu_vls_printf(&tmp_vls, "%g %g %g %g\n", pmat[3], pmat[7], pmat[11],pmat[15]);

    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  glBegin(GL_LINES); 
  glVertex2f(GED2IRIS(x1),  GED2IRIS(y1));
  glVertex2f(GED2IRIS(x2),  GED2IRIS(y2));
  glEnd();

  return TCL_OK;
}

static int
ogl_drawVertex2D(dmp, x, y)
struct dm *dmp;
int x, y;
{
  glBegin(GL_POINTS);
  glVertex2f(GED2IRIS(x), GED2IRIS(y));
  glEnd();

  return TCL_OK;
}


static int
ogl_setColor(dmp, r, g, b, strict)
struct dm *dmp;
register short r, g, b;
int strict;
{
  register int nvec;

  if(strict){
    glColor3ub( (GLubyte)r, (GLubyte)g, (GLubyte)b );
  }else{
    float material[4];
  
    if(((struct ogl_vars *)dmp->dm_vars)->mvars.lighting_on){
      /* Ambient = .2, Diffuse = .6, Specular = .2 */

      material[0] = ( r / 255.0) * .2;
      material[1] = ( g / 255.0) * .2;
      material[2] = ( b / 255.0) * .2;
      material[3] = 1.0;
      glMaterialfv(GL_FRONT, GL_AMBIENT, material);
      glMaterialfv(GL_FRONT, GL_SPECULAR, material);

      material[0] *= 3.0;
      material[1] *= 3.0;
      material[2] *= 3.0;
      glMaterialfv(GL_FRONT, GL_DIFFUSE, material);
    }else{
      glColor3ub( (GLubyte)r,  (GLubyte)g,  (GLubyte)b );
    }
  }

  return TCL_OK;
}


static int
ogl_setLineAttr(dmp, width, style)
struct dm *dmp;
int width;
int style;
{
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
static int
ogl_debug(dmp, lvl)
struct dm *dmp;
{
  ((struct ogl_vars *)dmp->dm_vars)->mvars.debug = lvl;
  XFlush(((struct ogl_vars *)dmp->dm_vars)->dpy);
  Tcl_AppendResult(interp, "flushed\n", (char *)NULL);

  return TCL_OK;
}

static int
ogl_setWinBounds(w)
register int w[];
{
  return TCL_OK;
}

#define OGL_DO_STEREO 1
/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
static XVisualInfo *
ogl_set_visual(dmp, tkwin)
struct dm *dmp;
Tk_Window tkwin;
{
  XVisualInfo *vip, vitemp, *vibase, *maxvip;
  int tries, baddepth;
  int good[40];
  int num, i, j;
  int use = 0;
  int rgba = 0;
  int dbfr = 0;
#if OGL_DO_STEREO
  int m_stereo, stereo;

  /*
   * m_stereo - try to get stereo 
   */

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

  vibase = XGetVisualInfo(((struct ogl_vars *)dmp->dm_vars)->dpy,
			  0, &vitemp, &num);

  while (1) {
    for (i=0, j=0, vip=vibase; i<num; i++, vip++){
      /* requirements */
      glXGetConfig(((struct ogl_vars *)dmp->dm_vars)->dpy,
		   vip, GLX_USE_GL, &use);
      if (!use)
	continue;

      glXGetConfig(((struct ogl_vars *)dmp->dm_vars)->dpy,
		     vip, GLX_RGBA, &rgba);
      if (!rgba)
	continue;

      glXGetConfig(((struct ogl_vars *)dmp->dm_vars)->dpy,
		     vip, GLX_DOUBLEBUFFER,&dbfr);
      if (!dbfr)
	continue;

#if OGL_DO_STEREO
      /* desires */
      if ( m_stereo ) {
	glXGetConfig(((struct ogl_vars *)dmp->dm_vars)->dpy,
		     vip, GLX_STEREO, &stereo);
	if (!stereo)
	  continue;
      }
#endif

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

	{
	  Colormap default_cmap;
	  int default_screen;

	  ((struct ogl_vars *)dmp->dm_vars)->cmap =
	    XCreateColormap(((struct ogl_vars *)dmp->dm_vars)->dpy,
			    RootWindow(((struct ogl_vars *)dmp->dm_vars)->dpy,
				       maxvip->screen), maxvip->visual, AllocNone);

	  /* Copy default colors below CMAP_BASE to private colormap to help
	     reduce flashing */
	  default_screen = DefaultScreen(((struct ogl_vars *)dmp->dm_vars)->dpy);
	  default_cmap = DefaultColormap(((struct ogl_vars *)dmp->dm_vars)->dpy,
					 default_screen);
	  dm_copy_cmap(((struct ogl_vars *)dmp->dm_vars)->dpy,
		       ((struct ogl_vars *)dmp->dm_vars)->cmap, /* destination */
		       default_cmap,                            /* source */
		       /* low, high,  use XAllocColor */
		       0, CMAP_BASE, 0);
	}

	if (Tk_SetWindowVisual(tkwin,
			       maxvip->visual, maxvip->depth,
			       ((struct ogl_vars *)dmp->dm_vars)->cmap)){
	  glXGetConfig(((struct ogl_vars *)dmp->dm_vars)->dpy, maxvip, GLX_DEPTH_SIZE,
		       &((struct ogl_vars *)dmp->dm_vars)->mvars.depth);
	  if (((struct ogl_vars *)dmp->dm_vars)->mvars.depth > 0)
	    ((struct ogl_vars *)dmp->dm_vars)->mvars.zbuf = 1;
#if 0
	  XFree((void *)vibase);
#endif
	  return (maxvip); /* success */
	} else { 
	  /* retry with lesser depth */
	  baddepth = maxvip->depth;
	  XFreeColormap(((struct ogl_vars *)dmp->dm_vars)->dpy,
			((struct ogl_vars *)dmp->dm_vars)->cmap);
	}
      }
    }

#if OGL_DO_STEREO
    /* if no success at this point, relax a desire and try again */
    if ( m_stereo ){
      m_stereo = 0;
      Tcl_AppendResult(interp, "Stereo not available.\n", (char *)NULL);
    }
#endif
#if 0
      XFree((void *)vibase);
#endif
      return(NULL); /* failure */
  }
}


/* 
 *			O G L _ C O N F I G U R E _ W I N D O W _ S H A P E
 *
 *  Either initially, or on resize/reshape of the window,
 *  sense the actual size of the window, and perform any
 *  other initializations of the window configuration.
 *
 * also change font size if necessary
 */
void
ogl_configure_window_shape(dmp)
struct dm *dmp;
{
  int		npix;
  GLint mm; 
  XWindowAttributes xwa;
  XFontStruct	*newfontstruct;

  XGetWindowAttributes( ((struct ogl_vars *)dmp->dm_vars)->dpy,
			((struct ogl_vars *)dmp->dm_vars)->win, &xwa );
  dmp->dm_height = xwa.height;
  dmp->dm_width = xwa.width;
	
  glViewport(0,  0, (dmp->dm_width),
	     (dmp->dm_height));
#if 0
  glScissor(0,  0, (dmp->dm_width)+1,
	    (dmp->dm_height)+1);
#endif

  if( ((struct ogl_vars *)dmp->dm_vars)->mvars.zbuffer_on )
    ogl_establish_zbuffer(dmp);

  ogl_establish_lighting(dmp);

#if 0
  glDrawBuffer(GL_FRONT_AND_BACK);

  glClearColor(0.0, 0.0, 0.0, 0.0);
  if (((struct ogl_vars *)dmp->dm_vars)->mvars.zbuffer_on)
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  else
    glClear( GL_COLOR_BUFFER_BIT);

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.doublebuffer)
    glDrawBuffer(GL_BACK);
  else
    glDrawBuffer(GL_FRONT);

  /*CJXX*/
  glFlush();
#else
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
  /*CJXX this might cause problems in perspective mode? */
  glGetIntegerv(GL_MATRIX_MODE, &mm);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho( -xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0 );
  glMatrixMode(mm);
  dmp->dm_aspect =
    (fastf_t)dmp->dm_height/
    (fastf_t)dmp->dm_width;


  /* First time through, load a font or quit */
  if (((struct ogl_vars *)dmp->dm_vars)->fontstruct == NULL) {
    if ((((struct ogl_vars *)dmp->dm_vars)->fontstruct =
	 XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
			FONT9)) == NULL ) {
      /* Try hardcoded backup font */
      if ((((struct ogl_vars *)dmp->dm_vars)->fontstruct =
	   XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
			  FONTBACK)) == NULL) {
	Tcl_AppendResult(interp, "dm-Ogl: Can't open font '", FONT9,
			 "' or '", FONTBACK, "'\n", (char *)NULL);
	return;
      }
    }
    glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		 0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
  }
		

  /* Always try to choose a the font that best fits the window size.
   */

  if (dmp->dm_width < 582) {
    if (((struct ogl_vars *)dmp->dm_vars)->fontstruct->per_char->width != 5) {
      if ((newfontstruct = XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
					  FONT5)) != NULL ) {
	XFreeFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
		  ((struct ogl_vars *)dmp->dm_vars)->fontstruct);
	((struct ogl_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 679) {
    if (((struct ogl_vars *)dmp->dm_vars)->fontstruct->per_char->width != 6){
      if ((newfontstruct = XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
					  FONT6)) != NULL ) {
	XFreeFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
		  ((struct ogl_vars *)dmp->dm_vars)->fontstruct);
	((struct ogl_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 776) {
    if (((struct ogl_vars *)dmp->dm_vars)->fontstruct->per_char->width != 7){
      if ((newfontstruct = XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
					  FONT7)) != NULL ) {
	XFreeFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
		  ((struct ogl_vars *)dmp->dm_vars)->fontstruct);
	((struct ogl_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
      }
    }
  } else if (dmp->dm_width < 873) {
    if (((struct ogl_vars *)dmp->dm_vars)->fontstruct->per_char->width != 8){
      if ((newfontstruct = XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
					  FONT8)) != NULL ) {
	XFreeFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
		  ((struct ogl_vars *)dmp->dm_vars)->fontstruct);
	((struct ogl_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
      }
    }
  } else {
    if (((struct ogl_vars *)dmp->dm_vars)->fontstruct->per_char->width != 9){
      if ((newfontstruct = XLoadQueryFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
					  FONT9)) != NULL ) {
	XFreeFont(((struct ogl_vars *)dmp->dm_vars)->dpy,
		  ((struct ogl_vars *)dmp->dm_vars)->fontstruct);
	((struct ogl_vars *)dmp->dm_vars)->fontstruct = newfontstruct;
	glXUseXFont( ((struct ogl_vars *)dmp->dm_vars)->fontstruct->fid,
		     0, 127, ((struct ogl_vars *)dmp->dm_vars)->fontOffset);
      }
    }
  }
}

void	
ogl_establish_lighting(dmp)
struct dm *dmp;
{
  if (!((struct ogl_vars *)dmp->dm_vars)->mvars.lighting_on) {
    /* Turn it off */
    glDisable(GL_LIGHTING);
  } else {
    /* Turn it on */

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

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
    glEnable(GL_LIGHT3);
    glEnable(GL_LIGHT2);
  }
}	

void	
ogl_establish_zbuffer(dmp)
struct dm *dmp;
{
  if( ((struct ogl_vars *)dmp->dm_vars)->mvars.zbuf == 0 ) {
    Tcl_AppendResult(interp, "dm-Ogl: This machine has no Zbuffer to enable\n",
		     (char *)NULL);
    ((struct ogl_vars *)dmp->dm_vars)->mvars.zbuffer_on = 0;
  }

  if (((struct ogl_vars *)dmp->dm_vars)->mvars.zbuffer_on)  {
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
}

void
ogl_establish_perspective(dmp)
struct dm *dmp;
{
  struct bu_vls vls;

  bu_vls_init(&vls);
  bu_vls_printf( &vls, "set perspective %d\n",
		 ((struct ogl_vars *)dmp->dm_vars)->mvars.perspective_mode ?
		 perspective_table[((struct ogl_vars *)dmp->dm_vars)->perspective_angle] :
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
ogl_set_perspective(dmp)
struct dm *dmp;
{
  /* set perspective matrix */
  if(((struct ogl_vars *)dmp->dm_vars)->mvars.dummy_perspective > 0)
    ((struct ogl_vars *)dmp->dm_vars)->perspective_angle =
      ((struct ogl_vars *)dmp->dm_vars)->mvars.dummy_perspective <= 4 ?
      ((struct ogl_vars *)dmp->dm_vars)->mvars.dummy_perspective - 1: 3;
  else if (--((struct ogl_vars *)dmp->dm_vars)->perspective_angle < 0) /* toggle perspective matrix */
    ((struct ogl_vars *)dmp->dm_vars)->perspective_angle = 3;

  if(((struct ogl_vars *)dmp->dm_vars)->mvars.perspective_mode){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf( &vls, "set perspective %d\n",
		   perspective_table[((struct ogl_vars *)dmp->dm_vars)->perspective_angle] );
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
  }

  /*
     Just in case the "!" is used with the set command. This
     allows us to toggle through more than two values.
   */
  ((struct ogl_vars *)dmp->dm_vars)->mvars.dummy_perspective = 1;
}

int
ogl_beginDList(dmp, list)
struct dm *dmp;
GLuint list;
{
  if (!glXMakeCurrent(((struct ogl_vars *)dmp->dm_vars)->dpy,
		      ((struct ogl_vars *)dmp->dm_vars)->win,
		      ((struct ogl_vars *)dmp->dm_vars)->glxc)){
    Tcl_AppendResult(interp,
 		     "ogl_beginDList: Couldn't make context current\n", (char *)NULL);
    return TCL_ERROR;
  }

  glNewList(list, GL_COMPILE);
  return TCL_OK;
}

int
ogl_endDList(dmp)
struct dm *dmp;
{
  glEndList();
  return TCL_OK;
}

int
ogl_drawDList(dmp, list)
struct dm *dmp;
GLuint list;
{
  glCallList(list);
  return TCL_OK;
}

int
ogl_freeDLists(dmp, list, range)
struct dm *dmp;
GLuint list;
GLsizei range;
{
  glDeleteLists(list, range);
  return TCL_OK;
}
