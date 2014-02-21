/*                       D M - O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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
/** @file libdm/dm-osg.cpp
 *
 * A libdm interface to OpenSceneGraph
 *
 */
#include "common.h"
#include <locale.h>

#ifdef DM_OSG

#include "bio.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "tk.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "dm.h"
#include "dm/dm-osg.h"
#include "dm/dm_xvars.h"

#include "./dm_util.h"

/* Display Manager package interface */
#define IRBOUND 4095.9	/* Max magnification in Rot matrix */
#define PLOTBOUND 1000.0	/* Max magnification in Rot matrix */

extern "C" {
    struct dm *osg_open(Tcl_Interp *interp, int argc, char **argv);
}
HIDDEN int osg_close(struct dm *dmp);
HIDDEN int osg_drawBegin(struct dm *dmp);
HIDDEN int osg_drawEnd(struct dm *dmp);
HIDDEN int osg_normal(struct dm *dmp);
HIDDEN int osg_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye);
HIDDEN int osg_loadPMatrix(struct dm *dmp, fastf_t *mat);
HIDDEN int osg_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
HIDDEN int osg_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2);
HIDDEN int osg_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2);
HIDDEN int osg_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag);
HIDDEN int osg_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y);
HIDDEN int osg_drawPoint3D(struct dm *dmp, point_t point);
HIDDEN int osg_drawPoints3D(struct dm *dmp, int npoints, point_t *points);
HIDDEN int osg_drawVList(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int osg_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *vp);
HIDDEN int osg_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data);
HIDDEN int osg_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency);
HIDDEN int osg_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b);
HIDDEN int osg_setLineAttr(struct dm *dmp, int width, int style);
HIDDEN int osg_configureWin_guts(struct dm *dmp, int force);
HIDDEN int osg_configureWin(struct dm *dmp, int force);
HIDDEN int osg_setLight(struct dm *dmp, int lighting_on);
HIDDEN int osg_setTransparency(struct dm *dmp, int transparency_on);
HIDDEN int osg_setDepthMask(struct dm *dmp, int depthMask_on);
HIDDEN int osg_setZBuffer(struct dm *dmp, int zbuffer_on);
HIDDEN int osg_setWinBounds(struct dm *dmp, fastf_t *w);
HIDDEN int osg_debug(struct dm *dmp, int lvl);
HIDDEN int osg_beginDList(struct dm *dmp, unsigned int list);
HIDDEN int osg_endDList(struct dm *dmp);
HIDDEN void osg_drawDList(unsigned int list);
HIDDEN int osg_freeDLists(struct dm *dmp, unsigned int list, int range);
HIDDEN int osg_genDLists(struct dm *dmp, size_t range);
HIDDEN int osg_getDisplayImage(struct dm *dmp, unsigned char **image);
HIDDEN void osg_reshape(struct dm *dmp, int width, int height);
HIDDEN int osg_makeCurrent(struct dm *dmp);


struct dm dm_osg = {
    osg_close,
    osg_drawBegin,
    osg_drawEnd,
    osg_normal,
    osg_loadMatrix,
    osg_loadPMatrix,
    osg_drawString2D,
    osg_drawLine2D,
    osg_drawLine3D,
    osg_drawLines3D,
    osg_drawPoint2D,
    osg_drawPoint3D,
    osg_drawPoints3D,
    osg_drawVList,
    osg_drawVListHiddenLine,
    osg_draw,
    osg_setFGColor,
    osg_setBGColor,
    osg_setLineAttr,
    osg_configureWin,
    osg_setWinBounds,
    osg_setLight,
    osg_setTransparency,
    osg_setDepthMask,
    osg_setZBuffer,
    osg_debug,
    osg_beginDList,
    osg_endDList,
    osg_drawDList,
    osg_freeDLists,
    osg_genDLists,
    osg_getDisplayImage, /* display to image function */
    osg_reshape,
    osg_makeCurrent,
    0,
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit */
    1,				/* bound flag */
    "osg",
    "OpenSceneGraph graphics",
    DM_TYPE_OSG,
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
    BU_VLS_INIT_ZERO,		/* bu_vls path name*/
    BU_VLS_INIT_ZERO,		/* bu_vls full name drawing window */
    BU_VLS_INIT_ZERO,		/* bu_vls short name drawing window */
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
    0				/* Tcl interpreter */
};


