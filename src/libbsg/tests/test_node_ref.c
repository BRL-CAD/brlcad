/*                  T E S T _ N O D E _ R E F . C
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
/** @file libbsg/tests/test_node_ref.c
 *
 * Unit tests for public typed BSG node refs.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/group.h"
#include "bsg/node.h"
#include "bsg/separator.h"
#include "bsg/switch.h"
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
    bu_vls_sprintf(&v->gv_name, "test_node_ref_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test node ref view");
}

static int
test_constructors_and_casts(void)
{
    struct bsg_view *v = make_view();
    bsg_group_ref group = bsg_group_ref_create(v, "group");
    bsg_separator_ref sep = bsg_separator_ref_create(v, "separator");
    bsg_switch_ref sw = bsg_switch_ref_create(v, "switch");
    bsg_lod_ref lod = bsg_lod_ref_create(v, "lod");
    bsg_transform_ref xf = bsg_transform_ref_create(v, NULL, "xf");
    bsg_material_ref mat = bsg_material_ref_create(v, "mat");
    bsg_draw_style_ref style = bsg_draw_style_ref_create(v, "style");
    bsg_complexity_ref complexity = bsg_complexity_ref_create(v, "complexity");
    bsg_camera_ref camera = bsg_camera_ref_create(v, "camera");
    bsg_light_ref light = bsg_light_ref_create(v, "light");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    bsg_geometry_ref geom = bsg_geometry_ref_create(v, "geom");

    CHECK(bsg_node_is_a(bsg_group_ref_as_node(group), bsg_group_type()), "group type");
    CHECK(bsg_node_is_a(bsg_separator_ref_as_node(sep), bsg_separator_type()), "separator type");
    CHECK(bsg_node_is_a(bsg_switch_ref_as_node(sw), bsg_switch_type()), "switch type");
    CHECK(bsg_node_is_a(bsg_lod_ref_as_node(lod), bsg_lod_type()), "lod type");
    CHECK(bsg_node_is_a(bsg_transform_ref_as_node(xf), bsg_transform_type()), "transform type");
    CHECK(bsg_node_is_a(bsg_material_ref_as_node(mat), bsg_material_type()), "material type");
    CHECK(bsg_node_is_a(bsg_draw_style_ref_as_node(style), bsg_draw_style_type()), "draw style type");
    CHECK(bsg_node_is_a(bsg_complexity_ref_as_node(complexity), bsg_complexity_type()), "complexity type");
    CHECK(bsg_node_is_a(bsg_camera_ref_as_node(camera), bsg_camera_type()), "camera type");
    CHECK(bsg_node_is_a(bsg_light_ref_as_node(light), bsg_light_type()), "light type");
    CHECK(bsg_node_is_a(bsg_shape_ref_as_node(shape), bsg_shape_type()), "shape type");
    CHECK(bsg_node_is_a(bsg_geometry_ref_as_node(geom), bsg_geometry_type()), "geometry type");

    CHECK(!bsg_node_ref_is_null(bsg_group_ref_as_node(bsg_node_ref_as_group(bsg_separator_ref_as_node(sep)))), "separator downcasts to group");
    CHECK(bsg_node_ref_is_null(bsg_group_ref_as_node(bsg_node_ref_as_group(bsg_shape_ref_as_node(shape)))), "shape does not downcast to group");
    CHECK(!bsg_node_ref_is_null(bsg_shape_ref_as_node(bsg_node_ref_as_shape(bsg_geometry_ref_as_node(geom)))), "geometry downcasts to shape");
    CHECK(bsg_node_ref_is_null(bsg_geometry_ref_as_node(bsg_node_ref_as_geometry(bsg_shape_ref_as_node(shape)))), "plain shape does not downcast to geometry");

    bsg_node_ref_destroy(bsg_geometry_ref_as_node(geom));
    bsg_node_ref_destroy(bsg_shape_ref_as_node(shape));
    bsg_node_ref_destroy(bsg_light_ref_as_node(light));
    bsg_node_ref_destroy(bsg_camera_ref_as_node(camera));
    bsg_node_ref_destroy(bsg_complexity_ref_as_node(complexity));
    bsg_node_ref_destroy(bsg_draw_style_ref_as_node(style));
    bsg_node_ref_destroy(bsg_material_ref_as_node(mat));
    bsg_node_ref_destroy(bsg_transform_ref_as_node(xf));
    bsg_node_ref_destroy(bsg_lod_ref_as_node(lod));
    bsg_node_ref_destroy(bsg_switch_ref_as_node(sw));
    bsg_node_ref_destroy(bsg_separator_ref_as_node(sep));
    bsg_node_ref_destroy(bsg_group_ref_as_node(group));
    free_view(v);
    return 0;
}

static int
test_node_state_and_children(void)
{
    struct bsg_view *v = make_view();
    bsg_group_ref group = bsg_group_ref_create(v, "parent");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "child");
    bsg_node_ref shape_node = bsg_shape_ref_as_node(shape);
    mat_t mat, got;
    point_t bmin, bmax, gmin, gmax;

    MAT_IDN(mat);
    mat[5] = 3.0;
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);

    CHECK(BU_STR_EQUAL(bsg_node_ref_name(shape_node), "child"), "node name");
    bsg_node_ref_set_name(shape_node, "renamed");
    CHECK(BU_STR_EQUAL(bsg_node_ref_name(shape_node), "renamed"), "renamed node");

    bsg_node_ref_set_visible(shape_node, 0);
    CHECK(!bsg_node_ref_visible(shape_node), "visible off");
    bsg_node_ref_set_visible(shape_node, 1);
    CHECK(bsg_node_ref_visible(shape_node), "visible on");

    bsg_node_ref_set_color(shape_node, 3, 4, 5);
    unsigned char r = 0, g = 0, b = 0;
    bsg_node_ref_color(shape_node, &r, &g, &b);
    CHECK(r == 3 && g == 4 && b == 5, "color round trip");

    bsg_node_ref_set_transform(shape_node, mat);
    bsg_node_ref_transform(shape_node, got);
    CHECK(memcmp(got, mat, sizeof(mat_t)) == 0, "transform round trip");

    bsg_node_ref_set_bounds(shape_node, bmin, bmax, 1);
    CHECK(bsg_node_ref_bounds(shape_node, gmin, gmax) == 1, "bounds valid");
    CHECK(VNEAR_EQUAL(gmin, bmin, SMALL_FASTF), "bounds min");
    CHECK(VNEAR_EQUAL(gmax, bmax, SMALL_FASTF), "bounds max");

    bsg_group_ref_append_child(group, shape_node);
    CHECK(bsg_node_ref_equal(bsg_node_ref_parent(shape_node), bsg_group_ref_as_node(group)), "parent assigned");
    CHECK(bsg_group_ref_child_count(group) == 1, "child count");
    CHECK(bsg_node_ref_equal(bsg_group_ref_child_at(group, 0), shape_node), "child lookup");
    bsg_group_ref_remove_child(group, shape_node);
    CHECK(bsg_node_ref_is_null(bsg_node_ref_parent(shape_node)), "parent removed");

    bsg_node_ref_destroy(shape_node);
    bsg_node_ref_destroy(bsg_group_ref_as_node(group));
    free_view(v);
    return 0;
}

static int
test_parent_destroy_detaches_independently_owned_child(void)
{
    struct bsg_view *v = make_view();
    bsg_group_ref group = bsg_group_ref_create(v, "parent");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "child");
    bsg_node_ref shape_node = bsg_shape_ref_as_node(shape);

    bsg_group_ref_append_child(group, shape_node);
    CHECK(bsg_node_ref_equal(bsg_node_ref_parent(shape_node), bsg_group_ref_as_node(group)), "parent assigned");

    bsg_node_ref_destroy(bsg_group_ref_as_node(group));
    CHECK(bsg_node_ref_is_null(bsg_node_ref_parent(shape_node)), "parent destroy detaches child");
    CHECK(BU_STR_EQUAL(bsg_node_ref_name(shape_node), "child"), "child remains valid after parent destroy");

    bsg_node_ref_destroy(shape_node);
    free_view(v);
    return 0;
}

static int
test_group_separator_switch_children(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref sep = bsg_separator_ref_create(v, "sep");
    bsg_switch_ref sw = bsg_switch_ref_create(v, "sw");
    bsg_shape_ref a = bsg_shape_ref_create(v, "a");
    bsg_shape_ref b = bsg_shape_ref_create(v, "b");
    bsg_node_ref a_node = bsg_shape_ref_as_node(a);
    bsg_node_ref b_node = bsg_shape_ref_as_node(b);

    bsg_separator_ref_append_child(sep, bsg_switch_ref_as_node(sw));
    CHECK(bsg_separator_ref_child_count(sep) == 1, "separator child count");
    CHECK(bsg_node_ref_equal(bsg_separator_ref_child_at(sep, 0), bsg_switch_ref_as_node(sw)), "separator child lookup");

    CHECK(bsg_switch_ref_which_child(sw) == BSG_SWITCH_NONE, "switch defaults to none");
    bsg_switch_ref_append_child(sw, a_node);
    bsg_switch_ref_append_child(sw, b_node);
    CHECK(bsg_switch_ref_child_count(sw) == 2, "switch child count");
    CHECK(bsg_node_ref_equal(bsg_switch_ref_child_at(sw, 1), b_node), "switch child lookup");

    bsg_switch_ref_set_which_child(sw, BSG_SWITCH_ALL);
    CHECK(bsg_switch_ref_which_child(sw) == BSG_SWITCH_ALL, "switch all");
    bsg_switch_ref_set_which_child(sw, 1);
    CHECK(bsg_switch_ref_which_child(sw) == 1, "switch single child");
    bsg_switch_ref_set_which_child(sw, BSG_SWITCH_NONE);
    CHECK(bsg_switch_ref_which_child(sw) == BSG_SWITCH_NONE, "switch none");

    bsg_switch_ref_remove_child(sw, a_node);
    CHECK(bsg_switch_ref_child_count(sw) == 1, "switch remove child");

    bsg_node_ref_destroy(a_node);
    bsg_node_ref_destroy(b_node);
    bsg_node_ref_destroy(bsg_switch_ref_as_node(sw));
    bsg_node_ref_destroy(bsg_separator_ref_as_node(sep));
    free_view(v);
    return 0;
}

static int
test_root_separator(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_node_ref root_node = bsg_separator_ref_as_node(root);

    CHECK(!bsg_node_ref_is_null(root_node), "root separator created");
    CHECK(bsg_node_is_a(root_node, bsg_separator_type()), "root is separator");
    CHECK(bsg_node_is_a(root_node, bsg_group_type()), "root is group-compatible");
    free_view(v);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_constructors_and_casts();
    failures += test_node_state_and_children();
    failures += test_parent_destroy_detaches_independently_owned_child();
    failures += test_group_separator_switch_children();
    failures += test_root_separator();

    if (!failures)
	printf("PASS: test_node_ref\n");
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
