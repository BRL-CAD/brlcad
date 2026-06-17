/*                          N O D E . H
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
 * Public opaque typed node references.
 *
 * Node refs are thin typed handles over BSG object refs.  A typed ref is null
 * when its object member is null.  Constructors return owning refs; release
 * owning refs with bsg_node_ref_destroy() or the matching typed destroy helper.
 * View-scene accessors return borrowed refs tied to the view attachment; do
 * not release borrowed view-scene refs with bsg_node_ref_destroy().
 *
 * Hierarchy append APIs store a parent-child relationship but do not transfer
 * ownership of the child ref.  Destroying a parent detaches independently
 * owned children instead of destroying them.
 */
/** @{ */
/* @file bsg/node.h */

#ifndef BSG_NODE_H
#define BSG_NODE_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/object.h"

__BEGIN_DECLS

typedef struct bsg_node_ref { bsg_object_ref object; } bsg_node_ref;
typedef struct bsg_group_ref { bsg_node_ref node; } bsg_group_ref;
typedef struct bsg_separator_ref { bsg_group_ref group; } bsg_separator_ref;
typedef struct bsg_switch_ref { bsg_group_ref group; } bsg_switch_ref;
typedef struct bsg_lod_ref { bsg_node_ref node; } bsg_lod_ref;
typedef struct bsg_transform_ref { bsg_node_ref node; } bsg_transform_ref;
typedef struct bsg_material_ref { bsg_node_ref node; } bsg_material_ref;
typedef struct bsg_draw_style_ref { bsg_node_ref node; } bsg_draw_style_ref;
typedef struct bsg_complexity_ref { bsg_node_ref node; } bsg_complexity_ref;
typedef struct bsg_camera_ref { bsg_node_ref node; } bsg_camera_ref;
typedef struct bsg_light_ref { bsg_node_ref node; } bsg_light_ref;
typedef struct bsg_shape_ref { bsg_node_ref node; } bsg_shape_ref;
typedef struct bsg_geometry_ref { bsg_shape_ref shape; } bsg_geometry_ref;
typedef struct bsg_database_source_ref { bsg_node_ref node; } bsg_database_source_ref;

#define BSG_NODE_REF_NULL_INIT { BSG_OBJECT_REF_NULL_INIT }
#define BSG_GROUP_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_SEPARATOR_REF_NULL_INIT { BSG_GROUP_REF_NULL_INIT }
#define BSG_SWITCH_REF_NULL_INIT { BSG_GROUP_REF_NULL_INIT }
#define BSG_LOD_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_TRANSFORM_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_MATERIAL_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_DRAW_STYLE_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_COMPLEXITY_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_CAMERA_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_LIGHT_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_REF_SHAPE_NULL_INIT { BSG_NODE_REF_NULL_INIT }
#define BSG_GEOMETRY_REF_NULL_INIT { BSG_REF_SHAPE_NULL_INIT }
#define BSG_DATABASE_SOURCE_REF_NULL_INIT { BSG_NODE_REF_NULL_INIT }

