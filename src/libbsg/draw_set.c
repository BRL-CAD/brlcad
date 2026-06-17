/*                   D R A W _ S E T . C
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
/** @file libbsg/draw_set.c
 *
 * Pure-BSG draw-tree helpers with no dependency on GED types.
 *
 * These functions implement the tree-navigation layer used by GED drawing
 * transactions.  GED supplies command policy and database lookup context while
 * libbsg owns retained scene manipulation.
 *
 * Dependencies: libbsg lifecycle helpers, bv/defines.h, bu (bu_ptbl, bu_vls).
 * No librt, no libged.
 */

#include "common.h"

#include "bu/list.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/object.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"

#include "bsg/draw_ctx.h"
#include "bsg/draw_set.h"
#include "appearance_private.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "draw_set_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "payload_private.h"
#include "scene_object_private.h"
#include "scene_store_private.h"


/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int
_draw_node_is_group_like(const bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_group_type());
}


static int
_draw_node_is_shape_like(const bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_shape_type());
}

void
bsg_node_set_draw_ctx(struct bsg_node *node, struct bsg_draw_ctx *ctx)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->draw_ctx = ctx;
}

struct bsg_draw_ctx *
bsg_node_draw_ctx(const struct bsg_node *node)
{
    return (node && node->i) ? node->i->draw_ctx : NULL;
}

void
bsg_scene_set_draw_ctx(bsg_scene_ref ref, struct bsg_draw_ctx *ctx)
{
    bsg_node *node = (bsg_node *)ref.opaque;
    bsg_node_set_draw_ctx(node, ctx);
}

struct bsg_draw_ctx *
bsg_scene_draw_ctx(bsg_scene_ref ref)
{
    return bsg_node_draw_ctx((const bsg_node *)ref.opaque);
}

int
bsg_draw_tree_depth(const bsg_node *g)
{
    if (!g)
	return 0;

    int depth = 0;
    const bsg_node *cur = (const bsg_node *)g;
    while (cur->parent) {
	depth++;
	cur = (const bsg_node *)cur->parent;
    }
    return depth;
}

int
bsg_scene_draw_tree_depth(bsg_scene_ref ref)
{
    return bsg_draw_tree_depth((const bsg_node *)ref.opaque);
}


bsg_node *
bsg_group_find_child(bsg_node *parent, const char *name)
{
    if (!parent || !name)
	return NULL;

    bsg_node *p = (bsg_node *)parent;
    for (size_t i = 0; i < BU_PTBL_LEN(&p->children); i++) {
	bsg_node *c =
	    (bsg_node *)BU_PTBL_GET(&p->children, i);
	if (!c)
	    continue;
	if (_draw_node_is_group_like(c) &&
	    BU_STR_EQUAL(name, bsg_node_name(c)))
	    return (bsg_node *)c;
    }
    return NULL;
}

bsg_scene_ref
bsg_scene_group_find_child(bsg_scene_ref parent, const char *name)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = bsg_group_find_child((bsg_node *)parent.opaque, name);
    return ref;
}


bsg_node *
bsg_group_ensure_child(bsg_node *parent, struct bsg_view *v,
		       const char *name, void *dp_hint)
{
    if (!parent || !name)
	return NULL;

    /* Fast path: child already exists */
    bsg_node *existing = bsg_group_find_child(parent, name);
    if (existing)
	return existing;

    /* Need a view for allocation */
    if (!v)
	return NULL;
    (void)dp_hint;

    /* Allocate a new GROUP node through libbv. */
    bsg_node *child = bsg_scene_node_create(v, BSG_SOURCE_CHILD);
    if (!child)
	return NULL;

    bsg_node_set_object_type(child, bsg_group_type());
    bsg_appearance_set_highlighted(child, 0);
    bsg_node_set_visible_state(child, 1);
    bsg_node_set_name(child, name);
    bsg_node_add_child(parent, child);

    /* A new child invalidates the parent chain's cached aggregate bbox.
     * The new group itself is leaf-empty so its own cache (when computed
     * later) will simply read back as the empty box; clearing now on the
     * parent and above is what matters. */
    bsg_node_bbox_invalidate(parent);

    return (bsg_node *)child;
}

