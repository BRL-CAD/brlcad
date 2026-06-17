/*                    L O D _ N O D E . C
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
/** @file libbsg/lod_node.c
 *
 * BSG_NODE_LOD lifecycle — create / set_ops / attach_level / cursor.
 *
 * A LoD node sits between a path-group node and its level representation
 * children.  Its dedicated LoD payload contains the policy vtable
 * (bsg_lod_ops), opaque user_data, and a small per-view cursor array.
 *
 * No level-selection policy is implemented here; policy is supplied by
 * libbv (mesh pop-buffer) and libged (CSG adaptive wireframe) via
 * bsg_lod_node_set_ops().
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/util.h"

#include "bsg/draw_set.h"
#include "bsg/field.h"
#include "bsg/lod.h"
#include "bsg_private.h"
#include "node_private.h"
#include "draw_set_private.h"
#include "field_private.h"
#include "lod_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "scene_object_private.h"


/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int
_lod_node_is_lod(const bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_lod_type());
}

struct bsg_lod_payload *
bsg_lod_node_payload(bsg_node *node)
{
    return (node && node->i) ? node->i->lod_payload : NULL;
}


void
bsg_lod_node_set_payload(bsg_node *node, struct bsg_lod_payload *payload)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->lod_payload = payload;
}


/*
 * Return the bsg_lod_payload stored on the LoD node, or NULL when
 * node is NULL, not a BSG_NODE_LOD node, or has no payload.
 */
static struct bsg_lod_payload *
_lod_payload(bsg_node *node)
{
    if (!node)
	return NULL;
    bsg_node *n = (bsg_node *)node;
    if (!_lod_node_is_lod(n))
	return NULL;
    return bsg_lod_node_payload(n);
}


/*
 * Release callback installed on every BSG_NODE_LOD node.
 * Frees the bsg_lod_payload and calls ops->free() if present.
 */
static void
_lod_node_free_cb(bsg_node *s)
{
    if (!s)
	return;
    struct bsg_lod_payload *pl = bsg_lod_node_payload(s);
    if (!pl)
	return;

    /* Let the policy release its state first. */
    if (pl->ops && pl->ops->free)
	pl->ops->free((bsg_node *)s);

    if (pl->source_name)
	bu_free(pl->source_name, "bsg_lod_payload source name");
    bu_free(pl->cursors, "bsg_lod_payload cursors");
    bu_free(pl, "bsg_lod_payload");
    bsg_lod_node_set_payload(s, NULL);
}


/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

bsg_node *
bsg_lod_node_create(struct bsg_view *v)
{
    if (!v)
	return NULL;

    bsg_node *n = bsg_node_create(v, BSG_NODE_LOD);
    if (!n)
	return NULL;

    bsg_node_set_object_type(n, bsg_lod_type());

    /* Allocate the payload. */
    struct bsg_lod_payload *pl;
    BU_GET(pl, struct bsg_lod_payload);
    memset(pl, 0, sizeof(struct bsg_lod_payload));

    /* Pre-allocate 4 cursor slots (enough for Quad view). */
    pl->cursor_alloc = 4;
    pl->cursors = (struct bsg_lod_view_cursor *)bu_malloc(
	pl->cursor_alloc * sizeof(struct bsg_lod_view_cursor),
	"bsg_lod_payload cursors");
    memset(pl->cursors, 0,
	   pl->cursor_alloc * sizeof(struct bsg_lod_view_cursor));
    pl->cursor_count = 0;

    bsg_lod_node_set_payload(n, pl);
    bsg_node_set_release_callback(n, _lod_node_free_cb);

    return (bsg_node *)n;
}


void
bsg_lod_node_set_ops(bsg_node *node,
		     struct bsg_lod_ops *ops,
		     void *user_data)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    if (!pl)
	return;
    pl->ops       = ops;
    pl->user_data = user_data;
}


struct bsg_lod_ops *
bsg_lod_node_ops(bsg_node *node)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    return pl ? pl->ops : NULL;
}


void *
bsg_lod_node_user_data(bsg_node *node)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    return pl ? pl->user_data : NULL;
}


void
bsg_lod_node_attach_level(bsg_node *lod_node, bsg_node *level_node)
{
    if (!lod_node || !level_node)
	return;
    bsg_node *n = (bsg_node *)lod_node;
    if (!_lod_node_is_lod(n))
	return;
    bsg_node *c = (bsg_node *)level_node;

    bsg_node_add_child(n, c);
}


