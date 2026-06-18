/*                S N A P _ A C T I O N . C
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
/** @file libbsg/snap_action.c */

#include "common.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "bu/malloc.h"
#include "bg/lseg.h"
#include "bsg/action.h"
#include "bsg/draw_intent.h"
#include "bsg/geometry.h"
#include "bsg/node.h"
#include "bsg/snap.h"
#include "bsg/snap_action.h"
#include "bsg/scene_object.h"
#include "action_private.h"
#include "draw_intent_private.h"
#include "feature_private.h"
#include "field_private.h"
#include "bsg_private.h"
#include "node_private.h"
#include "payload_private.h"
#include "payload_typed_private.h"

static const struct bsg_draw_intent *
_snap_nearest_intent(const struct bsg_node *node)
{
    const struct bsg_node *n = node;
    while (n) {
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(n);
	if (di)
	    return di;
	n = bsg_node_parent((struct bsg_node *)n);
    }
    return NULL;
}

void
bsg_snap_result_init(struct bsg_snap_result *out)
{
    if (!out)
	return;
    out->sr_candidates = NULL;
    out->sr_cnt = 0;
}

void
bsg_snap_result_free(struct bsg_snap_result *out)
{
    if (!out)
	return;
    if (out->sr_candidates) {
	for (size_t i = 0; i < out->sr_cnt; i++)
	    bu_vls_free(&out->sr_candidates[i].sc_source_path);
	bu_free((void *)out->sr_candidates, "bsg_snap_result candidates");
    }
    out->sr_candidates = NULL;
    out->sr_cnt = 0;
}

static int
_bsg_snap_result_append_node(struct bsg_snap_result *out, const point_t p,
			     bsg_snap_kind kind, fastf_t dist,
			     struct bsg_node *node)
{
    if (!out)
	return 0;
    size_t ncnt = out->sr_cnt + 1;
    struct bsg_snap_candidate *nc =
	(struct bsg_snap_candidate *)bu_realloc((void *)out->sr_candidates,
		ncnt * sizeof(struct bsg_snap_candidate),
		"grow bsg_snap_result candidates");
    if (!nc)
	return 0;
    out->sr_candidates = nc;
    VMOVE(out->sr_candidates[out->sr_cnt].sc_point, p);
    out->sr_candidates[out->sr_cnt].sc_kind = kind;
    out->sr_candidates[out->sr_cnt].sc_distance = dist;
    bsg_feature_ref null_feature = BSG_FEATURE_REF_NULL_INIT;
    out->sr_candidates[out->sr_cnt].sc_feature = node ?
	bsg_feature_ref_from_node(node, BSG_FEATURE_UNKNOWN) :
	null_feature;
    bu_vls_init(&out->sr_candidates[out->sr_cnt].sc_source_path);
    out->sr_candidates[out->sr_cnt].sc_stale = 0;
    if (node) {
	const struct bsg_draw_intent *di = _snap_nearest_intent(node);
	const char *path = di ? bsg_draw_intent_path(di) : NULL;
	if (path && path[0])
	    bu_vls_sprintf(&out->sr_candidates[out->sr_cnt].sc_source_path, "%s", path);
	else
	    bu_vls_sprintf(&out->sr_candidates[out->sr_cnt].sc_source_path, "%s", bsg_node_name(node));
    }
    out->sr_cnt = ncnt;
    return 1;
}

static int
_bsg_snap_result_append(struct bsg_snap_result *out, const point_t p,
			bsg_snap_kind kind, fastf_t dist)
{
    return _bsg_snap_result_append_node(out, p, kind, dist, NULL);
}

struct snap_line_ctx {
    point_t sample;
    point_t best;
    struct bsg_node *best_node;
    double best_dsq;
    double tol_sq;
    int found;
};

struct snap_action_data {
    struct bsg_view *v;
    point_t sample;
    double tol;
    bsg_snap_kind_mask kinds;
    struct bsg_snap_result *out;
};

