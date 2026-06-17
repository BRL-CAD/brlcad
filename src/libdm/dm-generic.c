/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2026 United States Government as represented by
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
/** @file libdm/dm-generic.c
 *
 * Generic display manager routines.
 *
 */

#include "common.h"

#include <string.h>

#include "vmath.h"
#include "bu/hash.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bsg/defines.h"
#include "bsg/plot3.h"
#include "bsg/render_item.h"
#include "bsg/backend_adapter.h"
#include "dm.h"
#include "dm/vlist.h"
#include "./include/private.h"
#include "./null/dm-Null.h"

void *
dm_interp(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return (void *)dmp->i->dm_interp;
}

void *
dm_get_ctx(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return (void *)dmp->i->dm_ctx;
}

void *
dm_get_udata(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return (void *)dmp->i->dm_udata;
}

void
dm_set_udata(struct dm *dmp, void *udata)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_udata = udata;
}


/* --- Backend-ops accessors / dispatch wrappers --- */

const struct dm_backend_ops *
dm_get_backend_ops(struct dm *dmp)
{
    if (UNLIKELY(!dmp || !dmp->i)) return NULL;
    return dmp->i->dm_backend_ops;
}

void
dm_set_backend_ops(struct dm *dmp, const struct dm_backend_ops *ops)
{
    if (UNLIKELY(!dmp || !dmp->i)) return;
    dmp->i->dm_backend_ops = ops;
}

int
dm_backend_begin_frame(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return -1;
    dmp->i->dm_backend_generation++;
    const struct dm_backend_ops *ops = dm_get_backend_ops(dmp);
    if (ops && ops->begin_frame)
	return ops->begin_frame(dmp);
    return 0;
}

void
dm_backend_end_frame(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return;
    const struct dm_backend_ops *ops = dm_get_backend_ops(dmp);
    if (ops && ops->end_frame)
	ops->end_frame(dmp);
}

int
dm_backend_draw_item(struct dm *dmp, const struct bsg_render_item *item)
{
    if (UNLIKELY(!dmp || !item || item->geometry.kind == BSG_RENDER_GEOMETRY_NONE)) return -1;
    const struct dm_backend_ops *ops = dm_get_backend_ops(dmp);
    if (ops && ops->draw_item)
	return ops->draw_item(dmp, item);
    return -1;
}

void
dm_backend_invalidate_item(struct dm *dmp, const struct bsg_render_item *item, unsigned int reason_mask)
{
    if (UNLIKELY(!dmp || !item)) return;
    const struct dm_backend_ops *ops = dm_get_backend_ops(dmp);
    if (ops && ops->invalidate_item)
	ops->invalidate_item(dmp, item, reason_mask);
    else
	dm_backend_resource_invalidate(dmp,
		item->source.source_id ? item->source.source_id : item->geometry.source_identity);
}

void
dm_backend_release_source(struct dm *dmp, uint64_t source_identity)
{
    if (UNLIKELY(!dmp || !source_identity)) return;
    const struct dm_backend_ops *ops = dm_get_backend_ops(dmp);
    if (ops && ops->release_source)
	ops->release_source(dmp, source_identity);
    else
	dm_backend_resource_release_source(dmp, source_identity);
}

/* --- end backend-ops API --- */

static void
_dm_backend_resource_free(struct dm *dmp, struct dm_backend_resource *r)
{
    if (!r)
	return;
    if (r->free_resource)
	r->free_resource(dmp, r);
    BU_PUT(r, struct dm_backend_resource);
}

