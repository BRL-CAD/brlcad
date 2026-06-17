/*                    O V E R L A Y . C
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
/** @file libbsg/overlay.c
 *
 * Overlay ownership, lifecycle, and query helpers.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "bsg/defines.h"
#include "bsg/draw_ctx.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_set.h"
#include "bsg/overlay.h"
#include "bsg/payload.h"
#include "bsg_private.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "draw_intent_private.h"
#include "draw_set_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "overlay_private.h"
#include "payload_private.h"
#include "scene_store_private.h"


/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static const char *
_strip_lead_slash(const char *path)
{
    return (path && path[0] == '/') ? path + 1 : path;
}

static int
_path_matches(const char *lhs, const char *rhs)
{
    lhs = _strip_lead_slash(lhs);
    rhs = _strip_lead_slash(rhs);
    if (!lhs || !rhs)
return 0;
    if (BU_STR_EQUAL(lhs, rhs))
return 1;
    size_t llen = strlen(lhs);
    size_t rlen = strlen(rhs);
    if (llen < rlen && !bu_strncmp(rhs, lhs, llen) && rhs[llen] == '/')
return 1;
    if (rlen < llen && !bu_strncmp(lhs, rhs, rlen) && lhs[rlen] == '/')
return 1;
    return 0;
}

static struct bsg_overlay_info *
_overlay_info(bsg_node *node)
{
    if (!node || !node->i)
return NULL;
    return node->i->overlay;
}

static const struct bsg_overlay_info *
_overlay_info_const(const bsg_node *node)
{
    if (!node || !node->i)
return NULL;
    return node->i->overlay;
}

static void
_collect_nodes_recursive(bsg_node *root, struct bu_ptbl *nodes,
 int (*predicate)(const bsg_node *, void *), void *ctx)
{
    if (!root)
return;
    if (!predicate || predicate(root, ctx))
bu_ptbl_ins(nodes, (long *)root);
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
bsg_node *child = (bsg_node *)BU_PTBL_GET(&root->children, i);
_collect_nodes_recursive(child, nodes, predicate, ctx);
    }
}

struct owner_ctx {
    const void *owner;
    const bsg_node *skip;
};

static int
_match_owner(const bsg_node *node, void *data)
{
    const struct bsg_overlay_info *info = _overlay_info_const(node);
    const struct owner_ctx *ctx = (const struct owner_ctx *)data;
    return (info && ctx && ctx->owner && node != ctx->skip && info->owner == ctx->owner);
}

struct role_ctx {
    bsg_overlay_role role;
};

static int
_match_role(const bsg_node *node, void *data)
{
    const struct bsg_overlay_info *info = _overlay_info_const(node);
    const struct role_ctx *ctx = (const struct role_ctx *)data;
    return (info && ctx && info->role == ctx->role);
}

struct source_ctx {
    const char *path;
};

static int
_match_source(const bsg_node *node, void *data)
{
    const struct bsg_overlay_info *info = _overlay_info_const(node);
    const struct source_ctx *ctx = (const struct source_ctx *)data;
    if (!info || !ctx || !ctx->path)
return 0;
    if (info->lifecycle != BSG_OVERLAY_LC_AUTO_REMOVE_ON_SOURCE)
return 0;
    if (!BU_VLS_IS_INITIALIZED(&info->source_path) || bu_vls_strlen(&info->source_path) == 0)
return 0;
    return _path_matches(bu_vls_cstr(&info->source_path), ctx->path);
}

static size_t
_clear_nodes(struct bu_ptbl *nodes)
{
    size_t removed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(nodes); i++) {
bsg_node *node = (bsg_node *)BU_PTBL_GET(nodes, i);
if (!node)
    continue;
bsg_node_destroy(node);
removed++;
    }
    return removed;
}

static size_t
_collect_from_root(bsg_node *root, struct bu_ptbl *nodes,
   int (*predicate)(const bsg_node *, void *), void *ctx)
{
    size_t before = BU_PTBL_LEN(nodes);
    _collect_nodes_recursive(root, nodes, predicate, ctx);
    return BU_PTBL_LEN(nodes) - before;
}


/* ------------------------------------------------------------------ */

