/*                     D M - O S G L . C P P
 * BRL-CAD
 *
 * Copyright (c) 1988-2021 United States Government as represented by
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
/** @file libdm/dm-osgl.cpp
 *
 * An OpenGL Display Manager using OpenSceneGraph.
 *
 */

#include "common.h"

//#define OSG_VIEWER_TEST 1

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wdeprecated-copy"
#endif

#include <osg/GraphicsContext>
#include <osgViewer/Viewer>

#if defined(_WIN32)
#  include <osgViewer/api/Win32/GraphicsWindowWin32>
#else
#  include <osgViewer/api/X11/GraphicsWindowX11>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "tcl.h"
#include "tk.h"
#include "tkPlatDecls.h"

#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "bview/defines.h"

#include "./fb_osgl.h"
#include "./dm-osgl.h"

#include "../include/private.h"

/* For Tk, we need to offset when thinking about screen size in
 * order to allow for the Mac OSX top-of-screen toolbar - Tk
 * itself is quite happy to put things under it */
#define TK_SCREEN_OFFSET 30

#define VIEWFACTOR      (1.0/(*dmp->i->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->i->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

extern "C" {
    struct dm *osgl_open(void *vinterp, int argc, const char **argv);
}
HIDDEN int osgl_close(struct dm *dmp);
HIDDEN int osgl_drawBegin(struct dm *dmp);
HIDDEN int osgl_drawEnd(struct dm *dmp);
HIDDEN int osgl_normal(struct dm *dmp);
HIDDEN int osgl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int osgl_loadPMatrix(struct dm *dmp, fastf_t *mat);
HIDDEN int osgl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int osgl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
HIDDEN int osgl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);
HIDDEN int osgl_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);
HIDDEN int osgl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int osgl_drawPoint3D(struct dm *dmp, point_t point);
HIDDEN int osgl_drawPoints3D(struct dm *dmp, int npoints, point_t *points);
HIDDEN int osgl_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int osgl_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int osgl_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data);
HIDDEN int osgl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int osgl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int osgl_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int osgl_configureWin_guts(struct dm *dmp, int force);
HIDDEN int osgl_configureWin(struct dm *dmp, int force);
HIDDEN int osgl_setLight(struct dm *dmp, int lighting_on);
HIDDEN int osgl_setTransparency(struct dm *dmp, int transparency_on);
HIDDEN int osgl_setDepthMask(struct dm *dmp, int depthMask_on);
HIDDEN int osgl_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int osgl_setWinBounds(struct dm *dmp, fastf_t *w);
HIDDEN int osgl_debug(struct dm *dmp, int vl);
HIDDEN int osgl_logfile(struct dm *dmp, const char *filename);
HIDDEN int osgl_beginDList(struct dm *dmp, unsigned int list);
HIDDEN int osgl_endDList(struct dm *dmp);
HIDDEN int osgl_drawDList(unsigned int list);
HIDDEN int osgl_freeDLists(struct dm *dmp, unsigned int list, int range);
HIDDEN int osgl_genDLists(struct dm *dmp, size_t range);
HIDDEN int osgl_getDisplayImage(struct dm *dmp, unsigned char **image);
HIDDEN int osgl_reshape(struct dm *dmp, int width, int height);
HIDDEN int osgl_makeCurrent(struct dm *dmp);


static fastf_t default_viewscale = 1000.0;
static double xlim_view = 1.0;	/* args for glOrtho*/
static double ylim_view = 1.0;

/* lighting parameters */
static float amb_three[] = {0.3, 0.3, 0.3, 1.0};

static float light0_position[] = {0.0, 0.0, 1.0, 0.0};
static float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
static float wireColor[4];
static float ambientColor[4];
static float specularColor[4];
static float diffuseColor[4];
static float backDiffuseColorDark[4];
static float backDiffuseColorLight[4];

HIDDEN void
osgl_printmat(struct bu_vls *tmp_vls, fastf_t *mat) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
}

HIDDEN void
osgl_printglmat(struct bu_vls *tmp_vls, GLfloat *m) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[3], m[7], m[11], m[15]);
}

extern "C" {
void
osgl_fogHint(struct dm *dmp, int fastfog)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    mvars->fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}
}

HIDDEN int
osgl_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (dmp->i->dm_debugLevel == 1)
	bu_log("osgl_setBGColor()\n");

    dmp->i->dm_bg[0] = r;
    dmp->i->dm_bg[1] = g;
    dmp->i->dm_bg[2] = b;

    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r = r / 255.0;
    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g = g / 255.0;
    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b = b / 255.0;

    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->swapBuffers();
    glClearColor(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r,
	    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g,
	    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b,
	    0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return TCL_OK;
}


/*
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 */
HIDDEN int
osgl_configureWin_guts(struct dm *dmp, int force)
{
    int width = 0;
    int height = 0;
    struct dm_osglvars *pubvars = (struct dm_osglvars *)dmp->i->dm_vars.pub_vars;
#if !defined(_WIN32)
    int bl = Tk_InternalBorderLeft(Tk_Parent(pubvars->xtkwin));
    int bt = Tk_InternalBorderTop(Tk_Parent(pubvars->xtkwin));
    width = Tk_Width(pubvars->xtkwin) + bl;
    height = Tk_Height(pubvars->xtkwin) + bt;
#else
    width = Tk_Width(pubvars->xtkwin);
    height = Tk_Height(pubvars->xtkwin);
#endif
    if (!force && dmp->i->dm_height == height && dmp->i->dm_width == width) {
	return TCL_OK;
    }
    osgl_reshape(dmp, width, height);
    return TCL_OK;
}


HIDDEN int
osgl_reshape(struct dm *dmp, int width, int height)
{
    GLint mm;

    dmp->i->dm_height = height;
    dmp->i->dm_width = width;
    dmp->i->dm_aspect = (fastf_t)dmp->i->dm_width / (fastf_t)dmp->i->dm_height;

    if (dmp->i->dm_debugLevel) {
	GLfloat m[16];
	bu_log("osgl_reshape()\n");
	bu_log("width = %d, height = %d\n", dmp->i->dm_width, dmp->i->dm_height);
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	glGetFloatv (GL_PROJECTION_MATRIX, m);
    }

    glViewport(0, 0, dmp->i->dm_width, dmp->i->dm_height);

    glClearColor(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r,
		 ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g,
		 ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glMatrixMode(mm);
#ifdef OSG_VIEWER_TEST
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;

    //struct osg_vars *privvars = (struct osg_vars *)dmp->i->dm_vars.priv_vars;
    if (privvars->testviewer) {

	osgViewer::Viewer::Windows    windows;
	privvars->testviewer->getWindows(windows);
	for(osgViewer::Viewer::Windows::iterator itr = windows.begin();
		itr != windows.end();
		++itr)
	{
	    (*itr)->setWindowRectangle(0, 0, dmp->i->dm_width, dmp->i->dm_height);
	}

	privvars->testviewer->getCamera()->setViewport(0, 0, dmp->i->dm_width, dmp->i->dm_height);

	osg::Matrixf orthom;
	orthom.makeIdentity();
	orthom.makeOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	privvars->testviewer->getCamera()->setProjectionMatrix(orthom);

	privvars->testviewer->frame();

    }
#endif
    return 0;
}


HIDDEN int
osgl_makeCurrent(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_makeCurrent()\n");

    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    return TCL_OK;
}

HIDDEN int
osgl_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    XEvent *eventPtr= (XEvent *)veventPtr;
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
        (void)dm_make_current(dmp);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        dm_set_dirty(dmp, 1);
        return TCL_OK;
    }
    /* allow further processing of this event */
    return TCL_OK;
}

