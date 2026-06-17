/*                 D R A W _ S E T _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/draw_set_private.h
 *
 * Private raw-node draw-tree helpers.  Public callers use scene refs.
 */

#ifndef LIBBSG_DRAW_SET_PRIVATE_H
#define LIBBSG_DRAW_SET_PRIVATE_H

#include "common.h"
#include "bsg/draw_set.h"
#include "node_private.h"

__BEGIN_DECLS

typedef int (*bsg_path_match_fn)(struct bsg_node *shape_node, void *match_ctx);

BSG_EXPORT extern int bsg_draw_tree_depth(const bsg_node *g);
BSG_EXPORT extern bsg_node *bsg_group_find_child(bsg_node *parent, const char *name);
BSG_EXPORT extern bsg_node *bsg_group_ensure_child(bsg_node *parent, struct bsg_view *v,
						   const char *name, void *dp_hint);
BSG_EXPORT extern void bsg_bump_rev_node(bsg_node *n);
struct bsg_scene_recycle_pool;

BSG_EXPORT extern void bsg_free_children_recursive(bsg_node *g,
	struct bsg_scene_recycle_pool *fso);
BSG_EXPORT extern void bsg_free_group_contents(bsg_node *g);
BSG_EXPORT extern void bsg_free_group(bsg_node *g);
BSG_EXPORT extern void bsg_erase_nested_subpath(bsg_node *parent,
						const char * const *comp_names,
						size_t comp_count,
						size_t depth_start,
						bsg_path_match_fn match_fn,
						void *match_ctx);
BSG_EXPORT extern void bsg_node_bbox_invalidate(bsg_node *n);
BSG_EXPORT extern void bsg_node_bbox_cache_clear(bsg_node *n);
BSG_EXPORT extern void bsg_node_bbox_cache_set(bsg_node *n, const point_t min, const point_t max);
BSG_EXPORT extern int bsg_node_bbox_cache_get(const bsg_node *n, point_t min, point_t max);
BSG_EXPORT extern int bsg_subtree_bbox(bsg_node *n, vect_t *min, vect_t *max,
				       int include_overlays);

__END_DECLS

#endif /* LIBBSG_DRAW_SET_PRIVATE_H */
