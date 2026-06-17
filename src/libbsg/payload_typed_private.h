/*               P A Y L O A D _ T Y P E D _ P R I V A T E . H
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
 * Typed payload object model.
 *
 * The edit-preview payload API adds a payload-agnostic
 * @c bsg_edit_preview_source helper that lets interactive editors publish
 * transient draft/commit geometry through typed preview records instead of
 * exposing application-owned live graph scaffolding.
 *
 * A *payload* is the typed geometry or annotation data carried by a
 * BSG_NODE_SHAPE.  Previously, payload data was stored in untyped
 * node-private lifecycle pointers with caller-managed lifecycle.  Typed
 * payloads replace that with a first-class @c bsg_payload handle that:
 *
 *  - carries an explicit type discriminant (@c bsg_payload_type enum);
 *  - tracks a monotonic revision stamp so renderers can detect stale state
 *    without inspecting type-private data;
 *  - provides a common lifecycle/query interface: @c pl_free, @c pl_update,
 *    @c pl_bounds, @c pl_export, and @c pl_backend_prepare;
 *  - stores the type-private data in a typed @c pl_data union so tools can
 *    reach concrete data through a documented field rather than a raw cast.
 *
 * The private @c BSG_PAYLOAD_* role masks remain valid for coarse metadata
 * queries in private traversal.  @c bsg_payload_type gives the full
 * fine-grained discriminant and is the preferred way to branch on payload kind
 * in new code.
 *
 * To attach a payload to a scene shape, call @c bsg_scene_set_payload().  The
 * shape then owns the payload and will free it through @c bsg_payload_free()
 * when the shape is destroyed.
 */
/** @{ */
/* @file payload_typed_private.h */

#ifndef BSG_PAYLOAD_TYPED_PRIVATE_H
#define BSG_PAYLOAD_TYPED_PRIVATE_H

#include "common.h"
#include <stdint.h>
#include "bu/list.h"
#include "vmath.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/edit_preview.h"
#include "bsg/node.h"
#include "bsg/payload.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

/* Forward declarations */
struct bsg_view;
struct bsg_payload;
struct bsg_polygon;
struct bsg_mesh_lod;
struct bsg_edit_preview_source;
struct bsg_sketch_live_data;
struct bsg_snap_result;
struct bsg_pick_record;
struct bsg_measure_result;
struct fb;

/* -----------------------------------------------------------------------
 * Payload type discriminant
 * ----------------------------------------------------------------------- */

/**
 * Fine-grained payload type for BSG_NODE_SHAPE nodes.
 *
 * Each value corresponds to a concrete payload data layout and a set of
 * builder/accessor functions defined in this header.  The coarser
 * private @c BSG_PAYLOAD_* role masks remain valid for fast path filtering;
 * these enum values give the full type when switching on payload kind.
 */
/* -----------------------------------------------------------------------
 * Typed payload data union
 *
 * The union lets typed-payload accessors reach the concrete data through a
 * documented field rather than a raw void* cast.  The untyped pl_opaque
 * member handles payload types whose concrete structs are defined in other
 * headers (e.g. bsg_mesh_lod from bsg/lod.h).
 * ----------------------------------------------------------------------- */

/**
 * Union of typed payload data pointers.
 * One member is valid depending on the @c bsg_payload::pl_type field.
 */
union bsg_payload_data {
    struct bsg_label           *text;        /**< @brief BSG_PL_TEXT — 3D text label */
    struct bsg_label           *hud_text;    /**< @brief BSG_PL_HUD_TEXT — HUD label/text */
    struct bsg_payload_line_set *line_set;   /**< @brief BSG_PL_LINE_SET — point/cmd line set */
    struct bsg_polygon         *polygon;     /**< @brief BSG_PL_POLYGON — interactive polygon */
    struct bsg_mesh_lod        *mesh;        /**< @brief BSG_PL_MESH — LoD mesh payload */
    void                       *csg;         /**< @brief BSG_PL_CSG — adaptive CSG source */
    void                       *brep;        /**< @brief BSG_PL_BREP — BRep source */
    struct bsg_payload_image   *image;       /**< @brief BSG_PL_IMAGE — raster image */
    struct bsg_payload_framebuffer *framebuffer; /**< @brief BSG_PL_FRAMEBUFFER — framebuffer payload */
    struct bsg_axes            *axes;        /**< @brief BSG_PL_AXES — axes widget */
    struct bsg_grid_state      *grid;        /**< @brief BSG_PL_GRID — grid state */
    struct bsg_sketch_live_data *sketch;     /**< @brief BSG_PL_SKETCH — sketch edit-preview source */
    struct bsg_payload_annotation *annotation; /**< @brief BSG_PL_ANNOTATION — annotation payload */
    struct bsg_edit_preview_source *edit_preview; /**< @brief BSG_PL_EDIT_PREVIEW — edit-preview source */
    void                       *opaque;      /**< @brief catch-all for other types */
};

