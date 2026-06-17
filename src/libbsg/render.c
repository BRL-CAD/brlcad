/*                     R E N D E R . C
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
/** @file libbsg/render.c
 *
 * BSG render-request — pre-render traversal, render-item production, and
 * phase-ordered dispatch via backend adapter or typed payload realization.
 *
 * Traversal overview
 * ------------------
 * bsg_action_traverse() is now an internal collection detail.  It walks the
 * view stream and supplies a semantic action
 * state containing transform, visibility, inherited appearance, view-scope,
 * and LoD.  For each drawable shape it:
 *   1. Checks visibility (BSG_RENDER_FLAG_VISIBLE_ONLY).
 *   2. Realizes attached typed payloads when requested.
 *   3. Resolves appearance from render defaults, material, inherited group
 *      settings, local node settings, and highlight state.
 *   4. Classifies the item into one of the four bsg_render_phase buckets.
 *   5. Allocates a bsg_render_item and appends it to the appropriate
 *      phase bucket (bu_ptbl).
 *
 * After collection, items are stored in a bsg_render_batch.  Transparent items
 * are sorted back-to-front when
 * BSG_RENDER_FLAG_SORTED_ALPHA is set.  The current sort key uses the
 * transformed node origin as an approximate depth proxy; payload-specific
 * depth sorting can refine this in a later slice.
 *
 * Phase-ordered dispatch
 * ----------------------
 * Items are dispatched from the retained batch in phase order:
 * OPAQUE → TRANSPARENT → OVERLAY → HUD.  Per item, if req->adapter is set:
 * adapter->prepare() + adapter->draw().  BSG_RENDER_FLAG_COLLECT_ITEMS is
 * retained only for bsg_render_request_execute() compatibility; new callers
 * use bsg_render_request_collect() and own the returned batch.
 */

#include "common.h"

#include <limits.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "vmath.h"
#include "bn/mat.h"

#include "bsg/defines.h"
#include "bsg/action.h"
#include "bsg/draw_intent.h"
#include "bsg/payload.h"
#include "payload_typed_private.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/render_settings.h"
#include "bsg/state.h"
#include "bsg/backend_adapter.h"
#include "bsg/selection.h"
#include "bsg/hud.h"
#include "bsg/overlay.h"
#include "bsg/appearance.h"
#include "draw_source_private.h"
#include "bsg/appearance_action.h"
#include "bsg_private.h"
#include "bsg/draw_source.h"
#include "bsg/geometry.h"
#include "bsg/interaction.h"
#include "bsg/node.h"
#include "node_private.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bsg/scene_object.h"
#include "action_private.h"
#include "appearance_private.h"
#include "draw_intent_private.h"
#include "field_private.h"
#include "hud_private.h"
#include "node_storage_private.h"
#include "geometry_private.h"
#include "lod_private.h"
#include "object_private.h"
#include "overlay_private.h"
#include "payload_private.h"


/* ------------------------------------------------------------------ */
/* Internal collection state                                            */
/* ------------------------------------------------------------------ */

struct bsg_render_request {
    struct bsg_view *view;
    void *dmp;
    unsigned int flags;
    struct bsg_backend_adapter *adapter;
    struct bu_ptbl *items;
    struct bsg_render_settings *settings;
    struct bsg_render_context batch;
};

struct bsg_render_batch {
    struct bsg_render_context context;
    struct bu_ptbl items;
};

struct collect_state {
    const struct bsg_render_request *req;
    size_t next_sequence;
    /* Per-phase item buckets (indexed by bsg_render_phase) */
    struct bu_ptbl phase_items[BSG_RENDER_PHASE_COUNT];
};

static bsg_node *
_request_render_root(const struct bsg_render_request *req)
{
    if (!req || !req->view)
	return NULL;
    if (req->flags & BSG_RENDER_FLAG_HUD_PASS)
	return (bsg_node *)bsg_hud_root_get(req->view);
    return bsg_view_scene_root(req->view);
}

/* Preserve six decimal places of view-space depth when projecting the
 * floating-point Z value into the integer sort_key field. */
static const fastf_t depth_key_scale_factor = 1000000.0;

static const char *
_nearest_draw_intent_path(const struct bsg_node *node)
{
    const struct bsg_node *n = node;
    while (n) {
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(n);
	const char *path = di ? bsg_draw_intent_path(di) : NULL;
	if (path && path[0])
	    return path;
	n = bsg_node_parent((struct bsg_node *)n);
    }
    return NULL;
}

/**
 * Resolve the render phase for a shape node.
 *
 * Priority (highest to lowest):
 *   1. BSG_PAYLOAD_OVERLAY → OVERLAY or HUD (HUD if node is a HUD child)
 *   2. BSG_RENDER_FLAG_HUD_PASS request → HUD
 *   3. transparency < 1.0 → TRANSPARENT
 *   4. default → OPAQUE
 */
static bsg_render_phase
_classify_phase(const struct bsg_render_request *req,
		const bsg_node *node,
		fastf_t transparency)
{
    unsigned long long ptype = bsg_node_get_payload_type((bsg_node *)node);

    if ((ptype & BSG_PAYLOAD_OVERLAY) || bsg_overlay_info_get(node)) {
	/* HUD pass flag promotes overlays into the HUD phase */
	if (req->flags & BSG_RENDER_FLAG_HUD_PASS) {
	    const struct bsg_hud_node_meta *meta =
		bsg_hud_node_get_meta((bsg_node *)node);
	    if (meta)
		return BSG_RENDER_PHASE_HUD;
	}
	return BSG_RENDER_PHASE_OVERLAY;
    }

    if (transparency < 1.0)
	return BSG_RENDER_PHASE_TRANSPARENT;

    return BSG_RENDER_PHASE_OPAQUE;
}

static void
_apply_traversal_appearance(const struct bsg_action_state *state,
			    struct bsg_resolved_appearance *ra)
{
    if (!state || !ra || bsg_state_ref_is_null(state->state))
	return;
    if (bsg_state_ref_has_material(state->state)) {
	bsg_state_ref_material_color(state->state, ra->color);
	ra->transparency = bsg_state_ref_material_transparency(state->state);
	ra->material_rev = bsg_state_ref_material_revision(state->state);
	ra->appearance_rev ^= bsg_state_ref_appearance_revision(state->state);
    }
    if (bsg_state_ref_has_draw_style(state->state)) {
	ra->draw_mode = bsg_state_ref_draw_mode(state->state);
	ra->line_width = bsg_state_ref_line_width(state->state);
	ra->line_style = bsg_state_ref_line_style(state->state);
	ra->appearance_rev ^= (uint32_t)bsg_state_ref_line_width(state->state);
	ra->appearance_rev ^= (uint32_t)(bsg_state_ref_line_style(state->state) << 8);
	ra->appearance_rev ^= (uint32_t)(bsg_state_ref_draw_mode(state->state) << 16);
    }
}


/**
 * Resolve the sort_key for @p item.
 *
 * For HUD items: use bsg_hud_node_meta::sort_order.
 * For transparent items: use the negated view-space Z of the transformed
 * node origin so larger values sort farther items first (back-to-front).
 * For all others: 0.
 */
static int
_sort_key(const struct bsg_render_request *req,
	  const bsg_node *node,
	  const struct bsg_render_item *item)
{
    if (item->phase == BSG_RENDER_PHASE_HUD) {
	const struct bsg_hud_node_meta *meta =
	    bsg_hud_node_get_meta((bsg_node *)node);
	if (meta)
	    return meta->sort_order;
    }

    if (item->phase == BSG_RENDER_PHASE_OVERLAY) {
	const struct bsg_overlay_info *info =
	    bsg_overlay_info_get(node);
	if (info)
	    return ((int)info->ordering * 1000) + info->sort_order;
    }

    if (item->phase == BSG_RENDER_PHASE_TRANSPARENT && req && req->view) {
	mat_t view_mat;
	point_t model_origin = VINIT_ZERO;
	point_t view_origin;
	fastf_t depth_key;

	bn_mat_mul(view_mat, req->view->gv_model2view, item->model_mat);
	MAT4X3PNT(view_origin, view_mat, model_origin);
	/* In BRL-CAD view space, geometry in front of the camera has negative Z.
	 * Negating Z makes farther items larger so descending sort order yields
	 * a back-to-front transparent draw sequence. */
	depth_key = -view_origin[Z] * depth_key_scale_factor;

	if (depth_key >= (fastf_t)INT_MAX)
	    return INT_MAX;
	if (depth_key <= (fastf_t)INT_MIN)
	    return INT_MIN;
	return (int)depth_key;
    }

    return 0;
}


static int
_transparent_item_cmp(const void *a, const void *b, void *UNUSED(context))
{
    const struct bsg_render_item *ia =
	*(const struct bsg_render_item * const *)a;
    const struct bsg_render_item *ib =
	*(const struct bsg_render_item * const *)b;

    if (ia->sort_key > ib->sort_key)
	return -1;
    if (ia->sort_key < ib->sort_key)
	return 1;
    return 0;
}


