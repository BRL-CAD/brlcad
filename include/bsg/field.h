/*                       F I E L D . H
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
 * Public opaque field API (SoField analogue).
 *
 * Field refs are borrowed handles to field storage owned by their node.
 * They remain valid until the owning node is destroyed or reset.  Field
 * connections are non-owning links; callers are responsible for ensuring a
 * connected source field outlives any destination fields connected to it.
 */
/** @{ */
/* @file bsg/field.h */

#ifndef BSG_FIELD_H
#define BSG_FIELD_H

#include "common.h"
#include <stdint.h>
#include <stddef.h>
#include "vmath.h"
#include "bsg/node.h"
#include "bsg/object.h"

__BEGIN_DECLS

typedef struct bsg_field_ref {
    void *opaque;
} bsg_field_ref;

#define BSG_FIELD_REF_NULL_INIT { NULL }

typedef enum bsg_field_value_type {
    BSG_FIELD_VALUE_INVALID = 0,
    BSG_FIELD_VALUE_BOOL,
    BSG_FIELD_VALUE_INT,
    BSG_FIELD_VALUE_UINT64,
    BSG_FIELD_VALUE_FLOAT,
    BSG_FIELD_VALUE_DOUBLE,
    BSG_FIELD_VALUE_ENUM,
    BSG_FIELD_VALUE_STRING,
    BSG_FIELD_VALUE_VEC3,
    BSG_FIELD_VALUE_COLOR3,
    BSG_FIELD_VALUE_MATRIX,
    BSG_FIELD_VALUE_BBOX,
    BSG_FIELD_VALUE_OBJECT,
    BSG_FIELD_VALUE_NODE,
    BSG_FIELD_VALUE_MULTI_NODE,
    BSG_FIELD_VALUE_MULTI_DOUBLE,
    BSG_FIELD_VALUE_MULTI_POINT,
    BSG_FIELD_VALUE_MULTI_INT,
    BSG_FIELD_VALUE_MULTI_COLOR3
} bsg_field_value_type;

BSG_EXPORT extern bsg_field_ref bsg_field_ref_null(void);
BSG_EXPORT extern int bsg_field_ref_is_null(bsg_field_ref ref);
BSG_EXPORT extern int bsg_field_ref_equal(bsg_field_ref a, bsg_field_ref b);
BSG_EXPORT extern bsg_node_ref bsg_field_owner(bsg_field_ref ref);
BSG_EXPORT extern const char *bsg_field_name(bsg_field_ref ref);
BSG_EXPORT extern bsg_field_value_type bsg_field_ref_value_type(bsg_field_ref ref);
BSG_EXPORT extern uint64_t bsg_field_revision(bsg_field_ref ref);
BSG_EXPORT extern int bsg_field_touch(bsg_field_ref ref);

BSG_EXPORT extern int bsg_field_connect(bsg_field_ref dst, bsg_field_ref src);
BSG_EXPORT extern int bsg_field_disconnect(bsg_field_ref dst);
BSG_EXPORT extern bsg_field_ref bsg_field_connection(bsg_field_ref ref);

BSG_EXPORT extern bsg_field_ref bsg_node_ref_field(bsg_node_ref node, const char *name);
BSG_EXPORT extern bsg_field_ref bsg_node_ref_name_field(bsg_node_ref node);
BSG_EXPORT extern bsg_field_ref bsg_node_ref_visibility_field(bsg_node_ref node);
BSG_EXPORT extern bsg_field_ref bsg_node_ref_color_field(bsg_node_ref node);
BSG_EXPORT extern bsg_field_ref bsg_node_ref_transform_field(bsg_node_ref node);
BSG_EXPORT extern bsg_field_ref bsg_node_ref_bounds_field(bsg_node_ref node);
BSG_EXPORT extern bsg_field_ref bsg_group_ref_children_field(bsg_group_ref group);
BSG_EXPORT extern bsg_field_ref bsg_switch_ref_which_child_field(bsg_switch_ref sw);

