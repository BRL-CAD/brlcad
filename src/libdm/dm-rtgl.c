/*                        D M - R T G L . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2009 United States Government as represented by
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
/** @file dm-rtgl.c
 *
 *  A Ray Tracing X11 OpenGL Display Manager.
 *
 */

#include "common.h"

#ifdef DM_RTGL

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_X11_XOSDEFS_H
#  include <X11/Xfuncproto.h>
#  include <X11/Xosdefs.h>
#endif
#ifdef linux
#  undef   X_NOT_STDC_ENV
#  undef   X_NOT_POSIX
#endif

#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */

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

static int rtgl_actively_drawing;
HIDDEN XVisualInfo *rtgl_choose_visual(struct dm *dmp, Tk_Window tkwin);

/* Display Manager package interface */
#define IRBOUND	4095.9	/* Max magnification in Rot matrix */

#define PLOTBOUND	1000.0	/* Max magnification in Rot matrix */
struct dm	*rtgl_open(Tcl_Interp *interp, int argc, char **argv);
HIDDEN int	rtgl_close(struct dm *dmp);
HIDDEN int	rtgl_drawBegin(struct dm *dmp);
HIDDEN int      rtgl_drawEnd(struct dm *dmp);
HIDDEN int	rtgl_normal(struct dm *dmp), rtgl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int	rtgl_drawString2D(struct dm *dmp, register char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int	rtgl_drawLine2D(struct dm *dmp, fastf_t x1, fastf_t y1, fastf_t x2, fastf_t y2);
HIDDEN int      rtgl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int	rtgl_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int      rtgl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int	rtgl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int	rtgl_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int	rtgl_configureWin_guts(struct dm *dmp, int force);
HIDDEN int	rtgl_configureWin(struct dm *dmp);
HIDDEN int	rtgl_setLight(struct dm *dmp, int lighting_on);
HIDDEN int	rtgl_setTransparency(struct dm *dmp, int transparency_on);
HIDDEN int	rtgl_setDepthMask(struct dm *dmp, int depthMask_on);
HIDDEN int	rtgl_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int	rtgl_setWinBounds(struct dm *dmp, int *w), rtgl_debug(struct dm *dmp, int lvl);
HIDDEN int      rtgl_beginDList(struct dm *dmp, unsigned int list), rtgl_endDList(struct dm *dmp);
HIDDEN int      rtgl_drawDList(struct dm *dmp, unsigned int list);
HIDDEN int      rtgl_freeDLists(struct dm *dmp, unsigned int list, int range);

struct dm dm_rtgl = {
    rtgl_close,
    rtgl_drawBegin,
    rtgl_drawEnd,
    rtgl_normal,
    rtgl_loadMatrix,
    rtgl_drawString2D,
    rtgl_drawLine2D,
    rtgl_drawPoint2D,
    rtgl_drawVList,
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
    0,
    1,				/* has displaylist */
    0,                            /* no stereo by default */
    1.0,				/* zoom-in limit, */
    1,				/* bound flag */
    "rtgl",
    "X Windows with OpenGL graphics",
    DM_TYPE_RTGL,
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
    1,				/* depth buffer is writable */
    1,				/* zbuffer */
    0,				/* no zclipping */
    0,                          /* clear back buffer after drawing and swap */
    0				/* Tcl interpreter */
};

extern fastf_t default_viewscale;
extern double	xlim_view;	/* args for glOrtho*/
extern double	ylim_view;

/* lighting parameters */
extern float amb_three[];
extern float light0_direction[];
extern float light0_position[];
extern float light0_diffuse[];
extern float wireColor[4];
extern float ambientColor[4];
extern float specularColor[4];
extern float diffuseColor[4];
extern float backColor[];

int controlClip = 1;
int calls, jobsDone;

/* ray trace vars */
struct application app;
struct rt_i *rtip;

/* last trees drawn */
char *oldTrees[RT_MAXARGS];
int oldNumTrees = 0;

/* list of hit points */
struct ptInfoList ptInfo;
struct ptInfoList *currItem;

/* remove all points from points list */
void freeInfoList(void) {

    /* list cannot be empty */
    if (ptInfo.l.forw != NULL && (struct ptInfoList *)ptInfo.l.forw != &ptInfo) {

	struct ptInfoList *curr;
	while (BU_LIST_WHILE(curr, ptInfoList, &(ptInfo.l))) {
	    BU_LIST_DEQUEUE(&(curr->l));
	    bu_free(curr, "free ptInfoList curr");
	}

	ptInfo.l.forw = BU_LIST_NULL;
    }
}

void
rtgl_fogHint(struct dm *dmp, int fastfog)
{
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}

/*
 *			R T G L _ O P E N
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
    Display *tmp_dpy;
    struct dm *dmp = (struct dm *)NULL;
    Tk_Window tkwin;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GETSTRUCT(dmp, dm);
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

    calls = 1;
    jobsDone = 1;

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

    if (dmp->dm_width == 0) {
	dmp->dm_width =
	    DisplayWidth(tmp_dpy, DefaultScreen(tmp_dpy)) - 30;
	++make_square;
    }
    if (dmp->dm_height == 0) {
	dmp->dm_height =
	    DisplayHeight(tmp_dpy, DefaultScreen(tmp_dpy)) - 30;
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

    if ( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin == NULL ) {
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

    /* IRIX 4.0.5 bug workaround */
    if ( list == (XDeviceInfoPtr)NULL ||
	 list == (XDeviceInfoPtr)1 )  goto Done;

    for (j = 0; j < ndevices; ++j, list++) {
	if (list->use == IsXExtensionDevice) {
	    if (!strcmp(list->name, "dial+buttons")) {
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
    glGetDoublev(GL_PROJECTION_MATRIX, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -1.0);
    glPushMatrix();
    glLoadIdentity();
    ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;	/* faceplate matrix is on top of stack */

    Tk_MapWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
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
	/* dmp1 will share it's display lists with dmp2 */

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
 *  			O G L _ C L O S E
 *
 *  Gracefully release the display.
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

#if 0
	XCloseDisplay(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy);
#endif
    }

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "rtgl_close: rtgl_vars");
    bu_free(dmp->dm_vars.pub_vars, "rtgl_close: dm_xvars");
    bu_free(dmp, "rtgl_close: dmp");

    /* free points list */
    freeInfoList();

    return TCL_OK;
}

/*
 *			O G L _ D R A W B E G I N
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
 *			O G L _ D R A W E N D
 */
HIDDEN int
rtgl_drawEnd(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawEnd\n");


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_direction);
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
 *  		       R T G L _ L O A D M A T R I X
 *
 *  Load a new transformation matrix.  This will be followed by
 *  many calls to rtgl_drawVList().
 */
HIDDEN int
rtgl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    GLfloat gtmat[16];
    mat_t	newm;

    fastf_t clip, scale = 1;

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

    switch (which_eye)  {
	case 0:
	    /* Non-stereo */
	    break;
	case 1:
	    /* R eye */
	    glViewport(0,  0, (XMAXSCREEN)+1, ( YSTEREO)+1);
	    glScissor(0,  0, (XMAXSCREEN)+1, (YSTEREO)+1);
	    rtgl_drawString2D( dmp, "R", 0.986, 0.0, 0, 1 );
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
		       ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, ( XMAXSCREEN)+1,
		      ( YSTEREO+YOFFSET_LEFT)-( YOFFSET_LEFT)+1);
	    break;
    }

    mat_t zclip;
    MAT_IDN(zclip);

    /* use z-clipping */
    if (dmp->dm_zclip) {

	/* use custom clipping to control zbuffer precision */
	if (controlClip) {
	    clip = scale / startScale - .75;

	    if (clip > 1)
		clip = 1;
	    if (clip < .001)
		clip = .001;

	    /* [0, 1], smaller value implies larger volume (less
	     * clipping) in z direction, but less precision
	     */
	    zclip[10] = clip;
	}

	/* use default z clipping */
	else {
	    zclip[10] = dmp->dm_bound;
	}
    }
    
    /* prevent z-clipping */
    else {
	zclip[10] = 1e-20;
    }

    /* apply clip to view matrix */
    bn_mat_mul(newm, zclip, mat);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Initial Material Properties */
    GLfloat mat_specular[] = {.8, .8, .8, .8}; 
    GLfloat mat_shininess[] = { 5.0 }; 
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular); 
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess); 

    /* set light position now, so that it moves with the view */
    GLfloat light_position[] = { 1.0, 1.0, 1.0, 0.0 }; 
    GLfloat white_light[] = { .8, .8, .8, 1.0 }; 
    glLightfv(GL_LIGHT0, GL_POSITION, light_position); 
    glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light); 
    glLightfv(GL_LIGHT0, GL_SPECULAR, white_light); 

    glShadeModel(GL_FLAT); 
    GLfloat lmodel_ambient[] = { 0.1, 0.1, 0.1, 1.0 }; 
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient); 
    glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
    glLightModelf(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

    glEnable(GL_LIGHTING); 
    glEnable(GL_LIGHT0);
    glEnable(GL_RESCALE_NORMAL);

    /* apply view */
    if (controlClip) {
/*
	bu_log("clip: %3.2f translate:%3.2f", clip, clip - 1.25);
*/
	/* move clipping volume when zooming-in
	 * to prevent clipping front surfaces
	 */
	glTranslatef(0.0, 0.0, clip - 1.75);
    }

    else {
	glTranslatef( 0.0, 0.0, -1.0 );
    }

    /* transpose to OpenGL format before applying view */
     glMultTransposeMatrixd(newm);

    return TCL_OK;
}

