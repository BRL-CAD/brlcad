/*                 T E S T _ P H A S E 6 . C
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
/** @file libbsg/tests/test_phase6.c
 *
 * Unit tests for field accessors, sensors, runtime types, and typed payload binding.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/node.h"
#include "bsg/sensor.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../payload_typed_private.h"
#include "../field_private.h"
#include "../object_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_view");
    return v;
}

static int
node_is_a(bsg_node *node, bsg_type_id type)
{
    if (!node)
	return 0;
    return bsg_node_is_a(bsg_node_ref_from_object(bsg_object_ref_from_node(node)),
	    type);
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test_view");
}


/* ------------------------------------------------------------------ */
/* Test 1: Field accessors (color, visible)                             */
/* ------------------------------------------------------------------ */

static int
test_field_accessor(void)
{
    printf("=== Test 1: field_accessor ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    bsg_node_set_color(root, 10, 20, 30);
    unsigned char r = 0, g = 0, b = 0;
    bsg_node_get_color(root, &r, &g, &b);
    if (r != 10 || g != 20 || b != 30) FAIL("color round-trip");

    bsg_node_set_visible(root, 1);
    if (!bsg_node_get_visible(root)) FAIL("set_visible(1)");

    bsg_node_set_visible(root, 0);
    if (bsg_node_get_visible(root)) FAIL("set_visible(0)");

    /* NULL guards */
    bsg_node_set_color(NULL, 1, 2, 3);
    bsg_node_set_visible(NULL, 1);
    bsg_node_get_color(NULL, &r, &g, &b);
    if (bsg_node_get_visible(NULL) != 0) FAIL("get_visible(NULL) != 0");

    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("field_accessor");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: FieldSensor fires on specific field touch                    */
/* ------------------------------------------------------------------ */

static int s_field_cb_count = 0;
static bsg_field_ref s_field_cb_last_field = BSG_FIELD_REF_NULL_INIT;

static int
field_sensor_cb(bsg_field_ref field, void *data)
{
    s_field_cb_count++;
    s_field_cb_last_field = field;
    if (data) *(int *)data = 1;
    return 0;
}

static int
test_field_touch_sensor(void)
{
    printf("=== Test 2: field_touch_sensor ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    int fired = 0;
    s_field_cb_count = 0;
    s_field_cb_last_field = bsg_field_ref_null();

    bsg_node_ref root_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(root));
    bsg_field_ref color_field = bsg_node_ref_color_field(root_ref);
    bsg_field_sensor_ref sensor = bsg_field_sensor_ref_create(color_field,
	    field_sensor_cb, &fired);
    if (bsg_sensor_ref_is_null(bsg_field_sensor_ref_as_sensor(sensor)))
	FAIL("bsg_field_sensor_ref_create returned NULL");

    /* Mutate a different field — should NOT fire the color sensor. */
    bsg_node_set_visible(root, 0);
    if (s_field_cb_count != 0) FAIL("sensor fired for wrong field");

    /* Touch the watched field — should fire */
    bsg_node_set_color(root, 12, 34, 56);
    if (s_field_cb_count != 1) FAIL("sensor did not fire for BSG_FIELD_COLOR");
    if (!bsg_field_ref_equal(s_field_cb_last_field, color_field)) FAIL("wrong field in callback");
    if (!fired) FAIL("data pointer not updated by callback");

    bsg_field_sensor_ref_destroy(sensor);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("field_touch_sensor");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: NodeSensor fires on any field touch                          */
/* ------------------------------------------------------------------ */

static int s_node_cb_count = 0;

static int
node_sensor_cb(bsg_node_ref UNUSED(node), bsg_field_ref UNUSED(changed_field), void *UNUSED(data))
{
    s_node_cb_count++;
    return 0;
}

static int
test_node_sensor(void)
{
    printf("=== Test 3: node_sensor ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    s_node_cb_count = 0;

    bsg_node_ref root_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(root));
    bsg_node_sensor_ref sensor = bsg_node_sensor_ref_create(root_ref, node_sensor_cb, NULL);
    if (bsg_sensor_ref_is_null(bsg_node_sensor_ref_as_sensor(sensor)))
	FAIL("bsg_node_sensor_ref_create returned NULL");

    bsg_node_set_visible(root, 0);
    bsg_node_set_color(root, 10, 20, 30);
    if (s_node_cb_count != 2)
	FAIL("NodeSensor did not fire twice");

    bsg_node_sensor_ref_destroy(sensor);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("node_sensor");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: Group node create / add / remove                             */
/* ------------------------------------------------------------------ */

static int
test_group_create_add_remove(void)
{
    printf("=== Test 4: group_create_add_remove ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    bsg_node *grp = bsg_group_create(v);
    if (!grp) FAIL("bsg_group_create returned NULL");

    if (!node_is_a(grp, bsg_group_type())) FAIL("runtime type not BSGGroup");

    bsg_node *child1 = bsg_group_create(v);
    bsg_node *child2 = bsg_group_create(v);
    if (!child1 || !child2) FAIL("child group_create returned NULL");

    bsg_group_add_child(grp, child1);
    bsg_group_add_child(grp, child2);
    if (bsg_node_child_count(grp) != 2) FAIL("expected 2 children after add");

    /* Adding the same child twice should be a no-op */
    bsg_group_add_child(grp, child1);
    if (bsg_node_child_count(grp) != 2) FAIL("duplicate add should be no-op");

    bsg_group_remove_child(grp, child1);
    if (bsg_node_child_count(grp) != 1) FAIL("expected 1 child after remove");

    bsg_group_destroy(child1);
    bsg_group_destroy(child2);
    bsg_group_destroy(grp);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("group_create_add_remove");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: Transform node matrix round-trip                             */
/* ------------------------------------------------------------------ */

static int
test_transform_matrix(void)
{
    printf("=== Test 5: transform_matrix ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    bsg_node *xf = bsg_transform_create(v);
    if (!xf) FAIL("bsg_transform_create returned NULL");

    if (!node_is_a(xf, bsg_transform_type()))
	FAIL("runtime type not BSGTransform");

    /* Verify identity matrix after create */
    mat_t got;
    bsg_transform_get_matrix(xf, got);
    mat_t ident;
    MAT_IDN(ident);
    if (memcmp(got, ident, sizeof(mat_t)) != 0)
	FAIL("initial matrix not identity");

    /* Set a non-identity matrix and read it back */
    mat_t set_mat;
    MAT_IDN(set_mat);
    set_mat[0] = 2.0; set_mat[5] = 3.0; set_mat[10] = 4.0;
    bsg_transform_set_matrix(xf, set_mat);
    bsg_transform_get_matrix(xf, got);
    if (memcmp(got, set_mat, sizeof(mat_t)) != 0)
	FAIL("set/get matrix round-trip failed");

    bsg_transform_destroy(xf);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("transform_matrix");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 6: Typed payload binding                                        */
/* ------------------------------------------------------------------ */

static int
test_payload_binding(void)
{
    printf("=== Test 6: payload_binding ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");

    bsg_node *shape = bsg_shape_create(v);
    if (!shape) FAIL("bsg_shape_create returned NULL");

    if (bsg_node_get_payload(shape) != NULL)
	FAIL("initial payload should be NULL");

    struct bsg_payload *pl = bsg_payload_create(BSG_PL_LINE_SET);
    if (!pl)
	FAIL("bsg_payload_create returned NULL");
    bsg_node_set_payload(shape, pl);
    if (bsg_node_get_payload(shape) != pl)
	FAIL("payload binding round-trip failed");
    if (bsg_node_get_payload(shape)->pl_type != BSG_PL_LINE_SET)
	FAIL("payload type should be BSG_PL_LINE_SET");

    bsg_payload_realize(NULL, NULL);        /* no-op, must not crash */
    bsg_payload_realize(bsg_node_get_payload(shape), v); /* empty line set: no crash */

    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("payload_binding");
    return 0;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_field_accessor();
    failures += test_field_touch_sensor();
    failures += test_node_sensor();
    failures += test_group_create_add_remove();
    failures += test_transform_matrix();
    failures += test_payload_binding();

    if (failures == 0)
	printf("RESULT: all field/sensor/runtime-type/payload tests PASSED\n");
    else
	printf("RESULT: %d test(s) FAILED\n", failures);

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