static fastf_t default_viewscale = 1000.0;
static double xlim_view = 1.0;	/* args for glOrtho*/
static double ylim_view = 1.0;

static float wireColor[4];

void
osg_fogHint(struct dm *dmp, int fastfog)
{
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


HIDDEN int
osg_setBGColor(struct dm *dmp, unsigned char UNUSED(r), unsigned char UNUSED(g), unsigned char UNUSED(b))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setBGColor()\n");

    return TCL_OK;
}


HIDDEN void
osg_reshape(struct dm *dmp, int width, int height)
{
    assert(dmp);
    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	bu_log("osg_reshape()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
    }

    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;


    osg::Matrixf orthom;
    orthom.makeIdentity();
    orthom.makeOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    osp->mainviewer->getCamera()->setProjectionMatrix(osg::Matrix::ortho2D(0,dmp->dm_width,0,dmp->dm_height));
    osp->mainviewer->getCamera()->setViewMatrix(orthom);
    osp->mainviewer->getCamera()->setViewport(0, 0, dmp->dm_width, dmp->dm_height);

    osp->mainviewer->frame();

}


HIDDEN int
osg_makeCurrent(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_makeCurrent()\n");

    return TCL_OK;
}


HIDDEN int
osg_configureWin(struct dm *UNUSED(dmp), int UNUSED(force))
{
    return TCL_OK;
}


HIDDEN int
osg_setLight(struct dm *dmp, int UNUSED(lighting_on))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setLight()\n");

    return TCL_OK;
}

/*
 * Gracefully release the display.
 */
HIDDEN int
osg_close(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_close()\n");

    return TCL_OK;
}


/*
 * Fire up the display manager, and the display processor.
 *
 */