/* converts degrees of azimuth/elevation into a vector */
void aeVect(fastf_t *aeVect, fastf_t *aet, fastf_t *center, double radius) {
    fastf_t azRad, elRad, crossRad;

    /* convert to radians */
    azRad = aet[0] * DEG2RAD;
    elRad = aet[1] * DEG2RAD;

    /* calculate Z */
    aeVect[Z] = radius * sin(elRad);

    /* calculate radius of this cross-section */
    crossRad = sqrt((radius * radius) - (aeVect[Z] * aeVect[Z]));

    /* calculate X and Y for this cross-section */
    aeVect[X] = crossRad * cos(azRad); 
    aeVect[Y] = crossRad * sin(azRad);

    /* apply center offset */
    aeVect[X] += center[X];
    aeVect[Y] += center[Y];
    aeVect[Z] += center[Z];
}

void aeUniformVect(fastf_t *aeVect, fastf_t *aet, fastf_t *center, double radius) {
    fastf_t azRad, elRad, crossRad, scale;

    /* convert to radians */
    azRad = aet[0] * DEG2RAD;
    elRad = aet[1] * DEG2RAD;

    /* calculate Z */
    aeVect[Z] = radius * sin(elRad);

    /* calculate radius of this cross-section */
    crossRad = sqrt((radius * radius) - (aeVect[Z] * aeVect[Z]));

    /* calculate X and Y for this cross-section */
    scale = sqrt((radius * radius) - aeVect[Z] * (aeVect[Z]));
    aeVect[X] = scale * cos(azRad); 
    aeVect[Y] = scale * sin(azRad);

    /* apply center offset */
    aeVect[X] += center[X];
    aeVect[Y] += center[Y];
    aeVect[Z] += center[Z];
}

