/*              B A C K E N D _ S C E N E . H
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
 * Retained backend-scene contract.
 *
 * bsg_backend_scene is an OpenInventor-style backend adapter that consumes
 * resolved bsg_render_item records and mirrors them into a retained scene.
 * It deliberately does not inspect private scene internals: every retained field is
 * copied from the render item contract.
 *
 * A native backend or future Obol adapter can either consume the flat
 * bsg_render_item stream directly or iterate these retained nodes.  Both paths
 * preserve GED/BSG scene semantics; no backend owns GED commands or private
 * BSG node layout.
 *
 * Obol integration is intentionally outside GED command policy.  An Obol
 * backend should enter through a `bsg_backend_adapter` consuming render
 * batches, or through iteration over `bsg_backend_scene_node` records.
 */
/** @{ */
/* @file bsg/backend_scene.h */

#ifndef BSG_BACKEND_SCENE_H
#define BSG_BACKEND_SCENE_H

#include "common.h"
#include "vmath.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/backend_adapter.h"
#include "bsg/render_item.h"

__BEGIN_DECLS

typedef enum bsg_backend_scene_record_kind {
    BSG_BACKEND_SCENE_NODE_GEOMETRY = 0,
    BSG_BACKEND_SCENE_NODE_TEXT,
    BSG_BACKEND_SCENE_NODE_IMAGE,
    BSG_BACKEND_SCENE_NODE_OVERLAY,
    BSG_BACKEND_SCENE_NODE_HUD,
    BSG_BACKEND_SCENE_NODE_CAMERA,
    BSG_BACKEND_SCENE_NODE_LIGHT,
    BSG_BACKEND_SCENE_NODE_CLIP_PLANE
} bsg_backend_scene_record_kind;

typedef enum bsg_backend_scene_capability_gap {
    BSG_BACKEND_SCENE_GAP_NONE         = 0x00u,
    BSG_BACKEND_SCENE_GAP_TRANSPARENCY = 0x01u,
    BSG_BACKEND_SCENE_GAP_WIREFRAME    = 0x02u,
    BSG_BACKEND_SCENE_GAP_SHADED       = 0x04u,
    BSG_BACKEND_SCENE_GAP_HUD          = 0x08u,
    BSG_BACKEND_SCENE_GAP_SORTED_ALPHA = 0x10u,
    BSG_BACKEND_SCENE_GAP_BREP         = 0x20u
} bsg_backend_scene_capability_gap;

typedef enum bsg_backend_scene_adapter_kind {
    BSG_BACKEND_SCENE_ADAPTER_EXTERNAL = 0,
    BSG_BACKEND_SCENE_ADAPTER_RETAINED_REFERENCE,
    BSG_BACKEND_SCENE_ADAPTER_OBOL_RESERVED
} bsg_backend_scene_adapter_kind;

struct bsg_backend_scene_camera {
    mat_t model2view;
    mat_t view2model;
    mat_t projection;
    int has_projection;
    int viewport_width;
    int viewport_height;
    unsigned char background1[3];
    unsigned char background2[3];
};

struct bsg_backend_scene_clip {
    vect_t min;
    vect_t max;
    int enabled;
};

struct bsg_backend_scene_lights {
    int enabled;
    size_t count;
};

struct bsg_backend_scene_material {
    unsigned char color[3];
    fastf_t transparency;
    int draw_mode;
    int line_width;
    int line_style;
    int evaluated_region;
    unsigned int active_layers;
    uint32_t material_revision;
    uint32_t appearance_revision;
};

struct bsg_backend_scene_transform {
    mat_t model;
    point_t bounds_center;
    fastf_t bounds_radius;
};

struct bsg_backend_scene_geometry {
    bsg_render_geometry_kind kind;
    uint64_t source_identity;
    uint64_t revision;
    size_t element_count;
    /* Scene-owned field snapshots copied from the render item. */
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
    bsg_payload_realization_kind realization_kind;
    bsg_payload_realization_status realization_status;
    bsg_payload_stale_reason stale_reason;
};

struct bsg_backend_scene_source {
    uint64_t geometry_id;
    uint64_t lod_id;
    uint64_t lod_policy_id;
    bsg_render_source_scope scope;
    bsg_render_geometry_role geometry_role;
    bsg_render_draw_intent draw_intent;
    enum bsg_feature_family feature_family;
    bsg_overlay_role overlay_role;
    bsg_overlay_class overlay_class;
    int non_database_source;
    int graph_depth;
    size_t sequence;
    unsigned int order_flags;
};

struct bsg_backend_scene_features {
    unsigned int flags;
    fastf_t arrow_tip_length;
    fastf_t arrow_tip_width;
    int hud_feature_type;
};

struct bsg_backend_scene_text {
    int hud_feature_type;
    char *label;
    point_t position;
    point_t target;
    int size;
    int line_flag;
    int anchor;
    int arrow;
};

struct bsg_backend_scene_overlay {
    int overlay_pass;
    int hud_pass;
    bsg_render_phase phase;
    int sort_key;
    bsg_render_overlay_geometry_kind geometry_kind;
    struct bsg_axes axes;
    struct bsg_grid_state grid;
};

