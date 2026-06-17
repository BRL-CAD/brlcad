/*                          P I C K . C
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
/** @file libbsg/pick.c
 *
 * Typed pick records and pick actions.
 *
 * Implementation of bsg/pick.h.
 *
 * Picking strategy:
 *   bsg_pick_point() and bsg_pick_rect() delegate the candidate-node
 *   collection to the bsg_view_pick_candidates() /
 *   bsg_view_rect_pick_candidates() helpers, which use an OBB SAT
 *   test to find nodes whose bounding boxes overlap the pick volume.
 *
 *   For each candidate the hit distance is estimated as the model-space
 *   distance from the view's back-out point to the centre of the node's
 *   bounding box; this gives a "nearest first" ordering consistent with the
 *   bbox-based closest_obj_bbox() logic in QgSelectFilter.cpp.
 *
 *   Source-path strings are extracted from the node's draw-intent when
 *   present, falling back to the canonical node name field.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bg/aabb_ray.h"
#include "bg/lseg.h"
#include "bg/tri_ray.h"
#include "vmath.h"

#include "bsg/defines.h"
#include "bsg/action.h"
#include "bsg/draw_intent.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "bsg/pick.h"
#include "bsg/selection.h"
#include "bsg/util.h"
#include "draw_intent_private.h"
#include "action_private.h"
#include "bsg_private.h"
#include "field_private.h"
#include "node_private.h"
#include "payload_typed_private.h"


/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/*
 * Estimate the model-space distance from the view back-out point to the
 * centre of node @p s's bounding box.
 */
static fastf_t
_node_hit_dist(const bsg_node *s, const struct bsg_view *v)
{
    if (!s || !v)
	return -1.0;

    point_t bmin, bmax;
    if (!bsg_node_bounds(s, bmin, bmax))
	return -1.0;

    point_t centre;
    VADD2SCALE(centre, bmin, bmax, 0.5);
    return DIST_PNT_PNT(centre, v->gv_vc_backout);
}

/*
 * Comparator for bu_sort: order bsg_pick_record* by ascending pr_hit_dist.
 */
static int
_record_cmp(const void *a, const void *b, void *UNUSED(ctx))
{
    const struct bsg_pick_record *ra = *(const struct bsg_pick_record **)a;
    const struct bsg_pick_record *rb = *(const struct bsg_pick_record **)b;
    if (ra->pr_hit_dist < rb->pr_hit_dist) return -1;
    if (ra->pr_hit_dist > rb->pr_hit_dist) return  1;
    return 0;
}

/*
 * Allocate a single pick record for @p node in view @p v at screen (@p sx,
 * @p sy).  Source path comes from the node's draw intent when available,
 * falling back to the canonical node name field.
 *
 * Returns NULL on allocation failure.
 */
static const struct bsg_draw_intent *
_nearest_intent(const bsg_node *node);

static struct bsg_pick_record *
_record_create(bsg_node *node, struct bsg_view *v, int sx, int sy)
{
    struct bsg_pick_record *pr;
    BU_GET(pr, struct bsg_pick_record);
    bu_vls_init(&pr->pr_source_path);
    bu_vls_init(&pr->pr_instance_path);

    pr->pr_scene.opaque = node;
    pr->pr_feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    pr->pr_valid    = node ? 1 : 0;
    pr->pr_view     = v;
    pr->pr_screen_x = sx;
    pr->pr_screen_y = sy;
    pr->pr_hit_dist = (v) ? _node_hit_dist(node, v) : -1.0;
    pr->pr_primitive_id = -1;
    pr->pr_subelement_id = -1;

    /* Prefer nearest draw-intent path; fall back to the node name. */
    const struct bsg_draw_intent *di = _nearest_intent(node);
    const char *di_path = (di) ? bsg_draw_intent_path(di) : NULL;
    if (di_path && *di_path) {
	bu_vls_sprintf(&pr->pr_source_path, "%s", di_path);
    } else {
	bu_vls_sprintf(&pr->pr_source_path, "%s", bsg_node_name(node));
    }
    bu_vls_sprintf(&pr->pr_instance_path, "%s", bu_vls_cstr(&pr->pr_source_path));
    if (bu_vls_strlen(&pr->pr_source_path) == 0)
	pr->pr_valid = 0;

    return pr;
}

