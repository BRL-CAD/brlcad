/*                B S G _ G E D _ D R A W . H
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
/** @addtogroup ged_view
 *
 * @brief
 * BSG-backed GED draw-scene API.
 *
 * Modern callers should treat the draw set as semantic records:
 *   - ged_draw_shape_ref / ged_draw_group_ref are stable handles for the
 *     current draw-scene revision.
 *   - ged_draw_shape_record / ged_draw_group_record expose path identity,
 *     draw source/style, visibility, and highlight state without requiring
 *     callers to inspect BSG node layout.
 *
 * The BSG tree remains the private implementation used by the GED/BSG bridge.
 * Installed GED callers should not need BSG node pointers to query or update
 * the draw scene.
 *
 * Drawn database objects are keyed by struct db_full_path.  String path
 * formatting is available for logging/UI, but graph mutation APIs use the
 * structured path form.
 */
/** @{ */
/* @file ged/bsg_ged_draw.h */

#ifndef GED_BSG_GED_DRAW_H
#define GED_BSG_GED_DRAW_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/draw_intent.h"
#include "bsg/interaction.h"
#include "ged/defines.h"

/* Forward declaration to keep the include surface narrow.  Callers that
 * actually use the db_full_path variants will already be including
 * "raytrace.h" or "rt/db_fullpath.h" anyway. */
struct db_full_path;
struct db_i;
struct bn_tol;
struct bg_tess_tol;
struct bsg_view;
struct bsg_mesh_lod_context;

typedef enum ged_draw_stale_reason {
    GED_DRAW_STALE_NONE = 0,
    GED_DRAW_STALE_SOURCE_CHANGED,
    GED_DRAW_STALE_VIEW_INPUT_CHANGED,
    GED_DRAW_STALE_SETTINGS_CHANGED,
    GED_DRAW_STALE_FORCED,
    GED_DRAW_STALE_UPDATE_FAILED
} ged_draw_stale_reason;

typedef enum ged_draw_transaction_kind {
    GED_DRAW_TXN_NONE = 0,
    GED_DRAW_TXN_DRAW,
    GED_DRAW_TXN_ERASE,
    GED_DRAW_TXN_ERASE_PREFIX,
    GED_DRAW_TXN_REDRAW,
    GED_DRAW_TXN_VISIBILITY,
    GED_DRAW_TXN_HIGHLIGHT,
    GED_DRAW_TXN_MATERIAL_CHANGED,
    GED_DRAW_TXN_REFRESH_MATERIAL_COLORS,
    GED_DRAW_TXN_TRANSPARENCY,
    GED_DRAW_TXN_DEFAULT_DRAW_MODE,
    GED_DRAW_TXN_STALE_SOURCE,
    GED_DRAW_TXN_SOURCE_UPDATED,
    GED_DRAW_TXN_SOURCE_RENAMED,
    GED_DRAW_TXN_SOURCE_REFERENCES_REMOVED,
    GED_DRAW_TXN_CLEAR,
    GED_DRAW_TXN_CLEAR_SCOPE,
    GED_DRAW_TXN_TEARDOWN
} ged_draw_transaction_kind;

struct ged_draw_transaction {
    ged_draw_transaction_kind kind;
    const char *path;                 /**< borrowed command-facing path/name */
    const char *new_path;             /**< optional borrowed replacement path/name */
    const char **paths;               /**< optional borrowed draw path array */
    int path_count;                   /**< entries in @p paths */
    const struct db_full_path *dfp;   /**< optional structured path target */
    struct bsg_view *view;            /**< optional view scope for view-aware transactions */
    int mode;                         /**< optional draw-mode filter; <0 means all modes */
    const struct bsg_appearance_settings *appearance; /**< optional borrowed draw settings */
    int autoview;                     /**< draw transaction may update view bounds */
    double value;                     /**< visibility/highlight/transparency/mode */
    ged_draw_stale_reason stale_reason;
    int removed;
    int redraw;
};