struct dm_backend_resource *
dm_backend_resource_get(struct dm *dmp,
			uint64_t cache_identity,
			uint64_t source_identity,
			uint64_t backend_identity,
			int create,
			dm_backend_resource_free_fn free_resource)
{
    if (UNLIKELY(!dmp) || !cache_identity)
	return NULL;
    struct dm_backend_resource **prev = &dmp->i->dm_backend_resources;
    struct dm_backend_resource *r = dmp->i->dm_backend_resources;
    while (r) {
	if (r->cache_identity == cache_identity &&
		r->source_identity == source_identity &&
		r->backend_identity == backend_identity) {
	    if (!r->stale)
		return r;
	    if (!create)
		return NULL;
	    *prev = r->next;
	    _dm_backend_resource_free(dmp, r);
	    break;
	}
	prev = &r->next;
	r = r->next;
    }
    if (!create)
	return NULL;

    BU_GET(r, struct dm_backend_resource);
    memset(r, 0, sizeof(*r));
    r->cache_identity = cache_identity;
    r->source_identity = source_identity;
    r->backend_identity = backend_identity;
    r->free_resource = free_resource;
    r->last_seen_generation = dmp->i->dm_backend_generation;
    r->next = dmp->i->dm_backend_resources;
    dmp->i->dm_backend_resources = r;
    return r;
}

void
dm_backend_resource_touch(struct dm *dmp, struct dm_backend_resource *resource)
{
    if (UNLIKELY(!dmp) || !resource)
	return;
    resource->last_seen_generation = dmp->i->dm_backend_generation;
}

void
dm_backend_resource_invalidate(struct dm *dmp, uint64_t source_identity)
{
    if (UNLIKELY(!dmp) || !source_identity)
	return;
    for (struct dm_backend_resource *r = dmp->i->dm_backend_resources; r; r = r->next) {
	if (r->source_identity == source_identity)
	    r->stale = 1;
    }
}

void
dm_backend_resource_release_source(struct dm *dmp, uint64_t source_identity)
{
    if (UNLIKELY(!dmp) || !source_identity)
	return;
    struct dm_backend_resource **prev = &dmp->i->dm_backend_resources;
    struct dm_backend_resource *r = dmp->i->dm_backend_resources;
    while (r) {
	struct dm_backend_resource *next = r->next;
	if (r->source_identity == source_identity) {
	    *prev = next;
	    _dm_backend_resource_free(dmp, r);
	} else {
	    prev = &r->next;
	}
	r = next;
    }
}

void
dm_backend_resource_reclaim_unseen(struct dm *dmp)
{
    if (UNLIKELY(!dmp))
	return;
    uint64_t gen = dmp->i->dm_backend_generation;
    struct dm_backend_resource **prev = &dmp->i->dm_backend_resources;
    struct dm_backend_resource *r = dmp->i->dm_backend_resources;
    while (r) {
	struct dm_backend_resource *next = r->next;
	if (r->last_seen_generation != gen) {
	    *prev = next;
	    _dm_backend_resource_free(dmp, r);
	} else {
	    prev = &r->next;
	}
	r = next;
    }
}


void
dm_fogHint(struct dm *dmp, int fastfog)
{
    if (UNLIKELY(!dmp)) {
	bu_log("WARNING: NULL display (fastfog => %d)\n", fastfog);
	return;
    }

    if (dmp->i->dm_fogHint) {
	dmp->i->dm_fogHint(dmp, fastfog);
    }
}

int
dm_write_image(struct bu_vls *msgs, FILE *fp, struct dm *dmp)
{
    if (!dmp || !dmp->i->dm_write_image) return -1;
    return dmp->i->dm_write_image(msgs, fp, dmp);
}

/* Properly generic functions */

void
dm_geometry_request(struct dm *dmp, int width, int height)
{
    if (!dmp || !dmp->i->dm_geometry_request) return;
    (void)dmp->i->dm_geometry_request(dmp, width, height);
}

void
dm_internal_var(struct bu_vls *result, struct dm *dmp, const char *key)
{
    if (!dmp || !result || !dmp->i->dm_internal_var) return;
    (void)dmp->i->dm_internal_var(result, dmp, key);
}

int
dm_event_cmp(struct dm *dmp, dm_event_t type, int event)
{
    if (!dmp || !dmp->i->dm_event_cmp) return -1;
    return dmp->i->dm_event_cmp(dmp, type, event);
}

