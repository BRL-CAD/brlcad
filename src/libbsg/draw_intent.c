/*                  D R A W _ I N T E N T . C
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
/** @file libbsg/draw_intent.c
 *
 * Explicit draw-intent metadata attached to BSG scene groups.
 *
 * Implementation of bsg/draw_intent.h — lifecycle, node binding,
 * accessors, and scene-level actions.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/visibility.h"
#include "bsg/draw_intent.h"
#include "bsg/overlay.h"
#include "draw_intent_private.h"
#include "draw_source_private.h"
#include "node_private.h"
#include "overlay_private.h"
#include "visibility_private.h"

struct bsg_draw_operation
bsg_draw_operation_make(bsg_draw_operation_kind kind, const char *path)
{
    struct bsg_draw_operation op;
    op.op_kind = kind;
    op.op_path = path;
    op.op_value = 0.0;
    return op;
}

struct bsg_draw_operation
bsg_draw_operation_make_value(bsg_draw_operation_kind kind, const char *path,
			      double value)
{
    struct bsg_draw_operation op;
    op.op_kind = kind;
    op.op_path = path;
    op.op_value = value;
    return op;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

struct bsg_draw_intent *
bsg_draw_intent_create(const char *path, bsg_draw_mode mode)
{
    struct bsg_draw_intent *di;
    struct bsg_appearance_settings appearance = BSG_APPEARANCE_SETTINGS_INIT;
    BU_GET(di, struct bsg_draw_intent);
    bu_vls_init(&di->di_path);
    if (path)
	bu_vls_sprintf(&di->di_path, "%s", path);
    di->di_mode    = mode;
    di->di_lod     = BSG_DRAW_LOD_AUTO;
    di->di_mixed   = 0;
    appearance.draw_mode = mode;
    appearance.mixed_modes = 0;
    di->di_appearance = appearance;
    di->di_overlay = 0;
    return di;
}


struct bsg_draw_intent *
bsg_draw_intent_create_overlay(const char *name)
{
    struct bsg_draw_intent *di;
    struct bsg_appearance_settings appearance = BSG_APPEARANCE_SETTINGS_INIT;
    BU_GET(di, struct bsg_draw_intent);
    bu_vls_init(&di->di_path);
    if (name)
	bu_vls_sprintf(&di->di_path, "%s", name);
    di->di_mode    = BSG_DRAW_MODE_WIRE;
    di->di_lod     = BSG_DRAW_LOD_OFF;
    di->di_mixed   = 0;
    appearance.draw_mode = BSG_DRAW_MODE_WIRE;
    appearance.mixed_modes = 0;
    di->di_appearance = appearance;
    di->di_overlay = 1;
    return di;
}


void
bsg_draw_intent_free(struct bsg_draw_intent *di)
{
    if (!di)
	return;
    bu_vls_free(&di->di_path);
    BU_PUT(di, struct bsg_draw_intent);
}


/* -----------------------------------------------------------------------
 * Node binding
 * ----------------------------------------------------------------------- */

void
bsg_node_set_draw_intent(bsg_node *node, struct bsg_draw_intent *di)
{
    if (!node)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    if (ni->draw_intent)
	bsg_draw_intent_free(ni->draw_intent);
    ni->draw_intent = di;
}


struct bsg_draw_intent *
bsg_node_get_draw_intent(const bsg_node *node)
{
    if (!node)
	return NULL;
    return node->i ? node->i->draw_intent : NULL;
}

void
bsg_scene_set_draw_intent(bsg_scene_ref ref, struct bsg_draw_intent *di)
{
    bsg_node_set_draw_intent((bsg_node *)ref.opaque, di);
}

struct bsg_draw_intent *
bsg_scene_draw_intent(bsg_scene_ref ref)
{
    return bsg_node_get_draw_intent((const bsg_node *)ref.opaque);
}


int
bsg_scene_draw_intent_revalidate(bsg_scene_ref root,
				 const struct bsg_db_event *event)
{
    return bsg_draw_intent_revalidate((bsg_node *)root.opaque, event);
}


/* -----------------------------------------------------------------------
 * Accessors
 * ----------------------------------------------------------------------- */