bsg_scene_ref
bsg_scene_group_ensure_child(bsg_scene_ref parent, struct bsg_view *v,
			     const char *name, void *dp_hint)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = bsg_group_ensure_child((bsg_node *)parent.opaque, v, name,
	    dp_hint);
    return ref;
}


/* ------------------------------------------------------------------ */
/* Free-group helpers                                                 */
/* ------------------------------------------------------------------ */

void
bsg_bump_rev_node(bsg_node *n)
{
    struct bsg_draw_ctx *ctx = _ctx_of_node(n);
    if (!ctx || !ctx->draw_rev)
	return;
    ++(*ctx->draw_rev);
}

void
bsg_scene_bump_rev(bsg_scene_ref ref)
{
    bsg_bump_rev_node((bsg_node *)ref.opaque);
}


void
bsg_free_children_recursive(bsg_node *gn, struct bsg_scene_recycle_pool *fso)
{
    bsg_node *g = (bsg_node *)gn;

    /* Removing children invalidates this group's aggregate bbox cache
     * and that of all of its ancestors. */
    if (BU_PTBL_LEN(&g->children) > 0)
	bsg_node_bbox_invalidate(gn);

    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&g->children); i++)
	bu_ptbl_ins(&snap, BU_PTBL_GET(&g->children, i));

    for (size_t i = 0; i < BU_PTBL_LEN(&snap); i++) {
	bsg_node *child =
	    (bsg_node *)BU_PTBL_GET(&snap, i);
	if (_draw_node_is_group_like(child)) {
	    bsg_free_children_recursive((bsg_node *)child, fso);
	    child->parent = NULL;
	    struct bsg_scene_recycle_pool *cfso = bsg_scene_node_recycle_pool(child);
	    if (cfso)
		bsg_scene_recycle_release(cfso, child);
	} else {
	    /* Fire per-object teardown callbacks before recycling. */
	    bsg_node_invoke_release_callback(child);
	    child->parent = NULL;
	    struct bsg_scene_recycle_pool *sfso =
		fso ? fso : bsg_scene_node_recycle_pool(child);
	    if (sfso)
		bsg_scene_recycle_release(sfso, child);
	}
    }
    bu_ptbl_free(&snap);
    bu_ptbl_reset(&g->children);
}


void
bsg_free_group_contents(bsg_node *gn)
{
    bsg_node *g = (bsg_node *)gn;
    if (!g || BU_PTBL_LEN(&g->children) == 0)
	return;

    /* Obtain the free-object pool pointer from the root draw-tree context.
     * Fall back to the group's own recycle pool if no context is present. */
    struct bsg_draw_ctx *ctx = _ctx_of_node(g);
    struct bsg_scene_recycle_pool *fso = (ctx && ctx->fso) ?
	(struct bsg_scene_recycle_pool *)ctx->fso :
	bsg_scene_node_recycle_pool(g);

    bsg_free_children_recursive(gn, fso);
}

void
bsg_scene_free_group_contents(bsg_scene_ref ref)
{
    bsg_free_group_contents((bsg_node *)ref.opaque);
}


void
bsg_free_group(bsg_node *gn)
{
    bsg_node *g = (bsg_node *)gn;
    if (!g)
	return;

    bsg_free_group_contents(gn);

    bsg_node *parent = (bsg_node *)g->parent;
    if (parent)
	bu_ptbl_rm(&parent->children, (const long *)g);

    /* g->parent is still set at this point; walk up to root for ctx */
    bsg_bump_rev_node(gn);
    /* Removing this subtree invalidates ancestors' bbox caches. */
    bsg_node_bbox_invalidate(gn);

    g->parent = NULL;
    struct bsg_scene_recycle_pool *fso = bsg_scene_node_recycle_pool(g);
    if (fso)
	bsg_scene_recycle_release(fso, g);
}

