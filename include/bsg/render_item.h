/*                  R E N D E R _ I T E M . H
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
 * Render item — the resolved, flat descriptor emitted by BSG traversal
 * that a backend draws without further scene-graph access.
 *
 * During `bsg_render_request_collect` the renderer resolves each shape's
 * accumulated world transform, appearance properties, payload type,
 * visibility state, selection/highlight state, and LoD decision into a
 * `bsg_render_item`.  Items are then sorted into four render phases and
 * stored in a `bsg_render_batch`.  Payload realization is a distinct
 * pre-render step; drawing no longer owns payload update policy.
 *
 * Backends receive items in this order:
 *   BSG_RENDER_PHASE_OPAQUE → BSG_RENDER_PHASE_TRANSPARENT →
 *   BSG_RENDER_PHASE_OVERLAY → BSG_RENDER_PHASE_HUD
 *
 * Within BSG_RENDER_PHASE_TRANSPARENT items are ordered back-to-front
 * when `BSG_RENDER_FLAG_SORTED_ALPHA` is set on the request.
 */
/** @{ */
/* @file bsg/render_item.h */

#ifndef BSG_RENDER_ITEM_H
#define BSG_RENDER_ITEM_H

#include "common.h"

#include "vmath.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/appearance_action.h"
#include "bsg/faceplate.h"
#include "bsg/feature.h"
#include "bsg/payload.h"
#include "bsg/render_settings.h"

__BEGIN_DECLS

struct bsg_mesh_lod;

/**
 * Render phase — controls the order in which items are dispatched.
 *
 * Phase ordering: OPAQUE (0) → TRANSPARENT (1) → OVERLAY (2) → HUD (3).
 * Do not change the integer values; they serve as indices into the
 * per-phase bucket arrays inside the render-request executor.
 */
typedef enum bsg_render_phase {
    BSG_RENDER_PHASE_OPAQUE      = 0, /**< @brief  solid geometry, no alpha */
    BSG_RENDER_PHASE_TRANSPARENT = 1, /**< @brief  geometry with transparency > 0 */
    BSG_RENDER_PHASE_OVERLAY     = 2, /**< @brief  overlay shapes */
    BSG_RENDER_PHASE_HUD         = 3, /**< @brief  HUD / faceplate shapes */
    BSG_RENDER_PHASE_COUNT       = 4  /**< @brief  sentinel — number of phases */
} bsg_render_phase;

typedef enum bsg_render_geometry_kind {
    BSG_RENDER_GEOMETRY_NONE = 0,
    BSG_RENDER_GEOMETRY_MESH,
    BSG_RENDER_GEOMETRY_LINE_SET,
    BSG_RENDER_GEOMETRY_INDEXED_LINE_SET,
    BSG_RENDER_GEOMETRY_POINT_SET,
    BSG_RENDER_GEOMETRY_INDEXED_FACE_SET,
    BSG_RENDER_GEOMETRY_TEXT,
    BSG_RENDER_GEOMETRY_IMAGE,
    BSG_RENDER_GEOMETRY_OVERLAY,
    BSG_RENDER_GEOMETRY_HUD,
    BSG_RENDER_GEOMETRY_ANNOTATION,
    BSG_RENDER_GEOMETRY_CSG_PROXY,
    BSG_RENDER_GEOMETRY_BREP_PROXY,
    BSG_RENDER_GEOMETRY_EDIT_PREVIEW,
    BSG_RENDER_GEOMETRY_EXTERNAL_PROXY
} bsg_render_geometry_kind;

typedef enum bsg_render_overlay_geometry_kind {
    BSG_RENDER_OVERLAY_GEOMETRY_NONE = 0,
    BSG_RENDER_OVERLAY_GEOMETRY_AXES,
    BSG_RENDER_OVERLAY_GEOMETRY_GRID
} bsg_render_overlay_geometry_kind;

typedef enum bsg_render_image_kind {
    BSG_RENDER_IMAGE_NONE = 0,
    BSG_RENDER_IMAGE_RASTER,
    BSG_RENDER_IMAGE_FRAMEBUFFER
} bsg_render_image_kind;

typedef enum bsg_render_surface_normal_kind {
    BSG_RENDER_SURFACE_NORMALS_NONE = 0,
    BSG_RENDER_SURFACE_NORMALS_PER_VERTEX,
    BSG_RENDER_SURFACE_NORMALS_PER_FACE,
    BSG_RENDER_SURFACE_NORMALS_PER_INDEX /**< @brief one normal per non-negative face-index entry */
} bsg_render_surface_normal_kind;

typedef enum bsg_render_proxy_kind {
    BSG_RENDER_PROXY_NONE = 0,
    BSG_RENDER_PROXY_CSG,
    BSG_RENDER_PROXY_BREP,
    BSG_RENDER_PROXY_EDIT_PREVIEW,
    BSG_RENDER_PROXY_EXTERNAL
} bsg_render_proxy_kind;