static int
_lod_field_level(bsg_node *node, struct bsg_view *v)
{
    int selection = BSG_LOD_SELECT_AUTO;
    int active = -1;
    int level = 0;
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_LOD_SELECTION), &selection);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_LOD_ACTIVE_LEVEL), &active);

    if (selection == BSG_LOD_SELECT_FORCE_LEVEL)
	return (active < 0) ? 0 : active;

    size_t range_cnt = bsg_field_multi_count(bsg_node_field_ref(node, BSG_FIELD_LOD_RANGES));
    if (!v || range_cnt == 0)
	return (active < 0) ? 0 : active;

    for (size_t i = 0; i < range_cnt; i++) {
	double range = 0.0;
	if (!bsg_field_multi_double_at(bsg_node_field_ref(node, BSG_FIELD_LOD_RANGES), i, &range))
	    continue;
	if (v->gv_size <= range)
	    return level;
	level++;
    }

    return level;
}


struct bsg_lod_view_cursor *
bsg_lod_node_get_cursor(bsg_node *node, struct bsg_view *v)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    if (!pl || !v)
	return NULL;

    /* Linear scan — in practice cursor_count <= 4. */
    for (size_t i = 0; i < pl->cursor_count; i++) {
	if (pl->cursors[i].v == v)
	    return &pl->cursors[i];
    }

    /* Not found — create a new slot. */
    if (pl->cursor_count >= pl->cursor_alloc) {
	size_t new_alloc = pl->cursor_alloc * 2;
	pl->cursors = (struct bsg_lod_view_cursor *)bu_realloc(
	    pl->cursors,
	    new_alloc * sizeof(struct bsg_lod_view_cursor),
	    "bsg_lod_payload cursors grow");
	/* Zero new slots. */
	size_t added = new_alloc - pl->cursor_alloc;
	memset(&pl->cursors[pl->cursor_alloc], 0,
	       added * sizeof(struct bsg_lod_view_cursor));
	pl->cursor_alloc = new_alloc;
    }

    struct bsg_lod_view_cursor *c = &pl->cursors[pl->cursor_count++];
    memset(c, 0, sizeof(struct bsg_lod_view_cursor));
    c->v     = v;
    c->level = -1;  /* not yet selected */
    return c;
}


int
bsg_lod_node_active_level(bsg_node *node, struct bsg_view *v)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    if (!pl || !v)
	return -1;
    for (size_t i = 0; i < pl->cursor_count; i++) {
	if (pl->cursors[i].v == v)
	    return pl->cursors[i].level;
    }
    return -1;
}


int
bsg_lod_node_resolve_level(bsg_node *node, struct bsg_view *v)
{
    struct bsg_lod_payload *pl = _lod_payload(node);
    if (!pl)
	return -1;

    if (v && pl->ops) {
	bsg_lod_node_get_cursor(node, v);
	if (!pl->ops->is_stale || pl->ops->is_stale(node, v)) {
	    int level = pl->ops->select_level ? pl->ops->select_level(node, v) : 0;
	    if (pl->ops->activate_level)
		pl->ops->activate_level(node, v, level);
	}
	return bsg_lod_node_active_level(node, v);
    }

    return _lod_field_level(node, v);
}


int
bsg_lod_node_level_count(bsg_node *node)
{
    if (!node)
	return 0;
    bsg_node *n = (bsg_node *)node;
    if (!_lod_node_is_lod(n))
	return 0;
    return (int)BU_PTBL_LEN(&n->children);
}


bsg_node *
bsg_lod_node_insert_above(bsg_node *leaf, struct bsg_view *v)
{
    if (!leaf || !v)
	return NULL;

    bsg_node *sleaf = (bsg_node *)leaf;
    bsg_node *parent = (bsg_node *)sleaf->parent;
    if (!parent)
	return NULL;

    intmax_t loc = bu_ptbl_locate(&parent->children, (const long *)sleaf);
    if (loc < 0)
	return NULL;

    bsg_node *lod = bsg_lod_node_create(v);
    if (!lod)
	return NULL;

    bsg_node *slod = (bsg_node *)lod;
    slod->parent = parent;
    BU_PTBL_SET(&parent->children, (size_t)loc, slod);

    sleaf->parent = slod;
    bsg_lod_node_attach_level(lod, leaf);

    bsg_bump_rev_node((bsg_node *)parent);
    bsg_node_field_touch(parent, BSG_FIELD_CHILDREN);
    bsg_node_bbox_invalidate((bsg_node *)parent);

    return lod;
}

int
bsg_lod_source_ref_is_null(bsg_lod_source_ref ref)
{
    return ref.token ? 0 : 1;
}