HIDDEN int
osgl_configureWin(struct dm *dmp, int force)
{
    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    return osgl_configureWin_guts(dmp, force);
}


HIDDEN int
osgl_setLight(struct dm *dmp, int lighting_on)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_setLight()\n");

    dmp->i->dm_light = lighting_on;
    mvars->lighting_on = dmp->i->dm_light;

    if (!dmp->i->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	if (1 < dmp->i->dm_light)
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	else
	    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb_three);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light0_diffuse);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
    }

    return TCL_OK;
}


HIDDEN void
OSGUpdate(struct dm *dmp) {
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel == 1)
	bu_log("OSGUpdate()\n");

    if (!privvars->is_init) {
	privvars->graphicsContext->swapBuffers();
	privvars->is_init = 1;
    }
}

HIDDEN void
OSGEventProc(ClientData clientData, XEvent *UNUSED(eventPtr))
{
    struct dm *dmp = (struct dm *)clientData;

    OSGUpdate(dmp);
}

/*
 * Gracefully release the display.
 */
HIDDEN int
osgl_close(struct dm *dmp)
{
    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->makeCurrent();
    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->releaseContext();


    if (((struct dm_osglvars *)dmp->i->dm_vars.pub_vars)->xtkwin) {
	Tk_DeleteEventHandler(((struct dm_osglvars *)dmp->i->dm_vars.pub_vars)->xtkwin, VisibilityChangeMask, OSGEventProc, (ClientData)dmp);
	Tk_DestroyWindow(((struct dm_osglvars *)dmp->i->dm_vars.pub_vars)->xtkwin);
    }

    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free(dmp->i->dm_vars.priv_vars, "osgl_close: osgl_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "osgl_close: dm_osglvars");
    bu_free(dmp->i, "osgl_close: dmp impl");
    bu_free(dmp, "osgl_close: dmp");

    return TCL_OK;
}

// TODO - properly test this...
int
osgl_viable(const char *UNUSED(dpy_string))
{
    return 1;
}


/*
 * Fire up the display manager, and the display processor.
 *
 */
extern "C" struct dm *
osgl_open(void *vinterp, int argc, const char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = (struct dm *)NULL;
    struct modifiable_osgl_vars *mvars = NULL;
    Tk_Window tkwin = (Tk_Window)NULL;
    Tcl_Interp *interp = (Tcl_Interp *)vinterp;

    struct dm_osglvars *pubvars = NULL;
    struct osgl_vars *privvars = NULL;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;

    BU_GET(dmp->i, struct dm_impl);

    *dmp->i = *dm_osgl.i; /* struct copy */
    dmp->i->dm_interp = interp;
    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_light = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->i->dm_log));

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_osglvars);
    if (dmp->i->dm_vars.pub_vars == (void *)NULL) {
	bu_free(dmp, "osgl_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_osglvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct osgl_vars);
    if (dmp->i->dm_vars.priv_vars == (void *)NULL) {
	bu_free(dmp->i->dm_vars.pub_vars, "osgl_open: dmp->i->dm_vars.pub_vars");
	bu_free(dmp, "osgl_open: dmp");
	return DM_NULL;
    }
    privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;

    dmp->i->dm_get_internal(dmp);
    mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;

    dmp->i->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_osgl%d", count);
    ++count;
    if (bu_vls_strlen(&dmp->i->dm_dName) == 0) {
	char *dp;

	dp = getenv("DISPLAY");
	if (dp)
	    bu_vls_strcpy(&dmp->i->dm_dName, dp);
	else
	    bu_vls_strcpy(&dmp->i->dm_dName, ":0.0");
    }

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->i->dm_aspect = 1.0;

    /* initialize modifiable variables */
    mvars->rgb = 1;
    mvars->doublebuffer = 1;
    mvars->fastfog = 1;
    mvars->fogdensity = 1.0;
    mvars->lighting_on = dmp->i->dm_light;
    mvars->zbuffer_on = dmp->i->dm_zbuffer;
    mvars->zclipping_on = dmp->i->dm_zclip;
    mvars->debug = dmp->i->dm_debugLevel;
    mvars->bound = dmp->i->dm_bound;
    mvars->boundFlag = dmp->i->dm_boundFlag;
    mvars->text_shadow = 1;

    if (dmp->i->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin =
	    Tk_CreateWindowFromPath(interp,
				    tkwin,
				    bu_vls_addr(&dmp->i->dm_pathName),
				    bu_vls_addr(&dmp->i->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->i->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->i->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->i->dm_pathName), cp - bu_vls_addr(&dmp->i->dm_pathName));

	    pubvars->top =
		Tk_NameToWindow(interp, bu_vls_addr(&top_vls), tkwin);
	    bu_vls_free(&top_vls);
	}

	/* Make xtkwin an embedded window */
	pubvars->xtkwin =
	    Tk_CreateWindow(interp, pubvars->top,
			    cp + 1, (char *)NULL);
    }

    if (pubvars->xtkwin == NULL) {
	bu_log("dm-osgl: Failed to open %s\n", bu_vls_addr(&dmp->i->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)osgl_close(dmp);
	return DM_NULL;
    }

    /* Make sure the window is large enough.  If these values are not large
     * enough, the drawing window will not resize properly to match
     * the height change of the parent window.  This is also true
     * of the original ogl display manager, although it is undocumented *why*
     * this initial screen-based sizing is needed... */
    {
	int make_square = -1;

	if (dmp->i->dm_width == 0) {
	    bu_vls_sprintf(&str, "winfo screenwidth .");
	    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
		bu_vls_free(&init_proc_vls);
		bu_vls_free(&str);
		(void)osgl_close(dmp);
		return DM_NULL;
	    } else {
		Tcl_Obj *tclresult = Tcl_GetObjResult(interp);
		dmp->i->dm_width = tclresult->internalRep.longValue - TK_SCREEN_OFFSET;
	    }
	    ++make_square;
	}
	if (dmp->i->dm_height == 0) {
	    bu_vls_sprintf(&str, "winfo screenheight .");
	    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
		bu_vls_free(&init_proc_vls);
		bu_vls_free(&str);
		(void)osgl_close(dmp);
		return DM_NULL;
	    } else {
		Tcl_Obj *tclresult = Tcl_GetObjResult(interp);
		dmp->i->dm_height = tclresult->internalRep.longValue - TK_SCREEN_OFFSET;
	    }
	    ++make_square;
	}

	if (make_square > 0) {
	    /* Make window square */
	    if (dmp->i->dm_height < dmp->i->dm_width)
		dmp->i->dm_width = dmp->i->dm_height;
	    else
		dmp->i->dm_height = dmp->i->dm_width;
	}
    }

    bu_vls_printf(&dmp->i->dm_tkName, "%s",
		  (char *)Tk_Name(pubvars->xtkwin));

    if (bu_vls_strlen(&init_proc_vls) > 0) {
	/* Important - note that this is a bu_vls_sprintf, to clear the string */
	bu_vls_sprintf(&str, "%s %s\n", bu_vls_addr(&init_proc_vls), bu_vls_addr(&dmp->i->dm_pathName));

	if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	    bu_vls_free(&init_proc_vls);
	    bu_vls_free(&str);
	    (void)osgl_close(dmp);
	    return DM_NULL;
	}
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy =
	Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!(pubvars->dpy)) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)osgl_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->i->dm_width,
		       dmp->i->dm_height);

    pubvars->depth = mvars->depth;

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->i->dm_id = pubvars->win;

    Tk_MapWindow(pubvars->xtkwin);

    // Init the Windata Variable that holds the handle for the Window to display OSG in.
    // Check the QOSGWidget.cpp example for more logic relevant to this.  Need to find
    // something showing how to handle Cocoa for the Mac, if that's possible
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
#if defined(_WIN32)
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowWin32::WindowData(Tk_GetHWND(((struct dm_osglvars *)(dmp->i->dm_vars.pub_vars))->win));
#else
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowX11::WindowData(((struct dm_osglvars *)(dmp->i->dm_vars.pub_vars))->win);
#endif

    // Although we are not making direct use of osgViewer currently, we need its
    // initialization to make sure we have all the libraries we need loaded and
    // ready.
    osgViewer::Viewer *viewer = new osgViewer::Viewer();
    delete viewer;

    // For recent Gnome 3 - see https://github.com/openscenegraph/OpenSceneGraph/issues/615
    traits->readDISPLAY();
    traits->setUndefinedScreenDetailsToDefaultScreen();

    // Setup the traits parameters
    traits->x = 0;
    traits->y = 0;
    traits->width = dmp->i->dm_width;
    traits->height = dmp->i->dm_height;
    traits->depth = 24;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->setInheritedWindowPixelFormat = true;

    traits->inheritedWindowData = windata;

    // Create the Graphics Context

    privvars->graphicsContext = osg::GraphicsContext::createGraphicsContext(traits.get());
    privvars->traits = traits;

    privvars->graphicsContext->realize();
    privvars->graphicsContext->makeCurrent();

    privvars->timer = new osg::Timer;
    privvars->last_update_time = 0;
    privvars->timer->setStartTick();

    /* this is where font information is set up */
    privvars->fs = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
    if (privvars->fs == NULL) {
	bu_log("dm-osgl: Failed to create font stash");
	bu_vls_free(&init_proc_vls);
	(void)osgl_close(dmp);
	return DM_NULL;
    }
    privvars->fontNormal = FONS_INVALID;
    privvars->fontNormal = fonsAddFont(privvars->fs, "sans", bu_dir(NULL, 0, BU_DIR_DATA, "fonts", "ProFont.ttf", NULL));

    /* This is the applications display list offset */
    dmp->i->dm_displaylist = glGenLists(0);

    osgl_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)osgl_configureWin_guts(dmp, 1);

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
    glGetDoublev(GL_PROJECTION_MATRIX, privvars->faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    privvars->face_flag = 1;	/* faceplate matrix is on top of stack */

    osgl_setZBuffer(dmp, dmp->i->dm_zbuffer);
    osgl_setLight(dmp, dmp->i->dm_light);

    //Tk_CreateEventHandler(pubvars->xtkwin, PointerMotionMask|ExposureMask|StructureNotifyMask|FocusChangeMask|VisibilityChangeMask|ButtonReleaseMask, OSGEventProc, (ClientData)dmp);
    Tk_CreateEventHandler(pubvars->xtkwin, VisibilityChangeMask, OSGEventProc, (ClientData)dmp);

    privvars->is_init = 0;


#ifdef OSG_VIEWER_TEST
    privvars->testviewer = new osgViewer::Viewer();
    privvars->testviewer->setUpViewInWindow(0, 0, 1, 1);
    privvars->testviewer->realize();

    privvars->osg_root = new osg::Group();
    privvars->testviewer->setSceneData(privvars->osg_root);
    privvars->testviewer->getCamera()->setCullingMode(osg::CullSettings::NO_CULLING);

    privvars->testviewer->frame();
#endif
    return dmp;
}


