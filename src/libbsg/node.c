/*                          N O D E . C
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
/** @file libbsg/node.c
 *
 * Generic BSG node lifecycle and accessor API.
 */

#include "common.h"

#include "bu/ptbl.h"
#include "vmath.h"

#include "bsg/defines.h"
#include "bsg/draw_set.h"
#include "bsg/field.h"
#include "bsg/group.h"
#include "bsg/identity.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "bsg/separator.h"
#include "bsg/switch.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg_private.h"
#include "draw_source_private.h"
#include "draw_set_private.h"
#include "field_private.h"
#include "identity_private.h"
#include "lod_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"

#define BSG_NODE_SOURCE_MASK ((unsigned int)(BSG_SOURCE_DB | BSG_SOURCE_VIEW | BSG_SOURCE_LOCAL | BSG_SOURCE_CHILD))
#define BSG_NODE_GEOMETRY_MASK ((unsigned int)(BSG_GEOMETRY_DB_OBJECT | BSG_GEOMETRY_VIEW_OVERLAY | BSG_GEOMETRY_LINE_SET | BSG_GEOMETRY_TEXT_LABELS | BSG_GEOMETRY_AXES_WIDGET | BSG_GEOMETRY_POLYGON_REGION))


bsg_node *
bsg_node_create(struct bsg_view *v, unsigned long long kind)
{
    (void)kind;

    if (!v)
	return NULL;

    bsg_node *n = bsg_scene_node_create(v, BSG_SOURCE_VIEW | BSG_SOURCE_LOCAL);
    if (!n)
	return NULL;

    bsg_node_set_source_flags(n, 0);
    bsg_node_set_object_type(n, bsg_node_type());
    bsg_node_set_visible_state(n, 1);
    bsg_node_revision_set(n, 0);

    return n;
}


void
bsg_node_destroy(bsg_node *node)
{
    if (!node)
	return;

    if (node->parent)
	bsg_node_remove_child(node->parent, node);

    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	bsg_node *c = (bsg_node *)BU_PTBL_GET(&node->children, i);
	if (c && c->parent == node)
	    c->parent = NULL;
    }
    bu_ptbl_reset(&node->children);
    bsg_scene_node_release(node);
}


void
bsg_node_set_source_flags(bsg_node *node, unsigned int source_flags)
{
    if (!node)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    ni->source_flags = source_flags & BSG_NODE_SOURCE_MASK;
    ni->source_flags_valid = 1;
}


unsigned int
bsg_node_source_flags(const bsg_node *node)
{
    if (!node)
	return 0;

    unsigned int flags = 0;
    if (node->i && node->i->source_flags_valid) {
	flags = node->i->source_flags;
    }

    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_DB_OBJECT))
	flags |= BSG_SOURCE_DB;

    return flags & BSG_NODE_SOURCE_MASK;
}


int
bsg_node_is_database_source(const bsg_node *node)
{
    return (bsg_node_source_flags(node) & BSG_SOURCE_DB) ? 1 : 0;
}


int
bsg_node_is_view_source(const bsg_node *node)
{
    return (bsg_node_source_flags(node) & BSG_SOURCE_VIEW) ? 1 : 0;
}


int
bsg_node_is_local_source(const bsg_node *node)
{
    return (bsg_node_source_flags(node) & BSG_SOURCE_LOCAL) ? 1 : 0;
}


int
bsg_node_is_internal_child_source(const bsg_node *node)
{
    return (bsg_node_source_flags(node) & BSG_SOURCE_CHILD) ? 1 : 0;
}


void
bsg_node_set_geometry_roles(bsg_node *node, unsigned int geometry_roles)
{
    if (!node)
	return;
    struct bsg_node_internal *ni = bsg_node_internal_ensure(node);
    ni->geometry_roles = geometry_roles & BSG_NODE_GEOMETRY_MASK;
    ni->geometry_roles_valid = 1;
}


void
bsg_node_add_geometry_roles(bsg_node *node, unsigned int geometry_roles)
{
    if (!node)
	return;
    bsg_node_set_geometry_roles(node, bsg_node_geometry_roles(node) | geometry_roles);
}