struct bsg_render_proxy {
    bsg_render_proxy_kind kind;
    uint64_t source_identity;
    uint64_t revision;
};

struct bsg_render_geometry {
    bsg_render_geometry_kind kind;
    uint64_t revision;
    uint64_t source_identity;
    size_t element_count;
    struct {
	char *label;
	point_t position;
	point_t target;
	int size;
	int line_flag;
	int anchor;
	int arrow;
    } text;
    struct {
	point_t *points;
	point_t *normals;
	int *commands;
	int *indices;
	size_t point_count;
	size_t normal_count;
	size_t command_count;
	size_t index_count;
    } arrays;
    struct {
	bsg_render_overlay_geometry_kind kind;
	struct bsg_axes axes;
	struct bsg_grid_state grid;
    } overlay;
    struct {
	bsg_render_image_kind kind;
	size_t width;
	size_t height;
	size_t channels;
	unsigned char *pixels;
	size_t pixel_count;
	int framebuffer_mode;
    } image;
    struct {
	point_t *points;
	vect_t *normals;
	int *indices;
	size_t point_count;
	size_t normal_count;
	size_t index_count;
	size_t face_count;
	bsg_render_surface_normal_kind normal_kind;
	int material_valid;
	struct bsg_resolved_appearance material;
    } surface;
    struct {
	char *text;
	bsg_annotation_space space;
	point_t anchor;
	mat_t model_mat;
	mat_t display_mat;
	point_t *points;
	size_t point_count;
	struct bsg_annotation_segment *segments;
	size_t segment_count;
    } annotation;
    struct bsg_render_proxy proxy;
    union {
	const struct bsg_mesh_lod *mesh;
    } data;
};

#define BSG_RENDER_FEATURE_ARROWS 0x01u

typedef int (*bsg_render_segment_cb)(const point_t a,
				     const point_t b,
				     void *data);

struct bsg_render_features {
    unsigned int flags;
    fastf_t arrow_tip_length;
    fastf_t arrow_tip_width;
    int hud_feature_type;
};

#define BSG_RENDER_ORDER_OPAQUE_CHILD_FIRST 0x01u

typedef enum bsg_render_source_scope {
    BSG_RENDER_SOURCE_SCOPE_UNKNOWN = 0,
    BSG_RENDER_SOURCE_SCOPE_DATABASE,
    BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED,
    BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL,
    BSG_RENDER_SOURCE_SCOPE_INTERNAL_CHILD
} bsg_render_source_scope;

typedef enum bsg_render_geometry_role {
    BSG_RENDER_GEOMETRY_ROLE_UNKNOWN = 0,
    BSG_RENDER_GEOMETRY_ROLE_DATABASE_OBJECT,
    BSG_RENDER_GEOMETRY_ROLE_VIEW_OVERLAY,
    BSG_RENDER_GEOMETRY_ROLE_LINE_SET,
    BSG_RENDER_GEOMETRY_ROLE_TEXT_LABEL,
    BSG_RENDER_GEOMETRY_ROLE_AXES_WIDGET,
    BSG_RENDER_GEOMETRY_ROLE_POLYGON_REGION,
    BSG_RENDER_GEOMETRY_ROLE_HUD,
    BSG_RENDER_GEOMETRY_ROLE_IMAGE,
    BSG_RENDER_GEOMETRY_ROLE_MESH
} bsg_render_geometry_role;

typedef enum bsg_render_draw_intent {
    BSG_RENDER_DRAW_INTENT_UNKNOWN = 0,
    BSG_RENDER_DRAW_INTENT_DATABASE,
    BSG_RENDER_DRAW_INTENT_VIEW_FEATURE,
    BSG_RENDER_DRAW_INTENT_OVERLAY,
    BSG_RENDER_DRAW_INTENT_HUD,
    BSG_RENDER_DRAW_INTENT_EDIT_PREVIEW,
    BSG_RENDER_DRAW_INTENT_INTERNAL
} bsg_render_draw_intent;

struct bsg_render_source {
    uint64_t source_id;        /**< @brief stable drawable-source identity */
    uint64_t geometry_id;      /**< @brief backend geometry source identity */
    uint64_t lod_id;           /**< @brief LoD selector identity, 0 if none */
    uint64_t lod_policy_id;    /**< @brief selected LoD/source-policy identity, 0 if none */
    const char *name;          /**< @brief borrowed source/display name */
    bsg_render_source_scope scope;
    bsg_render_geometry_role geometry_role;
    bsg_render_draw_intent draw_intent;
    enum bsg_feature_family feature_family;
    bsg_overlay_role overlay_role;
    bsg_overlay_class overlay_class;
    int non_database_source;
};

