/*                    D M - E X T G L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libdm/dm-extgl.cpp
 *
 * Display manager for an OpenGL context owned by an external host.
 */

#include "common.h"

#include <algorithm>
#include <cmath>

extern "C" {
#include "bn.h"
#include "bu.h"
#include "bv/defines.h"
#include "dm.h"
#include "dm/extgl.h"
#include "dm/util.h"
#include "dm-gl.h"
#include "null/dm-Null.h"
}

#include "include/private.h"

#define VIEWFACTOR (1.0 / (*dmp->i->dm_vp))

struct extgl_vars {
    struct dm_extgl_context host;
};

static int
extgl_make_current(struct dm *dmp)
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    if (!vars || !vars->host.make_current ||
	vars->host.make_current(vars->host.data) != 0) {
	bu_log("dm-extgl: could not make the host context current\n");
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
extgl_configure_win(struct dm *dmp, int UNUSED(force))
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    int width = 0;
    int height = 0;

    if (!vars || !vars->host.get_framebuffer_size ||
	vars->host.get_framebuffer_size(vars->host.data, &width, &height) != 0 ||
	width <= 0 || height <= 0) {
	bu_log("dm-extgl: invalid host framebuffer size\n");
	return BRLCAD_ERROR;
    }
    if (extgl_make_current(dmp) != BRLCAD_OK)
	return BRLCAD_ERROR;

    int result = gl_reshape(dmp, width, height);
    if (result == BRLCAD_OK && dmp->i->fbp &&
	(fb_getwidth(dmp->i->fbp) != width ||
	 fb_getheight(dmp->i->fbp) != height)) {
	result = fb_configure_window(dmp->i->fbp, width, height) == 0
	    ? BRLCAD_OK : BRLCAD_ERROR;
    }
    return result;
}

static int
extgl_present(struct dm *dmp)
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    if (!vars || !vars->host.present)
	return BRLCAD_OK;
    return vars->host.present(vars->host.data) == 0 ? BRLCAD_OK : BRLCAD_ERROR;
}

struct extgl_fb_axis {
    int first;
    int count;
    double origin;
    double span;
};

static struct extgl_fb_axis
extgl_clip_axis(int first, int count, int image_size, int viewport_size, int center, int zoom)
{
    const double pixel_offset = 0.25 / zoom;
    const double span = (double)viewport_size / zoom;
    const int64_t visible_first = (int64_t)center - viewport_size / (2LL * zoom);
    const int64_t requested_end = (int64_t)first + count;
    const int64_t clipped_first = std::max<int64_t>({0,
	    std::min<int64_t>(first, requested_end), visible_first});
    const int64_t clipped_end = std::min<int64_t>({image_size,
	    std::max<int64_t>(first, requested_end), visible_first + (int64_t)std::ceil(span)});
    if (clipped_first >= clipped_end)
	return {0, 0, 0, 0};
    return {(int)clipped_first, (int)(clipped_end - clipped_first), visible_first - pixel_offset, span};
}

static int
extgl_refresh_fb(void *data, const unsigned char *pixels,
	int image_width, int image_height, int xcenter, int ycenter,
	int xzoom, int yzoom, int x, int y, int width, int height)
{
    struct dm *dmp = (struct dm *)data;
    if (!dmp || !pixels || image_width <= 0 || image_height <= 0 ||
	xzoom <= 0 || yzoom <= 0 || extgl_make_current(dmp) != BRLCAD_OK)
	return BRLCAD_ERROR;

    const int viewport_width = dm_get_width(dmp);
    const int viewport_height = dm_get_height(dmp);
    const struct extgl_fb_axis columns = extgl_clip_axis(x, width, image_width,
	    viewport_width, xcenter, xzoom);
    const struct extgl_fb_axis rows = extgl_clip_axis(y, height, image_height,
	    viewport_height, ycenter, yzoom);
    /* A raster position outside the visible image discards the whole blit. */
    if (!columns.count || !rows.count)
	return BRLCAD_OK;

    GLdouble projection[16], modelview[16];
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glPushAttrib(GL_TRANSFORM_BIT | GL_VIEWPORT_BIT | GL_PIXEL_MODE_BIT | GL_CURRENT_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(columns.origin, columns.origin + columns.span,
	rows.origin, rows.origin + rows.span, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glViewport(0, 0, viewport_width, viewport_height);
    glPixelZoom((GLfloat)xzoom, (GLfloat)yzoom);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, image_width);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, columns.first);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, rows.first);
    glRasterPos2i(columns.first, rows.first);
    glDrawPixels(columns.count, rows.count, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glLoadMatrixd(modelview);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(projection);
    glPopClientAttrib();
    glPopAttrib();
    dm_set_dirty(dmp, 1);
    return BRLCAD_OK;
}