struct ged_draw_transaction_result {
    int status;                       /**< >=0 success/count, <0 error */
    int affected_groups;
    int affected_shapes;
    int stale_count;
    int redrawn_count;
    int error_count;
    uint64_t scene_revision_before;
    uint64_t scene_revision_after;
    struct bu_vls names;              /**< affected user-facing target names */
    struct bu_vls errors;             /**< diagnostic details */
};

typedef uintptr_t ged_draw_observer_token;
typedef void (*ged_draw_transaction_observer_func_t)(
	struct ged *gedp,
	const struct ged_draw_transaction *txn,
	const struct ged_draw_transaction_result *result,
	void *client_data);

__BEGIN_DECLS

typedef struct ged_draw_shape_ref {
    uintptr_t token;
    uint64_t scene_revision;
} ged_draw_shape_ref;

typedef struct ged_draw_group_ref {
    uintptr_t token;
    uint64_t scene_revision;
} ged_draw_group_ref;

#define GED_DRAW_SHAPE_REF_NULL_INIT {0, 0}
#define GED_DRAW_GROUP_REF_NULL_INIT {0, 0}

#ifdef __cplusplus
#  define GED_DRAW_SHAPE_REF_NULL ged_draw_shape_ref{0, 0}
#  define GED_DRAW_GROUP_REF_NULL ged_draw_group_ref{0, 0}
#else
#  define GED_DRAW_SHAPE_REF_NULL ((ged_draw_shape_ref){0, 0})
#  define GED_DRAW_GROUP_REF_NULL ((ged_draw_group_ref){0, 0})
#endif

struct ged_draw_shape_record {
    ged_draw_shape_ref ref;
    ged_draw_group_ref group;
    const struct db_full_path *fullpath; /**< borrowed; valid while draw scene is unchanged */
    const char *display_name;            /**< borrowed */
    const char *leaf_name;               /**< borrowed */
    unsigned long long path_hash;
    int visible;
    int highlighted;
    int selected;
    int evaluated_region;
    int stale;
    const char *stale_reason;
    uint64_t source_revision;
    uint64_t inputs_revision;
    uint64_t realized_source_revision;
    uint64_t realized_inputs_revision;
    uint64_t drawn_revision;
    fastf_t transparency;
    bsg_draw_mode draw_mode;
    int line_width;
    point_t center;
};

struct ged_draw_group_record {
    ged_draw_group_ref ref;
    const char *path;                    /**< borrowed */
    struct bsg_view *view;               /**< borrowed; NULL for view-independent groups */
    bsg_draw_mode draw_mode;
    fastf_t transparency;
    int visible;
    int is_overlay;
    int is_view_scope;
    int in_view_scope;
    int is_view_source;
    int is_local_source;
    int shape_count;
};

GED_EXPORT extern struct ged_draw_transaction
ged_draw_transaction_make(ged_draw_transaction_kind kind, const char *path);

GED_EXPORT extern struct ged_draw_transaction
ged_draw_transaction_make_value(ged_draw_transaction_kind kind,
				const char *path,
				double value);

GED_EXPORT extern void
ged_draw_transaction_result_init(struct ged_draw_transaction_result *result);

GED_EXPORT extern void
ged_draw_transaction_result_free(struct ged_draw_transaction_result *result);

GED_EXPORT extern int
ged_draw_apply_transaction(struct ged *gedp,
			   const struct ged_draw_transaction *txn,
			   struct ged_draw_transaction_result *result);

/**
 * Subscribe to post-transaction draw-state events.
 *
 * Observers are called synchronously after a successful draw transaction has
 * reconciled libged draw state and populated a stable
 * ged_draw_transaction_result.  The returned token may be passed to
 * ged_draw_observer_remove(); 0 indicates registration failure.
 */
GED_EXPORT extern ged_draw_observer_token
ged_draw_observer_add(struct ged *gedp,
		      ged_draw_transaction_observer_func_t func,
		      void *client_data);