/* calculate and add hit-point info to info list */
void addInfo(struct application *app, struct hit *hit, struct soltab *soltab, char flip, float *partColor) {
    point_t point;
    vect_t normal;
    struct ptInfoList *start;
    int newColor = 1;
    double dot;

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

    /* get vector in the right color */
    for (BU_LIST_FOR(currItem, ptInfoList, &(ptInfo.l))) {
	if (VEQUAL(currItem->color, partColor) && currItem->used < PT_ARRAY_SIZE) {
	    newColor = 0;
	    break;
	}
    }    

    /* get new list item if current is full or if starting a new color */
    if (currItem->used == PT_ARRAY_SIZE || newColor) {
	BU_GETSTRUCT(currItem, ptInfoList);
	BU_LIST_PUSH(&(ptInfo.l), currItem);
	currItem->used = 0;
	VMOVE(currItem->color, partColor);
    }

    /* add point to list */
    currItem->points[X + currItem->used] = point[X];
    currItem->points[Y + currItem->used] = point[Y];
    currItem->points[Z + currItem->used] = point[Z];

    /* add normal to list */
    currItem->norms[X + currItem->used] = normal[X];
    currItem->norms[Y + currItem->used] = normal[Y];
    currItem->norms[Z + currItem->used] = normal[Z];

    currItem->used += 3;
}