static struct bsg_pick_record *
_record_clone(const struct bsg_pick_record *src)
{
    if (!src)
	return NULL;
    struct bsg_pick_record *pr;
    BU_GET(pr, struct bsg_pick_record);
    bu_vls_init(&pr->pr_source_path);
    bu_vls_init(&pr->pr_instance_path);
    pr->pr_scene = src->pr_scene;
    pr->pr_feature = src->pr_feature;
    pr->pr_valid = src->pr_valid;
    pr->pr_view = src->pr_view;
    pr->pr_screen_x = src->pr_screen_x;
    pr->pr_screen_y = src->pr_screen_y;
    pr->pr_hit_dist = src->pr_hit_dist;
    pr->pr_primitive_id = src->pr_primitive_id;
    pr->pr_subelement_id = src->pr_subelement_id;
    bu_vls_sprintf(&pr->pr_source_path, "%s", bu_vls_cstr(&src->pr_source_path));
    bu_vls_sprintf(&pr->pr_instance_path, "%s", bu_vls_cstr(&src->pr_instance_path));
    return pr;
}

static const struct bsg_draw_intent *
_nearest_intent(const bsg_node *node)
{
    const bsg_node *n = node;
    while (n) {
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(n);
	if (di)
	    return di;
	n = bsg_node_parent((bsg_node *)n);
    }
    return NULL;
}

static void
_transformed_node_bounds(const bsg_node *node,
			 const struct bsg_action_state *state,
			 point_t bmin,
			 point_t bmax)
{
    point_t nbmin, nbmax;
    if (!bsg_node_bounds(node, nbmin, nbmax)) {
	VSETALL(bmin, 0.0);
	VSETALL(bmax, 0.0);
	return;
    }

    mat_t model_mat;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(), model_mat);
    int first = 1;
    for (int ix = 0; ix < 2; ix++) {
	for (int iy = 0; iy < 2; iy++) {
	    for (int iz = 0; iz < 2; iz++) {
		point_t c, wc;
		VSET(c, ix ? nbmax[X] : nbmin[X],
			iy ? nbmax[Y] : nbmin[Y],
			iz ? nbmax[Z] : nbmin[Z]);
		MAT4X3PNT(wc, model_mat, c);
		if (first) {
		    VMOVE(bmin, wc);
		    VMOVE(bmax, wc);
		    first = 0;
		} else {
		    VMIN(bmin, wc);
		    VMAX(bmax, wc);
		}
	    }
	}
    }
}

static void
_record_free(struct bsg_pick_record *pr)
{
    if (!pr)
	return;
    bu_vls_free(&pr->pr_source_path);
    bu_vls_free(&pr->pr_instance_path);
    BU_PUT(pr, struct bsg_pick_record);
}

struct pick_ray_ctx {
    struct bsg_pick_result *res;
    struct bsg_view *v;
    point_t orig;
    vect_t dir;
    vect_t invdir;
    int include_scene;
    int include_overlays;
};

struct pick_action_data {
    struct bsg_pick_result *res;
    struct bsg_view *v;
    point_t orig;
    vect_t dir;
    unsigned int flags;
};

static int
_pick_ray_record_hit(struct pick_ray_ctx *pctx, bsg_node *node, fastf_t hit_dist)
{
    struct bsg_pick_record *pr = _record_create(node, pctx->v, -1, -1);
    if (!pr)
	return 0;
    pr->pr_hit_dist = hit_dist;
    bu_ptbl_ins(&pctx->res->pr_records, (long *)pr);
    return 1;
}

static int
_pick_ray_segment_hit_dist(struct pick_ray_ctx *pctx,
			   const point_t a,
			   const point_t b,
			   fastf_t *hit_dist)
{
    vect_t dir = VINIT_ZERO;
    VMOVE(dir, pctx->dir);
    fastf_t dir_mag = MAGNITUDE(dir);
    if (ZERO(dir_mag))
	return 0;
    VSCALE(dir, dir, 1.0 / dir_mag);

    fastf_t ray_len = DIST_PNT_PNT(pctx->orig, a);
    fastf_t bdist = DIST_PNT_PNT(pctx->orig, b);
    if (bdist > ray_len)
	ray_len = bdist;
    ray_len += 1.0;
    if (ray_len < 1.0)
	ray_len = 1.0;

    point_t ray_end;
    VJOIN1(ray_end, pctx->orig, ray_len, dir);

    point_t ray_closest = VINIT_ZERO;
    point_t seg_closest = VINIT_ZERO;
    double dsq = bg_distsq_lseg3_lseg3(&ray_closest, &seg_closest,
	    pctx->orig, ray_end, a, b);
    if (dsq > BN_TOL_DIST * BN_TOL_DIST)
	return 0;

    vect_t from_orig = VINIT_ZERO;
    VSUB2(from_orig, ray_closest, pctx->orig);
    fastf_t t = VDOT(from_orig, dir);
    if (t < -BN_TOL_DIST)
	return 0;
    if (hit_dist)
	*hit_dist = (t > 0.0) ? t : 0.0;
    return 1;
}