int
osgl_share_dlist(struct dm *UNUSED(dmp1), struct dm *UNUSED(dmp2))
{
#if 0
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp1->i->m_vars;
    GLfloat backgnd[4];
    GLfloat vf;
    GLXContext old_glxContext;

    if (dmp1 == (struct dm *)NULL)
	return TCL_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc;

	if ((((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->dpy,
			      ((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->vip,
			      (GLXContext)NULL, GL_TRUE))==NULL) {
	    bu_log("osgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->dpy,
			    ((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->win,
			    ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc)) {
	    bu_log("osgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-osgl: Can't make display lists for font.\nUsing old context\n.");
	    ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->i->dm_displaylist = ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset + 128;

	osgl_setBGColor(dmp1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that osgl_configureWin knows to set the font */
	((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)osgl_configureWin_guts(dmp1, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp1->i->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_osglvars *)dmp1->i->dm_vars.pub_vars)->dpy, old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	old_glxContext = ((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc;

	if ((((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->dpy,
			      ((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->vip,
			      ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->glxc,
			      GL_TRUE))==NULL) {
	    bu_log("osgl_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->dpy,
			    ((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->win,
			    ((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc)) {
	    bu_log("osgl_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->fontOffset = ((struct osgl_vars *)dmp1->i->dm_vars.priv_vars)->fontOffset;
	dmp2->i->dm_displaylist = dmp1->i->dm_displaylist;

	osgl_setBGColor(dmp2, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (mvars->doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)osgl_configureWin_guts(dmp2, 1);

	/* Lines will be solid when stippling disabled, dashed when enabled*/
	glLineStipple(1, 0xCF33);
	glDisable(GL_LINE_STIPPLE);

	backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0);
	glFogf(GL_FOG_END, 2.0);
	glFogfv(GL_FOG_COLOR, backgnd);

	/*XXX Need to do something about VIEWFACTOR */
	vf = 1.0/(*dmp2->i->dm_vp);
	glFogf(GL_FOG_DENSITY, vf);

	/* Initialize matrices */
	/* Leave it in model_view mode normally */
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, 0.0, 2.0);
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct osgl_vars *)dmp2->i->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_osglvars *)dmp2->i->dm_vars.pub_vars)->dpy, old_glxContext);
    }
#endif
    return TCL_OK;
}


/*
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
osgl_drawBegin(struct dm *dmp)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    GLfloat fogdepth;

    if (dmp->i->dm_debugLevel) {
	bu_log("osgl_drawBegin\n");
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "initial view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "initial projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    /* clear back buffer */
    if (!dmp->i->dm_clearBufferAfter && mvars->doublebuffer) {
	glClearColor(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r,
		     ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g,
		     ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->face_flag = 0;
	if (mvars->cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->i->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*mvars->fogdensity/
				  (*dmp->i->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->i->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (dmp->i->dm_light) {
	    glEnable(GL_LIGHTING);
	}
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "after begin view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "after begin projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return TCL_OK;
}


HIDDEN int
osgl_drawEnd(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_drawEnd\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of end view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of end projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->i->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->graphicsContext->swapBuffers();
    if (dmp->i->dm_clearBufferAfter) {
	/* give Graphics pipe time to work */
	glClearColor(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r,
		((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g,
		((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b,
		0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (dmp->i->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of drawend view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of drawend projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

#ifdef OSG_VIEWER_TEST
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;
    privvars->testviewer->frame();
#endif
    return TCL_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to osgl_draw().
 */
HIDDEN int
osgl_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    if (dmp->i->dm_debugLevel == 1)
	bu_log("osgl_loadMatrix()\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->i->dm_debugLevel == 3) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	osgl_printmat(&tmp_vls, mat);

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
	    osgl_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
	    break;
	case 2:
	    /* L eye */
	    glViewport(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		       (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    glScissor(0,  0+YOFFSET_LEFT, (XMAXSCREEN)+1,
		      (YSTEREO+YOFFSET_LEFT)-(YOFFSET_LEFT)+1);
	    break;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[5] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[9] = *(mptr++) * dmp->i->dm_aspect;
    gtmat[13] = *(mptr++) * dmp->i->dm_aspect;

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
    glLoadMatrixf(gtmat);

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


#ifdef OSG_VIEWER_TEST
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;
    mat_t glmat;
    glmat[0] = mat[0];
    glmat[4] = mat[1];
    glmat[8] = mat[2];
    glmat[12] = mat[3];

    glmat[1] = mat[4] * dmp->i->dm_aspect;
    glmat[5] = mat[5] * dmp->i->dm_aspect;
    glmat[9] = mat[6] * dmp->i->dm_aspect;
    glmat[13] = mat[7] * dmp->i->dm_aspect;

    glmat[2] = mat[8];
    glmat[6] = mat[9];
    glmat[10] = mat[10];
    glmat[14] = mat[11];

    glmat[3] = mat[12];
    glmat[7] = mat[13];
    glmat[11] = mat[14];
    glmat[15] = mat[15];

    osg::Matrix osg_mp(
	    glmat[0], glmat[1], glmat[2],  glmat[3],
	    glmat[4], glmat[5], glmat[6],  glmat[7],
	    glmat[8], glmat[9], glmat[10], glmat[11],
	    glmat[12], glmat[13], glmat[14], glmat[15]);

    privvars->testviewer->getCamera()->getViewMatrix().set(osg_mp);
    privvars->testviewer->frame();
#endif

    return TCL_OK;
}


/*
 * Load a new projection matrix.
 *
 */
HIDDEN int
osgl_loadPMatrix(struct dm *dmp, fastf_t *mat)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    glMatrixMode(GL_PROJECTION);

    if (mat == (fastf_t *)NULL) {
	if (((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->face_flag) {
	    glPopMatrix();
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	    glPushMatrix();
	    glLoadMatrixd(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->faceplate_mat);
	} else {
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	}

	return TCL_OK;
    }

    mptr = mat;

    gtmat[0] = *(mptr++);
    gtmat[4] = *(mptr++);
    gtmat[8] = *(mptr++);
    gtmat[12] = *(mptr++);

    gtmat[1] = *(mptr++);
    gtmat[5] = *(mptr++);
    gtmat[9] = *(mptr++);
    gtmat[13] = *(mptr++);

    gtmat[2] = *(mptr++);
    gtmat[6] = *(mptr++);
    gtmat[10] = -*(mptr++);
    gtmat[14] = -*(mptr++);

    gtmat[3] = *(mptr++);
    gtmat[7] = *(mptr++);
    gtmat[11] = *(mptr++);
    gtmat[15] = *(mptr++);

    glLoadIdentity();
    glLoadMatrixf(gtmat);

#ifdef OSG_VIEWER_TEST
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;
    mat_t glmat;
    glmat[0] = mat[0];
    glmat[4] = mat[1];
    glmat[8] = mat[2];
    glmat[12] = mat[3];

    glmat[1] = mat[4];
    glmat[5] = mat[5];
    glmat[9] = mat[6];
    glmat[13] = mat[7];

    glmat[2] = mat[8];
    glmat[6] = mat[9];
    glmat[10] = -mat[10];
    glmat[14] = -mat[11];

    glmat[3] = mat[12];
    glmat[7] = mat[13];
    glmat[11] = mat[14];
    glmat[15] = mat[15];

    osg::Matrix osg_mp(
	    glmat[0], glmat[1], glmat[2], glmat[3],
	    glmat[4], glmat[5], glmat[6], glmat[7],
	    glmat[8], glmat[9], glmat[10], glmat[11],
	    glmat[12], glmat[13], glmat[14], glmat[15]);

    privvars->testviewer->getCamera()->setProjectionMatrix(osg_mp);
    privvars->testviewer->frame();
#endif
    return TCL_OK;
}


HIDDEN int
osgl_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    register struct bn_vlist *tvp;
    register int first;

    if (dmp->i->dm_debugLevel == 1)
	bu_log("osgl_drawVList()\n");


    /* First, draw polygons using background color. */

    if (dmp->i->dm_light) {
	glDisable(GL_LIGHTING);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glPolygonOffset(1.0, 1.0);

    /* Set color to background color for drawing polygons. */
    glColor3f(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->r,
	      ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->g,
	      ((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->b);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		case BN_VLIST_LINE_DRAW:
		    break;
		case BN_VLIST_POLY_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_POLYGON);
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_TRI_START:
		    if (first == 1) {
			glBegin(GL_TRIANGLES);
			first = 0;
		    }
		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    break;
		case BN_VLIST_TRI_END:
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    /* Last, draw wireframe/edges. */

    /* Set color to wireColor for drawing wireframe/edges */
    glColor3f(wireColor[0], wireColor[1], wireColor[2]);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	register int i;
	register int nused = tvp->nused;
	register int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt); /* fastf_t-to-double */
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */
		    if (first == 0)
			glEnd();
		    first = 0;

		    glBegin(GL_LINE_STRIP);
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		case BN_VLIST_TRI_END:
		    /* Draw, End Polygon */
		    glVertex3dv(dpt);
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->i->dm_light) {
	glEnable(GL_LIGHTING);
    }

    if (!mvars->zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->i->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    return TCL_OK;
}


int
osgl_draw_data_axes(struct dm *dmp,
                  fastf_t sf,
                  struct bview_data_axes_state *bndasp)
{
    int npoints = bndasp->num_points * 6;
    if (npoints < 1)
        return 0;

    /* set color */
    dm_set_fg(dmp, bndasp->color[0], bndasp->color[1], bndasp->color[2], 1, 1.0);

    if (bndasp->draw > 1) {
        if (dmp->i->dm_light)
            glDisable(GL_LIGHTING);

        glPointSize(bndasp->size);
        dm_draw_points_3d(dmp, bndasp->num_points, bndasp->points);
        glPointSize(1);

        if (dmp->i->dm_light)
            glEnable(GL_LIGHTING);

	return 0;
    }

    int i, j;
    fastf_t halfAxesSize;               /* half the length of an axis */
    point_t ptA, ptB;
    point_t *points;
    /* Save the line attributes */
    int saveLineWidth = dmp->i->dm_lineWidth;
    int saveLineStyle = dmp->i->dm_lineStyle;

    points = (point_t *)bu_calloc(npoints, sizeof(point_t), "data axes points");
    halfAxesSize = bndasp->size * 0.5 * sf;

    /* set linewidth */
    dm_set_line_attr(dmp, bndasp->line_width, 0);  /* solid lines */

    for (i = 0, j = -1; i < bndasp->num_points; ++i) {
	/* draw X axis with x/y offsets */
	VSET(ptA, bndasp->points[i][X] - halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	VSET(ptB, bndasp->points[i][X] + halfAxesSize, bndasp->points[i][Y], bndasp->points[i][Z]);
	++j;
	VMOVE(points[j], ptA);
	++j;
	VMOVE(points[j], ptB);
        /* draw Y axis with x/y offsets */
        VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y] - halfAxesSize, bndasp->points[i][Z]);
        VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y] + halfAxesSize, bndasp->points[i][Z]);
        ++j;
        VMOVE(points[j], ptA);
        ++j;
        VMOVE(points[j], ptB);

        /* draw Z axis with x/y offsets */
        VSET(ptA, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] - halfAxesSize);
        VSET(ptB, bndasp->points[i][X], bndasp->points[i][Y], bndasp->points[i][Z] + halfAxesSize);
        ++j;
        VMOVE(points[j], ptA);
        ++j;
        VMOVE(points[j], ptB);
    }

    dm_draw_lines_3d(dmp, npoints, points, 0);
    bu_free((void *)points, "data axes points");

    /* Restore the line attributes */
    dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);


    return 0;
}


HIDDEN int
osgl_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    struct bn_vlist *tvp;
    register int first;
    register int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalPointSize, originalLineWidth;
    GLfloat m[16];

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    if (dmp->i->dm_debugLevel == 1)
	bu_log("osgl_drawVList()\n");

    /* OGL dm appears to have this set already, but we need to
     * do it here to match the default appearance of the wireframes
     * with OSG. */
    glEnable(GL_DEPTH_TEST);

    /* Viewing region is from -1.0 to +1.0 */
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    GLdouble dpt[3];
	    VMOVE(dpt, *pt);
/*
	    if (dmp->i->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0)
			glEnd();
		    first = 0;

		    if (dmp->i->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, wireColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, black);

			if (dmp->i->dm_transparency)
			    glDisable(GL_BLEND);
		    }

		    glBegin(GL_LINE_STRIP);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_MODEL_MAT:
		    glMatrixMode(GL_PROJECTION);
		    glLoadIdentity();
		    glLoadMatrixf(m);
		    break;
		case BN_VLIST_DISPLAY_MAT:
		    glMatrixMode(GL_PROJECTION);
		    glGetFloatv (GL_PROJECTION_MATRIX, m);
		    glPopMatrix();
		    glLoadIdentity();
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */

		    if (dmp->i->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

			switch (dmp->i->dm_light) {
			    case 1:
				break;
			    case 2:
				glMaterialfv(GL_BACK, GL_DIFFUSE, diffuseColor);
				break;
			    case 3:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorDark);
				break;
			    default:
				glMaterialfv(GL_BACK, GL_DIFFUSE, backDiffuseColorLight);
				break;
			}

			if (dmp->i->dm_transparency)
			    glEnable(GL_BLEND);
		    }

		    if (*cmd == BN_VLIST_POLY_START) {
			if (first == 0)
			    glEnd();

			glBegin(GL_POLYGON);
		    } else if (first)
			glBegin(GL_TRIANGLES);

		    /* Set surface normal (vl_pnt points outward) */
		    glNormal3dv(dpt);

		    first = 0;

		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		case BN_VLIST_TRI_MOVE:
		case BN_VLIST_TRI_DRAW:
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_END:
		    /* Draw, End Polygon */
		    glEnd();
		    first = 1;
		    break;
		case BN_VLIST_TRI_END:
		    break;
		case BN_VLIST_POLY_VERTNORM:
		case BN_VLIST_TRI_VERTNORM:
		    /* Set per-vertex normal.  Given before vert. */
		    glNormal3dv(dpt);
		    break;
		case BN_VLIST_POINT_DRAW:
		    if (first == 0)
			glEnd();
		    first = 0;
		    glEnable(GL_POINT_SMOOTH);
		    glBegin(GL_POINTS);
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_LINE_WIDTH: {
		    GLfloat lineWidth = (GLfloat)(*pt)[0];
		    if (lineWidth > 0.0) {
			glLineWidth(lineWidth);
		    }
		    break;
		}
		case BN_VLIST_POINT_SIZE: {
		    GLfloat pointSize = (GLfloat)(*pt)[0];
		    if (pointSize > 0.0) {
			glPointSize(pointSize);
		    }
		    break;
		}
	    }
	}
    }

    if (first == 0)
	glEnd();

    if (dmp->i->dm_light && dmp->i->dm_transparency)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

#ifdef OSG_VIEWER_TEST
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    // create the osg containers to hold our data.
    osg::ref_ptr<osg::Geode> geode = new osg::Geode(); // Maybe create this at drawBegin?
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray;
    osg::ref_ptr<osg::Vec3dArray> normals = new osg::Vec3dArray;

    // Set line color
    osg::Vec4Array* line_color = new osg::Vec4Array;
    line_color->push_back(osg::Vec4(255, 255, 100, 70));
    geom->setColorArray(line_color, osg::Array::BIND_OVERALL);

    // Set wireframe state
    osg::StateSet *geom_state = geom->getOrCreateStateSet();
    osg::ref_ptr<osg::PolygonMode> geom_polymode = new osg::PolygonMode;
    geom_polymode->setMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE);
    geom_state->setAttributeAndModes(geom_polymode);
    geom_state->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);

    /* Viewing region is from -1.0 to +1.0 */
    int begin = 0;
    int nverts = 0;
    first = 1;
    for (BU_LIST_FOR(tvp, bn_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0) {
			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));

		    } else
			first = 0;

		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    normals->push_back(osg::Vec3(0.0f,0.0f,1.0f));
		    begin += nverts;
		    nverts = 1;
		    break;
		case BN_VLIST_POLY_START:
		    normals->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    begin += nverts;
		    nverts = 0;
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    ++nverts;
		    break;
		case BN_VLIST_POLY_END:
		    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON,begin,nverts));
		    first = 1;
		    break;
		case BN_VLIST_POLY_VERTNORM:
		    break;
	    }
	}
    }

    if (first == 0) {
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));
    }

    geom->setVertexArray(vertices);
    geom->setNormalArray(normals, osg::Array::BIND_OVERALL);
    //geom->setNormalBinding(osg::Geometry::BIND_PER_PRIMITIVE_SET);

    geom->setUseDisplayList(true);
    geode->addDrawable(geom);
    privvars->osg_root->addChild(geode);
#endif


    /* Need this back off for underlay with framebuffer */
    glDisable(GL_DEPTH_TEST);

    return TCL_OK;
}


HIDDEN int
osgl_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), void **data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    osgl_drawVList(dmp, vp);
	}
    } else {
	if (!data) {
	    return TCL_ERROR;
	} else {
	(void)callback_function(data);
    }
    }
    return TCL_OK;
}


/*
 * Restore the display processor to a normal mode of operation
 * (i.e., not scaled, rotated, displaced, etc.).
 */
HIDDEN int
osgl_normal(struct dm *dmp)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_normal\n");

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of osgl_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of osgl_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (!((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	((struct osgl_vars *)dmp->i->dm_vars.priv_vars)->face_flag = 1;
	if (mvars->cueing_on)
	    glDisable(GL_FOG);
	if (dmp->i->dm_light)
	    glDisable(GL_LIGHTING);
    }

    if (dmp->i->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of osgl_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of osgl_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osgl_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return TCL_OK;
}


/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
osgl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int UNUSED(use_aspect))
{
    fastf_t font_size = dm_get_fontsize(dmp);
    struct osgl_vars *privvars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    int blend_state = glIsEnabled(GL_BLEND);
    fastf_t coord_x, coord_y;
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_drawString2D()\n");
    if (!(int)font_size) {
	font_size = dm_get_height(dmp)/60.0;
    }
    if (privvars->fontNormal != FONS_INVALID) {
	coord_x = (x + 1)/2 * dm_get_width(dmp);
	coord_y = dm_get_height(dmp) - ((y + 1)/2 * dm_get_height(dmp));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0,dm_get_width(dmp),dm_get_height(dmp),0,-1,1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	fonsSetFont(privvars->fs, privvars->fontNormal);
	/* drop shadow */
	if (mvars->text_shadow) {
	    unsigned int black = glfonsRGBA(0, 0, 0, 255);
	    fonsSetColor(privvars->fs, black);
	    fonsDrawText(privvars->fs, coord_x, coord_y-2, str, NULL);
	    fonsDrawText(privvars->fs, coord_x, coord_y+2, str, NULL);
	    fonsDrawText(privvars->fs, coord_x-2, coord_y, str, NULL);
	    fonsDrawText(privvars->fs, coord_x+2, coord_y, str, NULL);
	}
	/* normal text */
	unsigned int color = glfonsRGBA(dmp->i->dm_fg[0], dmp->i->dm_fg[1], dmp->i->dm_fg[2], 255);
	fonsSetSize(privvars->fs, (int)font_size); /* cast to int so we always get a font */
	fonsSetColor(privvars->fs, color);
	fonsDrawText(privvars->fs, coord_x, coord_y, str, NULL);

	if (!blend_state) glDisable(GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
    }

    return TCL_OK;
}


HIDDEN int
osgl_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{
    return drawLine2D(dmp, X1, Y1, X2, Y2, "osgl_drawLine2D()\n");
}


HIDDEN int
osgl_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return drawLine3D(dmp, pt1, pt2, "osgl_drawLine3D()\n", wireColor);
}


HIDDEN int
osgl_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag)
{
    return drawLines3D(dmp, npoints, points, sflag, "osgl_drawLine3D()\n", wireColor);
}


HIDDEN int
osgl_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->i->dm_debugLevel) {
	bu_log("osgl_drawPoint2D():\n");
	bu_log("\tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
    }

    glEnable(GL_POINT_SMOOTH);
    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return TCL_OK;
}


HIDDEN int
osgl_drawPoint3D(struct dm *dmp, point_t point)
{
    GLdouble dpt[3];

    if (!dmp || !point)
	return TCL_ERROR;

    if (dmp->i->dm_debugLevel) {
	bu_log("osgl_drawPoint3D():\n");
	bu_log("\tdmp: %p\tpt - %lf %lf %lf\n", (void*)dmp, V3ARGS(point));
    }

    /* fastf_t to double */
    VMOVE(dpt, point);

    glEnable(GL_POINT_SMOOTH);
    glBegin(GL_POINTS);
    glVertex3dv(dpt);
    glEnd();

    return TCL_OK;
}


HIDDEN int
osgl_drawPoints3D(struct dm *dmp, int npoints, point_t *points)
{
    GLdouble dpt[3];
    register int i;

    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;

    if (dmp->i->dm_debugLevel) {
	bu_log("osgl_drawPoint3D():\n");
    }

    glEnable(GL_POINT_SMOOTH);
    glBegin(GL_POINTS);
    for (i = 0; i < npoints; ++i) {
	/* fastf_t to double */
	VMOVE(dpt, points[i]);
	glVertex3dv(dpt);
    }
    glEnd();

    return TCL_OK;
}


HIDDEN int
osgl_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    /*if (dmp->i->dm_debugLevel)
	bu_log("osgl_setFGColor()\n");*/

    dmp->i->dm_fg[0] = r;
    dmp->i->dm_fg[1] = g;
    dmp->i->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    wireColor[0] = r / 255.0;
    wireColor[1] = g / 255.0;
    wireColor[2] = b / 255.0;
    wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (dmp->i->dm_light) {
	    /* Ambient = .2, Diffuse = .6, Specular = .2 */

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

	    backDiffuseColorDark[0] = wireColor[0] * 0.3;
	    backDiffuseColorDark[1] = wireColor[1] * 0.3;
	    backDiffuseColorDark[2] = wireColor[2] * 0.3;
	    backDiffuseColorDark[3] = wireColor[3];

	    backDiffuseColorLight[0] = wireColor[0] * 0.9;
	    backDiffuseColorLight[1] = wireColor[1] * 0.9;
	    backDiffuseColorLight[2] = wireColor[2] * 0.9;
	    backDiffuseColorLight[3] = wireColor[3];

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
osgl_setLineAttr(struct dm *dmp, int width, int style)
{
    /*if (dmp->i->dm_debugLevel)
	bu_log("osgl_setLineAttr()\n");*/

    dmp->i->dm_lineWidth = width;
    dmp->i->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return TCL_OK;
}


HIDDEN int
osgl_debug(struct dm *dmp, int lvl)
{
    dmp->i->dm_debugLevel = lvl;

    return TCL_OK;
}

HIDDEN int
osgl_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->i->dm_log, "%s", filename);

    return TCL_OK;
}

