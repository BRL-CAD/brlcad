/*                    D R A W _ S E T . H
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
/** @addtogroup libbsg
 *
 * @brief
 * Pure-BSG draw-tree helpers that are independent of GED types.
 *
 * These functions operate on typed scene refs and @c struct @c bsg_view
 * pointers.  They carry no dependency on
 * @c struct @c ged or @c ged_private.h, making them safe to implement in
 * libbsg.  GED-specific functionality (coloring, draw-scene callbacks,
 * database lookups) remains in libged's GED draw-scene implementation.
 *
 * Together with scene refs and actions these form the tree-navigation layer
 * used by drawing producers and command implementations.
 */
/** @{ */
/* @file bsg/draw_set.h */

#ifndef BSG_DRAW_SET_H
#define BSG_DRAW_SET_H

#include "common.h"
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

struct bsg_view;          /* forward-declare to avoid circular includes */

typedef int (*bsg_scene_path_match_fn)(bsg_scene_ref shape_ref, void *match_ctx);

/**
 * Return the depth of node @p g in its BSG tree.
 *
 * A node with no parent has depth 0; immediate children have depth 1; and so
 * on.  The walk follows the @c parent pointer chain, so only nodes that are
 * actually linked into a tree give a meaningful result.
 *
 * Replaces the file-private @c _sg_tree_depth() helper formerly in
 * libged's draw-scene implementation.
 */
BSG_EXPORT extern int
bsg_scene_draw_tree_depth(bsg_scene_ref ref);


/**
 * Find a BSG_NODE_GROUP child of @p parent named exactly @p name.
 *
 * Returns the matching child, or NULL when no such child exists.
 * Does NOT create a new child if the name is absent — use
 * bsg_group_ensure_child() for that behaviour.
 *
 * Replaces the linear search extracted from @c _sg_find_or_create_child_group
 * formerly in libged's draw-scene implementation.
 */
BSG_EXPORT extern bsg_scene_ref
bsg_scene_group_find_child(bsg_scene_ref parent, const char *name);


/**
 * Find or create a BSG_NODE_GROUP child of @p parent named @p name.
 *
 * When a child with the given name already exists it is returned directly.
 * Otherwise a new BSG_NODE_GROUP node is allocated for @p v, linked into the
 * tree, and returned.
 *
 * @p dp_hint is accepted for legacy call sites and ignored; database identity
 * is owned by application-level draw-source payloads.
 * Pass @c NULL if the caller has no corresponding database object.  For GED
 * draw-trees the caller should pass @c (void*)dp from a prior @c db_lookup()
 * call so that path-matching logic has a fast handle.
 *
 * Returns NULL on allocation failure.
 *
 * Replaces the @c _sg_find_or_create_child_group() helper formerly in
 * libged's draw-scene implementation (minus the db_lookup call which belongs
 * in the libged wrapper).
 */
BSG_EXPORT extern bsg_scene_ref
bsg_scene_group_ensure_child(bsg_scene_ref parent, struct bsg_view *v,
			     const char *name, void *dp_hint);


/**
 * Walk from node @p n to the draw root and bump the structural revision
 * counter stored in the root's bsg_draw_ctx.
 *
 * Call on every add or remove of a group or shape node in the draw tree.
 * Does nothing if @p n is NULL or if the root has no bsg_draw_ctx.
 *
 * Replaces the file-private @c _sg_bump_rev_node() helper formerly in
 * libged's draw-scene implementation.
 */
BSG_EXPORT extern void
bsg_scene_bump_rev(bsg_scene_ref ref);


/**
 * Recursively free all descendant nodes of @p g (shapes and nested
 * sub-groups) without freeing @p g itself.
 *
 * @p fso must be the private recycle-pool handle for this draw tree.  Obtain
 * it from the bsg_draw_ctx attached to the root (field bsg_draw_ctx::fso), or
 * let libbsg fall back to the individual node's own recycle handle.
 *
 * Each freed shape node has its release callback fired before recycling.
 * Group nodes are freed recursively.
 *
 * This function does NOT bump the draw-tree revision counter; callers
 * must call bsg_bump_rev_node() at the appropriate ancestor.
 *
 * Replaces the file-private @c _sg_free_children_recursive() helper formerly
 * in libged's draw-scene implementation.
 */