static int
extgl_open_fb(struct dm *dmp)
{
    struct fb_platform_specific *fbps =
	fb_get_platform_specific(FB_MEMORY_MAGIC);
    if (!fbps)
	return BRLCAD_ERROR;

    struct fb_mem_platform_data presentation = {dmp, extgl_refresh_fb};
    fbps->data = &presentation;
    dmp->i->fbp = fb_open_existing("mem", dm_get_width(dmp),
	dm_get_height(dmp), fbps);
    fb_put_platform_specific(fbps);
    if (!dmp->i->fbp)
	return BRLCAD_ERROR;
    dmp->i->fbp->i->dmp = dmp;
    return BRLCAD_OK;
}

static int
extgl_get_display_image(struct dm *dmp, unsigned char **image, int flip,
	int alpha)
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    if (!image)
	return BRLCAD_ERROR;
    *image = NULL;
    if (extgl_make_current(dmp) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (vars && vars->host.get_display_image)
	return vars->host.get_display_image(vars->host.data, image, flip, alpha) == 0
	    ? BRLCAD_OK : BRLCAD_ERROR;
    GLint draw_buffer, read_buffer;
    glGetIntegerv(GL_DRAW_BUFFER, &draw_buffer);
    glGetIntegerv(GL_READ_BUFFER, &read_buffer);
    const int width = dm_get_width(dmp);
    const int height = dm_get_height(dmp);
    const size_t channels = alpha ? 4 : 3;
    if (draw_buffer == GL_NONE || width <= 0 || height <= 0 ||
	(size_t)width > SIZE_MAX / (size_t)height / channels)
	return BRLCAD_ERROR;

    *image = (unsigned char *)bu_malloc((size_t)width * height * channels, "extgl image");
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glReadBuffer(draw_buffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glReadPixels(0, 0, width, height, alpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, *image);
    glReadBuffer(read_buffer);
    glPopClientAttrib();
    if (flip)
	flip_display_image_vertically(*image, width, height, alpha);
    return BRLCAD_OK;
}

static int
extgl_draw_string_2d(struct dm *dmp, const char *text, fastf_t x, fastf_t y,
	int size, int use_aspect)
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    if (!vars || !vars->host.draw_string || !text)
	return BRLCAD_ERROR;

    glRasterPos2f((GLfloat)x,
	(GLfloat)(use_aspect ? y * dmp->i->dm_aspect : y));
    int font_size = size > 0 ? size : dm_get_fontsize(dmp);
    return vars->host.draw_string(vars->host.data, text, font_size) == 0
	? BRLCAD_OK : BRLCAD_ERROR;
}