/**
 * Remove a draw-state observer.  Safe to call from inside an observer callback.
 * Returns 1 when a live observer was removed, 0 otherwise.
 */
GED_EXPORT extern int
ged_draw_observer_remove(struct ged *gedp, ged_draw_observer_token token);

GED_EXPORT extern int
ged_draw_mark_database_change(struct ged *gedp,
			      const char *path,
			      ged_draw_stale_reason reason);

GED_EXPORT extern int
ged_draw_apply_database_update(struct ged *gedp,
			       const char *path,
			       int removed,
			       int redraw);

GED_EXPORT extern const char *
ged_draw_stale_reason_name(ged_draw_stale_reason reason);

/**
 * Return non-zero when @p gedp has an initialized draw scene.
 */
GED_EXPORT extern int
ged_draw_scene_available(struct ged *gedp);

/**
 * Structured-path convenience wrappers for common command operations.
 */
GED_EXPORT extern int
ged_draw_apply_erase_path(struct ged *gedp,
			  const struct db_full_path *dfp);

GED_EXPORT extern int
ged_draw_apply_erase_path_prefix(struct ged *gedp,
				 const struct db_full_path *dfp);

/**
 * Return the current default draw mode used by draw commands that do not
 * specify an explicit render style override.
 */
GED_EXPORT extern bsg_draw_mode
ged_draw_default_mode(const struct ged *gedp);

/**
 * Erase from @p gedp's drawn scene the entry whose path equals @p dfp.
 * When a scene group's path is a strict ancestor of the erase path, the
 * matching sub-tree is removed without disturbing sibling sub-groups.
 */
GED_EXPORT extern void
ged_draw_erase_path(struct ged *gedp,
			     const struct db_full_path *dfp);

/**
 * Erase from @p gedp's drawn-object set every scene object whose path
 * contains @p name as one of its directory components.
 *
 * This is used when a database object is renamed or deleted and all drawn
 * references to that name must be removed.
 */
GED_EXPORT extern void
ged_draw_erase_name(struct ged *gedp, const char *name);

/**
 * Erase every drawn scene object whose path has @p dfp as a prefix subset.
 */
GED_EXPORT extern void
ged_draw_erase_path_prefix(struct ged *gedp,
			       const struct db_full_path *dfp);

/**
 * Compute the axis-aligned bounding box of the visible drawn scene in
 * @p gedp's active view set.  When @p pflag is zero, non-database overlay
 * records are excluded.
 * Returns 1 if the result is empty (no contributing objects), 0 otherwise.
 *
 * The bounds are returned in model coordinates.
 */
GED_EXPORT extern int
ged_draw_bounds(struct ged *gedp, vect_t *min, vect_t *max,
		    int pflag);

/**
 * Set the draw-scene highlight state for every drawn shape.  A non-zero
 * @p highlighted value asserts highlight state; zero clears it.
 */
GED_EXPORT extern void
ged_draw_set_highlight_state(struct ged *gedp, int highlighted);

/**
 * Set highlight state on all draw shapes whose database full path begins with
 * @p path[0..path_pos].  This is the semantic operation behind MGED matrix
 * path picking, where multiple rendered shapes can share the same editable
 * path prefix.
 *
 * Returns the number of matching shape records.
 */
GED_EXPORT extern int
ged_draw_set_highlighted_path_prefix(struct ged *gedp,
				     const struct db_full_path *path,
				     size_t path_pos,
				     int highlighted);

/**
 * Return the highlight-state revision counter.  Bumped on every transition
 * of the highlighted draw ref.  Cache a snapshot and compare to detect
 * "highlight may have changed since I last looked" cheaply.
 */
GED_EXPORT extern uint64_t
ged_draw_highlight_revision(const struct ged *gedp);

/**
 * Return the material revision counter.  The counter is incremented by
 * ged_draw_bump_material_revision() whenever the material/color table changes.
 * ged_draw_refresh_material_colors() does NOT increment the counter; it only
 * stamps per-shape color revisions.  Callers that cache per-shape color
 * data can compare a saved snapshot to detect when a recolor sweep is needed.
 */