static int
_snap_line_update_best(struct snap_line_ctx *ctx, struct bsg_node *node,
		       const mat_t model_mat, const point_t a, const point_t b)
{
    point_t w1, w2, c;
    MAT4X3PNT(w1, model_mat, a);
    MAT4X3PNT(w2, model_mat, b);
    double dsq = bg_distsq_lseg3_pt(&c, w1, w2, ctx->sample);
    if (dsq > ctx->tol_sq || dsq >= ctx->best_dsq)
	return 0;
    VMOVE(ctx->best, c);
    ctx->best_node = node;
    ctx->best_dsq = dsq;
    ctx->found = 1;
    return 1;
}

static int
_snap_point_update_best(struct snap_line_ctx *ctx, struct bsg_node *node,
			const mat_t model_mat, const point_t p)
{
    point_t wp = VINIT_ZERO;
    MAT4X3PNT(wp, model_mat, p);
    double dsq = DIST_PNT_PNT_SQ(wp, ctx->sample);
    if (dsq > ctx->tol_sq || dsq >= ctx->best_dsq)
	return 0;
    VMOVE(ctx->best, wp);
    ctx->best_node = node;
    ctx->best_dsq = dsq;
    ctx->found = 1;
    return 1;
}

static int
_snap_line_geometry_fields(struct bsg_node *node,
			   const struct bsg_action_state *state,
			   struct snap_line_ctx *ctx)
{
    int kind = BSG_GEOMETRY_NODE_NONE;
    if (!node || !ctx)
	return 0;
    if (!bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_KIND), &kind))
	return 0;

    bsg_field_ref coords = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_COORDINATES);
    size_t point_count = bsg_field_multi_count(coords);
    if (!point_count)
	return 0;

    mat_t model_mat;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(), model_mat);

    if (kind == BSG_GEOMETRY_NODE_POINT_SET ||
	    kind == BSG_GEOMETRY_NODE_INDEXED_FACE_SET) {
	point_t cur = VINIT_ZERO;
	int processed = 0;
	for (size_t i = 0; i < point_count; i++) {
	    if (!bsg_field_multi_point_at(coords, i, cur))
		continue;
	    (void)_snap_point_update_best(ctx, node, model_mat, cur);
	    processed = 1;
	}
	return processed;
    }

    if (kind == BSG_GEOMETRY_NODE_INDEXED_LINE_SET) {
	bsg_field_ref indices = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_INDICES);
	size_t index_count = bsg_field_multi_count(indices);
	point_t last = VINIT_ZERO;
	point_t cur = VINIT_ZERO;
	int have_last = 0;
	int processed = 0;
	for (size_t i = 0; i < index_count; i++) {
	    int idx = -1;
	    if (!bsg_field_multi_int_at(indices, i, &idx))
		continue;
	    if (idx < 0) {
		have_last = 0;
		processed = 1;
		continue;
	    }
	    if ((size_t)idx >= point_count)
		continue;
	    if (!bsg_field_multi_point_at(coords, (size_t)idx, cur))
		continue;
	    if (have_last)
		(void)_snap_line_update_best(ctx, node, model_mat, last, cur);
	    VMOVE(last, cur);
	    have_last = 1;
	    processed = 1;
	}
	return processed;
    }

    if (kind != BSG_GEOMETRY_NODE_LINE_SET)
	return 0;

    bsg_field_ref cmds = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
    point_t last = VINIT_ZERO;
    point_t cur = VINIT_ZERO;
    int have_last = 0;
    int processed = 0;
    for (size_t i = 0; i < point_count; i++) {
	int cmd = (i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
	if (!bsg_field_multi_point_at(coords, i, cur))
	    continue;
	(void)bsg_field_multi_int_at(cmds, i, &cmd);
	switch (cmd) {
	    case BSG_GEOMETRY_LINE_MOVE:
		VMOVE(last, cur);
		have_last = 1;
		break;
	    case BSG_GEOMETRY_LINE_DRAW:
		if (have_last)
		    (void)_snap_line_update_best(ctx, node, model_mat, last, cur);
		VMOVE(last, cur);
		have_last = 1;
		break;
	    default:
		break;
	}
	processed = 1;
    }
    return processed;
}