static int
_pick_ray_point_hit_dist(struct pick_ray_ctx *pctx,
			 const point_t p,
			 fastf_t *hit_dist)
{
    vect_t dir = VINIT_ZERO;
    VMOVE(dir, pctx->dir);
    fastf_t dir_mag = MAGNITUDE(dir);
    if (ZERO(dir_mag))
	return 0;
    VSCALE(dir, dir, 1.0 / dir_mag);

    fastf_t ray_len = DIST_PNT_PNT(pctx->orig, p) + 1.0;
    if (ray_len < 1.0)
	ray_len = 1.0;

    point_t ray_end;
    VJOIN1(ray_end, pctx->orig, ray_len, dir);

    point_t ray_closest = VINIT_ZERO;
    double dsq = bg_distsq_lseg3_pt(&ray_closest, pctx->orig, ray_end, p);
    if (dsq > BN_TOL_DIST * BN_TOL_DIST)
	return 0;

    vect_t from_orig = VINIT_ZERO;
    VSUB2(from_orig, ray_closest, pctx->orig);
    fastf_t t = VDOT(from_orig, dir);
    if (t < -BN_TOL_DIST)
	return 0;
    if (hit_dist)
	*hit_dist = (t > 0.0) ? t : 0.0;
    return 1;
}

static int
_pick_ray_triangle_hit_dist(struct pick_ray_ctx *pctx,
			    const point_t a,
			    const point_t b,
			    const point_t c,
			    fastf_t *hit_dist)
{
    vect_t dir = VINIT_ZERO;
    VMOVE(dir, pctx->dir);
    fastf_t dir_mag = MAGNITUDE(dir);
    if (ZERO(dir_mag))
	return 0;
    VSCALE(dir, dir, 1.0 / dir_mag);

    point_t isect = VINIT_ZERO;
    if (!bg_isect_tri_ray(pctx->orig, dir, a, b, c, &isect))
	return 0;

    vect_t from_orig = VINIT_ZERO;
    VSUB2(from_orig, isect, pctx->orig);
    fastf_t t = VDOT(from_orig, dir);
    if (t < -BN_TOL_DIST)
	return 0;
    if (hit_dist)
	*hit_dist = (t > 0.0) ? t : 0.0;
    return 1;
}

static int
_pick_ray_line_segment(struct pick_ray_ctx *pctx,
		       const mat_t model_mat,
		       const point_t a,
		       const point_t b,
		       fastf_t *best_dist)
{
    point_t wa = VINIT_ZERO;
    point_t wb = VINIT_ZERO;
    fastf_t hit_dist = 0.0;
    MAT4X3PNT(wa, model_mat, a);
    MAT4X3PNT(wb, model_mat, b);
    if (!_pick_ray_segment_hit_dist(pctx, wa, wb, &hit_dist))
	return 0;
    if (*best_dist < 0.0 || hit_dist < *best_dist)
	*best_dist = hit_dist;
    return 1;
}

static int
_pick_ray_triangle(struct pick_ray_ctx *pctx,
		   const mat_t model_mat,
		   const point_t a,
		   const point_t b,
		   const point_t c,
		   fastf_t *best_dist)
{
    point_t wa = VINIT_ZERO;
    point_t wb = VINIT_ZERO;
    point_t wc = VINIT_ZERO;
    fastf_t hit_dist = 0.0;
    MAT4X3PNT(wa, model_mat, a);
    MAT4X3PNT(wb, model_mat, b);
    MAT4X3PNT(wc, model_mat, c);
    if (!_pick_ray_triangle_hit_dist(pctx, wa, wb, wc, &hit_dist))
	return 0;
    if (*best_dist < 0.0 || hit_dist < *best_dist)
	*best_dist = hit_dist;
    return 1;
}