/* add all hit point info to info list */
int recordHit(struct application *app, struct partition *partH, struct seg *segs)
{
    struct partition *part;
    struct soltab *soltab;
    float *partColor;

    /* add all hit points */
    for (part = partH->pt_forw; part != partH; part = part->pt_forw) {

	partColor = part->pt_regionp->reg_mater.ma_color;

	/* add "in" hit point info */
	soltab = part->pt_inseg->seg_stp;
	addInfo(app, part->pt_inhit, soltab, part->pt_inflip, partColor);

	/* add "out" hit point info */        
	soltab = part->pt_inseg->seg_stp;

	if (strncmp("half", soltab->st_meth->ft_label, 4) != 0) { /* don't attempt to calculate half-space out point */
	
	    addInfo(app, part->pt_outhit, soltab, part->pt_outflip, partColor);
	}
    }

    return 1;
}

/* don't care about misses */
int ignoreMiss(struct application *app) {
    return 0;
}

double jitter(double range) {
    if (rand() % 2)
	return (fmod(rand(), range));

    return (-1 *(fmod(rand(), range)));
}

void randShots(fastf_t *center, fastf_t radius, int flag) {
    int i, j;
    vect_t view, dir, jit, test;
    point_t pt;
    double halfRad = radius / 2;
    view[2] = 0;

    glDisable(GL_LIGHTING);

    for (i = 0; i < 360; i += 1) {
	for (j = 0; j < 90; j += 1) {
	    view[0] = i;
	    view[1] = j;
	    
	    /* use view vector for direction */
	    aeVect(dir, view, center, radius);
	    VMOVE(app.a_ray.r_pt, dir);
	
	    /* use opposite sphere point for origin */
	    VSUB2(pt, dir, center);
	    VREVERSE(pt, pt);
	    VADD2(pt, pt, center);
	    
	    /* jitter point */
#if 0
	    VSET(jit, jitter(halfRad), jitter(halfRad), jitter(halfRad));
	    VADD2(pt, pt, jit);
#endif
	    VMOVE(app.a_ray.r_dir, pt);
	    
	    
	    if (RT_BADVEC(app.a_ray.r_dir)) {
		VPRINT("bad dir:", app.a_ray.r_dir);
	    }
	    
	    if (RT_BADVEC(app.a_ray.r_pt)) {
		VPRINT("bad pt:", app.a_ray.r_pt);
	    }
	    
	    else if (flag) {
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

static struct jobList jobs;
static struct jobList *currJob;

void swapItems(struct bu_list *a, struct bu_list *b) {
    struct bu_list temp;

    /* a immediately followed by b */
    if (a->forw == b) {

	/* fix surrounding links */
	a->back->forw = b;
	b->forw->back = a;

	/* fix a and b links */
	a->forw = b->forw;
	b->back = a->back;
	a->back = b;
	b->forw = a;
    }

    /* b immediately followed by a */
    else if (b->forw == a) {

	/* fix surrounding links */
	b->back->forw = a;
	a->forw->back = b;

	/* fix a and b links */
	b->forw = a->forw;
	a->back = b->back;
	b->back = a;
	a->forw = b;
    }

    /* general case */
    else {
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

void shuffleJobs(void) {
    int i, j, swap, numJobs = 0;
    struct bu_list *pick, *curr;

    /* list cannot be empty */
    if (jobs.l.forw != NULL && (struct jobList *)jobs.l.forw != &jobs) {

	/* count list items */
	for (BU_LIST_FOR(currJob, jobList, &(jobs.l))) {
	    numJobs++;
	}

	/* randomize list (Fisher-Yates shuffle) */
	for (i = numJobs - 1; i > 1; i--) {
	   
	    j = 0;
	    swap = (rand() % i) + 1;
	    pick = curr = NULL;

	    if (swap != i) {

		for (BU_LIST_FOR(currJob, jobList, &(jobs.l))) {
		    
		    j++;
		    
		    /* element being set */
		    if (j == i) {
			curr = &(currJob->l);
		    }
		    
		    /* random element to set it to*/
		    if (j == swap) {
			pick = &(currJob->l);
		    }
		    
		    if (pick != NULL && curr != NULL) {
			break;
		    }
		}
		
		/* swap random element into position */
		if (pick != &(jobs.l) && curr != &(jobs.l)) {
		    swapItems(pick, curr);
		} else {
		    bu_log("null");
		}
	    }
	}
    }
}

/* shoot an even grid of parallel rays in a principle direction */
void shootGrid(vect_t min, vect_t max, double maxSpan, int pixels, int uAxis, int vAxis, int iAxis) {
    int i, j;
    vect_t span;

    /* calculate span in each dimension */
    VSUB2(span, max, min);

    /* calculate firing intervals (trying to achieve pixel density) */
    int uDivs = pixels * (span[uAxis] / maxSpan);
    int vDivs = pixels * (span[vAxis] / maxSpan);

#if 1
    uDivs /= 2;
    vDivs /= 2;
#endif

    bu_log("adding %d x %d jobs", uDivs, vDivs);

    fastf_t uWidth = span[uAxis] / uDivs;
    fastf_t vWidth = span[vAxis] / vDivs;

    /* calculate starting offsets */
    fastf_t uOff;
    fastf_t u = uOff = min[uAxis] - (uWidth / 2);
    fastf_t v = min[vAxis] - (vWidth / 2);

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
	    if (currJob->used == JOB_ARRAY_SIZE) {

		BU_GETSTRUCT(currJob, jobList);
		BU_LIST_INSERT(jobs.l.back, (struct bu_list *)currJob);
		currJob->used = 0;
	    }
	    
	    VMOVE(currJob->jobs[currJob->used].pt, app.a_ray.r_pt);
	    VMOVE(currJob->jobs[currJob->used].dir, app.a_ray.r_dir);

	    currJob->used++;
	}
	
	/* reset u */
	u = uOff;
    }
}

/* return 1 if all jobs done, 0 if not */
int shootJobs(void) {
    int i;
    double elapsed_time;

    /* list cannot be empty */
    if (jobs.l.forw != NULL && (struct jobList *)jobs.l.forw != &jobs) {

	while (BU_LIST_WHILE(currJob, jobList, &(jobs.l))) {

	    /* shoot all jobs in this array */
	    for (i = 0; i < currJob->used; i++) {
		VMOVE(app.a_ray.r_pt, currJob->jobs[i].pt);
		VMOVE(app.a_ray.r_dir, currJob->jobs[i].dir);

		rt_shootray(&app);
	    }

	    /* free memory for this item */
	    BU_LIST_DEQUEUE(&(currJob->l));
	    bu_free(currJob, "free jobs currJob");

	    (void)rt_get_timer( (struct bu_vls *)0, &elapsed_time);
	    if (elapsed_time > .1) /* 100ms */
		return 0;
	}

	jobs.l.forw = BU_LIST_NULL;
    }

    return 1;
}

void drawPoints(float *view, int pointSize) {
    glPointSize(pointSize);

#if 0 /* use vertex arrays */
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
#endif
    int i, numPoints, index, count = 0;
    float normal[3];
    float dot;


    for (BU_LIST_FOR(currItem, ptInfoList, &(ptInfo.l))) {
	numPoints = currItem->used / 3;
	count += numPoints;

	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, (float *) &(currItem->color));

#if 1
	glBegin(GL_POINTS);
	for (i = 0; i < numPoints; i++) {
	    index = i + i + i;
	    
	    normal[X] = currItem->norms[X + index];
	    normal[Y] = currItem->norms[Y + index];
	    normal[Z] = currItem->norms[Z + index];

	    dot = VDOT(view, normal);
#if 0
	    /* make all normals face front*/
	    if (dot < 0) {
		VREVERSE(normal, normal);
	    }
#endif
	    /* draw if visible */
	    if (dot > 0) {
		glNormal3f(normal[X], normal[Y], normal[Z]);
		glVertex3f(currItem->points[X + index], currItem->points[Y + index], currItem->points[Z + index]);
	    }
	}
	glEnd();
#else
	glVertexPointer(3, GL_DOUBLE, 0, &(currItem->points));
        glNormalPointer(GL_DOUBLE, 0, &(currItem->norms));        
	glDrawArrays(GL_POINTS, 0, numPoints);
#endif
    }
    glDisable(GL_LIGHTING);
    glColor3d(0.0, 1.0, 0.0);
    glPointSize(1);
}

int numTops;
struct objTree *tops;
struct objTree *currObj;

int getTops(struct ged *gedp) {
    register struct directory *dp;
    register int i, j;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    int c;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* Can this be executed only sometimes?
     *  Perhaps a "dirty bit" on the database? */
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    /* Find number of possible entries and allocate memory  */
    dirp = (struct directory **)ged_dir_getspace(gedp->ged_wdbp->dbip, 0);
    dirp0 = dirp;

    if (gedp->ged_wdbp->dbip->dbi_version < 5) {
	for (i = 0; i < RT_DBNHASH; i++) {
            for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)  {
                if (dp->d_nref == 0) {
                    *dirp++ = dp;

		}
            }
	}
    } else {
        for (i = 0; i < RT_DBNHASH; i++) {
            for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
                if (dp->d_nref == 0 && (dp->d_flags != RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {

                    /* add non-hidden real object */
                    *dirp++ = dp;
                }
            }
	}
    }

    int length, numTops = (int)(dirp - dirp0);

    /* build top objects */
    tops = bu_malloc(numTops * sizeof(struct objTree), "dm-rtgl.c: getTops");

    for (i = 0; i < numTops; i++) {

	/* initialize this tree */
	INIT_OBJTREE(&(tops[i]));

	/* set tree name */
	length = strlen(dirp0[i]->d_namep) + 1;
	tops[i].name = bu_malloc(length * sizeof(char), "dm-rtgl.c: getTops");
	strncpy(tops[i].name, dirp0[i]->d_namep, length);
    }

    bu_free((genptr_t)dirp0, "wdb_tops_cmd: wdb_dir_getspace");

    return numTops;
}

int getChildren(struct ged *gedp, struct objTree *tree) {
    int i, node_count, actual_count;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, tree->name, LOOKUP_NOISY)) != DIR_NULL) {

	if (dp->d_flags != RT_DIR_COMB)
	    return 0;

	/*                                                                                                                                              
	 *  This node is a combination (a directory).
	 *  Process all the arcs (directory members).
	 */
	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting");
	    return;
	}
	
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	
	if (comb->tree) {

	    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
		db_non_union_push(comb->tree, &rt_uniresource);
		
		if (db_ck_v4gift_tree(comb->tree) < 0) {
		    bu_vls_printf(&gedp->ged_result_str, "Cannot flatten tree for listing");
		    return;
		}
	    }

	    node_count = db_tree_nleaves(comb->tree);

	    if (node_count > 0) {
		rt_tree_array = (struct rt_tree_array *) bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");
		actual_count = (struct rt_tree_array *) db_flatten_tree(rt_tree_array, comb->tree, OP_UNION, 1, &rt_uniresource) - rt_tree_array;
		BU_ASSERT_PTR(actual_count, ==, node_count);
		comb->tree = TREE_NULL;
	    } else {
		actual_count = 0;
		rt_tree_array = NULL;
	    }
	    
	    tree->numChildren = actual_count;
	    tree->children = bu_malloc(actual_count * sizeof(struct objTree), "dm-rtgl.c: getChildren");

	    for (i = 0; i < actual_count; i++) {

		INIT_OBJTREE(&(tree->children[i]));

		/* set child's parent */
		tree->children[i].parent = tree;

		/* set child's name */
		int length = strlen(rt_tree_array[i].tl_tree->tr_l.tl_name) + 1;
		tree->children[i].name = bu_malloc(length * sizeof(char), "dm-rtgl.c: getChildren");
		strncpy(tree->children[i].name, rt_tree_array[i].tl_tree->tr_l.tl_name, length);

		db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	    }
	    
	    if (rt_tree_array) {
		bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
	    }
	}
	
	rt_db_free_internal(&intern, &rt_uniresource);
    }

    return actual_count;
}