static void
_sort_transparent_bucket(struct bu_ptbl *bucket)
{
    if (!bucket || BU_PTBL_LEN(bucket) < 2)
	return;

    bu_sort(BU_PTBL_BASEADDR(bucket),
	    BU_PTBL_LEN(bucket),
	    sizeof(void *),
	    _transparent_item_cmp,
	    NULL);
}


static int
_hud_item_cmp(const void *a, const void *b, void *UNUSED(context))
{
    const struct bsg_render_item *ia =
	*(const struct bsg_render_item * const *)a;
    const struct bsg_render_item *ib =
	*(const struct bsg_render_item * const *)b;

    if (ia->sort_key < ib->sort_key)
	return -1;
    if (ia->sort_key > ib->sort_key)
	return 1;
    return 0;
}


static int
_polygon_display_item(const struct bsg_render_item *item)
{
    return (item && (item->order_flags & BSG_RENDER_ORDER_OPAQUE_CHILD_FIRST)) ? 1 : 0;
}


static int
_opaque_item_cmp(const void *a, const void *b, void *UNUSED(context))
{
    const struct bsg_render_item *ia =
	*(const struct bsg_render_item * const *)a;
    const struct bsg_render_item *ib =
	*(const struct bsg_render_item * const *)b;

    if (_polygon_display_item(ia) && _polygon_display_item(ib)) {
	if (ia->graph_depth > ib->graph_depth)
	    return -1;
	if (ia->graph_depth < ib->graph_depth)
	    return 1;
    }
    if (ia->sequence < ib->sequence)
	return -1;
    if (ia->sequence > ib->sequence)
	return 1;
    return 0;
}


static void
_sort_opaque_bucket(struct bu_ptbl *bucket)
{
    if (!bucket || BU_PTBL_LEN(bucket) < 2)
	return;

    bu_sort(BU_PTBL_BASEADDR(bucket),
	    BU_PTBL_LEN(bucket),
	    sizeof(void *),
	    _opaque_item_cmp,
	    NULL);
}


static void
_sort_hud_bucket(struct bu_ptbl *bucket)
{
    if (!bucket || BU_PTBL_LEN(bucket) < 2)
	return;

    bu_sort(BU_PTBL_BASEADDR(bucket),
	    BU_PTBL_LEN(bucket),
	    sizeof(void *),
	    _hud_item_cmp,
	    NULL);
}


static void
_sort_overlay_bucket(struct bu_ptbl *bucket)
{
    if (!bucket || BU_PTBL_LEN(bucket) < 2)
	return;

    bu_sort(BU_PTBL_BASEADDR(bucket),
	    BU_PTBL_LEN(bucket),
	    sizeof(void *),
	    _hud_item_cmp,
	    NULL);
}


static int
_node_depth(const bsg_node *node)
{
    int depth = 0;
    for (const bsg_node *n = node; n && n->parent; n = n->parent)
	depth++;
    return depth;
}


static int
_resolve_item_bounds(const bsg_node *node,
		     const struct bsg_payload_realization *realization,
		     struct bsg_payload *payload,
		     point_t center,
		     fastf_t *radius)
{
    point_t bmin, bmax;
    vect_t diag;

    if (!center || !radius)
	return 0;

    VSETALL(center, 0.0);
    *radius = 0.0;

    if (node && node->i && node->i->draw_extent.valid) {
	bsg_node_get_draw_center(node, center);
	*radius = bsg_node_draw_size(node);
	return 1;
    }

    if (node && bsg_node_bounds(node, bmin, bmax)) {
	VADD2(center, bmin, bmax);
	VSCALE(center, center, 0.5);
	VSUB2(diag, bmax, bmin);
	*radius = 0.5 * MAGNITUDE(diag);
	return 1;
    }

    if (realization && realization->status == BSG_REALIZE_STATUS_CURRENT &&
	    realization->has_bounds) {
	VADD2(center, realization->bmin, realization->bmax);
	VSCALE(center, center, 0.5);
	VSUB2(diag, realization->bmax, realization->bmin);
	*radius = 0.5 * MAGNITUDE(diag);
	return 1;
    }

    if (payload && payload->pl_bounds &&
	    payload->pl_bounds(payload, &bmin, &bmax)) {
	VADD2(center, bmin, bmax);
	VSCALE(center, center, 0.5);
	VSUB2(diag, bmax, bmin);
	*radius = 0.5 * MAGNITUDE(diag);
	return 1;
    }

    if (realization && realization->has_bounds) {
	VADD2(center, realization->bmin, realization->bmax);
	VSCALE(center, center, 0.5);
	VSUB2(diag, realization->bmax, realization->bmin);
	*radius = 0.5 * MAGNITUDE(diag);
	return 1;
    }

    return 0;
}


static uint64_t
_cache_hash_bytes(uint64_t h, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < len; i++) {
	h ^= (uint64_t)p[i];
	h *= 1099511628211ULL;
    }
    return h;
}


#define CACHE_HASH_VALUE(_h, _v) \
    _cache_hash_bytes((_h), &(_v), sizeof(_v))

static uint64_t
_hash_cstr(uint64_t h, const char *str)
{
    if (!str)
	return CACHE_HASH_VALUE(h, str);
    return _cache_hash_bytes(h, str, strlen(str) + 1);
}

static uint64_t
_settings_hash(const struct bsg_render_settings *settings)
{
    uint64_t h = 1469598103934665603ULL;
    if (!settings)
	return h;
    return _cache_hash_bytes(h, settings, sizeof(*settings));
}

static uint64_t
_lod_policy_identity(const struct bsg_render_settings *settings,
		     bsg_render_geometry_role geometry_role,
		     bsg_render_geometry_kind geometry_kind,
		     int lod_level)
{
    uint64_t h = 1469598103934665603ULL;
    if (!settings)
	return 0;
    if (!settings->lod_source_policy.mesh_enabled &&
	    !settings->lod_source_policy.csg_enabled &&
	    settings->lod_source_policy.policy == BSG_LOD_AUTO)
	return 0;
    h = _cache_hash_bytes(h, &settings->lod_source_policy,
	    sizeof(settings->lod_source_policy));
    h = CACHE_HASH_VALUE(h, geometry_role);
    h = CACHE_HASH_VALUE(h, geometry_kind);
    h = CACHE_HASH_VALUE(h, lod_level);
    return h ? h : 1;
}

static uint64_t
_render_source_id(const bsg_node *node,
		  const struct bsg_payload *payload,
		  const char *name,
		  bsg_render_source_scope scope,
		  bsg_render_geometry_role geometry_role,
		  bsg_render_draw_intent draw_intent,
		  int non_database_source)
{
    uint64_t h = 1469598103934665603ULL;
    uintptr_t payload_identity = (uintptr_t)payload;
    h = _hash_cstr(h, name);
    h = CACHE_HASH_VALUE(h, scope);
    h = CACHE_HASH_VALUE(h, geometry_role);
    h = CACHE_HASH_VALUE(h, draw_intent);
    h = CACHE_HASH_VALUE(h, non_database_source);
    if (payload_identity)
	h = CACHE_HASH_VALUE(h, payload_identity);
    else {
	uintptr_t node_id = (uintptr_t)node;
	h = CACHE_HASH_VALUE(h, node_id);
    }
    return h ? h : 1;
}

static void
_populate_batch_context(struct bsg_render_request *req)
{
    if (!req)
	return;
    memset(&req->batch, 0, sizeof(req->batch));
    MAT_IDN(req->batch.model2view);
    MAT_IDN(req->batch.view2model);
    MAT_IDN(req->batch.projection);
    VSET(req->batch.clipmin, -1.0, -1.0, -1.0);
    VSET(req->batch.clipmax, 1.0, 1.0, 1.0);
    req->batch.request_flags = req->flags;
    req->batch.settings_hash = _settings_hash(req->settings);
    if (req->settings) {
	req->batch.settings = *req->settings;
	req->batch.zclip = req->settings->zclip;
    }
    if (req->view) {
	MAT_COPY(req->batch.model2view, req->view->gv_model2view);
	MAT_COPY(req->batch.view2model, req->view->gv_view2model);
	MAT_COPY(req->batch.projection, req->view->gv_pmat);
	req->batch.has_projection = (SMALL_FASTF < req->view->gv_perspective);
	req->batch.viewport_width = req->view->gv_width;
	req->batch.viewport_height = req->view->gv_height;
    }
    req->batch.hud_pass = (req->flags & BSG_RENDER_FLAG_HUD_PASS) ? 1 : 0;
}