fastf_t
dm_Xx2Normal(struct dm *dmp, int x)
{
    if (UNLIKELY(!dmp)) return 0.0;
    return ((x / (fastf_t)dmp->i->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(struct dm *dmp, fastf_t f)
{
    if (UNLIKELY(!dmp)) return 0.0;
    return (f * 0.5 + 0.5) * dmp->i->dm_width;
}

fastf_t
dm_Xy2Normal(struct dm *dmp, int y, int use_aspect)
{
    if (UNLIKELY(!dmp)) return 0.0;
    if (use_aspect)
	return ((0.5 - y / (fastf_t)dmp->i->dm_height) / dmp->i->dm_aspect * 2.0);
    else
	return ((0.5 - y / (fastf_t)dmp->i->dm_height) * 2.0);
}

int
dm_Normal2Xy(struct dm *dmp, fastf_t f, int use_aspect)
{
    if (UNLIKELY(!dmp)) return 0.0;
    if (use_aspect)
	return (0.5 - f * 0.5 * dmp->i->dm_aspect) * dmp->i->dm_height;
    else
	return (0.5 - f * 0.5) * dmp->i->dm_height;
}

struct fb *
dm_get_fb(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    if (dmp->i->fbp == FB_NULL)
	dmp->i->dm_openFb(dmp);
    return dmp->i->fbp;
}

const char *
dm_get_dm_name(const struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return "(DM-NULL)";
    return dmp->i->dm_name;
}

const char *
dm_get_dm_lname(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return "(DM-NULL)";
    return dmp->i->dm_lname;
}

int
dm_get_width(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_width;
}

int
dm_get_height(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_height;
}

void
dm_set_width(struct dm *dmp, int width)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_width = width;
}

void
dm_set_height(struct dm *dmp, int height)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_height = height;
}

int
dm_graphical(const struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_graphical;
}

const char *
dm_get_type(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return "(DM-NULL)";
    return dmp->i->dm_name;
}

int
dm_get_backend_cache(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_backend_cache;
}

fastf_t
dm_get_aspect(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_aspect;
}

int
dm_get_fontsize(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_fontsize;
}

void
dm_set_fontsize(struct dm *dmp, int size)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_fontsize = size;
}

int
dm_close(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    while (dmp->i->dm_backend_resources) {
	struct dm_backend_resource *r = dmp->i->dm_backend_resources;
	dmp->i->dm_backend_resources = r->next;
	_dm_backend_resource_free(dmp, r);
    }
    return dmp->i->dm_close(dmp);
}

int
dm_get_bg(unsigned char **bg1, unsigned char **bg2, struct dm *dmp)
{
    static unsigned char dbg1[3] = {0, 0, 0};
    static unsigned char dbg2[3] = {0, 0, 0};
    if (UNLIKELY(!dmp)) {
	if (bg1)
	    (*bg1) = (unsigned char *)dbg1;
	if (bg2)
	    (*bg2) = (unsigned char *)dbg2;
	return 0;
    }
    if (bg1)
	(*bg1) = (unsigned char *)dmp->i->dm_bg1;
    if (bg2)
	(*bg2) = (unsigned char *)dmp->i->dm_bg2;
    if (dmp->i->dm_bg1[0] != dmp->i->dm_bg2[0])
	return 1;
    if (dmp->i->dm_bg1[1] != dmp->i->dm_bg2[1])
	return 1;
    if (dmp->i->dm_bg1[2] != dmp->i->dm_bg2[2])
	return 1;
    return 0;
}

int
dm_set_bg(struct dm *dmp,
	unsigned char r1, unsigned char g1, unsigned char b1,
	unsigned char r2, unsigned char g2, unsigned char b2
	)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setBGColor(dmp, r1, g1, b1, r2, g2, b2);
}

unsigned char *
dm_get_fg(struct dm *dmp)
{
    static unsigned char dfg[3] = {0, 0, 0};
    if (UNLIKELY(!dmp)) return dfg;
    return dmp->i->dm_fg;
}

int
dm_set_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setFGColor(dmp, r, g, b, strict, transparency);
}

unsigned char *
dm_get_geometry_default_color(struct dm *dmp)
{
    static unsigned char dgc[3] = {255, 0, 0};
    if (UNLIKELY(!dmp)) return dgc;
    return dmp->i->dm_geometry_default_color;
}

void
dm_set_geometry_default_color(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_geometry_default_color[0] = r;
    dmp->i->dm_geometry_default_color[1] = g;
    dmp->i->dm_geometry_default_color[2] = b;
}

