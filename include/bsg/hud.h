/*                        H U D . H
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
 * Per-view HUD root and overlay
 * role/lifecycle/order metadata.
 *
 * A @c bsg_view owns a HUD scene root (@c gv_hud_root) that is separate
 * from the model scene attachment.  The HUD root contains one
 * child per faceplate feature (center dot, axes, scale, ADC,
 * grid, rubber-band rect, params text, framebuffer).  Each feature carries a
 * @c bsg_hud_node_meta descriptor that records:
 *
 *   - which faceplate feature it represents (@c bsg_hud_feature_type)
 *   - its coordinate space (@c bsg_hud_coord)
 *   - its render-stack placement (@c bsg_overlay_role)
 *   - its semantic overlay class (@c bsg_overlay_class)
 *   - its lifecycle policy (@c bsg_overlay_lifecycle)
 *   - its render-phase sort order
 *
 * @c bsg_hud_sync() reads the canonical faceplate records exposed by
 * @c bsg/view_state.h and updates each feature node's visibility state
 * (UP = enabled, DOWN = disabled) plus a lightweight HUD payload snapshot
 * stored behind HUD accessors.
 * The HUD render pass then collects @c gv_hud_root through
 * @c bsg_render_request_collect() using @c sort_order to order the items.
 *
 * Actual rasterisation stays in libdm via a backend adapter that consumes the
 * HUD render items produced by the render batch collector.
 */
/** @{ */
/* @file bsg/hud.h */

#ifndef BSG_HUD_H
#define BSG_HUD_H

#include "common.h"
#include "bsg/defines.h"

__BEGIN_DECLS

struct bsg_view;   /* forward declaration */


/* -----------------------------------------------------------------------
 * Overlay role
 *
 * Describes where in the render stack a node lives.
 * ----------------------------------------------------------------------- */

/**
 * Overlay rendering role — controls when in the render pipeline a node is
 * drawn relative to scene geometry.
 */
typedef enum bsg_overlay_role {
    BSG_OVERLAY_ROLE_MODEL  = 0, /**< @brief drawn after scene solids, in model space */
    BSG_OVERLAY_ROLE_SCREEN = 1, /**< @brief drawn after model overlays, in screen/HUD space */
    BSG_OVERLAY_ROLE_XRAY   = 2  /**< @brief always on top (depth-ignore), in model space */
} bsg_overlay_role;

/**
 * Semantic overlay class — records what kind of overlay/HUD feature a node
 * represents independent of its placement in the render pipeline.
 */
typedef enum bsg_overlay_class {
    BSG_OVERLAY_CLASS_FACEPLATE             = 0, /**< @brief built-in faceplate/HUD feature */
    BSG_OVERLAY_CLASS_EDIT_HANDLE           = 1, /**< @brief edit handles / grips */
    BSG_OVERLAY_CLASS_MEASURE               = 2, /**< @brief measurement graphics */
    BSG_OVERLAY_CLASS_SELECTION_RUBBER_BAND = 3, /**< @brief selection rectangle */
    BSG_OVERLAY_CLASS_SNAP_GUIDE            = 4, /**< @brief snapping guide */
    BSG_OVERLAY_CLASS_COMMAND_RESULT        = 5, /**< @brief command-owned result graphic */
    BSG_OVERLAY_CLASS_DIAGNOSTIC            = 6, /**< @brief diagnostics / status visuals */
    BSG_OVERLAY_CLASS_TCL_OVERLAY           = 7, /**< @brief Tcl-created overlay */
    BSG_OVERLAY_CLASS_POLYGON_EDIT          = 8, /**< @brief polygon edit helper */
    BSG_OVERLAY_CLASS_SKETCH_EDIT           = 9, /**< @brief sketch edit helper */
    BSG_OVERLAY_CLASS_USER_ANNOTATION       = 10 /**< @brief user-created annotation */
} bsg_overlay_class;


/* -----------------------------------------------------------------------
 * HUD coordinate space
 * ----------------------------------------------------------------------- */

/**
 * Coordinate space used for geometry in a HUD node.
 */
typedef enum bsg_hud_coord {
    BSG_HUD_COORD_SCREEN_PX       = 0, /**< @brief pixel coordinates (0,0 = top-left) */
    BSG_HUD_COORD_NDC             = 1, /**< @brief normalized device coords (+-1) */
    BSG_HUD_COORD_VIEW_PLANE      = 2, /**< @brief view-plane coords (same as NDC for ortho) */
    BSG_HUD_COORD_MODEL_ANCHORED  = 3  /**< @brief label anchored to a model-space point */
} bsg_hud_coord;


/* -----------------------------------------------------------------------
 * Overlay lifecycle policy
 * ----------------------------------------------------------------------- */

/**
 * Lifecycle policy for an overlay or HUD node.
 */
