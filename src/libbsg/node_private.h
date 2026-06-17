/*                  N O D E _ P R I V A T E . H
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
/** @file node_private.h
 *
 * Internal bsg_node layout for libbsg implementation code.
 * New code should prefer bsg/node.h accessors when possible.
 */

#ifndef BSG_NODE_PRIVATE_H
#define BSG_NODE_PRIVATE_H

#include "bsg/defines.h"
#include "bsg/lod.h"
#include "bsg/scene_builder.h"
#include "bsg/object.h"
#include "node_fwd_private.h"
#include "scene_object_private.h"

/** Legacy semantic bit masks retained for private traversal compatibility. */
#define BSG_NODE_ROOT         0x10000000ULL
#define BSG_NODE_GROUP        0x20000000ULL
#define BSG_NODE_SHAPE        0x40000000ULL
#define BSG_NODE_LOD          0x80000000ULL
#define BSG_NODE_VLIST       0x100000000ULL
#define BSG_NODE_TRANSFORM   0x200000000ULL
#define BSG_NODE_SENSOR      0x400000000ULL
#define BSG_NODE_VIEW_SCOPE  0x800000000ULL
#define BSG_NODE_VIEW_REF    0x8000000000ULL

/** Private payload role masks retained for coarse metadata queries. */
#define BSG_PAYLOAD_VLIST   0x10000000000ULL
#define BSG_PAYLOAD_CSG     0x20000000000ULL
#define BSG_PAYLOAD_MESH    0x40000000000ULL
#define BSG_PAYLOAD_BREP    0x80000000000ULL
#define BSG_PAYLOAD_OVERLAY 0x100000000000ULL
#define BSG_PAYLOAD_MASK    0x1F0000000000ULL

struct bsg_hud_node_meta;
struct bsg_hud_payload;
struct bsg_lod_payload;
struct bsg_draw_ctx;
struct bsg_node_fields;
struct bsg_payload;
struct bsg_view;
struct bsg_lod_source_policy;

struct bsg_feature_meta {
    int family;
    char *display_name;
    char *owner;
};

struct bsg_node  {
    struct bsg_node_internal *i;

    struct bu_ptbl children;
    struct bsg_node *parent;
};

__BEGIN_DECLS

BSG_EXPORT extern bsg_node *bsg_node_create(struct bsg_view *v, unsigned long long kind);
BSG_EXPORT extern void bsg_node_destroy(bsg_node *node);
BSG_EXPORT extern void bsg_node_set_name(bsg_node *node, const char *name);
BSG_EXPORT extern const char *bsg_node_name(const bsg_node *node);
BSG_EXPORT extern bsg_node *bsg_node_parent(const bsg_node *node);
BSG_EXPORT extern size_t bsg_node_child_count(const bsg_node *node);
BSG_EXPORT extern bsg_node *bsg_node_child_at(const bsg_node *node, size_t idx);
BSG_EXPORT extern void bsg_node_add_child(bsg_node *parent, bsg_node *child);
BSG_EXPORT extern void bsg_node_remove_child(bsg_node *parent, bsg_node *child);
BSG_EXPORT extern void bsg_node_set_visible_state(bsg_node *node, int on);
BSG_EXPORT extern int bsg_node_visible(const bsg_node *node);
BSG_EXPORT extern void bsg_node_set_transform(bsg_node *node, const mat_t mat);
BSG_EXPORT extern void bsg_node_transform(const bsg_node *node, mat_t mat);
BSG_EXPORT extern void bsg_node_set_bounds(bsg_node *node, const point_t bmin, const point_t bmax, int valid);
BSG_EXPORT extern int bsg_node_bounds(const bsg_node *node, point_t bmin, point_t bmax);
BSG_EXPORT extern void bsg_switch_node_set_which_child(bsg_node *node, int which_child);
BSG_EXPORT extern int bsg_switch_node_which_child(const bsg_node *node);
BSG_EXPORT extern int bsg_switch_node_child_selected(const bsg_node *node, size_t idx);

BSG_EXPORT extern bsg_node *bsg_view_scene_root_ensure(struct bsg_view *v);
BSG_EXPORT extern void bsg_view_scene_root_sync(bsg_node *root, struct bsg_view *v);
BSG_EXPORT extern void bsg_view_scene_root_detach_from_root(bsg_node *root);

BSG_EXPORT extern void bsg_node_set_payload(bsg_node *node, struct bsg_payload *pl);
BSG_EXPORT extern struct bsg_payload *bsg_node_get_payload(const bsg_node *node);

BSG_EXPORT extern void bsg_mesh_lod_memshrink(struct bsg_node *s);
BSG_EXPORT extern int bsg_mesh_lod_view(struct bsg_node *s, struct bsg_view *v, int reset);
BSG_EXPORT extern int bsg_mesh_lod_level(struct bsg_node *s, int level, int reset);
BSG_EXPORT extern void bsg_mesh_lod_free(struct bsg_node *s);

BSG_EXPORT extern bsg_lod_source_ref
bsg_lod_source_insert_above(bsg_node *leaf,
			    struct bsg_view *view,
			    const struct bsg_lod_source_policy *policy);

BSG_EXPORT extern bsg_lod_source_ref bsg_lod_source_from_node(bsg_node *node);
BSG_EXPORT extern void bsg_lod_sources_update(bsg_node *root, struct bsg_view *view);

BSG_EXPORT extern bsg_node *bsg_group_create(struct bsg_view *v);
BSG_EXPORT extern void bsg_group_add_child(bsg_node *group, bsg_node *child);
BSG_EXPORT extern void bsg_group_remove_child(bsg_node *group, bsg_node *child);
BSG_EXPORT extern void bsg_group_destroy(bsg_node *group);

BSG_EXPORT extern bsg_node *bsg_transform_create(struct bsg_view *v);
BSG_EXPORT extern void bsg_transform_set_matrix(bsg_node *transform, const mat_t mat);
BSG_EXPORT extern void bsg_transform_get_matrix(const bsg_node *transform, mat_t mat);
BSG_EXPORT extern void bsg_transform_destroy(bsg_node *transform);

BSG_EXPORT extern bsg_node *bsg_shape_create(struct bsg_view *v);
BSG_EXPORT extern void bsg_shape_mark_db_object(bsg_node *shape);
BSG_EXPORT extern int bsg_shape_is_non_database_source(const bsg_node *shape);
BSG_EXPORT extern void bsg_shape_set_non_database_source(bsg_node *shape, int non_database_source);
BSG_EXPORT extern int bsg_shape_is_mesh(const bsg_node *shape);
BSG_EXPORT extern int bsg_shape_arrows_enabled(const bsg_node *shape);
BSG_EXPORT extern void bsg_shape_set_arrows_enabled(bsg_node *shape, int enabled);
BSG_EXPORT extern fastf_t bsg_shape_arrow_tip_length(const bsg_node *shape);
BSG_EXPORT extern void bsg_shape_set_arrow_tip_length(bsg_node *shape, fastf_t length);
BSG_EXPORT extern fastf_t bsg_shape_arrow_tip_width(const bsg_node *shape);
BSG_EXPORT extern void bsg_shape_set_arrow_tip_width(bsg_node *shape, fastf_t width);
BSG_EXPORT extern void bsg_shape_destroy(bsg_node *shape);

__END_DECLS

#endif /* BSG_NODE_PRIVATE_H */