int
dm_reshape(struct dm *dmp, int width, int height)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_reshape(dmp, width, height);
}

int
dm_make_current(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_makeCurrent(dmp);
}

int
dm_doevent(struct dm *dmp, void *clientData, void *eventPtr)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_doevent(dmp, clientData, eventPtr);
}

int
dm_get_native_repaint_pending(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_native_repaint_pending;
}

void
dm_set_native_repaint_pending(struct dm *dmp, int i)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_native_repaint_pending = i;
}

vect_t *
dm_get_clipmin(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return  &(dmp->i->dm_clipmin);
}


vect_t *
dm_get_clipmax(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return  &(dmp->i->dm_clipmax);
}

int
dm_get_bound_flag(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getBoundFlag(dmp);
}

void
dm_set_bound_flag(struct dm *dmp, int boundf)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_setBoundFlag(dmp, boundf);
}

fastf_t
dm_get_bound(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getBound(dmp);
}

void
dm_set_bound(struct dm *dmp, fastf_t val)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_setBound(dmp, val);
}

int
dm_set_win_bounds(struct dm *dmp, fastf_t *w)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setWinBounds(dmp, w);
}

int
dm_get_stereo(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_stereo;
}
int
dm_configure_win(struct dm *dmp, int force)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_configureWin(dmp, force);
}

struct bu_vls *
dm_get_pathname(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    return &(dmp->i->dm_pathName);
}

void
dm_set_pathname(struct dm *dmp, const char *pname)
{
    if (UNLIKELY(!dmp)) return;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    bu_vls_sprintf(&(dmp->i->dm_pathName), "%s", pname);
}

const char *
dm_get_name(const struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->i->dm_name;
}

struct bu_vls *
dm_get_dname(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    return &(dmp->i->dm_dName);
}

const char *
dm_get_graphics_system(const struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->i->graphics_system;
}

struct bu_vls *
dm_get_tkname(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    return &(dmp->i->dm_tkName);
}

unsigned long
dm_get_id(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_id;
}

void
dm_set_id(struct dm *dmp, unsigned long new_id)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_id = new_id;
}

int
dm_get_light(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getLight(dmp);
}

int
dm_set_light(struct dm *dmp, int light)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setLight(dmp, light);
}

int
dm_get_transparency(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getTransparency(dmp);
}

int
dm_set_transparency(struct dm *dmp, int transparency)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setTransparency(dmp, transparency);
}

int
dm_get_zbuffer(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getZBuffer(dmp);
}

int
dm_set_zbuffer(struct dm *dmp, int zbuffer)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setZBuffer(dmp, zbuffer);
}

int
dm_get_linewidth(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_lineWidth;
}

void
dm_set_linewidth(struct dm *dmp, int linewidth)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_lineWidth = linewidth;
}

int
dm_get_linestyle(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_lineStyle;
}

void
dm_set_linestyle(struct dm *dmp, int linestyle)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_lineStyle = linestyle;
}

int
dm_set_line_attr(struct dm *dmp, int width, int style)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setLineAttr(dmp, width, style);
}


int
dm_get_zclip(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_getZClip(dmp);
}

void
dm_set_zclip(struct dm *dmp, int zclip)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_setZClip(dmp, zclip);
}

int
dm_get_perspective(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_perspective;
}

void
dm_set_perspective(struct dm *dmp, fastf_t perspective)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_perspective = perspective;
}

int
dm_get_display_image(struct dm *dmp, unsigned char **image, int flip, int alpha)
{
    if (!dmp || !image) return 0;
    return dmp->i->dm_getDisplayImage(dmp, image, flip, alpha);
}

int
dm_draw_device_vlist(struct dm *dmp, bsg_vlist *vp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawVList(dmp, vp);
}

int
dm_draw_device_vlist_hidden_line(struct dm *dmp, bsg_vlist *vp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawVListHiddenLine(dmp, vp);
}