struct bsg_backend_scene_selection {
    int visible;
    int force_draw;
    int highlighted;
    int selected;
};

struct bsg_backend_scene_lod {
    int level;
    int level_count;
    uint64_t identity;
};

struct bsg_backend_scene_node {
    uint64_t cache_identity;
    uint64_t source_identity;
    uint64_t payload_revision;
    uint64_t geometry_revision;
    uint64_t last_seen_generation;
    uint64_t update_count;
    uint64_t draw_count;
    unsigned int stale;

    bsg_backend_scene_record_kind scene_kind;
    struct bsg_backend_scene_source source;
    struct bsg_backend_scene_geometry geometry;
    struct bsg_backend_scene_features features;
    struct bsg_backend_scene_material material;
    struct bsg_backend_scene_transform transform;
    struct bsg_backend_scene_text text;
    struct bsg_backend_scene_overlay overlay;
    struct bsg_backend_scene_selection selection;
    struct bsg_backend_scene_lod lod;
    uint64_t settings_hash;
    unsigned int request_flags;
    unsigned int backend_capabilities;
    unsigned int capability_gaps;

    struct bsg_backend_scene_node *next;
};

struct bsg_backend_scene_stats {
    uint64_t generation;
    size_t node_count;
    size_t created;
    size_t updated;
    size_t reused;
    size_t removed;
    size_t drawn;
    size_t capability_gaps;
};

struct bsg_backend_scene;

struct bsg_backend_scene_update_result {
    int rendered;
    struct bsg_backend_scene *scene;
    struct bsg_backend_scene_stats stats;
    unsigned int capabilities;
    size_t capability_gaps;
};
typedef int (*bsg_backend_scene_node_cb)(const struct bsg_backend_scene_node *node,
					 void *userdata);

BSG_EXPORT extern struct bsg_backend_scene *
bsg_backend_scene_create(void);

BSG_EXPORT extern struct bsg_backend_scene *
bsg_backend_scene_create_with_capabilities(unsigned int capabilities);

BSG_EXPORT extern void
bsg_backend_scene_destroy(struct bsg_backend_scene *scene);

BSG_EXPORT extern struct bsg_backend_adapter *
bsg_backend_scene_adapter(struct bsg_backend_scene *scene);

BSG_EXPORT extern struct bsg_backend_adapter *
bsg_backend_scene_select_adapter(bsg_backend_scene_adapter_kind kind,
				 struct bsg_backend_scene *scene,
				 struct bsg_backend_adapter *external,
				 struct bu_vls *diagnostics);

BSG_EXPORT extern const struct bsg_backend_scene_node *
bsg_backend_scene_find(const struct bsg_backend_scene *scene,
		       uint64_t cache_identity);

BSG_EXPORT extern int
bsg_backend_scene_foreach_node(const struct bsg_backend_scene *scene,
			       bsg_backend_scene_node_cb cb,
			       void *userdata);

BSG_EXPORT extern const struct bsg_backend_scene_camera *
bsg_backend_scene_get_camera(const struct bsg_backend_scene *scene);

BSG_EXPORT extern const struct bsg_backend_scene_clip *
bsg_backend_scene_get_clip(const struct bsg_backend_scene *scene);

BSG_EXPORT extern const struct bsg_backend_scene_lights *
bsg_backend_scene_get_lights(const struct bsg_backend_scene *scene);

BSG_EXPORT extern size_t
bsg_backend_scene_count(const struct bsg_backend_scene *scene);

BSG_EXPORT extern void
bsg_backend_scene_get_stats(const struct bsg_backend_scene *scene,
			    struct bsg_backend_scene_stats *stats);

BSG_EXPORT extern void
bsg_backend_scene_invalidate_item(struct bsg_backend_scene *scene,
				  const struct bsg_render_item *item,
				  unsigned int reason_mask);

BSG_EXPORT extern void
bsg_backend_scene_release_source(struct bsg_backend_scene *scene,
				 uint64_t source_identity);

BSG_EXPORT extern unsigned int
bsg_backend_scene_capabilities(const struct bsg_backend_scene *scene);

BSG_EXPORT extern void
bsg_backend_scene_set_capabilities(struct bsg_backend_scene *scene,
				   unsigned int capabilities);

BSG_EXPORT extern int
bsg_backend_scene_render_request(struct bsg_view *view,
				 struct bsg_backend_scene *scene,
				 unsigned int flags);

BSG_EXPORT extern int
bsg_backend_scene_update(struct bsg_backend_scene *scene,
			 struct bsg_view *view,
			 unsigned int flags,
			 struct bsg_backend_scene_update_result *result,
			 struct bu_vls *diagnostics);

BSG_EXPORT extern int
bsg_backend_scene_compare_items(const struct bsg_backend_scene *scene,
				const struct bu_ptbl *items,
				struct bu_vls *report);

BSG_EXPORT extern void
bsg_backend_scene_diagnostics(const struct bsg_backend_scene *scene,
			      struct bu_vls *out);

__END_DECLS

#endif /* BSG_BACKEND_SCENE_H */

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
