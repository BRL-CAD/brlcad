/*                  D R A W _ I N T E N T . H
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
 * Explicit draw-intent metadata attached to BSG scene groups.
 *
 * When a database draw command (e.g. "draw comb/a") creates a scene group
 * in the BSG draw tree, a @c bsg_draw_intent record is attached to that
 * group through scene-ref APIs.  The intent records the source
 * path that was drawn, the draw mode (wireframe/shaded/hidden-line/etc.),
 * the LoD policy, and whether the group is a synthetic overlay container.
 *
 * This replaces the old convention of:
 *   - inferring the drawn path from the group's node name
 *   - detecting overlay groups by comparing the node name to "_overlays"
 *   - discovering draw mode by walking shape descendants
 *
 * Draw/erase semantics are now queryable through the intent API rather than
 * reconstructed from group naming conventions and child structure.
 *
 * Per the draw-intent API criteria:
 *   - Database draw/erase semantics are explicit scene metadata.
 *   - Export/raytrace/report commands do not reconstruct command intent
 *     from child tables or raw node fields.
 */
/** @{ */
/* @file bsg/draw_intent.h */

#ifndef BSG_DRAW_INTENT_H
#define BSG_DRAW_INTENT_H

#include "common.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/scene_builder.h"

struct bsg_view; /* forward declaration */
struct bsg_db_event; /* forward declaration */

__BEGIN_DECLS

/* -----------------------------------------------------------------------
 * Draw mode enumeration
 *
 * These values match the retained per-shape draw-mode storage used by
 * bsg_appearance_settings.  New code should use BSG_DRAW_MODE_* names rather
 * than raw integers.
 * ----------------------------------------------------------------------- */

/**
 * Rendering mode requested by the draw command.
 *
 * Numeric values are intentionally identical to the retained draw-mode field
 * values so that existing renderers can cast freely between the two
 * representations while both representations exist.  Some old GED parser
 * names do not describe the modern retained behavior; prefer the semantic BSG
 * names below for new code.
 */
typedef enum {
    BSG_DRAW_MODE_WIRE         = 0,  /**< wireframe only */
    BSG_DRAW_MODE_SHADED_BOTS  = 1,  /**< shaded BOTs/polysolids; CSG NOT evaluated */
    BSG_DRAW_MODE_SHADED       = 2,  /**< shaded; CSG NOT evaluated */
    BSG_DRAW_MODE_EVAL_WIRE    = 3,  /**< wireframe with evaluated booleans */
    BSG_DRAW_MODE_HIDDEN_LINE  = 4,  /**< hidden-line removal */
    BSG_DRAW_MODE_EVAL_POINTS  = 5   /**< sampled evaluated triangle points */
} bsg_draw_mode;

/* -----------------------------------------------------------------------
 * LoD (Level-of-Detail) policy
 * ----------------------------------------------------------------------- */

/**
 * Level-of-detail policy for the drawn group.
 */
typedef enum {
    BSG_DRAW_LOD_AUTO  = 0,  /**< automatic (let the renderer decide) */
    BSG_DRAW_LOD_FORCE = 1,  /**< force LoD mesh even at close range */
    BSG_DRAW_LOD_FULL  = 2,  /**< force full-detail tessellation */
    BSG_DRAW_LOD_OFF   = 3   /**< LoD disabled (always draw full detail) */
} bsg_draw_lod_policy;

/* -----------------------------------------------------------------------
 * Draw operation intent
 *
 * Command paths use these lightweight records to describe the requested
 * scene mutation before GED applies it to the BSG draw graph.  The records
 * are stack-allocatable and do not own their string pointers.
 * ----------------------------------------------------------------------- */

typedef enum {
    BSG_DRAW_OP_NONE = 0,
    BSG_DRAW_OP_ERASE_PATH,
    BSG_DRAW_OP_ERASE_PATH_PREFIX,
    BSG_DRAW_OP_ERASE_NAME,
    BSG_DRAW_OP_CLEAR,
    BSG_DRAW_OP_SET_TRANSPARENCY,
    BSG_DRAW_OP_SET_DEFAULT_DRAW_MODE,
    BSG_DRAW_OP_SET_VISIBILITY,
    BSG_DRAW_OP_SET_HIGHLIGHT,
    BSG_DRAW_OP_MATERIAL_CHANGED,
    BSG_DRAW_OP_REFRESH_MATERIAL_COLORS,
    BSG_DRAW_OP_REDRAW
} bsg_draw_operation_kind;

struct bsg_draw_operation {
    bsg_draw_operation_kind op_kind;
    const char *op_path;
    double op_value;
};