static struct bsg_payload_line_set *
_snap_node_payload_line_set(struct bsg_node *node)
{
    struct bsg_payload *payload = bsg_node_get_payload(node);
    if (!payload)
	return NULL;
    if (payload->pl_type == BSG_PL_LINE_SET)
	return bsg_payload_line_set_get(payload);
    if (payload->pl_type == BSG_PL_SKETCH) {
	struct bsg_payload *geometry = bsg_payload_sketch_geometry(payload);
	return bsg_payload_line_set_get(geometry);
    }
    return NULL;
}

static int
_snap_line_payload_line_set(struct bsg_node *node,
			    const struct bsg_action_state *state,
			    struct snap_line_ctx *ctx)
{
    struct bsg_payload_line_set *ls = _snap_node_payload_line_set(node);
    if (!ls || !ls->points || !ls->point_cnt)
	return 0;

    mat_t model_mat;
    point_t last = VINIT_ZERO;
    int have_last = 0;
    int processed = 0;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(), model_mat);
    for (size_t i = 0; i < ls->point_cnt; i++) {
	int cmd = ls->cmds ? ls->cmds[i] :
	    ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	if (cmd == BSG_GEOMETRY_LINE_MOVE) {
	    VMOVE(last, ls->points[i]);
	    have_last = 1;
	    processed = 1;
	    continue;
	}
	if (cmd == BSG_GEOMETRY_LINE_DRAW) {
	    if (have_last)
		(void)_snap_line_update_best(ctx, node, model_mat, last, ls->points[i]);
	    VMOVE(last, ls->points[i]);
	    have_last = 1;
	    processed = 1;
	}
    }

    return processed;
}

static int
_snap_line_shape_cb(struct bsg_node *node,
		    const struct bsg_action_state *state,
		    void *userdata)
{
    struct snap_line_ctx *ctx = (struct snap_line_ctx *)userdata;
    if (_snap_line_geometry_fields(node, state, ctx))
	return 1;
    if (_snap_line_payload_line_set(node, state, ctx))
	return 1;

    return 1;
}

static int
_snap_candidates_impl(struct bsg_view *v, point_t sample, double tol,
		      bsg_snap_kind_mask kinds, struct bsg_snap_result *out)
{
    if (!v || !out)
	return 0;

    bsg_snap_result_free(out);
    bsg_snap_result_init(out);

    point_t sample_model = VINIT_ZERO;
    VMOVE(sample_model, sample);

    if ((kinds & BSG_SNAP_KIND_ENDPOINT) ||
	(kinds & BSG_SNAP_KIND_MIDPOINT) ||
	(kinds & BSG_SNAP_KIND_INTERSECTION) ||
	(kinds & BSG_SNAP_KIND_PERPENDICULAR) ||
	(kinds & BSG_SNAP_KIND_TANGENT) ||
	(kinds & BSG_SNAP_KIND_OVERLAY_HANDLE)) {
	struct snap_line_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	VMOVE(ctx.sample, sample_model);
	ctx.best_dsq = DBL_MAX;
	ctx.tol_sq = (tol > 0.0) ? tol * tol : DBL_MAX;
	if (bsg_view_scene_root(v)) {
	    struct bsg_action_traversal traversal;
	    memset(&traversal, 0, sizeof(traversal));
	    traversal.view = v;
	    traversal.root = bsg_view_scene_root(v);
	    traversal.flags = BSG_ACTION_TRAVERSE_VISIBLE_ONLY |
		BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT;
	    traversal.shape_cb = _snap_line_shape_cb;
	    traversal.userdata = &ctx;
	    bsg_action_traverse(&traversal);
	}
	if (ctx.found) {
	    fastf_t dist = sqrt(ctx.best_dsq);
	    _bsg_snap_result_append_node(out, ctx.best, BSG_SNAP_KIND_ENDPOINT,
		    dist, ctx.best_node);
	}
    }

    if (kinds & BSG_SNAP_KIND_GRID) {
	point_t view_pt = VINIT_ZERO;
	point_t grid_model = VINIT_ZERO;
	fastf_t fx = 0.0;
	fastf_t fy = 0.0;

	MAT4X3PNT(view_pt, v->gv_model2view, sample_model);
	fx = view_pt[X];
	fy = view_pt[Y];
	if (bsg_snap_grid_2d(v, &fx, &fy)) {
	    VSET(view_pt, fx, fy, view_pt[Z]);
	    MAT4X3PNT(grid_model, v->gv_view2model, view_pt);
	    fastf_t dist = DIST_PNT_PNT(sample_model, grid_model);
	    if (tol <= 0.0 || dist <= tol)
		_bsg_snap_result_append(out, grid_model, BSG_SNAP_KIND_GRID, dist);
	}
    }

    return (int)out->sr_cnt;
}