/* -----------------------------------------------------------------------
 * Core payload handle
 * ----------------------------------------------------------------------- */

/**
 * Typed payload handle.
 *
 * Attach to a scene shape with @c bsg_scene_set_payload().  The shape takes
 * ownership and will call @c bsg_payload_free() when the shape is freed.
 *
 * Lifecycle summary:
 *  - Allocate with @c bsg_payload_create() (or a type-specific builder).
 *  - Attach to a shape with @c bsg_scene_set_payload(); after this the shape
 *    owns the payload.
 *  - Bump @c pl_revision with @c bsg_payload_bump_revision() whenever the
 *    payload data changes so renderers can detect stale cached state.
 *  - The shape frees the payload via @c pl_free() during its normal teardown.
 */
struct bsg_payload {
    bsg_payload_type          pl_type;      /**< @brief type discriminant */
    uint64_t                  pl_revision;  /**< @brief monotonic revision stamp; bump on data change */
    void                     *pl_owner;     /**< @brief owning scene storage while attached, NULL when detached */
    union bsg_payload_data    pl;           /**< @brief typed data pointer */
    struct bsg_payload_realization realization; /**< @brief explicit realization state and diagnostics */

    /** Free this payload and its private data.  Must not be NULL. */
    void  (*pl_free)(struct bsg_payload *pl);

    /**
     * Optionally refresh @c pl.opaque or generated vlist geometry before
     * drawing.  Called by private realization for payload types that
     * require an update pass.  May be NULL for static payloads.
     */
    void  (*pl_update)(struct bsg_payload *pl, struct bsg_view *v);

    /**
     * Compute the axis-aligned bounding box of this payload in model
     * coordinates.  Returns 1 on success, 0 if the payload has no
     * meaningful spatial extent.  May be NULL.
     */
    int   (*pl_bounds)(struct bsg_payload *pl, point_t *bmin, point_t *bmax);

    /**
     * Serialize payload geometry to @p out (e.g. as a vlist or JSON blob).
     * Returns 0 on success.  May be NULL.
     */
    int   (*pl_export)(struct bsg_payload *pl, struct bu_vls *out);

    /**
     * Prepare backend-specific cached data for @p backend_ctx.  Called
     * by the backend adapter before the first draw after creation or after
     * revision changes.  Returns 0 on success.  May be NULL.
     *
     * Render settings extend this into a full
     * backend adapter interface.
     */
    int   (*pl_backend_prepare)(struct bsg_payload *pl, void *backend_ctx);

    /**
     * Optional snap candidate query for this payload.
     * Returns 1 on success, 0 on no candidates, negative for unsupported.
     */
    int   (*pl_snap)(struct bsg_payload *pl, const point_t sample, double tol,
		     unsigned long long kinds, struct bsg_snap_result *out);

    /**
     * Optional measure candidate query for this payload.
     * Returns 1 on success, 0 on no candidates, negative for unsupported.
     */
    int   (*pl_measure_candidates)(struct bsg_payload *pl, const point_t a,
				   const point_t b, struct bsg_measure_result *out);

    /**
     * Optional per-payload vertex pick query.
     * Given a 3D @p sample point (in model coordinates), find the nearest
     * payload vertex and fill @p out with the result.
     * Returns 1 on success, 0 on no candidates, negative for unsupported.
     * May be NULL for payloads that do not support vertex picking.
     */
    int   (*pl_pick)(struct bsg_payload *pl, const point_t sample,
		     struct bsg_pick_record *out);
};