unsigned int
bsg_node_geometry_roles(const bsg_node *node)
{
    if (!node)
	return 0;
    if (node->i && node->i->geometry_roles_valid)
	return node->i->geometry_roles & BSG_NODE_GEOMETRY_MASK;
    return 0;
}


int
bsg_node_has_geometry_role(const bsg_node *node, unsigned int geometry_role)
{
    return (bsg_node_geometry_roles(node) & (geometry_role & BSG_NODE_GEOMETRY_MASK)) ? 1 : 0;
}


void
bsg_node_set_name(bsg_node *node, const char *name)
{
    (void)bsg_field_set_string(bsg_node_field_ref(node, BSG_FIELD_NAME), name);
}


const char *
bsg_node_name(const bsg_node *node)
{
    const char *name = NULL;
    if (!bsg_field_get_string(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_NAME), &name))
	return bsg_node_identity_name(node);
    return name;
}


bsg_node *
bsg_node_parent(const bsg_node *node)
{
    if (!node)
	return NULL;
    return node->parent;
}


size_t
bsg_node_child_count(const bsg_node *node)
{
    if (!node)
	return 0;
    return BU_PTBL_LEN(&node->children);
}


bsg_node *
bsg_node_child_at(const bsg_node *node, size_t idx)
{
    if (!node || idx >= BU_PTBL_LEN(&node->children))
	return NULL;
    return (bsg_node *)BU_PTBL_GET(&node->children, idx);
}


void
bsg_node_add_child(bsg_node *parent, bsg_node *child)
{
    if (!parent || !child || parent == child)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(&parent->children); i++) {
	if ((bsg_node *)BU_PTBL_GET(&parent->children, i) == child)
	    return;
    }

    if (child->parent && child->parent != parent)
	bsg_node_remove_child(child->parent, child);

    child->parent = parent;
    bu_ptbl_ins(&parent->children, (long *)child);
    bsg_node_touch(parent);
    bsg_node_field_touch(parent, BSG_FIELD_CHILDREN);
    bsg_node_bbox_invalidate(parent);
}


void
bsg_node_remove_child(bsg_node *parent, bsg_node *child)
{
    if (!parent || !child)
	return;

    intmax_t loc = bu_ptbl_locate(&parent->children, (const long *)child);
    if (loc < 0)
	return;

    bu_ptbl_rm(&parent->children, (const long *)child);
    if (child->parent == parent)
	child->parent = NULL;
    bsg_node_touch(parent);
    bsg_node_field_touch(parent, BSG_FIELD_CHILDREN);
    bsg_node_bbox_invalidate(parent);
}


void
bsg_switch_node_set_which_child(bsg_node *node, int which_child)
{
    (void)bsg_field_set_int(bsg_node_field_ref(node, BSG_FIELD_SWITCH_WHICH), which_child);
}


int
bsg_switch_node_which_child(const bsg_node *node)
{
    int which_child = BSG_SWITCH_NONE;
    if (!bsg_field_get_int(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_SWITCH_WHICH), &which_child))
	return BSG_SWITCH_NONE;
    return which_child;
}


int
bsg_switch_node_child_selected(const bsg_node *node, size_t idx)
{
    int which_child = bsg_switch_node_which_child(node);
    if (which_child == BSG_SWITCH_ALL)
	return 1;
    if (which_child == BSG_SWITCH_NONE || which_child < 0)
	return 0;
    return ((size_t)which_child == idx) ? 1 : 0;
}


void
bsg_node_set_visible_state(bsg_node *node, int on)
{
    (void)bsg_field_set_bool(bsg_node_field_ref(node, BSG_FIELD_VISIBILITY), on ? 1 : 0);
}


int
bsg_node_visible(const bsg_node *node)
{
    int visible = 0;
    if (!bsg_field_get_bool(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_VISIBILITY), &visible))
	return 0;
    return visible ? 1 : 0;
}


void
bsg_node_set_transform(bsg_node *node, const mat_t mat)
{
    (void)bsg_field_set_matrix(bsg_node_field_ref(node, BSG_FIELD_TRANSFORM), mat);
}