static uint64_t
_cache_identity(const struct bsg_render_item *item)
{
    uint64_t h = 1469598103934665603ULL;
    h = CACHE_HASH_VALUE(h, item->source.source_id);
    h = CACHE_HASH_VALUE(h, item->source.geometry_id);
    h = CACHE_HASH_VALUE(h, item->payload_revision);
    h = CACHE_HASH_VALUE(h, item->source.lod_id);
    h = CACHE_HASH_VALUE(h, item->source.lod_policy_id);
    h = CACHE_HASH_VALUE(h, item->source.scope);
    h = CACHE_HASH_VALUE(h, item->source.geometry_role);
    h = CACHE_HASH_VALUE(h, item->source.draw_intent);
    h = CACHE_HASH_VALUE(h, item->source.feature_family);
    h = CACHE_HASH_VALUE(h, item->source.overlay_role);
    h = CACHE_HASH_VALUE(h, item->source.overlay_class);
    h = CACHE_HASH_VALUE(h, item->geometry.kind);
    h = CACHE_HASH_VALUE(h, item->geometry.source_identity);
    h = CACHE_HASH_VALUE(h, item->geometry.revision);
    h = CACHE_HASH_VALUE(h, item->features.flags);
    h = CACHE_HASH_VALUE(h, item->non_database_source);
    h = CACHE_HASH_VALUE(h, item->lod_level);
    h = CACHE_HASH_VALUE(h, item->lod_level_count);
    h = CACHE_HASH_VALUE(h, item->context.settings_hash);
    return h ? h : 1;
}

#undef CACHE_HASH_VALUE

static int _copy_geometry_point_field(bsg_field_ref field, point_t **points, size_t *point_count);
static int _copy_geometry_int_field(bsg_field_ref field, int **values, size_t *value_count, const char *what);
static int _copy_payload_line_set_geometry(const struct bsg_payload_line_set *ls,
	struct bsg_render_geometry *geometry);
static int _copy_label_geometry(const struct bsg_label *label,
	struct bsg_render_geometry *geometry);
static int _copy_overlay_geometry(const struct bsg_payload *payload,
	struct bsg_render_geometry *geometry);
static int _copy_image_payload_geometry(const struct bsg_payload *payload,
	struct bsg_render_geometry *geometry,
	int payload_identity);
static int _copy_image_field_geometry(const bsg_node *node,
	struct bsg_render_geometry *geometry);
static int _copy_annotation_geometry(const struct bsg_payload_annotation *annotation,
	struct bsg_render_geometry *geometry);
static int _copy_indexed_surface_geometry(struct bsg_render_geometry *geometry);
static void _set_proxy_metadata(struct bsg_render_geometry *geometry,
	const struct bsg_payload *payload,
	bsg_render_proxy_kind kind);

static void
_resolve_item_geometry_node(const bsg_node *node,
			    struct bsg_render_item *item,
			    struct bsg_payload *payload)
{
    bsg_geometry_node_kind kind;
    uint64_t revision;

    if (!node || !item || !bsg_node_type_is_a(node, bsg_geometry_type()))
	return;

    kind = bsg_geometry_node_kind_get(node);
    if (kind == BSG_GEOMETRY_NODE_NONE)
	return;

    revision = bsg_geometry_node_revision(node);
    item->geometry.revision = revision ? revision : bsg_node_revision(node);
    item->geometry.source_identity = ((uint64_t)(uintptr_t)node) ^ item->geometry.revision;

    switch (kind) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
	    (void)_copy_geometry_point_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES),
		    &item->geometry.arrays.points,
		    &item->geometry.arrays.point_count);
	    (void)_copy_geometry_int_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS),
		    &item->geometry.arrays.commands,
		    &item->geometry.arrays.command_count,
		    "bsg render geometry commands");
	    item->geometry.element_count = item->geometry.arrays.point_count;
	    break;
	case BSG_GEOMETRY_NODE_INDEXED_LINE_SET:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_INDEXED_LINE_SET;
	    (void)_copy_geometry_point_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES),
		    &item->geometry.arrays.points,
		    &item->geometry.arrays.point_count);
	    (void)_copy_geometry_int_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_INDICES),
		    &item->geometry.arrays.indices,
		    &item->geometry.arrays.index_count,
		    "bsg render geometry indices");
	    item->geometry.element_count = item->geometry.arrays.index_count;
	    break;
	case BSG_GEOMETRY_NODE_POINT_SET:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_POINT_SET;
	    (void)_copy_geometry_point_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES),
		    &item->geometry.arrays.points,
		    &item->geometry.arrays.point_count);
	    (void)_copy_geometry_int_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS),
		    &item->geometry.arrays.commands,
		    &item->geometry.arrays.command_count,
		    "bsg render geometry commands");
	    item->geometry.element_count = item->geometry.arrays.point_count;
	    break;
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_INDEXED_FACE_SET;
	    (void)_copy_geometry_point_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES),
		    &item->geometry.arrays.points,
		    &item->geometry.arrays.point_count);
	    (void)_copy_geometry_point_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_NORMALS),
		    &item->geometry.arrays.normals,
		    &item->geometry.arrays.normal_count);
	    (void)_copy_geometry_int_field(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_INDICES),
		    &item->geometry.arrays.indices,
		    &item->geometry.arrays.index_count,
		    "bsg render geometry indices");
	    item->geometry.element_count = item->geometry.arrays.index_count;
	    (void)_copy_indexed_surface_geometry(&item->geometry);
	    break;
	case BSG_GEOMETRY_NODE_MESH:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_MESH;
	    item->geometry.data.mesh = payload ? bsg_payload_mesh_get(payload) : NULL;
	    item->geometry.revision ^= (uint64_t)(unsigned int)(item->lod_level + 1) * 1099511628211ULL;
	    break;
	case BSG_GEOMETRY_NODE_TEXT:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_TEXT;
	    item->geometry.element_count = bsg_field_multi_count(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES));
	    {
		const struct bsg_label *label = payload ? payload->pl.text : NULL;
		(void)_copy_label_geometry(label, &item->geometry);
		item->geometry.source_identity = (uint64_t)(uintptr_t)label;
	    }
	    break;
	case BSG_GEOMETRY_NODE_IMAGE:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_IMAGE;
	    if (!_copy_image_payload_geometry(payload, &item->geometry, 0))
		(void)_copy_image_field_geometry(node, &item->geometry);
	    break;
	case BSG_GEOMETRY_NODE_FRAMEBUFFER:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_IMAGE;
	    if (!_copy_image_payload_geometry(payload, &item->geometry, 0))
		(void)_copy_image_field_geometry(node, &item->geometry);
	    break;
	case BSG_GEOMETRY_NODE_OVERLAY:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_OVERLAY;
	    if (!_copy_overlay_geometry(payload, &item->geometry))
		_set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_EXTERNAL);
	    break;
	case BSG_GEOMETRY_NODE_HUD:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_HUD;
	    item->geometry.element_count = bsg_field_multi_count(
		    bsg_node_field_ref((bsg_node *)node, BSG_FIELD_GEOMETRY_COORDINATES));
	    {
		const struct bsg_label *label = payload ? payload->pl.hud_text : NULL;
		(void)_copy_label_geometry(label, &item->geometry);
		item->geometry.source_identity = (uint64_t)(uintptr_t)label;
	    }
	    break;
	case BSG_GEOMETRY_NODE_ANNOTATION:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_ANNOTATION;
	    (void)_copy_annotation_geometry(payload ? payload->pl.annotation : NULL,
		    &item->geometry);
	    item->geometry.source_identity = (uint64_t)(uintptr_t)(payload ? payload->pl.annotation : NULL);
	    break;
	case BSG_GEOMETRY_NODE_CSG_PROXY:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_CSG_PROXY;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_CSG);
	    break;
	case BSG_GEOMETRY_NODE_BREP_PROXY:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_BREP_PROXY;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_BREP);
	    break;
	case BSG_GEOMETRY_NODE_EDIT_PREVIEW:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_EDIT_PREVIEW;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_EDIT_PREVIEW);
	    break;
	default:
	    break;
    }
}

static int
_copy_geometry_point_field(bsg_field_ref field, point_t **points, size_t *point_count)
{
    if (points)
	*points = NULL;
    if (point_count)
	*point_count = 0;
    if (!points || !point_count)
	return 0;

    size_t cnt = bsg_field_multi_count(field);
    if (!cnt)
	return 1;

    point_t *copy = (point_t *)bu_calloc(cnt, sizeof(point_t), "bsg render geometry points");
    for (size_t i = 0; i < cnt; i++) {
	if (!bsg_field_multi_point_at(field, i, copy[i])) {
	    bu_free(copy, "bsg render geometry points");
	    return 0;
	}
    }

    *points = copy;
    *point_count = cnt;
    return 1;
}

static int
_copy_geometry_int_field(bsg_field_ref field, int **values, size_t *value_count, const char *what)
{
    if (values)
	*values = NULL;
    if (value_count)
	*value_count = 0;
    if (!values || !value_count)
	return 0;

    size_t cnt = bsg_field_multi_count(field);
    if (!cnt)
	return 1;

    int *copy = (int *)bu_calloc(cnt, sizeof(int), what);
    for (size_t i = 0; i < cnt; i++) {
	if (!bsg_field_multi_int_at(field, i, &copy[i])) {
	    bu_free(copy, what);
	    return 0;
	}
    }

    *values = copy;
    *value_count = cnt;
    return 1;
}