/* -----------------------------------------------------------------------
 * Core payload lifecycle
 * ----------------------------------------------------------------------- */

/**
 * Allocate a new @c bsg_payload of the given @p type.
 *
 * All function pointers are initialised to NULL; set them as needed after
 * calling this function or use a type-specific builder (e.g.
 * @c bsg_payload_text_create()).
 *
 * @p pl.opaque is set to NULL; set @p pl.text / @p pl.axes / etc. after
 * allocation.
 *
 * @returns newly allocated payload, or NULL on allocation failure.
 */
BSG_EXPORT extern struct bsg_payload *
bsg_payload_create(bsg_payload_type type);

/**
 * Free @p pl and its private data by calling @c pl->pl_free(pl).
 * No-op if @p pl is NULL.
 */
BSG_EXPORT extern void
bsg_payload_free(struct bsg_payload *pl);

/**
 * Increment @p pl->pl_revision by one.
 * Used to signal that payload data has changed so that renderers can
 * detect and re-upload cached GPU resources.
 * No-op if @p pl is NULL.
 */
BSG_EXPORT extern void
bsg_payload_bump_revision(struct bsg_payload *pl);

BSG_EXPORT extern bsg_payload_realization_kind
bsg_payload_default_realization_kind(bsg_payload_type type);

BSG_EXPORT extern const char *
bsg_payload_realization_kind_name(bsg_payload_realization_kind kind);

BSG_EXPORT extern const char *
bsg_payload_realization_status_name(bsg_payload_realization_status status);

BSG_EXPORT extern const char *
bsg_payload_stale_reason_name(bsg_payload_stale_reason reason);

BSG_EXPORT extern void
bsg_payload_mark_source_revision(struct bsg_payload *pl,
				 uint64_t source_revision,
				 bsg_payload_stale_reason reason);

BSG_EXPORT extern void
bsg_payload_mark_stale(struct bsg_payload *pl,
		       bsg_payload_stale_reason reason,
		       const char *explanation);

BSG_EXPORT extern const struct bsg_payload_realization *
bsg_payload_realization_state(const struct bsg_payload *pl);

BSG_EXPORT extern int
bsg_payload_is_stale(const struct bsg_payload *pl);

BSG_EXPORT extern int
bsg_payload_realize(struct bsg_payload *pl, struct bsg_view *v);

/* -----------------------------------------------------------------------
 * Scene-shape payload binding
 * ----------------------------------------------------------------------- */

/**
 * Attach @p pl to @p ref, transferring ownership to that scene shape.
 * Any previously attached payload is freed first.  No-op if @p ref is NULL.
 * If @p pl is NULL the existing payload, if any, is removed and freed.
 */
BSG_EXPORT extern void
bsg_scene_set_payload(bsg_scene_ref ref, struct bsg_payload *pl);

BSG_EXPORT extern struct bsg_payload *
bsg_scene_payload(bsg_scene_ref ref);

/* -----------------------------------------------------------------------
 * Typed payload update helpers
 * ----------------------------------------------------------------------- */

/**
 * Call @c pl_update on the payload attached to @p ref (if any) when the
 * payload type requires a pre-render update.
 */
BSG_EXPORT extern void
bsg_scene_payload_update(bsg_scene_ref ref, struct bsg_view *v);

/* -----------------------------------------------------------------------
 * TEXT payload (bsg_label)
 * ----------------------------------------------------------------------- */

/**
 * Create a BSG_PL_TEXT payload wrapping @p label.
 * Ownership of @p label is transferred to the payload.
 *
 * The payload's @c pl_free will call @c BU_VLS_VLSFREE on the label's
 * @c bu_vls field and then release the @c bsg_label allocation.
 *
 * @returns newly allocated payload, or NULL on failure.
 */
BSG_EXPORT extern struct bsg_payload *
bsg_payload_text_create(struct bsg_label *label);

/**
 * Return the @c bsg_label from a BSG_PL_TEXT @p payload, or NULL if @p
 * payload is not of that type.
 */
BSG_EXPORT extern struct bsg_label *
bsg_payload_text_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * HUD_TEXT payload
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_hud_text_create(struct bsg_label *label);

