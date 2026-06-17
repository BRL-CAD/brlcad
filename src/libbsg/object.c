/*                         O B J E C T . C
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
/** @file libbsg/object.c
 *
 * BSG runtime object and type support.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/str.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg_private.h"
#include "node_private.h"
#include "object_private.h"

enum bsg_builtin_type_id {
    BSG_BUILTIN_OBJECT = 1,
    BSG_BUILTIN_NODE,
    BSG_BUILTIN_GROUP,
    BSG_BUILTIN_SEPARATOR,
    BSG_BUILTIN_SWITCH,
    BSG_BUILTIN_LOD,
    BSG_BUILTIN_TRANSFORM,
    BSG_BUILTIN_MATERIAL,
    BSG_BUILTIN_DRAW_STYLE,
    BSG_BUILTIN_COMPLEXITY,
    BSG_BUILTIN_CAMERA,
    BSG_BUILTIN_LIGHT,
    BSG_BUILTIN_SHAPE,
    BSG_BUILTIN_GEOMETRY,
    BSG_BUILTIN_FIELD,
    BSG_BUILTIN_SENSOR,
    BSG_BUILTIN_ACTION,
    BSG_BUILTIN_VIEW_SCOPE,
    BSG_BUILTIN_LAST = BSG_BUILTIN_VIEW_SCOPE
};

#define BSG_TYPE_REGISTRY_MAX 256

struct bsg_type_record {
    const char *name;
    bsg_type_id parent;
    int owned_name;
};

static struct bsg_type_record bsg_type_registry[BSG_TYPE_REGISTRY_MAX];
static bsg_type_id bsg_type_registry_next = BSG_BUILTIN_LAST + 1;
static int bsg_type_registry_initialized = 0;

static void
_bsg_type_builtin(bsg_type_id type, const char *name, bsg_type_id parent)
{
    bsg_type_registry[type].name = name;
    bsg_type_registry[type].parent = parent;
    bsg_type_registry[type].owned_name = 0;
}

static void
_bsg_type_registry_init_locked(void)
{
    if (bsg_type_registry_initialized)
	return;

    memset(bsg_type_registry, 0, sizeof(bsg_type_registry));
    _bsg_type_builtin(BSG_BUILTIN_OBJECT, "BSGObject", BSG_TYPE_INVALID);
    _bsg_type_builtin(BSG_BUILTIN_NODE, "BSGNode", BSG_BUILTIN_OBJECT);
    _bsg_type_builtin(BSG_BUILTIN_GROUP, "BSGGroup", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_SEPARATOR, "BSGSeparator", BSG_BUILTIN_GROUP);
    _bsg_type_builtin(BSG_BUILTIN_SWITCH, "BSGSwitch", BSG_BUILTIN_GROUP);
    _bsg_type_builtin(BSG_BUILTIN_LOD, "BSGLevelOfDetail", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_TRANSFORM, "BSGTransform", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_MATERIAL, "BSGMaterial", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_DRAW_STYLE, "BSGDrawStyle", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_COMPLEXITY, "BSGComplexity", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_CAMERA, "BSGCamera", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_LIGHT, "BSGLight", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_SHAPE, "BSGShape", BSG_BUILTIN_NODE);
    _bsg_type_builtin(BSG_BUILTIN_GEOMETRY, "BSGGeometry", BSG_BUILTIN_SHAPE);
    _bsg_type_builtin(BSG_BUILTIN_FIELD, "BSGField", BSG_BUILTIN_OBJECT);
    _bsg_type_builtin(BSG_BUILTIN_SENSOR, "BSGSensor", BSG_BUILTIN_OBJECT);
    _bsg_type_builtin(BSG_BUILTIN_ACTION, "BSGAction", BSG_BUILTIN_OBJECT);
    _bsg_type_builtin(BSG_BUILTIN_VIEW_SCOPE, "BSGViewScope", BSG_BUILTIN_NODE);

    bsg_type_registry_next = BSG_BUILTIN_LAST + 1;
    bsg_type_registry_initialized = 1;
}

static int
_bsg_type_valid_locked(bsg_type_id type)
{
    _bsg_type_registry_init_locked();
    if (type == BSG_TYPE_INVALID || type >= BSG_TYPE_REGISTRY_MAX)
	return 0;
    return bsg_type_registry[type].name ? 1 : 0;
}

static int
_bsg_type_valid(bsg_type_id type)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    int ret = _bsg_type_valid_locked(type);
    bu_semaphore_release(BU_SEM_SYSCALL);
    return ret;
}

static bsg_type_id
_bsg_node_runtime_type(const bsg_node *node)
{
    if (!node)
	return BSG_TYPE_INVALID;

    if (node->i && _bsg_type_valid(node->i->object_type))
	return node->i->object_type;

    return bsg_node_type();
}

bsg_object_ref
bsg_object_ref_null(void)
{
    bsg_object_ref object = BSG_OBJECT_REF_NULL_INIT;
    return object;
}

int
bsg_object_ref_is_null(bsg_object_ref object)
{
    return object.opaque ? 0 : 1;
}

int
bsg_object_ref_equal(bsg_object_ref a, bsg_object_ref b)
{
    return a.opaque == b.opaque;
}

bsg_type_id
bsg_type_register(const char *name, bsg_type_id parent)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    if (!name || !name[0] || !_bsg_type_valid_locked(parent)) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	return BSG_TYPE_INVALID;
    }

    for (bsg_type_id i = 1; i < bsg_type_registry_next; i++) {
	if (bsg_type_registry[i].name && BU_STR_EQUAL(bsg_type_registry[i].name, name))
	{
	    bsg_type_id ret = (bsg_type_registry[i].parent == parent) ? i : BSG_TYPE_INVALID;
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    return ret;
	}
    }

    if (bsg_type_registry_next >= BSG_TYPE_REGISTRY_MAX) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	return BSG_TYPE_INVALID;
    }

    bsg_type_id type = bsg_type_registry_next++;
    bsg_type_registry[type].name = bu_strdup(name);
    bsg_type_registry[type].parent = parent;
    bsg_type_registry[type].owned_name = 1;
    bu_semaphore_release(BU_SEM_SYSCALL);
    return type;
}

const char *
bsg_type_name(bsg_type_id type)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    const char *name = _bsg_type_valid_locked(type) ? bsg_type_registry[type].name : NULL;
    bu_semaphore_release(BU_SEM_SYSCALL);
    return name;
}

int
bsg_type_is_a(bsg_type_id type, bsg_type_id base)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    if (!_bsg_type_valid_locked(type) || !_bsg_type_valid_locked(base)) {
	bu_semaphore_release(BU_SEM_SYSCALL);
	return 0;
    }

    while (_bsg_type_valid_locked(type)) {
	if (type == base) {
	    bu_semaphore_release(BU_SEM_SYSCALL);
	    return 1;
	}
	type = bsg_type_registry[type].parent;
    }

    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}

bsg_type_id
bsg_object_type(bsg_object_ref object)
{
    bsg_node *node = (bsg_node *)object.opaque;

    if (!node)
	return BSG_TYPE_INVALID;

    return _bsg_node_runtime_type(node);
}

void
bsg_object_destroy(bsg_object_ref object)
{
    if (!object.opaque)
	return;

    if (bsg_type_is_a(bsg_object_type(object), bsg_node_type()))
	bsg_node_destroy((bsg_node *)object.opaque);
}

static bsg_type_id
_bsg_builtin_type(bsg_type_id type)
{
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    _bsg_type_registry_init_locked();
    bu_semaphore_release(BU_SEM_SYSCALL);
    return type;
}

bsg_type_id
bsg_type_object(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_OBJECT);
}

bsg_type_id
bsg_node_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_NODE);
}

bsg_type_id
bsg_group_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_GROUP);
}

bsg_type_id
bsg_separator_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_SEPARATOR);
}

bsg_type_id
bsg_switch_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_SWITCH);
}

bsg_type_id
bsg_lod_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_LOD);
}

bsg_type_id
bsg_transform_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_TRANSFORM);
}

bsg_type_id
bsg_material_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_MATERIAL);
}

bsg_type_id
bsg_draw_style_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_DRAW_STYLE);
}

bsg_type_id
bsg_complexity_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_COMPLEXITY);
}

bsg_type_id
bsg_camera_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_CAMERA);
}

bsg_type_id
bsg_light_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_LIGHT);
}

bsg_type_id
bsg_shape_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_SHAPE);
}

bsg_type_id
bsg_geometry_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_GEOMETRY);
}

bsg_type_id
bsg_field_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_FIELD);
}

bsg_type_id
bsg_sensor_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_SENSOR);
}

bsg_type_id
bsg_action_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_ACTION);
}

bsg_type_id
bsg_view_scope_type(void)
{
    return _bsg_builtin_type(BSG_BUILTIN_VIEW_SCOPE);
}

bsg_object_ref
bsg_object_ref_from_node(bsg_node *node)
{
    bsg_object_ref object = BSG_OBJECT_REF_NULL_INIT;
    object.opaque = (void *)node;
    return object;
}

bsg_node *
bsg_object_ref_node(bsg_object_ref object)
{
    if (!bsg_type_is_a(bsg_object_type(object), bsg_node_type()))
	return NULL;
    return (bsg_node *)object.opaque;
}

void
bsg_node_set_object_type(bsg_node *node, bsg_type_id type)
{
    if (!node)
	return;
    bsg_node_internal_ensure(node)->object_type =
	_bsg_type_valid(type) ? type : bsg_node_type();
}

bsg_type_id
bsg_node_object_type(const bsg_node *node)
{
    return _bsg_node_runtime_type(node);
}

int
bsg_node_type_is_a(const bsg_node *node, bsg_type_id type)
{
    return node ? bsg_type_is_a(bsg_node_object_type(node), type) : 0;
}

bsg_object_ref
bsg_object_ref_from_scene_ref(bsg_scene_ref ref)
{
    bsg_object_ref object = BSG_OBJECT_REF_NULL_INIT;
    object.opaque = ref.opaque;
    return object;
}

bsg_scene_ref
bsg_scene_ref_from_object_ref(bsg_object_ref object)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = bsg_object_ref_node(object);
    return ref;
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