static int
extgl_string_2d_bbox(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax,
	const char *text, fastf_t x, fastf_t y, int size, int use_aspect)
{
    struct extgl_vars *vars =
	(struct extgl_vars *)dmp->i->dm_vars.priv_vars;
    double width = 0.0;
    double height = 0.0;
    if (!vars || !vars->host.string_bbox || !text)
	return BRLCAD_ERROR;

    int font_size = size > 0 ? size : dm_get_fontsize(dmp);
    if (vars->host.string_bbox(vars->host.data, text, font_size, &width,
	    &height) != 0)
	return BRLCAD_ERROR;

    fastf_t adjusted_y = use_aspect ? y * dmp->i->dm_aspect : y;
    fastf_t screen_x = (x + 1.0) * 0.5 * dm_get_width(dmp);
    fastf_t screen_y = (1.0 - adjusted_y) * 0.5 * dm_get_height(dmp);
    if (bmin)
	V2SET(*bmin, screen_x, screen_y - height);
    if (bmax)
	V2SET(*bmax, screen_x + width, screen_y);
    return BRLCAD_OK;
}

static int
extgl_close(struct dm *dmp)
{
    if (!dmp)
	return BRLCAD_OK;
    if (extgl_make_current(dmp) == BRLCAD_OK) {
	GLint matrix_mode;
	glGetIntegerv(GL_MATRIX_MODE, &matrix_mode);
	(void)gl_hud_end(dmp);
	glMatrixMode(matrix_mode);
    }
    if (dm_close_fb(dmp) != BRLCAD_OK)
	return BRLCAD_ERROR;
    (void)gl_put_internal(dmp);
    bu_vls_free(&dmp->i->dm_pathName);
    bu_vls_free(&dmp->i->dm_tkName);
    bu_vls_free(&dmp->i->dm_dName);
    bu_vls_free(&dmp->i->dm_log);
    bu_free(dmp->i->dm_vars.priv_vars, "extgl_close: extgl_vars");
    BU_PUT(dmp->i, struct dm_impl);
    BU_PUT(dmp, struct dm);
    return BRLCAD_OK;
}

static int
extgl_viable(const char *UNUSED(display))
{
    return 1;
}

static int
extgl_doevent(struct dm *UNUSED(dmp), void *UNUSED(client_data),
	void *UNUSED(event))
{
    return BRLCAD_OK;
}

static int
extgl_geometry_request(struct dm *dmp, int UNUSED(width), int UNUSED(height))
{
    return dmp ? BRLCAD_OK : BRLCAD_ERROR;
}

static int
extgl_internal_var(struct bu_vls *UNUSED(result), struct dm *UNUSED(dmp),
	const char *UNUSED(key))
{
    return BRLCAD_ERROR;
}

static int
extgl_write_image(struct bu_vls *UNUSED(messages), FILE *UNUSED(file),
	struct dm *UNUSED(dmp))
{
    return BRLCAD_ERROR;
}

static int
extgl_event_cmp(struct dm *UNUSED(dmp), dm_event_t UNUSED(type),
	int UNUSED(event))
{
    return 0;
}