void
bsg_node_transform(const bsg_node *node, mat_t mat)
{
    if (!mat)
	return;
    (void)bsg_field_get_matrix(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_TRANSFORM), mat);
}


void
bsg_node_set_bounds(bsg_node *node, const point_t bmin, const point_t bmax, int valid)
{
    (void)bsg_field_set_bbox(bsg_node_field_ref(node, BSG_FIELD_BOUNDS), bmin, bmax, valid);
}


int
bsg_node_bounds(const bsg_node *node, point_t bmin, point_t bmax)
{
    int valid = 0;
    if (!bsg_field_get_bbox(bsg_node_field_ref((bsg_node *)node, BSG_FIELD_BOUNDS), bmin, bmax, &valid))
	return 0;
    return valid ? 1 : 0;
}


static bsg_node_ref
_node_ref_from_node(bsg_node *node)
{
    bsg_node_ref ref = BSG_NODE_REF_NULL_INIT;
    ref.object = bsg_object_ref_from_node(node);
    return ref;
}


static bsg_node *
_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(ref.object);
}


static bsg_node_ref
_typed_node_create(struct bsg_view *v, unsigned long long kind, bsg_type_id type, const char *name)
{
    bsg_node *node = bsg_node_create(v, kind);
    if (!node)
	return bsg_node_ref_null();
    bsg_node_set_object_type(node, type);
    if (name)
	bsg_node_set_name(node, name);
    return _node_ref_from_node(node);
}


bsg_node_ref
bsg_node_ref_null(void)
{
    bsg_node_ref ref = BSG_NODE_REF_NULL_INIT;
    return ref;
}


int
bsg_node_ref_is_null(bsg_node_ref ref)
{
    return bsg_object_ref_is_null(ref.object);
}


int
bsg_node_ref_equal(bsg_node_ref a, bsg_node_ref b)
{
    return bsg_object_ref_equal(a.object, b.object);
}


bsg_object_ref
bsg_node_ref_object(bsg_node_ref ref)
{
    return ref.object;
}


bsg_node_ref
bsg_node_ref_from_object(bsg_object_ref object)
{
    if (!bsg_type_is_a(bsg_object_type(object), bsg_node_type()))
	return bsg_node_ref_null();
    bsg_node_ref ref = BSG_NODE_REF_NULL_INIT;
    ref.object = object;
    return ref;
}


bsg_type_id
bsg_node_ref_type(bsg_node_ref ref)
{
    return bsg_object_type(ref.object);
}


int
bsg_node_is_a(bsg_node_ref ref, bsg_type_id type)
{
    return bsg_type_is_a(bsg_node_ref_type(ref), type);
}


void
bsg_node_ref_destroy(bsg_node_ref ref)
{
    bsg_node *node = _node_from_ref(ref);
    struct bsg_view *view = bsg_node_get_view(node);
    if (node && view && bsg_view_scene_root(view) == node)
	bsg_view_scene_root_detach_from_root(node);
    bsg_object_destroy(ref.object);
}


void
bsg_node_ref_set_name(bsg_node_ref ref, const char *name)
{
    bsg_node_set_name(_node_from_ref(ref), name);
}


const char *
bsg_node_ref_name(bsg_node_ref ref)
{
    return bsg_node_name(_node_from_ref(ref));
}


void
bsg_node_ref_set_visible(bsg_node_ref ref, int visible)
{
    bsg_node_set_visible_state(_node_from_ref(ref), visible);
}


int
bsg_node_ref_visible(bsg_node_ref ref)
{
    return bsg_node_visible(_node_from_ref(ref));
}


void
bsg_node_ref_set_color(bsg_node_ref ref, unsigned char r, unsigned char g, unsigned char b)
{
    bsg_node_set_color(_node_from_ref(ref), r, g, b);
}


void
bsg_node_ref_color(bsg_node_ref ref, unsigned char *r, unsigned char *g, unsigned char *b)
{
    bsg_node_get_color(_node_from_ref(ref), r, g, b);
}


void
bsg_node_ref_set_transform(bsg_node_ref ref, const mat_t mat)
{
    bsg_node_set_transform(_node_from_ref(ref), mat);
}