static bsg_node *
_lod_source_node(bsg_lod_source_ref ref)
{
    bsg_node *node = (ref.token) ? (bsg_node *)ref.token : NULL;
    if (!node || !_lod_node_is_lod(node))
	return NULL;
    return node;
}

static bsg_lod_source_ref
_lod_source_ref(bsg_node *node)
{
    bsg_lod_source_ref ref = BSG_LOD_SOURCE_REF_NULL_INIT;
    if (!node || !_lod_node_is_lod(node))
	return ref;
    ref.token = (uintptr_t)node;
    ref.revision = bsg_node_revision(node);
    return ref;
}

int
bsg_lod_source_record_get(bsg_lod_source_ref ref, struct bsg_lod_source_record *record)
{
    bsg_node *node = _lod_source_node(ref);
    if (!node || !record)
	return 0;
    struct bsg_lod_payload *pl = _lod_payload(node);
    if (!pl)
	return 0;
    memset(record, 0, sizeof(*record));
    record->ref = _lod_source_ref(node);
    record->kind = pl->source_kind;
    record->identity = pl->source_identity ? pl->source_identity : (uint64_t)(uintptr_t)node;
    record->name = pl->source_name ? pl->source_name : NULL;
    record->level_count = bsg_lod_node_level_count(node);
    return 1;
}

static void
_cursor_to_state(struct bsg_lod_view_state *state, const struct bsg_lod_view_cursor *cursor)
{
    if (!state || !cursor)
	return;
    state->view = cursor->v;
    state->level = cursor->level;
    state->view_scale = cursor->view_scale;
    state->curve_scale = cursor->curve_scale;
    state->point_scale = cursor->point_scale;
    state->perspective_flag = cursor->perspective_flag;
    state->frame_revision = cursor->last_frame_rev;
    state->policy_tag = cursor->policy_tag;
}

static void
_state_to_cursor(struct bsg_lod_view_cursor *cursor, const struct bsg_lod_view_state *state)
{
    if (!cursor || !state)
	return;
    cursor->v = state->view;
    cursor->level = state->level;
    cursor->view_scale = state->view_scale;
    cursor->curve_scale = state->curve_scale;
    cursor->point_scale = state->point_scale;
    cursor->perspective_flag = state->perspective_flag;
    cursor->last_frame_rev = state->frame_revision;
    cursor->policy_tag = state->policy_tag;
}

struct bsg_lod_source_policy_state {
    struct bsg_lod_source_policy policy;
};

static int
_lod_source_select_level(bsg_node *node, struct bsg_view *v)
{
    struct bsg_lod_source_policy_state *st = (struct bsg_lod_source_policy_state *)bsg_lod_node_user_data(node);
    if (!st || !st->policy.select)
	return 0;
    struct bsg_lod_source_record record;
    if (!bsg_lod_source_record_get(_lod_source_ref(node), &record))
	return 0;
    int level = st->policy.select(&record, v, st->policy.policy_data);
    return (level < 0) ? 0 : level;
}

static void
_lod_source_activate_level(bsg_node *node, struct bsg_view *v, int level)
{
    struct bsg_lod_view_cursor *cursor = bsg_lod_node_get_cursor(node, v);
    if (!cursor || !v)
	return;

    struct bsg_lod_view_state state = BSG_LOD_VIEW_STATE_INIT;
    _cursor_to_state(&state, cursor);
    state.view = v;
    state.level = level;
    state.view_scale = v->gv_size;
    state.perspective_flag = (SMALL_FASTF < v->gv_perspective) ? 1 : 0;
    state.frame_revision = v->gv_frame_rev;

    struct bsg_lod_source_policy_state *st = (struct bsg_lod_source_policy_state *)bsg_lod_node_user_data(node);
    if (st && st->policy.activate) {
	struct bsg_lod_source_record record;
	if (bsg_lod_source_record_get(_lod_source_ref(node), &record))
	    st->policy.activate(&record, v, level, &state, st->policy.policy_data);
    }

    _state_to_cursor(cursor, &state);
}

static int
_lod_source_is_stale(bsg_node *node, struct bsg_view *v)
{
    struct bsg_lod_view_cursor *cursor = bsg_lod_node_get_cursor(node, v);
    if (!cursor || !v)
	return 0;
    if (cursor->level < 0)
	return 1;

    struct bsg_lod_view_state state = BSG_LOD_VIEW_STATE_INIT;
    _cursor_to_state(&state, cursor);

    struct bsg_lod_source_policy_state *st = (struct bsg_lod_source_policy_state *)bsg_lod_node_user_data(node);
    if (st && st->policy.stale) {
	struct bsg_lod_source_record record;
	if (bsg_lod_source_record_get(_lod_source_ref(node), &record))
	    return st->policy.stale(&record, v, &state, st->policy.policy_data);
    }

    if (!ZERO(state.view_scale - v->gv_size))
	return 1;
    if (state.perspective_flag != ((SMALL_FASTF < v->gv_perspective) ? 1 : 0))
	return 1;
    return 0;
}