BSG_EXPORT extern struct bsg_draw_operation
bsg_draw_operation_make(bsg_draw_operation_kind kind, const char *path);

BSG_EXPORT extern struct bsg_draw_operation
bsg_draw_operation_make_value(bsg_draw_operation_kind kind, const char *path,
			      double value);

/* -----------------------------------------------------------------------
 * bsg_draw_intent
 *
 * Lifecycle: bsg_draw_intent_create() / bsg_draw_intent_free().
 * Ownership: the scene graph owns attached draw-intent records; scene teardown
 * frees them.
 * ----------------------------------------------------------------------- */

/**
 * Draw-intent metadata record.
 *
 * Attached to BSG scene groups by the draw command when a database path
 * is drawn.  Synthetic groups (the overlay container, etc.) are marked
 * with @c di_overlay = 1.
 *
 * Fields:
 *   di_path     — source path string as supplied to the draw command
 *                 (e.g. "comb/a").  Never NULL on valid intents.
 *   di_mode     — draw mode selected for this group.
 *   di_lod      — LoD policy for this group.
 *   di_mixed    — non-zero when mixed-mode drawing is enabled
 *                 (do not evict objects drawn in a different mode).
 *   di_appearance — draw-command style/fallback settings to apply when this
 *                 intent is realized or re-expanded.
 *   di_overlay  — non-zero when this is a synthetic overlay container
 *                 (the "_overlays" group or similar) rather than a real
 *                 database-path draw.
 */
struct bsg_draw_intent {
    struct bu_vls  di_path;    /**< @brief drawn source path string */
    bsg_draw_mode  di_mode;    /**< @brief draw mode */
    bsg_draw_lod_policy di_lod;     /**< @brief LoD policy */
    int            di_mixed;   /**< @brief mixed-mode flag */
    struct bsg_appearance_settings di_appearance; /**< @brief draw style/fallback settings */
    int            di_overlay; /**< @brief 1 = synthetic overlay container */
};

/* -----------------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------------- */

/**
 * Allocate and initialize a draw-intent for a database draw command.
 *
 * @p path    Source path string (copied into @c di_path).
 * @p mode    Draw mode.
 *
 * Returns NULL on allocation failure.  The caller should bind the intent
 * to a scene ref via bsg_scene_set_draw_intent().
 */
BSG_EXPORT extern struct bsg_draw_intent *
bsg_draw_intent_create(const char *path, bsg_draw_mode mode);

/**
 * Allocate and initialize an overlay draw-intent.
 *
 * Used internally to mark the "_overlays" container.
 * The @c di_overlay flag is set; @c di_path is set to @p name.
 *
 * Returns NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_draw_intent *
bsg_draw_intent_create_overlay(const char *name);

/**
 * Free a draw-intent allocated by bsg_draw_intent_create() or
 * bsg_draw_intent_create_overlay().
 *
 * No-op if @p di is NULL.
 */
BSG_EXPORT extern void
bsg_draw_intent_free(struct bsg_draw_intent *di);

BSG_EXPORT extern void
bsg_scene_set_draw_intent(bsg_scene_ref ref, struct bsg_draw_intent *di);

