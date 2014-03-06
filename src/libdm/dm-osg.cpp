/*                       D M - O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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

#ifdef DM_OGL

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

/* For Tk, we need to offset when thinking about screen size in
 * order to allow for the Mac OSX top-of-screen toolbar - Tk
 * itself is quite happy to put things under it */
#define TK_SCREEN_OFFSET 30

#define VIEWFACTOR      (1.0/(*dmp->dm_vp))
#define VIEWSIZE        (2.0*(*dmp->dm_vp))

/* these are from /usr/include/gl.h could be device dependent */
#define XMAXSCREEN	1279
#define YMAXSCREEN	1023
#define YSTEREO		491	/* subfield height, in scanlines */
#define YOFFSET_LEFT	532	/* YSTEREO + YBLANK ? */

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
HIDDEN int osg_logfile(struct dm *dmp, const char *filename);
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
    osg_logfile,
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
    BU_VLS_INIT_ZERO,		/* bu_vls logfile */
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
static float light0_diffuse[] = {1.0, 1.0, 1.0, 1.0}; /* white */
static float wireColor[4];
static float ambientColor[4];
static float specularColor[4];
static float diffuseColor[4];
static float backDiffuseColorDark[4];
static float backDiffuseColorLight[4];

HIDDEN void
osg_printmat(struct bu_vls *tmp_vls, fastf_t *mat) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[0], mat[4], mat[8], mat[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[1], mat[5], mat[9], mat[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[2], mat[6], mat[10], mat[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", mat[3], mat[7], mat[11], mat[15]);
}

HIDDEN void
osg_printglmat(struct bu_vls *tmp_vls, GLfloat *m) {
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[0], m[4], m[8], m[12]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[1], m[5], m[9], m[13]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[2], m[6], m[10], m[14]);
    bu_vls_printf(tmp_vls, "%g %g %g %g\n", m[3], m[7], m[11], m[15]);
}


void
osg_fogHint(struct dm *dmp, int fastfog)
{
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog = fastfog;
    glHint(GL_FOG_HINT, fastfog ? GL_FASTEST : GL_NICEST);
}


HIDDEN int
osg_setBGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (dmp->dm_debugLevel == 1)
	bu_log("osg_setBGColor()\n");

    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;

    ((struct osg_vars *)dmp->dm_vars.priv_vars)->r = r / 255.0;
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->g = g / 255.0;
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->b = b / 255.0;

    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->swapBuffers();
    glClearColor(((struct osg_vars *)dmp->dm_vars.priv_vars)->r,
	    ((struct osg_vars *)dmp->dm_vars.priv_vars)->g,
	    ((struct osg_vars *)dmp->dm_vars.priv_vars)->b,
	    0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return TCL_OK;
}


/*
 * Either initially, or on resize/reshape of the window,
 * sense the actual size of the window, and perform any
 * other initializations of the window configuration.
 *
 * also change font size if necessary
 */
HIDDEN int
osg_configureWin_guts(struct dm *dmp, int force)
{
#if 0
    XWindowAttributes xwa;
    XFontStruct *newfontstruct;

    if (dmp->dm_debugLevel)
	bu_log("osg_configureWin_guts()\n");

    XGetWindowAttributes(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			 ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win, &xwa);
    /* nothing to do */
    if (!force &&
	dmp->dm_height == xwa.height &&
	dmp->dm_width == xwa.width)
	return TCL_OK;
#endif

    int width;
    int height;

    struct dm_xvars *pubvars = (struct dm_xvars *)dmp->dm_vars.pub_vars;

    if (pubvars->top != pubvars->xtkwin) {
	width = Tk_Width(Tk_Parent(pubvars->xtkwin));
	height = Tk_Height(Tk_Parent(pubvars->xtkwin));
    } else {
	width = Tk_Width(pubvars->top);
	height = Tk_Height(pubvars->top);
    }

    if (!force &&
	    dmp->dm_height == height &&
	    dmp->dm_width == width)
	return TCL_OK;

    osg_reshape(dmp, width, height);
#if 0
    /* First time through, load a font or quit */
    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct == NULL) {
	if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
	     XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    FONT9)) == NULL) {
	    /* Try hardcoded backup font */
	    if ((((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct =
		 XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
				FONTBACK)) == NULL) {
		bu_log("osg_configureWin_guts: Can't open font '%s' or '%s'\n", FONT9, FONTBACK);
		return TCL_ERROR;
	    }
	}
	glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
		    0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    }

    if (DM_VALID_FONT_SIZE(dmp->dm_fontsize)) {
	if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != dmp->dm_fontsize) {
	    if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						DM_FONT_SIZE_TO_NAME(dmp->dm_fontsize))) != NULL) {
		XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			  ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
			    0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
	    }
	}
    } else {
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
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
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
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
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
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
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
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 1455) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 9) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT9)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else if (dmp->dm_width < 2037) {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 10) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT10)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	} else {
	    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->per_char->width != 12) {
		if ((newfontstruct = XLoadQueryFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
						    FONT12)) != NULL) {
		    XFreeFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct);
		    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct = newfontstruct;
		    glXUseXFont(((struct dm_xvars *)dmp->dm_vars.pub_vars)->fontstruct->fid,
				0, 127, ((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
		}
	    }
	}
    }