static struct dm *
extgl_open(void *context, void *UNUSED(interp), int argc, const char **argv)
{
    static int count = 0;
    struct dm_extgl_context *host = (struct dm_extgl_context *)context;
    struct bu_vls init_proc = BU_VLS_INIT_ZERO;
    struct dm *dmp = NULL;
    struct dm_impl *impl = NULL;
    struct gl_vars *glvars = NULL;
    struct extgl_vars *vars = NULL;
    GLfloat background[4] = {0.0, 0.0, 0.0, 0.0};

    if (!host || host->api_version != DM_EXTGL_API || !host->make_current ||
	!host->get_framebuffer_size)
	return DM_NULL;

    BU_GET(dmp, struct dm);
    dmp->magic = DM_MAGIC;
    dmp->start_time = 0;

    BU_GET(impl, struct dm_impl);
    *impl = *dm_extgl.i;
    dmp->i = impl;

    dmp->i->dm_lineWidth = 1;
    dmp->i->dm_bytes_per_pixel = sizeof(GLuint);
    dmp->i->dm_bits_per_channel = 8;
    bu_vls_init(&dmp->i->dm_log);

    BU_ALLOC(vars, struct extgl_vars);
    vars->host = *host;
    dmp->i->dm_vars.priv_vars = vars;
    dmp->i->dm_ctx = host->data;

    dmp->i->dm_get_internal(dmp);
    glvars = (struct gl_vars *)dmp->i->m_vars;
    glvars_init(dmp);
    dmp->i->dm_vp = &glvars->i.default_viewscale;

    bu_vls_init(&dmp->i->dm_pathName);
    bu_vls_init(&dmp->i->dm_tkName);
    bu_vls_init(&dmp->i->dm_dName);

    if (argc > 1 && argv)
	dm_processOptions(dmp, &init_proc, argc - 1, argv + 1);
    bu_vls_free(&init_proc);
    if (bu_vls_strlen(&dmp->i->dm_pathName) == 0)
	bu_vls_printf(&dmp->i->dm_pathName, ".dm_extgl%d", count++);

    dmp->i->dm_aspect = 1.0;
    dmp->i->dm_fontsize = 20;
    glvars->depth = 24;
    glvars->zbuf = 1;
    glvars->rgb = 1;
    glvars->doublebuffer = 1;
    glvars->fastfog = 1;
    glvars->fogdensity = 1.0;
    glvars->lighting_on = 1;
    glvars->zbuffer_on = 1;
    glvars->zclipping_on = 0;
    glvars->bound = 1.0;
    glvars->boundFlag = 1;

    if (extgl_make_current(dmp) != BRLCAD_OK ||
	extgl_configure_win(dmp, 1) != BRLCAD_OK) {
	(void)extgl_close(dmp);
	return DM_NULL;
    }

    gl_setBGColor(dmp, 0, 0, 0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLineStipple(1, 0xCF33);
    glDisable(GL_LINE_STIPPLE);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.0);
    glFogf(GL_FOG_END, 2.0);
    glFogfv(GL_FOG_COLOR, background);
    glFogf(GL_FOG_DENSITY, VIEWFACTOR);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-glvars->i.xlim_view, glvars->i.xlim_view,
	-glvars->i.ylim_view, glvars->i.ylim_view, 0.0, 2.0);
    glGetDoublev(GL_PROJECTION_MATRIX, glvars->i.faceplate_mat);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glPushMatrix();
    glLoadIdentity();
    glvars->i.faceFlag = 1;
    gl_setZBuffer(dmp, glvars->zbuffer_on);
    gl_setLight(dmp, glvars->lighting_on);

    return dmp;
}

struct dm_impl dm_extgl_impl = {
    extgl_open,
    extgl_close,
    extgl_viable,
    gl_drawBegin,
    gl_drawEnd,
    gl_hud_begin,
    gl_hud_end,
    gl_loadMatrix,
    gl_loadPMatrix,
    gl_popPMatrix,
    extgl_draw_string_2d,
    extgl_string_2d_bbox,
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
    extgl_configure_win,
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
    extgl_get_display_image,
    gl_reshape,
    extgl_make_current,
    extgl_present,
    extgl_doevent,
    extgl_open_fb,
    gl_get_internal,
    gl_put_internal,
    extgl_geometry_request,
    extgl_internal_var,
    extgl_write_image,
    NULL,
    NULL,
    extgl_event_cmp,
    gl_fogHint,
    NULL,
    0,
    1,
    "OpenGL",
    1,
    0,
    "extgl",
    "External OpenGL context",
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1.0,
    0,
    {0, 0},
    NULL,
    NULL,
    BU_VLS_INIT_ZERO,
    BU_VLS_INIT_ZERO,
    BU_VLS_INIT_ZERO,
    BU_VLS_INIT_ZERO,
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {BV_MIN, BV_MIN, BV_MIN},
    {BV_MAX, BV_MAX, BV_MAX},
    0,
    0,
    1,
    0,
    0,
    gl_vparse,
    FB_NULL,
    0,
    NULL,
    NULL,
    NULL,
    NULL
};

struct dm dm_extgl = { DM_MAGIC, &dm_extgl_impl, 0 };

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