void
bsg_node_ref_transform(bsg_node_ref ref, mat_t mat)
{
    bsg_node_transform(_node_from_ref(ref), mat);
}


void
bsg_node_ref_set_bounds(bsg_node_ref ref, const point_t bmin, const point_t bmax, int valid)
{
    bsg_node_set_bounds(_node_from_ref(ref), bmin, bmax, valid);
}


int
bsg_node_ref_bounds(bsg_node_ref ref, point_t bmin, point_t bmax)
{
    return bsg_node_bounds(_node_from_ref(ref), bmin, bmax);
}


bsg_node_ref
bsg_node_ref_parent(bsg_node_ref ref)
{
    return _node_ref_from_node(bsg_node_parent(_node_from_ref(ref)));
}


size_t
bsg_group_ref_child_count(bsg_group_ref ref)
{
    bsg_node *node = _node_from_ref(ref.node);
    if (!node || !bsg_type_is_a(bsg_object_type(ref.node.object), bsg_group_type()))
	return 0;
    return bsg_node_child_count(node);
}


bsg_node_ref
bsg_group_ref_child_at(bsg_group_ref ref, size_t idx)
{
    bsg_node *node = _node_from_ref(ref.node);
    if (!node || !bsg_type_is_a(bsg_object_type(ref.node.object), bsg_group_type()))
	return _node_ref_from_node(NULL);
    return _node_ref_from_node(bsg_node_child_at(node, idx));
}


void
bsg_group_ref_append_child(bsg_group_ref parent, bsg_node_ref child)
{
    bsg_node *parent_node = _node_from_ref(parent.node);
    if (!parent_node || !bsg_type_is_a(bsg_object_type(parent.node.object), bsg_group_type()))
	return;
    bsg_node_add_child(parent_node, _node_from_ref(child));
}


void
bsg_group_ref_remove_child(bsg_group_ref parent, bsg_node_ref child)
{
    bsg_node *parent_node = _node_from_ref(parent.node);
    if (!parent_node || !bsg_type_is_a(bsg_object_type(parent.node.object), bsg_group_type()))
	return;
    bsg_node_remove_child(parent_node, _node_from_ref(child));
}


size_t
bsg_separator_ref_child_count(bsg_separator_ref ref)
{
    return bsg_group_ref_child_count(ref.group);
}


bsg_node_ref
bsg_separator_ref_child_at(bsg_separator_ref ref, size_t idx)
{
    return bsg_group_ref_child_at(ref.group, idx);
}


void
bsg_separator_ref_append_child(bsg_separator_ref parent, bsg_node_ref child)
{
    bsg_group_ref_append_child(parent.group, child);
}


void
bsg_separator_ref_remove_child(bsg_separator_ref parent, bsg_node_ref child)
{
    bsg_group_ref_remove_child(parent.group, child);
}


size_t
bsg_switch_ref_child_count(bsg_switch_ref ref)
{
    return bsg_group_ref_child_count(ref.group);
}


bsg_node_ref
bsg_switch_ref_child_at(bsg_switch_ref ref, size_t idx)
{
    return bsg_group_ref_child_at(ref.group, idx);
}


void
bsg_switch_ref_append_child(bsg_switch_ref parent, bsg_node_ref child)
{
    bsg_group_ref_append_child(parent.group, child);
}


void
bsg_switch_ref_remove_child(bsg_switch_ref parent, bsg_node_ref child)
{
    bsg_group_ref_remove_child(parent.group, child);
}


void
bsg_switch_ref_set_which_child(bsg_switch_ref ref, int which_child)
{
    if (!bsg_type_is_a(bsg_object_type(ref.group.node.object), bsg_switch_type()))
	return;
    bsg_switch_node_set_which_child(_node_from_ref(ref.group.node), which_child);
}


int
bsg_switch_ref_which_child(bsg_switch_ref ref)
{
    if (!bsg_type_is_a(bsg_object_type(ref.group.node.object), bsg_switch_type()))
	return BSG_SWITCH_NONE;
    return bsg_switch_node_which_child(_node_from_ref(ref.group.node));
}


