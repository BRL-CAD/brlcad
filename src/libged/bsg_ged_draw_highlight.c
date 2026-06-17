/*                B S G _ G E D _ D R A W _ H I G H L I G H T . C
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
/** @file libged/b_s_g___g_e_d___d_r_a_w___h_i_g_h_l_i_g_h_t_._c.c
 *
 * Highlighted-shape tracking and highlight-state mutation bridge.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bsg/appearance.h"
#include "bsg/defines.h"
#include "bsg/database_source.h"
#include "bsg/draw_ctx.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_set.h"
#include "bsg/draw_source.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "bsg/lod.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/payload.h"
#include "bsg/plot3.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg/selection.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"
#include "bsg/util.h"
#include "bg/clip.h"

#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"

struct ged_draw_highlight_path_prefix_ctx {
    struct ged *gedp;
    const struct db_full_path *path;
    size_t prefix_len;
    int highlighted;
    int matched;
};


struct _highlight_by_name_ctx {
    struct ged          *gedp;
    const char          *name;
    ged_draw_shape_ref   result;
};


struct _iflag_shape_ctx {
    struct ged *gedp;
    int iflag;
};


static int
_iflag_shape_record_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct _iflag_shape_ctx *ctx = (struct _iflag_shape_ctx *)ud;
    bsg_scene_ref shape_ref = ctx ?
	ged_draw_registry_shape_scene_ref(ctx->gedp, rec->ref) :
	bsg_scene_ref_null();
    if (!bsg_scene_ref_is_null(shape_ref))
	bsg_scene_set_highlighted(shape_ref, (ctx->iflag == UP) ? 1 : 0);
    return 1;
}


static void
_sg_set_iflag(struct ged *gedp, int iflag)
{
    struct _iflag_shape_ctx ctx;
    ctx.gedp = gedp;
    ctx.iflag = iflag;
    ged_draw_foreach_shape_record(gedp, _iflag_shape_record_cb, &ctx);
}


/* ------------------------------------------------------------------ */
/* highlighted draw-ref tracker                                       */
/* ------------------------------------------------------------------ */
static ged_draw_shape_ref
_sg_highlighted_shape_ref(const struct ged_drawable *gdp)
{
    ged_draw_shape_ref ref = GED_DRAW_SHAPE_REF_NULL;
    if (!gdp || !gdp->gd_highlight_token)
	return ref;
    ref.token = gdp->gd_highlight_token;
    ref.scene_revision = gdp->gd_highlight_scene_rev;
    return ref;
}


static bsg_scene_ref
_sg_highlighted_shape_scene_ref(const struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return bsg_scene_ref_null();
    return ged_draw_registry_shape_scene_ref((struct ged *)gedp,
	    _sg_highlighted_shape_ref(gedp->i->ged_gdp));
}


void
ged_draw_highlighted_shape_ref_invalidate(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return;
    struct ged_drawable *gdp = gedp->i->ged_gdp;
    if (!gdp->gd_highlight_token && !gdp->gd_highlight_scene_rev)
	return;
    gdp->gd_highlight_token = 0;
    gdp->gd_highlight_scene_rev = 0;
    gdp->gd_highlight_rev++;
}


/*
 * Register @p ref as the single currently highlighted shape.
 * Clears the highlight state of any previously registered shape first.
 * Passing a null ref clears the previously registered shape and deregisters
 * the single-highlight ref.  Operations that set multiple highlighted records
 * use ged_draw_highlighted_shape_ref_invalidate() instead so their highlighted
 * records are not undone while the O(N) clear fallback remains safe.
 *
 * Highlight identity is a draw ref.  Node highlight bits are updated here
 * only as a derived compatibility surface for older callers that still ask
 * appearance directly.
 */
static void
_sg_set_highlighted_shape_ref(struct ged *gedp, ged_draw_shape_ref ref)
{
    struct ged_drawable *gdp = gedp->i->ged_gdp;
    ged_draw_shape_ref old_ref = _sg_highlighted_shape_ref(gdp);
    bsg_scene_ref old = ged_draw_registry_shape_scene_ref(gedp, old_ref);
    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);

    if (old_ref.token == ref.token &&
	    old_ref.scene_revision == ref.scene_revision) {
	return;
    }

    if (!bsg_scene_ref_is_null(old))
        bsg_scene_set_highlighted(old, 0);
    if (!bsg_scene_ref_is_null(shape_ref))
        bsg_scene_set_highlighted(shape_ref, 1);

    gdp->gd_highlight_token = bsg_scene_ref_is_null(shape_ref) ? 0 : ref.token;
    gdp->gd_highlight_scene_rev =
	bsg_scene_ref_is_null(shape_ref) ? 0 : ref.scene_revision;

    /* Every transition is itself a highlight-state change. */
    gdp->gd_highlight_rev++;
}