static int
_pick_ray_point(struct pick_ray_ctx *pctx,
		const mat_t model_mat,
		const point_t p,
		fastf_t *best_dist)
{
    point_t wp = VINIT_ZERO;
    fastf_t hit_dist = 0.0;
    MAT4X3PNT(wp, model_mat, p);
    if (!_pick_ray_point_hit_dist(pctx, wp, &hit_dist))
	return 0;
    if (*best_dist < 0.0 || hit_dist < *best_dist)
	*best_dist = hit_dist;
    return 1;
}

static int
_annotation_local_point(const struct bsg_payload_annotation *ann, int idx,
	point_t out)
{
    if (!ann || idx < 0 || (size_t)idx >= ann->point_cnt || !ann->points)
	return 0;
    VMOVE(out, ann->points[idx]);
    return 1;
}

static void
_annotation_world_point(const struct bsg_payload_annotation *ann,
	const mat_t state_model_mat, const point_t local, point_t out)
{
    point_t resolved;
    if (ann->space == BSG_ANNOTATION_SPACE_DISPLAY) {
	point_t display_local;
	MAT4X3PNT(display_local, ann->display_mat, local);
	VADD2(resolved, ann->anchor, display_local);
    } else {
	MAT4X3PNT(resolved, ann->model_mat, local);
    }
    MAT4X3PNT(out, state_model_mat, resolved);
}

static void
_pick_ray_annotation_point(struct pick_ray_ctx *pctx,
	const struct bsg_payload_annotation *ann,
	const mat_t state_model_mat,
	const point_t local,
	fastf_t *best_dist)
{
    point_t world;
    fastf_t hit_dist = 0.0;
    _annotation_world_point(ann, state_model_mat, local, world);
    if (!_pick_ray_point_hit_dist(pctx, world, &hit_dist))
	return;
    if (*best_dist < 0.0 || hit_dist < *best_dist)
	*best_dist = hit_dist;
}

static void
_pick_ray_annotation_line(struct pick_ray_ctx *pctx,
	const struct bsg_payload_annotation *ann,
	const mat_t state_model_mat,
	const point_t a,
	const point_t b,
	fastf_t *best_dist)
{
    point_t wa, wb;
    fastf_t hit_dist = 0.0;
    _annotation_world_point(ann, state_model_mat, a, wa);
    _annotation_world_point(ann, state_model_mat, b, wb);
    if (!_pick_ray_segment_hit_dist(pctx, wa, wb, &hit_dist))
	return;
    if (*best_dist < 0.0 || hit_dist < *best_dist)
	*best_dist = hit_dist;
}

static int
_pick_ray_annotation_payload(bsg_node *node,
			     const struct bsg_action_state *state,
			     struct pick_ray_ctx *pctx)
{
    struct bsg_payload_annotation *ann =
	bsg_payload_annotation_get(bsg_node_get_payload(node));
    if (!ann || !ann->segment_cnt || !ann->segments)
	return 0;

    mat_t state_model_mat;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(),
	    state_model_mat);

    fastf_t best_dist = -1.0;
    for (size_t i = 0; i < ann->segment_cnt; i++) {
	const struct bsg_annotation_segment *seg = &ann->segments[i];
	point_t a, b;
	switch (seg->kind) {
	    case BSG_ANNOTATION_SEGMENT_LINE:
		if (_annotation_local_point(ann, seg->data.line.start, a) &&
			_annotation_local_point(ann, seg->data.line.end, b))
		    _pick_ray_annotation_line(pctx, ann, state_model_mat, a, b,
			    &best_dist);
		break;
	    case BSG_ANNOTATION_SEGMENT_CARC:
		if (_annotation_local_point(ann, seg->data.carc.start, a) &&
			_annotation_local_point(ann, seg->data.carc.end, b))
		    _pick_ray_annotation_line(pctx, ann, state_model_mat, a, b,
			    &best_dist);
		break;
	    case BSG_ANNOTATION_SEGMENT_BEZIER:
		for (size_t j = 1; j < seg->data.bezier.control_point_count; j++) {
		    if (_annotation_local_point(ann,
				seg->data.bezier.control_points[j - 1], a) &&
			    _annotation_local_point(ann,
				seg->data.bezier.control_points[j], b))
			_pick_ray_annotation_line(pctx, ann, state_model_mat, a, b,
				&best_dist);
		}
		break;
	    case BSG_ANNOTATION_SEGMENT_NURB:
		for (size_t j = 1; j < seg->data.nurb.control_point_count; j++) {
		    if (_annotation_local_point(ann,
				seg->data.nurb.control_points[j - 1], a) &&
			    _annotation_local_point(ann,
				seg->data.nurb.control_points[j], b))
			_pick_ray_annotation_line(pctx, ann, state_model_mat, a, b,
				&best_dist);
		}
		break;
	    case BSG_ANNOTATION_SEGMENT_TEXT:
		if (_annotation_local_point(ann, seg->data.text.ref_pt, a))
		    _pick_ray_annotation_point(pctx, ann, state_model_mat, a,
			    &best_dist);
		break;
	    default:
		break;
	}
    }

    if (best_dist >= 0.0)
	(void)_pick_ray_record_hit(pctx, node, best_dist);
    return 1;
}

