/*                      D M - Q T G L . C P P
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
/** @file libdm/dm-ogl.c
 *
 * A Qt OpenGL Display Manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif

#include <QEvent>
#include <QtWidgets/QOpenGLWidget>

#undef VMIN		/* is used in vmath.h, too */

extern "C" {
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "bview/defines.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "../dm-gl.h"
}
#include "./fb_qtgl.h"
#include "./dm-qtgl.h"

#include "../include/private.h"

#define ENABLE_POINT_SMOOTH 1

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

static struct dm *qtgl_open(void *ctx, void *interp, int argc, const char **argv);
static int qtgl_close(struct dm *dmp);
static int qtgl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
static int qtgl_String2DBBox(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
static int qtgl_configureWin(struct dm *dmp, int force);
static int qtgl_makeCurrent(struct dm *dmp);


static int
qtgl_makeCurrent(struct dm *dmp)
{
    struct qtgl_vars *privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;

    if (dmp->i->dm_debugLevel)
	bu_log("qtgl_makeCurrent()\n");

    privars->qw->makeCurrent();

    return BRLCAD_OK;
}

static int
qtgl_doevent(struct dm *dmp, void *UNUSED(vclientData), void *veventPtr)
{
    QEvent *eventPtr= (QEvent *)veventPtr;
    if (eventPtr->type() == QEvent::Expose) {
	(void)dm_make_current(dmp);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	dm_set_dirty(dmp, 1);
	return BRLCAD_OK;
    }
    /* allow further processing of this event */
    return BRLCAD_OK;
}

static int
qtgl_configureWin(struct dm *dmp, int UNUSED(force))
{
    struct qtgl_vars *privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;

    if (!privars || !privars->qw) {
	bu_log("qtgl_configureWin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    gl_reshape(dmp, privars->qw->width(), privars->qw->height());

    /* this is where font information is set up, if not already done */
    if (!privars->fs) {
	privars->fs = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
	if (privars->fs == NULL) {
	    bu_log("dm-qtgl: Failed to create font stash");
	    return BRLCAD_ERROR;
	}
	privars->fontNormal = FONS_INVALID;
	privars->fontNormal = fonsAddFont(privars->fs, "sans", bu_dir(NULL, 0, BU_DIR_DATA, "fonts", "ProFont.ttf", NULL));
    }

    return BRLCAD_OK;
}

/*
 * Gracefully release the display.
 */
int
qtgl_close(struct dm *dmp)
{
    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_free(dmp->i->dm_vars.priv_vars, "qtgl_close: qtgl_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "qtgl_close: dm_qtvars");
    BU_PUT(dmp->i, struct dm_impl);
    BU_PUT(dmp, struct dm);

    return BRLCAD_OK;
}

int
qtgl_viable(const char *UNUSED(dpy_string))
{
    return 1;
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
qtgl_open(void *ctx, void *UNUSED(interp), int argc, const char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = NULL;
    struct dm_impl *dmpi = NULL;
    struct gl_vars *mvars = NULL;
    struct dm_qtvars *pubvars = NULL;
    struct qtgl_vars *privars = NULL;

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_GET(dmpi, struct dm_impl);
    *dmpi = *dm_qtgl.i; /* struct copy */
    dmp->i = dmpi;

    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_light = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->i->dm_log));

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_qtvars);
    if (dmp->i->dm_vars.pub_vars == (void *)NULL) {
	bu_free(dmp, "qtgl_open: dmp");
	return DM_NULL;
    }
    pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct qtgl_vars);
    if (dmp->i->dm_vars.priv_vars == (void *)NULL) {
	bu_free(dmp->i->dm_vars.pub_vars, "qtgl_open: dmp->i->dm_vars.pub_vars");
	bu_free(dmp, "qtgl_open: dmp");
	return DM_NULL;
    }
    privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;
    privars->qw = (QOpenGLWidget *)ctx;

    dmp->i->dm_get_internal(dmp);
    mvars = (struct gl_vars *)dmp->i->m_vars;
    glvars_init(dmp);

    dmp->i->dm_vp = &mvars->i.default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_qtgl%d", count);
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
    pubvars->devmotionnotify = 0;
    pubvars->devbuttonpress = 0;
    pubvars->devbuttonrelease = 0;
    dmp->i->dm_aspect = 1.0;
    dmp->i->dm_fontsize = 20;

    /* initialize modifiable variables */
    mvars->depth = 24;
    mvars->zbuf = 1;
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

    /* this is important so that qtgl_configureWin knows to set the font */
    privars->fs = NULL;


    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);


    dm_make_current(dmp);
    gl_setBGColor(dmp, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)qtgl_configureWin(dmp, 1);

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
    glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, mvars->i.faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    mvars->i.faceFlag = 1;	/* faceplate matrix is on top of stack */

    gl_setZBuffer(dmp, dmp->i->dm_zbuffer);
    gl_setLight(dmp, dmp->i->dm_light);

    return dmp;
}