void
bsg_scene_free_group(bsg_scene_ref ref)
{
    bsg_free_group((bsg_node *)ref.opaque);
}


/* ------------------------------------------------------------------ */
/* Path-navigation erase helper                                       */
/* ------------------------------------------------------------------ */

void
bsg_erase_nested_subpath(bsg_node *parent_node,
			 const char * const *comp_names, size_t comp_count,
			 size_t depth_start,
			 bsg_path_match_fn match_fn, void *match_ctx)
{
    if (!parent_node || !comp_names || comp_count == 0 || depth_start >= comp_count)
	return;

    bsg_node *cur = (bsg_node *)parent_node;

    for (size_t i = depth_start; i < comp_count; i++) {
	const char *comp = comp_names[i];
	bsg_node *child_group = NULL;

	for (size_t j = 0; j < BU_PTBL_LEN(&cur->children); j++) {
	    bsg_node *c =
		(bsg_node *)BU_PTBL_GET(&cur->children, j);
	    if (_draw_node_is_group_like(c) &&
		BU_STR_EQUAL(bsg_node_name(c), comp)) {
		child_group = c;
		break;
	    }
	}

	if (i < comp_count - 1) {
	    /* Intermediate component — must be a group */
	    if (!child_group)
		return;
	    cur = child_group;
	    continue;
	}

	/* Final component */
	if (child_group) {
	    /* Case (a): a group node with this name — free its entire subtree */
	    bsg_free_group_contents((bsg_node *)child_group);
	    bu_ptbl_rm(&cur->children, (const long *)child_group);
	    /* cur is still in the tree; bump rev before clearing parent */
	    bsg_bump_rev_node((bsg_node *)cur);
	    /* Shrinking cur invalidates its and ancestors' bbox caches. */
	    bsg_node_bbox_invalidate((bsg_node *)cur);
	    child_group->parent = NULL;
	    struct bsg_scene_recycle_pool *cfso =
		bsg_scene_node_recycle_pool(child_group);
	    if (cfso)
		bsg_scene_recycle_release(cfso, child_group);
	} else {
	    /* Case (b): the final component is a leaf primitive — erase
	     * matching BSG_NODE_SHAPE children by calling match_fn. */
	    struct bsg_draw_ctx *ctx = _ctx_of_node(cur);
	    struct bsg_scene_recycle_pool *fso = (ctx && ctx->fso) ?
		(struct bsg_scene_recycle_pool *)ctx->fso : NULL;

	    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
	    for (size_t j = 0; j < BU_PTBL_LEN(&cur->children); j++) {
		bsg_node *c =
		    (bsg_node *)BU_PTBL_GET(&cur->children, j);
		if (!_draw_node_is_shape_like(c))
		    continue;
		if (!match_fn || match_fn(c, match_ctx))
		    bu_ptbl_ins(&snap, (long *)c);
	    }
	    for (size_t j = 0; j < BU_PTBL_LEN(&snap); j++) {
		bsg_node *sp =
		    (bsg_node *)BU_PTBL_GET(&snap, j);
		bsg_node_invoke_release_callback(sp);
		bu_ptbl_rm(&cur->children, (const long *)sp);
		/* cur is in the tree; bump rev then clear parent */
		bsg_bump_rev_node((bsg_node *)cur);
		/* Shrinking cur invalidates its and ancestors' bbox caches. */
		bsg_node_bbox_invalidate((bsg_node *)cur);
		sp->parent = NULL;
		struct bsg_scene_recycle_pool *sfso =
		    fso ? fso : bsg_scene_node_recycle_pool(sp);
		if (sfso)
		    bsg_scene_recycle_release(sfso, sp);
	    }
	    bu_ptbl_free(&snap);
	}
	return;
    }
}