const char *
bsg_draw_intent_path(const struct bsg_draw_intent *di)
{
    if (!di)
	return NULL;
    return bu_vls_cstr(&di->di_path);
}


void
bsg_draw_intent_set_path(struct bsg_draw_intent *di, const char *path)
{
    if (!di)
	return;
    bu_vls_trunc(&di->di_path, 0);
    if (path)
	bu_vls_sprintf(&di->di_path, "%s", path);
}


bsg_draw_mode
bsg_draw_intent_mode(const struct bsg_draw_intent *di)
{
    if (!di)
	return BSG_DRAW_MODE_WIRE;
    return di->di_mode;
}


void
bsg_draw_intent_set_mode(struct bsg_draw_intent *di, bsg_draw_mode mode)
{
    if (!di)
	return;
    di->di_mode = mode;
    di->di_appearance.draw_mode = mode;
}


bsg_draw_lod_policy
bsg_draw_intent_lod(const struct bsg_draw_intent *di)
{
    if (!di)
	return BSG_DRAW_LOD_AUTO;
    return di->di_lod;
}


int
bsg_draw_intent_appearance(const struct bsg_draw_intent *di,
			   struct bsg_appearance_settings *out)
{
    if (!di || !out)
	return 0;
    *out = di->di_appearance;
    return 1;
}


void
bsg_draw_intent_set_appearance(struct bsg_draw_intent *di,
			       const struct bsg_appearance_settings *settings)
{
    if (!di || !settings)
	return;
    di->di_appearance = *settings;
    di->di_mode = (bsg_draw_mode)settings->draw_mode;
    di->di_mixed = settings->mixed_modes;
}


int
bsg_draw_intent_is_overlay(const struct bsg_draw_intent *di)
{
    if (!di)
	return 0;
    return di->di_overlay;
}


/* -----------------------------------------------------------------------
 * Scene-level actions
 * ----------------------------------------------------------------------- */

/*
 * Strip a single leading '/' from @p s if present.  Used to normalize
 * paths before comparison (db_path_to_string prepends '/').
 */
static const char *
_strip_lead_slash(const char *s)
{
    if (s && *s == '/')
	return s + 1;
    return s;
}


/*
 * DFS helper for bsg_draw_intent_find.  Returns the first intent-bearing
 * node under @p node (including @p node itself) whose di_path equals
 * @p norm_path, or NULL.
 */
static bsg_node *
_find_intent_dfs(bsg_node *node, const char *norm_path)
{
    if (!node)
	return NULL;

    const struct bsg_draw_intent *di = bsg_node_get_draw_intent(node);
    if (di) {
	const char *gpath = _strip_lead_slash(bu_vls_cstr(&di->di_path));
	if (BU_STR_EQUAL(gpath, norm_path))
	    return node;
	/* Intent-bearing node with a non-matching path: do NOT recurse. */
	return NULL;
    }

    /* No intent yet — this is an intermediate path-component group.
     * Search its children. */
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	bsg_node *child = (bsg_node *)BU_PTBL_GET(&node->children, i);
	bsg_node *found = _find_intent_dfs(child, norm_path);
	if (found)
	    return found;
    }
    return NULL;
}


bsg_node *
bsg_draw_intent_find(bsg_node *root, const char *path)
{
    if (!root || !path)
	return NULL;

    const char *norm = _strip_lead_slash(path);

    /* Search each direct child of root. */
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&root->children, i);
	bsg_node *found = _find_intent_dfs(g, norm);
	if (found)
	    return found;
    }
    return NULL;
}


/*
 * DFS helper for bsg_collect_draw_groups.  Collects all intent-bearing
 * leaf groups under @p node (including @p node itself).  When
 * @p include_overlays is zero, overlay intents are skipped.
 */
static void
_collect_intent_dfs(bsg_node *node, struct bu_ptbl *groups,
		    int include_overlays)
{
    if (!node)
	return;

    const struct bsg_draw_intent *di = bsg_node_get_draw_intent(node);
    if (di) {
	if (include_overlays || !di->di_overlay)
	    bu_ptbl_ins(groups, (long *)node);
	/* Intent-bearing node: do not recurse into its children. */
	return;
    }

    /* No intent — intermediate path component; recurse. */
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	bsg_node *child = (bsg_node *)BU_PTBL_GET(&node->children, i);
	_collect_intent_dfs(child, groups, include_overlays);
    }
}