bsg_node *
bsg_find_overlay_group(bsg_node *draw_root)
{
    bsg_node *root = (bsg_node *)draw_root;
    if (!root)
return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
bsg_node *g = (bsg_node *)BU_PTBL_GET(&root->children, i);
if (g && BU_STR_EQUAL("_overlays", bsg_node_name(g)))
    return (bsg_node *)g;
    }
    return NULL;
}


bsg_node *
bsg_ensure_overlay_group(bsg_node *draw_root, struct bsg_view *v)
{
    bsg_node *existing = bsg_find_overlay_group(draw_root);
    if (existing)
return existing;

    if (!draw_root || !v)
return NULL;

    bsg_node *ov = bsg_scene_node_create(v, BSG_SOURCE_CHILD);
    if (!ov)
return NULL;

    bsg_node_set_object_type(ov, bsg_group_type());
    bsg_node_set_visible_state(ov, 1);
    bsg_node_set_name(ov, "_overlays");
    bsg_node_add_child(draw_root, ov);
    bsg_node_set_draw_intent(ov, bsg_draw_intent_create_overlay("_overlays"));

    return (bsg_node *)ov;
}


int
bsg_overlay_append_scene(struct bsg_view *v, bsg_scene_ref overlay)
{
    bsg_node *node = (bsg_node *)overlay.opaque;
    if (!v || !node)
	return 0;

    bsg_node *root = bsg_view_scene_root(v);
    if (!root)
	return 0;

    bsg_node *ov = bsg_ensure_overlay_group(root, v);
    if (!ov)
	return 0;

    bsg_node_add_child(ov, node);
    bsg_bump_rev_node(ov);
    bsg_node_bbox_invalidate(ov);
    return 1;
}


void
bsg_erase_overlay_by_name(bsg_node *draw_root, const char *name)
{
    bsg_node *root = (bsg_node *)draw_root;
    if (!root)
return;

    bsg_node *ov = (bsg_node *)bsg_find_overlay_group(draw_root);
    if (!ov)
return;

    struct bsg_draw_ctx *ctx = _ctx_of_node(root);
    struct bsg_scene_recycle_pool *fso = (ctx && ctx->fso) ?
	(struct bsg_scene_recycle_pool *)ctx->fso : NULL;

    struct bu_ptbl snap = BU_PTBL_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&ov->children); i++)
bu_ptbl_ins(&snap, BU_PTBL_GET(&ov->children, i));

    for (size_t i = 0; i < BU_PTBL_LEN(&snap); i++) {
bsg_node *sp = (bsg_node *)BU_PTBL_GET(&snap, i);
if (!sp || !BU_STR_EQUAL(name, bsg_node_name(sp)))
    continue;

bu_ptbl_rm(&ov->children, (const long *)sp);
bsg_bump_rev_node(draw_root);
sp->parent = NULL;
bsg_overlay_info_clear(sp);
struct bsg_scene_recycle_pool *sfso =
    fso ? fso : bsg_scene_node_recycle_pool(sp);
if (sfso)
    bsg_scene_recycle_release(sfso, sp);
    }
    bu_ptbl_free(&snap);

    if (BU_PTBL_LEN(&ov->children) == 0) {
bu_ptbl_rm(&root->children, (const long *)ov);
ov->parent = NULL;
bsg_overlay_info_clear(ov);
struct bsg_scene_recycle_pool *ofso = bsg_scene_node_recycle_pool(ov);
if (ofso)
    bsg_scene_recycle_release(ofso, ov);
    }
}


bsg_scene_ref
bsg_overlay_find_scene(struct bsg_view *v, const char *name)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    if (!v || !name)
	return ref;

    bsg_node *ov = bsg_find_overlay_group(bsg_view_scene_root(v));
    if (!ov)
	return ref;

    for (size_t i = 0; i < BU_PTBL_LEN(&ov->children); i++) {
	bsg_node *sp = (bsg_node *)BU_PTBL_GET(&ov->children, i);
	if (sp && BU_STR_EQUAL(name, bsg_node_name(sp))) {
	    ref.opaque = sp;
	    return ref;
	}
    }

    return ref;
}

void
bsg_overlay_erase_name(struct bsg_view *v, const char *name)
{
    if (!v)
	return;
    bsg_erase_overlay_by_name(bsg_view_scene_root(v), name);
}