/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
qtgl_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct qtgl_vars *privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("qtgl_drawString2D()\n");

    if (privars->fontNormal != FONS_INVALID) {

	/* First, we set the position using glRasterPos2f like ogl does */
	if (use_aspect)
	    glRasterPos2f(x, y * dmp->i->dm_aspect);
	else
	    glRasterPos2f(x, y);

	/* Next, we set up for fontstash */
	fastf_t font_size = dm_get_fontsize(dmp);
	int blend_state = glIsEnabled(GL_BLEND);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Fontstash does not work in OpenGL raster coordinates,
	// so we need the view and the coordinates in window
	// XY coordinates.
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0,dm_get_width(dmp),dm_get_height(dmp),0,-1,1);
	GLfloat pos[4];
	glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
	fastf_t coord_x = pos[0];
	fastf_t coord_y = (fastf_t)dm_get_height(dmp)-pos[1];
	//printf("%f:%f,%f:%f\n",x, coord_x, y, coord_y);

	// Have info and OpenGL state, do the text drawing
	fonsSetFont(privars->fs, privars->fontNormal);
	unsigned int color = glfonsRGBA(dmp->i->dm_fg[0], dmp->i->dm_fg[1], dmp->i->dm_fg[2], 255);
	fonsSetSize(privars->fs, (int)font_size); /* cast to int so we always get a font */
	fonsSetColor(privars->fs, color);
	fonsDrawText(privars->fs, coord_x, coord_y, str, NULL);

#if 0
	// Debugging - see what the bounds are:
	float bounds[4];
	int width = fonsTextBounds(privars->fs, coord_x, coord_y, str, NULL, (float *)bounds);
	bu_log("%s bounds: min(%f,%f) max(%f,%f)\n", str, bounds[0], bounds[1], bounds[2], bounds[3]);
	bu_log("%s width %d\n", str, width);
#endif

	// Done with text, put matrices back
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	if (!blend_state) glDisable(GL_BLEND);

	glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    }
    return BRLCAD_OK;
}

static int
qtgl_String2DBBox(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t x, fastf_t y, int UNUSED(size), int use_aspect)
{
    struct qtgl_vars *privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("qtgl_drawString2D()\n");

    if (privars->fontNormal != FONS_INVALID) {

	/* First, we set the position using glRasterPos2f like ogl does */
	if (use_aspect)
	    glRasterPos2f(x, y * dmp->i->dm_aspect);
	else
	    glRasterPos2f(x, y);

	/* Next, we set up for fontstash */
	fastf_t font_size = dm_get_fontsize(dmp);

	// Fontstash does not work in OpenGL raster coordinates,
	// so we need the view and the coordinates in window
	// XY coordinates.
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0,dm_get_width(dmp),dm_get_height(dmp),0,-1,1);
	GLfloat pos[4];
	glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
	fastf_t coord_x = pos[0];
	fastf_t coord_y = (fastf_t)dm_get_height(dmp)-pos[1];

	// Have info and OpenGL state, proceed
	fonsSetFont(privars->fs, privars->fontNormal);
	fonsSetSize(privars->fs, (int)font_size); /* cast to int so we always get a font */

	float bounds[4];
	int width = fonsTextBounds(privars->fs, coord_x, coord_y, str, NULL, (float *)bounds);
	bu_log("%s bounds: min(%f,%f) max(%f,%f)\n", str, bounds[0], bounds[1], bounds[2], bounds[3]);
	bu_log("%s width %d\n", str, width);

	// Done with text, put matrices back
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	if (bmin)
	    V2SET(*bmin, bounds[0], bounds[1]);
	if (bmax)
	    V2SET(*bmax, bounds[2], bounds[3]);
    }
    return BRLCAD_OK;
}