BSG_EXPORT extern struct bsg_label *
bsg_payload_hud_text_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * LINE_SET payload
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_line_set_create(point_t *points, const int *cmds, size_t point_cnt);

BSG_EXPORT extern struct bsg_payload_line_set *
bsg_payload_line_set_get(struct bsg_payload *payload);

/**
 * Append @p add_cnt additional (point, cmd) pairs to @p payload.
 * The existing point/cmd arrays are reallocated to accommodate the new data.
 * Bumps the payload revision on success.
 * @returns 1 on success, 0 on failure.
 */
BSG_EXPORT extern int
bsg_payload_line_set_append_segments(struct bsg_payload *payload,
	const point_t *points, const int *cmds, size_t add_cnt);

/**
 * Replace the entire content of @p payload with @p point_cnt new (point, cmd)
 * pairs.  The previous arrays are freed and new ones allocated.
 * Bumps the payload revision on success.
 * @returns 1 on success, 0 on failure.
 */
BSG_EXPORT extern int
bsg_payload_line_set_replace(struct bsg_payload *payload,
	const point_t *points, const int *cmds, size_t point_cnt);

/**
 * Remove all points and commands from @p payload (sets point_cnt to 0 and
 * frees the point/cmd arrays).
 * Bumps the payload revision on success.
 * @returns 1 on success, 0 on failure.
 */
BSG_EXPORT extern int
bsg_payload_line_set_clear(struct bsg_payload *payload);

/**
 * Return the number of points in @p payload, or 0 if @p payload is not a
 * BSG_PL_LINE_SET payload.
 */
BSG_EXPORT extern size_t
bsg_payload_line_set_point_count(const struct bsg_payload *payload);

/**
 * Return the command code at index @p idx in @p payload, or -1 if @p payload
 * is not a BSG_PL_LINE_SET payload or @p idx is out of range.
 */
BSG_EXPORT extern int
bsg_payload_line_set_cmd_at(const struct bsg_payload *payload, size_t idx);

/* -----------------------------------------------------------------------
 * POLYGON payload
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_polygon_create(struct bsg_polygon *polygon);

BSG_EXPORT extern struct bsg_polygon *
bsg_payload_polygon_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * MESH / CSG / BREP payloads
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_mesh_create(struct bsg_mesh_lod *mesh);

BSG_EXPORT extern struct bsg_mesh_lod *
bsg_payload_mesh_get(struct bsg_payload *payload);

BSG_EXPORT extern struct bsg_payload *
bsg_payload_csg_create(void *opaque);

BSG_EXPORT extern struct bsg_payload *
bsg_payload_brep_create(void *opaque);

/* -----------------------------------------------------------------------
 * IMAGE / FRAMEBUFFER payloads
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_image_create(size_t width, size_t height, size_t channels, const unsigned char *pixels);

BSG_EXPORT extern struct bsg_payload_image *
bsg_payload_image_get(struct bsg_payload *payload);

BSG_EXPORT extern struct bsg_payload *
bsg_payload_framebuffer_create(struct fb *fbp, int mode);

BSG_EXPORT extern struct bsg_payload_framebuffer *
bsg_payload_framebuffer_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * AXES payload (bsg_axes)
 * ----------------------------------------------------------------------- */

/**
 * Create a BSG_PL_AXES payload wrapping @p axes.
 * Ownership of @p axes is transferred to the payload.
 *
 * @returns newly allocated payload, or NULL on failure.
 */
BSG_EXPORT extern struct bsg_payload *
bsg_payload_axes_create(struct bsg_axes *axes);

/**
 * Return the @c bsg_axes from a BSG_PL_AXES @p payload, or NULL if @p
 * payload is not of that type.
 */
BSG_EXPORT extern struct bsg_axes *
bsg_payload_axes_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * GRID payload
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_grid_create(const struct bsg_grid_state *grid);

BSG_EXPORT extern struct bsg_grid_state *
bsg_payload_grid_get(struct bsg_payload *payload);

