/*                        D M - R T G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @file libdm/dm-rtgl.c
 *
 * A Ray Tracing X11 OpenGL Display Manager.
 *
 */

#include "common.h"

#ifdef DM_RTGL

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif
#ifdef linux
#  undef X_NOT_STDC_ENV
#  undef X_NOT_POSIX
#endif

#include <X11/extensions/XInput.h>
#include <GL/glx.h>
#include <GL/gl.h>

#include "tk.h"

#undef VMIN		/* is used in vmath.h, too */

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm-rtgl.h"
#include "dm_xvars.h"
#include "solid.h"

#define VIEWFACTOR (1.0/(*dmp->dm_vp))
#define VIEWSIZE (2.0*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN 1279
#define YMAXSCREEN 1023
#define YSTEREO 491	/* subfield height, in scanlines */
#define YOFFSET_LEFT 532	/* YSTEREO + YBLANK ? */

#define USE_VECTOR_THRESHHOLD 0
#if USE_VECTOR_THRESHHOLD
extern int vectorThreshold;	/* defined in libdm/tcl.c */
#endif

static int rtgl_actively_drawing;
HIDDEN XVisualInfo *rtgl_choose_visual(struct dm *dmp, Tk_Window tkwin);

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */


struct dm *rtgl_open(Tcl_Interp *interp, int argc, char **argv);

HIDDEN_DM_FUNCTION_PROTOTYPES(rtgl)

struct dm dm_rtgl = {
    rtgl_close,
    rtgl_drawBegin,
    rtgl_drawEnd,
    rtgl_normal,
    rtgl_loadMatrix,
    rtgl_drawString2D,
    rtgl_drawLine2D,
    rtgl_drawLine3D,
    rtgl_drawLines3D,
    rtgl_drawPoint2D,
    rtgl_drawVList,
    rtgl_drawVList,
    rtgl_draw,
    rtgl_setFGColor,
    rtgl_setBGColor,
    rtgl_setLineAttr,
    rtgl_configureWin,
    rtgl_setWinBounds,
    rtgl_setLight,
    rtgl_setTransparency,
    rtgl_setDepthMask,
    rtgl_setZBuffer,
    rtgl_debug,
    rtgl_beginDList,
    rtgl_endDList,
    rtgl_drawDList,
    rtgl_freeDLists,
    Nu_int0, /* display to image function */
    Nu_void,
    0,
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit, */
    1,				/* bound flag */
    "rtgl",
    "X Windows with OpenGL graphics",
    DM_TYPE_RTGL,
    1,
    0,
    0,
    0, /* bytes per pixel */
    0, /* bits per channel */
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
    {GED_MIN, GED_MIN, GED_MIN}, /* clipmin */
    {GED_MAX, GED_MAX, GED_MAX}, /* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    0,				/* no lighting */
    0,				/* no transparency */
    1,				/* depth buffer is writable */
    1,				/* zbuffer */
    0,				/* no zclipping */
    0,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    0				/* Tcl interpreter */
};


static fastf_t default_viewscale = 1000.0;
static double xlim_view = 1.0;	/* args for glOrtho*/
static double ylim_view = 1.0;

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};
static float light0_position[] = {0.0, 0.0, 1.0, 0.0};
static float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0};
static float wireColor[4];
static float ambientColor[4];
static float specularColor[4];
static float diffuseColor[4];

struct rtglJobs rtgljob = {
    1,
    0,
    0,
    NULL,
    0,
    0,
    NULL,
    NULL,
    NULL,
    0,
    0
};


/* ray trace vars */
struct application app;
struct rt_i *rtip;

/* free all jobs from job list */
void
freeJobList(struct jobList *jobs)
{

    /* list cannot be empty */
    if (jobs->l.forw != NULL && (struct jobList *)jobs->l.forw != &(*jobs)) {

	while (BU_LIST_WHILE (rtgljob.currJob, jobList, &(jobs->l))) {

	    BU_LIST_DEQUEUE(&(rtgljob.currJob->l));
	    bu_free(rtgljob.currJob, "free jobs rtgljob.currJob");
	}

	jobs->l.forw = BU_LIST_NULL;
    }
}