void buildTree(struct ged *gedp, struct objTree *tree) {
    int i, newTrees;

    /* get this tree's children */
    newTrees = getChildren(gedp, tree);

    /* build tree for each child */
    if (newTrees > 0) {
	for (i = 0; i < newTrees; i++) {
	    buildTree(gedp, &(tree->children[i]));
	}
    }
}


vect_t min, max, center, view, vect;
fastf_t radius;
double maxSpan;

/*
 *  			R T G L _ D R A W V L I S T
 *
 */
HIDDEN int
rtgl_drawVList(struct dm *dmp, register struct bn_vlist *vp)
{
    int i, j, new, numVisible, numNew, maxPixels, viewSize;
    vect_t span;
    char *currTree, *visibleTrees[RT_MAXARGS];
    struct ptInfoList *curr;
    
    /* get ged struct */
    struct ged *gedp = RTGL_GEDP;

    if (gedp == GED_NULL)
	return TCL_ERROR;

    /* get database instance */
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    
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

    /* initialize lists */
    if (calls == 1 || ptInfo.l.forw == BU_LIST_NULL) {
	/* initialize head */
        BU_LIST_INIT(&(ptInfo.l));
	ptInfo.used = 0;

	/* get first list item */
	BU_GETSTRUCT(currItem, ptInfoList);
	BU_LIST_PUSH(&(ptInfo.l), currItem);
	currItem->used = 0;
    }

    /* get names of all drawable objects */
    if (0) {
	numTops = getTops(gedp);

	for (i = 0; i < numTops; i++) {
	    buildTree(gedp, &tops[i]);
	}
    }

    /* get number and names of visible tree tops */
    numVisible = ged_build_tops(gedp, visibleTrees, &visibleTrees[RT_MAXARGS]);

    /* no objects are visible */
    if (numVisible == 0) {

	/* drop previous work */
	oldNumTrees = 0;
	freeInfoList();

	RTGL_DIRTY = 0;

	/* reset for dynamic z-clipping */
	if (dmp->dm_zclip) {
	    startScale = 1;
	}

	maxSpan = 0.0;

	return TCL_OK;
    }

    /* look for new trees in need of ray tracing */
    numNew = 0;
    
    for (i = 0; i < numVisible; i++) {
        currTree = visibleTrees[i];
        new = 1;
        
        /* if this tree is in the old tree list, it's not new */
        for (j = 0; j < oldNumTrees; j++) {
            if (strcmp(currTree, oldTrees[j]) == 0)
                new = 0;
        }
        
        if (new) {
	    /* will ray trace new tree*/
            if (rt_gettree(rtip, currTree) < 0)
                return TCL_ERROR;

	    /* add new tree to list of displayed */
            numNew++;
	    oldTrees[oldNumTrees++] = currTree;
        }
    }

    /* get points for new trees */
    if (numNew > 0) {

	/* initialize job list */
	BU_LIST_INIT(&(jobs.l));

	BU_GETSTRUCT(currJob, jobList);
	BU_LIST_PUSH(&(jobs.l), currJob);
	currJob->used = 0;

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

#if 1
	/* shoot grid of rays in each principle direction */
	shootGrid(min, max, maxSpan, maxPixels, X, Y, Z);
	shootGrid(min, max, maxSpan, maxPixels, Z, X, Y);
	shootGrid(min, max, maxSpan, maxPixels, Y, Z, X);
#if 1
	shuffleJobs();
#endif
#else
        /* use length of bounding box's longest diagonal as radius of bounding sphere */
	radius = sqrt((span[X] * span[X]) + (span[Y] * span[Y]) + (span[Z] * span[Z])); 

	/* use center of bounding box as center of bounding sphere */
	center[X] = (min[X] + max[X]) / 2;
	center[Y] = (min[Y] + max[Y]) / 2;
	center[Z] = (min[Z] + max[Z]) / 2;

	randShots(center, radius, 1);
#endif

	/* new jobs to do */
	jobsDone = 0;

    } /* numNew > 0 */

    /* get view vector */
    vect_t vCenter;
    VSET(vCenter, 0, 0, 0);
    aeVect(view, gedp->ged_gvp->gv_aet, vCenter, 1);

    /* determine point size */
    int pointSize = 4;

    if (maxSpan != 0.0) {
	double ratio = maxSpan / viewSize;

	pointSize = 4 * ratio;

	if (pointSize < 1)
	    pointSize = 1;
    }

    float fview[3];

    VMOVE(fview, view);

    drawPoints(fview, pointSize);
    
    if (!jobsDone) {
	RTGL_DIRTY = 1;

	if ((jobsDone = shootJobs())) {
	    bu_log("jobs done");
	}
    } else {
	RTGL_DIRTY = 0;
    }

    calls++;

    return TCL_OK;
}

