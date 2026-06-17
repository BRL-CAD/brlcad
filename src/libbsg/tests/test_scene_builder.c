/*             T E S T _ S C E N E _ B U I L D E R . C
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
/** @file libbsg/tests/test_scene_builder.c
 *
 * Unit tests for the typed scene construction API.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "../payload_typed_private.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"

#define CHECK(_expr, _msg) \
    do { \
	if (!(_expr)) { \
	    printf("FAIL: %s\n", (_msg)); \
	    return 1; \
	} \
    } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v = NULL;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_scene_builder_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test scene builder view");
}

static int
test_typed_create_and_state(void)
{
    struct bsg_view *v = make_view();
    mat_t mat, got;
    point_t bmin, bmax, gmin, gmax;

    MAT_IDN(mat);
    mat[0] = 2.0;
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);

    bsg_scene_ref group = bsg_scene_group_create(v, "group");
    bsg_scene_ref shape = bsg_scene_shape_create(v, "shape");
    bsg_scene_ref transform = bsg_scene_transform_create(v, mat, "xform");
    CHECK(!bsg_scene_ref_is_null(group), "create group");
    CHECK(!bsg_scene_ref_is_null(shape), "create shape");
    CHECK(!bsg_scene_ref_is_null(transform), "create transform");
    CHECK(bsg_scene_ref_type(group) == BSG_SCENE_ELEMENT_GROUP, "group type");
    CHECK(bsg_scene_ref_type(shape) == BSG_SCENE_ELEMENT_SHAPE, "shape type");
    CHECK(bsg_scene_ref_type(transform) == BSG_SCENE_ELEMENT_TRANSFORM, "transform type");
    CHECK(BU_STR_EQUAL(bsg_scene_name(shape), "shape"), "shape name");

    bsg_scene_transform(transform, got);
    CHECK(memcmp(got, mat, sizeof(mat_t)) == 0, "transform round trip");

    bsg_scene_set_visible(shape, 0);
    CHECK(!bsg_scene_visible(shape), "visible off");
    bsg_scene_set_visible(shape, 1);
    CHECK(bsg_scene_visible(shape), "visible on");

    bsg_scene_set_bounds(shape, bmin, bmax, 1);
    CHECK(bsg_scene_bounds(shape, gmin, gmax) == 1, "bounds valid");
    CHECK(VNEAR_EQUAL(gmin, bmin, SMALL_FASTF), "bounds min");
    CHECK(VNEAR_EQUAL(gmax, bmax, SMALL_FASTF), "bounds max");

    bsg_scene_ref_destroy(shape);
    bsg_scene_ref_destroy(transform);
    bsg_scene_ref_destroy(group);
    free_view(v);
    return 0;
}

static int
test_payload_binding(void)
{
    struct bsg_view *v = make_view();
    bsg_scene_ref shape = bsg_scene_shape_create(v, "payload_shape");
    CHECK(!bsg_scene_ref_is_null(shape), "create payload shape");

    struct bsg_payload *pl = bsg_payload_line_set_create(NULL, NULL, 0);
    CHECK(pl != NULL, "create line-set payload");
    bsg_scene_set_payload(shape, pl);
    CHECK(bsg_scene_payload(shape) == pl, "payload round trip");
    CHECK(pl->pl_owner != NULL, "payload owner assigned");

    bsg_scene_set_payload(shape, NULL);
    CHECK(bsg_scene_payload(shape) == NULL, "payload cleared");

    bsg_scene_ref_destroy(shape);
    free_view(v);
    return 0;
}

static int
test_root_and_color_refs(void)
{
    struct bsg_view *v = make_view();
    bsg_scene_ref root = bsg_view_scene_ref_ensure(v);
    CHECK(!bsg_scene_ref_is_null(root), "create scene root ref");
    CHECK(bsg_scene_ref_type(root) == BSG_SCENE_ELEMENT_GROUP, "root type");

    bsg_scene_set_color(root, 10, 20, 30);
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    bsg_scene_color(root, &r, &g, &b);
    CHECK(r == 10 && g == 20 && b == 30, "color round trip");

    free_view(v);
    return 0;
}

static int
test_parent_child_refs(void)
{
    struct bsg_view *v = make_view();
    bsg_scene_ref parent = bsg_scene_group_create(v, "parent");
    bsg_scene_ref other = bsg_scene_group_create(v, "other");
    bsg_scene_ref child = bsg_scene_shape_create(v, "child");

    CHECK(!bsg_scene_ref_is_null(parent), "create parent");
    CHECK(!bsg_scene_ref_is_null(other), "create other");
    CHECK(!bsg_scene_ref_is_null(child), "create child");

    bsg_scene_append_child(parent, child);
    CHECK(bsg_scene_ref_equal(bsg_scene_parent(child), parent), "parent assigned");
    CHECK(bsg_scene_child_count(parent) == 1, "child count");
    CHECK(bsg_scene_ref_equal(bsg_scene_child_at(parent, 0), child), "child index");
    CHECK(bsg_scene_ref_equal(bsg_scene_find_child(parent, "child"), child), "child find");

    bsg_scene_append_child(other, child);
    CHECK(bsg_scene_ref_equal(bsg_scene_parent(child), other), "reparented");
    CHECK(bsg_scene_child_count(parent) == 0, "old parent updated");
    CHECK(bsg_scene_child_count(other) == 1, "new parent count");

    bsg_scene_remove_child(other, child);
    CHECK(bsg_scene_ref_is_null(bsg_scene_parent(child)), "parent cleared");
    CHECK(bsg_scene_child_count(other) == 0, "remove child");

    bsg_scene_ref_destroy(child);
    bsg_scene_ref_destroy(parent);
    bsg_scene_ref_destroy(other);
    free_view(v);
    return 0;
}

static int
test_ref_bridges_and_scene_ops(void)
{
    struct bsg_view *v = make_view();
    point_t bmin, bmax;
    VSET(bmin, -2.0, -2.0, -2.0);
    VSET(bmax, 2.0, 2.0, 2.0);

    bsg_scene_ref scene = bsg_scene_shape_create(v, "shape_ref");
    CHECK(!bsg_scene_ref_is_null(scene), "scene ref");
    CHECK(bsg_scene_view(scene) == v, "scene view");

    bsg_scene_set_bounds(scene, bmin, bmax, 1);
    (void)bsg_scene_update_bounds(scene, v);
    bsg_scene_invalidate(scene);
    CHECK(bsg_scene_view_depth(scene, v, 0) >= 0.0, "scene view depth");

    bsg_scene_ref_destroy(scene);
    free_view(v);
    return 0;
}

static int
test_null_guards(void)
{
    bsg_scene_ref ref = bsg_scene_ref_null();
    mat_t mat;
    point_t bmin, bmax;

    MAT_IDN(mat);
    CHECK(bsg_scene_ref_is_null(ref), "null ref");
    CHECK(bsg_scene_ref_type(ref) == BSG_SCENE_ELEMENT_NONE, "null type");
    CHECK(!bsg_scene_visible(ref), "null visible");
    CHECK(bsg_scene_child_count(ref) == 0, "null child count");
    CHECK(bsg_scene_bounds(ref, bmin, bmax) == 0, "null bounds");
    bsg_scene_set_name(ref, "ignored");
    bsg_scene_set_visible(ref, 1);
    bsg_scene_set_transform(ref, mat);
    bsg_scene_ref_destroy(ref);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_typed_create_and_state();
    failures += test_payload_binding();
    failures += test_root_and_color_refs();
    failures += test_parent_child_refs();
    failures += test_ref_bridges_and_scene_ops();
    failures += test_null_guards();

    if (!failures)
	printf("PASS: test_scene_builder\n");
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