/* -----------------------------------------------------------------------
 * EDIT-PREVIEW payload
 *
 * bsg_edit_preview_source is the typed transient draft/commit geometry
 * contract for interactive editors.  It stores editor context plus optional
 * callbacks used by render, pick, snap, and measure paths:
 *   - revision query   (bsg_edit_preview_revision_cb_t)
 *   - draft/update     (bsg_edit_preview_update_cb_t)
 *   - bounds query     (bsg_edit_preview_bounds_cb_t)
 *   - pick query       (bsg_edit_preview_pick_cb_t)
 *   - snap query       (bsg_edit_preview_snap_cb_t)
 *   - teardown         (bsg_edit_preview_free_cb_t)
 *
 * Editors create BSG_PL_EDIT_PREVIEW payloads for transient preview features
 * and publish begin/update/commit/cancel interaction records with stable
 * feature refs and source paths.
 * ----------------------------------------------------------------------- */

struct bsg_edit_preview_source {
    void *editor_ctx;          /**< @brief borrowed editor state */
    void *aux_ctx;             /**< @brief optional auxiliary context */
    void *preview_ctx;         /**< @brief callback context, defaults to editor_ctx */
    int owns_preview_ctx;      /**< @brief if non-zero, call free_cb on teardown */
    uint64_t source_revision;  /**< @brief source/database revision currently advertised */
    uint64_t inputs_revision;  /**< @brief view/settings revision currently advertised */
    uint64_t last_realized_revision; /**< @brief last realized preview revision */
    uint64_t last_realized_source_revision; /**< @brief source revision last realized */
    uint64_t last_realized_inputs_revision; /**< @brief input revision last realized */
    bsg_edit_preview_stale_reason stale_reason; /**< @brief current/stale explanation */
    bsg_edit_preview_revision_cb_t revision_cb; /**< @brief revision query */
    bsg_edit_preview_update_cb_t   update_cb;   /**< @brief draft/update */
    bsg_edit_preview_bounds_cb_t   bounds_cb;   /**< @brief bounds query */
    bsg_edit_preview_pick_cb_t     pick_cb;     /**< @brief pick query */
    bsg_edit_preview_snap_cb_t     snap_cb;     /**< @brief snap query */
    bsg_edit_preview_free_cb_t     free_cb;     /**< @brief teardown */
};

BSG_EXPORT extern struct bsg_payload *
bsg_payload_edit_preview_create(void *editor_ctx, void *aux_ctx);

BSG_EXPORT extern struct bsg_edit_preview_source *
bsg_payload_edit_preview_get_data(struct bsg_payload *payload);

BSG_EXPORT extern int
bsg_payload_edit_preview_set_ops(struct bsg_payload *payload,
	void *preview_ctx,
	int owns_preview_ctx,
	bsg_edit_preview_revision_cb_t revision_cb,
	bsg_edit_preview_update_cb_t   update_cb,
	bsg_edit_preview_bounds_cb_t   bounds_cb,
	bsg_edit_preview_pick_cb_t     pick_cb,
	bsg_edit_preview_snap_cb_t     snap_cb,
	bsg_edit_preview_free_cb_t     free_cb);

BSG_EXPORT extern uint64_t
bsg_payload_edit_preview_revision(struct bsg_payload *payload);

BSG_EXPORT extern int
bsg_payload_edit_preview_mark_source_revision(struct bsg_payload *payload,
					      uint64_t source_revision,
					      bsg_edit_preview_stale_reason reason);

BSG_EXPORT extern int
bsg_payload_edit_preview_mark_inputs_revision(struct bsg_payload *payload,
					      uint64_t inputs_revision,
					      bsg_edit_preview_stale_reason reason);

BSG_EXPORT extern int
bsg_payload_edit_preview_is_stale(struct bsg_payload *payload);

BSG_EXPORT extern bsg_edit_preview_stale_reason
bsg_payload_edit_preview_stale_reason(struct bsg_payload *payload);

BSG_EXPORT extern const char *
bsg_payload_edit_preview_stale_reason_name(bsg_edit_preview_stale_reason reason);

BSG_EXPORT extern uint64_t
bsg_payload_edit_preview_last_realized_source_revision(struct bsg_payload *payload);

BSG_EXPORT extern uint64_t
bsg_payload_edit_preview_last_realized_inputs_revision(struct bsg_payload *payload);

