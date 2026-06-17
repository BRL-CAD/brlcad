/*                         P I C K . H
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
 * Typed pick records and pick actions.
 *
 * A @c bsg_pick_record is the typed result of a scene pick operation.  It
 * feeds interaction records and selection state without exposing legacy raw
 * node-table conversion APIs.
 *
 * Pick records carry:
 *  - the matched scene node while the source scene is live;
 *  - the source-path string from the node's draw intent (when present);
 *  - the model-space hit distance (for the nearest-hit ordering);
 *  - the screen coordinates where the pick was initiated;
 *  - the view the pick was performed against.
 *
 * Pick actions build a @c bsg_pick_result (an ordered list of records) from
 * a view and screen coordinates.  The caller is responsible for freeing the
 * result with @c bsg_pick_result_free().
 *
 * This header is intentionally free of Qt and GED dependencies so that
 * libbsg can implement the pick logic and Qt/GED consumers can both use it.
 *
 * Per the pick and selection API criteria:
 *   - Tools receive typed records rather than raw node tables.
 *   - Selection state is stored in bsg_selection; highlighting is derived
 *     appearance.
 */
/** @{ */
/* @file bsg/pick.h */

#ifndef BSG_PICK_H
#define BSG_PICK_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bsg/scene_builder.h"
#include "bsg/selection.h"

__BEGIN_DECLS

/* -----------------------------------------------------------------------
 * Pick operation kind
 * ----------------------------------------------------------------------- */

/**
 * What to do with nodes matched by a pick operation.
 */
typedef enum {
    BSG_PICK_OP_SET    = 0,  /**< replace current selection with pick result */
    BSG_PICK_OP_ADD    = 1,  /**< add pick result to current selection */
    BSG_PICK_OP_REMOVE = 2   /**< remove pick result from current selection */
} bsg_pick_op;

/**
 * Flags controlling D3 pick-action queries.
 */
typedef enum {
    BSG_PICK_INCLUDE_SCENE    = 0x01, /**< include database-drawn scene groups */
    BSG_PICK_INCLUDE_OVERLAYS = 0x02, /**< include overlay groups/controls      */
    BSG_PICK_FIRST_ONLY       = 0x04  /**< return nearest hit only              */
} bsg_pick_flags;

/* -----------------------------------------------------------------------
 * bsg_pick_record — single pick hit
 * ----------------------------------------------------------------------- */

/**
 * One record in a pick result set.
 *
 * Lifecycle: allocated and owned by a @c bsg_pick_result; do not free
 * individual records directly.  Use @c bsg_pick_result_free() to release
 * the entire result.
 *
 * Fields:
 *   pr_scene        — transient matched scene ref while the source scene is
 *                     live.  Public consumers should prefer
 *                     pr_feature/pr_source_path/pr_instance_path for identity.
 *   pr_feature      — feature ref when the hit source is a feature.
 *   pr_valid        — non-zero when the semantic identity fields describe a
 *                     current hit; zero when stale/erased.
 *   pr_source_path  — source path string copied from the node's draw intent,
 *                     or empty string when no draw intent is attached.
 *   pr_hit_dist     — model-space distance from the pick ray origin to the
 *                     centre of the node's bounding box.  Used for
 *                     nearest-first ordering.  Negative means not computed.
 *   pr_primitive_id — primitive identifier when available, else -1.
 *   pr_subelement_id — subelement identifier when available, else -1.
 *   pr_instance_path — resolved instance-path string for semantic picks.
 *   pr_screen_x,
 *   pr_screen_y     — screen-pixel coordinates where the pick was initiated.
 *   pr_view         — view the pick was performed against (borrowed).
 */
struct bsg_pick_record {
    bsg_scene_ref      pr_scene;        /**< @brief matched scene ref */
    bsg_feature_ref    pr_feature;      /**< @brief feature ref, when applicable */
    int                pr_valid;        /**< @brief semantic hit is valid/current */
    struct bu_vls      pr_source_path;  /**< @brief drawn source path */
    fastf_t            pr_hit_dist;     /**< @brief bbox centre hit distance */
    int                pr_primitive_id; /**< @brief primitive id or -1 */
    int                pr_subelement_id;/**< @brief subelement id or -1 */
    struct bu_vls      pr_instance_path;/**< @brief resolved instance path */
    int                pr_screen_x;     /**< @brief screen X at pick time */
    int                pr_screen_y;     /**< @brief screen Y at pick time */
    struct bsg_view   *pr_view;         /**< @brief view (borrowed) */
};