bsg_group_ref
bsg_group_ref_create(struct bsg_view *v, const char *name)
{
    bsg_group_ref ref = BSG_GROUP_REF_NULL_INIT;
    ref.node = _typed_node_create(v, BSG_NODE_GROUP, bsg_group_type(), name);
    return ref;
}


bsg_separator_ref
bsg_separator_ref_create(struct bsg_view *v, const char *name)
{
    bsg_separator_ref ref = BSG_SEPARATOR_REF_NULL_INIT;
    ref.group.node = _typed_node_create(v, BSG_NODE_GROUP, bsg_separator_type(), name);
    return ref;
}


bsg_separator_ref
bsg_view_scene_separator_ref(struct bsg_view *v, int create)
{
    bsg_separator_ref ref = BSG_SEPARATOR_REF_NULL_INIT;
    bsg_node *root = create ? bsg_view_scene_root_ensure(v) : bsg_view_scene_root(v);
    if (!root)
	return ref;
    bsg_node_set_object_type(root, bsg_separator_type());
    ref.group.node = _node_ref_from_node(root);
    return ref;
}


bsg_switch_ref
bsg_switch_ref_create(struct bsg_view *v, const char *name)
{
    bsg_switch_ref ref = BSG_SWITCH_REF_NULL_INIT;
    ref.group.node = _typed_node_create(v, BSG_NODE_GROUP, bsg_switch_type(), name);
    return ref;
}


bsg_lod_ref
bsg_lod_ref_create(struct bsg_view *v, const char *name)
{
    bsg_lod_ref ref = BSG_LOD_REF_NULL_INIT;
    bsg_node *node = bsg_lod_node_create(v);
    if (!node)
	return ref;
    bsg_node_set_object_type(node, bsg_lod_type());
    if (name)
	bsg_node_set_name(node, name);
    ref.node = _node_ref_from_node(node);
    return ref;
}


bsg_field_ref
bsg_lod_ref_selection_field(bsg_lod_ref lod)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_node_from_ref(lod.node), BSG_FIELD_LOD_SELECTION);
}


bsg_field_ref
bsg_lod_ref_active_level_field(bsg_lod_ref lod)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_node_from_ref(lod.node), BSG_FIELD_LOD_ACTIVE_LEVEL);
}


bsg_field_ref
bsg_lod_ref_ranges_field(bsg_lod_ref lod)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return bsg_field_ref_null();
    return bsg_node_field_ref(_node_from_ref(lod.node), BSG_FIELD_LOD_RANGES);
}


int
bsg_lod_ref_set_selection(bsg_lod_ref lod, int selection)
{
    return bsg_field_set_enum(bsg_lod_ref_selection_field(lod), selection);
}


int
bsg_lod_ref_selection(bsg_lod_ref lod)
{
    int selection = BSG_LOD_SELECT_AUTO;
    (void)bsg_field_get_enum(bsg_lod_ref_selection_field(lod), &selection);
    return selection;
}


int
bsg_lod_ref_set_active_level(bsg_lod_ref lod, int level)
{
    return bsg_field_set_int(bsg_lod_ref_active_level_field(lod), level);
}


int
bsg_lod_ref_active_level(bsg_lod_ref lod)
{
    int level = -1;
    (void)bsg_field_get_int(bsg_lod_ref_active_level_field(lod), &level);
    return level;
}


size_t
bsg_lod_ref_range_count(bsg_lod_ref lod)
{
    return bsg_field_multi_count(bsg_lod_ref_ranges_field(lod));
}


int
bsg_lod_ref_range_at(bsg_lod_ref lod, size_t idx, double *range)
{
    return bsg_field_multi_double_at(bsg_lod_ref_ranges_field(lod), idx, range);
}


int
bsg_lod_ref_append_range(bsg_lod_ref lod, double range)
{
    return bsg_field_multi_double_append(bsg_lod_ref_ranges_field(lod), range);
}


int
bsg_lod_ref_clear_ranges(bsg_lod_ref lod)
{
    return bsg_field_multi_clear(bsg_lod_ref_ranges_field(lod));
}


