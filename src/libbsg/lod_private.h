/*                    L O D _ P R I V A T E . H
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

#ifndef BSG_LOD_PRIVATE_H
#define BSG_LOD_PRIVATE_H

#include "common.h"
#include "node_fwd_private.h"
#include "vmath.h"
#include "bsg/lod.h"
#include "bsg/defines.h"

__BEGIN_DECLS

enum bsg_lod_source_kind {
    BSG_LOD_SOURCE_UNKNOWN = 0,
    BSG_LOD_SOURCE_MESH,
    BSG_LOD_SOURCE_CSG_WIREFRAME,
    BSG_LOD_SOURCE_TEST
};

struct bsg_lod_view_state {
    struct bsg_view *view;
    int level;
    fastf_t view_scale;
    fastf_t curve_scale;
    fastf_t point_scale;
    int perspective_flag;
    uint64_t frame_revision;
    int policy_tag;
};

#define BSG_LOD_VIEW_STATE_INIT { NULL, -1, 0.0, 0.0, 0.0, 0, 0, 0 }

struct bsg_lod_source_record {
    bsg_lod_source_ref ref;
    enum bsg_lod_source_kind kind;
    uint64_t identity;
    const char *name;
    int level_count;
};

typedef int (*bsg_lod_source_select_cb)(const struct bsg_lod_source_record *source,
					struct bsg_view *view,
					void *policy_data);

typedef void (*bsg_lod_source_activate_cb)(const struct bsg_lod_source_record *source,
					   struct bsg_view *view,
					   int level,
					   struct bsg_lod_view_state *state,
					   void *policy_data);

typedef int (*bsg_lod_source_stale_cb)(const struct bsg_lod_source_record *source,
				       struct bsg_view *view,
				       const struct bsg_lod_view_state *state,
				       void *policy_data);

typedef void (*bsg_lod_source_free_cb)(void *policy_data);

struct bsg_lod_source_policy {
    enum bsg_lod_source_kind kind;
    uint64_t identity;
    const char *name;
    void *policy_data;
    bsg_lod_source_select_cb select;
    bsg_lod_source_activate_cb activate;
    bsg_lod_source_stale_cb stale;
    bsg_lod_source_free_cb free;
};

#define BSG_LOD_SOURCE_POLICY_INIT { BSG_LOD_SOURCE_UNKNOWN, 0, NULL, NULL, NULL, NULL, NULL, NULL }

struct bsg_lod_view_cursor {
    struct bsg_view *v;
    int           level;
    fastf_t       view_scale;
    fastf_t       curve_scale;
    fastf_t       point_scale;
    int           perspective_flag;
    uint64_t      last_frame_rev;
    int           policy_tag;
};

struct bsg_lod_ops {
    int  (*select_level)(bsg_node *node, struct bsg_view *v);
    void (*activate_level)(bsg_node *node, struct bsg_view *v, int level);
    int  (*is_stale)(bsg_node *node, struct bsg_view *v);
    void (*free)(bsg_node *node);
};

struct bsg_lod_payload {
    struct bsg_lod_ops *ops;
    void               *user_data;
    enum bsg_lod_source_kind source_kind;
    uint64_t            source_identity;
    char               *source_name;

    struct bsg_lod_view_cursor *cursors;
    size_t cursor_alloc;
    size_t cursor_count;
};

struct bsg_lod_payload *bsg_lod_node_payload(bsg_node *node);
void bsg_lod_node_set_payload(bsg_node *node, struct bsg_lod_payload *payload);

bsg_node *bsg_lod_node_create(struct bsg_view *v);
void bsg_lod_node_set_ops(bsg_node *node, struct bsg_lod_ops *ops, void *user_data);
struct bsg_lod_ops *bsg_lod_node_ops(bsg_node *node);
void *bsg_lod_node_user_data(bsg_node *node);
void bsg_lod_node_attach_level(bsg_node *lod_node, bsg_node *level_node);
struct bsg_lod_view_cursor *bsg_lod_node_get_cursor(bsg_node *node, struct bsg_view *v);
int bsg_lod_node_active_level(bsg_node *node, struct bsg_view *v);
int bsg_lod_node_resolve_level(bsg_node *node, struct bsg_view *v);
int bsg_lod_node_level_count(bsg_node *node);
bsg_node *bsg_lod_node_insert_above(bsg_node *leaf, struct bsg_view *v);

BSG_EXPORT extern bsg_lod_source_ref
bsg_lod_source_insert_above_scene_ref(bsg_scene_ref leaf,
				      struct bsg_view *view,
				      const struct bsg_lod_source_policy *policy);

BSG_EXPORT extern int
bsg_lod_source_view_state_ensure(bsg_lod_source_ref ref,
				 struct bsg_view *view,
				 struct bsg_lod_view_state *state_out);

BSG_EXPORT extern int
bsg_lod_source_view_state_invalidate(bsg_lod_source_ref ref,
				     struct bsg_view *view);

BSG_EXPORT extern int
bsg_lod_source_view_policy_tag_sync(bsg_lod_source_ref ref,
				    struct bsg_view *view,
				    int policy_tag);

BSG_EXPORT extern int
bsg_lod_source_record_get(bsg_lod_source_ref ref,
			  struct bsg_lod_source_record *record);

BSG_EXPORT extern void
bsg_lod_sources_update_scene_ref(bsg_scene_ref root, struct bsg_view *view);

BSG_EXPORT extern int
bsg_lod_source_stale(bsg_lod_source_ref ref, struct bsg_view *view);

BSG_EXPORT extern int
bsg_mesh_lod_load_view(struct bsg_mesh_lod *l,
		       struct bsg_node *visibility_node,
		       struct bsg_view *v,
		       int reset);

__END_DECLS

#endif /* BSG_LOD_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