BSG_EXPORT extern bsg_node_ref bsg_node_ref_null(void);
BSG_EXPORT extern int bsg_node_ref_is_null(bsg_node_ref ref);
BSG_EXPORT extern int bsg_node_ref_equal(bsg_node_ref a, bsg_node_ref b);
BSG_EXPORT extern bsg_object_ref bsg_node_ref_object(bsg_node_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_node_ref_from_object(bsg_object_ref object);
BSG_EXPORT extern bsg_type_id bsg_node_ref_type(bsg_node_ref ref);
BSG_EXPORT extern int bsg_node_is_a(bsg_node_ref ref, bsg_type_id type);
BSG_EXPORT extern void bsg_node_ref_destroy(bsg_node_ref ref);

BSG_EXPORT extern void bsg_node_ref_set_name(bsg_node_ref ref, const char *name);
BSG_EXPORT extern const char *bsg_node_ref_name(bsg_node_ref ref);
BSG_EXPORT extern void bsg_node_ref_set_visible(bsg_node_ref ref, int visible);
BSG_EXPORT extern int bsg_node_ref_visible(bsg_node_ref ref);
BSG_EXPORT extern void bsg_node_ref_set_color(bsg_node_ref ref, unsigned char r, unsigned char g, unsigned char b);
BSG_EXPORT extern void bsg_node_ref_color(bsg_node_ref ref, unsigned char *r, unsigned char *g, unsigned char *b);
BSG_EXPORT extern void bsg_node_ref_set_transform(bsg_node_ref ref, const mat_t mat);
BSG_EXPORT extern void bsg_node_ref_transform(bsg_node_ref ref, mat_t mat);
BSG_EXPORT extern void bsg_node_ref_set_bounds(bsg_node_ref ref, const point_t bmin, const point_t bmax, int valid);
BSG_EXPORT extern int bsg_node_ref_bounds(bsg_node_ref ref, point_t bmin, point_t bmax);
BSG_EXPORT extern bsg_node_ref bsg_node_ref_parent(bsg_node_ref ref);

BSG_EXPORT extern bsg_group_ref bsg_group_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_separator_ref bsg_separator_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_separator_ref bsg_view_scene_separator_ref(struct bsg_view *v, int create);
BSG_EXPORT extern bsg_switch_ref bsg_switch_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_lod_ref bsg_lod_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_transform_ref bsg_transform_ref_create(struct bsg_view *v, const mat_t mat, const char *name);
BSG_EXPORT extern bsg_material_ref bsg_material_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_draw_style_ref bsg_draw_style_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_complexity_ref bsg_complexity_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_camera_ref bsg_camera_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_light_ref bsg_light_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_shape_ref bsg_shape_ref_create(struct bsg_view *v, const char *name);
BSG_EXPORT extern bsg_geometry_ref bsg_geometry_ref_create(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_node_ref bsg_group_ref_as_node(bsg_group_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_separator_ref_as_node(bsg_separator_ref ref);
BSG_EXPORT extern bsg_group_ref bsg_separator_ref_as_group(bsg_separator_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_switch_ref_as_node(bsg_switch_ref ref);
BSG_EXPORT extern bsg_group_ref bsg_switch_ref_as_group(bsg_switch_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_lod_ref_as_node(bsg_lod_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_transform_ref_as_node(bsg_transform_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_material_ref_as_node(bsg_material_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_draw_style_ref_as_node(bsg_draw_style_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_complexity_ref_as_node(bsg_complexity_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_camera_ref_as_node(bsg_camera_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_light_ref_as_node(bsg_light_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_shape_ref_as_node(bsg_shape_ref ref);
BSG_EXPORT extern bsg_shape_ref bsg_geometry_ref_as_shape(bsg_geometry_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_geometry_ref_as_node(bsg_geometry_ref ref);

BSG_EXPORT extern bsg_group_ref bsg_node_ref_as_group(bsg_node_ref ref);
BSG_EXPORT extern bsg_separator_ref bsg_node_ref_as_separator(bsg_node_ref ref);
BSG_EXPORT extern bsg_switch_ref bsg_node_ref_as_switch(bsg_node_ref ref);
BSG_EXPORT extern bsg_lod_ref bsg_node_ref_as_lod(bsg_node_ref ref);
BSG_EXPORT extern bsg_transform_ref bsg_node_ref_as_transform(bsg_node_ref ref);
BSG_EXPORT extern bsg_material_ref bsg_node_ref_as_material(bsg_node_ref ref);
BSG_EXPORT extern bsg_draw_style_ref bsg_node_ref_as_draw_style(bsg_node_ref ref);
BSG_EXPORT extern bsg_complexity_ref bsg_node_ref_as_complexity(bsg_node_ref ref);
BSG_EXPORT extern bsg_camera_ref bsg_node_ref_as_camera(bsg_node_ref ref);
BSG_EXPORT extern bsg_light_ref bsg_node_ref_as_light(bsg_node_ref ref);
BSG_EXPORT extern bsg_shape_ref bsg_node_ref_as_shape(bsg_node_ref ref);
BSG_EXPORT extern bsg_geometry_ref bsg_node_ref_as_geometry(bsg_node_ref ref);

__END_DECLS

#endif /* BSG_NODE_H */

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
