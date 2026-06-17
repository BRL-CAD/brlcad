/*                          L O D . H
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
 * Level-of-detail helpers for the BSG scene graph.
 *
 * Canonical home; bv/lod.h is a backward-compatibility bridge.
 */
/** @{ */
/* @file bsg/lod.h */

#ifndef BSG_LOD_H
#define BSG_LOD_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bsg/defines.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

/* =========================================================================
 * Public typed LoD node fields
 * ========================================================================= */

typedef enum bsg_lod_selection {
    BSG_LOD_SELECT_AUTO = 0,
    BSG_LOD_SELECT_FORCE_LEVEL = 1
} bsg_lod_selection;

BSG_EXPORT extern bsg_field_ref bsg_lod_ref_selection_field(bsg_lod_ref lod);
BSG_EXPORT extern bsg_field_ref bsg_lod_ref_active_level_field(bsg_lod_ref lod);
BSG_EXPORT extern bsg_field_ref bsg_lod_ref_ranges_field(bsg_lod_ref lod);

BSG_EXPORT extern int bsg_lod_ref_set_selection(bsg_lod_ref lod, int selection);
BSG_EXPORT extern int bsg_lod_ref_selection(bsg_lod_ref lod);
BSG_EXPORT extern int bsg_lod_ref_set_active_level(bsg_lod_ref lod, int level);
BSG_EXPORT extern int bsg_lod_ref_active_level(bsg_lod_ref lod);
BSG_EXPORT extern size_t bsg_lod_ref_range_count(bsg_lod_ref lod);
BSG_EXPORT extern int bsg_lod_ref_range_at(bsg_lod_ref lod, size_t idx, double *range);
BSG_EXPORT extern int bsg_lod_ref_append_range(bsg_lod_ref lod, double range);
BSG_EXPORT extern int bsg_lod_ref_clear_ranges(bsg_lod_ref lod);

BSG_EXPORT extern size_t bsg_lod_ref_level_count(bsg_lod_ref lod);
BSG_EXPORT extern bsg_node_ref bsg_lod_ref_level_at(bsg_lod_ref lod, size_t idx);
BSG_EXPORT extern void bsg_lod_ref_append_level(bsg_lod_ref lod, bsg_node_ref level);
BSG_EXPORT extern void bsg_lod_ref_remove_level(bsg_lod_ref lod, bsg_node_ref level);

/* =========================================================================
 * View bounds and object selection
 * ========================================================================= */

/* Given a view, construct either an oriented bounding box or a view frustum
 * extruded to contain scene objects visible in the view.  Conceptually, think
 * of it as a framebuffer pane pushed through the scene in the direction the
 * camera is looking.  If the view width and height are not set or there is
 * some other problem, no volume is computed. This function is intended
 * primarily to be set as an updating callback for the bsg_view structure. */
BSG_EXPORT extern void
bsg_view_bounds(struct bsg_view *v);

/* Given a screen x,y coordinate, construct the set of shape nodes whose AABB
 * intersects the OBB created by projecting that pixel through the scene. */
BSG_EXPORT int
bsg_view_pick_candidates(struct bu_ptbl *sset, struct bsg_view *v, int x, int y);

/* Given a screen x1,y1,x2,y2 rectangle, construct and return the set of shape
 * nodes whose AABB intersects the OBB created by projecting that rectangle
 * through the scene. */
BSG_EXPORT int
bsg_view_rect_pick_candidates(struct bu_ptbl *sset, struct bsg_view *v, int x1, int y1, int x2, int y2);

/* =========================================================================
 * Mesh LoD context
 * ========================================================================= */

/* Storing and reading from a lot of small, individual files doesn't work very
 * well on some platforms.  We provide a "context" to manage bookkeeping of data
 * across objects. The details are implementation internal - the application
 * will simply create a context with a file name and provide it to the various
 * LoD calls. */
struct bsg_mesh_lod_context_internal;
struct bsg_mesh_lod_context {
    struct bsg_mesh_lod_context_internal *i;
};

/* Create an LoD context using "name". If data is already present associated
 * with that name it will be loaded, otherwise a new storage structure will be
 * initialized.  If creation or loading fails for any reason the return value
 * NULL. */
BSG_EXPORT struct bsg_mesh_lod_context *
bsg_mesh_lod_context_create(const char *name);

/* Free all memory associated with context c.  Note that this call does NOT
 * remove the on-disk data. */
BSG_EXPORT void
bsg_mesh_lod_context_destroy(struct bsg_mesh_lod_context *c);

/* Remove cache data associated with key.  If key == 0, remove ALL cache data
 * associated with all LoD objects in c.  If key == 0 AND c == NULL, clear all
 * LoD cache data for all .g databases associated with the current user's cache. */
BSG_EXPORT void
bsg_mesh_lod_clear_cache(struct bsg_mesh_lod_context *c, unsigned long long key);