HIDDEN int
osgl_setWinBounds(struct dm *dmp, fastf_t *w)
{
    GLint mm;

    if (dmp->i->dm_debugLevel)
	bu_log("osgl_setWinBounds()\n");

    dmp->i->dm_clipmin[0] = w[0];
    dmp->i->dm_clipmin[1] = w[2];
    dmp->i->dm_clipmin[2] = w[4];
    dmp->i->dm_clipmax[0] = w[1];
    dmp->i->dm_clipmax[1] = w[3];
    dmp->i->dm_clipmax[2] = w[5];

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    glPushMatrix();
    glMatrixMode(mm);

    return TCL_OK;
}


HIDDEN int
osgl_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_setTransparency()\n");

    dmp->i->dm_transparency = transparency_on;
    mvars->transparency_on = dmp->i->dm_transparency;

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
osgl_setDepthMask(struct dm *dmp,
		 int enable) {
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_setDepthMask()\n");

    dmp->i->dm_depthMask = enable;

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}


HIDDEN int
osgl_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_setZBuffer:\n");

    dmp->i->dm_zbuffer = zbuffer_on;
    mvars->zbuffer_on = dmp->i->dm_zbuffer;

    if (mvars->zbuf == 0) {
	dmp->i->dm_zbuffer = 0;
	mvars->zbuffer_on = dmp->i->dm_zbuffer;
    }

    if (mvars->zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}