/*
 *			O G L _ N O R M A L
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
	glLoadMatrixd( ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->faceplate_mat );
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
 *			O G L _ D R A W S T R I N G 2 D
 *
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
rtgl_drawString2D(struct dm *dmp, register char *str, fastf_t x, fastf_t y, int size, int use_aspect)
{
    if (dmp->dm_debugLevel)
	bu_log("rtgl_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    glCallLists(strlen( str ), GL_UNSIGNED_BYTE,  str );

    return TCL_OK;
}


/*
 *			O G L _ D R A W L I N E 2 D
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
	glColor3ub( (GLubyte)r, (GLubyte)g, (GLubyte)b );
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

#if 1
	    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuseColor);
#endif

	} else {
	    glColor3ub( (GLubyte)r,  (GLubyte)g,  (GLubyte)b );
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
rtgl_setWinBounds(struct dm *dmp, int *w)
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
    if ( dmp->dm_stereo )  {
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
	    if ( m_zbuffer ) {
		fail = glXGetConfig(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				    vip, GLX_DEPTH_SIZE, &zbuffer);
		if (fail || !zbuffer)
		    continue;
	    }

#if RTGL_DO_STEREO
	    if ( m_stereo ) {
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

#if RTGL_DO_STEREO
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
rtgl_configureWin_guts(struct dm *dmp, int force)
{
    GLint mm;
    XWindowAttributes xwa;
    XFontStruct	*newfontstruct;

    if (dmp->dm_debugLevel)
	bu_log("rtgl_configureWin_guts()\n");

    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc)) {
	bu_log("rtgl_configureWin_guts: Couldn't make context current\n");
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
	bu_log("rtgl_configureWin_guts()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);
#if 0
    glScissor(0,  0, (dmp->dm_width)+1,
	      (dmp->dm_height)+1);
#endif

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
		bu_log("rtgl_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return TCL_ERROR;
	    }
	}
	glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
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
			     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 679) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 6) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT6)) != NULL ) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 776) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 7) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT7)) != NULL ) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else if (dmp->dm_width < 873) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 8) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT8)) != NULL ) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 9) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						FONT9)) != NULL ) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont( ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			     0, 127, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    }

    return TCL_OK;
}

HIDDEN int
rtgl_configureWin(struct dm *dmp)
{
    return rtgl_configureWin_guts(dmp, 0);
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