void
rtgl_fogHint(struct dm *dmp, int fastfog)
{
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


/*
 * R T G L _ O P E N
 *
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
rtgl_open(Tcl_Interp *interp, int argc, char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int j, k;
    int make_square = -1;
    int ndevices;
    int nclass = 0;
    int unused;
    XDeviceInfoPtr olist = NULL, list = NULL;
    XDevice *dev = NULL;
    XEventClass e_class[15];
    XInputClassInfo *cip;
    struct bu_vls str;
    struct bu_vls init_proc_vls;
    Display *tmp_dpy = (Display *)NULL;
    struct dm *dmp = (struct dm *)NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    int screen_number = -1;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GET(dmp, struct dm);
    if (dmp == DM_NULL) {
	return DM_NULL;
    }

    *dmp = dm_rtgl; /* struct copy */
    dmp->dm_interp = interp;
    dmp->dm_lineWidth = 1;

    dmp->dm_vars.pub_vars = (genptr_t)bu_calloc(1, sizeof(struct dm_xvars), "rtgl_open: dm_xvars");
    if (dmp->dm_vars.pub_vars == (genptr_t)NULL) {
	bu_free(dmp, "rtgl_open: dmp");
	return DM_NULL;
    }

    dmp->dm_vars.priv_vars = (genptr_t)bu_calloc(1, sizeof(struct rtgl_vars), "rtgl_open: rtgl_vars");
    if (dmp->dm_vars.priv_vars == (genptr_t)NULL) {
	bu_free(dmp->dm_vars.pub_vars, "rtgl_open: dmp->dm_vars.pub_vars");
	bu_free(dmp, "rtgl_open: dmp");
	return DM_NULL;
    }

    dmp->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);
    bu_vls_init(&init_proc_vls);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->dm_pathName) == 0)
	bu_vls_printf(&dmp->dm_pathName, ".dm_rtgl%d", count);
    ++count;
    if (bu_vls_strlen(&dmp->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->dm_dName, ":0.0");
    }
    if (bu_vls_strlen(&init_proc_vls) == 0)
	bu_vls_strcpy(&init_proc_vls, "bind_dm");

    rtgljob.calls = 1;
    rtgljob.jobsDone = 1;

    /* initialize dm specific variables */
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devmotionnotify = LASTEvent;
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress = LASTEvent;
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease = LASTEvent;
    dmp->dm_aspect = 1.0;

    /* initialize modifiable variables */
    RTGL_MVARS.gedp = GED_NULL;
    RTGL_MVARS.needRefresh = 0;
    RTGL_MVARS.rgb = 1;
    RTGL_MVARS.doublebuffer = 1;
    RTGL_MVARS.fastfog = 1;
    RTGL_MVARS.fogdensity = 1.0;
    RTGL_MVARS.lighting_on = dmp->dm_light;
    RTGL_MVARS.zbuffer_on = dmp->dm_zbuffer;
    RTGL_MVARS.debug = dmp->dm_debugLevel;
    RTGL_MVARS.bound = dmp->dm_bound;
    RTGL_MVARS.boundFlag = dmp->dm_boundFlag;
    RTGL_MVARS.zclipping_on = dmp->dm_zclip;

    /* this is important so that rtgl_configureWin knows to set the font */
    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = NULL;

    if ((tmp_dpy = XOpenDisplay(bu_vls_addr(&dmp->dm_dName))) == NULL) {
	bu_vls_free(&init_proc_vls);
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

#ifdef HAVE_XQUERYEXTENSION
    {
	int return_val;

	if (!XQueryExtension(tmp_dpy, "GLX", &return_val, &return_val, &return_val)) {
	    bu_vls_free(&init_proc_vls);
	    (void)rtgl_close(dmp);
	    return DM_NULL;
	}
    }
#endif

    screen_number = XDefaultScreen(tmp_dpy);
    if (screen_number < 0)
	bu_log("WARNING: screen number is [%d]\n", screen_number);


    if (dmp->dm_width == 0) {
	dmp->dm_width = XDisplayWidth(tmp_dpy, screen_number) - 30;
	++make_square;
    }
    if (dmp->dm_height == 0) {
	dmp->dm_height = XDisplayHeight(tmp_dpy, screen_number) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height <
	    dmp->dm_width)
	    dmp->dm_width =
		dmp->dm_height;
	else
	    dmp->dm_height =
		dmp->dm_width;
    }

    XCloseDisplay(tmp_dpy);

    if (dmp->dm_top) {
	/* Make xtkwin a toplevel window */
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin =
	    Tk_CreateWindowFromPath(interp,
				    tkwin,
				    bu_vls_addr(&dmp->dm_pathName),
				    bu_vls_addr(&dmp->dm_dName));
	((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->dm_pathName)) {
	    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->top = tkwin;
	} else {
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

    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin == NULL) {
	bu_log("dm-Rtgl: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    bu_vls_printf(&dmp->dm_tkName, "%s",
		  (char *)Tk_Name(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin));

    bu_vls_init(&str);
    bu_vls_printf(&str, "_init_dm %V %V\n",
		  &init_proc_vls,
		  &dmp->dm_pathName);

    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy =
	Tk_Display(((struct dm_xvars *)dmp->dm_vars.pub_vars)->top);

    /* make sure there really is a display before proceeding. */
    if (!((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin,
		       dmp->dm_width,
		       dmp->dm_height);

    /* must do this before MakeExist */
    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip=rtgl_choose_visual(dmp,
									    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)) == NULL) {
	bu_log("rtgl_open: Can't get an appropriate visual.\n");
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->depth = ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.depth;

    Tk_MakeWindowExist(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win =
	Tk_WindowId(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
    dmp->dm_id = ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win;

    /* open GLX context */
    if ((((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc =
	 glXCreateContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
			  (GLXContext)NULL, GL_TRUE))==NULL) {
	bu_log("rtgl_open: couldn't create glXContext.\n");
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    /* If we used an indirect context, then as far as sgi is concerned,
     * gl hasn't been used.
     */
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->is_direct =
	(char) glXIsDirect(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			   ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc);

    /*
     * Take a look at the available input devices. We're looking
     * for "dial+buttons".
     */
    if (XQueryExtension(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, "XInputExtension", &unused, &unused, &unused)) {
	olist = list = (XDeviceInfoPtr)XListInputDevices(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, &ndevices);
    }

    if (list == (XDeviceInfoPtr)NULL ||
	list == (XDeviceInfoPtr)1) goto Done;

    for (j = 0; j < ndevices; ++j, list++) {
	if (list->use == IsXExtensionDevice) {
	    if (BU_STR_EQUAL(list->name, "dial+buttons")) {
		if ((dev = XOpenDevice(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				       list->id)) == (XDevice *)NULL) {
		    bu_log("rtgl_open: Couldn't open the dials+buttons\n");
		    goto Done;
		}

		for (cip = dev->classes, k = 0; k < dev->num_classes;
		     ++k, ++cip) {
		    switch (cip->input_class) {
#ifdef IR_BUTTONS
			case ButtonClass:
			    DeviceButtonPress(dev, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonpress,
					      e_class[nclass]);
			    ++nclass;
			    DeviceButtonRelease(dev, ((struct dm_xvars *)dmp->dm_vars.pub_vars)->devbuttonrelease,
						e_class[nclass]);
			    ++nclass;
			    break;
#endif
#ifdef IR_KNOBS
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

    Tk_MapWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_open: Couldn't make context current\n");
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	bu_log("dm-rtgl: Can't make display lists for font.\n");
	(void)rtgl_close(dmp);
	return DM_NULL;
    }

    /* This is the applications display list offset */
    dmp->dm_displaylist = ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset + 128;

    rtgl_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)rtgl_configureWin_guts(dmp, 1);

    /* Lines will be solid when stippling disabled, dashed when enabled*/
    glLineStipple(1, 0xCF33);
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
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -1.0);
    glPushMatrix();
    glLoadIdentity();
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */

    return dmp;
}


/*
 */
int
rtgl_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    GLfloat backgnd[4];
    GLfloat vf;
    GLXContext old_glxContext;

    if (dmp1 == (struct dm *)NULL)
	return TCL_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc;

	if ((((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->vip,
			      (GLXContext)NULL, GL_TRUE))==NULL) {
	    bu_log("rtgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->win,
			    ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc)) {
	    bu_log("rtgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-rtgl: Can't make display lists for font.\nUsing old context\n.");
	    ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->dm_displaylist = ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset + 128;

	rtgl_setBGColor(dmp1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->mvars.doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that rtgl_configureWin knows to set the font */
	((struct dm_xvars *)dmp1->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)rtgl_configureWin_guts(dmp1, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
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
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -1.0);
	glPushMatrix();
	glLoadIdentity();
	((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	old_glxContext = ((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->glxc;

	if ((((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->vip,
			      ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->glxc,
			      GL_TRUE))==NULL) {
	    bu_log("rtgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->win,
			    ((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->glxc)) {
	    bu_log("rtgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->fontOffset = ((struct rtgl_vars *)dmp1->dm_vars.priv_vars)->fontOffset;
	dmp2->dm_displaylist = dmp1->dm_displaylist;

	rtgl_setBGColor(dmp2, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->mvars.doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)rtgl_configureWin_guts(dmp2, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
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
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, -1.0);
	glPushMatrix();
	glLoadIdentity();
	((struct rtgl_vars *)dmp2->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, old_glxContext);
    }

    return TCL_OK;
}


/*
 * O G L _ C L O S E
 *
 * Gracefully release the display.
 */
HIDDEN int
rtgl_close(struct dm *dmp)
{
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy) {
	if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc) {
	    glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, None, NULL);
	    glXDestroyContext(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc);
	}

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)
	    XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);

	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
	    Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
    }

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "rtgl_close: rtgl_vars");
    bu_free(dmp->dm_vars.pub_vars, "rtgl_close: dm_xvars");
    bu_free(dmp, "rtgl_close: dmp");

    /* reset job count */
    rtgljob.controlClip = 1;
    rtgljob.calls = 0;
    rtgljob.jobsDone = 0;
    rtgljob.numTrees = 0;
    rtgljob.numJobs = 0;
    rtgljob.rtglWasClosed = 1;

    /* release trees */
    if (rtgljob.oldTrees != NULL)
	bu_free(rtgljob.oldTrees, "free oldTrees");
    rtgljob.oldTrees = (char **)NULL;
    rtgljob.treeCapacity = 0;

    /* free draw list */
    if (rtgljob.colorTable != NULL) {
	bu_hash_tbl_free(rtgljob.colorTable);
	rtgljob.colorTable = NULL;
    }

    rtgljob.currItem = NULL;
    rtgljob.currJob = NULL;

    return TCL_OK;
}


/* stash a new job into a dynamically allocated container */
HIDDEN void
rtgl_stashTree(struct rtglJobs *job, char *tree)
{
    static const size_t STEP = 1024;

    /* make sure there is enough room */
    if (job->treeCapacity == 0) {
	job->oldTrees = (char **)bu_calloc(STEP, sizeof(char *), "called oldTrees");
	job->treeCapacity = STEP;
    } else if (job->numTrees + 1 >= job->treeCapacity) {
	job->oldTrees = (char **)bu_realloc(job->oldTrees, (sizeof(char *) * job->treeCapacity) + STEP, "realloc oldTrees");
	job->treeCapacity += STEP;
    }

    /* add it */
    job->oldTrees[job->numTrees] = tree;
    job->numTrees++;
}


/*
 * O G L _ D R A W B E G I N
 *
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
rtgl_drawBegin(struct dm *dmp)
{
    GLfloat fogdepth;

    if (dmp->dm_debugLevel) {
	bu_log("rtgl_drawBegin\n");

	if (rtgl_actively_drawing)
	    bu_log("rtgl_drawBegin: already actively drawing\n");
    }

    rtgl_actively_drawing = 1;

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_drawBegin: Couldn't make context current\n");
	return TCL_ERROR;
    }

    /* clear back buffer */
    if (!dmp->dm_clearBufferAfter &&
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	glClearColor(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
	if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity/
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
 * O G L _ D R A W E N D
 */
HIDDEN int
rtgl_drawEnd(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawEnd\n");


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);

	if (dmp->dm_clearBufferAfter) {
	    /* give Graphics pipe time to work */
	    glClearColor(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->r,
			 ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->g,
			 ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->b,
			 0.0);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
    }

    if (dmp->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    /*XXX Keep this off unless testing */
#if 0
    glFinish();
#endif

    rtgl_actively_drawing = 0;
    return TCL_OK;
}


double startScale = 1;

/*
 * R T G L _ L O A D M A T R I X
 *
 * Load a new transformation matrix.  This will be followed by
 * many calls to rtgl_draw().
 */
HIDDEN int
rtgl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    mat_t newm;

    fastf_t clip, scale = 1;

    mat_t zclip;

    /* get ged struct */
    struct ged *gedp = RTGL_GEDP;

    if (gedp != GED_NULL) {

	/* calculate scale factor for clipping */
	scale = 1 / gedp->ged_gvp->gv_isize;

	if (startScale == 1) {
	    startScale = scale;
	}
    }

    if (dmp->dm_debugLevel) {
	struct bu_vls tmp_vls;

	bu_log("rtgl_loadMatrix()\n");

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "which eye = %d\t", which_eye);
	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
	bu_vls_printf(&tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    switch (which_eye) {
	case 0:
	    /* Non-stereo */
	    break;
	case 1:
	    /* R eye */
	    glViewport(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    rtgl_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    MAT_IDN(zclip);

    /* use z-clipping */
    if (dmp->dm_zclip) {

	/* use custom clipping to control zbuffer precision */
	if (rtgljob.controlClip) {
	    clip = scale / startScale - .75;

	    if (clip > 1)
		clip = 1;
	    if (clip < .001)
		clip = .001;

	    /* [0, 1], smaller value implies larger volume (less
	     * clipping) in z direction, but less precision
	     */
	    zclip[10] = clip;
	} else {
	    /* use default z clipping */
	    zclip[10] = dmp->dm_bound;
	}
    } else {
	/* prevent z-clipping */
	zclip[10] = 1e-20;
    }

    /* apply clip to view matrix */
    bn_mat_mul(newm, zclip, mat);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* set light position now, so that it moves with the view */
    {
	GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 };
	GLfloat white_light[] = { .5, .5, .5, 1.0 };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light);
	glLightfv(GL_LIGHT0, GL_SPECULAR, white_light);
    }

    {
	GLfloat lmodel_ambient[] = { 0.1, 0.1, 0.1, 1.0 };
	glShadeModel(GL_FLAT);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
	glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
	glLightModelf(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_RESCALE_NORMAL);

    /* apply view */
    if (rtgljob.controlClip) {
	/* move clipping volume when zooming-in
	 * to prevent clipping front surfaces
	 */
	glTranslatef(0.0, 0.0, clip - 1.75);
    } else {
	glTranslatef(0.0, 0.0, -1.0);
    }

    /* transpose to OpenGL format before applying view */
    glMultTransposeMatrixd(newm);

    return TCL_OK;
}


/* convert color vector to unsigned char array */
HIDDEN unsigned char *
getColorKey(float *color)
{
    int i, value;
    unsigned char* key = bu_malloc(sizeof(char) * KEY_LENGTH, "dm-rtgl.c: getColorKey");

    for (i = 0; i < KEY_LENGTH; i++) {
	value = color[i] * 255;
	key[i] = value;
    }

    return key;
}


/* calculate and add hit-point info to info list */
HIDDEN void
addInfo(struct application *app, struct hit *hit, struct soltab *soltab, char flip, float *partColor)
{
    point_t point;
    vect_t normal;
    int newColor;
    unsigned long index;
    unsigned char *colorKey;
    struct bu_hash_entry *prev, *entry;
    struct colorBin *bin;
    struct bu_list *head;

    /* calculate intersection point */
    VJOIN1(point, app->a_ray.r_pt, hit->hit_dist, app->a_ray.r_dir);

    /* hack fix for bad tgc surfaces */
    if (strncmp("rec", soltab->st_meth->ft_label, 3) == 0 || strncmp("tgc", soltab->st_meth->ft_label, 3) == 0) {

	/* correct invalid surface number */
	if (hit->hit_surfno < 1 || hit->hit_surfno > 3) {
	    hit->hit_surfno = 2;
	}
    }

    /* calculate normal vector */
    RT_HIT_NORMAL(normal, hit, soltab, &(app->a_ray), flip);

    /* find the table bin for this color */
    colorKey = getColorKey(partColor);
    entry = bu_find_hash_entry(rtgljob.colorTable, colorKey, KEY_LENGTH, &prev, &index);

    /* look for the correct color bin in the found entries */
    newColor = 1;

    while (entry != NULL) {
	bin = (struct colorBin *)bu_get_hash_value(entry);

	if (VEQUAL(bin->color, partColor)) {
	    newColor = 0;
	    break;
	}

	entry = entry->next;
    }

    /* have to make color bin for new color*/
    if (newColor) {

	/* create new color bin */
	bin = bu_malloc(sizeof(struct colorBin), "dm-rtgl.c: addInfo");
	VMOVE(bin->color, partColor);

	/* create bin list head */
	BU_GET(bin->list, struct ptInfoList);
	head = &(bin->list->l);
	BU_LIST_INIT(head);

	/* add first list item */
	BU_GET(rtgljob.currItem, struct ptInfoList);
	BU_LIST_PUSH(head, rtgljob.currItem);
	rtgljob.currItem->used = 0;

	/* add the new bin to the table */
	entry = bu_hash_add_entry(rtgljob.colorTable, colorKey, KEY_LENGTH, &newColor);
	bu_set_hash_value(entry, (unsigned char *)bin);
    } else {
	/* found existing color bin */

	/* get bin's current list item */
	head = &(bin->list->l);
	rtgljob.currItem = (struct ptInfoList *)head->forw;

	/* if list item is full, create a new item */
	if (rtgljob.currItem->used == PT_ARRAY_SIZE) {

	    BU_GET(rtgljob.currItem, struct ptInfoList);
	    BU_LIST_PUSH(head, rtgljob.currItem);
	    rtgljob.currItem->used = 0;
	}
    }

    /* add point and normal to bin's current list */
    rtgljob.currItem->points[X + rtgljob.currItem->used] = point[X];
    rtgljob.currItem->points[Y + rtgljob.currItem->used] = point[Y];
    rtgljob.currItem->points[Z + rtgljob.currItem->used] = point[Z];

    rtgljob.currItem->norms[X + rtgljob.currItem->used] = normal[X];
    rtgljob.currItem->norms[Y + rtgljob.currItem->used] = normal[Y];
    rtgljob.currItem->norms[Z + rtgljob.currItem->used] = normal[Z];

    rtgljob.currItem->used += 3;
}


/* add all hit point info to info list */
HIDDEN int
recordHit(struct application *app, struct partition *partH, struct seg *UNUSED(segs))
{
    struct partition *part;
    struct soltab *soltab;
    float *partColor;

    RT_CK_APPLICATION(app);

    /* add all hit points */
    for (part = partH->pt_forw; part != partH; part = part->pt_forw) {

	partColor = part->pt_regionp->reg_mater.ma_color;

	/* add "in" hit point info */
	soltab = part->pt_inseg->seg_stp;
	addInfo(app, part->pt_inhit, soltab, part->pt_inflip, partColor);

	/* add "out" hit point info (unless half-space) */
	soltab = part->pt_inseg->seg_stp;

	if (strncmp("half", soltab->st_meth->ft_label, 4) != 0) {

	    addInfo(app, part->pt_outhit, soltab, part->pt_outflip, partColor);
	}
    }

    return 1;
}


/* don't care about misses */
HIDDEN int
ignoreMiss(struct application *app)
{
    RT_CK_APPLICATION(app);
    return 0;
}


HIDDEN double
jitter(double range)
{
    if (rand() % 2)
	return fmod(rand(), range);

    return (-1 *(fmod(rand(), range)));
}


HIDDEN void
randShots(fastf_t *center, fastf_t radius, int flag)
{
    int i, j;
    vect_t view, dir;
    point_t pt;
    view[2] = 0;

    glDisable(GL_LIGHTING);

    for (i = 0; i < 360; i += 1) {
	for (j = 0; j < 90; j += 1) {
	    view[0] = i;
	    view[1] = j;

	    /* use view vector for direction */
	    bn_vec_aed(dir, view[0]*DEG2RAD, view[1]*DEG2RAD, radius);
	    VADD2(dir, dir, center);
	    VMOVE(app.a_ray.r_pt, dir);

	    /* use opposite sphere point for origin */
	    VSUB2(pt, dir, center);
	    VREVERSE(pt, pt);
	    VADD2(pt, pt, center);

	    /* jitter point */
	    VMOVE(app.a_ray.r_dir, pt);


	    if (RT_BADVEC(app.a_ray.r_dir)) {
		VPRINT("bad dir:", app.a_ray.r_dir);
	    }

	    if (RT_BADVEC(app.a_ray.r_pt)) {
		VPRINT("bad pt:", app.a_ray.r_pt);
	    } else if (flag) {
		/* shoot ray */
		rt_shootray(&app);
	    }

	    if (!flag) {
		glPointSize(5);
		glBegin(GL_POINTS);
		glColor3d(0.0, 1.0, 0.0);
		glVertex3dv(pt);
		glColor3d(1.0, 0.0, 0.0);
		glVertex3dv(dir);
		glEnd();

		glColor4d(0.0, 0.0, 1.0, .1);
		glBegin(GL_LINES);
		glVertex3dv(pt);
		glVertex3dv(dir);
		glEnd();
	    }
	}
    }

    glEnable(GL_LIGHTING);
}


HIDDEN void
swapItems(struct bu_list *a, struct bu_list *b)
{
    struct bu_list temp;

    if (a->forw == b) {
	/* a immediately followed by b */

	/* fix surrounding links */
	a->back->forw = b;
	b->forw->back = a;

	/* fix a and b links */
	a->forw = b->forw;
	b->back = a->back;
	a->back = b;
	b->forw = a;
    } else if (b->forw == a) {
	/* b immediately followed by a */

	/* fix surrounding links */
	b->back->forw = a;
	a->forw->back = b;

	/* fix a and b links */
	b->forw = a->forw;
	a->back = b->back;
	b->back = a;
	a->forw = b;
    } else {
	/* general case */

	/* fix surrounding links */
	a->back->forw = b;
	a->forw->back = b;

	b->back->forw = a;
	b->forw->back = a;

	/* switch a and b links */
	temp.forw = a->forw;
	temp.back = a->back;

	a->forw = b->forw;
	a->back = b->back;

	b->forw = temp.forw;
	b->back = temp.back;
    }
}


struct jobList **jobsArray = NULL;

/* get nth job from job list */
HIDDEN struct job*
getJob(size_t n)
{
    size_t bin, index, start;

    if (n > rtgljob.numJobs)
	return (struct job *)NULL;

    n--; /* meta-index of nth item */

    /* determine what bin holds the nth item */
    bin = (double) n / JOB_ARRAY_SIZE;

    /* meta-index of first item in bin */
    start = bin * JOB_ARRAY_SIZE;

    /* actual index of nth item in bin */
    index = n - start;

    /* get the bin link */
    rtgljob.currJob = jobsArray[bin];

    return &(rtgljob.currJob->jobs[index]);
}


/* Fisher-Yates shuffle */
HIDDEN void
shuffleJobs(void)
{
    int i;
    struct job *a, *b, temp;

    for (i = rtgljob.numJobs; i > 0; i--) {

	a = getJob(i);
	b = getJob((rand() % i) + 1);

	/* swap current and rand element */
	COPY_JOB(temp, *a);
	COPY_JOB(*a, *b);
	COPY_JOB(*b, temp);
    }
}


/* add jobs for an even grid of parallel rays in a principle direction */
HIDDEN void
shootGrid(struct jobList *jobs, vect_t min, vect_t max, double maxSpan, int pixels, int uAxis, int vAxis, int iAxis)
{
    int i, j;
    vect_t span;
    int uDivs, vDivs;
    fastf_t uWidth, vWidth;

    fastf_t uOff;
    fastf_t u, v;

    /* calculate span in each dimension */
    VSUB2(span, max, min);

    /* calculate firing intervals (trying to achieve pixel density) */
    uDivs = pixels * (span[uAxis] / maxSpan);
    vDivs = pixels * (span[vAxis] / maxSpan);

    /* provides an easy means to toggle quality during development */
#if 0
    uDivs /= 2;
    vDivs /= 2;
#endif

    uWidth = span[uAxis] / uDivs;
    vWidth = span[vAxis] / vDivs;

    /* calculate starting offsets */
    u = uOff = min[uAxis] - (uWidth / 2);
    v = min[vAxis] - (vWidth / 2);

    /* set direction */
    app.a_ray.r_dir[uAxis] = 0;
    app.a_ray.r_dir[vAxis] = 0;
    app.a_ray.r_dir[iAxis] = -1;

    app.a_ray.r_pt[iAxis] = max[iAxis] + 100;

    for (i = 0; i < vDivs; i++) {
	v += vWidth;

	for (j = 0; j < uDivs; j++) {
	    u += uWidth;

	    app.a_ray.r_pt[uAxis] = u;
	    app.a_ray.r_pt[vAxis] = v;

	    /* make new job if needed */
	    if (rtgljob.currJob->used == JOB_ARRAY_SIZE) {

		BU_GET(rtgljob.currJob, struct jobList);
		BU_LIST_PUSH(&(jobs->l), rtgljob.currJob);
		rtgljob.currJob->used = 0;
	    }

	    VMOVE(rtgljob.currJob->jobs[rtgljob.currJob->used].pt, app.a_ray.r_pt);
	    VMOVE(rtgljob.currJob->jobs[rtgljob.currJob->used].dir, app.a_ray.r_dir);

	    rtgljob.currJob->used++;

	    rtgljob.numJobs++;
	}

	/* reset u */
	u = uOff;
    }
}


int numShot = 0;

/* return 1 if all jobs done, 0 if not */
HIDDEN int
shootJobs(struct jobList *jobs)
{
    int i, last, *used;
    double elapsed_time;

    /* list cannot be empty */
    if (jobsArray != NULL) {

	/* get last non-null item */
	last = rtgljob.numJobs - numShot;
	last /= JOB_ARRAY_SIZE;
	last++;

	while (jobsArray[last] == NULL) {
	    last--;
	}

	/* last to first item */
	for (i = last; i >= 0; i--) {

	    rtgljob.currJob = jobsArray[i];
	    used = &(rtgljob.currJob->used);

	    /* shoot jobs in this array */
	    while (*used > 0) {

		VMOVE(app.a_ray.r_pt, rtgljob.currJob->jobs[*used].pt);
		VMOVE(app.a_ray.r_dir, rtgljob.currJob->jobs[*used].dir);
		rt_shootray(&app);

		numShot++;
		(*used)--;

		if (*used == 0) {
		    BU_LIST_DEQUEUE(&(rtgljob.currJob->l));
		    bu_free(rtgljob.currJob, "free jobs rtgljob.currJob");
		    jobsArray[i] = NULL;
		    break;
		}

		(void)rt_get_timer((struct bu_vls *)0, &elapsed_time);
		if (elapsed_time > .1) /* 100ms */
		    return 0;
	    }
	}

	jobs->l.forw = BU_LIST_NULL;
    }

    return 1;
}


HIDDEN void
drawPoints(float *view, int pointSize)
{
    int i, used;
    float *point, *normal, dot;
    struct colorBin *bin;
    struct bu_list *head;
    struct bu_hash_entry *entry;
    struct bu_hash_record record;

    /* get first table entry */
    if ((entry = bu_hash_tbl_first(rtgljob.colorTable, &record)) == NULL)
	return;

    /* drawing shaded points */
    glEnable(GL_LIGHTING);
    glPointSize(pointSize);

    /* for all table entries */
    do {

	/* get color bin from entry */
	bin = (struct colorBin *)bu_get_hash_value(entry);

	/* set color for bin */
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, bin->color);

	/* visit each item in bin's list */
	head = &(bin->list->l);

	for (BU_LIST_FOR (rtgljob.currItem, ptInfoList, head)) {
	    used = rtgljob.currItem->used;

#if 1
	    glBegin(GL_POINTS);
	    for (i = 0; i < used; i += 3) {

		point = &(rtgljob.currItem->points[i]);
		normal = &(rtgljob.currItem->norms[i]);

		/* draw if visible */
		dot = VDOT(view, normal);

		if (dot > 0) {
		    glNormal3fv(normal); /* MUST specify normal first */
		    glVertex3fv(point);
		}
	    }
	    glEnd();
#else
	    glNormalPointer(GL_FLOAT, 0, &(rtgljob.currItem->norms));
	    glVertexPointer(3, GL_FLOAT, 0, &(rtgljob.currItem->points));
	    glDrawArrays(GL_POINTS, 0, (used / 3));
#endif
	}
    } while ((entry = bu_hash_tbl_next(&record)) != NULL);

    glDisable(GL_LIGHTING);
    glPointSize(1);
}


vect_t min, max, center, view, vect;
fastf_t radius;
double maxSpan;
time_t start = 0;

/*
 * R T G L _ D R A W V L I S T
 *
 */
HIDDEN int
rtgl_drawVList(struct dm *dmp, struct bn_vlist *UNUSED(vp))
{
    size_t i, j, new, numNew, maxPixels, viewSize;
    vect_t span;
    char *currTree;
    struct db_i *dbip;
    struct jobList jobs;

    size_t numVisible = 0;
    size_t visibleCount = 0;
    char **visibleTrees = NULL;

    int foundalloldtrees = 1;
    int foundthistree = 0;

    /* get ged struct */
    struct ged *gedp = RTGL_GEDP;

    if (gedp == GED_NULL)
	return TCL_ERROR;

    /* get database instance */
    dbip = gedp->ged_wdbp->dbip;

    if (dbip == DBI_NULL)
	return TCL_ERROR;

    /* get new ray trace instance */
    rtip = rt_new_rti(dbip);

    if (rtip == RTI_NULL)
	return TCL_ERROR;

    /* get view dimension information */
    if (dmp->dm_height > dmp->dm_width) {
	maxPixels = dmp->dm_height;
    } else {
	maxPixels = dmp->dm_width;
    }

    viewSize = gedp->ged_gvp->gv_size;

    /* initialize draw list */
    if (rtgljob.calls == 1 || rtgljob.colorTable == NULL) {

	/* create color hash table */
	rtgljob.colorTable = bu_create_hash_tbl(START_TABLE_SIZE);
    }

    /* allocate our visible trees */
    visibleCount = ged_count_tops(gedp);
    if (visibleCount) {
	visibleTrees = (char **)bu_calloc(visibleCount, sizeof(char *), "alloc visibleTrees");

	/* get number and names of visible tree tops */
	numVisible = ged_build_tops(gedp, visibleTrees, &visibleTrees[visibleCount]);

	for (i = 0; i < rtgljob.numTrees; i++) {
	    currTree = rtgljob.oldTrees[i];
	    foundthistree = 0;
	    for (j = 0; j < numVisible; j++) {
		if (BU_STR_EQUAL(currTree, visibleTrees[j]))
		    foundthistree = 1;
	    }
	    if (foundthistree == 0) foundalloldtrees = 0;
	}

	/* display out of date */
	if (foundalloldtrees == 0) {

	    foundalloldtrees = 1;

	    /* drop previous work */
	    rtgljob.numTrees = 0;
	    freeJobList(&jobs);

	    if (rtgljob.colorTable != NULL) {
		bu_hash_tbl_free(rtgljob.colorTable);
		rtgljob.colorTable = NULL;
	    }

	    if (jobsArray != NULL) {
		bu_free(jobsArray, "dm-rtgl.c: jobsArray");
		jobsArray = NULL;
	    }

	    RTGL_DIRTY = 1;

	    maxSpan = 0.0;
	    numShot = rtgljob.numJobs = 0;
	}
    } else {
	numVisible = 0;
    }

    /* no objects are visible */
    if (numVisible == 0) {

	/* drop previous work */
	rtgljob.numTrees = 0;
	freeJobList(&jobs);

	if (rtgljob.colorTable != NULL) {
	    bu_hash_tbl_free(rtgljob.colorTable);
	    rtgljob.colorTable = NULL;
	}

	if (jobsArray != NULL) {
	    bu_free(jobsArray, "dm-rtgl.c: jobsArray");
	    jobsArray = NULL;
	}

	RTGL_DIRTY = 0;

	/* reset for dynamic z-clipping */
	if (dmp->dm_zclip) {
	    startScale = 1;
	}

	maxSpan = 0.0;
	numShot = rtgljob.numJobs = 0;

	return TCL_OK;
    }

    /* look for new trees in need of ray tracing */
    numNew = 0;

    if (rtgljob.rtglWasClosed == 1) {
	rtgljob.rtglWasClosed = 0;
	rtgljob.numTrees = 0;
	/* drop previous work */
	freeJobList(&jobs);

	if (rtgljob.colorTable != NULL) {
	    bu_hash_tbl_free(rtgljob.colorTable);
	    rtgljob.colorTable = NULL;
	}

	if (jobsArray != NULL) {
	    bu_free(jobsArray, "dm-rtgl.c: jobsArray");
	    jobsArray = NULL;
	}

	RTGL_DIRTY = 0;

	/* reset for dynamic z-clipping */
	if (dmp->dm_zclip) {
	    startScale = 1;
	}

	maxSpan = 0.0;
	numShot = rtgljob.numJobs = 0;
    }


    for (i = 0; i < numVisible; i++) {
	currTree = visibleTrees[i];
	new = 1;

	/* if this tree is in the old tree list, it's not new
	 * if it's NOT in the old list, it needs to be cleared,
	 * but that's not set up yet without clearing everything
	 * first and starting over.
	 */
	for (j = 0; j < rtgljob.numTrees; j++) {
	    if (BU_STR_EQUAL(currTree, rtgljob.oldTrees[j]))
		new = 0;
	}

	if (new) {
	    /* will ray trace new tree*/
	    if (rt_gettree(rtip, currTree) < 0)
		return TCL_ERROR;

	    /* add new tree to list of displayed */
	    numNew++;
	    rtgl_stashTree(&rtgljob, currTree);
	}
    }

    /* get points for new trees */
    if (numNew > 0) {
	/* If we're still in progress on something,
	 * adding another job is Bad given current
	 * setup - for now, punt and start over.
	 */
	if (rtgljob.numJobs != 0) {
	    freeJobList(&jobs);
	    if (rtgljob.colorTable != NULL) {
		bu_hash_tbl_free(rtgljob.colorTable);
		rtgljob.colorTable = NULL;
	    }
	    if (jobsArray != NULL) {
		bu_free(jobsArray, "dm-rtgl.c: jobsArray");
		jobsArray = NULL;
	    }
	    maxSpan = 0.0;
	    rtgljob.numTrees = 0;
	    numShot = rtgljob.numJobs = 0;
	    rtgljob.currJob = NULL;
	    numVisible = ged_build_tops(gedp, visibleTrees, &visibleTrees[visibleCount]);
	    for (i = 0; i < numVisible; i++) {
		currTree = visibleTrees[i];
		new = 1;

		/* if this tree is in the old tree list, it's not new
		 * if it's NOT in the old list, it needs to be cleared,
		 * but that's not set up yet without clearing everything
		 * first and starting over.
		 **/
		for (j = 0; j < rtgljob.numTrees; j++) {
		    if (BU_STR_EQUAL(currTree, rtgljob.oldTrees[j]))
			new = 0;
		}

		if (new) {
		    /* will ray trace new tree*/
		    if (rt_gettree(rtip, currTree) < 0)
			return TCL_ERROR;

		    /* add new tree to list of displayed */
		    numNew++;
		    rtgl_stashTree(&rtgljob, currTree);
		}
	    }

	}

	/* initialize job list */
	BU_LIST_INIT(&(jobs.l));

	BU_GET(rtgljob.currJob, struct jobList);
	BU_LIST_PUSH(&(jobs.l), rtgljob.currJob);
	rtgljob.currJob->used = 0;

	/* set up application */
	RT_APPLICATION_INIT(&app);
	app.a_onehit = 0;
	app.a_logoverlap = rt_silent_logoverlap;
	app.a_hit = recordHit;
	app.a_miss = ignoreMiss;
	app.a_rt_i = rtip;

	/* prepare for ray tracing */
	rt_prep_parallel(rtip, 1);

	/* get min and max points of bounding box */
	VMOVE(min, rtip->mdl_min);
	VMOVE(max, rtip->mdl_max);
	VSUB2(span, max, min);

	maxSpan = span[X];

	if (span[Y] > maxSpan)
	    maxSpan = span[Y];

	if (span[Z] > maxSpan)
	    maxSpan = span[Z];

	/* create ray-trace jobs */
	shootGrid(&jobs, min, max, maxSpan, maxPixels, X, Y, Z);
	shootGrid(&jobs, min, max, maxSpan, maxPixels, Z, X, Y);
	shootGrid(&jobs, min, max, maxSpan, maxPixels, Y, Z, X);

	bu_log("firing %d jobs", rtgljob.numJobs);

	/* create job array */
	jobsArray = bu_malloc(sizeof(struct jobList *) * rtgljob.numJobs, "dm-rtgl.c: jobsArray");

	i = 0;
	for (BU_LIST_FOR_BACKWARDS(rtgljob.currJob, jobList, &(jobs.l))) {
	    jobsArray[i++] = rtgljob.currJob;
	}

	start = time(NULL);

	shuffleJobs();
	/* new jobs to do */
	rtgljob.jobsDone = 0;

    } /* numNew > 0 */

    /* done with visibleTrees */
    if (visibleTrees != NULL) {
	bu_free(visibleTrees, "free visibleTrees");
	visibleTrees = NULL;
    }

    /* get view vector */
    bn_vec_aed(view, gedp->ged_gvp->gv_aet[0]*DEG2RAD, gedp->ged_gvp->gv_aet[1]*DEG2RAD, 1);

    if (difftime(time(NULL), start) > 3) {

	/* adjust point size based on zoom */
	size_t pointSize = 2;

	/* adjust point size based on % jobs completed */
	double p = (double) numShot / (double) rtgljob.numJobs;

	float fview[3];
	VMOVE(fview, view);

	if (maxSpan != 0.0) {
	    double ratio = maxSpan / viewSize;

	    pointSize = 2 * ratio;

	    if (pointSize < 1)
		pointSize = 1;
	}

	pointSize = (size_t)rint((double)pointSize / p);
	if (pointSize > (maxPixels / 50)) {
	    pointSize = maxPixels / 50;
	}

	drawPoints(fview, pointSize);
    }

    if (!rtgljob.jobsDone) {
	RTGL_DIRTY = 1;

	if ((rtgljob.jobsDone = shootJobs(&jobs))) {
	    freeJobList(&jobs);

	    if (jobsArray != NULL) {
		bu_free(jobsArray, "dm-rtgl.c: jobsArray");
		jobsArray = NULL;
	    }

	    RTGL_DIRTY = 0;

	    numShot = rtgljob.numJobs = 0;

	    bu_log("jobs done");
	}
    } else {
	RTGL_DIRTY = 0;
    }

    rtgljob.calls++;

    return TCL_OK;
}


/*
 * R T G L _ D R A W
 *
 */
HIDDEN int
rtgl_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    rtgl_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return TCL_ERROR;
	} else {
	    vp = callback_function(data);
	}
    }
    return TCL_OK;
}


/*
 * O G L _ N O R M A L
 *
 * Restore the display processor to a normal mode of operation
 * (ie, not scaled, rotated, displaced, etc).
 */
HIDDEN int
rtgl_normal(struct dm *dmp)
{

    if (dmp->dm_debugLevel)
	bu_log("rtgl_normal\n");

    if (!((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
	if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on)
	    glDisable(GL_FOG);
	if (dmp->dm_light)
	    glDisable(GL_LIGHTING);
    }

    return TCL_OK;
}


/*
 * O G L _ D R A W S T R I N G 2 D
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
rtgl_drawString2D(struct dm *dmp, char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    if (!dmp)
	return TCL_ERROR;

    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

    return TCL_OK;
}


/*
 * O G L _ D R A W L I N E 2 D
 *
 */
HIDDEN int
rtgl_drawLine2D(struct dm *dmp, fastf_t x1, fastf_t y1, fastf_t x2, fastf_t y2)
{

    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawLine2D()\n");

    if (dmp->dm_debugLevel) {
	GLfloat pmat[16];

	glGetFloatv(GL_PROJECTION_MATRIX, pmat);
	bu_log("projection matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
	glGetFloatv(GL_MODELVIEW_MATRIX, pmat);
	bu_log("modelview matrix:\n");
	bu_log("%g %g %g %g\n", pmat[0], pmat[4], pmat[8], pmat[12]);
	bu_log("%g %g %g %g\n", pmat[1], pmat[5], pmat[9], pmat[13]);
	bu_log("%g %g %g %g\n", pmat[2], pmat[6], pmat[10], pmat[14]);
	bu_log("%g %g %g %g\n", pmat[3], pmat[7], pmat[11], pmat[15]);
    }

    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();

    return TCL_OK;
}


/*
 * O G L _ D R A W L I N E 3 D
 *
 */
HIDDEN int
rtgl_drawLine3D(struct dm *dmp, point_t UNUSED(pt1), point_t UNUSED(pt2))
{
    if (!dmp)
	return TCL_ERROR;
    return TCL_OK;
}


/*
 * O G L _ D R A W L I N E S 3 D
 *
 */
HIDDEN int
rtgl_drawLines3D(struct dm *dmp, int npoints, point_t *points, int UNUSED(sflag))
{
    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;
    return TCL_OK;
}


HIDDEN int
rtgl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->dm_debugLevel) {
	bu_log("rtgl_drawPoint2D():\n");
	bu_log("\tdmp: %lu\tx - %lf\ty - %lf\n", (unsigned long)dmp, x, y);
    }

    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return TCL_OK;
}


HIDDEN int
rtgl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setFGColor()\n");

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

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

	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);

	} else {
	    glColor3ub((GLubyte)r,  (GLubyte)g,  (GLubyte)b);
	}
    }

    return TCL_OK;
}


HIDDEN int
rtgl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	    bu_log("rtgl_setBGColor: Couldn't make context current\n");
	    return TCL_ERROR;
	}

	glXSwapBuffers(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win);
	glClearColor(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    return TCL_OK;
}


HIDDEN int
rtgl_setLineAttr(struct dm *dmp, int width, int style)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setLineAttr()\n");

    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return TCL_OK;
}


/* ARGSUSED */
HIDDEN int
rtgl_debug(struct dm *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return TCL_OK;
}


HIDDEN int
rtgl_setWinBounds(struct dm *dmp, fastf_t *w)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setWinBounds()\n");

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


#define RTGL_DO_STEREO 1
/* currently, get a double buffered rgba visual that works with Tk and
 * OpenGL
 */
HIDDEN XVisualInfo *
rtgl_choose_visual(struct dm *dmp, Tk_Window tkwin)
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
#if RTGL_DO_STEREO
    int m_stereo; /* m_stereo - try to get stereo */
    int stereo;

    /*XXX Need to do something with this */
    if (dmp->dm_stereo) {
	m_stereo = 1;
    } else {
	m_stereo = 0;
    }
#endif

    memset((void *)&vitemp, 0, sizeof(XVisualInfo));
    /* Try to satisfy the above desires with a color visual of the
     * greatest depth */

    vibase = XGetVisualInfo(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    0, &vitemp, &num);

    while (1) {
	for (i=0, j=0, vip=vibase; i<num; i++, vip++) {
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
				vip, GLX_DOUBLEBUFFER, &dbfr);
	    if (fail || !dbfr)
		continue;

	    /* desires */
	    if (m_zbuffer) {
		fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    vip, GLX_DEPTH_SIZE, &zbuffer);
		if (fail || !zbuffer)
		    continue;
	    }

#if RTGL_DO_STEREO
	    if (m_stereo) {
		fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    vip, GLX_STEREO, &stereo);
		if (fail || !stereo) {
		    bu_log("rtgl_choose_visual: failed visual - GLX_STEREO\n");
		    continue;
		}
	    }
#endif

	    /* this visual meets criteria */
	    if (j >= NGOOD) {
		bu_log("rtgl_choose_visual: More than %d candidate visuals!\n", NGOOD);
		break;
	    }
	    good[j++] = i;
	}

	/* j = number of acceptable visuals under consideration */
	if (j >= 1) {
	    baddepth = 1000;
	    for (tries = 0; tries < j; ++tries) {
		maxvip = vibase + good[0];
		for (i=1; i<j; i++) {
		    vip = vibase + good[i];
		    if ((vip->depth > maxvip->depth)&&(vip->depth < baddepth)) {
			maxvip = vip;
		    }
		}

		((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap =
		    XCreateColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    RootWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
					       maxvip->screen), maxvip->visual, AllocNone);

		if (Tk_SetWindowVisual(tkwin,
				       maxvip->visual, maxvip->depth,
				       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap)) {

		    glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				 maxvip, GLX_DEPTH_SIZE,
				 &((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.depth);
		    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.depth > 0)
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf = 1;

		    return maxvip; /* success */
		} else {
		    /* retry with lesser depth */
		    baddepth = maxvip->depth;
		    XFreeColormap(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap);
		}
	    }
	}

	/* if no success at this point, relax a desire and try again */

#if RTGL_DO_STEREO
	if (m_stereo) {
	    m_stereo = 0;
	    bu_log("Stereo not available.\n");
	    continue;
	}
#endif

	if (m_zbuffer) {
	    m_zbuffer = 0;
	    continue;
	}

	return (XVisualInfo *)NULL; /* failure */
    }
}


/**
 * O G L _ C O N F I G U R E W I N
 *
 * Either initially, or on resize/reshape of the window, sense the
 * actual size of the window, and perform any other initializations of
 * the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
rtgl_configureWin_guts(struct dm *dmp, int force)
{
    GLint mm;
    XWindowAttributes xwa;
    XFontStruct *newfontstruct;

    if (dmp->dm_debugLevel)
	bu_log("rtgl_configureWin_guts()\n");

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_configureWin_guts: Couldn't make context current\n");
	return TCL_ERROR;
    }

    XGetWindowAttributes(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			 ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win, &xwa);

    /* nothing to do */
    if (!force &&
	dmp->dm_height == xwa.height &&
	dmp->dm_width == xwa.width)
	return TCL_OK;

    dmp->dm_height = xwa.height;
    dmp->dm_width = xwa.width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	bu_log("rtgl_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);

    if (dmp->dm_zbuffer)
	rtgl_setZBuffer(dmp, dmp->dm_zbuffer);

    rtgl_setLight(dmp, dmp->dm_light);

    glClearColor(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*CJXX this might cause problems in perspective mode? */
    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
    glMatrixMode(mm);

    /* First time through, load a font or quit */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct == NULL) {
	if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	     XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    FONT9)) == NULL) {
	    /* Try hardcoded backup font */
	    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
		 XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				FONTBACK)) == NULL) {
		bu_log("rtgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return TCL_ERROR;
	    }
	}
	glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    }


    /* Always try to choose a the font that best fits the window size.
     */

    if (dmp->dm_width < 582) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 5) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT5)) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 679) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 6) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT6)) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 776) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 7) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT7)) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 873) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 8) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT8)) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 9) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT9)) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    }

    return TCL_OK;
}