int
bsg_overlay_register_owner(bsg_node *overlay_node,
   const void *owner,
   bsg_overlay_role role,
   bsg_overlay_class overlay_class,
   bsg_overlay_lifecycle lifecycle,
   bsg_overlay_order ordering,
   const char *source_path,
   int sort_order)
{
    if (!overlay_node)
return 0;

    if (!overlay_node->i)
return 0;

    if (!overlay_node->i->overlay) {
BU_ALLOC(overlay_node->i->overlay, struct bsg_overlay_info);
memset(overlay_node->i->overlay, 0, sizeof(struct bsg_overlay_info));
BU_VLS_INIT(&overlay_node->i->overlay->source_path);
    }

    struct bsg_overlay_info *info = overlay_node->i->overlay;
    info->owner = owner;
    info->role = role;
    info->overlay_class = overlay_class;
    info->lifecycle = lifecycle;
    info->ordering = ordering;
    info->sort_order = sort_order;
    bu_vls_trunc(&info->source_path, 0);
    if (source_path)
bu_vls_sprintf(&info->source_path, "%s", _strip_lead_slash(source_path));

    bsg_node_set_payload_type(overlay_node,
    bsg_node_get_payload_type(overlay_node) | BSG_PAYLOAD_OVERLAY);

    return 1;
}


int
bsg_overlay_register_scene_owner(bsg_scene_ref overlay,
				 const void *owner,
				 bsg_overlay_role role,
				 bsg_overlay_class overlay_class,
				 bsg_overlay_lifecycle lifecycle,
				 bsg_overlay_order ordering,
				 const char *source_path,
				 int sort_order)
{
    return bsg_overlay_register_owner((bsg_node *)overlay.opaque, owner,
	    role, overlay_class, lifecycle, ordering, source_path, sort_order);
}


bsg_node *
bsg_overlay_replace(struct bsg_view *v, const void *owner, bsg_node *overlay_node)
{
    if (!v || !owner)
return overlay_node;

    struct bu_ptbl nodes = BU_PTBL_INIT_ZERO;
    struct owner_ctx ctx = {owner, overlay_node};
    _collect_from_root(bsg_view_scene_root(v), &nodes, _match_owner, &ctx);
    _collect_from_root((bsg_node *)v->gv_hud_root, &nodes, _match_owner, &ctx);
    (void)_clear_nodes(&nodes);
    bu_ptbl_free(&nodes);
    return overlay_node;
}


size_t
bsg_overlay_clear_owned(struct bsg_view *v, const void *owner)
{
    if (!v || !owner)
return 0;

    struct bu_ptbl nodes = BU_PTBL_INIT_ZERO;
    struct owner_ctx ctx = {owner, NULL};
    _collect_from_root(bsg_view_scene_root(v), &nodes, _match_owner, &ctx);
    _collect_from_root((bsg_node *)v->gv_hud_root, &nodes, _match_owner, &ctx);
    size_t removed = _clear_nodes(&nodes);
    bu_ptbl_free(&nodes);
    return removed;
}


size_t
bsg_overlay_query_by_role(bsg_node *root, bsg_overlay_role role, struct bu_ptbl *out)
{
    if (!root || !out)
return 0;
    struct role_ctx ctx = {role};
    return _collect_from_root(root, out, _match_role, &ctx);
}


size_t
bsg_overlay_auto_remove(bsg_node *root, const char *source_path)
{
    if (!root || !source_path)
return 0;

    struct bu_ptbl nodes = BU_PTBL_INIT_ZERO;
    struct source_ctx ctx = {source_path};
    _collect_from_root(root, &nodes, _match_source, &ctx);
    size_t removed = _clear_nodes(&nodes);
    bu_ptbl_free(&nodes);
    return removed;
}


const struct bsg_overlay_info *
bsg_overlay_info_get(const bsg_node *overlay_node)
{
    return _overlay_info_const(overlay_node);
}


void
bsg_overlay_info_clear(bsg_node *overlay_node)
{
    struct bsg_overlay_info *info = _overlay_info(overlay_node);
    if (!info)
return;
    if (BU_VLS_IS_INITIALIZED(&info->source_path))
bu_vls_free(&info->source_path);
    BU_PUT(info, struct bsg_overlay_info);
    overlay_node->i->overlay = NULL;
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