BSG_EXPORT extern struct bsg_draw_intent *
bsg_scene_draw_intent(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_draw_intent_revalidate(bsg_scene_ref root,
				 const struct bsg_db_event *event);

/* -----------------------------------------------------------------------
 * Accessors
 * ----------------------------------------------------------------------- */

/**
 * Return the source path string from @p di (never NULL for valid intents).
 *
 * Returns NULL if @p di is NULL.
 */
BSG_EXPORT extern const char *
bsg_draw_intent_path(const struct bsg_draw_intent *di);

/**
 * Update the source path stored in @p di.
 *
 * No-op if @p di is NULL.
 */
BSG_EXPORT extern void
bsg_draw_intent_set_path(struct bsg_draw_intent *di, const char *path);

/**
 * Return the draw mode recorded in @p di.
 *
 * Returns BSG_DRAW_MODE_WIRE (0) if @p di is NULL.
 */
BSG_EXPORT extern bsg_draw_mode
bsg_draw_intent_mode(const struct bsg_draw_intent *di);

/**
 * Update the draw mode stored in @p di.
 *
 * No-op if @p di is NULL.
 */
BSG_EXPORT extern void
bsg_draw_intent_set_mode(struct bsg_draw_intent *di, bsg_draw_mode mode);

/**
 * Return the LoD policy recorded in @p di.
 *
 * Returns BSG_DRAW_LOD_AUTO (0) if @p di is NULL.
 */
BSG_EXPORT extern bsg_draw_lod_policy
bsg_draw_intent_lod(const struct bsg_draw_intent *di);

/**
 * Copy the draw-command appearance settings recorded in @p di into @p out.
 *
 * Returns 1 when @p out was populated, 0 when either pointer is NULL.
 */
BSG_EXPORT extern int
bsg_draw_intent_appearance(const struct bsg_draw_intent *di,
			   struct bsg_appearance_settings *out);

/**
 * Update the draw-command appearance settings recorded in @p di.
 *
 * Also synchronizes @c di_mode and @c di_mixed from the supplied settings.
 * No-op if either pointer is NULL.
 */
BSG_EXPORT extern void
bsg_draw_intent_set_appearance(struct bsg_draw_intent *di,
			       const struct bsg_appearance_settings *settings);

/**
 * Return non-zero if @p di is a synthetic overlay intent
 * (di_overlay == 1), zero for real database-path intents.
 *
 * Returns 0 if @p di is NULL.
 */
BSG_EXPORT extern int
bsg_draw_intent_is_overlay(const struct bsg_draw_intent *di);

/* -----------------------------------------------------------------------
 * Database event (D2)
 *
 * Describes a database I/O change that may affect drawn intents.
 * Produced by libged after every modifying db_io call and consumed by
 * bsg_draw_intent_revalidate().
 * ----------------------------------------------------------------------- */

/** Kind of database change that triggered the event. */
typedef enum {
    BSG_DB_EVENT_MODIFIED = 0,  /**< object contents changed */
    BSG_DB_EVENT_REMOVED  = 1,  /**< object deleted from database */
    BSG_DB_EVENT_RENAMED  = 2   /**< object moved/renamed (old → new path) */
} bsg_db_event_kind;

/**
 * Lightweight value-type describing a single database change.
 *
 * For BSG_DB_EVENT_RENAMED, @c dbe_path holds the *new* full path and
 * @c dbe_old_path holds the previous name.  For other kinds @c dbe_old_path
 * is unused (set to NULL / empty).
 *
 * Callers may stack-allocate this struct; bsg_draw_intent_revalidate()
 * does not retain a reference after it returns.
 */
struct bsg_db_event {
    const char       *dbe_path;      /**< affected full path (new path for renames) */
    const char       *dbe_old_path;  /**< previous path (rename only, else NULL)    */
    bsg_db_event_kind dbe_kind;      /**< what happened                             */
};

/* -----------------------------------------------------------------------
 * Scene-level actions
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Draw-intent action API
 *
 * The functions below are the primary draw-intent manipulation interface
 * for draw-intent consumers.  Implementations live in libbsg/draw_intent.c.
 * ----------------------------------------------------------------------- */

/**
 * Re-evaluate every intent whose source path is affected by @p event.
 *
 * - BSG_DB_EVENT_MODIFIED: marks the matching intent's geometry as stale
 *   (sets a "needs-redraw" flag on the group node).  The group is NOT
 *   removed; callers must issue a new draw command or call the redraw
 *   path to regenerate geometry.
 * - BSG_DB_EVENT_REMOVED: calls bsg_draw_intent_erase_by_path() for the
 *   affected path, removing the group from the scene.
 * - BSG_DB_EVENT_RENAMED: updates @c di_path in the matching intent to
 *   the new name (@c dbe_path); the group is retained in the scene.
 *
 * No-op if @p root or @p event is NULL, or if no matching intent exists.
 *
 * @returns the number of intents affected (0 = no match).
 */
/**
 * Collect the intent-bearing groups under @p root that should contribute
 * to a geometry export (rt, nirt, plot, bot_dump, …) into @p out.
 *
 * @p flags is a bitmask that controls which groups are included:
 *   - BSG_EXPORT_INCLUDE_OVERLAYS (0x01) — include synthetic overlay groups.
 *   - BSG_EXPORT_SHADED_ONLY      (0x02) — only include groups whose draw
 *     mode is shaded or shaded-eval (exclude wireframe-only groups).
 *
 * @p out must be initialised by the caller.  The caller is responsible
 * for bu_ptbl_free().
 *
 * No-op if @p root or @p out is NULL.
 */
#define BSG_EXPORT_INCLUDE_OVERLAYS 0x01ULL  /**< include overlay groups */
#define BSG_EXPORT_SHADED_ONLY      0x02ULL  /**< shaded/eval groups only */

__END_DECLS

#endif /* BSG_DRAW_INTENT_H */

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