#endif
    return TCL_OK;
}


HIDDEN void
osg_reshape(struct dm *dmp, int width, int height)
{
    GLint mm;

    dmp->dm_height = height;
    dmp->dm_width = width;
    dmp->dm_aspect = (fastf_t)dmp->dm_width / (fastf_t)dmp->dm_height;

    if (dmp->dm_debugLevel) {
	GLfloat m[16];
	bu_log("osg_reshape()\n");
	bu_log("width = %d, height = %d\n", dmp->dm_width, dmp->dm_height);
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	glGetFloatv (GL_PROJECTION_MATRIX, m);
    }

    glViewport(0, 0, dmp->dm_width, dmp->dm_height);

    glClearColor(((struct osg_vars *)dmp->dm_vars.priv_vars)->r,
		 ((struct osg_vars *)dmp->dm_vars.priv_vars)->g,
		 ((struct osg_vars *)dmp->dm_vars.priv_vars)->b,
		 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    glMatrixMode(mm);
}


HIDDEN int
osg_makeCurrent(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_makeCurrent()\n");

    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    return TCL_OK;
}


HIDDEN int
osg_configureWin(struct dm *dmp, int force)
{
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    return osg_configureWin_guts(dmp, force);
}


HIDDEN int
osg_setLight(struct dm *dmp, int lighting_on)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setLight()\n");

    dmp->dm_light = lighting_on;
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on = dmp->dm_light;

    if (!dmp->dm_light) {
	/* Turn it off */
	glDisable(GL_LIGHTING);
    } else {
	/* Turn it on */

	if (1 < dmp->dm_light)
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


/*
 * Gracefully release the display.
 */
HIDDEN int
osg_close(struct dm *dmp)
{
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->makeCurrent();
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->releaseContext();

    if (((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin)
	Tk_DestroyWindow(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);

    bu_vls_free(&dmp->dm_pathName);
    bu_vls_free(&dmp->dm_tkName);
    bu_vls_free(&dmp->dm_dName);
    bu_free(dmp->dm_vars.priv_vars, "osg_close: osg_vars");
    bu_free(dmp->dm_vars.pub_vars, "osg_close: dm_xvars");
    bu_free(dmp, "osg_close: dmp");

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
    bu_vls_init(&(dmp->dm_log));

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
    bu_vls_init(&(privvars->mvars.log));

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
	bu_log("dm-osg: Failed to open %s\n", bu_vls_addr(&dmp->dm_pathName));
	bu_vls_free(&init_proc_vls);
	(void)osg_close(dmp);
	return DM_NULL;
    }


    /* Make sure the window is large enough.  If these values are not large
     * enough, the drawing window will not resize properly to match
     * the height change of the parent window.  This is also true
     * of the ogl display manager, although it is undocumented *why*
     * this initial screen-based sizing is needed... */
    {
	int make_square = -1;

	if (dmp->dm_width == 0) {
	    bu_vls_sprintf(&str, "winfo screenwidth .");
	    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
		bu_vls_free(&init_proc_vls);
		bu_vls_free(&str);
		(void)osg_close(dmp);
		return DM_NULL;
	    } else {
		Tcl_Obj *tclresult = Tcl_GetObjResult(interp);
		dmp->dm_width = tclresult->internalRep.longValue - TK_SCREEN_OFFSET;
	    }
	    ++make_square;
	}
	if (dmp->dm_height == 0) {
	    bu_vls_sprintf(&str, "winfo screenheight .");
	    if (Tcl_Eval(interp, bu_vls_addr(&str)) == TCL_ERROR) {
		bu_vls_free(&init_proc_vls);
		bu_vls_free(&str);
		(void)osg_close(dmp);
		return DM_NULL;
	    } else {
		Tcl_Obj *tclresult = Tcl_GetObjResult(interp);
		dmp->dm_height = tclresult->internalRep.longValue - TK_SCREEN_OFFSET;
	    }
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
    }

    bu_vls_printf(&dmp->dm_tkName, "%s", (char *)Tk_Name(pubvars->xtkwin));

    bu_vls_sprintf(&str, "_init_dm %s %s\n",
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


    // Init the Windata Variable that holds the handle for the Window to display OSG in.
    // Check the QOSGWidget.cpp example for more logic relevant to this.  Need to find
    // something showing how to handle Cocoa for the Mac, if that's possible
    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
#if defined(DM_OSG)  /* Will eventually change to DM_X11 */
    osg::ref_ptr<osg::Referenced> windata = new osgViewer::GraphicsWindowX11::WindowData(((struct dm_xvars *)(dmp->dm_vars.pub_vars))->win);
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

    // Create the Graphics Context
    privvars->graphicsContext = osg::GraphicsContext::createGraphicsContext(traits.get());

    privvars->graphicsContext->realize();
    privvars->graphicsContext->makeCurrent();

    /* display list (fontOffset + char) will display a given ASCII char */
    if ((privvars->fontOffset = glGenLists(128))==0) {
	bu_log("dm-ogl: Can't make display lists for font.\n");
	(void)osg_close(dmp);
	return DM_NULL;
    }

    /* This is the applications display list offset */
    dmp->dm_displaylist = privvars->fontOffset + 128;

    osg_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (privvars->mvars.doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)osg_configureWin_guts(dmp, 1);

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

    osg_setZBuffer(dmp, dmp->dm_zbuffer);
    osg_setLight(dmp, dmp->dm_light);

    return dmp;
}


int
osg_share_dlist(struct dm *UNUSED(dmp1), struct dm *UNUSED(dmp2))
{
#if 0
    GLfloat backgnd[4];
    GLfloat vf;
    GLXContext old_glxContext;

    if (dmp1 == (struct dm *)NULL)
	return TCL_ERROR;

    if (dmp2 == (struct dm *)NULL) {
	/* create a new graphics context for dmp1 with private display lists */

	old_glxContext = ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc;

	if ((((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->vip,
			      (GLXContext)NULL, GL_TRUE))==NULL) {
	    bu_log("osg_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp1->dm_vars.pub_vars)->win,
			    ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc)) {
	    bu_log("osg_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* display list (fontOffset + char) will display a given ASCII char */
	if ((((struct osg_vars *)dmp1->dm_vars.priv_vars)->fontOffset = glGenLists(128))==0) {
	    bu_log("dm-ogl: Can't make display lists for font.\nUsing old context\n.");
	    ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	/* This is the applications display list offset */
	dmp1->dm_displaylist = ((struct osg_vars *)dmp1->dm_vars.priv_vars)->fontOffset + 128;

	osg_setBGColor(dmp1, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (((struct osg_vars *)dmp1->dm_vars.priv_vars)->mvars.doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* this is important so that osg_configureWin knows to set the font */
	((struct dm_xvars *)dmp1->dm_vars.pub_vars)->fontstruct = NULL;

	/* do viewport, ortho commands and initialize font */
	(void)osg_configureWin_guts(dmp1, 1);

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
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct osg_vars *)dmp1->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct osg_vars *)dmp1->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp1->dm_vars.pub_vars)->dpy, old_glxContext);
    } else {
	/* dmp1 will share its display lists with dmp2 */

	old_glxContext = ((struct osg_vars *)dmp2->dm_vars.priv_vars)->glxc;

	if ((((struct osg_vars *)dmp2->dm_vars.priv_vars)->glxc =
	     glXCreateContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			      ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->vip,
			      ((struct osg_vars *)dmp1->dm_vars.priv_vars)->glxc,
			      GL_TRUE))==NULL) {
	    bu_log("osg_share_dlist: couldn't create glXContext.\nUsing old context\n.");
	    ((struct osg_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	if (!glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp2->dm_vars.pub_vars)->win,
			    ((struct osg_vars *)dmp2->dm_vars.priv_vars)->glxc)) {
	    bu_log("osg_share_dlist: Couldn't make context current\nUsing old context\n.");
	    ((struct osg_vars *)dmp2->dm_vars.priv_vars)->glxc = old_glxContext;

	    return TCL_ERROR;
	}

	((struct osg_vars *)dmp2->dm_vars.priv_vars)->fontOffset = ((struct osg_vars *)dmp1->dm_vars.priv_vars)->fontOffset;
	dmp2->dm_displaylist = dmp1->dm_displaylist;

	osg_setBGColor(dmp2, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (((struct osg_vars *)dmp2->dm_vars.priv_vars)->mvars.doublebuffer)
	    glDrawBuffer(GL_BACK);
	else
	    glDrawBuffer(GL_FRONT);

	/* do viewport, ortho commands and initialize font */
	(void)osg_configureWin_guts(dmp2, 1);

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
	glGetDoublev(GL_PROJECTION_MATRIX, ((struct osg_vars *)dmp2->dm_vars.priv_vars)->faceplate_mat);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glLoadIdentity();
	((struct osg_vars *)dmp2->dm_vars.priv_vars)->face_flag = 1; /* faceplate matrix is on top of stack */

	/* destroy old context */
	glXMakeCurrent(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, None, NULL);
	glXDestroyContext(((struct dm_xvars *)dmp2->dm_vars.pub_vars)->dpy, old_glxContext);
    }
#endif
    return TCL_OK;
}


/*
 * There are global variables which are parameters to this routine.
 */
HIDDEN int
osg_drawBegin(struct dm *dmp)
{
    GLfloat fogdepth;

    if (dmp->dm_debugLevel) {
	bu_log("osg_drawBegin\n");
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "initial view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "initial projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->makeCurrent();

    /* clear back buffer */
    if (!dmp->dm_clearBufferAfter &&
	((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer) {
	glClearColor(((struct osg_vars *)dmp->dm_vars.priv_vars)->r,
		     ((struct osg_vars *)dmp->dm_vars.priv_vars)->g,
		     ((struct osg_vars *)dmp->dm_vars.priv_vars)->b,
		     0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (((struct osg_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	((struct osg_vars *)dmp->dm_vars.priv_vars)->face_flag = 0;
	if (((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	    glEnable(GL_FOG);
	    /*XXX Need to do something with Viewscale */
	    fogdepth = 2.2 * (*dmp->dm_vp); /* 2.2 is heuristic */
	    glFogf(GL_FOG_END, fogdepth);
	    fogdepth = (GLfloat) (0.5*((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.fogdensity/
				  (*dmp->dm_vp));
	    glFogf(GL_FOG_DENSITY, fogdepth);
	    glFogi(GL_FOG_MODE, dmp->dm_perspective ? GL_EXP : GL_LINEAR);
	}
	if (dmp->dm_light) {
	    glEnable(GL_LIGHTING);
	}
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "after begin view matrix = \n");

	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "after begin projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return TCL_OK;
}


HIDDEN int
osg_drawEnd(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawEnd()\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of end view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of end projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->dm_light) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    }

    ((struct osg_vars *)dmp->dm_vars.priv_vars)->graphicsContext->swapBuffers();
    if (dmp->dm_clearBufferAfter) {
	/* give Graphics pipe time to work */
	glClearColor(((struct osg_vars *)dmp->dm_vars.priv_vars)->r,
		((struct osg_vars *)dmp->dm_vars.priv_vars)->g,
		((struct osg_vars *)dmp->dm_vars.priv_vars)->b,
		0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (dmp->dm_debugLevel) {
	int error;
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "ANY ERRORS?\n");

	while ((error = glGetError())!=0) {
	    bu_vls_printf(&tmp_vls, "Error: %x\n", error);
	}

	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of drawend view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of drawend projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    return TCL_OK;
}


/*
 * Load a new transformation matrix.  This will be followed by
 * many calls to osg_draw().
 */
HIDDEN int
osg_loadMatrix(struct dm *dmp, fastf_t *mat, int which_eye)
{
    fastf_t *mptr;
    GLfloat gtmat[16];

    if (dmp->dm_debugLevel == 1)
	bu_log("osg_loadMatrix()\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }


    if (dmp->dm_debugLevel == 3) {
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&tmp_vls, "transformation matrix = \n");
	osg_printmat(&tmp_vls, mat);

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
	    osg_drawString2D(dmp, "R", 0.986, 0.0, 0, 1);
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
    glLoadMatrixf(gtmat);

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of loadMatrix view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of loadMatrix projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    return TCL_OK;
}


/*
 * Load a new projection matrix.
 *
 */
HIDDEN int
osg_loadPMatrix(struct dm *dmp, fastf_t *mat)
{
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
osg_drawVListHiddenLine(struct dm *dmp, struct bn_vlist *vp)
{
    register struct bn_vlist *tvp;
    register int first;

    if (dmp->dm_debugLevel == 1)
	bu_log("osg_drawVList()\n");


    /* First, draw polygons using background color. */

    if (dmp->dm_light) {
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
    glColor3f(((struct osg_vars *)dmp->dm_vars.priv_vars)->r,
	      ((struct osg_vars *)dmp->dm_vars.priv_vars)->g,
	      ((struct osg_vars *)dmp->dm_vars.priv_vars)->b);

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
	    if (dmp->dm_debugLevel > 2)
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
		    if (first)
			glBegin(GL_TRIANGLES);

			first = 0;

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
	    if (dmp->dm_debugLevel > 2)
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

    if (dmp->dm_light) {
	glEnable(GL_LIGHTING);
    }

    if (!((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on)
	glDisable(GL_DEPTH_TEST);

    if (!dmp->dm_depthMask)
	glDepthMask(GL_FALSE);

    glDisable(GL_POLYGON_OFFSET_FILL);

    return TCL_OK;
    }


HIDDEN int
osg_drawVList(struct dm *dmp, struct bn_vlist *vp)
{
    struct bn_vlist *tvp;
    register int first;
    register int mflag = 1;
    static float black[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat originalPointSize, originalLineWidth;

    glGetFloatv(GL_POINT_SIZE, &originalPointSize);
    glGetFloatv(GL_LINE_WIDTH, &originalLineWidth);

    if (dmp->dm_debugLevel == 1)
	bu_log("osg_drawVList()\n");

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
	    if (dmp->dm_debugLevel > 2)
		bu_log(" %d (%g %g %g)\n", *cmd, V3ARGS(dpt));*/

	    switch (*cmd) {
		case BN_VLIST_LINE_MOVE:
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
		    glVertex3dv(dpt);
		    break;
		case BN_VLIST_POLY_START:
		case BN_VLIST_TRI_START:
		    /* Start poly marker & normal */

		    if (dmp->dm_light && mflag) {
			mflag = 0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambientColor);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
			glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseColor);

			switch (dmp->dm_light) {
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

			if (dmp->dm_transparency)
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

    if (dmp->dm_light && dmp->dm_transparency)
	glDisable(GL_BLEND);

    glPointSize(originalPointSize);
    glLineWidth(originalLineWidth);

    return TCL_OK;
}


HIDDEN int
osg_draw(struct dm *dmp, struct bn_vlist *(*callback_function)(void *), genptr_t *data)
{
    struct bn_vlist *vp;
    if (!callback_function) {
	if (data) {
	    vp = (struct bn_vlist *)data;
	    osg_drawVList(dmp, vp);
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
osg_normal(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_normal\n");

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "beginning of osg_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "beginning of osg_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_log("%s", bu_vls_addr(&tmp_vls));
	bu_vls_free(&tmp_vls);
    }

    if (!((struct osg_vars *)dmp->dm_vars.priv_vars)->face_flag) {
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixd(((struct osg_vars *)dmp->dm_vars.priv_vars)->faceplate_mat);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	((struct osg_vars *)dmp->dm_vars.priv_vars)->face_flag = 1;
	if (((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on)
	    glDisable(GL_FOG);
	if (dmp->dm_light)
	    glDisable(GL_LIGHTING);
    }

    if (dmp->dm_debugLevel == 3) {
	GLfloat m[16];
	struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	bu_vls_printf(&tmp_vls, "end of osg_normal view matrix = \n");
	glGetFloatv (GL_MODELVIEW_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
	bu_vls_printf(&tmp_vls, "end of osg_normal projection matrix = \n");
	glGetFloatv (GL_PROJECTION_MATRIX, m);
	osg_printglmat(&tmp_vls, m);
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
osg_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_drawString2D()\n");

    if (use_aspect)
	glRasterPos2f(x, y * dmp->dm_aspect);
    else
	glRasterPos2f(x, y);

    glListBase(((struct osg_vars *)dmp->dm_vars.priv_vars)->fontOffset);
    glCallLists(strlen(str), GL_UNSIGNED_BYTE,  str);

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
osg_setFGColor(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    /*if (dmp->dm_debugLevel)
	bu_log("osg_setFGColor()\n");*/

    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;

    /* wireColor gets the full rgb */
    wireColor[0] = r / 255.0;
    wireColor[1] = g / 255.0;
    wireColor[2] = b / 255.0;
    wireColor[3] = transparency;

    if (strict) {
	glColor3ub((GLubyte)r, (GLubyte)g, (GLubyte)b);
    } else {

	if (dmp->dm_light) {
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
osg_setLineAttr(struct dm *dmp, int width, int style)
{
    /*if (dmp->dm_debugLevel)
	bu_log("osg_setLineAttr()\n");*/

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
osg_logfile(struct dm *dmp, const char *filename)
{
    bu_vls_sprintf(&dmp->dm_log, "%s", filename);

    return TCL_OK;
}

HIDDEN int
osg_setWinBounds(struct dm *dmp, fastf_t *w)
{
    GLint mm;

    if (dmp->dm_debugLevel)
	bu_log("osg_setWinBounds()\n");

    dmp->dm_clipmin[0] = w[0];
    dmp->dm_clipmin[1] = w[2];
    dmp->dm_clipmin[2] = w[4];
    dmp->dm_clipmax[0] = w[1];
    dmp->dm_clipmax[1] = w[3];
    dmp->dm_clipmax[2] = w[5];

    glGetIntegerv(GL_MATRIX_MODE, &mm);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLoadIdentity();
    glOrtho(-xlim_view, xlim_view, -ylim_view, ylim_view, dmp->dm_clipmin[2], dmp->dm_clipmax[2]);
    glPushMatrix();
    glMatrixMode(mm);

    return TCL_OK;
}


HIDDEN int
osg_setTransparency(struct dm *dmp,
		    int transparency_on)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setTransparency()\n");

    dmp->dm_transparency = transparency_on;
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on = dmp->dm_transparency;

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
osg_setDepthMask(struct dm *dmp,
		 int enable) {
    if (dmp->dm_debugLevel)
	bu_log("osg_setDepthMask()\n");

    dmp->dm_depthMask = enable;

    if (enable)
	glDepthMask(GL_TRUE);
    else
	glDepthMask(GL_FALSE);

    return TCL_OK;
}


HIDDEN int
osg_setZBuffer(struct dm *dmp, int zbuffer_on)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_setZBuffer:\n");

    dmp->dm_zbuffer = zbuffer_on;
    ((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;

    if (((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.zbuf == 0) {
	dmp->dm_zbuffer = 0;
	((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on = dmp->dm_zbuffer;
    }

    if (((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on) {
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_DEPTH_TEST);
    } else {
	glDisable(GL_DEPTH_TEST);
    }

    return TCL_OK;
}


HIDDEN int
osg_beginDList(struct dm *dmp, unsigned int list)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_beginDList()\n");

    glNewList((GLuint)list, GL_COMPILE);
    return TCL_OK;
}


HIDDEN int
osg_endDList(struct dm *dmp)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_endDList()\n");

    glEndList();
    return TCL_OK;
}


HIDDEN void
osg_drawDList(unsigned int list)
{
    glCallList((GLuint)list);
}


HIDDEN int
osg_freeDLists(struct dm *dmp, unsigned int list, int range)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_freeDLists()\n");

    glDeleteLists((GLuint)list, (GLsizei)range);
    return TCL_OK;
}


HIDDEN int
osg_genDLists(struct dm *dmp, size_t range)
{
    if (dmp->dm_debugLevel)
	bu_log("osg_freeDLists()\n");

    return glGenLists((GLsizei)range);
}


HIDDEN int
osg_getDisplayImage(struct dm *dmp, unsigned char **image)
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
#if defined(DM_WGL)
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

    if (dmp->dm_type == DM_TYPE_WGL || dmp->dm_type == DM_TYPE_OGL) {
	width = dmp->dm_width;
	height = dmp->dm_height;

	pixels = (GLuint *)bu_calloc(width * height, sizeof(GLuint), "pixels");

	{
	    glReadBuffer(GL_FRONT);
#if defined(DM_WGL)
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

	    for (h = 0; h < height; h++) {
		for (w = 0; w < width; w++) {
		    int i = h*width + w;
		    int i_h_inv = (height - h - 1)*width + w;
		    int j = i*bytes_per_pixel;
		    unsigned char *value = (unsigned char *)(idata + j);
#if defined(DM_WGL)
		    unsigned char alpha;
#endif

		    pixel = pixels[i_h_inv];

		    value[0] = (pixel & red_mask) >> 24;
		    value[1] = (pixel & green_mask) >> 16;
		    value[2] = (pixel & blue_mask) >> 8;

#if defined(DM_WGL)
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
    } else {
	bu_log("osg_getDisplayImage: Display type not set as OGL or WGL\n");
	return TCL_ERROR;
    }

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
