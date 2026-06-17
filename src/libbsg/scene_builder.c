/*                S C E N E _ B U I L D E R . C
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
/** @file libbsg/scene_builder.c
 *
 * Typed scene construction API.
 */

#include "common.h"

#include "bsg/field.h"
#include "bsg/draw_source.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"
#include "bsg/scene_object.h"
#include "bsg/util.h"
#include "bsg/view_scope.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "field_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "scene_object_private.h"
#include "view_scope_private.h"


static bsg_scene_ref
_ref_from_node(bsg_node *node)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = (void *)node;
    return ref;
}


static bsg_node *
_node_from_ref(bsg_scene_ref ref)
{
    return (bsg_node *)ref.opaque;
}


static void
_set_initial_name(bsg_scene_ref ref, const char *name)
{
    if (!name)
	return;
    bsg_scene_set_name(ref, name);
}


static int
_view_root_shared_by_other(const struct bsg_view *v, const bsg_node *root)
{
    if (!v || !root || !v->vset)
	return 0;

    struct bu_ptbl *views = bsg_set_views(v->vset);
    if (!views)
	return 0;

    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *other = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (other && other != v && bsg_view_scene_root(other) == root)
	    return 1;
    }

    return 0;
}


static void
_view_scene_root_release(struct bsg_view *v)
{
    if (!v)
	return;

    bsg_node *root = bsg_view_scene_root(v);
    if (!root)
	return;

    bsg_view_scene_root_set(v, NULL);
    if (bsg_node_get_view(root) == v && !_view_root_shared_by_other(v, root))
	bsg_node_destroy(root);
}


bsg_scene_ref
bsg_scene_ref_null(void)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    return ref;
}


int
bsg_scene_ref_is_null(bsg_scene_ref ref)
{
    return _node_from_ref(ref) ? 0 : 1;
}


int
bsg_scene_ref_equal(bsg_scene_ref a, bsg_scene_ref b)
{
    return _node_from_ref(a) == _node_from_ref(b);
}


enum bsg_scene_element_type
bsg_scene_ref_type(bsg_scene_ref ref)
{
    bsg_type_id type = bsg_object_type(bsg_object_ref_from_scene_ref(ref));

    if (type == BSG_TYPE_INVALID)
	return BSG_SCENE_ELEMENT_NONE;
    if (bsg_type_is_a(type, bsg_lod_type()))
	return BSG_SCENE_ELEMENT_LOD;
    if (bsg_type_is_a(type, bsg_transform_type()))
	return BSG_SCENE_ELEMENT_TRANSFORM;
    if (bsg_type_is_a(type, bsg_view_scope_type()))
	return BSG_SCENE_ELEMENT_VIEW_SCOPE;
    if (bsg_type_is_a(type, bsg_shape_type()))
	return BSG_SCENE_ELEMENT_SHAPE;
    if (bsg_type_is_a(type, bsg_group_type()))
	return BSG_SCENE_ELEMENT_GROUP;
    return BSG_SCENE_ELEMENT_NONE;
}


bsg_scene_ref
bsg_scene_group_create(struct bsg_view *v, const char *name)
{
    bsg_scene_ref ref = _ref_from_node(bsg_group_create(v));
    _set_initial_name(ref, name);
    return ref;
}


bsg_scene_ref
bsg_scene_shape_create(struct bsg_view *v, const char *name)
{
    bsg_scene_ref ref = _ref_from_node(bsg_shape_create(v));
    _set_initial_name(ref, name);
    return ref;
}


bsg_scene_ref
bsg_scene_transform_create(struct bsg_view *v, const mat_t mat, const char *name)
{
    bsg_scene_ref ref = _ref_from_node(bsg_transform_create(v));
    if (mat)
	bsg_scene_set_transform(ref, mat);
    _set_initial_name(ref, name);
    return ref;
}


bsg_scene_ref
bsg_scene_view_scope_create(struct bsg_view *v, const char *name)
{
    bsg_scene_ref ref = _ref_from_node(bsg_view_scope_create(v));
    _set_initial_name(ref, name);
    return ref;
}


bsg_scene_ref
bsg_view_scene_ref(const struct bsg_view *v)
{
    return _ref_from_node(bsg_view_scene_root(v));
}


bsg_scene_ref
bsg_view_scene_ref_ensure(struct bsg_view *v)
{
    return _ref_from_node(bsg_view_scene_root_ensure(v));
}


void
bsg_view_scene_ref_detach(struct bsg_view *v)
{
    _view_scene_root_release(v);
}


int
bsg_view_scene_ref_attach(struct bsg_view *v, bsg_scene_ref root_ref)
{
    if (!v)
	return 0;

    bsg_node *root = _node_from_ref(root_ref);
    bsg_node *old_root = bsg_view_scene_root(v);
    if (old_root && old_root != root)
	_view_scene_root_release(v);

    bsg_view_scene_root_set(v, root);
    return 1;
}