extern "C"
struct dm *
osg_open(Tcl_Interp *interp, int argc, char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    int make_square = -1;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = (struct dm *)NULL;
    Tk_Window tkwin = (Tk_Window)NULL;

    struct dm_xvars *pubvars = NULL;
    struct osg_vars *privvars = NULL;

    if ((tkwin = Tk_MainWindow(interp)) == NULL) {
	return DM_NULL;
    }

    BU_ALLOC(dmp, struct dm);

    *dmp = dm_osg; /* struct copy */
    dmp->dm_interp = interp;
    dmp->dm_lineWidth = 1;
    dmp->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->dm_bits_per_channel = 8;

    BU_ALLOC(dmp->dm_vars.pub_vars, struct dm_xvars);
    if (dmp->dm_vars.pub_vars == (genptr_t)NULL) {
	bu_free(dmp, "osg_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;

    BU_ALLOC(dmp->dm_vars.priv_vars, struct osg_vars);
    if (dmp->dm_vars.priv_vars == (genptr_t)NULL) {
	bu_free(dmp->dm_vars.pub_vars, "osg_open: dmp->dm_vars.pub_vars");
	bu_free(dmp, "osg_open: dmp");
	return DM_NULL;
    }
    privvars = (struct osg_vars *)dmp->dm_vars.priv_vars;

    dmp->dm_vp = &default_viewscale;

    bu_vls_init(&dmp->dm_pathName);
    bu_vls_init(&dmp->dm_tkName);
    bu_vls_init(&dmp->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->dm_pathName) == 0)
	bu_vls_printf(&dmp->dm_pathName, ".dm_osg%d", count);
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

    /* initialize dm specific variables */
    pubvars->devmotionnotify = LASTEvent;
    pubvars->devbuttonpress = LASTEvent;
    pubvars->devbuttonrelease = LASTEvent;
    dmp->dm_aspect = 1.0;

    /* initialize modifiable variables */
    privvars->mvars.rgb = 1;
    privvars->mvars.doublebuffer = 1;
    privvars->mvars.fastfog = 1;
    privvars->mvars.fogdensity = 1.0;
    privvars->mvars.lighting_on = dmp->dm_light;
    privvars->mvars.zbuffer_on = dmp->dm_zbuffer;
    privvars->mvars.zclipping_on = dmp->dm_zclip;
    privvars->mvars.debug = dmp->dm_debugLevel;
    privvars->mvars.bound = dmp->dm_bound;
    privvars->mvars.boundFlag = dmp->dm_boundFlag;

    /* this is important so that osg_configureWin knows to set the font */
    pubvars->fontstruct = NULL;

    if (dmp->dm_top) {
	/* Make xtkwin a toplevel window */
	pubvars->xtkwin =
	    Tk_CreateWindowFromPath(interp,
				    tkwin,
				    bu_vls_addr(&dmp->dm_pathName),
				    bu_vls_addr(&dmp->dm_dName));
	pubvars->top = pubvars->xtkwin;
    } else {
	char *cp;

	cp = strrchr(bu_vls_addr(&dmp->dm_pathName), (int)'.');
	if (cp == bu_vls_addr(&dmp->dm_pathName)) {
	    pubvars->top = tkwin;
	} else {
	    struct bu_vls top_vls = BU_VLS_INIT_ZERO;

	    bu_vls_strncpy(&top_vls, (const char *)bu_vls_addr(&dmp->dm_pathName), cp - bu_vls_addr(&dmp->dm_pathName));

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
	bu_log("dm-Ogl: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)osg_close(dmp);
	return DM_NULL;
    }


    if (dmp->dm_width == 0) {
	dmp->dm_width = WidthOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }
    if (dmp->dm_height == 0) {
	dmp->dm_height = HeightOfScreen(Tk_Screen(pubvars->xtkwin)) - 30;
	++make_square;
    }

    if (make_square > 0) {
	/* Make window square */
	if (dmp->dm_height < dmp->dm_width)
	    dmp->dm_width = dmp->dm_height;
	else
	    dmp->dm_height = dmp->dm_width;
    }



    bu_vls_printf(&dmp->dm_tkName, "%s", (char *)Tk_Name(pubvars->xtkwin));

    bu_vls_printf(&str, "_init_dm %s %s\n",
		  bu_vls_addr(&init_proc_vls),
		  bu_vls_addr(&dmp->dm_pathName));

    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)osg_close(dmp);
	return DM_NULL;
    }

    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);

    pubvars->dpy = Tk_Display(pubvars->top);

    /* make sure there really is a display before proceeding. */
    if (!(pubvars->dpy)) {
	bu_vls_free(&init_proc_vls);
	bu_vls_free(&str);
	(void)osg_close(dmp);
	return DM_NULL;
    }

    Tk_GeometryRequest(pubvars->xtkwin,
		       dmp->dm_width,
		       dmp->dm_height);

    pubvars->depth = privvars->mvars.depth;

    Tk_MakeWindowExist(pubvars->xtkwin);

    pubvars->win = Tk_WindowId(pubvars->xtkwin);
    dmp->dm_id = pubvars->win;

    Tk_MapWindow(pubvars->xtkwin);

    backgnd[0] = backgnd[1] = backgnd[2] = backgnd[3] = 0.0;

    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;
    Window win;

    assert(dmp);

    //create our graphics context directly so we can pass our own window
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;

    // Init the Windata Variable that holds the handle for the Window to display OSG in.
    // Check the QOSGWidget.cpp example for more logic relevant to this.  Need to find
    // something showing how to handle Cocoa for the Mac, if that's possible
#if defined(DM_OSG)  /* Will eventually change to DM_X11 */
    win = ((struct dm_xvars *)(dmp->dm_vars.pub_vars))->win;
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowX11::WindowData(win);
#elif defined(DM_WIN32)
    /* win = ? OSG needs HWND for win... */
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowWin32::WindowData(win);
#endif

    // Setup the traits parameters
    traits->x = 0;
    traits->y = 0;
    traits->width = dmp->dm_width;
    traits->height = dmp->dm_height;
    traits->depth = 24;
    traits->windowDecoration = false;
    traits->doubleBuffer = true;
    traits->sharedContext = 0;
    traits->setInheritedWindowPixelFormat = true;

    traits->inheritedWindowData = windata;

    // Create the Composite viewer
    osp->viewer = new osgViewer::CompositeViewer();

    // Create the 3D view
    osp->mainviewer = new osgViewer::Viewer();
    osp->viewer->addView(osp->mainviewer);

    // Create the Graphics Context
    osg::ref_ptr<osg::GraphicsContext> graphicsContext = osg::GraphicsContext::createGraphicsContext(traits.get());

    if (graphicsContext) {
	osp->mainviewer->getCamera()->setGraphicsContext(graphicsContext);
	osp->mainviewer->getCamera()->setViewport(new osg::Viewport(0, 0, dmp->dm_width, dmp->dm_height));
	//osp->mainviewer->getCamera()->setClearColor(osg::Vec4(1.0f, 1.0f, 1.0f, 0.0f));
    }

    osp->mainviewer->setCameraManipulator( new osgGA::TrackballManipulator() );
    osp->mainviewer->getCamera()->setAllowEventFocus(false);
    osp->mainviewer->getCamera()->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    osp->mainviewer->getCamera()->setProjectionMatrix(osg::Matrix::ortho2D(0,traits->width,0,traits->height));
    osp->mainviewer->getCamera()->setViewMatrix(osg::Matrix::identity());
    //osp->mainviewer->getCamera()->getProjectionMatrixAsFrustum(osp->left, osp->right, osp->bottom, osp->top, osp->near, osp->far);
    //osp->mainviewer->addEventHandler(new osgViewer::StatsHandler);
    osp->prev_pflag = dmp->dm_perspective;

    osp->osg_root = new osg::Group();
    osp->mainviewer->setSceneData(osp->osg_root);

    /* Until we have the ability to populate using .g information, do this for testing */
    //osg::ref_ptr<osg::Node> scene = osgDB::readNodeFile(bu_brlcad_data("test.osg", 0));
    //osp->mainviewer->setSceneData(scene.get());

    /* Provide a timer */
    osp->timer = new osg::Timer();
    osp->last_local_draw_time = 0.0;
    osp->cumulative_draw_time = 0.0;

    osp->initial_draw = 1;
    osp->nverts = 0;

    /* Make sure the initial frame is rendered */
    osp->mainviewer->frame();

    /* These next 2 lines aren't needed or useful here - the idea is to test whether we can "halt"
     * a multithreaded render using a scene graph with STATIC data per setDataVariance.
     * Most of the time data in BRL-CAD graphics will be static, with the exception
     * being an edit operation.  What I'm wondering is if it is possible to create an all STATIC
     * tree, and when an edit operation is requested halt all rendering, remove the static
     * version of the object and replace it with a dynamic one, and start over with the updated
     * tree.  It would be a shame if we had to declare all objects in a model DYNAMIC when most
     * of them won't be most of the time. */
    osp->mainviewer->stopThreading();
    /* Step 2: update nodes based on what we'll be editing */
    osp->mainviewer->startThreading();

    bu_log("max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());

    return dmp;
}


int
osg_share_dlist(struct dm *UNUSED(dmp1), struct dm *UNUSED(dmp2))
{
    return TCL_OK;
}


/*
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
osg_drawBegin(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawBegin()\n");

    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;

    if (!osp->init) {
	osp->timer->setStartTick();
    }

    return TCL_OK;
}


HIDDEN int
osg_drawEnd(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawEnd()\n");

    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;

    if (osp->initial_draw) {
	osg::BoundingSphere sph = osp->mainviewer->getScene()->getSceneData()->getBound();
	double radius = sph.radius();
	double wfactor = 1.0;
	double hfactor = 1.0;
	if (dmp->dm_width > dmp->dm_height) {
	    hfactor = dmp->dm_height/dmp->dm_width;
	}
	if (dmp->dm_height > dmp->dm_width) {
	    wfactor = dmp->dm_width/dmp->dm_height;
	}
	osp->mainviewer->getCamera()->setProjectionMatrix(osg::Matrix::ortho2D(wfactor * -radius,wfactor * radius, hfactor * -radius, hfactor * radius));
	osgGA::TrackballManipulator *tbmp = (dynamic_cast<osgGA::TrackballManipulator *>(osp->mainviewer->getCameraManipulator()));
	tbmp->setCenter(sph.center());
	bu_log("initial center: %f, %f, %f\n", sph.center().x(), sph.center().y(), sph.center().z());
	tbmp->setDistance(sph.radius());
	if (osp->nverts > 0)
	    osp->initial_draw = 0;
	bu_log("initial!\n");
    }

    osp->mainviewer->frame();

    return TCL_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to osg_draw().
 */
HIDDEN int
osg_loadMatrix(struct dm *dmp, fastf_t *mat, int UNUSED(which_eye))
{
    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;

    assert(dmp);
    osgGA::TrackballManipulator *tbmp = (dynamic_cast<osgGA::TrackballManipulator *>(osp->mainviewer->getCameraManipulator()));
    quat_t quat;
    osg::Quat rot;
    osg::Vec3d center;
    mat_t brl_rot;
    mat_t brl_invrot;
    mat_t brl_center;

    // Set the view rotation
    quat_mat2quat(quat, mat);
    rot.set(quat[X], quat[Y], quat[Z], quat[W]);

    // Find the view center
    quat_quat2mat(brl_rot, quat);
    bn_mat_inv(brl_invrot, brl_rot);
    bn_mat_mul(brl_center, brl_invrot, mat);
    center.set(-brl_center[MDX], -brl_center[MDY], -brl_center[MDZ]);


    // The order is important - first the rotation, then the center.
    tbmp->setRotation(rot);
    tbmp->setCenter(center);

    return TCL_OK;
}


/*
 * Load a new projection matrix.
 *
 */
HIDDEN int
osg_loadPMatrix(struct dm *dmp, fastf_t *mat)
{
    bu_log("osg_loadPMatrix()\n");


    fastf_t *mptr;
    GLfloat gtmat[16];

    glMatrixMode(GL_PROJECTION);

    if (mat == (fastf_t *)NULL) {
	if (((struct osg_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	    glPopMatrix();
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
	    glPushMatrix();
	    glLoadMatrixd(((struct osg_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	} else {
	    glLoadIdentity();
	    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
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

    return TCL_OK;
}


HIDDEN int
osg_drawVListHiddenLine(struct dm *dmp, register struct bn_vlist *UNUSED(vp))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawVListHiddenLine()\n");

    return TCL_OK;
}


HIDDEN int
osg_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;
    register struct bn_vlist *tvp;
    register int first;
    int begin;
    int nverts;

    /* Horrible hack to confirm that continually re-creating
     * osg objects every time havoc is rotated is causing
     * slow frame rates.  Looks like we need some solution
     * to recognize when vp is already in osg so we
     * aren't totally rebuilding the scene graph every time
     * we update a frame. Even with this, fps is less than
     * half that of a raw ogl display list view */
//    /*
    if (osp->init >= 8121) return TCL_OK;
    osp->init++;
//    */

    if (dmp->dm_debugLevel)
	bu_log("osg_drawVList()\n");


    // create the osg containers to hold our data.
    osg::ref_ptr<osg::Geode> geode = new osg::Geode(); // Maybe create this at drawBegin?
    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray;
    osg::ref_ptr<osg::Vec3dArray> normals = new osg::Vec3dArray;

    // Set line color
    osg::Vec4Array* line_color = new osg::Vec4Array;
    line_color->push_back(osg::Vec4(osp->wireColor[0], osp->wireColor[1], osp->wireColor[2], osp->wireColor[3]));
    geom->setColorArray(line_color, osg::Array::BIND_OVERALL);

    // Set wireframe state
    osg::StateSet *geom_state = geom->getOrCreateStateSet();
    osg::ref_ptr<osg::PolygonMode> geom_polymode = new osg::PolygonMode;
    geom_polymode->setMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE);
    geom_state->setAttributeAndModes(geom_polymode);
    geom_state->setMode(GL_LIGHTING,osg::StateAttribute::OVERRIDE|osg::StateAttribute::OFF);

    /* Viewing region is from -1.0 to +1.0 */
    begin = 0;
    nverts = 0;
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
		    osp->nverts++;
		    nverts = 1;
		    break;
		case BN_VLIST_POLY_START:
		    normals->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    begin += nverts;
		    osp->nverts++;
		    nverts = 0;
		    break;
		case BN_VLIST_LINE_DRAW:
		case BN_VLIST_POLY_MOVE:
		case BN_VLIST_POLY_DRAW:
		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    ++nverts;
		    osp->nverts++;
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

    osp->osg_root->addChild(geode);

    /* Nifty trick - incremental updating of the view.  It does add
     * time to the overall drawing process, so it should be a dm
     * option. */
#if 0
    if (osp->timer->time_m() > (50 + 2 * osp->last_local_draw_time)) {
	osg::Timer localtimer;
	localtimer.setStartTick();
	osp->mainviewer->frame();
	osp->last_local_draw_time = localtimer.time_m();
	osp->cumulative_draw_time += osp->last_local_draw_time;
	//bu_log("draw time: %f\n", osp->last_local_draw_time / 1000);
	//bu_log("cumulative draw time: %f\n", osp->cumulative_draw_time/ 1000);
	osp->timer->setStartTick();
    }
#endif

    return TCL_OK;
}


HIDDEN int
osg_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_draw()\n");

    if (!callback_function) {
	if (data) {
	    osg_drawVList(dmp, (struct bn_vlist *)data);
	}
    } else {
	(void)callback_function(data);
    }
    return TCL_OK;
}


/*
 * Restore the display processor to a normal mode of operation
 * (i.e., not scaled, rotated, displaced, etc.).
 */
HIDDEN int
osg_normal(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_normal\n");
    return TCL_OK;
}


/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
HIDDEN int
osg_drawString2D(struct dm *dmp, const char *UNUSED(str), fastf_t UNUSED(x), fastf_t UNUSED(y), int UNUSED(size), int UNUSED(use_aspect))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawString2D()\n");

    return TCL_OK;
}


HIDDEN int
osg_drawLine2D(struct dm *dmp, fastf_t X1, fastf_t Y1, fastf_t X2, fastf_t Y2)
{
    return drawLine2D(dmp, X1, Y1, X2, Y2, "osg_drawLine2D()\n");
}


HIDDEN int
osg_drawLine3D(struct dm *dmp, point_t pt1, point_t pt2)
{
    return drawLine3D(dmp, pt1, pt2, "osg_drawLine3D()\n", wireColor);
}


HIDDEN int
osg_drawLines3D(struct dm *dmp, int npoints, point_t *points, int sflag)
{
    return drawLines3D(dmp, npoints, points, sflag, "osg_drawLine3D()\n", wireColor);
}


HIDDEN int
osg_drawPoint2D(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (dmp->dm_debugLevel) {
	bu_log("osg_drawPoint2D():\n");
	bu_log("\tdmp: %p\tx - %lf\ty - %lf\n", (void *)dmp, x, y);
    }

    glBegin(GL_POINTS);
    glVertex2f(x, y);
    glEnd();

    return TCL_OK;
}


HIDDEN int
osg_drawPoint3D(struct dm *dmp, point_t point)
{
    GLdouble dpt[3];

    if (!dmp || !point)
	return TCL_ERROR;

    if (dmp->dm_debugLevel) {
	bu_log("osg_drawPoint3D():\n");
	bu_log("\tdmp: %p\tpt - %lf %lf %lf\n", (void*)dmp, V3ARGS(point));
    }

    /* fastf_t to double */
    VMOVE(dpt, point);

    glBegin(GL_POINTS);
    glVertex3dv(dpt);
    glEnd();

    return TCL_OK;
}


HIDDEN int
osg_drawPoints3D(struct dm *dmp, int npoints, point_t *points)
{
    GLdouble dpt[3];
    register int i;

    if (!dmp || npoints < 0 || !points)
	return TCL_ERROR;

    if (dmp->dm_debugLevel) {
	bu_log("osg_drawPoint3D():\n");
    }


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
osg_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int UNUSED(strict), fastf_t transparency)
{
    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;

    if (dmp->dm_debugLevel)
	bu_log("osg_setFGColor()\n");

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    osp->wireColor[0] = r / 255.0;
    osp->wireColor[1] = g / 255.0;
    osp->wireColor[2] = b / 255.0;
    osp->wireColor[3] = transparency;

    return TCL_OK;
}


HIDDEN int
osg_setLineAttr(struct dm *dmp, int width, int style)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setLineAttr()\n");

    dmp->dm_lineWidth = width;
    dmp->dm_lineStyle = style;

    glLineWidth((GLfloat) width);

    if (style == DM_DASHED_LINE)
	glEnable(GL_LINE_STIPPLE);
    else
	glDisable(GL_LINE_STIPPLE);

    return TCL_OK;
}


HIDDEN int
osg_debug(struct dm *dmp, int lvl)
{
    dmp->dm_debugLevel = lvl;

    return TCL_OK;
}


HIDDEN int
osg_setWinBounds(struct dm *dmp, fastf_t *w)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setWinBounds()\n");

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    struct osg_vars *osp = (struct osg_vars *)dmp->dm_vars.priv_vars;
    osg::Matrixf orthom;
    orthom.makeIdentity();
    orthom.makeOrtho(-1.0, 1.0, -1.0, 1.0, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    osp->mainviewer->getCamera()->setProjectionMatrix(orthom);
    osp->mainviewer->getCamera()->setViewport(0, 0, dmp->dm_width, dmp->dm_height);

    return TCL_OK;
}


HIDDEN int
osg_setTransparency(struct dm *dmp,
		    int UNUSED(transparency_on))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setTransparency()\n");

    return TCL_OK;
}


HIDDEN int
osg_setDepthMask(struct dm *dmp,
		 int UNUSED(enable)) {
    if (dmp->dm_debugLevel)
	bu_log("osg_setDepthMask()\n");

    return TCL_OK;
}


HIDDEN int
osg_setZBuffer(struct dm *dmp, int UNUSED(zbuffer_on))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setZBuffer:\n");

    return TCL_OK;
}


HIDDEN int
osg_beginDList(struct dm *dmp, unsigned int UNUSED(list))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_beginDList()\n");

    return TCL_OK;
}


HIDDEN int
osg_endDList(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_endDList()\n");

    return TCL_OK;
}


HIDDEN void
osg_drawDList(unsigned int UNUSED(list))
{
}


HIDDEN int
osg_freeDLists(struct dm *dmp, unsigned int UNUSED(list), int UNUSED(range))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_freeDLists()\n");

    return TCL_OK;
}


HIDDEN int
osg_genDLists(struct dm *dmp, size_t range)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_genDLists()\n");

    return glGenLists((GLsizei)range);
}


HIDDEN int
osg_getDisplayImage(struct dm *dmp, unsigned char **UNUSED(image))
{
    if (dmp->dm_debugLevel)
	bu_log("osg_getDisplayImage()\n");

    return TCL_OK; /* caller will need to bu_free(idata, "image data"); */
}


#endif /* DM_OSG */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