GED_EXPORT extern uint64_t
ged_draw_material_revision(const struct ged *gedp);

/**
 * Bump the material revision counter.
 *
 * Call this after any operation that changes the effective material or
 * color table (e.g. 'color', 'mater', 'rmater', 'edmater') so that the
 * next ged_draw_refresh_material_colors() call recolors shapes that are
 * now stale.
 */
GED_EXPORT extern void
ged_draw_bump_material_revision(struct ged *gedp);

/**
 * Refresh per-object base color from the dbip's region/material table
 * (mater_struct chain) for every drawn scene object whose color stamp is
 * stale relative to the material revision counter.  Shapes that
 * were already colored since the last material-change event are skipped.
 *
 * Callers must invoke ged_draw_bump_material_revision() before this function
 * to signal that a material change occurred; without that bump, successive
 * calls will skip all already-stamped shapes.
 */
GED_EXPORT extern void
ged_draw_refresh_material_colors(struct ged *gedp);

/**
 * Return the current draw-scene structural revision as an unsigned long long.
 * Returns 0 when the scene is unavailable or after a clear operation.
 *
 * Prefer ged_draw_scene_revision() for new code that only needs change
 * detection.
 */
GED_EXPORT extern unsigned long long
ged_draw_scene_hash(struct ged *gedp);

/**
 * Return the raw structural revision counter for @p gedp's draw tree.
 * The counter is incremented on every structural mutation (group/shape
 * addition or removal) and reset to 0 by ged_draw_clear().  Returns
 * 0 when no objects have been drawn since the last zap (or ever).
 *
 * Callers that need to detect "drawn set changed" cheaply should compare
 * snapshots of this value rather than recomputing a content hash.
 */
GED_EXPORT extern uint64_t
ged_draw_scene_revision(struct ged *gedp);

GED_EXPORT extern int
ged_draw_shape_ref_is_null(ged_draw_shape_ref ref);

GED_EXPORT extern int
ged_draw_group_ref_is_null(ged_draw_group_ref ref);

GED_EXPORT extern int
ged_draw_shape_record_get(struct ged *gedp,
			  ged_draw_shape_ref ref,
			  struct ged_draw_shape_record *out);

GED_EXPORT extern int
ged_draw_group_record_get(struct ged *gedp,
			  ged_draw_group_ref ref,
			  struct ged_draw_group_record *out);

GED_EXPORT extern void
ged_draw_foreach_group_record(struct ged *gedp,
			      int (*cb)(const struct ged_draw_group_record *rec,
					void *userdata),
			      void *userdata);

GED_EXPORT extern void
ged_draw_foreach_shape_record(struct ged *gedp,
			      int (*cb)(const struct ged_draw_shape_record *rec,
					void *userdata),
			      void *userdata);

GED_EXPORT extern ged_draw_shape_ref
ged_draw_first_shape_ref(struct ged *gedp);

GED_EXPORT extern ged_draw_shape_ref
ged_draw_shape_ref_at(struct ged *gedp, int idx);

GED_EXPORT extern int
ged_draw_shape_ref_index(struct ged *gedp, ged_draw_shape_ref ref);

GED_EXPORT extern ged_draw_shape_ref
ged_draw_advance_shape_ref(struct ged *gedp, ged_draw_shape_ref ref, int delta);

GED_EXPORT extern ged_draw_group_ref
ged_draw_group_ref_of_shape(struct ged *gedp, ged_draw_shape_ref ref);

GED_EXPORT extern void
ged_draw_set_highlighted_shape_ref(struct ged *gedp, ged_draw_shape_ref ref);

GED_EXPORT extern int
ged_draw_shape_set_highlighted(struct ged *gedp, ged_draw_shape_ref ref, int highlighted);

GED_EXPORT extern int
ged_draw_shape_ref_get_color(struct ged *gedp, ged_draw_shape_ref ref, unsigned char rgb[3]);