void
bsg_scene_ref_destroy(bsg_scene_ref ref)
{
    bsg_node *node = _node_from_ref(ref);

    if (!node)
	return;
    bsg_node_destroy(node);
}


void
bsg_scene_set_name(bsg_scene_ref ref, const char *name)
{
    bsg_node_set_name(_node_from_ref(ref), name);
}


const char *
bsg_scene_name(bsg_scene_ref ref)
{
    return bsg_node_name(_node_from_ref(ref));
}


void
bsg_scene_set_visible(bsg_scene_ref ref, int visible)
{
    bsg_node_set_visible_state(_node_from_ref(ref), visible);
}


int
bsg_scene_visible(bsg_scene_ref ref)
{
    return bsg_node_visible(_node_from_ref(ref));
}


void
bsg_scene_set_color(bsg_scene_ref ref, unsigned char r, unsigned char g, unsigned char b)
{
    bsg_node_set_color(_node_from_ref(ref), r, g, b);
}


void
bsg_scene_color(bsg_scene_ref ref, unsigned char *r, unsigned char *g, unsigned char *b)
{
    bsg_node_get_color(_node_from_ref(ref), r, g, b);
}


void
bsg_scene_set_transform(bsg_scene_ref ref, const mat_t mat)
{
    bsg_node_set_transform(_node_from_ref(ref), mat);
}


void
bsg_scene_transform(bsg_scene_ref ref, mat_t mat)
{
    bsg_node_transform(_node_from_ref(ref), mat);
}


void
bsg_scene_set_bounds(bsg_scene_ref ref, const point_t bmin, const point_t bmax, int valid)
{
    bsg_node_set_bounds(_node_from_ref(ref), bmin, bmax, valid);
}


int
bsg_scene_bounds(bsg_scene_ref ref, point_t bmin, point_t bmax)
{
    return bsg_node_bounds(_node_from_ref(ref), bmin, bmax);
}


struct bsg_view *
bsg_scene_view(bsg_scene_ref ref)
{
    return bsg_node_get_view(_node_from_ref(ref));
}


int
bsg_scene_is_view_scope(bsg_scene_ref ref)
{
    bsg_object_ref object = bsg_object_ref_from_scene_ref(ref);
    return bsg_type_is_a(bsg_object_type(object), bsg_view_scope_type());
}


int
bsg_scene_is_database_source(bsg_scene_ref ref)
{
    bsg_node *node = _node_from_ref(ref);
    return bsg_node_is_database_source(node);
}


int
bsg_scene_is_view_source(bsg_scene_ref ref)
{
    bsg_node *node = _node_from_ref(ref);
    return bsg_node_is_view_source(node);
}


int
bsg_scene_is_local_source(bsg_scene_ref ref)
{
    bsg_node *node = _node_from_ref(ref);
    return bsg_node_is_local_source(node);
}


void
bsg_scene_set_view(bsg_scene_ref ref, struct bsg_view *v)
{
    bsg_node_set_view(_node_from_ref(ref), v);
}


int
bsg_scene_update_bounds(bsg_scene_ref ref, struct bsg_view *v)
{
    return bsg_scene_node_update_bounds(_node_from_ref(ref), v);
}


void
bsg_scene_invalidate(bsg_scene_ref ref)
{
    bsg_scene_node_invalidate(_node_from_ref(ref));
}


fastf_t
bsg_scene_view_depth(bsg_scene_ref ref, struct bsg_view *v, int mode)
{
    return bsg_scene_node_view_depth(_node_from_ref(ref), v, mode);
}


void
bsg_scene_append_child(bsg_scene_ref parent, bsg_scene_ref child)
{
    bsg_node_add_child(_node_from_ref(parent), _node_from_ref(child));
}


void
bsg_scene_remove_child(bsg_scene_ref parent, bsg_scene_ref child)
{
    bsg_node_remove_child(_node_from_ref(parent), _node_from_ref(child));
}


bsg_scene_ref
bsg_scene_parent(bsg_scene_ref ref)
{
    return _ref_from_node(bsg_node_parent(_node_from_ref(ref)));
}


size_t
bsg_scene_child_count(bsg_scene_ref ref)
{
    return bsg_node_child_count(_node_from_ref(ref));
}


bsg_scene_ref
bsg_scene_child_at(bsg_scene_ref ref, size_t idx)
{
    return _ref_from_node(bsg_node_child_at(_node_from_ref(ref), idx));
}


bsg_scene_ref
bsg_scene_find_child(bsg_scene_ref parent, const char *name)
{
    return _ref_from_node(bsg_scene_child_find(_node_from_ref(parent), name));
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