static int
_pick_ray_geometry_fields(bsg_node *node,
			  const struct bsg_action_state *state,
			  struct pick_ray_ctx *pctx)
{
    int kind = BSG_GEOMETRY_NODE_NONE;
    if (!node || !pctx)
	return 0;
    if (!bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_KIND), &kind))
	return 0;
    if (kind == BSG_GEOMETRY_NODE_ANNOTATION)
	return _pick_ray_annotation_payload(node, state, pctx);
    if (kind != BSG_GEOMETRY_NODE_LINE_SET &&
	    kind != BSG_GEOMETRY_NODE_INDEXED_LINE_SET &&
	    kind != BSG_GEOMETRY_NODE_POINT_SET &&
	    kind != BSG_GEOMETRY_NODE_INDEXED_FACE_SET)
	return 0;

    bsg_field_ref coords = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_COORDINATES);
    size_t point_count = bsg_field_multi_count(coords);
    if (!point_count)
	return 1;

    mat_t model_mat;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(), model_mat);

    fastf_t best_dist = -1.0;
    if (kind == BSG_GEOMETRY_NODE_POINT_SET) {
	point_t cur = VINIT_ZERO;
	for (size_t i = 0; i < point_count; i++) {
	    if (!bsg_field_multi_point_at(coords, i, cur))
		continue;
	    (void)_pick_ray_point(pctx, model_mat, cur, &best_dist);
	}
    } else if (kind == BSG_GEOMETRY_NODE_INDEXED_FACE_SET) {
	bsg_field_ref indices = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_INDICES);
	size_t index_count = bsg_field_multi_count(indices);
	point_t first = VINIT_ZERO;
	point_t prev = VINIT_ZERO;
	point_t cur = VINIT_ZERO;
	int have_first = 0;
	int have_prev = 0;
	for (size_t i = 0; i < index_count; i++) {
	    int idx = -1;
	    if (!bsg_field_multi_int_at(indices, i, &idx))
		continue;
	    if (idx < 0) {
		have_first = 0;
		have_prev = 0;
		continue;
	    }
	    if ((size_t)idx >= point_count)
		continue;
	    if (!bsg_field_multi_point_at(coords, (size_t)idx, cur))
		continue;
	    if (!have_first) {
		VMOVE(first, cur);
		have_first = 1;
	    } else if (!have_prev) {
		VMOVE(prev, cur);
		have_prev = 1;
	    } else {
		(void)_pick_ray_triangle(pctx, model_mat, first, prev, cur, &best_dist);
		VMOVE(prev, cur);
	    }
	}
    } else if (kind == BSG_GEOMETRY_NODE_INDEXED_LINE_SET) {
	bsg_field_ref indices = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_INDICES);
	size_t index_count = bsg_field_multi_count(indices);
	point_t last = VINIT_ZERO;
	point_t cur = VINIT_ZERO;
	int have_last = 0;
	for (size_t i = 0; i < index_count; i++) {
	    int idx = -1;
	    if (!bsg_field_multi_int_at(indices, i, &idx))
		continue;
	    if (idx < 0) {
		have_last = 0;
		continue;
	    }
	    if ((size_t)idx >= point_count)
		continue;
	    if (!bsg_field_multi_point_at(coords, (size_t)idx, cur))
		continue;
	    if (have_last)
		(void)_pick_ray_line_segment(pctx, model_mat, last, cur, &best_dist);
	    VMOVE(last, cur);
	    have_last = 1;
	}
    } else {
	bsg_field_ref cmds = bsg_node_field_ref(node, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
	point_t last = VINIT_ZERO;
	point_t cur = VINIT_ZERO;
	int have_last = 0;
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
			(void)_pick_ray_line_segment(pctx, model_mat, last, cur, &best_dist);
		    VMOVE(last, cur);
		    have_last = 1;
		    break;
		default:
		    break;
	    }
	}
    }

    if (best_dist >= 0.0)
	(void)_pick_ray_record_hit(pctx, node, best_dist);
    return 1;
}