static void
_surface_index_counts(const int *indices,
		      size_t index_count,
		      size_t *face_count_out,
		      size_t *vertex_index_count_out)
{
    size_t face_count = 0;
    size_t vertex_index_count = 0;
    int have_vertex = 0;

    if (face_count_out)
	*face_count_out = 0;
    if (vertex_index_count_out)
	*vertex_index_count_out = 0;

    if (!indices || !index_count)
	return;

    for (size_t i = 0; i < index_count; i++) {
	if (indices[i] < 0) {
	    if (have_vertex)
		face_count++;
	    have_vertex = 0;
	    continue;
	}
	vertex_index_count++;
	have_vertex = 1;
    }
    if (have_vertex)
	face_count++;

    if (face_count_out)
	*face_count_out = face_count;
    if (vertex_index_count_out)
	*vertex_index_count_out = vertex_index_count;
}

static bsg_render_surface_normal_kind
_surface_normal_kind(size_t point_count,
		     size_t face_count,
		     size_t vertex_index_count,
		     size_t normal_count,
		     size_t UNUSED(index_count))
{
    if (!normal_count)
	return BSG_RENDER_SURFACE_NORMALS_NONE;
    if (vertex_index_count && normal_count == vertex_index_count)
	return BSG_RENDER_SURFACE_NORMALS_PER_INDEX;
    if (point_count && normal_count == point_count)
	return BSG_RENDER_SURFACE_NORMALS_PER_VERTEX;
    if (face_count && normal_count == face_count)
	return BSG_RENDER_SURFACE_NORMALS_PER_FACE;
    return BSG_RENDER_SURFACE_NORMALS_NONE;
}

static int
_copy_indexed_surface_geometry(struct bsg_render_geometry *geometry)
{
    size_t vertex_index_count = 0;

    if (!geometry)
	return 0;

    memset(&geometry->surface, 0, sizeof(geometry->surface));

    if (geometry->arrays.point_count && geometry->arrays.points) {
	geometry->surface.points = (point_t *)bu_calloc(geometry->arrays.point_count,
		sizeof(point_t), "bsg render surface points");
	for (size_t i = 0; i < geometry->arrays.point_count; i++)
	    VMOVE(geometry->surface.points[i], geometry->arrays.points[i]);
	geometry->surface.point_count = geometry->arrays.point_count;
    }
    if (geometry->arrays.normal_count && geometry->arrays.normals) {
	geometry->surface.normals = (vect_t *)bu_calloc(geometry->arrays.normal_count,
		sizeof(vect_t), "bsg render surface normals");
	for (size_t i = 0; i < geometry->arrays.normal_count; i++)
	    VMOVE(geometry->surface.normals[i], geometry->arrays.normals[i]);
	geometry->surface.normal_count = geometry->arrays.normal_count;
    }
    if (geometry->arrays.index_count && geometry->arrays.indices) {
	geometry->surface.indices = (int *)bu_calloc(geometry->arrays.index_count,
		sizeof(int), "bsg render surface indices");
	memcpy(geometry->surface.indices, geometry->arrays.indices,
		geometry->arrays.index_count * sizeof(int));
	geometry->surface.index_count = geometry->arrays.index_count;
    }

    _surface_index_counts(geometry->surface.indices, geometry->surface.index_count,
	    &geometry->surface.face_count, &vertex_index_count);
    geometry->surface.normal_kind = _surface_normal_kind(
	    geometry->surface.point_count,
	    geometry->surface.face_count,
	    vertex_index_count,
	    geometry->surface.normal_count,
	    geometry->surface.index_count);
    return 1;
}