struct bsg_scene_erase_adapter {
    bsg_scene_path_match_fn match_fn;
    void *match_ctx;
};

static int
_bsg_scene_erase_match_adapter(struct bsg_node *shape_node, void *ctx)
{
    struct bsg_scene_erase_adapter *adapter =
	(struct bsg_scene_erase_adapter *)ctx;
    if (!adapter || !adapter->match_fn)
	return 1;
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = shape_node;
    return adapter->match_fn(ref, adapter->match_ctx);
}

void
bsg_scene_erase_nested_subpath(bsg_scene_ref parent,
			       const char * const *comp_names,
			       size_t comp_count,
			       size_t depth_start,
			       bsg_scene_path_match_fn match_fn,
			       void *match_ctx)
{
    struct bsg_scene_erase_adapter adapter;
    adapter.match_fn = match_fn;
    adapter.match_ctx = match_ctx;
    bsg_erase_nested_subpath((bsg_node *)parent.opaque, comp_names, comp_count,
	    depth_start, match_fn ? _bsg_scene_erase_match_adapter : NULL,
	    match_fn ? (void *)&adapter : match_ctx);
}


/* ------------------------------------------------------------------ */
/* Subtree bbox cache                                                 */
/* ------------------------------------------------------------------ */

void
bsg_node_bbox_cache_clear(bsg_node *n)
{
    if (!n || !n->i)
	return;
    n->i->bbox_cache.valid = 0;
}

void
bsg_node_bbox_cache_set(bsg_node *n, const point_t min, const point_t max)
{
    if (!n || !min || !max)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(n);
    VMOVE(ni->bbox_cache.min, min);
    VMOVE(ni->bbox_cache.max, max);
    ni->bbox_cache.valid = 1;
}

int
bsg_node_bbox_cache_get(const bsg_node *n, point_t min, point_t max)
{
    if (!n || !n->i || !n->i->bbox_cache.valid)
	return 0;
    if (min)
	VMOVE(min, n->i->bbox_cache.min);
    if (max)
	VMOVE(max, n->i->bbox_cache.max);
    return 1;
}

void
bsg_node_bbox_invalidate(bsg_node *n)
{
    if (!n)
	return;
    bsg_node *cur = (bsg_node *)n;

    /* Walk up the parent chain and clear cached aggregate bounds on every ancestor.
     * A newly-created or otherwise already-dirty child can still be attached
     * below a cached ancestor, so invalidation must not stop at the first
     * dirty node.
     *
     * The starting node itself is included in the walk: if @p n is a
     * leaf shape we skip to its parent (leaves have no aggregate
     * cache); otherwise we clear @p n too.
     */
    while (cur) {
	if (_draw_node_is_group_like(cur)) {
	    bsg_node_bbox_cache_clear(cur);
	}
	cur = (bsg_node *)cur->parent;
    }
}

void
bsg_scene_bbox_invalidate(bsg_scene_ref ref)
{
    bsg_node_bbox_invalidate((bsg_node *)ref.opaque);
}


/*
 * Internal: aggregate the bbox of a single shape leaf into (*min, *max).
 * Uses the draw-extent center/radius pair to match the historical
 * bounding-sphere behaviour exactly when no canonical bounds field is present.
 */