BSG_EXPORT extern int
bsg_payload_edit_preview_realize(struct bsg_payload *payload, struct bsg_view *v);

BSG_EXPORT extern int
bsg_payload_edit_preview_bounds(struct bsg_payload *payload, point_t *bmin, point_t *bmax);

BSG_EXPORT extern int
bsg_payload_edit_preview_pick(struct bsg_payload *payload, struct bsg_view *v,
	int x, int y, void *pick_out);

BSG_EXPORT extern int
bsg_payload_edit_preview_snap(struct bsg_payload *payload, struct bsg_view *v,
	const point_t sample_pt, point_t out_pt);

/* -----------------------------------------------------------------------
 * SKETCH payload
 *
 * The sketch-specific bsg_sketch_live_data struct and bsg_payload_sketch_*()
 * functions remain available for existing sketch callers. New editors should
 * use BSG_PL_EDIT_PREVIEW / bsg_edit_preview_source /
 * bsg_payload_edit_preview_*() instead.
 *
 * Note on callback type compatibility:
 *   The bsg_sketch_live_*_cb_t typedefs below are identical in signature to
 *   the corresponding bsg_edit_preview_*_cb_t typedefs above.
 * ----------------------------------------------------------------------- */

typedef uint64_t (*bsg_sketch_live_revision_cb_t)(void *live_ctx);
/**
 * Return non-zero when the sketch preview changed and should advance revision,
 * zero when no change occurred.
 */
typedef int (*bsg_sketch_live_update_cb_t)(void *live_ctx, struct bsg_view *v);
typedef int (*bsg_sketch_live_bounds_cb_t)(void *live_ctx, point_t *bmin, point_t *bmax);
typedef int (*bsg_sketch_live_pick_cb_t)(void *live_ctx, struct bsg_view *v, int x, int y, void *pick_out);
typedef int (*bsg_sketch_live_snap_cb_t)(void *live_ctx, struct bsg_view *v, const point_t sample_pt, point_t out_pt);
typedef void (*bsg_sketch_live_free_cb_t)(void *live_ctx);

/**
 * Opaque edit-preview contract for a BSG_PL_SKETCH node.
 *
 * Stores editor/view pointers plus optional callbacks implementing the full
 * edit-preview contract:
 *   - revision query
 *   - realize/update
 *   - bounds query
 *   - pick query
 *   - snap query
 *   - teardown/free
 *
 * Ownership rules:
 *   - @c rt_edit_ptr and @c grid_ptr are borrowed from the editor/view.
 *   - @c live_ctx ownership is controlled by @c owns_live_ctx; if non-zero,
 *     @c free_cb is called when replacing the owned context or during payload
 *     teardown.
 */
struct bsg_sketch_live_data {
    void *rt_edit_ptr;   /**< @brief opaque rt_edit * — owned by the editor */
    void *grid_ptr;      /**< @brief opaque bsg_grid_state * — owned by the view */
    void *live_ctx;      /**< @brief callback context (defaults to rt_edit_ptr) */
    int owns_live_ctx;   /**< @brief if non-zero, call free_cb on teardown */
    struct bsg_payload *geometry; /**< @brief generated sketch line-set payload */
    uint64_t last_realized_revision; /**< @brief last realized live revision */
    bsg_sketch_live_revision_cb_t revision_cb; /**< @brief revision query callback (optional) */
    bsg_sketch_live_update_cb_t update_cb; /**< @brief realize/update callback (non-zero => changed) */
    bsg_sketch_live_bounds_cb_t bounds_cb; /**< @brief bounds query callback */
    bsg_sketch_live_pick_cb_t pick_cb; /**< @brief pick query callback */
    bsg_sketch_live_snap_cb_t snap_cb; /**< @brief snap query callback */
    bsg_sketch_live_free_cb_t free_cb; /**< @brief optional teardown callback */
};

/**
 * Create a BSG_PL_SKETCH payload carrying @p rt_edit_ptr and @p grid_ptr.
 *
 * Neither pointer is dereferenced by libbsg; they are stored for external
 * tooling (e.g. type-safe cast via bsg_payload_sketch_get_data()).
 * The caller retains ownership of the pointed-to objects; the payload does
 * NOT free them.
 *
 * @returns newly allocated payload, or NULL on failure.
 */
