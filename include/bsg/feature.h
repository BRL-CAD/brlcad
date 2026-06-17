/*                     F E A T U R E . H
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
 * Typed view features for user-facing, non-database drawing.
 */
/** @{ */
/* @file bsg/feature.h */

#ifndef BSG_FEATURE_H
#define BSG_FEATURE_H

#include "common.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/edit_preview.h"
#include "bsg/overlay.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

enum bsg_feature_family {
    BSG_FEATURE_UNKNOWN = 0,
    BSG_FEATURE_ANNOTATION,
    BSG_FEATURE_TEXT,
    BSG_FEATURE_LABEL,
    BSG_FEATURE_LABEL_LEADER,
    BSG_FEATURE_AXES,
    BSG_FEATURE_GRID,
    BSG_FEATURE_LINES,
    BSG_FEATURE_ARROW,
    BSG_FEATURE_POLYGON,
    BSG_FEATURE_SKETCH,
    BSG_FEATURE_MEASUREMENT,
    BSG_FEATURE_SNAP_HINT,
    BSG_FEATURE_EDIT_HANDLE,
    BSG_FEATURE_TRANSIENT_PREVIEW,
    BSG_FEATURE_HUD,
    BSG_FEATURE_FACEPLATE,
    BSG_FEATURE_OVERLAY,
    BSG_FEATURE_FACE_SET
};

enum bsg_feature_scope {
    BSG_FEATURE_SCOPE_SHARED = 1,
    BSG_FEATURE_SCOPE_LOCAL = 2,
    BSG_FEATURE_SCOPE_ALL = BSG_FEATURE_SCOPE_SHARED | BSG_FEATURE_SCOPE_LOCAL
};

typedef struct bsg_feature_ref {
    uintptr_t token;
    uint64_t revision;
} bsg_feature_ref;

#define BSG_FEATURE_REF_NULL_INIT {0, 0}

struct bsg_line_layer_builder;
struct bsg_line_layer;

struct bsg_feature_opts {
    enum bsg_feature_family family;
    int local;
    int arrow;
    int visible;
    int color_valid;
    unsigned char color[3];
    int line_width;
    const char *display_name;
    const char *owner;
};

#define BSG_FEATURE_OPTS_INIT { BSG_FEATURE_UNKNOWN, 0, 0, -1, 0, {0, 0, 0}, -1, NULL, NULL }

struct bsg_feature_record {
    bsg_feature_ref ref;
    enum bsg_feature_family family;
    enum bsg_feature_scope scope;
    const char *name;
    const char *display_name;
    const char *owner;
    int visible;
    unsigned char color[3];
    int line_width;
    size_t child_count;
    size_t geometry_command_count;
};

struct bsg_feature_style {
    int visible;
    int color_valid;
    unsigned char color[3];
    int line_width;
    int line_style;
    int arrow;
    fastf_t arrow_tip_length;
    fastf_t arrow_tip_width;
};

#define BSG_FEATURE_STYLE_INIT { -1, 0, {0, 0, 0}, -1, -1, -1, -1.0, -1.0 }

struct bsg_feature_line_layer {
    const char *name;
    const point_t *points;
    const int *commands;
    size_t point_count;
    struct bsg_feature_style style;
};

#define BSG_FEATURE_LINE_LAYER_INIT { NULL, NULL, NULL, 0, BSG_FEATURE_STYLE_INIT }

BSG_EXPORT extern struct bsg_line_layer_builder *
bsg_line_layer_builder_create(void);

BSG_EXPORT extern void
bsg_line_layer_builder_clear(struct bsg_line_layer_builder *builder);

BSG_EXPORT extern void
bsg_line_layer_builder_free(struct bsg_line_layer_builder *builder);

BSG_EXPORT extern size_t
bsg_line_layer_builder_layer_count(const struct bsg_line_layer_builder *builder);

BSG_EXPORT extern size_t
bsg_line_layer_builder_point_count(const struct bsg_line_layer_builder *builder);

BSG_EXPORT extern const struct bsg_line_layer *
bsg_line_layer_builder_layer_at(const struct bsg_line_layer_builder *builder,
				size_t idx);

BSG_EXPORT extern int
bsg_line_layer_builder_add(struct bsg_line_layer_builder *builder,
			   int r,
			   int g,
			   int b,
			   const point_t point,
			   int command);

BSG_EXPORT extern struct bsg_line_layer *
bsg_line_layer_builder_find(struct bsg_line_layer_builder *builder,
			    int r,
			    int g,
			    int b);

BSG_EXPORT extern int
bsg_line_layer_add(struct bsg_line_layer *layer,
		   const point_t point,
		   int command);