/**
 * Given a set of points and faces, calculate a lookup key and determine if the
 * cache has the LoD data for this particular mesh.  If it does not, do the
 * initial calculations to generate the cached LoD data needed for subsequent
 * lookups, otherwise just return the calculated key.
 *
 * @return the lookup key calculated from the data */
BSG_EXPORT unsigned long long
bsg_mesh_lod_cache(struct bsg_mesh_lod_context *c, const point_t *v, size_t vcnt, const vect_t *vn, int *f, size_t fcnt, unsigned long long user_key, fastf_t fratio);

/**
 * Given a name, see if the context has a key associated with that name.
 * If so return it, else return zero. */
BSG_EXPORT unsigned long long
bsg_mesh_lod_key_get(struct bsg_mesh_lod_context *c, const char *name);

/**
 * Given a name and a key, instruct the context to associate that name with
 * the key.  Returns 0 if successful, else error. */
BSG_EXPORT int
bsg_mesh_lod_key_put(struct bsg_mesh_lod_context *c, const char *name, unsigned long long key);

/**
 * Set up the bsg_mesh_lod container using cached LoD information associated
 * with key.  If no cached data has been prepared, a NULL container is returned. */
BSG_EXPORT struct bsg_mesh_lod *
bsg_mesh_lod_create(struct bsg_mesh_lod_context *c, unsigned long long key);

/* Clean up the lod container. */
BSG_EXPORT void
bsg_mesh_lod_destroy(struct bsg_mesh_lod *l);

/**
 * Load a mesh LoD container's active arrays directly, without requiring the
 * container to be attached to a scene node payload.
 *
 * bsg_mesh_lod_load_level() selects a specific level.  Passing level == -1
 * returns the current level without changing active arrays.
 *
 * bsg_mesh_lod_load_view_scene_ref() selects a level from the view scale and
 * optional policy on @p v.  If @p visibility_ref is supplied, it is used only
 * for the visibility test and logging context; the LoD container remains
 * caller-owned.
 */
BSG_EXPORT int
bsg_mesh_lod_load_level(struct bsg_mesh_lod *l, int level, int reset);

BSG_EXPORT int
bsg_mesh_lod_load_view_scene_ref(struct bsg_mesh_lod *l,
				 bsg_scene_ref visibility_ref,
				 struct bsg_view *v,
				 int reset);

BSG_EXPORT void
bsg_mesh_lod_memshrink_scene_ref(bsg_scene_ref ref);

BSG_EXPORT void
bsg_mesh_lod_memshrink_node_ref(bsg_node_ref ref);

/**
 * Given a scene object with mesh LoD data attached as a typed mesh payload
 * and a bsg_view, load the appropriate level of detail for displaying the
 * mesh in that view.
 * Returns the level selected.  If v == NULL, return current level of l.
 * If there is an error or l == NULL, return -1. */
BSG_EXPORT int
bsg_mesh_lod_view_scene_ref(bsg_scene_ref ref, struct bsg_view *v, int reset);

BSG_EXPORT int
bsg_mesh_lod_view_node_ref(bsg_node_ref ref, struct bsg_view *v, int reset);

/**
 * Given a scene object with mesh LoD data attached as a typed mesh payload
 * and a detail level, load the appropriate data.
 * Returns the level selected.  If level == -1, return current level of l.
 * If there is an error, return -1. */
BSG_EXPORT int
bsg_mesh_lod_level_scene_ref(bsg_scene_ref ref, int level, int reset);

BSG_EXPORT int
bsg_mesh_lod_level_node_ref(bsg_node_ref ref, int level, int reset);

/* Detach and free a scene object's typed mesh LoD payload. */
BSG_EXPORT void
bsg_mesh_lod_free_scene_ref(bsg_scene_ref ref);

BSG_EXPORT void
bsg_mesh_lod_free_node_ref(bsg_node_ref ref);

/* Set function callbacks for retrieving and freeing high levels of mesh detail */
BSG_EXPORT void
bsg_mesh_lod_detail_setup_clbk(struct bsg_mesh_lod *lod, int (*clbk)(struct bsg_mesh_lod *, void *), void *cb_data);
BSG_EXPORT void
bsg_mesh_lod_detail_clear_clbk(struct bsg_mesh_lod *lod, int (*clbk)(struct bsg_mesh_lod *, void *));
BSG_EXPORT void
bsg_mesh_lod_detail_free_clbk(struct bsg_mesh_lod *lod, int (*clbk)(struct bsg_mesh_lod *, void *));

/* =========================================================================
 * BSG-native LoD source policy
 * ========================================================================= */

typedef struct bsg_lod_source_ref {
    uintptr_t token;
    uint64_t revision;
} bsg_lod_source_ref;

#define BSG_LOD_SOURCE_REF_NULL_INIT {0, 0}

BSG_EXPORT extern int
bsg_lod_source_ref_is_null(bsg_lod_source_ref ref);

__END_DECLS

#endif /* BSG_LOD_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