BSG_EXPORT extern struct bsg_payload *
bsg_payload_sketch_create(void *rt_edit_ptr, void *grid_ptr);

/**
 * Return the @c bsg_sketch_live_data from a BSG_PL_SKETCH @p payload,
 * or NULL if @p payload is not of that type.
 */
BSG_EXPORT extern struct bsg_sketch_live_data *
bsg_payload_sketch_get_data(struct bsg_payload *payload);

BSG_EXPORT extern struct bsg_payload *
bsg_payload_sketch_geometry(struct bsg_payload *payload);

/**
 * Set the full edit-preview callback contract for a BSG_PL_SKETCH payload.
 *
 * @p payload must be a BSG_PL_SKETCH payload.
 * @p live_ctx is passed to all callbacks.  If NULL, @c rt_edit_ptr is used.
 * If @p owns_live_ctx is non-zero, @p free_cb is called at payload teardown.
 * If @p revision_cb is supplied, it is authoritative for revision values.
 * If @p update_cb reports a change (non-zero) but @p revision_cb does not
 * advance, libbsg bumps the realized revision by one to preserve monotonicity.
 * Replacing a previously owned live context with a different pointer releases
 * the old context through its registered free callback.
 *
 * @returns 1 on success, 0 on failure (wrong payload type or NULL payload).
 */
BSG_EXPORT extern int
bsg_payload_sketch_set_live_ops(struct bsg_payload *payload,
	void *live_ctx,
	int owns_live_ctx,
	bsg_sketch_live_revision_cb_t revision_cb,
	bsg_sketch_live_update_cb_t update_cb,
	bsg_sketch_live_bounds_cb_t bounds_cb,
	bsg_sketch_live_pick_cb_t pick_cb,
	bsg_sketch_live_snap_cb_t snap_cb,
	bsg_sketch_live_free_cb_t free_cb);

/**
 * Return the current sketch preview revision for @p payload, or 0 on failure.
 */
BSG_EXPORT extern uint64_t
bsg_payload_sketch_revision(struct bsg_payload *payload);

/**
 * Realize/update @p payload by invoking the edit-preview update callback.
 *
 * Returns:
 *   1 if payload revision changed,
 *   0 if no change or no update callback is defined,
 *  -1 on invalid input.
 */
BSG_EXPORT extern int
bsg_payload_sketch_realize(struct bsg_payload *payload, struct bsg_view *v);

/**
 * Query sketch preview bounds for @p payload.  Returns non-zero on success.
 */
BSG_EXPORT extern int
bsg_payload_sketch_bounds(struct bsg_payload *payload, point_t *bmin, point_t *bmax);

/**
 * Query sketch preview pick information for @p payload.  Returns non-zero on success.
 */
BSG_EXPORT extern int
bsg_payload_sketch_pick(struct bsg_payload *payload, struct bsg_view *v, int x, int y, void *pick_out);

/**
 * Query sketch preview snap result for @p payload.  Returns non-zero on success.
 */
BSG_EXPORT extern int
bsg_payload_sketch_snap(struct bsg_payload *payload, struct bsg_view *v, const point_t sample_pt, point_t out_pt);

/* -----------------------------------------------------------------------
 * ANNOTATION payload
 * ----------------------------------------------------------------------- */

BSG_EXPORT extern struct bsg_payload *
bsg_payload_annotation_create(const char *text, point_t *points, size_t point_cnt);

BSG_EXPORT extern struct bsg_payload *
bsg_payload_annotation_create_record(const char *summary,
				     bsg_annotation_space space,
				     const point_t anchor,
				     const mat_t model_mat,
				     const mat_t display_mat,
				     const point_t *points,
				     size_t point_cnt,
				     const struct bsg_annotation_segment *segments,
				     size_t segment_cnt);

BSG_EXPORT extern struct bsg_payload_annotation *
bsg_payload_annotation_get(struct bsg_payload *payload);

__END_DECLS

#endif /* BSG_PAYLOAD_TYPED_PRIVATE_H */

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