int
dm_draw_begin(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    dmp->start_time = bu_gettime();
    return dmp->i->dm_drawBegin(dmp);
}
int
dm_draw_end(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawEnd(dmp);
}
int
dm_hud_begin(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_hud_begin(dmp);
}
int
dm_hud_end(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_hud_end(dmp);
}
int
dm_loadmatrix(struct dm *dmp, fastf_t *mat, int eye)
{
    if (!dmp || !mat) return 0;
    return dmp->i->dm_loadMatrix(dmp, mat, eye);
}
int
dm_loadpmatrix(struct dm *dmp, const fastf_t *mat)
{
    if (!dmp) return 0;
    return dmp->i->dm_loadPMatrix(dmp, mat);
}
void
dm_pop_pmatrix(struct dm *dmp)
{
    if (dmp)
	dmp->i->dm_popPMatrix(dmp);
}
int
dm_draw_string_2d(struct dm *dmp, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect)
{
    if (!dmp || !str) return 0;
    return dmp->i->dm_drawString2D(dmp, str, x, y, size, use_aspect);
}

static void
dm_stroke_text_point(fastf_t base_x, fastf_t base_y,
		     fastf_t px, fastf_t py,
		     fastf_t cang, fastf_t sang,
		     int width, int height,
		     fastf_t *out_x, fastf_t *out_y)
{
    fastf_t rx = cang * px - sang * py;
    fastf_t ry = sang * px + cang * py;

    *out_x = base_x + 2.0 * rx / (fastf_t)width;
    *out_y = base_y + 2.0 * ry / (fastf_t)height;
}

static int
dm_draw_string_2d_rot_stroke(struct dm *dmp, const char *str,
			     fastf_t x, fastf_t y,
			     int size, int use_aspect, fastf_t angle)
{
    if (!dmp || !dmp->i || !dmp->i->dm_drawLine2D || !str)
	return BRLCAD_ERROR;

    int width = dmp->i->dm_width;
    int height = dmp->i->dm_height;
    if (width <= 0 || height <= 0)
	return BRLCAD_ERROR;

    if (size <= 0)
	size = dmp->i->dm_fontsize;
    if (size <= 0)
	size = 10;

    fastf_t aspect = (dmp->i->dm_aspect > SMALL_FASTF) ?
	dmp->i->dm_aspect : 1.0;
    fastf_t base_y = use_aspect ? y * aspect : y;
    fastf_t rad = angle * M_PI / 180.0;
    fastf_t cang = cos(rad);
    fastf_t sang = sin(rad);
    fastf_t scale = (fastf_t)size;
    fastf_t offset = 0.0;
    int ret = BRLCAD_OK;

    for (const unsigned char *cp = (const unsigned char *)str; *cp; cp++, offset += scale) {
	int *p = plot3_font_getchar(cp);
	fastf_t pen_x = offset;
	fastf_t pen_y = 0.0;

	for (int stroke = *p; stroke != PLOT3_FONT_LAST; stroke = *++p) {
	    int ysign = 1;
	    int draw = 1;

	    if (stroke == PLOT3_FONT_NEGY) {
		ysign = -1;
		stroke = *++p;
	    }
	    if (stroke < 0) {
		stroke = -stroke;
		draw = 0;
	    }

	    fastf_t px = ((fastf_t)(stroke / 11) * 0.1 * scale) + offset;
	    fastf_t py = (fastf_t)ysign * (fastf_t)(stroke % 11) * 0.1 * scale;
	    if (draw) {
		fastf_t x1, y1, x2, y2;
		dm_stroke_text_point(x, base_y, pen_x, pen_y, cang, sang,
			width, height, &x1, &y1);
		dm_stroke_text_point(x, base_y, px, py, cang, sang,
			width, height, &x2, &y2);
		if (dmp->i->dm_drawLine2D(dmp, x1, y1, x2, y2) != BRLCAD_OK)
		    ret = BRLCAD_ERROR;
	    }
	    pen_x = px;
	    pen_y = py;
	}
    }

    return ret;
}