static void
_resolve_surface_material(struct bsg_render_item *item)
{
    if (!item || item->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	return;

    item->geometry.surface.material = item->appearance;
    item->geometry.surface.material_valid = 1;
}

static void *
_proxy_source_pointer(const struct bsg_payload *payload,
		      bsg_render_proxy_kind kind)
{
    if (!payload)
	return NULL;

    switch (kind) {
	case BSG_RENDER_PROXY_CSG:
	    return payload->pl.csg;
	case BSG_RENDER_PROXY_BREP:
	    return payload->pl.brep;
	case BSG_RENDER_PROXY_EDIT_PREVIEW:
	    return payload->pl.edit_preview;
	case BSG_RENDER_PROXY_EXTERNAL:
	    return payload->pl.opaque;
	default:
	    return NULL;
    }
}

static void
_set_proxy_metadata(struct bsg_render_geometry *geometry,
		    const struct bsg_payload *payload,
		    bsg_render_proxy_kind kind)
{
    if (!geometry)
	return;

    void *source = _proxy_source_pointer(payload, kind);
    uint64_t source_identity = (uint64_t)(uintptr_t)source;
    if (!source_identity && payload)
	source_identity = (uint64_t)(uintptr_t)payload;
    geometry->proxy.kind = kind;
    geometry->proxy.source_identity = source_identity;
    geometry->proxy.revision = payload ? payload->pl_revision : 0;
    if (geometry->proxy.source_identity)
	geometry->source_identity = geometry->proxy.source_identity;
}

static int
_copy_payload_line_set_geometry(const struct bsg_payload_line_set *ls,
				struct bsg_render_geometry *geometry)
{
    if (!geometry)
	return 0;

    geometry->arrays.points = NULL;
    geometry->arrays.commands = NULL;
    geometry->arrays.point_count = 0;
    geometry->arrays.command_count = 0;

    if (!ls || !ls->point_cnt)
	return 1;

    geometry->arrays.points = (point_t *)bu_calloc(ls->point_cnt,
	    sizeof(point_t), "bsg render line-set points");
    geometry->arrays.commands = (int *)bu_calloc(ls->point_cnt,
	    sizeof(int), "bsg render line-set commands");
    for (size_t i = 0; i < ls->point_cnt; i++) {
	if (ls->points)
	    VMOVE(geometry->arrays.points[i], ls->points[i]);
	geometry->arrays.commands[i] = ls->cmds ? ls->cmds[i] :
	    ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
    }
    geometry->arrays.point_count = ls->point_cnt;
    geometry->arrays.command_count = ls->point_cnt;
    return 1;
}

static int
_copy_label_geometry(const struct bsg_label *label,
		     struct bsg_render_geometry *geometry)
{
    if (!geometry)
	return 0;

    geometry->text.label = NULL;
    VSET(geometry->text.position, 0.0, 0.0, 0.0);
    VSET(geometry->text.target, 0.0, 0.0, 0.0);
    geometry->text.size = 0;
    geometry->text.line_flag = 0;
    geometry->text.anchor = BSG_ANCHOR_AUTO;
    geometry->text.arrow = 0;

    if (!label)
	return 1;

    geometry->text.label = bu_strdup(BU_VLS_IS_INITIALIZED(&label->label) ?
	    bu_vls_cstr(&label->label) : "");
    VMOVE(geometry->text.position, label->p);
    VMOVE(geometry->text.target, label->target);
    geometry->text.size = label->size;
    geometry->text.line_flag = label->line_flag;
    geometry->text.anchor = label->anchor;
    geometry->text.arrow = label->arrow;
    return 1;
}

static size_t
_image_pixel_count(size_t width, size_t height, size_t channels)
{
    size_t count;

    if (!width || !height || !channels)
	return 0;
    if (height > ((size_t)-1) / width)
	return 0;
    count = width * height;
    if (channels > ((size_t)-1) / count)
	return 0;
    return count * channels;
}

static int
_set_image_raster_geometry(struct bsg_render_geometry *geometry,
			   size_t width,
			   size_t height,
			   size_t channels,
			   const unsigned char *pixels)
{
    size_t pixel_count;

    if (!geometry)
	return 0;

    memset(&geometry->image, 0, sizeof(geometry->image));
    geometry->image.kind = BSG_RENDER_IMAGE_RASTER;
    geometry->image.width = width;
    geometry->image.height = height;
    geometry->image.channels = channels;

    pixel_count = _image_pixel_count(width, height, channels);
    if (pixel_count && pixels) {
	geometry->image.pixel_count = pixel_count;
	geometry->image.pixels = (unsigned char *)bu_malloc(pixel_count,
		"bsg render image pixels");
	memcpy(geometry->image.pixels, pixels, pixel_count);
    }

    return 1;
}

static int
_set_framebuffer_geometry(struct bsg_render_geometry *geometry, int mode)
{
    if (!geometry)
	return 0;

    memset(&geometry->image, 0, sizeof(geometry->image));
    geometry->image.kind = BSG_RENDER_IMAGE_FRAMEBUFFER;
    geometry->image.framebuffer_mode = mode;
    return 1;
}

static int
_copy_image_payload_geometry(const struct bsg_payload *payload,
			     struct bsg_render_geometry *geometry,
			     int payload_identity)
{
    if (!payload || !geometry)
	return 0;

    switch (payload->pl_type) {
	case BSG_PL_IMAGE:
	    if (!payload->pl.image)
		return 0;
	    if (!_set_image_raster_geometry(geometry,
			payload->pl.image->width,
			payload->pl.image->height,
			payload->pl.image->channels,
			payload->pl.image->pixels))
		return 0;
	    geometry->element_count = payload->pl.image->width * payload->pl.image->height;
	    if (payload_identity)
		geometry->source_identity = (uint64_t)(uintptr_t)payload->pl.image;
	    return 1;
	case BSG_PL_FRAMEBUFFER:
	    if (!payload->pl.framebuffer)
		return 0;
	    if (!_set_framebuffer_geometry(geometry,
			payload->pl.framebuffer->mode))
		return 0;
	    if (payload_identity)
		geometry->source_identity = (uint64_t)(uintptr_t)payload->pl.framebuffer;
	    return 1;
	default:
	    return 0;
    }
}

static int
_copy_image_field_geometry(const bsg_node *node,
			   struct bsg_render_geometry *geometry)
{
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t channels = 0;
    bsg_geometry_node_kind kind;

    if (!node || !geometry)
	return 0;

    kind = bsg_geometry_node_kind_get(node);
    if (kind == BSG_GEOMETRY_NODE_FRAMEBUFFER) {
	(void)bsg_field_get_uint64(
		bsg_node_field_ref((bsg_node *)node,
		    BSG_FIELD_GEOMETRY_IMAGE_CHANNELS),
		&channels);
	return _set_framebuffer_geometry(geometry, (int)channels);
    }

    (void)bsg_field_get_uint64(
	    bsg_node_field_ref((bsg_node *)node,
		BSG_FIELD_GEOMETRY_IMAGE_WIDTH),
	    &width);
    (void)bsg_field_get_uint64(
	    bsg_node_field_ref((bsg_node *)node,
		BSG_FIELD_GEOMETRY_IMAGE_HEIGHT),
	    &height);
    (void)bsg_field_get_uint64(
	    bsg_node_field_ref((bsg_node *)node,
		BSG_FIELD_GEOMETRY_IMAGE_CHANNELS),
	    &channels);

    if (!_set_image_raster_geometry(geometry, (size_t)width, (size_t)height,
		(size_t)channels, NULL))
	return 0;
    geometry->element_count = (size_t)width * (size_t)height;
    return 1;
}

static int
_copy_annotation_segment(struct bsg_annotation_segment *dst,
			 const struct bsg_annotation_segment *src,
			 const char *what)
{
    if (!dst || !src)
	return 0;
    memset(dst, 0, sizeof(*dst));
    dst->kind = src->kind;
    dst->reverse = src->reverse;
    switch (src->kind) {
	case BSG_ANNOTATION_SEGMENT_LINE:
	    dst->data.line = src->data.line;
	    break;
	case BSG_ANNOTATION_SEGMENT_CARC:
	    dst->data.carc = src->data.carc;
	    break;
	case BSG_ANNOTATION_SEGMENT_NURB:
	    dst->data.nurb.order = src->data.nurb.order;
	    dst->data.nurb.point_type = src->data.nurb.point_type;
	    dst->data.nurb.knot_count = src->data.nurb.knot_count;
	    dst->data.nurb.control_point_count = src->data.nurb.control_point_count;
	    if (src->data.nurb.knot_count && src->data.nurb.knots) {
		dst->data.nurb.knots = (fastf_t *)bu_calloc(src->data.nurb.knot_count,
			sizeof(fastf_t), what);
		memcpy(dst->data.nurb.knots, src->data.nurb.knots,
			src->data.nurb.knot_count * sizeof(fastf_t));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.control_points) {
		dst->data.nurb.control_points = (int *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(int), what);
		memcpy(dst->data.nurb.control_points, src->data.nurb.control_points,
			src->data.nurb.control_point_count * sizeof(int));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.weights) {
		dst->data.nurb.weights = (fastf_t *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(fastf_t), what);
		memcpy(dst->data.nurb.weights, src->data.nurb.weights,
			src->data.nurb.control_point_count * sizeof(fastf_t));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_BEZIER:
	    dst->data.bezier.degree = src->data.bezier.degree;
	    dst->data.bezier.control_point_count = src->data.bezier.control_point_count;
	    if (src->data.bezier.control_point_count && src->data.bezier.control_points) {
		dst->data.bezier.control_points = (int *)bu_calloc(
			src->data.bezier.control_point_count, sizeof(int), what);
		memcpy(dst->data.bezier.control_points, src->data.bezier.control_points,
			src->data.bezier.control_point_count * sizeof(int));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_TEXT:
	    dst->data.text.ref_pt = src->data.text.ref_pt;
	    dst->data.text.relative_position = src->data.text.relative_position;
	    dst->data.text.size = src->data.text.size;
	    dst->data.text.rotation = src->data.text.rotation;
	    if (src->data.text.text)
		dst->data.text.text = bu_strdup(src->data.text.text);
	    break;
	default:
	    break;
    }
    return 1;
}

static int
_copy_annotation_geometry(const struct bsg_payload_annotation *annotation,
			  struct bsg_render_geometry *geometry)
{
    if (!geometry)
	return 0;

    memset(&geometry->annotation, 0, sizeof(geometry->annotation));
    if (!annotation)
	return 1;

    geometry->annotation.text = bu_strdup(BU_VLS_IS_INITIALIZED(&annotation->text) ?
	    bu_vls_cstr(&annotation->text) : "");
    geometry->annotation.space = annotation->space;
    VMOVE(geometry->annotation.anchor, annotation->anchor);
    MAT_COPY(geometry->annotation.model_mat, annotation->model_mat);
    MAT_COPY(geometry->annotation.display_mat, annotation->display_mat);
    if (annotation->point_cnt && annotation->points) {
	geometry->annotation.points = (point_t *)bu_calloc(annotation->point_cnt,
		sizeof(point_t), "bsg render annotation points");
	for (size_t i = 0; i < annotation->point_cnt; i++)
	    VMOVE(geometry->annotation.points[i], annotation->points[i]);
	geometry->annotation.point_count = annotation->point_cnt;
    }
    if (annotation->segment_cnt && annotation->segments) {
	geometry->annotation.segments = (struct bsg_annotation_segment *)bu_calloc(
		annotation->segment_cnt, sizeof(struct bsg_annotation_segment),
		"bsg render annotation segments");
	for (size_t i = 0; i < annotation->segment_cnt; i++)
	    (void)_copy_annotation_segment(&geometry->annotation.segments[i],
		    &annotation->segments[i], "bsg render annotation segment data");
	geometry->annotation.segment_count = annotation->segment_cnt;
    }
    geometry->element_count = annotation->segment_cnt ? annotation->segment_cnt :
	annotation->point_cnt;
    return 1;
}

static int
_copy_overlay_geometry(const struct bsg_payload *payload,
		       struct bsg_render_geometry *geometry)
{
    if (!geometry)
	return 0;

    geometry->overlay.kind = BSG_RENDER_OVERLAY_GEOMETRY_NONE;
    memset(&geometry->overlay.axes, 0, sizeof(geometry->overlay.axes));
    memset(&geometry->overlay.grid, 0, sizeof(geometry->overlay.grid));

    if (!payload)
	return 0;

    switch (payload->pl_type) {
	case BSG_PL_AXES:
	    if (!payload->pl.axes)
		return 0;
	    geometry->overlay.kind = BSG_RENDER_OVERLAY_GEOMETRY_AXES;
	    geometry->overlay.axes = *payload->pl.axes;
	    geometry->source_identity = (uint64_t)(uintptr_t)payload->pl.axes;
	    return 1;
	case BSG_PL_GRID:
	    if (!payload->pl.grid)
		return 0;
	    geometry->overlay.kind = BSG_RENDER_OVERLAY_GEOMETRY_GRID;
	    geometry->overlay.grid = *payload->pl.grid;
	    geometry->source_identity = (uint64_t)(uintptr_t)payload->pl.grid;
	    return 1;
	default:
	    return 0;
    }
}

static void
_resolve_item_geometry(const bsg_node *node,
		       struct bsg_render_item *item,
		       struct bsg_payload *payload)
{
    if (!node || !item)
	return;

    item->geometry.kind = BSG_RENDER_GEOMETRY_NONE;
    item->geometry.revision = item->payload_revision;
    item->geometry.source_identity = item->cache_identity;
    item->geometry.element_count = 0;
    memset(&item->geometry.data, 0, sizeof(item->geometry.data));
    memset(&item->geometry.image, 0, sizeof(item->geometry.image));
    memset(&item->geometry.surface, 0, sizeof(item->geometry.surface));
    memset(&item->geometry.annotation, 0, sizeof(item->geometry.annotation));
    memset(&item->geometry.proxy, 0, sizeof(item->geometry.proxy));

    _resolve_item_geometry_node(node, item, payload);
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_NONE)
	return;

    if (!payload)
	return;

    switch (payload->pl_type) {
	case BSG_PL_MESH: {
	    struct bsg_mesh_lod *mesh = bsg_payload_mesh_get(payload);
	    if (mesh) {
		item->geometry.kind = BSG_RENDER_GEOMETRY_MESH;
		item->geometry.revision = item->payload_revision;
		item->geometry.revision ^= (uint64_t)(unsigned int)(item->lod_level + 1) * 1099511628211ULL;
		item->geometry.data.mesh = mesh;
		item->geometry.source_identity = (uint64_t)(uintptr_t)mesh;
	    }
	    break;
	}
	case BSG_PL_LINE_SET:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
	    {
		struct bsg_payload_line_set *ls = bsg_payload_line_set_get(payload);
		(void)_copy_payload_line_set_geometry(ls, &item->geometry);
		item->geometry.element_count = item->geometry.arrays.point_count;
		item->geometry.source_identity = (uint64_t)(uintptr_t)ls;
	    }
	    break;
	case BSG_PL_SKETCH: {
	    struct bsg_payload *geometry = bsg_payload_sketch_geometry(payload);
	    struct bsg_payload_line_set *ls = bsg_payload_line_set_get(geometry);
	    if (ls) {
		item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
		item->geometry.revision = geometry->pl_revision;
		(void)_copy_payload_line_set_geometry(ls, &item->geometry);
		item->geometry.element_count = item->geometry.arrays.point_count;
		item->geometry.source_identity = (uint64_t)(uintptr_t)ls;
	    }
	    break;
	}
	case BSG_PL_TEXT:
	case BSG_PL_HUD_TEXT:
	    item->geometry.kind = (payload->pl_type == BSG_PL_HUD_TEXT) ?
		BSG_RENDER_GEOMETRY_HUD : BSG_RENDER_GEOMETRY_TEXT;
	    {
		const struct bsg_label *label = (payload->pl_type == BSG_PL_HUD_TEXT) ?
		    payload->pl.hud_text : payload->pl.text;
		(void)_copy_label_geometry(label, &item->geometry);
		item->geometry.source_identity = (uint64_t)(uintptr_t)label;
	    }
	    break;
	case BSG_PL_IMAGE:
	case BSG_PL_FRAMEBUFFER:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_IMAGE;
	    (void)_copy_image_payload_geometry(payload, &item->geometry, 1);
	    break;
	case BSG_PL_AXES:
	case BSG_PL_GRID:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_OVERLAY;
	    (void)_copy_overlay_geometry(payload, &item->geometry);
	    break;
	case BSG_PL_ANNOTATION:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_ANNOTATION;
	    (void)_copy_annotation_geometry(payload->pl.annotation, &item->geometry);
	    item->geometry.source_identity = (uint64_t)(uintptr_t)payload->pl.annotation;
	    break;
	case BSG_PL_CSG:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_CSG_PROXY;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_CSG);
	    break;
	case BSG_PL_BREP:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_BREP_PROXY;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_BREP);
	    break;
	case BSG_PL_EDIT_PREVIEW:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_EDIT_PREVIEW;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_EDIT_PREVIEW);
	    break;
	default:
	    item->geometry.kind = BSG_RENDER_GEOMETRY_EXTERNAL_PROXY;
	    _set_proxy_metadata(&item->geometry, payload, BSG_RENDER_PROXY_EXTERNAL);
	    break;
    }
}