size_t
bsg_lod_ref_level_count(bsg_lod_ref lod)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return 0;
    return bsg_node_child_count(_node_from_ref(lod.node));
}


bsg_node_ref
bsg_lod_ref_level_at(bsg_lod_ref lod, size_t idx)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return bsg_node_ref_null();
    return _node_ref_from_node(bsg_node_child_at(_node_from_ref(lod.node), idx));
}


void
bsg_lod_ref_append_level(bsg_lod_ref lod, bsg_node_ref level)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return;
    bsg_lod_node_attach_level(_node_from_ref(lod.node), _node_from_ref(level));
}


void
bsg_lod_ref_remove_level(bsg_lod_ref lod, bsg_node_ref level)
{
    if (!bsg_type_is_a(bsg_object_type(lod.node.object), bsg_lod_type()))
	return;
    bsg_node_remove_child(_node_from_ref(lod.node), _node_from_ref(level));
}


bsg_transform_ref
bsg_transform_ref_create(struct bsg_view *v, const mat_t mat, const char *name)
{
    bsg_transform_ref ref = BSG_TRANSFORM_REF_NULL_INIT;
    ref.node = _typed_node_create(v, BSG_NODE_TRANSFORM, bsg_transform_type(), name);
    if (mat)
	bsg_node_set_transform(_node_from_ref(ref.node), mat);
    return ref;
}


bsg_material_ref
bsg_material_ref_create(struct bsg_view *v, const char *name)
{
    bsg_material_ref ref = BSG_MATERIAL_REF_NULL_INIT;
    ref.node = _typed_node_create(v, 0, bsg_material_type(), name);
    return ref;
}


bsg_draw_style_ref
bsg_draw_style_ref_create(struct bsg_view *v, const char *name)
{
    bsg_draw_style_ref ref = BSG_DRAW_STYLE_REF_NULL_INIT;
    ref.node = _typed_node_create(v, 0, bsg_draw_style_type(), name);
    return ref;
}


bsg_complexity_ref
bsg_complexity_ref_create(struct bsg_view *v, const char *name)
{
    bsg_complexity_ref ref = BSG_COMPLEXITY_REF_NULL_INIT;
    ref.node = _typed_node_create(v, 0, bsg_complexity_type(), name);
    return ref;
}


bsg_camera_ref
bsg_camera_ref_create(struct bsg_view *v, const char *name)
{
    bsg_camera_ref ref = BSG_CAMERA_REF_NULL_INIT;
    ref.node = _typed_node_create(v, 0, bsg_camera_type(), name);
    return ref;
}


bsg_light_ref
bsg_light_ref_create(struct bsg_view *v, const char *name)
{
    bsg_light_ref ref = BSG_LIGHT_REF_NULL_INIT;
    ref.node = _typed_node_create(v, 0, bsg_light_type(), name);
    return ref;
}


bsg_shape_ref
bsg_shape_ref_create(struct bsg_view *v, const char *name)
{
    bsg_shape_ref ref = BSG_REF_SHAPE_NULL_INIT;
    ref.node = _typed_node_create(v, BSG_NODE_SHAPE, bsg_shape_type(), name);
    return ref;
}


bsg_geometry_ref
bsg_geometry_ref_create(struct bsg_view *v, const char *name)
{
    bsg_geometry_ref ref = BSG_GEOMETRY_REF_NULL_INIT;
    ref.shape.node = _typed_node_create(v, BSG_NODE_SHAPE, bsg_geometry_type(), name);
    return ref;
}