int
dm_draw_string_2d_rot(struct dm *dmp, const char *str, fastf_t x, fastf_t y, int size, int use_aspect, fastf_t angle)
{
    if (!dmp || !str) return 0;
    if (dmp->i->dm_drawString2DRot)
	return dmp->i->dm_drawString2DRot(dmp, str, x, y, size, use_aspect, angle);
    if (!NEAR_ZERO(angle, SMALL_FASTF) &&
	    dm_draw_string_2d_rot_stroke(dmp, str, x, y, size, use_aspect, angle) == BRLCAD_OK)
	return BRLCAD_OK;
    return dmp->i->dm_drawString2D(dmp, str, x, y, size, use_aspect);
}
int
dm_string_bbox_2d(struct dm *dmp, vect2d_t *bmin, vect2d_t *bmax, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect)
{
    if (!dmp || !str) return 0;
    return dmp->i->dm_String2DBBox(dmp, bmin, bmax, str, x, y, size, use_aspect);
}
int
dm_draw_line_2d(struct dm *dmp, fastf_t x1, fastf_t y1_2d, fastf_t x2, fastf_t y2)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawLine2D(dmp, x1, y1_2d, x2, y2);
}
int
dm_draw_line_3d(struct dm *dmp, point_t pt1, point_t pt2)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawLine3D(dmp, pt1, pt2);
}
int
dm_draw_lines_3d(struct dm *dmp, int npoints, point_t *points, int sflag)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawLines3D(dmp, npoints, points, sflag);
}
int
dm_draw_point_2d(struct dm *dmp, fastf_t x, fastf_t y)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawPoint2D(dmp, x, y);
}
int
dm_draw_point_3d(struct dm *dmp, point_t pt)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawPoint3D(dmp, pt);
}
int
dm_draw_points_3d(struct dm *dmp, int npoints, point_t *points)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_drawPoints3D(dmp, npoints, points);
}
int
dm_draw(struct dm *dmp, bsg_vlist *(*callback)(void *), void **data)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_draw(dmp, callback, data);
}
int
dm_set_depth_mask(struct dm *dmp, int d_on)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_setDepthMask(dmp, d_on);
}
int
dm_set_debug(struct dm *dmp, int lvl)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_debug(dmp, lvl);
}
int
dm_get_debug(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->i->dm_debugLevel;
}
int
dm_logfile(struct dm *dmp, const char *filename)
{
    if (!dmp || !filename) return 0;
    return dmp->i->dm_logfile(dmp, filename);
}

void
dm_set_vp(struct dm *dmp, fastf_t *vp)
{
    if (UNLIKELY(!dmp)) return;
    dmp->i->dm_vp = vp;
}

/* This is the generic "catch-all" hook that is used
 * to run any user supplied callbacks.  If more side
 * effects are needed at the libdm level, a task-specific
 * hook should be defined using this function as a
 * starting point and adding in whatever additional
 * logic is needed.  The full dm structure should always
 * be accessible as a slot in the modifiable variables
 * structure passed in here as "base" */
void
dm_generic_hook(const struct bu_structparse *sdp,
		const char *name, void *base, const char *value, void *data)
{
    if (data) {
	struct dm_hook_data *hook= (struct dm_hook_data *)data;

	/* Call hook function(if it exists) to carry out the
	 * application requested logic */
	if (hook && hook->dm_hook)
	    hook->dm_hook(sdp, name, base, value, hook->dmh_data);
    }
}

int
dm_set_hook(const struct bu_structparse_map *map,
	    const char *key, void *data, struct dm_hook_data *hook)
{
    if (UNLIKELY(!map || !key || !hook)) return -1;
    hook->dm_hook = BU_STRUCTPARSE_FUNC_NULL;
    hook->dmh_data = NULL;
    for (; map->sp_name != (char *)0; map++) {
	if (BU_STR_EQUAL(map->sp_name, key)) {
	    hook->dm_hook = map->sp_hook;
	    if (data)
		hook->dmh_data = data;
	    return 0;
	}
    }
    return 1;
}

struct bu_structparse *
dm_get_vparse(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    return dmp->i->vparse;
}

void *
dm_get_mvars(struct dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    BU_CKMAG(dmp, DM_MAGIC, "dm internal");
    if (!dmp->i->m_vars) return (void *)dmp;
    return dmp->i->m_vars;
}