static bsg_render_source_scope
_source_scope_from_node(const bsg_node *node)
{
    if (!node)
	return BSG_RENDER_SOURCE_SCOPE_UNKNOWN;
    if (bsg_node_is_database_source(node))
	return BSG_RENDER_SOURCE_SCOPE_DATABASE;
    if (bsg_node_is_local_source(node))
	return BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL;
    if (bsg_node_is_view_source(node))
	return BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED;
    if (bsg_node_is_internal_child_source(node))
	return BSG_RENDER_SOURCE_SCOPE_INTERNAL_CHILD;
    if (bsg_shape_is_non_database_source(node))
	return BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED;
    return BSG_RENDER_SOURCE_SCOPE_UNKNOWN;
}

static bsg_render_geometry_role
_geometry_role_from_node(const bsg_node *node,
			 const struct bsg_render_item *item)
{
    if (node && bsg_shape_is_mesh(node))
	return BSG_RENDER_GEOMETRY_ROLE_MESH;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_DB_OBJECT))
	return BSG_RENDER_GEOMETRY_ROLE_DATABASE_OBJECT;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_AXES_WIDGET))
	return BSG_RENDER_GEOMETRY_ROLE_AXES_WIDGET;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_TEXT_LABELS))
	return BSG_RENDER_GEOMETRY_ROLE_TEXT_LABEL;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_POLYGON_REGION))
	return BSG_RENDER_GEOMETRY_ROLE_POLYGON_REGION;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_LINE_SET))
	return BSG_RENDER_GEOMETRY_ROLE_LINE_SET;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_VIEW_OVERLAY))
	return BSG_RENDER_GEOMETRY_ROLE_VIEW_OVERLAY;
    if (item) {
	if (item->phase == BSG_RENDER_PHASE_HUD ||
		item->geometry.kind == BSG_RENDER_GEOMETRY_HUD)
	    return BSG_RENDER_GEOMETRY_ROLE_HUD;
	switch (item->geometry.kind) {
	    case BSG_RENDER_GEOMETRY_MESH:
		return BSG_RENDER_GEOMETRY_ROLE_MESH;
	    case BSG_RENDER_GEOMETRY_TEXT:
	    case BSG_RENDER_GEOMETRY_ANNOTATION:
		return BSG_RENDER_GEOMETRY_ROLE_TEXT_LABEL;
	    case BSG_RENDER_GEOMETRY_IMAGE:
		return BSG_RENDER_GEOMETRY_ROLE_IMAGE;
	    case BSG_RENDER_GEOMETRY_LINE_SET:
		return BSG_RENDER_GEOMETRY_ROLE_LINE_SET;
	    case BSG_RENDER_GEOMETRY_OVERLAY:
		return BSG_RENDER_GEOMETRY_ROLE_VIEW_OVERLAY;
	    default:
		break;
	}
    }
    return BSG_RENDER_GEOMETRY_ROLE_UNKNOWN;
}

static enum bsg_feature_family
_feature_family_from_node(const bsg_node *node)
{
    return bsg_feature_node_family(node);
}

static bsg_render_draw_intent
_draw_intent_from_item(const bsg_node *node,
		       const struct bsg_render_item *item)
{
    if (!node || !item)
	return BSG_RENDER_DRAW_INTENT_UNKNOWN;
    if (item->source.scope == BSG_RENDER_SOURCE_SCOPE_DATABASE)
	return BSG_RENDER_DRAW_INTENT_DATABASE;
    if (item->phase == BSG_RENDER_PHASE_HUD ||
	    item->geometry.kind == BSG_RENDER_GEOMETRY_HUD)
	return BSG_RENDER_DRAW_INTENT_HUD;
    if (bsg_overlay_info_get(node))
	return BSG_RENDER_DRAW_INTENT_OVERLAY;
    if (item->source.feature_family == BSG_FEATURE_TRANSIENT_PREVIEW ||
	    item->source.feature_family == BSG_FEATURE_EDIT_HANDLE ||
	    item->source.feature_family == BSG_FEATURE_SKETCH)
	return BSG_RENDER_DRAW_INTENT_EDIT_PREVIEW;
    if (item->source.scope == BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL ||
	    item->source.scope == BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED)
	return BSG_RENDER_DRAW_INTENT_VIEW_FEATURE;
    if (item->source.scope == BSG_RENDER_SOURCE_SCOPE_INTERNAL_CHILD)
	return BSG_RENDER_DRAW_INTENT_INTERNAL;
    return BSG_RENDER_DRAW_INTENT_UNKNOWN;
}

static void
_resolve_item_features(const bsg_node *node, struct bsg_render_item *item)
{
    if (!node || !item)
	return;

    memset(&item->features, 0, sizeof(item->features));
    if (bsg_shape_arrows_enabled((bsg_node *)node)) {
	item->features.flags |= BSG_RENDER_FEATURE_ARROWS;
	item->features.arrow_tip_length = bsg_shape_arrow_tip_length((bsg_node *)node);
	item->features.arrow_tip_width = bsg_shape_arrow_tip_width((bsg_node *)node);
    }
    const struct bsg_hud_node_meta *hud = bsg_hud_node_get_meta((bsg_node *)node);
    if (hud)
	item->features.hud_feature_type = hud->feature_type;
}


