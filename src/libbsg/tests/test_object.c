/*                    T E S T _ O B J E C T . C
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
/** @file libbsg/tests/test_object.c
 *
 * Unit tests for the BSG object and runtime type API.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/object.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"
#include "../node_private.h"
#include "../object_private.h"

#define CHECK(_expr, _msg) \
    do { \
	if (!(_expr)) { \
	    printf("FAIL: %s\n", (_msg)); \
	    return 1; \
	} \
    } while (0)

struct type_case {
    bsg_type_id (*type_fn)(void);
    const char *name;
    bsg_type_id (*parent_fn)(void);
};

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v = NULL;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_object_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test object view");
}

static int
test_builtin_types(void)
{
    const struct type_case cases[] = {
	{ bsg_type_object, "BSGObject", NULL },
	{ bsg_node_type, "BSGNode", bsg_type_object },
	{ bsg_group_type, "BSGGroup", bsg_node_type },
	{ bsg_separator_type, "BSGSeparator", bsg_group_type },
	{ bsg_switch_type, "BSGSwitch", bsg_group_type },
	{ bsg_lod_type, "BSGLevelOfDetail", bsg_node_type },
	{ bsg_transform_type, "BSGTransform", bsg_node_type },
	{ bsg_material_type, "BSGMaterial", bsg_node_type },
	{ bsg_draw_style_type, "BSGDrawStyle", bsg_node_type },
	{ bsg_complexity_type, "BSGComplexity", bsg_node_type },
	{ bsg_camera_type, "BSGCamera", bsg_node_type },
	{ bsg_light_type, "BSGLight", bsg_node_type },
	{ bsg_shape_type, "BSGShape", bsg_node_type },
	{ bsg_geometry_type, "BSGGeometry", bsg_shape_type },
	{ bsg_field_type, "BSGField", bsg_type_object },
	{ bsg_sensor_type, "BSGSensor", bsg_type_object },
	{ bsg_action_type, "BSGAction", bsg_type_object },
	{ bsg_view_scope_type, "BSGViewScope", bsg_node_type }
    };

    CHECK(bsg_type_name(BSG_TYPE_INVALID) == NULL, "invalid type has no name");
    CHECK(!bsg_type_is_a(BSG_TYPE_INVALID, bsg_type_object()), "invalid is not an object");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	bsg_type_id type = cases[i].type_fn();
	CHECK(type != BSG_TYPE_INVALID, "built-in type is valid");
	CHECK(BU_STR_EQUAL(bsg_type_name(type), cases[i].name), cases[i].name);
	CHECK(bsg_type_is_a(type, type), "type is-a itself");
	CHECK(bsg_type_is_a(type, bsg_type_object()), "built-in type is an object");
	if (cases[i].parent_fn)
	    CHECK(bsg_type_is_a(type, cases[i].parent_fn()), "built-in type parent");
    }

    CHECK(bsg_type_is_a(bsg_separator_type(), bsg_group_type()), "separator is group");
    CHECK(bsg_type_is_a(bsg_switch_type(), bsg_group_type()), "switch is group");
    CHECK(bsg_type_is_a(bsg_geometry_type(), bsg_shape_type()), "geometry is shape");
    CHECK(!bsg_type_is_a(bsg_node_type(), bsg_group_type()), "node is not group");
    CHECK(!bsg_type_is_a(bsg_field_type(), bsg_node_type()), "field is not node");
    CHECK(!bsg_type_is_a(bsg_sensor_type(), bsg_node_type()), "sensor is not node");
    CHECK(!bsg_type_is_a(bsg_action_type(), bsg_node_type()), "action is not node");

    return 0;
}

static int
test_custom_registration(void)
{
    bsg_type_id custom = bsg_type_register("BSGTestCustomNode", bsg_node_type());
    CHECK(custom != BSG_TYPE_INVALID, "custom type registered");
    CHECK(BU_STR_EQUAL(bsg_type_name(custom), "BSGTestCustomNode"), "custom type name");
    CHECK(bsg_type_is_a(custom, bsg_node_type()), "custom type inherits node");
    CHECK(bsg_type_register("BSGTestCustomNode", bsg_node_type()) == custom, "duplicate custom type returns same id");
    CHECK(bsg_type_register("BSGTestCustomNode", bsg_type_object()) == BSG_TYPE_INVALID, "duplicate custom type with different parent fails");
    CHECK(bsg_type_register(NULL, bsg_node_type()) == BSG_TYPE_INVALID, "null custom type name fails");
    CHECK(bsg_type_register("", bsg_node_type()) == BSG_TYPE_INVALID, "empty custom type name fails");
    CHECK(bsg_type_register("BSGTestNoParent", BSG_TYPE_INVALID) == BSG_TYPE_INVALID, "invalid custom parent fails");
    return 0;
}

static int
test_null_refs(void)
{
    bsg_object_ref null_ref = bsg_object_ref_null();
    CHECK(bsg_object_ref_is_null(null_ref), "null object ref");
    CHECK(bsg_object_ref_equal(null_ref, bsg_object_ref_null()), "null refs equal");
    CHECK(bsg_object_type(null_ref) == BSG_TYPE_INVALID, "null object type");
    CHECK(bsg_object_ref_node(null_ref) == NULL, "null object has no node");
    bsg_object_destroy(null_ref);
    return 0;
}

static int
test_scene_ref_object_bridge(void)
{
    struct bsg_view *v = make_view();
    bsg_scene_ref group = bsg_scene_group_create(v, "group");
    bsg_scene_ref shape = bsg_scene_shape_create(v, "shape");
    bsg_scene_ref transform = bsg_scene_transform_create(v, NULL, "transform");
    bsg_scene_ref scope = bsg_scene_view_scope_create(v, "scope");
    bsg_scene_ref root = bsg_view_scene_ref_ensure(v);

    CHECK(!bsg_scene_ref_is_null(group), "create group");
    CHECK(!bsg_scene_ref_is_null(shape), "create shape");
    CHECK(!bsg_scene_ref_is_null(transform), "create transform");
    CHECK(!bsg_scene_ref_is_null(scope), "create view scope");
    CHECK(!bsg_scene_ref_is_null(root), "create root");

    bsg_object_ref group_object = bsg_object_ref_from_scene_ref(group);
    bsg_object_ref shape_object = bsg_object_ref_from_scene_ref(shape);
    bsg_object_ref transform_object = bsg_object_ref_from_scene_ref(transform);
    bsg_object_ref scope_object = bsg_object_ref_from_scene_ref(scope);
    bsg_object_ref root_object = bsg_object_ref_from_scene_ref(root);

    CHECK(bsg_type_is_a(bsg_object_type(group_object), bsg_group_type()), "group object type");
    CHECK(bsg_type_is_a(bsg_object_type(shape_object), bsg_shape_type()), "shape object type");
    CHECK(bsg_type_is_a(bsg_object_type(transform_object), bsg_transform_type()), "transform object type");
    CHECK(bsg_type_is_a(bsg_object_type(scope_object), bsg_view_scope_type()), "view scope object type");
    CHECK(bsg_type_is_a(bsg_object_type(root_object), bsg_group_type()), "root object type");

    CHECK(bsg_type_is_a(bsg_object_type(shape_object), bsg_shape_type()),
	    "object type is runtime metadata-backed");

    CHECK(bsg_scene_ref_equal(bsg_scene_ref_from_object_ref(group_object), group), "group object to scene ref");
    CHECK(bsg_scene_ref_equal(bsg_scene_ref_from_object_ref(shape_object), shape), "shape object to scene ref");
    CHECK(bsg_scene_ref_type(group) == BSG_SCENE_ELEMENT_GROUP, "group scene type");
    CHECK(bsg_scene_ref_type(shape) == BSG_SCENE_ELEMENT_SHAPE, "shape scene type");
    CHECK(bsg_scene_ref_type(transform) == BSG_SCENE_ELEMENT_TRANSFORM, "transform scene type");
    CHECK(bsg_scene_ref_type(scope) == BSG_SCENE_ELEMENT_VIEW_SCOPE, "scope scene type");
    CHECK(bsg_scene_ref_type(root) == BSG_SCENE_ELEMENT_GROUP, "root scene type");

    bsg_scene_ref_destroy(shape);
    bsg_scene_ref_destroy(transform);
    bsg_scene_ref_destroy(scope);
    bsg_scene_ref_destroy(group);
    free_view(v);
    return 0;
}

static int
test_object_destroy(void)
{
    struct bsg_view *v = make_view();
    bsg_scene_ref shape = bsg_scene_shape_create(v, "owned_shape");
    bsg_object_ref object = bsg_object_ref_from_scene_ref(shape);

    CHECK(!bsg_object_ref_is_null(object), "owned object ref");
    CHECK(bsg_type_is_a(bsg_object_type(object), bsg_shape_type()), "owned object type before destroy");
    bsg_object_destroy(object);

    free_view(v);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_builtin_types();
    failures += test_custom_registration();
    failures += test_null_refs();
    failures += test_scene_ref_object_bridge();
    failures += test_object_destroy();

    if (!failures)
	printf("PASS: test_object\n");
    return failures;
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