static int
_pick_ray_shape_cb(bsg_node *node,
		   const struct bsg_action_state *state,
		   void *userdata)
{
    struct pick_ray_ctx *pctx = (struct pick_ray_ctx *)userdata;
    const struct bsg_draw_intent *di = _nearest_intent(node);
    const int is_overlay = di ? bsg_draw_intent_is_overlay(di) : 0;
    if ((is_overlay && !pctx->include_overlays) ||
	    (!is_overlay && !pctx->include_scene))
	return 1;

    int field_status = _pick_ray_geometry_fields(node, state, pctx);
    if (field_status != 0)
	return 1;

    point_t bmin, bmax;
    if (!bsg_node_bounds(node, bmin, bmax))
	return 1;
    _transformed_node_bounds(node, state, bmin, bmax);
    fastf_t tmin = 0.0, tmax = 0.0;
    if (!bg_isect_aabb_ray(&tmin, &tmax, (fastf_t *)pctx->orig,
		(const fastf_t *)pctx->invdir,
		(const fastf_t *)bmin, (const fastf_t *)bmax))
	return 1;
    struct bsg_pick_record *pr = _record_create(node, pctx->v, -1, -1);
    if (!pr)
	return 1;
    pr->pr_hit_dist = (tmin >= 0.0) ? tmin : tmax;
    bu_ptbl_ins(&pctx->res->pr_records, (long *)pr);
    return 1;
}

/*
 * Sort the pr_records table in @p res by ascending pr_hit_dist.
 */
static void
_result_sort(struct bsg_pick_result *res)
{
    size_t n = BU_PTBL_LEN(&res->pr_records);
    if (n < 2)
	return;
    bu_sort(res->pr_records.buffer, n,
	  sizeof(long *), _record_cmp, NULL);
}


/* -----------------------------------------------------------------------
 * bsg_pick_result lifecycle
 * ----------------------------------------------------------------------- */

struct bsg_pick_result *
bsg_pick_result_create(void)
{
    struct bsg_pick_result *res;
    BU_GET(res, struct bsg_pick_result);
    bu_ptbl_init(&res->pr_records, 8, "bsg_pick_result");
    return res;
}

void
bsg_pick_result_free(struct bsg_pick_result *res)
{
    if (!res)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&res->pr_records); i++) {
	struct bsg_pick_record *pr =
	    (struct bsg_pick_record *)BU_PTBL_GET(&res->pr_records, i);
	_record_free(pr);
    }
    bu_ptbl_free(&res->pr_records);
    BU_PUT(res, struct bsg_pick_result);
}

size_t
bsg_pick_result_count(const struct bsg_pick_result *res)
{
    if (!res)
	return 0;
    return BU_PTBL_LEN(&res->pr_records);
}

struct bsg_pick_record *
bsg_pick_result_get(const struct bsg_pick_result *res, size_t i)
{
    if (!res || i >= BU_PTBL_LEN(&res->pr_records))
	return NULL;
    return (struct bsg_pick_record *)BU_PTBL_GET(&res->pr_records, i);
}


/* -----------------------------------------------------------------------
 * Pick actions
 * ----------------------------------------------------------------------- */