void
bsg_collect_draw_groups(bsg_node *root, struct bu_ptbl *groups,
			int include_overlays)
{
    if (!root || !groups)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&root->children, i);
	_collect_intent_dfs(g, groups, include_overlays);
    }
}


/* -----------------------------------------------------------------------
 * D2 Action API
 * ----------------------------------------------------------------------- */

/*
 * Return non-zero if @p prefix is a proper path prefix of @p path.
 * E.g. "a" is a prefix of "a/b" but not of "ab" or "a" itself.
 */
static int
_is_path_prefix(const char *prefix, const char *path)
{
    if (!prefix || !path)
	return 0;
    size_t plen = strlen(prefix);
    if (bu_strncmp(path, prefix, plen) != 0)
	return 0;
    /* Accept exactly "prefix/" as the next character. */
    return path[plen] == '/';
}


int
bsg_draw_intent_revalidate(bsg_node *root, const struct bsg_db_event *event)
{
    if (!root || !event || !event->dbe_path)
	return 0;

    int count = 0;
    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_collect_draw_groups(root, &groups, 1 /* include overlays */);

    if (event->dbe_kind == BSG_DB_EVENT_RENAMED) {
	/* Renamed: find intents matching the OLD path, update to new. */
	const char *old_norm = event->dbe_old_path
	    ? _strip_lead_slash(event->dbe_old_path)
	    : NULL;
	const char *new_norm = _strip_lead_slash(event->dbe_path);
	if (!old_norm) {
	    bu_ptbl_free(&groups);
	    return 0;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	    bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	    if (!g) continue;
	    const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g);
	    if (!di) continue;
	    const char *gpath = _strip_lead_slash(bu_vls_cstr(&di->di_path));
	    if (BU_STR_EQUAL(gpath, old_norm)) {
		bsg_draw_intent_set_path((struct bsg_draw_intent *)di, new_norm);
		count++;
	    }
	}
	count += (int)bsg_overlay_auto_remove(root, old_norm);
	bu_ptbl_free(&groups);
	return count;
    }

    const char *norm_path = _strip_lead_slash(event->dbe_path);
    count += (int)bsg_overlay_auto_remove(root, norm_path);
    struct bu_ptbl to_remove = BU_PTBL_INIT_ZERO;
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!g)
	    continue;
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g);
	if (!di)
	    continue;
	const char *gpath = _strip_lead_slash(bu_vls_cstr(&di->di_path));
	if (!BU_STR_EQUAL(gpath, norm_path))
	    continue;

	switch (event->dbe_kind) {
	    case BSG_DB_EVENT_MODIFIED:
		bsg_scene_node_invalidate(g);
		count++;
		break;
	    case BSG_DB_EVENT_REMOVED:
		bu_ptbl_ins(&to_remove, (long *)g);
		count++;
		break;
	    default:
		break;
	}
    }
    for (size_t i = 0; i < BU_PTBL_LEN(&to_remove); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&to_remove, i);
	if (g)
	    bsg_node_destroy(g);
    }
    bu_ptbl_free(&to_remove);
    bu_ptbl_free(&groups);
    return count;
}


int
bsg_draw_intent_erase_by_path(bsg_node *root, const char *path)
{
    if (!root || !path)
	return 0;

    bsg_node *g = bsg_draw_intent_find(root, path);
    if (g) {
	/* draw-intent actions operate on draw-tree-owned groups; callers pass
	 * roots from that tree, so destroy is the expected erase operation. */
	bsg_node_destroy(g);
	return 1;
    }
    return 0;
}


int
bsg_draw_intent_erase(bsg_node *root, struct bsg_draw_intent *intent)
{
    if (!root || !intent)
	return 0;

    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_collect_draw_groups(root, &groups, 1 /* include overlays */);
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!g)
	    continue;
	if (bsg_node_get_draw_intent(g) == intent) {
	    bsg_node_destroy(g);
	    bu_ptbl_free(&groups);
	    return 1;
	}
    }
    bu_ptbl_free(&groups);
    return 0;
}