bsg_node_ref bsg_group_ref_as_node(bsg_group_ref ref) { return ref.node; }
bsg_node_ref bsg_separator_ref_as_node(bsg_separator_ref ref) { return ref.group.node; }
bsg_group_ref bsg_separator_ref_as_group(bsg_separator_ref ref) { return ref.group; }
bsg_node_ref bsg_switch_ref_as_node(bsg_switch_ref ref) { return ref.group.node; }
bsg_group_ref bsg_switch_ref_as_group(bsg_switch_ref ref) { return ref.group; }
bsg_node_ref bsg_lod_ref_as_node(bsg_lod_ref ref) { return ref.node; }
bsg_node_ref bsg_transform_ref_as_node(bsg_transform_ref ref) { return ref.node; }
bsg_node_ref bsg_material_ref_as_node(bsg_material_ref ref) { return ref.node; }
bsg_node_ref bsg_draw_style_ref_as_node(bsg_draw_style_ref ref) { return ref.node; }
bsg_node_ref bsg_complexity_ref_as_node(bsg_complexity_ref ref) { return ref.node; }
bsg_node_ref bsg_camera_ref_as_node(bsg_camera_ref ref) { return ref.node; }
bsg_node_ref bsg_light_ref_as_node(bsg_light_ref ref) { return ref.node; }
bsg_node_ref bsg_shape_ref_as_node(bsg_shape_ref ref) { return ref.node; }
bsg_shape_ref bsg_geometry_ref_as_shape(bsg_geometry_ref ref) { return ref.shape; }
bsg_node_ref bsg_geometry_ref_as_node(bsg_geometry_ref ref) { return ref.shape.node; }


bsg_group_ref
bsg_node_ref_as_group(bsg_node_ref ref)
{
    bsg_group_ref typed = BSG_GROUP_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_group_type()))
	typed.node = ref;
    return typed;
}


bsg_separator_ref
bsg_node_ref_as_separator(bsg_node_ref ref)
{
    bsg_separator_ref typed = BSG_SEPARATOR_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_separator_type()))
	typed.group.node = ref;
    return typed;
}


bsg_switch_ref
bsg_node_ref_as_switch(bsg_node_ref ref)
{
    bsg_switch_ref typed = BSG_SWITCH_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_switch_type()))
	typed.group.node = ref;
    return typed;
}


bsg_lod_ref
bsg_node_ref_as_lod(bsg_node_ref ref)
{
    bsg_lod_ref typed = BSG_LOD_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_lod_type()))
	typed.node = ref;
    return typed;
}


bsg_transform_ref
bsg_node_ref_as_transform(bsg_node_ref ref)
{
    bsg_transform_ref typed = BSG_TRANSFORM_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_transform_type()))
	typed.node = ref;
    return typed;
}


bsg_material_ref
bsg_node_ref_as_material(bsg_node_ref ref)
{
    bsg_material_ref typed = BSG_MATERIAL_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_material_type()))
	typed.node = ref;
    return typed;
}


bsg_draw_style_ref
bsg_node_ref_as_draw_style(bsg_node_ref ref)
{
    bsg_draw_style_ref typed = BSG_DRAW_STYLE_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_draw_style_type()))
	typed.node = ref;
    return typed;
}


bsg_complexity_ref
bsg_node_ref_as_complexity(bsg_node_ref ref)
{
    bsg_complexity_ref typed = BSG_COMPLEXITY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_complexity_type()))
	typed.node = ref;
    return typed;
}


bsg_camera_ref
bsg_node_ref_as_camera(bsg_node_ref ref)
{
    bsg_camera_ref typed = BSG_CAMERA_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_camera_type()))
	typed.node = ref;
    return typed;
}


bsg_light_ref
bsg_node_ref_as_light(bsg_node_ref ref)
{
    bsg_light_ref typed = BSG_LIGHT_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_light_type()))
	typed.node = ref;
    return typed;
}


bsg_shape_ref
bsg_node_ref_as_shape(bsg_node_ref ref)
{
    bsg_shape_ref typed = BSG_REF_SHAPE_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_shape_type()))
	typed.node = ref;
    return typed;
}


bsg_geometry_ref
bsg_node_ref_as_geometry(bsg_node_ref ref)
{
    bsg_geometry_ref typed = BSG_GEOMETRY_REF_NULL_INIT;
    if (bsg_node_is_a(ref, bsg_geometry_type()))
	typed.shape.node = ref;
    return typed;
}


uint64_t
bsg_node_revision(const bsg_node *node)
{
    return bsg_node_revision_get(node);
}


void
bsg_node_touch(bsg_node *node)
{
    (void)bsg_node_revision_bump(node);
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
