/*                         O B J E C T . H
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
 * Opaque BSG object references and runtime type IDs.
 *
 * Object references are handles, not owned storage.  A function that creates
 * an object documents whether the returned reference owns a graph object; an
 * owning reference is released with bsg_object_destroy().  Destroying a null
 * reference is a no-op.  There is deliberately no public retain/release
 * protocol in this API.
 */
/** @{ */
/* @file bsg/object.h */

#ifndef BSG_OBJECT_H
#define BSG_OBJECT_H

#include "common.h"
#include <stdint.h>
#include "bsg/defines.h"

__BEGIN_DECLS

typedef uint32_t bsg_type_id;

#define BSG_TYPE_INVALID ((bsg_type_id)0)

typedef struct bsg_object_ref {
    void *opaque;
} bsg_object_ref;

#define BSG_OBJECT_REF_NULL_INIT { NULL }

BSG_EXPORT extern bsg_object_ref
bsg_object_ref_null(void);

BSG_EXPORT extern int
bsg_object_ref_is_null(bsg_object_ref object);

BSG_EXPORT extern int
bsg_object_ref_equal(bsg_object_ref a, bsg_object_ref b);

/**
 * Register a process-global runtime type name under an existing parent type.
 *
 * Registration is deterministic and thread-safe.  Re-registering the same
 * name with the same parent returns the existing type ID; using the same name
 * with a different parent fails with BSG_TYPE_INVALID.  Type names are copied
 * into libbsg-owned storage and remain valid for the life of the process.
 */
BSG_EXPORT extern bsg_type_id
bsg_type_register(const char *name, bsg_type_id parent);

BSG_EXPORT extern const char *
bsg_type_name(bsg_type_id type);

BSG_EXPORT extern int
bsg_type_is_a(bsg_type_id type, bsg_type_id base);

BSG_EXPORT extern bsg_type_id
bsg_object_type(bsg_object_ref object);

BSG_EXPORT extern void
bsg_object_destroy(bsg_object_ref object);

BSG_EXPORT extern bsg_type_id
bsg_type_object(void);

BSG_EXPORT extern bsg_type_id
bsg_node_type(void);

BSG_EXPORT extern bsg_type_id
bsg_group_type(void);

BSG_EXPORT extern bsg_type_id
bsg_separator_type(void);

BSG_EXPORT extern bsg_type_id
bsg_switch_type(void);

BSG_EXPORT extern bsg_type_id
bsg_lod_type(void);

BSG_EXPORT extern bsg_type_id
bsg_transform_type(void);

BSG_EXPORT extern bsg_type_id
bsg_material_type(void);

BSG_EXPORT extern bsg_type_id
bsg_draw_style_type(void);

BSG_EXPORT extern bsg_type_id
bsg_complexity_type(void);

BSG_EXPORT extern bsg_type_id
bsg_camera_type(void);

BSG_EXPORT extern bsg_type_id
bsg_light_type(void);

BSG_EXPORT extern bsg_type_id
bsg_shape_type(void);

BSG_EXPORT extern bsg_type_id
bsg_geometry_type(void);

BSG_EXPORT extern bsg_type_id
bsg_field_type(void);

BSG_EXPORT extern bsg_type_id
bsg_sensor_type(void);

BSG_EXPORT extern bsg_type_id
bsg_action_type(void);

BSG_EXPORT extern bsg_type_id
bsg_view_scope_type(void);

__END_DECLS

#endif /* BSG_OBJECT_H */

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