int
bsg_draw_intent_simplify(bsg_node *root)
{
    if (!root)
	return 0;

    int removed = 0;

    /* Pass 1: collect all intent-bearing groups. */
    struct bu_ptbl groups;
    bu_ptbl_init(&groups, 64, "simplify groups");
    bsg_collect_draw_groups(root, &groups, 1 /* include overlays */);

    size_t n = BU_PTBL_LEN(&groups);

    /* For each group, check whether another group earlier in the list
     * is a proper path prefix (subsumes it) or has the same path
     * (duplicate).  Mark redundant groups by NULL-ing their slot so
     * we don't double-free. */
    for (size_t i = 0; i < n; i++) {
	bsg_node *gi = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!gi)
	    continue;
	const struct bsg_draw_intent *di_i = bsg_node_get_draw_intent(gi);
	if (!di_i)
	    continue;
	const char *pi = _strip_lead_slash(bu_vls_cstr(&di_i->di_path));

	for (size_t j = 0; j < i; j++) {
	    bsg_node *gj = (bsg_node *)BU_PTBL_GET(&groups, j);
	    if (!gj)
		continue;
	    const struct bsg_draw_intent *di_j = bsg_node_get_draw_intent(gj);
	    if (!di_j)
		continue;
	    const char *pj = _strip_lead_slash(bu_vls_cstr(&di_j->di_path));

	    if (BU_STR_EQUAL(pi, pj) || _is_path_prefix(pj, pi)) {
		/* gi is subsumed by or duplicated by gj — remove gi. */
		bsg_node_destroy(gi);
		BU_PTBL_SET(&groups, i, NULL);
		removed++;
		break;
	    }
	}
    }

    bu_ptbl_free(&groups);
    return removed;
}


void
bsg_draw_intent_collect_visible(bsg_node *root, struct bu_ptbl *out,
				const struct bsg_view *v)
{
    if (!root || !out)
	return;

    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_collect_draw_groups(root, &groups, 0 /* no overlays */);
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!g)
	    continue;
	if (!bsg_visibility_visible_in_view(g, v))
	    continue;
	struct bsg_view *owner = bsg_node_get_view(g);
	if (owner && owner != (struct bsg_view *)v)
	    continue;
	bu_ptbl_ins(out, (long *)g);
    }
    bu_ptbl_free(&groups);
}


void
bsg_draw_intent_collect_for_export(bsg_node *root, struct bu_ptbl *out,
				   unsigned long long flags)
{
    if (!root || !out)
	return;

    int include_overlays = (flags & BSG_EXPORT_INCLUDE_OVERLAYS) ? 1 : 0;
    int shaded_only      = (flags & BSG_EXPORT_SHADED_ONLY)      ? 1 : 0;

    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_collect_draw_groups(root, &groups, include_overlays);
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!g)
	    continue;
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g);
	if (!di)
	    continue;
	if (shaded_only) {
	    bsg_draw_mode m = di->di_mode;
	    if (m != BSG_DRAW_MODE_SHADED_BOTS &&
		m != BSG_DRAW_MODE_SHADED      &&
		m != BSG_DRAW_MODE_EVAL_POINTS)
		continue;
	}
	bu_ptbl_ins(out, (long *)g);
    }
    bu_ptbl_free(&groups);
}


void
bsg_draw_intent_match(bsg_node *root, const char *pattern,
		      struct bu_ptbl *out)
{
    if (!root || !pattern || !out)
	return;

    struct bu_ptbl groups = BU_PTBL_INIT_ZERO;
    bsg_collect_draw_groups(root, &groups, 1 /* include overlays */);
    for (size_t i = 0; i < BU_PTBL_LEN(&groups); i++) {
	bsg_node *g = (bsg_node *)BU_PTBL_GET(&groups, i);
	if (!g)
	    continue;
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g);
	if (!di)
	    continue;
	const char *gpath = _strip_lead_slash(bu_vls_cstr(&di->di_path));
	if (bu_path_match(pattern, gpath, 0) == 0)
	    bu_ptbl_ins(out, (long *)g);
    }
    bu_ptbl_free(&groups);
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