void
ged_draw_set_highlight_state(struct ged *gedp, int highlighted)
{
    if (!gedp)
        return;
    int iflag = highlighted ? UP : DOWN;
    if (iflag == DOWN) {
        /* Fast path: when exactly one shape is highlighted and tracked,
         * clear only that shape in O(1) rather than sweeping the whole tree. */
        bsg_scene_ref illum = _sg_highlighted_shape_scene_ref(gedp);
        if (!bsg_scene_ref_is_null(illum)) {
            bsg_scene_set_highlighted(illum, 0);
	    _sg_set_highlighted_shape_ref(gedp, GED_DRAW_SHAPE_REF_NULL);
            return;
        }
    }
    _sg_set_iflag(gedp, iflag);
}


static int
_ged_draw_highlight_path_prefix_cb(const struct ged_draw_shape_record *rec,
				   void *userdata)
{
    struct ged_draw_highlight_path_prefix_ctx *ctx =
	(struct ged_draw_highlight_path_prefix_ctx *)userdata;
    if (!ctx || !rec || !rec->fullpath ||
	    rec->fullpath->fp_len < ctx->prefix_len)
	return 1;

    for (size_t i = 0; i < ctx->prefix_len; i++) {
	if (DB_FULL_PATH_GET(rec->fullpath, i) !=
		DB_FULL_PATH_GET(ctx->path, i))
	    return 1;
    }

    bsg_scene_ref shape_ref =
	ged_draw_registry_shape_scene_ref(ctx->gedp, rec->ref);
    if (!bsg_scene_ref_is_null(shape_ref)) {
	bsg_scene_set_highlighted(shape_ref, ctx->highlighted ? 1 : 0);
	ctx->matched++;
    }
    return 1;
}


int
ged_draw_set_highlighted_path_prefix(struct ged *gedp,
				     const struct db_full_path *path,
				     size_t path_pos,
				     int highlighted)
{
    if (!gedp || !path || path->fp_len <= 0)
	return 0;

    struct ged_draw_highlight_path_prefix_ctx ctx;
    ctx.gedp = gedp;
    ctx.path = path;
    ctx.prefix_len = path_pos + 1;
    if (ctx.prefix_len > path->fp_len)
	ctx.prefix_len = path->fp_len;
    ctx.highlighted = highlighted ? 1 : 0;
    ctx.matched = 0;

    ged_draw_foreach_shape_record(gedp, _ged_draw_highlight_path_prefix_cb, &ctx);
    ged_draw_highlighted_shape_ref_invalidate(gedp);
    return ctx.matched;
}


void
ged_draw_set_highlighted_shape_ref(struct ged *gedp, ged_draw_shape_ref ref)
{
    if (!gedp)
	return;
    _sg_set_highlighted_shape_ref(gedp, ref);
}


int
ged_draw_shape_set_highlighted(struct ged *gedp, ged_draw_shape_ref ref, int highlighted)
{
    if (!gedp || ged_draw_shape_ref_is_null(ref))
	return 0;

    bsg_scene_ref shape_ref = ged_draw_registry_shape_scene_ref(gedp, ref);
    if (bsg_scene_ref_is_null(shape_ref))
	return 0;

    bsg_scene_set_highlighted(shape_ref, highlighted ? 1 : 0);
    if (!highlighted) {
	struct ged_drawable *gdp = gedp->i ? gedp->i->ged_gdp : NULL;
	if (gdp && _sg_highlighted_shape_ref(gdp).token == ref.token)
	    _sg_set_highlighted_shape_ref(gedp, GED_DRAW_SHAPE_REF_NULL);
    } else {
	_sg_set_highlighted_shape_ref(gedp, ref);
    }
    return 1;
}


ged_draw_shape_ref
ged_draw_highlighted_shape_ref(const struct ged *gedp)
{
    if (!gedp)
	return GED_DRAW_SHAPE_REF_NULL;
    return _sg_highlighted_shape_ref(gedp->i->ged_gdp);
}


static int
_highlight_by_name_shape_cb(const struct ged_draw_shape_record *rec, void *udata)
{
    struct _highlight_by_name_ctx *ctx = (struct _highlight_by_name_ctx *)udata;
    if (!ctx || !rec || !ctx->name)
	return 1;
    if ((rec->leaf_name && BU_STR_EQUAL(rec->leaf_name, ctx->name)) ||
	    (rec->display_name && BU_STR_EQUAL(rec->display_name, ctx->name))) {
	ctx->result = rec->ref;
	return 0;
    }
    return 1;
}


ged_draw_shape_ref
ged_draw_highlight_shape_ref_by_name(struct ged *gedp, const char *name)
{
    if (!gedp || !name)
	return GED_DRAW_SHAPE_REF_NULL;
    struct _highlight_by_name_ctx ctx;
    ctx.gedp = gedp;
    ctx.name = name;
    ctx.result = GED_DRAW_SHAPE_REF_NULL;
    ged_draw_foreach_shape_record(gedp, _highlight_by_name_shape_cb, &ctx);
    ged_draw_set_highlighted_shape_ref(gedp, ctx.result);
    return ctx.result;
}


/**
 * Return the highlight-state revision counter.  Bumped on every transition
 * of the highlighted draw ref.
 *
 * Cache a snapshot, then compare against a later live read to detect
 * "highlight may have changed since I last looked" without calling
 * ged_draw_highlighted_shape repeatedly.
 */
uint64_t
ged_draw_highlight_revision(const struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
        return 0;
    return gedp->i->ged_gdp->gd_highlight_rev;
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