GED_EXPORT extern int
ged_draw_shape_ref_set_color(struct ged *gedp, ged_draw_shape_ref ref, const unsigned char rgb[3]);

GED_EXPORT extern ged_draw_shape_ref
ged_draw_highlighted_shape_ref(const struct ged *gedp);

GED_EXPORT extern ged_draw_shape_ref
ged_draw_highlight_shape_ref_by_name(struct ged *gedp, const char *name);

GED_EXPORT extern struct bsg_interaction_record *
ged_draw_shape_interaction_record(struct ged *gedp,
				  ged_draw_shape_ref ref,
				  bsg_interaction_kind kind);

/**
 * Returns 1 if @p gedp has at least one drawn shape, 0 otherwise.
 */
GED_EXPORT extern int
ged_draw_has_shapes(struct ged *gedp);

/**
 * Report whether @p path is represented in the retained draw scene.
 *
 * Return values match qged/MGED drawn-state conventions:
 *   - 0: path is not drawn
 *   - 1: path is fully drawn
 *   - 2: path is partially drawn
 *
 * The query is answered from retained draw records.  When hierarchy coverage
 * must be distinguished from a partial draw, the no-index fallback walks only
 * @p path's database subtree and compares its leaf paths to active retained
 * shapes.  Pass @p mode < 0 to query all draw modes.
 */
GED_EXPORT extern int
ged_draw_path_state(struct ged *gedp,
		    struct bsg_view *v,
		    const char *path,
		    int mode);

/**
 * Return non-zero when @p rec belongs to the drawable database scene for
 * @p v.  Shared draw groups match non-independent views; independent views
 * match only groups in their private view scope.  Passing NULL for @p v
 * queries only shared draw groups.
 *
 * This helper answers view-scope membership only.  Callers that list renderable
 * database paths must still check record visibility, overlay status, and draw
 * mode as appropriate.
 */
GED_EXPORT extern int
ged_draw_group_record_in_view(const struct ged_draw_group_record *rec,
			      struct bsg_view *v);

/**
 * Append retained draw paths to @p result and return the number appended.
 *
 * When @p expanded is zero, paths are draw-intent/group paths suitable for
 * command-level listings such as "who" and render-script replay.  When
 * @p expanded is non-zero, paths are realized leaf shape paths.  Overlay,
 * hidden, empty, and out-of-view records are omitted.  Pass @p mode < 0 to list
 * all draw modes.
 */
GED_EXPORT extern size_t
ged_draw_list_paths(struct ged *gedp,
		    struct bsg_view *v,
		    int mode,
		    int expanded,
		    struct bu_vls *result);

/**
 * Return non-zero if command-visible drawn database paths exist for @p v and
 * @p mode.  This uses the same visibility/view/overlay filtering as
 * ged_draw_list_paths(..., expanded=0) without formatting or sorting paths.
 */
GED_EXPORT extern int
ged_draw_has_paths(struct ged *gedp,
		   struct bsg_view *v,
		   int mode);

/**
 * Returns the first drawn scene object in display order, or NULL if
 * none are drawn.
 */
GED_EXPORT extern int
ged_draw_shape_count(struct ged *gedp);

/**
 * Erase all scene groups from @p gedp's drawn-object set, destroying vlists
 * and freeing all associated scene objects.
 * This is the "zap" operation.
 */
GED_EXPORT extern void
ged_draw_clear(struct ged *gedp);

/**
 * Erase database draw groups in one draw scope.  Passing NULL for @p v clears
 * shared database draw groups.  Passing an independent view clears that view's
 * independent database draw scope.
 *
 * Returns the number of top-level retained draw groups removed.
 */
GED_EXPORT extern int
ged_draw_clear_view(struct ged *gedp, struct bsg_view *v);

/**
 * Return 1 if any scene groups exist (i.e., something is drawn), 0 if the
 * display is empty.
 */
GED_EXPORT extern int
ged_draw_has_groups(struct ged *gedp);

__END_DECLS

#endif /* GED_BSG_GED_DRAW_H */

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