typedef enum bsg_overlay_lifecycle {
    BSG_OVERLAY_LC_PERSISTENT             = 0, /**< @brief node survives across frames; only rebuilt on settings change */
    BSG_OVERLAY_LC_PER_FRAME              = 1, /**< @brief node is rebuilt/refreshed every frame */
    BSG_OVERLAY_LC_PER_COMMAND            = 2, /**< @brief node lives for one command result */
    BSG_OVERLAY_LC_PER_TOOL               = 3, /**< @brief node lives for one tool session */
    BSG_OVERLAY_LC_PER_VIEW               = 4, /**< @brief node is private to one view */
    BSG_OVERLAY_LC_SHARED_VIEW_SET        = 5, /**< @brief node is shared by a view set */
    BSG_OVERLAY_LC_AUTO_REMOVE_ON_SOURCE  = 6  /**< @brief remove when source object changes */
} bsg_overlay_lifecycle;


/* -----------------------------------------------------------------------
 * Faceplate feature type
 *
 * Identifies which faceplate feature a HUD node represents.
 * Values also serve as the default sort_order (render-phase position).
 * ----------------------------------------------------------------------- */

/**
 * Faceplate feature tag stored in @c bsg_hud_node_meta::feature_type.
 * Numeric values define the default render-phase order: lower values are
 * drawn first.
 */
typedef enum bsg_hud_feature_type {
    BSG_HUD_FEATURE_CENTER_DOT  = 0, /**< @brief single center dot (screen-space point) */
    BSG_HUD_FEATURE_MODEL_AXES  = 1, /**< @brief model-space axes widget */
    BSG_HUD_FEATURE_VIEW_AXES   = 2, /**< @brief view-space axes widget (corner) */
    BSG_HUD_FEATURE_VIEW_SCALE  = 3, /**< @brief linear scale bar */
    BSG_HUD_FEATURE_ADC         = 4, /**< @brief angle/distance cursor */
    BSG_HUD_FEATURE_GRID        = 5, /**< @brief reference grid */
    BSG_HUD_FEATURE_RECT        = 6, /**< @brief rubber-band selection rectangle */
    BSG_HUD_FEATURE_VIEW_PARAMS = 7, /**< @brief text overlay (size, center, az/el, FPS) */
    BSG_HUD_FEATURE_FRAMEBUFFER = 8  /**< @brief framebuffer underlay/overlay */
} bsg_hud_feature_type;

/** Number of distinct faceplate features managed by the HUD root. */
#define BSG_HUD_FEATURE_COUNT 9


/* -----------------------------------------------------------------------
 * Per-node HUD metadata
 * ----------------------------------------------------------------------- */

/**
 * Metadata attached to each HUD child record.
 */
struct bsg_hud_node_meta {
    bsg_hud_feature_type  feature_type; /**< @brief which faceplate feature */
    bsg_hud_coord         coord_space;  /**< @brief geometry coordinate convention */
    bsg_overlay_role      role;         /**< @brief render-pass placement */
    bsg_overlay_class     overlay_class;/**< @brief semantic overlay classification */
    bsg_overlay_lifecycle lifecycle;    /**< @brief rebuild frequency */
    int                   sort_order;   /**< @brief render-phase ordering (lower = earlier) */
};

/**
 * HUD payload snapshot stored on each HUD feature record.
 *
 * This records the current faceplate/HUD state needed by the libdm HUD
 * backend adapter without requiring it to reach back into command or UI
 * state for feature-specific bookkeeping.
 */
struct bsg_hud_payload {
    bsg_hud_feature_type feature_type; /**< @brief feature this payload snapshot represents */
    union {
	struct bsg_other_state            other;
	struct bsg_axes                   axes;
	struct bsg_adc_state              adc;
	struct bsg_grid_state             grid;
	struct bsg_interactive_rect_state rect;
	struct bsg_params_state           params;
	struct {
	    int mode;
	} framebuffer;
    } data;
};


/* -----------------------------------------------------------------------
 * HUD root management
 * ----------------------------------------------------------------------- */

/**
 * Create the per-view HUD root for @p v and store it in @p v->gv_hud_root.
 *
 * Pre-allocates @c BSG_HUD_FEATURE_COUNT child nodes (one per faceplate
 * feature), each initially disabled.  @c bsg_hud_sync() subsequently updates
 * visibility to reflect the current retained faceplate records.
 *
 * Returns 0 on success, -1 if @p v is NULL or allocation fails.  If the HUD
 * root already exists this is a no-op.
 */
BSG_EXPORT extern int
bsg_hud_ensure(struct bsg_view *v);


/**
 * Destroy the HUD root and all its child feature nodes, and clear
 * @p v->gv_hud_root.
 *
 * No-op if @p v is NULL or the HUD root has not been created.
 */
BSG_EXPORT extern void
bsg_hud_root_destroy(struct bsg_view *v);


/**
 * Synchronize the HUD root's child nodes with the current faceplate settings
 * exposed by @p v's view-state record.
 *
 * For each faceplate feature: if enabled, the corresponding child node's
 * visible when enabled and hidden when disabled.  The HUD root is created
 * automatically on first call if it does not yet exist.
 *
 * Returns 0 on success, -1 if @p v is NULL or the root cannot be created.
 */
BSG_EXPORT extern int
bsg_hud_sync(struct bsg_view *v);


/**
 * HUD node metadata access is private to libbsg render collection.
 */

__END_DECLS

#endif /* BSG_HUD_H */

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