BSG_EXPORT extern int bsg_field_get_bool(bsg_field_ref ref, int *value);
BSG_EXPORT extern int bsg_field_set_bool(bsg_field_ref ref, int value);
BSG_EXPORT extern int bsg_field_get_int(bsg_field_ref ref, int *value);
BSG_EXPORT extern int bsg_field_set_int(bsg_field_ref ref, int value);
BSG_EXPORT extern int bsg_field_get_uint64(bsg_field_ref ref, uint64_t *value);
BSG_EXPORT extern int bsg_field_set_uint64(bsg_field_ref ref, uint64_t value);
BSG_EXPORT extern int bsg_field_get_float(bsg_field_ref ref, float *value);
BSG_EXPORT extern int bsg_field_set_float(bsg_field_ref ref, float value);
BSG_EXPORT extern int bsg_field_get_double(bsg_field_ref ref, double *value);
BSG_EXPORT extern int bsg_field_set_double(bsg_field_ref ref, double value);
BSG_EXPORT extern int bsg_field_get_enum(bsg_field_ref ref, int *value);
BSG_EXPORT extern int bsg_field_set_enum(bsg_field_ref ref, int value);
BSG_EXPORT extern int bsg_field_get_string(bsg_field_ref ref, const char **value);
BSG_EXPORT extern int bsg_field_set_string(bsg_field_ref ref, const char *value);
BSG_EXPORT extern int bsg_field_get_vec3(bsg_field_ref ref, vect_t value);
BSG_EXPORT extern int bsg_field_set_vec3(bsg_field_ref ref, const vect_t value);
BSG_EXPORT extern int bsg_field_get_color3(bsg_field_ref ref, unsigned char color[3]);
BSG_EXPORT extern int bsg_field_set_color3(bsg_field_ref ref, const unsigned char color[3]);
BSG_EXPORT extern int bsg_field_get_matrix(bsg_field_ref ref, mat_t value);
BSG_EXPORT extern int bsg_field_set_matrix(bsg_field_ref ref, const mat_t value);
BSG_EXPORT extern int bsg_field_get_bbox(bsg_field_ref ref, point_t bmin, point_t bmax, int *valid);
BSG_EXPORT extern int bsg_field_set_bbox(bsg_field_ref ref, const point_t bmin, const point_t bmax, int valid);
BSG_EXPORT extern int bsg_field_get_object(bsg_field_ref ref, bsg_object_ref *value);
BSG_EXPORT extern int bsg_field_set_object(bsg_field_ref ref, bsg_object_ref value);
BSG_EXPORT extern int bsg_field_get_node(bsg_field_ref ref, bsg_node_ref *value);
BSG_EXPORT extern int bsg_field_set_node(bsg_field_ref ref, bsg_node_ref value);

BSG_EXPORT extern size_t bsg_field_multi_count(bsg_field_ref ref);
BSG_EXPORT extern int bsg_field_multi_clear(bsg_field_ref ref);
BSG_EXPORT extern int bsg_field_multi_node_at(bsg_field_ref ref, size_t idx, bsg_node_ref *value);
BSG_EXPORT extern int bsg_field_multi_node_append(bsg_field_ref ref, bsg_node_ref value);
BSG_EXPORT extern int bsg_field_multi_node_remove(bsg_field_ref ref, bsg_node_ref value);
BSG_EXPORT extern int bsg_field_multi_double_at(bsg_field_ref ref, size_t idx, double *value);
BSG_EXPORT extern int bsg_field_multi_double_set(bsg_field_ref ref, size_t idx, double value);
BSG_EXPORT extern int bsg_field_multi_double_append(bsg_field_ref ref, double value);
BSG_EXPORT extern int bsg_field_multi_point_at(bsg_field_ref ref, size_t idx, point_t value);
BSG_EXPORT extern int bsg_field_multi_point_set(bsg_field_ref ref, size_t idx, const point_t value);
BSG_EXPORT extern int bsg_field_multi_point_append(bsg_field_ref ref, const point_t value);
BSG_EXPORT extern int bsg_field_multi_int_at(bsg_field_ref ref, size_t idx, int *value);
BSG_EXPORT extern int bsg_field_multi_int_set(bsg_field_ref ref, size_t idx, int value);
BSG_EXPORT extern int bsg_field_multi_int_append(bsg_field_ref ref, int value);
BSG_EXPORT extern int bsg_field_multi_color3_at(bsg_field_ref ref, size_t idx, unsigned char color[3]);
BSG_EXPORT extern int bsg_field_multi_color3_set(bsg_field_ref ref, size_t idx, const unsigned char color[3]);
BSG_EXPORT extern int bsg_field_multi_color3_append(bsg_field_ref ref, const unsigned char color[3]);

__END_DECLS

#endif /* BSG_FIELD_H */

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