HIDDEN int
rtgl_configureWin(struct dm *dmp, int force)
{
    return rtgl_configureWin_guts(dmp, force);
}


HIDDEN int
rtgl_setLight(struct dm *dmp, int lighting_on)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setLight()\n");

    dmp->dm_light = lighting_on;
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_setLight: Couldn't make context current\n");
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
rtgl_setTransparency(struct dm *dmp,
		     int transparency_on)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setTransparency()\n");

    dmp->dm_transparency = transparency_on;
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on = dmp->dm_transparency;

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_setTransparency: Couldn't make context current\n");
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
rtgl_setDepthMask(struct dm *dmp,
		  int enable) {
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setDepthMask()\n");

    dmp->dm_depthMask = enable;

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_setDepthMask: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}


HIDDEN int
rtgl_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_setZBuffer: Couldn't make context current\n");
	return TCL_ERROR;
    }

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf == 0) {
	dmp->dm_zbuffer = 0;
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
    }

    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}


int
rtgl_beginDList(struct dm *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_beginDList()\n");

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_beginDList: Couldn't make context current\n");
	return TCL_ERROR;
    }

    glNewList(dmp->dm_displaylist + list, GL_COMPILE);
    return TCL_OK;
}


int
rtgl_endDList(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_endDList()\n");

    glEndList();
    return TCL_OK;
}


int
rtgl_drawDList(struct dm *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawDList()\n");

    glCallList(dmp->dm_displaylist + list);
    return TCL_OK;
}


int
rtgl_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_freeDLists()\n");

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_freeDLists: Couldn't make context current\n");
	return TCL_ERROR;
    }

    glDeleteLists(dmp->dm_displaylist + list, (GLsizei)range);
    return TCL_OK;
}


#endif /* DM_RTGL */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