HIDDEN int
osgl_beginDList(struct dm *dmp, unsigned int list)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_beginDList()\n");

    glNewList((GLuint)list, GL_COMPILE);
    return TCL_OK;
}


HIDDEN int
osgl_endDList(struct dm *dmp)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_endDList()\n");

    glEndList();
    return TCL_OK;
}


HIDDEN int
osgl_drawDList(unsigned int list)
{
    glCallList((GLuint)list);
    return TCL_OK;
}


HIDDEN int
osgl_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_freeDLists()\n");

    glDeleteLists((GLuint)list, (GLsizei)range);
    return TCL_OK;
}


HIDDEN int
osgl_genDLists(struct dm *dmp, size_t range)
{
    if (dmp->i->dm_debugLevel)
	bu_log("osgl_freeDLists()\n");

    return glGenLists((GLsizei)range);
}

HIDDEN int
osgl_draw_obj(struct dm *dmp, struct display_list *obj)
{
    struct bview_scene_obj *sp;
    FOR_ALL_SOLIDS(sp, &obj->dl_head_scene_obj) {
	if (sp->s_dlist == 0)
	    sp->s_dlist = dm_gen_dlists(dmp, 1);

	(void)dm_make_current(dmp);
	(void)dm_begin_dlist(dmp, sp->s_dlist);
	if (sp->s_iflag == UP)
	    (void)dm_set_fg(dmp, 255, 255, 255, 0, sp->s_transparency);
	else
	    (void)dm_set_fg(dmp,
		    (unsigned char)sp->s_color[0],
		    (unsigned char)sp->s_color[1],
		    (unsigned char)sp->s_color[2], 0, sp->s_transparency);
	(void)dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist);
	(void)dm_end_dlist(dmp);
    }
    return 0;
}

