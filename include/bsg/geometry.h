/*                      G E O M E T R Y . H
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @addtogroup libbsg
 *
 * @brief
 * Public typed geometry nodes.
 *
 * Geometry node refs are shape refs whose drawable data is expressed as
 * fields.  Backends may still receive private realization data while the
 * drawing stack migrates, but public authoring should use these typed refs
 * and field accessors.
 */
/** @{ */
/* @file bsg/geometry.h */

#ifndef BSG_GEOMETRY_H
#define BSG_GEOMETRY_H

#include "common.h"
#include <stddef.h>
#include <stdint.h>
#include "vmath.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/annotation.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

typedef enum bsg_geometry_node_kind {
    BSG_GEOMETRY_NODE_NONE = 0,
    BSG_GEOMETRY_NODE_LINE_SET,
    BSG_GEOMETRY_NODE_INDEXED_LINE_SET,
    BSG_GEOMETRY_NODE_POINT_SET,
    BSG_GEOMETRY_NODE_INDEXED_FACE_SET,
    BSG_GEOMETRY_NODE_MESH,
    BSG_GEOMETRY_NODE_TEXT,
    BSG_GEOMETRY_NODE_IMAGE,
    BSG_GEOMETRY_NODE_FRAMEBUFFER,
    BSG_GEOMETRY_NODE_OVERLAY,
    BSG_GEOMETRY_NODE_HUD,
    BSG_GEOMETRY_NODE_ANNOTATION,
    BSG_GEOMETRY_NODE_CSG_PROXY,
    BSG_GEOMETRY_NODE_BREP_PROXY,
    BSG_GEOMETRY_NODE_EDIT_PREVIEW
} bsg_geometry_node_kind;

typedef enum bsg_geometry_line_command {
    BSG_GEOMETRY_LINE_MOVE = 0,
    BSG_GEOMETRY_LINE_DRAW = 1,
    BSG_GEOMETRY_POINT_DRAW = 12
} bsg_geometry_line_command;

typedef struct bsg_line_set_ref { bsg_geometry_ref geometry; } bsg_line_set_ref;
typedef struct bsg_indexed_line_set_ref { bsg_geometry_ref geometry; } bsg_indexed_line_set_ref;
typedef struct bsg_point_set_ref { bsg_geometry_ref geometry; } bsg_point_set_ref;
typedef struct bsg_indexed_face_set_ref { bsg_geometry_ref geometry; } bsg_indexed_face_set_ref;
typedef struct bsg_mesh_ref { bsg_geometry_ref geometry; } bsg_mesh_ref;
typedef struct bsg_text_ref { bsg_geometry_ref geometry; } bsg_text_ref;
typedef struct bsg_image_ref { bsg_geometry_ref geometry; } bsg_image_ref;
typedef struct bsg_framebuffer_ref { bsg_geometry_ref geometry; } bsg_framebuffer_ref;
typedef struct bsg_overlay_geometry_ref { bsg_geometry_ref geometry; } bsg_overlay_geometry_ref;
typedef struct bsg_hud_geometry_ref { bsg_geometry_ref geometry; } bsg_hud_geometry_ref;
typedef struct bsg_annotation_ref { bsg_geometry_ref geometry; } bsg_annotation_ref;
typedef struct bsg_csg_proxy_ref { bsg_geometry_ref geometry; } bsg_csg_proxy_ref;
typedef struct bsg_brep_proxy_ref { bsg_geometry_ref geometry; } bsg_brep_proxy_ref;
typedef struct bsg_edit_preview_ref { bsg_geometry_ref geometry; } bsg_edit_preview_ref;

#define BSG_LINE_SET_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_INDEXED_LINE_SET_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_POINT_SET_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_INDEXED_FACE_SET_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_MESH_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_TEXT_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_IMAGE_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_FRAMEBUFFER_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_OVERLAY_GEOMETRY_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_HUD_GEOMETRY_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_ANNOTATION_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_CSG_PROXY_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_BREP_PROXY_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }
#define BSG_EDIT_PREVIEW_REF_NULL_INIT { BSG_GEOMETRY_REF_NULL_INIT }

BSG_EXPORT extern bsg_type_id bsg_line_set_type(void);
BSG_EXPORT extern bsg_type_id bsg_indexed_line_set_type(void);
BSG_EXPORT extern bsg_type_id bsg_point_set_type(void);
BSG_EXPORT extern bsg_type_id bsg_indexed_face_set_type(void);
BSG_EXPORT extern bsg_type_id bsg_mesh_type(void);
BSG_EXPORT extern bsg_type_id bsg_text_type(void);
BSG_EXPORT extern bsg_type_id bsg_image_type(void);
BSG_EXPORT extern bsg_type_id bsg_framebuffer_type(void);
BSG_EXPORT extern bsg_type_id bsg_overlay_geometry_type(void);
BSG_EXPORT extern bsg_type_id bsg_hud_geometry_type(void);
BSG_EXPORT extern bsg_type_id bsg_annotation_type(void);
BSG_EXPORT extern bsg_type_id bsg_csg_proxy_type(void);
BSG_EXPORT extern bsg_type_id bsg_brep_proxy_type(void);
BSG_EXPORT extern bsg_type_id bsg_edit_preview_type(void);