void
_bu_structparse_hash(struct bu_data_hash_state *state, struct bu_structparse *parsetab, const char *base)
{
    const struct bu_structparse *sdp;
    char *loc;
    int lastoff = -1;
    for (sdp = parsetab; sdp->sp_name != (char *)0; sdp++) {
	/* Skip alternate keywords for same value */
	if (lastoff == (int)sdp->sp_offset)
	    continue;
	lastoff = (int)sdp->sp_offset;
	loc = (char *)(base + sdp->sp_offset);

	switch (sdp->sp_fmt[1]) {
	    case 'c':
	    case 's':
		if (sdp->sp_count == 1) {
		    if (*loc != '\0')
			bu_data_hash_update(state, loc, strlen((char *)loc));
		} else {
		    bu_data_hash_update(state, loc, strlen((char *)loc));
		}
		break;
	    case 'V':
		{
		    struct bu_vls *vls = (struct bu_vls *)loc;
		    bu_data_hash_update(state, bu_vls_cstr(vls), bu_vls_strlen(vls));
		}
		break;
	    case 'i':
		{
		    register size_t i = sdp->sp_count;
		    register short *sp = (short *)loc;
		    while (--i > 0)
			bu_data_hash_update(state, &(*sp++), sizeof(short));
		}
		break;
	    case 'd':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;
		    while (--i > 0)
			bu_data_hash_update(state, &(*dp++), sizeof(int));
		}
		break;
	    case 'f':
		{
		    register size_t i = sdp->sp_count;
		    register fastf_t *dp = (fastf_t *)loc;
		    while (--i > 0)
			bu_data_hash_update(state, &(*dp++), sizeof(fastf_t));
		}
		break;
	    case 'g':
		{
		    register size_t i = sdp->sp_count;
		    register double *dp = (double *)loc;
		    while (--i > 0) {
			bu_data_hash_update(state, &(*dp++), sizeof(fastf_t));
		    }
		    break;
		}
	    case 'x':
		{
		    register size_t i = sdp->sp_count;
		    register int *dp = (int *)loc;
		    while (--i > 0)
			bu_data_hash_update(state, &(*dp++), sizeof(int));
		}
		break;
	    case 'p':
		BU_ASSERT(sdp->sp_count == 1);
		_bu_structparse_hash(state, (struct bu_structparse *)sdp->sp_offset, base);
		break;
	    default:
		break;
	}
    }
}

unsigned long long
dm_hash(struct dm *dmp)
{
    if (!dmp)
	return 0;

    struct bu_data_hash_state *state = bu_data_hash_create();
    if (!state)
	return 0;

    // Note:  deliberately not checking names - a rename doesn't change the dm.
    // Also deliberately not checking dirty flag - that's usually what we're
    // using this hash to set or not set.
    bu_data_hash_update(state, &dmp->i->dm_stereo, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_width, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_height, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_lineWidth, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_lineStyle, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_aspect, sizeof(fastf_t));
    bu_data_hash_update(state, &dmp->i->dm_bg1, sizeof(unsigned char[3]));
    bu_data_hash_update(state, &dmp->i->dm_bg2, sizeof(unsigned char[3]));
    bu_data_hash_update(state, &dmp->i->dm_fg, sizeof(unsigned char[3]));
    bu_data_hash_update(state, &dmp->i->dm_clipmin, sizeof(vect_t));
    bu_data_hash_update(state, &dmp->i->dm_clipmax, sizeof(vect_t));
    bu_data_hash_update(state, &dmp->i->dm_perspective, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_depthMask, sizeof(int));
    bu_data_hash_update(state, &dmp->i->dm_fontsize, sizeof(int));

    if (dmp->i->fbp) {
	// TODO - check for framebuffer changes as well...
    }

    // Also check for changes in backend specific values by iterating
    // using the structparse.
    struct bu_structparse *pt = dm_get_vparse(dmp);
    void *mvars = dm_get_mvars(dmp);
    if (pt && mvars) {
	_bu_structparse_hash(state, pt, (const char *)mvars);
    }

    unsigned long long hash_val = bu_data_hash_val(state);
    bu_data_hash_destroy(state);

    return hash_val;
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