/* -----------------------------------------------------------------------
 * bsg_pick_result — ordered list of pick records
 * ----------------------------------------------------------------------- */

/**
 * Result of a pick action: a bu_ptbl of @c bsg_pick_record* ordered by
 * ascending @c pr_hit_dist (nearest first).
 *
 * Allocate with a pick action such as @c bsg_pick_point() and release with
 * @c bsg_pick_result_free().
 */
struct bsg_pick_result {
    struct bu_ptbl  pr_records;  /**< @brief bu_ptbl of bsg_pick_record* */
};

/* -----------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

/**
 * Allocate an empty pick result.
 * Returns NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_result_create(void);

/**
 * Free @p res and all pick records it owns.
 * The referenced nodes and views are NOT freed.
 * No-op if @p res is NULL.
 */
BSG_EXPORT extern void
bsg_pick_result_free(struct bsg_pick_result *res);

/**
 * Return the number of pick records in @p res.
 * Returns 0 if @p res is NULL.
 */
BSG_EXPORT extern size_t
bsg_pick_result_count(const struct bsg_pick_result *res);

/**
 * Return the @p i-th pick record in @p res, or NULL if @p i is out of range
 * or @p res is NULL.  Records are ordered nearest-first after any pick action
 * that computes @c pr_hit_dist.
 */
BSG_EXPORT extern struct bsg_pick_record *
bsg_pick_result_get(const struct bsg_pick_result *res, size_t i);

/* -----------------------------------------------------------------------
 * Pick actions
 * ----------------------------------------------------------------------- */

/**
 * Point pick: find all scene nodes whose bounding boxes are intersected by
 * the viewing ray through screen pixel (@p x, @p y) in view @p v.
 *
 * The result is sorted nearest-first by @c pr_hit_dist.
 *
 * When @p first_only is non-zero only the nearest (smallest @c pr_hit_dist)
 * record is included in the result.
 *
 * Returns a newly allocated @c bsg_pick_result on success, or NULL on
 * allocation failure.  The caller must call @c bsg_pick_result_free().
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_point(struct bsg_view *v, int x, int y, int first_only);

/**
 * Rectangle pick: find all scene nodes whose bounding boxes overlap the
 * screen rectangle [@p x0,@p y0]–[@p x1,@p y1] in view @p v.
 *
 * Records in the result are sorted nearest-first by @c pr_hit_dist (bbox
 * centre distance from view origin).
 *
 * Returns a newly allocated @c bsg_pick_result, or NULL on failure.
 * The caller must call @c bsg_pick_result_free().
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_rect(struct bsg_view *v,
	      int x0, int y0, int x1, int y1);

/**
 * Nearest-only pick: call @c bsg_pick_point() and retain only the record
 * with the smallest @c pr_hit_dist.
 *
 * Returns a newly allocated @c bsg_pick_result with at most one record, or
 * NULL on failure.  The caller must call @c bsg_pick_result_free().
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_nearest(struct bsg_view *v, int x, int y);

/**
 * D3 ray pick: intersect a model-space ray against visible scene and overlay
 * groups and return sorted typed pick records.
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_ray(struct bsg_view *v, const point_t orig, const vect_t dir,
	     bsg_pick_flags flags);

/**
 * D3 overlay-control pick: return nearest overlay handle/manipulator hit
 * near screen location (@p x,@p y).  @p role_mask is reserved for overlay
 * role filtering in later D4 policy work.
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_nearest_overlay_control(struct bsg_view *v, int x, int y,
				 unsigned long long role_mask);

/**
 * D3 semantic-path pick: match draw-intent source paths using glob pattern
 * semantics and return typed pick records for matching groups.
 */
BSG_EXPORT extern struct bsg_pick_result *
bsg_pick_semantic_path(struct bsg_view *v, const char *path_pattern);

/**
 * Apply @p res to @p sel according to @p op:
 *   BSG_PICK_OP_SET    — clear @p sel then add all nodes from @p res.
 *   BSG_PICK_OP_ADD    — add all nodes from @p res to @p sel.
 *   BSG_PICK_OP_REMOVE — remove all nodes in @p res from @p sel.
 *
 * No-op if @p sel or @p res is NULL.
 */
BSG_EXPORT extern void
bsg_pick_apply(struct bsg_selection    *sel,
	       struct bsg_pick_result  *res,
	       bsg_pick_op              op);

__END_DECLS

#endif /* BSG_PICK_H */

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