HIDDEN int
osgl_getDisplayImage(struct dm *dmp, unsigned char **image)
{
    unsigned char *idata = NULL;
    int width = 0;
    int height = 0;
    int bytes_per_pixel = 3; /*rgb no alpha for raw pix */
    GLuint *pixels;
    unsigned int pixel;
    unsigned int red_mask = 0xff000000;
    unsigned int green_mask = 0x00ff0000;
    unsigned int blue_mask = 0x0000ff00;
    int h, w;
#ifdef _WIN32
    unsigned int alpha_mask = 0x000000ff;
    int big_endian;
    int swap_bytes;

    if ((bu_byteorder() == BU_BIG_ENDIAN))
	big_endian = 1;
    else
	big_endian = 0;

    /* WTF */
    swap_bytes = !big_endian;
#endif

    width = dmp->i->dm_width;
    height = dmp->i->dm_height;

    pixels = (GLuint *)bu_calloc(width * height, sizeof(GLuint), "pixels");

    {
	glReadBuffer(GL_FRONT);
#ifdef _WIN32
	/* XXX GL_UNSIGNED_INT_8_8_8_8 is currently not
	 * available on windows.  Need to update when it
	 * becomes available.
	 */
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
#else
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixels);
#endif

	idata = (unsigned char *)bu_calloc(height * width * bytes_per_pixel, sizeof(unsigned char), "rgb data");
	*image = idata;
	flip_display_image_vertically(*image, width, height);

	for (h = 0; h < height; h++) {
	    for (w = 0; w < width; w++) {
		int i = h*width + w;
		int i_h_inv = (height - h - 1)*width + w;
		int j = i*bytes_per_pixel;
		unsigned char *value = (unsigned char *)(idata + j);
#ifdef _WIN32
		unsigned char alpha;
#endif

		pixel = pixels[i_h_inv];

		value[0] = (pixel & red_mask) >> 24;
		value[1] = (pixel & green_mask) >> 16;
		value[2] = (pixel & blue_mask) >> 8;

#ifdef _WIN32
		alpha = pixel & alpha_mask;
		if (swap_bytes) {
		    unsigned char tmp_byte;

		    value[0] = alpha;
		    /* swap byte1 and byte2 */
		    tmp_byte = value[1];
		    value[1] = value[2];
		    value[2] = tmp_byte;
		}
#endif
	    }

	}

	bu_free(pixels, "pixels");
    }


    return TCL_OK; /* caller will need to bu_free(idata, "image data"); */
}

