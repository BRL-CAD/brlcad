/*                    B S G _ P R I V A T E . H
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
/** @file libbsg/bsg_private.h
 *
 * Internal libbsg helpers shared between multiple translation units.
 * This header is NOT installed; it is for libbsg source use only.
 */

#ifndef LIBBSG_BSG_PRIVATE_H
#define LIBBSG_BSG_PRIVATE_H

#include "common.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/object.h"
#include "node_private.h"
#include "bsg/draw_ctx.h"

struct bsg_scene_recycle_pool;
struct bsg_view_set_scene_store;

/* Internal view-set implementation data */
struct bsg_view_set_internal {
    struct bu_ptbl views;
    struct bsg_view_set_scene_store *scene_store;
};

struct bsg_source_realization_state {
    int current;
    int view_dependent;
    int csg_obj;
    int mesh_obj;
    fastf_t view_scale;
    size_t bot_threshold;
    fastf_t curve_scale;
    fastf_t point_scale;
};

struct bsg_node_appearance_state {
    struct bsg_appearance_settings settings;
    int force_draw;
    int inherit_settings;
    int uses_default_color;
    int work_flag;
    int user_color;
    int default_color;
    unsigned char basecolor[3];
    int eval_flag;
    int region_id;
    int valid;
};

struct bsg_node_draw_extent_state {
    vect_t center;
    fastf_t size;
    int valid;
};

struct bsg_node_bbox_cache_state {
    point_t min;
    point_t max;
    int valid;
};

struct bsg_node_interaction_state {
    char highlight_state;
};

struct bsg_node_view_binding_state {
    struct bsg_view *view;
    int valid;
};

/* Internal scene-node implementation data */
struct bsg_node_internal {
    struct bsg_overlay_info *overlay;
    struct bsg_node_view_binding_state view_binding;
    struct bsg_source_realization_state source_realization;
    bsg_type_id object_type;
    struct bsg_node_appearance_state appearance;
    struct bsg_node_draw_extent_state draw_extent;
    struct bsg_node_bbox_cache_state bbox_cache;
    struct bsg_node_interaction_state interaction;
    struct bsg_node_fields *fields;
    struct bsg_payload *payload;
    struct bsg_draw_intent *draw_intent;
    struct bsg_feature_meta *feature_meta;
    struct bsg_hud_node_meta *hud_meta;
    struct bsg_hud_payload *hud_payload;
    struct bsg_lod_payload *lod_payload;
    int changed;
    uint64_t revision;
    uint64_t drawn_revision;
    uint32_t material_revision;
    unsigned int source_flags;
    int source_flags_valid;
    unsigned int geometry_roles;
    int geometry_roles_valid;
    unsigned long long payload_flags;
    int payload_flags_valid;
    int non_database_source;
    int non_database_source_valid;
    int arrows_enabled;
    struct bsg_draw_ctx *draw_ctx;
    void (*release_callback)(struct bsg_node *);
};

extern void
bsg_node_set_draw_ctx(struct bsg_node *node, struct bsg_draw_ctx *ctx);

extern struct bsg_draw_ctx *
bsg_node_draw_ctx(const struct bsg_node *node);

static inline struct bsg_node_internal *
bsg_node_internal_ensure(struct bsg_node *n)
{
    if (!n)
	return NULL;
    if (!n->i) {
	BU_ALLOC(n->i, struct bsg_node_internal);
    }
    return n->i;
}

/*
 * Walk node @p n up to the draw root and return the bsg_draw_ctx stored
 * on that root.  Returns NULL if the root has no context.
 */
static inline struct bsg_draw_ctx *
_ctx_of_node(struct bsg_node *n)
{
    if (!n)
	return NULL;
    while (n->parent)
	n = (struct bsg_node *)n->parent;
    if (!bsg_type_is_a((n->i ? n->i->object_type : BSG_TYPE_INVALID), bsg_group_type()))
	return NULL;
    return bsg_node_draw_ctx(n);
}

static inline struct bsg_node *
bsg_view_scene_root(const struct bsg_view *v)
{
    return (v && v->gv_scene) ? (struct bsg_node *)v->gv_scene : NULL;
}

static inline void
bsg_view_scene_root_set(struct bsg_view *v, struct bsg_node *root)
{
    if (v)
	v->gv_scene = root;
}

#endif /* LIBBSG_BSG_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