static void
_lod_source_policy_free(bsg_node *node)
{
    struct bsg_lod_source_policy_state *st = (struct bsg_lod_source_policy_state *)bsg_lod_node_user_data(node);
    if (!st)
	return;
    if (st->policy.free)
	st->policy.free(st->policy.policy_data);
    bu_free(st, "bsg_lod_source_policy_state");
}

static struct bsg_lod_ops _source_policy_ops = {
    _lod_source_select_level,
    _lod_source_activate_level,
    _lod_source_is_stale,
    _lod_source_policy_free
};

static int
_lod_source_policy_apply(bsg_node *lod, const struct bsg_lod_source_policy *policy)
{
    struct bsg_lod_payload *pl = _lod_payload(lod);
    if (!pl || !policy)
	return 0;

    if (pl->ops && pl->ops->free)
	pl->ops->free(lod);
    pl->ops = NULL;
    pl->user_data = NULL;

    if (pl->source_name)
	bu_free(pl->source_name, "bsg_lod_payload source name");
    pl->source_name = NULL;
    pl->source_kind = policy->kind;
    pl->source_identity = policy->identity ? policy->identity : (uint64_t)(uintptr_t)lod;
    if (policy->name)
	pl->source_name = bu_strdup(policy->name);

    struct bsg_lod_source_policy_state *st;
    BU_GET(st, struct bsg_lod_source_policy_state);
    memset(st, 0, sizeof(*st));
    st->policy = *policy;
    bsg_lod_node_set_ops(lod, &_source_policy_ops, st);

    bsg_bump_rev_node(lod);
    bsg_node_bbox_invalidate(lod);

    return 1;
}

bsg_lod_source_ref
bsg_lod_source_insert_above(bsg_node *leaf, struct bsg_view *view, const struct bsg_lod_source_policy *policy)
{
    if (!leaf || !view || !policy)
	return (bsg_lod_source_ref)BSG_LOD_SOURCE_REF_NULL_INIT;

    bsg_node *parent = bsg_node_parent(leaf);
    bsg_node *lod = (parent && _lod_node_is_lod(parent)) ?
	parent : bsg_lod_node_insert_above(leaf, view);
    if (!lod || !_lod_source_policy_apply(lod, policy))
	return (bsg_lod_source_ref)BSG_LOD_SOURCE_REF_NULL_INIT;

    return _lod_source_ref(lod);
}


bsg_lod_source_ref
bsg_lod_source_insert_above_scene_ref(bsg_scene_ref leaf, struct bsg_view *view, const struct bsg_lod_source_policy *policy)
{
    return bsg_lod_source_insert_above((bsg_node *)leaf.opaque, view, policy);
}


int
bsg_lod_source_view_state_ensure(bsg_lod_source_ref ref, struct bsg_view *view, struct bsg_lod_view_state *state_out)
{
    bsg_node *node = _lod_source_node(ref);
    struct bsg_lod_view_cursor *cursor = bsg_lod_node_get_cursor(node, view);
    if (!cursor)
	return 0;
    if (state_out)
	_cursor_to_state(state_out, cursor);
    return 1;
}

int
bsg_lod_source_view_state_invalidate(bsg_lod_source_ref ref, struct bsg_view *view)
{
    bsg_node *node = _lod_source_node(ref);
    struct bsg_lod_view_cursor *cursor = bsg_lod_node_get_cursor(node, view);
    if (!cursor)
	return 0;
    cursor->level = -1;
    return 1;
}

int
bsg_lod_source_view_policy_tag_sync(bsg_lod_source_ref ref, struct bsg_view *view, int policy_tag)
{
    bsg_node *node = _lod_source_node(ref);
    struct bsg_lod_view_cursor *cursor = bsg_lod_node_get_cursor(node, view);
    if (!cursor)
	return 0;
    if (cursor->policy_tag == policy_tag)
	return 0;
    cursor->policy_tag = policy_tag;
    cursor->level = -1;
    return 1;
}

bsg_lod_source_ref
bsg_lod_source_from_node(bsg_node *node)
{
    return _lod_source_ref(node);
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