static int
_render_collect_shape(bsg_node *node,
		      const struct bsg_action_state *state,
		      void *userdata)
{
    struct collect_state *st = (struct collect_state *)userdata;
    if (!node || !state || !st)
	return 1;

    const struct bsg_render_request *req = st->req;

    /* Pre-render update pass for payloads that need it. */
    if (req->flags & BSG_RENDER_FLAG_PAYLOAD_PREPARE)
	(void)bsg_payload_realize(bsg_node_get_payload(node), req->view);

    /* Resolve appearance so backends read from item->appearance and never
     * re-derive material state from node storage. */
    struct bsg_resolved_appearance ra;
    memset(&ra, 0, sizeof(ra));
    bsg_appearance_resolve_with_settings(req->settings, node,
	    state->inherited_settings, &ra);
    _apply_traversal_appearance(state, &ra);

    const char *source_name = _nearest_draw_intent_path(node);
    if (!source_name || !source_name[0])
	source_name = bsg_node_name(node);

    int selected = 0;
    struct bsg_selection *selection = bsg_view_selection(req->view);
    if (selection) {
	bsg_feature_ref feature = BSG_FEATURE_REF_NULL_INIT;
	struct bsg_interaction_record *record =
	    bsg_interaction_record_create_ref(req->view,
		    BSG_INTERACTION_SELECTED_PATH, feature, source_name, NULL);
	if (record) {
	    selected = bsg_selection_contains_record(selection, record);
	    bsg_interaction_record_free(record);
	}
    }
    int highlighted = ra.highlighted || selected;
    ra.highlighted = highlighted;

    bsg_render_phase phase =
	_classify_phase(req, node, ra.transparency);

    /* Build the render item */
    struct bsg_render_item *item = bsg_render_item_create();
    item->sequence      = st->next_sequence++;
    item->view          = req->view;
    bsg_state_ref_transform(state->state, item->model_mat);
    item->name          = source_name;
    item->appearance    = ra;
    struct bsg_payload *payload = bsg_node_get_payload(node);
    item->payload_revision = payload ? payload->pl_revision : 0;
    const struct bsg_payload_realization *realization =
	payload ? bsg_payload_realization_state(payload) : NULL;
    item->realization_kind = realization ? realization->kind : BSG_REALIZE_NONE;
    item->realization_status = realization ? realization->status : BSG_REALIZE_STATUS_CURRENT;
    item->stale_reason = realization ? realization->stale_reason : BSG_PAYLOAD_STALE_NONE;
    item->phase         = phase;
    bsg_state_ref_set_selection(state->state, selected, highlighted);
    bsg_state_ref_set_render_phase(state->state, (int)phase);
    item->visible       = bsg_state_ref_visible(state->state);
    item->force_draw    = bsg_state_ref_force_draw(state->state);
    item->highlighted   = highlighted;
    item->selected      = selected;
    item->has_bounds =
	_resolve_item_bounds(node, realization,
		payload,
		item->bounds_center, &item->bounds_radius);
    item->non_database_source = bsg_shape_is_non_database_source(node);
    item->graph_depth   = _node_depth(node);
    item->lod_level     = bsg_state_ref_lod_level(state->state);
    item->lod_level_count = bsg_state_ref_lod_level_count(state->state);
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_POLYGON_REGION) ||
	    (node->parent && bsg_node_has_geometry_role(node->parent, BSG_GEOMETRY_POLYGON_REGION)))
	item->order_flags |= BSG_RENDER_ORDER_OPAQUE_CHILD_FIRST;
    _resolve_item_geometry(node, item, payload);
    _resolve_surface_material(item);
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_MESH ||
		bsg_shape_is_mesh(node)) {
	mat_t node_draw_mat;
	bsg_node_get_draw_mat(node, node_draw_mat);
	mat_t state_mat;
	bsg_state_ref_transform(state->state, state_mat);
	bn_mat_mul(item->model_mat, state_mat, node_draw_mat);
    }
    item->sort_key      = _sort_key(req, node, item);
    _resolve_item_features(node, item);
    item->source.name = item->name;
    item->source.non_database_source = item->non_database_source;
    item->source.scope = _source_scope_from_node(node);
    item->source.geometry_role = _geometry_role_from_node(node, item);
    item->source.feature_family = _feature_family_from_node(node);
    const struct bsg_overlay_info *overlay_info = bsg_overlay_info_get(node);
    item->source.overlay_role = overlay_info ? overlay_info->role : BSG_OVERLAY_ROLE_MODEL;
    item->source.overlay_class = overlay_info ? overlay_info->overlay_class : BSG_OVERLAY_CLASS_COMMAND_RESULT;
    item->source.draw_intent = _draw_intent_from_item(node, item);
    uint64_t policy_lod_id = _lod_policy_identity(req->settings,
	    item->source.geometry_role, item->geometry.kind, item->lod_level);
    struct bsg_lod_source_record lod_record;
    item->source.lod_policy_id = policy_lod_id;
    if (bsg_lod_source_record_get(bsg_state_ref_lod_source(state->state), &lod_record))
	item->source.lod_id = lod_record.identity;
    else
	item->source.lod_id = 0;
    item->source.source_id = _render_source_id(node, payload, item->name,
	    item->source.scope, item->source.geometry_role,
	    item->source.draw_intent, item->non_database_source);
    bsg_state_ref_set_source_identity(state->state, item->source.source_id);
    bsg_state_ref_set_backend_capabilities(state->state, req->batch.backend_capabilities);
    if (!item->geometry.source_identity)
	item->geometry.source_identity = item->source.source_id;
    item->source.geometry_id = item->geometry.source_identity;
    item->context = req->batch;
    item->context.overlay_pass = (phase == BSG_RENDER_PHASE_OVERLAY) ? 1 : 0;
    item->cache_identity = _cache_identity(item);
    if (!item->source.geometry_id)
	item->source.geometry_id = item->geometry.source_identity;
    bu_ptbl_ins(&st->phase_items[(int)phase], (long *)item);
    return 1;
}


/* ------------------------------------------------------------------ */
/* Dispatch helpers                                                     */
/* ------------------------------------------------------------------ */

/**
 * Dispatch a single item: adapter callbacks, collect, or legacy fallback.
 * Returns 1 if the item was dispatched (or collected), 0 if skipped.
 */
static int
_dispatch_item(struct bsg_render_request *req,
	       const struct bsg_render_item *item)
{
    if (req->adapter) {
	if (req->adapter->prepare)
	    req->adapter->prepare(req->dmp, item);
	if (req->adapter->draw)
	    req->adapter->draw(req->dmp, item);
    }

    return 1;
}


/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

struct bsg_render_request *
bsg_render_request_create(struct bsg_view *view,
			  void            *dmp)
{
    struct bsg_render_request *req;
    BU_ALLOC(req, struct bsg_render_request);
    memset(req, 0, sizeof(struct bsg_render_request));
    req->view    = view;
    req->dmp     = dmp;
    req->flags   = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    req->adapter = NULL;
    req->items   = NULL;

    /* Populate render settings from view at request creation time.  The
     * executor and backends read policy fields from here rather than reaching
     * back into the view directly. */
    req->settings = bsg_render_settings_create();
    if (view)
	bsg_render_settings_from_view(req->settings, view);
    _populate_batch_context(req);

    return req;
}


void
bsg_render_request_destroy(struct bsg_render_request *req)
{
    if (!req)
	return;
    bsg_render_settings_destroy(req->settings);
    req->settings = NULL;
    bu_free(req, "bsg_render_request");
}


struct bsg_view *
bsg_render_request_view(const struct bsg_render_request *req)
{
    return req ? req->view : NULL;
}


void *
bsg_render_request_dmp(const struct bsg_render_request *req)
{
    return req ? req->dmp : NULL;
}


unsigned int
bsg_render_request_flags(const struct bsg_render_request *req)
{
    return req ? req->flags : 0;
}


void
bsg_render_request_set_flags(struct bsg_render_request *req, unsigned int flags)
{
    if (!req)
	return;
    req->flags = flags;
    req->batch.request_flags = flags;
    req->batch.hud_pass = (flags & BSG_RENDER_FLAG_HUD_PASS) ? 1 : 0;
}


void
bsg_render_request_add_flags(struct bsg_render_request *req, unsigned int flags)
{
    if (!req)
	return;
    bsg_render_request_set_flags(req, req->flags | flags);
}


void
bsg_render_request_set_adapter(struct bsg_render_request *req,
			       struct bsg_backend_adapter *adapter)
{
    if (req)
	req->adapter = adapter;
}


void
bsg_render_request_set_output_items(struct bsg_render_request *req,
				    struct bu_ptbl *items)
{
    if (req)
	req->items = items;
}


struct bsg_render_settings *
bsg_render_request_settings(struct bsg_render_request *req)
{
    return req ? req->settings : NULL;
}


const struct bsg_render_settings *
bsg_render_request_settings_const(const struct bsg_render_request *req)
{
    return req ? req->settings : NULL;
}