struct bsg_pick_result *
bsg_pick_point(struct bsg_view *v, int x, int y, int first_only)
{
    if (!v)
	return NULL;

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res)
	return NULL;

    struct bu_ptbl sset = BU_PTBL_INIT_ZERO;
    int scnt = bsg_view_pick_candidates(&sset, v, x, y);
    if (scnt <= 0) {
	bu_ptbl_free(&sset);
	return res;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&sset); i++) {
	bsg_node *node = (bsg_node *)BU_PTBL_GET(&sset, i);
	struct bsg_pick_record *pr = _record_create(node, v, x, y);
	if (pr)
	    bu_ptbl_ins(&res->pr_records, (long *)pr);
    }
    bu_ptbl_free(&sset);

    _result_sort(res);

    /* Apply first_only filter after sorting so the closest survives. */
    if (first_only && BU_PTBL_LEN(&res->pr_records) > 1) {
	for (size_t i = 1; i < BU_PTBL_LEN(&res->pr_records); i++) {
	    struct bsg_pick_record *pr =
		(struct bsg_pick_record *)BU_PTBL_GET(&res->pr_records, i);
	    _record_free(pr);
	}
	/* Truncate table to one entry. */
	res->pr_records.end = 1;
    }

    return res;
}


struct bsg_pick_result *
bsg_pick_rect(struct bsg_view *v, int x0, int y0, int x1, int y1)
{
    if (!v)
	return NULL;

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res)
	return NULL;

    struct bu_ptbl sset = BU_PTBL_INIT_ZERO;
    int scnt = bsg_view_rect_pick_candidates(&sset, v, x0, y0, x1, y1);
    if (scnt <= 0) {
	bu_ptbl_free(&sset);
	return res;
    }

    /* Use rect centre as representative screen coordinates. */
    int cx = (x0 + x1) / 2;
    int cy = (y0 + y1) / 2;

    for (size_t i = 0; i < BU_PTBL_LEN(&sset); i++) {
	bsg_node *node = (bsg_node *)BU_PTBL_GET(&sset, i);
	struct bsg_pick_record *pr = _record_create(node, v, cx, cy);
	if (pr)
	    bu_ptbl_ins(&res->pr_records, (long *)pr);
    }
    bu_ptbl_free(&sset);

    _result_sort(res);
    return res;
}


struct bsg_pick_result *
bsg_pick_nearest(struct bsg_view *v, int x, int y)
{
    return bsg_pick_point(v, x, y, 1 /* first_only */);
}

struct bsg_pick_result *
bsg_pick_ray(struct bsg_view *v, const point_t orig, const vect_t dir,
	     bsg_pick_flags flags)
{
    if (!v)
	return NULL;

    bsg_action_ref action = bsg_pick_ray_action_create(v, orig, dir,
	    (unsigned int)flags);
    (void)bsg_action_apply(action, bsg_node_ref_null());
    struct bsg_pick_result *res = bsg_pick_action_result(action);
    bsg_action_ref_destroy(action);
    return res;
}

static int
_pick_ray_action_apply(bsg_action_ref UNUSED(action),
		       bsg_node_ref UNUSED(root),
		       void *data)
{
    struct pick_action_data *st = (struct pick_action_data *)data;
    if (!st || !st->v || !st->res)
	return 0;

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res)
	return 0;
    bsg_pick_result_free(st->res);
    st->res = res;

    bsg_node *root = bsg_view_scene_root(st->v);
    if (!root)
	return 1;

    struct pick_ray_ctx ctx;
    ctx.res = st->res;
    ctx.v = st->v;
    VMOVE(ctx.orig, st->orig);
    VMOVE(ctx.dir, st->dir);
    ctx.include_scene = (st->flags & BSG_PICK_INCLUDE_SCENE) ? 1 : 0;
    ctx.include_overlays = (st->flags & BSG_PICK_INCLUDE_OVERLAYS) ? 1 : 0;
    if (!ctx.include_scene && !ctx.include_overlays)
	ctx.include_scene = 1;
    bg_ray_invdir(&ctx.invdir, (fastf_t *)st->dir);

    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = st->v;
    traversal.root = root;
    traversal.flags = BSG_ACTION_TRAVERSE_VISIBLE_ONLY |
	BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT;
    traversal.shape_cb = _pick_ray_shape_cb;
    traversal.userdata = &ctx;
    bsg_action_traverse(&traversal);

    _result_sort(st->res);
    if ((st->flags & BSG_PICK_FIRST_ONLY) && BU_PTBL_LEN(&st->res->pr_records) > 1) {
	for (size_t i = 1; i < BU_PTBL_LEN(&st->res->pr_records); i++) {
	    struct bsg_pick_record *pr =
		(struct bsg_pick_record *)BU_PTBL_GET(&st->res->pr_records, i);
	    _record_free(pr);
	}
	st->res->pr_records.end = 1;
    }

    return 1;
}