BSG_EXPORT extern void
bsg_scene_free_group_contents(bsg_scene_ref ref);


/**
 * Free the entire subtree rooted at @p g, including @p g itself.
 *
 * Removes @p g from its parent's children table, bumps the draw-tree
 * revision counter, then frees all descendants followed by @p g itself.
 *
 * It is safe to call this function on a group that has already been
 * unlinked from its parent (parent == NULL), but in that case the
 * revision bump is a no-op.
 *
 * Replaces the file-private @c _sg_free_group() helper formerly in libged's
 * draw-scene implementation.
 */
BSG_EXPORT extern void
bsg_scene_free_group(bsg_scene_ref ref);


/**
 * Navigate the draw sub-tree below @p parent and erase the matching
 * sub-group or leaf shape(s) at the deepest path component.
 *
 * @p comp_names  Array of @p comp_count path-component name strings,
 *                indexing from 0.  These correspond to successive BSG
 *                group names below @p parent.
 * @p comp_count  Total number of components in @p comp_names.
 * @p depth_start Index into @p comp_names where navigation should begin.
 *                Pass 0 to start from the first component; pass n to skip
 *                the first n components (use when @p parent already
 *                represents those n components).
 * @p match_fn    Path-match predicate called in case (b) — the leaf
 *                primitive case — to decide whether a BSG_NODE_SHAPE child
 *                should be erased.  The first argument is the shape's
 *                user-data pointer; the second is @p match_ctx.
 *                If NULL, every BSG_NODE_SHAPE child at the leaf level is
 *                erased.
 * @p match_ctx   Opaque context forwarded verbatim to @p match_fn.
 *
 * Two cases at the final component:
 *   a) A BSG_NODE_GROUP child with that name exists — its entire sub-tree
 *      is freed and the group node is removed from its parent.
 *   b) No matching group — the component is treated as a leaf primitive;
 *      all BSG_NODE_SHAPE children that satisfy @p match_fn are freed.
 *
 * The structural revision counter is bumped for each erasure.
 *
 * Extracted from the file-private @c _sg_erase_nested_subpath() helper in
 * src/libged/bsg_ged_draw.c.
 */
BSG_EXPORT extern void
bsg_scene_erase_nested_subpath(bsg_scene_ref parent,
			       const char * const *comp_names,
			       size_t comp_count,
			       size_t depth_start,
			       bsg_scene_path_match_fn match_fn,
			       void *match_ctx);


/* ------------------------------------------------------------------ */
/* Subtree bbox cache                        */
/* ------------------------------------------------------------------ */

/**
 * Mark the cached aggregate bbox for @p ref and its ancestors stale.
 *
 * Call this whenever subtree structure or bounds change.
 */
BSG_EXPORT extern void
bsg_scene_bbox_invalidate(bsg_scene_ref ref);


/**
 * Compute the aggregate axis-aligned bbox of the subtree rooted at @p ref
 * and store the result in (*@p min, *@p max).
 *
 * When @p include_overlays is 0 (the common case), overlay-only shapes are
 * skipped and implementation caches may be used.
 *
 * When @p include_overlays is non-zero the cache is bypassed and a
 * full walk is performed (overlay shapes are included).  This path is
 * not cached because the caller is the rare include-overlays case.
 *
 * Returns 1 if the subtree contributes nothing to the bbox (empty),
 * 0 otherwise.  When the subtree is empty (*@p min, *@p max) are set
 * to (+INFINITY, -INFINITY) — the caller should treat the result as
 * undefined unless the return value indicates non-empty.
 */
BSG_EXPORT extern int
bsg_scene_subtree_bbox(bsg_scene_ref ref,
		       vect_t *min, vect_t *max,
		       int include_overlays);


__END_DECLS

#endif /* BSG_DRAW_SET_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