BSG_EXPORT extern bsg_line_set_ref bsg_line_set_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_indexed_line_set_ref bsg_indexed_line_set_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_point_set_ref bsg_point_set_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_indexed_face_set_ref bsg_indexed_face_set_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_mesh_ref bsg_mesh_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_text_ref bsg_text_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_image_ref bsg_image_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_framebuffer_ref bsg_framebuffer_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_overlay_geometry_ref bsg_overlay_geometry_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_hud_geometry_ref bsg_hud_geometry_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_annotation_ref bsg_annotation_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_csg_proxy_ref bsg_csg_proxy_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_brep_proxy_ref bsg_brep_proxy_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_edit_preview_ref bsg_edit_preview_ref_create(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_node_ref bsg_line_set_ref_as_node(bsg_line_set_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_indexed_line_set_ref_as_node(bsg_indexed_line_set_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_point_set_ref_as_node(bsg_point_set_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_indexed_face_set_ref_as_node(bsg_indexed_face_set_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_mesh_ref_as_node(bsg_mesh_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_text_ref_as_node(bsg_text_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_image_ref_as_node(bsg_image_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_framebuffer_ref_as_node(bsg_framebuffer_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_overlay_geometry_ref_as_node(bsg_overlay_geometry_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_hud_geometry_ref_as_node(bsg_hud_geometry_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_annotation_ref_as_node(bsg_annotation_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_csg_proxy_ref_as_node(bsg_csg_proxy_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_brep_proxy_ref_as_node(bsg_brep_proxy_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_edit_preview_ref_as_node(bsg_edit_preview_ref ref);

BSG_EXPORT extern bsg_geometry_ref bsg_line_set_ref_as_geometry(bsg_line_set_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_indexed_line_set_ref_as_geometry(bsg_indexed_line_set_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_point_set_ref_as_geometry(bsg_point_set_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_indexed_face_set_ref_as_geometry(bsg_indexed_face_set_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_mesh_ref_as_geometry(bsg_mesh_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_text_ref_as_geometry(bsg_text_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_image_ref_as_geometry(bsg_image_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_framebuffer_ref_as_geometry(bsg_framebuffer_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_overlay_geometry_ref_as_geometry(bsg_overlay_geometry_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_hud_geometry_ref_as_geometry(bsg_hud_geometry_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_annotation_ref_as_geometry(bsg_annotation_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_csg_proxy_ref_as_geometry(bsg_csg_proxy_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_brep_proxy_ref_as_geometry(bsg_brep_proxy_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_edit_preview_ref_as_geometry(bsg_edit_preview_ref ref);

BSG_EXPORT extern bsg_line_set_ref bsg_node_ref_as_line_set(bsg_node_ref ref);
BSG_EXPORT extern bsg_indexed_line_set_ref bsg_node_ref_as_indexed_line_set(bsg_node_ref ref);
BSG_EXPORT extern bsg_point_set_ref bsg_node_ref_as_point_set(bsg_node_ref ref);
BSG_EXPORT extern bsg_indexed_face_set_ref bsg_node_ref_as_indexed_face_set(bsg_node_ref ref);
BSG_EXPORT extern bsg_mesh_ref bsg_node_ref_as_mesh(bsg_node_ref ref);
BSG_EXPORT extern bsg_text_ref bsg_node_ref_as_text(bsg_node_ref ref);
BSG_EXPORT extern bsg_image_ref bsg_node_ref_as_image(bsg_node_ref ref);
BSG_EXPORT extern bsg_framebuffer_ref bsg_node_ref_as_framebuffer(bsg_node_ref ref);
BSG_EXPORT extern bsg_overlay_geometry_ref bsg_node_ref_as_overlay_geometry(bsg_node_ref ref);
BSG_EXPORT extern bsg_hud_geometry_ref bsg_node_ref_as_hud_geometry(bsg_node_ref ref);
BSG_EXPORT extern bsg_annotation_ref bsg_node_ref_as_annotation(bsg_node_ref ref);
BSG_EXPORT extern bsg_csg_proxy_ref bsg_node_ref_as_csg_proxy(bsg_node_ref ref);
BSG_EXPORT extern bsg_brep_proxy_ref bsg_node_ref_as_brep_proxy(bsg_node_ref ref);
BSG_EXPORT extern bsg_edit_preview_ref bsg_node_ref_as_edit_preview(bsg_node_ref ref);

BSG_EXPORT extern bsg_geometry_node_kind bsg_geometry_ref_kind(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_scene_ref_as_geometry(bsg_scene_ref ref);
BSG_EXPORT extern bsg_scene_ref bsg_geometry_ref_as_scene(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_kind_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_coordinates_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_normals_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_colors_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_indices_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_primitive_sets_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_text_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_image_width_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_image_height_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_image_channels_field(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_field_ref bsg_geometry_ref_revision_field(bsg_geometry_ref ref);
BSG_EXPORT extern uint64_t bsg_geometry_ref_revision(bsg_geometry_ref ref);

BSG_EXPORT extern int bsg_geometry_ref_clear(bsg_geometry_ref ref);
BSG_EXPORT extern int bsg_geometry_ref_set_line_set(bsg_geometry_ref ref,
	const point_t *points, const int *commands, size_t point_count);
BSG_EXPORT extern int bsg_geometry_ref_set_point_set(bsg_geometry_ref ref,
	const point_t *points, size_t point_count);
BSG_EXPORT extern int bsg_line_set_ref_set_points(bsg_line_set_ref ref,
	const point_t *points, const int *commands, size_t point_count);
BSG_EXPORT extern size_t bsg_line_set_ref_point_count(bsg_line_set_ref ref);
BSG_EXPORT extern int bsg_line_set_ref_point_at(bsg_line_set_ref ref, size_t idx, point_t value);
BSG_EXPORT extern int bsg_line_set_ref_command_at(bsg_line_set_ref ref, size_t idx, int *command);
BSG_EXPORT extern int bsg_point_set_ref_set_points(bsg_point_set_ref ref,
	const point_t *points, size_t point_count);
/*
 * Indexed line sets use a polyline stream in @p indices: non-negative values
 * index @p points, and -1 terminates the current line.  A trailing -1 is
 * optional, every non-empty line must contain at least two vertices, and other
 * negative or out-of-range indices are rejected.
 */
BSG_EXPORT extern int bsg_indexed_line_set_ref_set_geometry(bsg_indexed_line_set_ref ref,
	const point_t *points, size_t point_count, const int *indices, size_t index_count);
/*
 * Indexed face sets use a polygon stream in @p indices: non-negative values
 * index @p points, and -1 terminates the current face.  A trailing -1 is
 * optional, every face must contain at least three vertices, and normals are
 * optional.  When supplied, normals must be per-index-entry, per-vertex, or
 * per-face.  Per-index-entry normals correspond only to non-negative vertex
 * index entries; -1 face separators do not have normals.
 */
BSG_EXPORT extern int bsg_indexed_face_set_ref_set_geometry(bsg_indexed_face_set_ref ref,
	const point_t *points, size_t point_count, const int *indices, size_t index_count);
BSG_EXPORT extern int bsg_geometry_ref_set_indexed_face_set(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count);
/* Replace indexed face fields while preserving the existing geometry object. */
BSG_EXPORT extern int bsg_geometry_ref_update_indexed_face_set(bsg_geometry_ref ref,
	const point_t *points, size_t point_count,
	const vect_t *normals, size_t normal_count,
	const int *indices, size_t index_count);
BSG_EXPORT extern int bsg_mesh_ref_set_lod(bsg_mesh_ref ref,
	struct bsg_mesh_lod *mesh);
BSG_EXPORT extern int bsg_text_ref_set_text(bsg_text_ref ref,
	const char *text, const point_t position, int size);
BSG_EXPORT extern int bsg_hud_geometry_ref_set_text(bsg_hud_geometry_ref ref,
	const char *text, fastf_t x, fastf_t y, int size);
BSG_EXPORT extern int bsg_annotation_ref_set_record(bsg_annotation_ref ref,
	const char *summary,
	bsg_annotation_space space,
	const point_t anchor,
	const mat_t model_mat,
	const mat_t display_mat,
	const point_t *points,
	size_t point_count,
	const struct bsg_annotation_segment *segments,
	size_t segment_count);
BSG_EXPORT extern int bsg_geometry_ref_set_annotation(bsg_geometry_ref ref,
	const char *summary,
	bsg_annotation_space space,
	const point_t anchor,
	const mat_t model_mat,
	const mat_t display_mat,
	const point_t *points,
	size_t point_count,
	const struct bsg_annotation_segment *segments,
	size_t segment_count);
BSG_EXPORT extern int bsg_image_ref_set_metadata(bsg_image_ref ref,
	size_t width, size_t height, size_t channels);
BSG_EXPORT extern int bsg_framebuffer_ref_set_mode(bsg_framebuffer_ref ref, int mode);

__END_DECLS

#endif /* BSG_GEOMETRY_H */

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