static void
_pick_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    struct pick_action_data *st = (struct pick_action_data *)data;
    if (!st)
	return;
    bsg_pick_result_free(st->res);
    bu_free(st, "pick action data");
}

bsg_action_ref
bsg_pick_ray_action_create(struct bsg_view *view,
			   const point_t orig,
			   const vect_t dir,
			   unsigned int flags)
{
    struct pick_action_data *st;
    BU_ALLOC(st, struct pick_action_data);
    memset(st, 0, sizeof(*st));
    st->res = bsg_pick_result_create();
    st->v = view;
    VMOVE(st->orig, orig);
    VMOVE(st->dir, dir);
    st->flags = flags;
    return bsg_action_ref_create_internal(bsg_pick_action_type(),
	    _pick_ray_action_apply, _pick_action_destroy, st, "pick action");
}

struct bsg_pick_result *
bsg_pick_action_result(bsg_action_ref action)
{
    struct pick_action_data *st =
	(struct pick_action_data *)bsg_action_ref_data(action);
    if (!st)
	return NULL;
    struct bsg_pick_result *res = st->res;
    st->res = NULL;
    return res;
}

struct bsg_pick_result *
bsg_pick_nearest_overlay_control(struct bsg_view *v, int x, int y,
				 unsigned long long role_mask)
{
    (void)role_mask;
    if (!v)
	return NULL;

    struct bsg_pick_result *candidates = bsg_pick_point(v, x, y, 0);
    if (!candidates)
	return NULL;

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res) {
	bsg_pick_result_free(candidates);
	return NULL;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&candidates->pr_records); i++) {
	const struct bsg_pick_record *pr =
	    (const struct bsg_pick_record *)BU_PTBL_GET(&candidates->pr_records, i);
	if (!pr || !pr->pr_scene.opaque)
	    continue;
	const struct bsg_draw_intent *di = _nearest_intent((const bsg_node *)pr->pr_scene.opaque);
	if (!di || !bsg_draw_intent_is_overlay(di))
	    continue;
	struct bsg_pick_record *copy = _record_clone(pr);
	if (copy)
	    bu_ptbl_ins(&res->pr_records, (long *)copy);
    }
    bsg_pick_result_free(candidates);
    _result_sort(res);

    if (BU_PTBL_LEN(&res->pr_records) > 1) {
	for (size_t i = 1; i < BU_PTBL_LEN(&res->pr_records); i++) {
	    struct bsg_pick_record *pr =
		(struct bsg_pick_record *)BU_PTBL_GET(&res->pr_records, i);
	    _record_free(pr);
	}
	res->pr_records.end = 1;
    }

    return res;
}

struct bsg_pick_result *
bsg_pick_semantic_path(struct bsg_view *v, const char *path_pattern)
{
    if (!v || !path_pattern || !*path_pattern)
	return NULL;

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res)
	return NULL;

    bsg_node *root = bsg_view_scene_root(v);
    if (!root)
	return res;

    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_draw_intent_match(root, path_pattern, &groups);
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	struct bsg_pick_record *pr = _record_create(g, v, -1, -1);
	if (!pr)
	    continue;
	bu_ptbl_ins(&res->pr_records, (long *)pr);
    }
    bu_ptbl_free(&groups);
    _result_sort(res);
    return res;
}


void
bsg_pick_apply(struct bsg_selection   *sel,
	       struct bsg_pick_result  *res,
	       bsg_pick_op              op)
{
    if (!sel || !res)
	return;

    bsg_interaction_apply_op iop = BSG_INTERACTION_APPLY_ADD;
    switch (op) {
	case BSG_PICK_OP_SET:
	    iop = BSG_INTERACTION_APPLY_SET;
	    break;
	case BSG_PICK_OP_REMOVE:
	    iop = BSG_INTERACTION_APPLY_REMOVE;
	    break;
	case BSG_PICK_OP_ADD:
	default:
	    iop = BSG_INTERACTION_APPLY_ADD;
	    break;
    }

    struct bsg_interaction_result *ir = bsg_interaction_from_pick_result(res);
    bsg_interaction_selection_apply(sel, ir, iop);
    bsg_interaction_result_free(ir);
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