BSG_EXPORT extern int
bsg_line_layer_color(const struct bsg_line_layer *layer,
		     unsigned char *r,
		     unsigned char *g,
		     unsigned char *b);

BSG_EXPORT extern size_t
bsg_line_layer_point_count(const struct bsg_line_layer *layer);

BSG_EXPORT extern int
bsg_line_layer_point_at(const struct bsg_line_layer *layer,
			size_t idx,
			point_t point);

BSG_EXPORT extern int
bsg_line_layer_command_at(const struct bsg_line_layer *layer,
			  size_t idx,
			  int *command);

struct bsg_feature_label_data {
    const char *text;
    point_t point;
    int color_valid;
    unsigned char color[3];
    int line_flag;
    point_t target;
    int anchor;
    int arrow;
};

#define BSG_FEATURE_LABEL_DATA_INIT { NULL, VINIT_ZERO, 0, {0, 0, 0}, 0, VINIT_ZERO, BSG_ANCHOR_AUTO, 0 }

typedef int (*bsg_feature_visit_cb)(bsg_feature_ref ref, const struct bsg_feature_record *record, void *data);

BSG_EXPORT extern const char *
bsg_feature_family_name(enum bsg_feature_family family);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create(struct bsg_view *v, const char *name, const struct bsg_feature_opts *opts);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_axes(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_lines(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_label(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_arrow(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_overlay(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_polygon(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_face_set(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_create_preview(struct bsg_view *v, const char *name, int local);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_find(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_ref_from_scene(bsg_scene_ref ref);

BSG_EXPORT extern bsg_scene_ref
bsg_feature_ref_as_scene(bsg_feature_ref ref);

BSG_EXPORT extern int
bsg_feature_remove(struct bsg_view *v, const char *name);

BSG_EXPORT extern size_t
bsg_feature_remove_all(struct bsg_view *v, int scope_mask);

BSG_EXPORT extern void
bsg_feature_visit(struct bsg_view *v, int scope_mask, bsg_feature_visit_cb cb, void *data);

BSG_EXPORT extern int
bsg_feature_record_get(bsg_feature_ref ref, struct bsg_feature_record *record);

BSG_EXPORT extern int
bsg_feature_ref_is_null(bsg_feature_ref ref);

BSG_EXPORT extern enum bsg_feature_family
bsg_feature_family(bsg_feature_ref ref);

BSG_EXPORT extern void
bsg_feature_set_family(bsg_feature_ref ref, enum bsg_feature_family family);

BSG_EXPORT extern int
bsg_feature_style_get(bsg_feature_ref ref, struct bsg_feature_style *style);

BSG_EXPORT extern int
bsg_feature_style_apply(bsg_feature_ref ref, const struct bsg_feature_style *style);

BSG_EXPORT extern int
bsg_feature_style_apply_recursive(bsg_feature_ref ref,
				  const struct bsg_feature_style *style);

BSG_EXPORT extern void
bsg_feature_set_color(bsg_feature_ref ref, int r, int g, int b);

BSG_EXPORT extern void
bsg_feature_set_line_width(bsg_feature_ref ref, int line_width);

BSG_EXPORT extern void
bsg_feature_set_visible(bsg_feature_ref ref, int visible);

BSG_EXPORT extern int
bsg_feature_touch(bsg_feature_ref ref);

BSG_EXPORT extern int
bsg_feature_realize(bsg_feature_ref ref, struct bsg_view *v, int recursive);

BSG_EXPORT extern int
bsg_feature_edit_preview_attach(bsg_feature_ref ref,
				void *editor_ctx,
				void *aux_ctx,
				const struct bsg_edit_preview_ops *ops);

BSG_EXPORT extern int
bsg_feature_edit_preview_set_ops(bsg_feature_ref ref,
				 const struct bsg_edit_preview_ops *ops);

BSG_EXPORT extern int
bsg_feature_edit_preview_touch(bsg_feature_ref ref);

BSG_EXPORT extern uint64_t
bsg_feature_edit_preview_revision(bsg_feature_ref ref);

BSG_EXPORT extern void
bsg_feature_edit_preview_mark_source_revision(bsg_feature_ref ref,
					      uint64_t source_revision,
					      bsg_edit_preview_stale_reason reason);

BSG_EXPORT extern void
bsg_feature_edit_preview_mark_inputs_revision(bsg_feature_ref ref,
					      uint64_t inputs_revision,
					      bsg_edit_preview_stale_reason reason);

BSG_EXPORT extern int
bsg_feature_edit_preview_is_stale(bsg_feature_ref ref);

BSG_EXPORT extern bsg_edit_preview_stale_reason
bsg_feature_edit_preview_stale_reason(bsg_feature_ref ref);

BSG_EXPORT extern int
bsg_feature_edit_preview_realize(bsg_feature_ref ref, struct bsg_view *v);

BSG_EXPORT extern int
bsg_feature_edit_preview_bounds(bsg_feature_ref ref, point_t *bmin, point_t *bmax);

BSG_EXPORT extern int
bsg_feature_edit_preview_pick(bsg_feature_ref ref, struct bsg_view *v,
			      int x, int y, void *pick_out);

BSG_EXPORT extern int
bsg_feature_edit_preview_snap(bsg_feature_ref ref, struct bsg_view *v,
			      const point_t sample_pt, point_t out_pt);

BSG_EXPORT extern fastf_t
bsg_feature_view_depth(bsg_feature_ref ref, struct bsg_view *v, int mode);

BSG_EXPORT extern int
bsg_feature_points_replace(bsg_feature_ref ref,
			   enum bsg_feature_family family,
			   const point_t *points,
			   const int *cmds,
			   size_t point_count);

BSG_EXPORT extern int
bsg_feature_points_copy(bsg_feature_ref ref,
			point_t **points_out,
			int **cmds_out,
			size_t *point_count_out);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_lines(struct bsg_view *v,
			  const char *name,
			  int local,
			  const point_t *points,
			  size_t point_count,
			  const struct bsg_feature_style *style);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_line_layers(struct bsg_view *v,
				const char *name,
				int local,
				const struct bsg_feature_line_layer *layers,
				size_t layer_count,
				const struct bsg_feature_style *style);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_line_layer_builder(struct bsg_view *v,
				       const char *name,
				       int local,
				       const struct bsg_line_layer_builder *builder,
				       const struct bsg_feature_style *style);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_indexed_face_set(struct bsg_view *v,
				     const char *name,
				     int local,
				     const point_t *points,
				     size_t point_count,
				     const vect_t *normals,
				     size_t normal_count,
				     const int *indices,
				     size_t index_count,
				     const struct bsg_feature_style *style);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_arrow(struct bsg_view *v,
			  const char *name,
			  int local,
			  const point_t *points,
			  size_t point_count,
			  const struct bsg_feature_style *style);

BSG_EXPORT extern bsg_feature_ref
bsg_feature_replace_axes(struct bsg_view *v,
			 const char *name,
			 int local,
			 const point_t *centers,
			 size_t center_count,
			 fastf_t half_axes_size,
			 const struct bsg_feature_style *style);

BSG_EXPORT extern int
bsg_feature_axes_state_get(bsg_feature_ref ref, struct bsg_axes *axes_out);

BSG_EXPORT extern int
bsg_feature_axes_state_replace(bsg_feature_ref ref, const struct bsg_axes *axes);

BSG_EXPORT extern int
bsg_feature_axes_centers_copy(bsg_feature_ref ref,
			      point_t **centers_out,
			      size_t *center_count_out);

BSG_EXPORT extern int
bsg_feature_arrow_tip_set(bsg_feature_ref ref, fastf_t tip_length, fastf_t tip_width);

BSG_EXPORT extern int
bsg_feature_arrow_tip_get(bsg_feature_ref ref, fastf_t *tip_length, fastf_t *tip_width);

BSG_EXPORT extern int
bsg_feature_labels_replace(bsg_feature_ref ref,
			   const struct bsg_feature_label_data *labels,
			   size_t label_count);

BSG_EXPORT extern size_t
bsg_feature_label_count(bsg_feature_ref ref);

BSG_EXPORT extern int
bsg_feature_label_copy(bsg_feature_ref ref,
		       size_t index,
		       struct bu_vls *text_out,
		       point_t point_out,
		       unsigned char color_out[3]);

BSG_EXPORT extern int
bsg_feature_label_point_set(bsg_feature_ref ref, size_t index, const point_t point);

BSG_EXPORT extern int
bsg_feature_labels_color_set(bsg_feature_ref ref, const unsigned char color[3]);

BSG_EXPORT extern void
bsg_feature_set_view(bsg_feature_ref ref, struct bsg_view *v);

BSG_EXPORT extern int
bsg_feature_overlay_register_owner(bsg_feature_ref ref,
				   const void *owner,
				   bsg_overlay_role role,
				   bsg_overlay_class overlay_class,
				   bsg_overlay_lifecycle lifecycle,
				   bsg_overlay_order ordering,
				   const char *source_path,
				   int sort_order);

BSG_EXPORT extern size_t
bsg_feature_overlay_auto_remove(struct bsg_view *v, const char *source_path);

BSG_EXPORT extern void
bsg_feature_labels_sync(struct bsg_view *v, struct bsg_data_label_state *gdlsp, const char *name);

__END_DECLS

#endif /* BSG_FEATURE_H */

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
