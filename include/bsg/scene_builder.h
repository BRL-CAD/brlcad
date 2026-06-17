/*                S C E N E _ B U I L D E R . H
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
 * Typed scene construction API.
 *
 * This interface is the preferred scene authoring surface for producers.  It
 * exposes semantic elements and opaque references rather than generic graph
 * node allocation.  References returned by the element create functions are
 * owning handles and are released with bsg_scene_ref_destroy().  References
 * returned by bsg_view_scene_ref() or bsg_view_scene_ref_ensure() are borrowed
 * view attachments; detach them from the view with bsg_view_scene_ref_detach()
 * and do not destroy them directly.
 */
/** @{ */
/* @file bsg/scene_builder.h */

#ifndef BSG_SCENE_BUILDER_H
#define BSG_SCENE_BUILDER_H

#include "common.h"
#include "vmath.h"
#include "bsg/defines.h"

__BEGIN_DECLS

typedef struct bsg_scene_ref {
    void *opaque;
} bsg_scene_ref;

#define BSG_SCENE_REF_NULL_INIT { NULL }

enum bsg_scene_element_type {
    BSG_SCENE_ELEMENT_NONE = 0,
    BSG_SCENE_ELEMENT_GROUP,
    BSG_SCENE_ELEMENT_SHAPE,
    BSG_SCENE_ELEMENT_TRANSFORM,
    BSG_SCENE_ELEMENT_VIEW_SCOPE,
    BSG_SCENE_ELEMENT_LOD
};

BSG_EXPORT extern bsg_scene_ref
bsg_scene_ref_null(void);

BSG_EXPORT extern int
bsg_scene_ref_is_null(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_ref_equal(bsg_scene_ref a, bsg_scene_ref b);

BSG_EXPORT extern enum bsg_scene_element_type
bsg_scene_ref_type(bsg_scene_ref ref);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_group_create(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_shape_create(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_transform_create(struct bsg_view *v, const mat_t mat, const char *name);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_view_scope_create(struct bsg_view *v, const char *name);

BSG_EXPORT extern bsg_scene_ref
bsg_view_scene_ref(const struct bsg_view *v);

BSG_EXPORT extern bsg_scene_ref
bsg_view_scene_ref_ensure(struct bsg_view *v);

BSG_EXPORT extern void
bsg_view_scene_ref_detach(struct bsg_view *v);

BSG_EXPORT extern int
bsg_view_scene_ref_attach(struct bsg_view *v, bsg_scene_ref root);

BSG_EXPORT extern void
bsg_scene_ref_destroy(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_name(bsg_scene_ref ref, const char *name);

BSG_EXPORT extern const char *
bsg_scene_name(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_visible(bsg_scene_ref ref, int visible);

BSG_EXPORT extern int
bsg_scene_visible(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_color(bsg_scene_ref ref, unsigned char r, unsigned char g, unsigned char b);

BSG_EXPORT extern void
bsg_scene_color(bsg_scene_ref ref, unsigned char *r, unsigned char *g, unsigned char *b);

BSG_EXPORT extern void
bsg_scene_set_transform(bsg_scene_ref ref, const mat_t mat);

BSG_EXPORT extern void
bsg_scene_transform(bsg_scene_ref ref, mat_t mat);

BSG_EXPORT extern void
bsg_scene_set_bounds(bsg_scene_ref ref, const point_t bmin, const point_t bmax, int valid);

BSG_EXPORT extern int
bsg_scene_bounds(bsg_scene_ref ref, point_t bmin, point_t bmax);

BSG_EXPORT extern struct bsg_view *
bsg_scene_view(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_is_view_scope(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_is_database_source(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_mark_db_object(bsg_scene_ref shape);

BSG_EXPORT extern void
bsg_scene_set_non_database_source(bsg_scene_ref shape, int non_database_source);

BSG_EXPORT extern int
bsg_scene_is_view_source(bsg_scene_ref ref);

BSG_EXPORT extern int
bsg_scene_is_local_source(bsg_scene_ref ref);

BSG_EXPORT extern void
bsg_scene_set_view(bsg_scene_ref ref, struct bsg_view *v);

BSG_EXPORT extern int
bsg_scene_update_bounds(bsg_scene_ref ref, struct bsg_view *v);

BSG_EXPORT extern void
bsg_scene_invalidate(bsg_scene_ref ref);

BSG_EXPORT extern fastf_t
bsg_scene_view_depth(bsg_scene_ref ref, struct bsg_view *v, int mode);

BSG_EXPORT extern void
bsg_scene_append_child(bsg_scene_ref parent, bsg_scene_ref child);

BSG_EXPORT extern void
bsg_scene_remove_child(bsg_scene_ref parent, bsg_scene_ref child);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_parent(bsg_scene_ref ref);

BSG_EXPORT extern size_t
bsg_scene_child_count(bsg_scene_ref ref);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_child_at(bsg_scene_ref ref, size_t idx);

BSG_EXPORT extern bsg_scene_ref
bsg_scene_find_child(bsg_scene_ref parent, const char *name);

__END_DECLS

#endif /* BSG_SCENE_BUILDER_H */

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