void
bsg_render_request_set_background(struct bsg_render_request *req,
				  const unsigned char *background1,
				  const unsigned char *background2)
{
    if (!req)
	return;
    if (background1) {
	req->batch.background1[0] = background1[0];
	req->batch.background1[1] = background1[1];
	req->batch.background1[2] = background1[2];
    }
    if (background2) {
	req->batch.background2[0] = background2[0];
	req->batch.background2[1] = background2[1];
	req->batch.background2[2] = background2[2];
    }
}


void
bsg_render_request_set_geometry_default_color(struct bsg_render_request *req,
					      const unsigned char *color)
{
    if (!req || !color)
	return;
    req->batch.geometry_default_color[0] = color[0];
    req->batch.geometry_default_color[1] = color[1];
    req->batch.geometry_default_color[2] = color[2];
    if (req->settings) {
	req->settings->geometry_default_color[0] = color[0];
	req->settings->geometry_default_color[1] = color[1];
	req->settings->geometry_default_color[2] = color[2];
    }
}


void
bsg_render_request_set_clip(struct bsg_render_request *req,
			    const vect_t *clipmin,
			    const vect_t *clipmax,
			    int zclip)
{
    if (!req)
	return;
    if (clipmin)
	VMOVE(req->batch.clipmin, *clipmin);
    if (clipmax)
	VMOVE(req->batch.clipmax, *clipmax);
    req->batch.zclip = zclip;
    if (req->settings)
	req->settings->zclip = zclip;
}


void
bsg_render_request_set_lighting(struct bsg_render_request *req, int lighting)
{
    if (req)
	req->batch.lighting = lighting;
}


struct bsg_render_batch *
bsg_render_batch_create(void)
{
    struct bsg_render_batch *batch;
    BU_ALLOC(batch, struct bsg_render_batch);
    memset(batch, 0, sizeof(*batch));
    bu_ptbl_init(&batch->items, 8, "bsg render batch items");
    return batch;
}


void
bsg_render_batch_clear(struct bsg_render_batch *batch)
{
    if (!batch)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&batch->items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&batch->items, i));
    bu_ptbl_reset(&batch->items);
    memset(&batch->context, 0, sizeof(batch->context));
}


void
bsg_render_batch_destroy(struct bsg_render_batch *batch)
{
    if (!batch)
	return;
    bsg_render_batch_clear(batch);
    bu_ptbl_free(&batch->items);
    bu_free(batch, "bsg_render_batch");
}


size_t
bsg_render_batch_count(const struct bsg_render_batch *batch)
{
    return batch ? BU_PTBL_LEN(&batch->items) : 0;
}


const struct bsg_render_item *
bsg_render_batch_get(const struct bsg_render_batch *batch, size_t idx)
{
    if (!batch || idx >= BU_PTBL_LEN(&batch->items))
	return NULL;
    return (const struct bsg_render_item *)BU_PTBL_GET(&batch->items, idx);
}


size_t
bsg_render_batch_move_items(struct bsg_render_batch *batch, struct bu_ptbl *dest)
{
    if (!batch || !dest)
	return 0;
    size_t moved = BU_PTBL_LEN(&batch->items);
    for (size_t i = 0; i < moved; i++)
	bu_ptbl_ins(dest, BU_PTBL_GET(&batch->items, i));
    bu_ptbl_reset(&batch->items);
    return moved;
}


static int
_render_request_collect_from_root(struct bsg_render_request *req,
				  struct bsg_render_batch *batch,
				  bsg_node *root)
{
    if (!req || !batch)
	return -1;
    bsg_render_batch_clear(batch);

    unsigned int adapter_caps = 0;
    int has_capability_query = (req->adapter && req->adapter->capabilities);
    if (has_capability_query)
	adapter_caps = req->adapter->capabilities(req->dmp);
    _populate_batch_context(req);
    req->batch.backend_capabilities = adapter_caps;
    if (req->settings) {
	req->batch.geometry_default_color[0] = req->settings->geometry_default_color[0];
	req->batch.geometry_default_color[1] = req->settings->geometry_default_color[1];
	req->batch.geometry_default_color[2] = req->settings->geometry_default_color[2];
    }
    if (req->adapter && req->adapter->begin_request)
	req->adapter->begin_request(req->dmp, req);
    batch->context = req->batch;
    int do_sorted_alpha = ((req->flags & BSG_RENDER_FLAG_SORTED_ALPHA) != 0);
    if (do_sorted_alpha && has_capability_query)
	do_sorted_alpha = ((adapter_caps & BSG_ADAPTER_CAP_SORTED_ALPHA) != 0);

    /* Set up per-phase collection buckets */
    struct collect_state st;
    st.req = req;
    st.next_sequence = 0;
    for (int p = 0; p < BSG_RENDER_PHASE_COUNT; p++)
	bu_ptbl_init(&st.phase_items[p], 8, "render phase items");

    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = req->view;
    traversal.root = root;
    traversal.flags = BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT;
    if (req->flags & BSG_RENDER_FLAG_VISIBLE_ONLY)
	traversal.flags |= BSG_ACTION_TRAVERSE_VISIBLE_ONLY;
    traversal.shape_cb = _render_collect_shape;
    traversal.userdata = &st;
    bsg_action_traverse(&traversal);

    _sort_opaque_bucket(&st.phase_items[BSG_RENDER_PHASE_OPAQUE]);
    if (do_sorted_alpha)
	_sort_transparent_bucket(&st.phase_items[BSG_RENDER_PHASE_TRANSPARENT]);
    _sort_overlay_bucket(&st.phase_items[BSG_RENDER_PHASE_OVERLAY]);
    if (req->flags & BSG_RENDER_FLAG_HUD_PASS)
	_sort_hud_bucket(&st.phase_items[BSG_RENDER_PHASE_HUD]);

    /* Transfer phase buckets into the retained batch in dispatch order. */
    int collected = 0;
    for (int p = 0; p < BSG_RENDER_PHASE_COUNT; p++) {
	struct bu_ptbl *bucket = &st.phase_items[p];
	for (size_t i = 0; i < BU_PTBL_LEN(bucket); i++) {
	    struct bsg_render_item *item =
		(struct bsg_render_item *)BU_PTBL_GET(bucket, i);
	    bu_ptbl_ins(&batch->items, (long *)item);
	    collected++;
	}
	bu_ptbl_free(bucket);
    }

    return collected;
}

struct render_action_data {
    struct bsg_render_request *req;
    struct bsg_render_batch *batch;
};

static int
_render_action_apply(bsg_action_ref UNUSED(action), bsg_node_ref root, void *data)
{
    struct render_action_data *st = (struct render_action_data *)data;
    bsg_node *node = bsg_action_node_from_ref(root);
    if (!st || !node)
	return -1;
    return _render_request_collect_from_root(st->req, st->batch, node);
}

static void
_render_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    if (data)
	bu_free(data, "render action data");
}

bsg_action_ref
bsg_render_action_create(struct bsg_render_request *req,
			 struct bsg_render_batch *batch)
{
    struct render_action_data *st;
    BU_ALLOC(st, struct render_action_data);
    st->req = req;
    st->batch = batch;
    return bsg_action_ref_create_internal(bsg_render_action_type(),
	    _render_action_apply, _render_action_destroy, st, "render action");
}

int
bsg_render_request_collect(struct bsg_render_request *req,
			   struct bsg_render_batch *batch)
{
    if (!req || !batch)
	return -1;
    bsg_node *root = _request_render_root(req);
    if (!root) {
	bsg_render_batch_clear(batch);
	return 0;
    }
    bsg_action_ref action = bsg_render_action_create(req, batch);
    int ret = bsg_action_apply(action, bsg_action_node_ref_from_node(root));
    bsg_action_ref_destroy(action);
    return ret;
}


int
bsg_render_batch_dispatch(struct bsg_render_request *req,
			  const struct bsg_render_batch *batch)
{
    if (!req || !batch)
	return -1;

    int dispatched = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&batch->items); i++) {
	const struct bsg_render_item *item =
	    (const struct bsg_render_item *)BU_PTBL_GET(&batch->items, i);
	dispatched += _dispatch_item(req, item);
    }

    if (req->adapter && req->adapter->end_request)
	req->adapter->end_request(req->dmp, req);

    return dispatched;
}


int
bsg_render_request_execute(struct bsg_render_request *req)
{
    if (!req)
	return -1;

    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!batch)
	return -1;

    int ret = bsg_render_request_collect(req, batch);
    if (ret >= 0) {
	if (req->flags & BSG_RENDER_FLAG_COLLECT_ITEMS) {
	    if (req->items) {
		for (size_t i = 0; i < BU_PTBL_LEN(&batch->items); i++)
		    bu_ptbl_ins(req->items, BU_PTBL_GET(&batch->items, i));
		bu_ptbl_reset(&batch->items);
	    }
	} else {
	    ret = bsg_render_batch_dispatch(req, batch);
	}
    }

    bsg_render_batch_destroy(batch);
    return ret;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