struct bsg_render_context {
    mat_t model2view;
    mat_t view2model;
    mat_t projection;
    int has_projection;
    vect_t clipmin;
    vect_t clipmax;
    int zclip;
    int lighting;
    int viewport_width;
    int viewport_height;
    unsigned char background1[3];
    unsigned char background2[3];
    unsigned char geometry_default_color[3];
    unsigned int request_flags;
    unsigned int backend_capabilities;
    int hud_pass;
    int overlay_pass;
    uint64_t settings_hash;
    struct bsg_render_settings settings;
};


/**
 * A fully resolved, flat description of one drawable shape.
 *
 * Produced by `bsg_render_request_collect` from the view's retained stream.
 * Graph-node pointers are render-construction context only and are not part
 * of this backend contract.  Backends consume `source`, `geometry`,
 * `features`, `appearance`, and `context` instead of dereferencing the graph.
 * The `model_mat` is the accumulated product of all `BSG_NODE_TRANSFORM`
 * ancestor matrices at the time the shape was visited.
 */
struct bsg_render_item {
    struct bsg_view   *view;          /**< @brief  request view context (borrowed) */
    mat_t              model_mat;     /**< @brief  accumulated model-to-world matrix */
    struct bsg_render_source source;  /**< @brief  backend-facing source identity */
    struct bsg_render_context context; /**< @brief request/batch metadata snapshot */
    const char        *name;          /**< @brief  source node name (borrowed) */
    uint64_t           payload_revision; /**< @brief typed payload revision captured for this item */
    uint64_t           cache_identity;/**< @brief  resolved backend cache identity */
    bsg_payload_realization_kind realization_kind; /**< @brief payload realization domain */
    bsg_payload_realization_status realization_status; /**< @brief current/stale/failed */
    bsg_payload_stale_reason stale_reason; /**< @brief why payload realization is stale */

    /**
     * Fully resolved appearance.
     *
     * Populated during bsg_render_request_collect.
     * Backends MUST read appearance fields from here and must NOT re-derive
     * them by accessing node internals during drawing.
     */
    struct bsg_resolved_appearance appearance;

    /* Geometry / phase classification */
    struct bsg_render_geometry geometry; /**< @brief backend-facing geometry view */
    struct bsg_render_features features; /**< @brief resolved feature-node behavior */
    bsg_render_phase   phase;         /**< @brief  render phase this item belongs to */

    /* Resolved visibility and selection semantics. */
    int                visible;       /**< @brief  effective visibility in item->view */
    int                force_draw;    /**< @brief  effective force-draw bypass */
    int                highlighted;   /**< @brief  resolved highlight state */
    int                selected;      /**< @brief  selection highlight compatibility state */

    /* Resolved draw bounds used by backends for small-object culling. */
    fastf_t            bounds_radius; /**< @brief  model-space bounding sphere radius */
    point_t            bounds_center; /**< @brief  model-space bounding sphere center */
    int                non_database_source;/**< @brief  non-database feature or overlay source */
    int                has_bounds; /**< @brief bounds_center and bounds_radius are valid */
    int                graph_depth; /**< @brief scene-graph depth captured during traversal */
    size_t             sequence; /**< @brief collection order within the render request */
    unsigned int       order_flags; /**< @brief BSG_RENDER_ORDER_* traversal ordering hints */

    /* Resolved LoD decision. */
    int                lod_level;     /**< @brief  active LoD level, or -1 */
    int                lod_level_count; /**< @brief  number of physical LoD levels */

    /**
     * Within-phase ordering hint.
     *
     * For BSG_RENDER_PHASE_TRANSPARENT: a larger value means the item is
     * farther from the camera and should be drawn first (back-to-front).
     * For BSG_RENDER_PHASE_HUD: corresponds to bsg_hud_node_meta::sort_order.
     * For other phases: 0 (insertion order, except opaque child-before-parent
     * dispatch uses graph_depth).
     */
    int                sort_key;
};


/* -----------------------------------------------------------------------
 * Item lifecycle
 * ----------------------------------------------------------------------- */

/**
 * Allocate and zero-initialise a render item.
 * Returns NULL on allocation failure.
 * The caller owns the returned item and must release it with
 * bsg_render_item_free().
 */
BSG_EXPORT extern struct bsg_render_item *
bsg_render_item_create(void);

BSG_EXPORT extern size_t
bsg_render_item_foreach_line_segment(const struct bsg_render_item *item,
				     bsg_render_segment_cb cb,
				     void *data);

BSG_EXPORT extern int
bsg_render_item_has_line_segments(const struct bsg_render_item *item);

BSG_EXPORT extern size_t
bsg_render_item_foreach_wire_segment(const struct bsg_render_item *item,
				     bsg_render_segment_cb cb,
				     void *data);

BSG_EXPORT extern int
bsg_render_item_has_wire_segments(const struct bsg_render_item *item);

/**
 * Release a render item previously allocated by bsg_render_item_create().
 * No-op if @p item is NULL.
 */
BSG_EXPORT extern void
bsg_render_item_free(struct bsg_render_item *item);

__END_DECLS

#endif /* BSG_RENDER_ITEM_H */

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