static int
_snap_action_apply(bsg_action_ref UNUSED(action),
		   bsg_node_ref UNUSED(root),
		   void *data)
{
    struct snap_action_data *st = (struct snap_action_data *)data;
    if (!st)
	return 0;
    return _snap_candidates_impl(st->v, st->sample, st->tol, st->kinds, st->out);
}

static void
_snap_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    if (data)
	bu_free(data, "snap action data");
}

bsg_action_ref
bsg_snap_action_create(struct bsg_view *view,
		       const point_t sample,
		       double tol,
		       unsigned long long kinds,
		       struct bsg_snap_result *result)
{
    struct snap_action_data *st;
    BU_ALLOC(st, struct snap_action_data);
    memset(st, 0, sizeof(*st));
    st->v = view;
    VMOVE(st->sample, sample);
    st->tol = tol;
    st->kinds = (bsg_snap_kind_mask)kinds;
    st->out = result;
    return bsg_action_ref_create_internal(bsg_snap_action_type(),
	    _snap_action_apply, _snap_action_destroy, st, "snap action");
}

int
bsg_snap_candidates(struct bsg_view *v, point_t sample, double tol,
		    bsg_snap_kind_mask kinds, struct bsg_snap_result *out)
{
    bsg_action_ref action = bsg_snap_action_create(v, sample, tol, kinds, out);
    int ret = bsg_action_apply(action, bsg_node_ref_null());
    bsg_action_ref_destroy(action);
    return ret;
}

int
bsg_snap_point_2d(struct bsg_view *v, fastf_t *vx, fastf_t *vy,
		  bsg_snap_kind_mask kinds)
{
    if (!v || !vx || !vy)
	return 0;

    /* Convert the 2D view-space sample to a 3D model point. */
    point_t view_pt = VINIT_ZERO;
    point_t model_pt = VINIT_ZERO;
    VSET(view_pt, *vx, *vy, 0.0);
    MAT4X3PNT(model_pt, v->gv_view2model, view_pt);

    struct bsg_snap_result sr;
    bsg_snap_result_init(&sr);
    if (!bsg_snap_candidates(v, model_pt, 0.0, kinds, &sr) || !sr.sr_cnt) {
	bsg_snap_result_free(&sr);
	return 0;
    }

    /* Pick the nearest candidate. */
    size_t best = 0;
    for (size_t i = 1; i < sr.sr_cnt; i++) {
	if (sr.sr_candidates[i].sc_distance < sr.sr_candidates[best].sc_distance)
	    best = i;
    }

    /* Convert winner back to 2D view space. */
    point_t snapped_view = VINIT_ZERO;
    MAT4X3PNT(snapped_view, v->gv_model2view, sr.sr_candidates[best].sc_point);
    *vx = snapped_view[X];
    *vy = snapped_view[Y];

    bsg_snap_result_free(&sr);
    return 1;
}