int
osgl_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct osgl_fb_info *ofb_ps;
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
    /*struct dm_osglvars *pubvars = (struct dm_osglvars *)dmp->i->dm_vars.pub_vars;*/
    struct osgl_vars *privars = (struct osgl_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_OSGL_MAGIC);
    ofb_ps = (struct osgl_fb_info *)fb_ps->data;

    ofb_ps->glc = (void *)privars->graphicsContext;
    ofb_ps->traits = (void *)privars->traits;
    ofb_ps->double_buffer = mvars->doublebuffer;
    ofb_ps->soft_cmap = 0;
    dmp->i->fbp = fb_open_existing("osgl", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

int
osgl_get_internal(struct dm *dmp)
{
    struct modifiable_osgl_vars *mvars = NULL;
    if (!dmp->i->m_vars) {
	BU_GET(dmp->i->m_vars, struct modifiable_osgl_vars);
	mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
	mvars->this_dm = dmp;
	bu_vls_init(&(mvars->log));
    }
    return 0;
}

int
osgl_put_internal(struct dm *dmp)
{
    struct modifiable_osgl_vars *mvars = NULL;
    if (dmp->i->m_vars) {
	mvars = (struct modifiable_osgl_vars *)dmp->i->m_vars;
	bu_vls_free(&(mvars->log));
	BU_PUT(dmp->i->m_vars, struct modifiable_osgl_vars);
    }
    return 0;
}

void
Osgl_colorchange(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    if (mvars->cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    dmp->i->dm_zclip = mvars->zclipping_on;

    if (dmp->i->dm_zclip) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    (void)dm_make_current(dmp);
    (void)dm_set_win_bounds(dmp, bounds);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_debug_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_debug(dmp, mvars->debug);

    dm_generic_hook(sdp, name, base, value, data);
}


static void
osgl_logfile_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_logfile(dmp, bu_vls_addr(&mvars->log));

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_bound_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dmp->i->dm_bound = mvars->bound;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_bound_flag_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dmp->i->dm_boundFlag = mvars->boundFlag;

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_zbuffer_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_zbuffer(dmp, mvars->zbuffer_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_lighting_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_light(dmp, mvars->lighting_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_transparency_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    (void)dm_make_current(dmp);
    (void)dm_set_transparency(dmp, mvars->transparency_on);

    dm_generic_hook(sdp, name, base, value, data);
}

static void
osgl_fog_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct modifiable_osgl_vars *mvars = (struct modifiable_osgl_vars *)base;
    struct dm *dmp = mvars->this_dm;

    dm_fogHint(dmp, mvars->fastfog);

    dm_generic_hook(sdp, name, base, value, data);
}

struct bu_structparse Osgl_vparse[] = {
    {"%d",  1, "depthcue",              Osgl_MV_O(cueing_on),    Osgl_colorchange, NULL, NULL },
    {"%d",  1, "zclip",         	Osgl_MV_O(zclipping_on), osgl_zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",               Osgl_MV_O(zbuffer_on),   osgl_zbuffer_hook, NULL, NULL },
    {"%d",  1, "lighting",              Osgl_MV_O(lighting_on),  osgl_lighting_hook, NULL, NULL },
    {"%d",  1, "transparency",  	Osgl_MV_O(transparency_on), osgl_transparency_hook, NULL, NULL },
    {"%d",  1, "fastfog",               Osgl_MV_O(fastfog),      osgl_fog_hook, NULL, NULL },
    {"%g",  1, "density",               Osgl_MV_O(fogdensity),   dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",              Osgl_MV_O(zbuf),         dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_rgb",               Osgl_MV_O(rgb),          dm_generic_hook, NULL, NULL },
    {"%d",  1, "has_doublebuffer",      Osgl_MV_O(doublebuffer), dm_generic_hook, NULL, NULL },
    {"%d",  1, "depth",         	Osgl_MV_O(depth),        dm_generic_hook, NULL, NULL },
    {"%d",  1, "debug",         	Osgl_MV_O(debug),        osgl_debug_hook, NULL, NULL },
    {"%V",  1, "log",   		Osgl_MV_O(log),  	 osgl_logfile_hook, NULL, NULL },
    {"%g",  1, "bound",         	Osgl_MV_O(bound),        osgl_bound_hook, NULL, NULL },
    {"%d",  1, "useBound",              Osgl_MV_O(boundFlag),    osgl_bound_flag_hook, NULL, NULL },
    {"%d",  1, "text_shadow",           Osgl_MV_O(text_shadow),    dm_generic_hook, NULL, NULL },
    {"",        0,  (char *)0,          0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
osgl_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp) return -1;
    Tk_GeometryRequest(((struct dm_osglvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define OSGLVARS_MV_O(_m) offsetof(struct dm_osglvars, _m)

struct bu_structparse dm_osglvars_vparse[] = {
    {"%x",      1,      "dpy",                  OSGLVARS_MV_O(dpy),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "win",                  OSGLVARS_MV_O(win),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "top",                  OSGLVARS_MV_O(top),        BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "tkwin",                OSGLVARS_MV_O(xtkwin),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "depth",                OSGLVARS_MV_O(depth),      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%x",      1,      "cmap",                 OSGLVARS_MV_O(cmap),       BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devmotionnotify",      OSGLVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       OSGLVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     OSGLVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
osgl_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal OSGL variables", dm_osglvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_osglvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}

int
osgl_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_osglvars *pubvars = (struct dm_osglvars *)dmp->i->dm_vars.pub_vars;
    switch (type) {
	case DM_MOTION_NOTIFY:
	    return (event == pubvars->devmotionnotify) ? 1 : 0;
	    break;
	case DM_BUTTON_PRESS:
	    return (event == pubvars->devbuttonpress) ? 1 : 0;
	    break;
	case DM_BUTTON_RELEASE:
	    return (event == pubvars->devbuttonrelease) ? 1 : 0;
	    break;
	default:
	    return -1;
	    break;
    };
}

struct dm_impl dm_osgl_impl = {
    osgl_open,
    osgl_close,
    osgl_viable,
    osgl_drawBegin,
    osgl_drawEnd,
    osgl_normal,
    osgl_loadMatrix,
    osgl_loadPMatrix,
    osgl_drawString2D,
    osgl_drawLine2D,
    osgl_drawLine3D,
    osgl_drawLines3D,
    osgl_drawPoint2D,
    osgl_drawPoint3D,
    osgl_drawPoints3D,
    osgl_drawVList,
    osgl_drawVListHiddenLine,
    osgl_draw_data_axes,
    osgl_draw,
    osgl_setFGColor,
    osgl_setBGColor,
    osgl_setLineAttr,
    osgl_configureWin,
    osgl_setWinBounds,
    osgl_setLight,
    osgl_setTransparency,
    osgl_setDepthMask,
    osgl_setZBuffer,
    osgl_debug,
    osgl_logfile,
    osgl_beginDList,
    osgl_endDList,
    osgl_drawDList,
    osgl_freeDLists,
    osgl_genDLists,
    osgl_draw_obj,
    osgl_getDisplayImage, /* display to image function */
    osgl_reshape,
    osgl_makeCurrent,
    NULL,
    osgl_doevent,
    osgl_openFb,
    osgl_get_internal,
    osgl_put_internal,
    osgl_geometry_request,
    osgl_internal_var,
    NULL,
    NULL,
    NULL,
    osgl_event_cmp,
    NULL,
    NULL,
    0,
    1,				/* is graphical */
    "Tk",                       /* uses Tk graphics system */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit */
    1,				/* bound flag */
    "osgl",
    "OpenGL graphics via OpenSceneGraph",
    1, /* top */
    0, /* width */
    0, /* height */
    0, /* dirty */
    0, /* bytes per pixel */
    0, /* bits per channel */
    0,
    0,
    1.0, /* aspect ratio */
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
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
    0,                          /* not overriding the auto font size */
    Osgl_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};


extern "C" {
    struct dm dm_osgl = { DM_MAGIC, &dm_osgl_impl };

#ifdef DM_PLUGIN
    static const struct dm_plugin pinfo = { DM_API, &dm_osgl };

    COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info()
    {
	return &pinfo;
    }
#endif
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