static void
_bsg_shape_bbox_accum(const bsg_node *sp,
		      vect_t *min, vect_t *max)
{
    vect_t lmin, lmax;
    point_t bmin, bmax;
    if (bsg_node_bounds(sp, bmin, bmax)) {
	point_t center;
	fastf_t size = bmax[X] - bmin[X];
	V_MAX(size, bmax[Y] - bmin[Y]);
	V_MAX(size, bmax[Z] - bmin[Z]);
	VSET(center,
		(bmin[X] + bmax[X]) * 0.5,
		(bmin[Y] + bmax[Y]) * 0.5,
		(bmin[Z] + bmax[Z]) * 0.5);
	VSET(lmin, center[X] - size, center[Y] - size, center[Z] - size);
	VSET(lmax, center[X] + size, center[Y] + size, center[Z] + size);
    } else {
	point_t center;
	fastf_t size = bsg_node_draw_size(sp);
	bsg_node_get_draw_center(sp, center);
	VSET(lmin, center[X] - size, center[Y] - size, center[Z] - size);
	VSET(lmax, center[X] + size, center[Y] + size, center[Z] + size);
    }
    VMIN(*min, lmin);
    VMAX(*max, lmax);
}


/*
 * Internal recursive aggregator.  Returns 1 when the subtree contributes
 * something to the bbox, 0 when it is empty.  When @p cache is non-zero
 * the caller is in the cached (no-overlay) mode and we may store the
 * aggregate at GROUP/ROOT nodes.
 */
static int
_bsg_subtree_bbox_walk(bsg_node *n,
		       vect_t *min, vect_t *max,
		       int include_overlays, int cache)
{
    if (!n)
	return 0;

    if (_draw_node_is_shape_like(n)) {
	if (!include_overlays && (bsg_node_get_payload_type(n) & BSG_PAYLOAD_OVERLAY))
	    return 0;
	_bsg_shape_bbox_accum(n, min, max);
	return 1;
    }

    if (!_draw_node_is_group_like(n))
	return 0;

    /* Cache hit: in cached mode and this group's aggregate is current. */
    point_t cached_min, cached_max;
    if (cache && bsg_node_bbox_cache_get(n, cached_min, cached_max)) {
	/* An empty subtree caches as an empty box (bmin=+INF, bmax=-INF).
	 * Detect that by checking VMIN(bmin, bmax) — accumulating an
	 * empty box is a no-op. */
	if (cached_min[X] <= cached_max[X]) {
	    VMIN(*min, cached_min);
	    VMAX(*max, cached_max);
	    return 1;
	}
	return 0;
    }

    /* Recompute: aggregate over children into a local min/max so we can
     * cache the result for this subtree before merging into the caller's
     * accumulator. */
    vect_t lmin, lmax;
    VSETALL(lmin,  INFINITY);
    VSETALL(lmax, -INFINITY);
    int contributed = 0;

    for (size_t i = 0; i < BU_PTBL_LEN(&n->children); i++) {
	bsg_node *c =
	    (bsg_node *)BU_PTBL_GET(&n->children, i);
	if (!c)
	    continue;
	if (_bsg_subtree_bbox_walk(c, &lmin, &lmax, include_overlays, cache))
	    contributed = 1;
    }

    if (cache) {
	/* Store local result into the node cache.  Empty subtrees cache
	 * the (+INF, -INF) sentinel pair so a subsequent hit detects them. */
	bsg_node_bbox_cache_set(n, lmin, lmax);
    }

    if (contributed) {
	VMIN(*min, lmin);
	VMAX(*max, lmax);
    }
    return contributed;
}


int
bsg_subtree_bbox(bsg_node *n,
		 vect_t *min, vect_t *max,
		 int include_overlays)
{
    if (!min || !max)
	return 1;

    VSETALL((*min),  INFINITY);
    VSETALL((*max), -INFINITY);

    if (!n)
	return 1;

    int cache = include_overlays ? 0 : 1;
    int contributed =
	_bsg_subtree_bbox_walk((bsg_node *)n,
			       min, max, include_overlays, cache);

    /* Return 1 for empty (matches _sg_bounding_sph contract). */
    return contributed ? 0 : 1;
}

int
bsg_scene_subtree_bbox(bsg_scene_ref ref,
		       vect_t *min, vect_t *max,
		       int include_overlays)
{
    return bsg_subtree_bbox((bsg_node *)ref.opaque, min, max,
	    include_overlays);
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
