/*                    D M - S W R A S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1988-2024 United States Government as represented by
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
/** @file libdm/dm-swrast.c
 *
 * A Software Rasterizer based OpenGL Display Manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "OSMesa/gl.h"
#include "OSMesa/osmesa.h"

#undef VMIN		/* is used in vmath.h, too */

extern "C" {
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "bv/defines.h"
#include "dm.h"
#include "../null/dm-Null.h"
#include "../dm-gl.h"
}
#include "./dm-swrast.h"

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

static struct dm *swrast_open(void *ctx, void *interp, int argc, const char **argv);
static int swrast_close(struct dm *dmp);
static int swrast_drawString2D(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
static int swrast_String2DBBox(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t x, fastf_t y, int size, int use_aspect);
static int swrast_configureWin(struct dm *dmp, int force);
static int swrast_makeCurrent(struct dm *dmp);


static int
swrast_makeCurrent(struct dm *dmp)
{
    struct swrast_vars *pv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;

    if (!pv || !pv->ctx || !pv->v) {
	bu_log("swrast_configureWin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    if (dmp->i->dm_debugLevel)
	bu_log("swrast_makeCurrent()\n");

    if (!OSMesaMakeCurrent(pv->ctx, pv->os_b, GL_UNSIGNED_BYTE, pv->v->gv_width, pv->v->gv_height)) {
	bu_log("OSMesaMakeCurrent failed!\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
swrast_doevent(struct dm *UNUSED(dmp), void *UNUSED(vclientData), void *UNUSED(veventPtr))
{
    /* allow further processing of this event */
    return BRLCAD_OK;
}

static int
swrast_configureWin(struct dm *dmp, int UNUSED(force))
{
    struct swrast_vars *pv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;

    if (!pv || !pv->ctx || !pv->v) {
	bu_log("swrast_configureWin: Couldn't make context current\n");
	return BRLCAD_ERROR;
    }

    int width = pv->v->gv_width;
    int height = pv->v->gv_height;

    if (!width || !height) {
	bu_log("swrast_configureWin: Zero sized window\n");
	return BRLCAD_ERROR;
    }

    // 4096 is from MAX_WIDTH and MAX_HEIGHT as set in OSMesa's config.h.  We
    // need to provide enough memory here for OSMesa to do all of its work
    // (textures, etc.), not just the current dm image.
    pv->os_b = bu_realloc(pv->os_b, 4096 * 4096 * sizeof(GLubyte)*4, "OSMesa rendering buffer");
    if (!pv->os_b) {
	bu_log("swrast_configureWin: render buffer allocation failed\n");
	return BRLCAD_ERROR;
    }

    if (!OSMesaMakeCurrent(pv->ctx, pv->os_b, GL_UNSIGNED_BYTE, width, height)) {
	bu_log("OSMesaMakeCurrent failed!\n");
	return BRLCAD_ERROR;
    }

    gl_reshape(dmp, width, height);

    /* this is where font information is set up, if not already done */
    if (!pv->fs) {
	pv->fs = glfonsCreate(512, 512, FONS_ZERO_TOPLEFT);
	if (pv->fs == NULL) {
	    bu_log("dm-swrast: Failed to create font stash");
	    return BRLCAD_ERROR;
	}
	pv->fontNormal = FONS_INVALID;
	pv->fontNormal = fonsAddFont(pv->fs, "sans", bu_dir(NULL, 0, BU_DIR_DATA, "fonts", "ProFont.ttf", NULL));
    }

    return BRLCAD_OK;
}

/*
 * Gracefully release the display.
 */
int
swrast_close(struct dm *dmp)
{
    struct swrast_vars *pv = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    bu_free(pv->os_b, "OSMesa rendering buffer");
    bu_vls_free(&dmp->i->dm_dName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_pathName);
    OSMesaDestroyContext((OSMesaContext)pv->ctx);
    bu_free(dmp->i->dm_vars.priv_vars, "swrast_close: swrast_vars");
    bu_free(dmp->i->dm_vars.pub_vars, "swrast_close: dm_swvars");
    bu_vls_free(&(dmp->i->dm_log));
    BU_PUT(dmp->i, struct dm_impl);
    BU_PUT(dmp, struct dm);

    return BRLCAD_OK;
}

int
swrast_viable(const char *UNUSED(dpy_string))
{
    return 1;
}

/*
 * Fire up the display manager, and the display processor.
 *
 */
struct dm *
swrast_open(void *ctx, void *UNUSED(interp), int argc, const char **argv)
{
    static int count = 0;
    GLfloat backgnd[4];
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls init_proc_vls = BU_VLS_INIT_ZERO;
    struct dm *dmp = NULL;
    struct dm_impl *dmpi = NULL;
    struct gl_vars *mvars = NULL;
    struct dm_swvars *pubvars = NULL;
    struct swrast_vars *privars = NULL;

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_GET(dmpi, struct dm_impl);
    *dmpi = *dm_swrast.i; /* struct copy */
    dmp->i = dmpi;

    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&(dmp->i->dm_log));

    BU_ALLOC(dmp->i->dm_vars.pub_vars, struct dm_swvars);
    pubvars = (struct dm_swvars *)dmp->i->dm_vars.pub_vars;

    BU_ALLOC(dmp->i->dm_vars.priv_vars, struct swrast_vars);
    privars = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    privars->v = (struct bview *)ctx;
    // Note - for Qt, dealing with GL_RGB data display was something of a pain.  This backend
    // was switched to RGBA to make it easier to display the output
    privars->ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    int width = (!privars->v->gv_width) ? 512 : privars->v->gv_width;
    int height = (!privars->v->gv_height) ? 512 : privars->v->gv_height;
    privars->v->gv_width = width;
    privars->v->gv_height = height;
    privars->os_b = bu_realloc(privars->os_b, width * height * sizeof(GLubyte)*4, "OSMesa rendering buffer");
    if (!OSMesaMakeCurrent(privars->ctx, privars->os_b, GL_UNSIGNED_BYTE, width, height)) {
	bu_log("OSMesaMakeCurrent failed!\n");
	bu_free(dmp->i->dm_vars.pub_vars, "swrast_open: dmp->i->dm_vars.pub_vars");
	bu_free(dmp, "swrast_open: dmp");
	return DM_NULL;
    }

    dmp->i->dm_get_internal(dmp);
    mvars = (struct gl_vars *)dmp->i->m_vars;
    glvars_init(dmp);

    dmp->i->dm_vp = &mvars->i.default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    dm_processOptions(dmp, &init_proc_vls, --argc, ++argv);

    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_swrast%d", count);
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
    mvars->lighting_on = 1;
    mvars->zbuffer_on = 1;
    mvars->zclipping_on = 0;
    mvars->bound = 1.0;
    mvars->boundFlag = 1;

    /* this is important so that swrast_configureWin knows to set the font */
    privars->fs = NULL;


    bu_vls_free(&init_proc_vls);
    bu_vls_free(&str);


    dm_make_current(dmp);
    gl_setBGColor(dmp, 0, 0, 0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mvars->doublebuffer)
	glDrawBuffer(GL_BACK);
    else
	glDrawBuffer(GL_FRONT);

    /* do viewport, ortho commands and initialize font */
    (void)swrast_configureWin(dmp, 1);

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

    gl_setZBuffer(dmp, mvars->zbuffer_on);
    gl_setLight(dmp, mvars->lighting_on);

    return dmp;
}

/*
 * Output a string.
 * The starting position of the beam is as specified.
 */
static int
swrast_drawString2D(struct dm *dmp, const char *str, fastf_t ix, fastf_t iy, int UNUSED(size), int use_aspect)
{
    struct gl_vars *mvars = (struct gl_vars *)dmp->i->m_vars;
    struct swrast_vars *privars = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("swrast_drawString2D()\n");

    // If the positions are out of range on the positive side, just don't draw -
    // text will go to the right and not be visible
    fastf_t x = ix;
    fastf_t y = (use_aspect) ? iy * dmp->i->dm_aspect : iy;
    if (x > 1 || y > 1)
       return BRLCAD_OK;

    // If we're given coordinates out of the view bounds to the left we need to do
    // some shifting so the gl raster position routines can work - we may need to do
    // partial drawing if the text is long enough.
    int nxc = (x < -1) ? 1 : 0;
    int nyc = (y < -1) ? 1 : 0;
    x = (nxc) ? x + 2 : x;
    y = (nyc) ? y + 2 : y;

    if (privars->fontNormal != FONS_INVALID) {

	/* First, we set the position using glRasterPos2f like ogl does */
	glRasterPos2f(x, y);

	/* Next, we set up for fontstash */
	fastf_t font_size = dm_get_fontsize(dmp);
	int blend_state = glIsEnabled(GL_BLEND);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* We want to restore the original state we were in when
	 * we started the text operation - save it */
	GLint mm;
	glGetIntegerv(GL_MATRIX_MODE, &mm);

	// Set up an identity matrix for text drawing
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	// Fontstash does not work in OpenGL raster coordinates,
	// so we need the view and the coordinates in window
	// XY coordinates.
	glOrtho(0,dm_get_width(dmp),dm_get_height(dmp),0,-1,1);
	GLfloat pos[4];
	glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
	fastf_t coord_x = pos[0];
	fastf_t coord_y = (fastf_t)dm_get_height(dmp)-pos[1];
	coord_x = (nxc) ? -1*(dm_get_width(dmp) - coord_x) : coord_x;
	coord_y = (nyc) ? coord_y + (fastf_t)dm_get_height(dmp) : coord_y;

	// Have info and OpenGL state, do the text drawing
	fonsSetFont(privars->fs, privars->fontNormal);
	unsigned int color = glfonsRGBA(dmp->i->dm_fg[0], dmp->i->dm_fg[1], dmp->i->dm_fg[2], 255);
	fonsSetSize(privars->fs, (int)font_size); /* cast to int so we always get a font */
	fonsSetColor(privars->fs, color);
	fonsDrawText(privars->fs, coord_x, coord_y, str, NULL);

	// Restore previous projection matrix
	glPopMatrix();

	// Restore view matrix (changed by glOrtho call)
	glPopMatrix();

	// Put us back in whatever mode we were in before starting the text draw
	glMatrixMode(mm);

	if (!blend_state) glDisable(GL_BLEND);

	glOrtho(-mvars->i.xlim_view, mvars->i.xlim_view, -mvars->i.ylim_view, mvars->i.ylim_view, dmp->i->dm_clipmin[2], dmp->i->dm_clipmax[2]);
    }
    return BRLCAD_OK;
}

static int
swrast_String2DBBox(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t ix, fastf_t iy, int UNUSED(size), int use_aspect)
{
    struct swrast_vars *privars = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;
    if (dmp->i->dm_debugLevel)
	bu_log("qtgl_drawString2D()\n");

    // If the positions are out of range on the positive side, just don't draw -
    // text will go to the right and not be visible
    fastf_t x = ix;
    fastf_t y = (use_aspect) ? iy * dmp->i->dm_aspect : iy;
    if (x > 1 || y > 1)
	return BRLCAD_ERROR;

    // If we're given coordinates out of the view bounds to the left we need to do
    // some shifting so the gl raster position routines can work - we may need to do
    // partial drawing if the text is long enough.
    int nxc = (x < -1) ? 1 : 0;
    int nyc = (y < -1) ? 1 : 0;
    x = (nxc) ? x + 2 : x;
    y = (nyc) ? y + 2 : y;

    if (privars->fontNormal != FONS_INVALID) {

	/* Stash the previous raster position */
	GLfloat rasterpos[4];
	glGetFloatv(GL_CURRENT_RASTER_POSITION, rasterpos);

	/* First, we set the position using glRasterPos2f like ogl does */
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
	coord_x = (nxc) ? -1*(dm_get_width(dmp) - coord_x) : coord_x;
	coord_y = (nyc) ? coord_y + (fastf_t)dm_get_height(dmp) : coord_y;

	// Have info and OpenGL state, proceed
	fonsSetFont(privars->fs, privars->fontNormal);
	fonsSetSize(privars->fs, (int)font_size); /* cast to int so we always get a font */

	float bounds[4] = {0.0, 0.0, 0.0, 0.0};
	fonsTextBounds(privars->fs, coord_x, coord_y, str, NULL, (float *)bounds);
	//int width = fonsTextBounds(privars->fs, coord_x, coord_y, str, NULL, (float *)bounds);
	//bu_log("%s bounds: min(%f,%f) max(%f,%f)\n", str, bounds[0], bounds[1], bounds[2], bounds[3]);
	//bu_log("%s width %d\n", str, width);

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
swrast_openFb(struct dm *dmp)
{
    struct fb_platform_specific *fb_ps;
    //struct swrast_fb_info *ofb_ps;
    //struct swrast_vars *privars = (struct swrast_vars *)dmp->i->dm_vars.priv_vars;

    fb_ps = fb_get_platform_specific(FB_SWFB_MAGIC);
    //ofb_ps = (struct swrast_fb_info *)fb_ps->data;
    //ofb_ps->glc = privars->qw;
    dmp->i->fbp = fb_open_existing("swrast", dm_get_width(dmp), dm_get_height(dmp), fb_ps);
    fb_put_platform_specific(fb_ps);
    return 0;
}

int
swrast_geometry_request(struct dm *dmp, int UNUSED(width), int UNUSED(height))
{
    if (!dmp) return -1;
    //Tk_GeometryRequest(((struct dm_swvars *)dmp->i->dm_vars.pub_vars)->xtkwin, width, height);
    return 0;
}

#define SWVARS_MV_O(_m) offsetof(struct dm_swvars, _m)

struct bu_structparse dm_swvars_vparse[] = {
    {"%d",      1,      "devmotionnotify",      SWVARS_MV_O(devmotionnotify),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonpress",       SWVARS_MV_O(devbuttonpress),     BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",      1,      "devbuttonrelease",     SWVARS_MV_O(devbuttonrelease),   BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",        0,      (char *)0,              0,                      BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};

int
swrast_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result) return -1;
    if (!key) {
        // Print all current vars
        bu_vls_struct_print2(result, "dm internal GLX variables", dm_swvars_vparse, (const char *)dmp->i->dm_vars.pub_vars);
        return 0;
    }
    // Print specific var
    bu_vls_struct_item_named(result, dm_swvars_vparse, key, (const char *)dmp->i->dm_vars.pub_vars, ',');
    return 0;
}


// TODO - this and getDisplayImage need to be consolidated...
int
swrast_write_image(struct bu_vls *UNUSED(msgs), FILE *UNUSED(fp), struct dm *UNUSED(dmp))
{
    return -1;
}

int
swrast_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    struct dm_swvars *pubvars = (struct dm_swvars *)dmp->i->dm_vars.pub_vars;
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

struct dm_impl dm_swrast_impl = {
    swrast_open,
    swrast_close,
    swrast_viable,
    gl_drawBegin,
    gl_drawEnd,
    gl_hud_begin,
    gl_hud_end,
    gl_loadMatrix,
    gl_loadPMatrix,
    gl_popPMatrix,
    swrast_drawString2D,
    swrast_String2DBBox,
    gl_drawLine2D,
    gl_drawLine3D,
    gl_drawLines3D,
    gl_drawPoint2D,
    gl_drawPoint3D,
    gl_drawPoints3D,
    gl_drawVList,
    gl_drawVListHiddenLine,
    gl_draw_obj,
    gl_draw_data_axes,
    gl_draw,
    gl_setFGColor,
    gl_setBGColor,
    gl_setLineAttr,
    swrast_configureWin,
    gl_setWinBounds,
    gl_setLight,
    gl_getLight,
    gl_setTransparency,
    gl_getTransparency,
    gl_setDepthMask,
    gl_setZBuffer,
    gl_getZBuffer,
    gl_setZClip,
    gl_getZClip,
    gl_setBound,
    gl_getBound,
    gl_setBoundFlag,
    gl_getBoundFlag,
    gl_debug,
    gl_logfile,
    gl_beginDList,
    gl_endDList,
    gl_drawDList,
    gl_freeDLists,
    gl_genDLists,
    gl_draw_display_list,
    gl_getDisplayImage, /* display to image function */
    gl_reshape,
    swrast_makeCurrent,
    null_SwapBuffers,
    swrast_doevent,
    swrast_openFb,
    gl_get_internal,
    gl_put_internal,
    swrast_geometry_request,
    swrast_internal_var,
    swrast_write_image,
    NULL,
    NULL,
    swrast_event_cmp,
    gl_fogHint,
    NULL, //swrast_share_dlist,
    0,
    1,				/* is graphical (sort of...) */
    "osmesa",                   /* uses OSMesa software rasterizer */
    1,				/* has displaylist */
    0,                          /* no stereo by default */
    "swrast",
    "OSMesa swrast graphics",
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
    {0, 0, 0},			/* bg1 color */
    {0, 0, 0},			/* bg2 color */
    {0, 0, 0},			/* fg color */
    {GED_MIN, GED_MIN, GED_MIN},	/* clipmin */
    {GED_MAX, GED_MAX, GED_MAX},	/* clipmax */
    0,				/* no debugging */
    0,				/* no perspective */
    1,				/* depth buffer is writable */
    0,                          /* clear back buffer after drawing and swap */
    0,                          /* not overriding the auto font size */
    gl_vparse,
    FB_NULL,
    0,				/* Tcl interpreter */
    NULL,                       /* Drawing context */
    NULL                        /* App data */
};

struct dm dm_swrast = { DM_MAGIC, &dm_swrast_impl, 0 };

#ifdef DM_PLUGIN
static const struct dm_plugin pinfo = { DM_API, &dm_swrast };
extern "C" {
COMPILER_DLLEXPORT const struct dm_plugin *dm_plugin_info(void)
{
    return &pinfo;
}
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