int
qtgl_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    struct qtgl_fb_info *ofb_ps;
    struct qtgl_vars *privars = (struct qtgl_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_QTGL_MAGIC);
    ofb_ps = (struct qtgl_fb_info *)fb_ps->data;
    ofb_ps->glc = privars->qw;
    dmp->i->fbp = fb_open_existing("qtgl", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

int
qtgl_geometry_request(struct dm *dmp, int UNUSED(width), int UNUSED(height))
{
    if (!dmp) return -1;
    //Tk_GeometryRequest(((struct dm_qtvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define QTVARS_MV_O(_m) offsetof(struct dm_qtvars, _m)

struct bu_structparse dm_qtvars_vparse[] = {
    {"%d",      1,      "devmotionnotify",      QTVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       QTVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     QTVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
qtgl_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal GLX variables", dm_qtvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_qtvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}


// TODO - this and getDisplayImage need to be consolidated...
int
qtgl_write_image(struct bu_vls *UNUSED(msgs), FILE *UNUSED(fp), struct dm *UNUSED(dmp))
{
    return -1;
}

int
qtgl_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_qtvars *pubvars = (struct dm_qtvars *)dmp->i->dm_vars.pub_vars;
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

struct dm_impl dm_qtgl_impl = {
    qtgl_open,
    qtgl_close,
    qtgl_viable,
    gl_drawBegin,
    gl_drawEnd,
    gl_hud_begin,
    gl_hud_end,
    gl_loadMatrix,
    gl_loadPMatrix,
    qtgl_drawString2D,
    qtgl_String2DBBox,
    gl_drawLine2D,
    gl_drawLine3D,
    gl_drawLines3D,
    gl_drawPoint2D,
    gl_drawPoint3D,
    gl_drawPoints3D,
    gl_drawVList,
    gl_drawVListHiddenLine,
    gl_draw_data_axes,
    gl_draw,
    gl_setFGColor,
    gl_setBGColor,
    gl_setLineAttr,
    qtgl_configureWin,
    gl_setWinBounds,
    gl_setLight,
    gl_setTransparency,
    gl_setDepthMask,
    gl_setZBuffer,
    gl_debug,
    gl_logfile,
    gl_beginDList,
    gl_endDList,
    gl_drawDList,
    gl_freeDLists,
    gl_genDLists,
    gl_draw_obj,
    gl_getDisplayImage, /* display to image function */
    gl_reshape,
    qtgl_makeCurrent,
    null_SwapBuffers,
    qtgl_doevent,
    qtgl_openFb,
    gl_get_internal,
    gl_put_internal,
    qtgl_geometry_request,
    qtgl_internal_var,
    qtgl_write_image,
    NULL,
    NULL,
    qtgl_event_cmp,
    gl_fogHint,
    NULL, //qtgl_share_dlist,
    0,
    1,				/* is graphical */
    "Qt",                       /* uses Qt graphics system */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    1.0,			/* zoom-in limit */
    1,				/* bound flag */
    "qtgl",
    "Qt Windows with OpenGL graphics",
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
    gl_vparse,
    FB_NULL,
    0				/* Tcl interpreter */
};

struct dm dm_qtgl = { DM_MAGIC, &dm_qtgl_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_qtgl };
extern "C" {
COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info()
{
    return &pinfo;
}
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
