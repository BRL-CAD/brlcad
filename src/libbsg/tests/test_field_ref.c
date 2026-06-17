/*                  T E S T _ F I E L D _ R E F . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbsg/tests/test_field_ref.c
 *
 * Public field-ref API coverage.
 */

#include "common.h"

#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/draw_style.h"
#include "bsg/field.h"
#include "bsg/group.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/separator.h"
#include "bsg/switch.h"
#include "bsg/util.h"
#include "../field_private.h"
#include "../node_private.h"
#include "../object_private.h"

#define CHECK(_expr, _msg) do { if (!(_expr)) { bu_log("FAIL: %s\n", (_msg)); return 1; } } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "field_ref_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "field_ref_view");
}

static int
test_node_property_fields(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_node_ref root_node = bsg_separator_ref_as_node(root);
    const char *name = NULL;
    int visible = 0;
    unsigned char color[3] = {0, 0, 0};
    mat_t mat;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    int valid = 0;

    bsg_field_ref name_field = bsg_node_ref_name_field(root_node);
    CHECK(!bsg_field_ref_is_null(name_field), "name field exists");
    CHECK(BU_STR_EQUAL(bsg_field_name(name_field), "name"), "name field label");
    CHECK(bsg_field_ref_value_type(name_field) == BSG_FIELD_VALUE_STRING, "name field type");
    uint64_t rev0 = bsg_field_revision(name_field);
    CHECK(bsg_field_set_string(name_field, "field-root"), "set name field");
    CHECK(bsg_field_revision(name_field) == rev0 + 1, "name field revision bumped");
    CHECK(bsg_field_get_string(name_field, &name), "get name field");
    CHECK(name && BU_STR_EQUAL(name, "field-root"), "name field round trip");
    CHECK(BU_STR_EQUAL(bsg_node_ref_name(root_node), "field-root"), "name field mirrors node name");

    bsg_field_ref visibility = bsg_node_ref_visibility_field(root_node);
    CHECK(bsg_field_set_bool(visibility, 0), "set visibility field false");
    CHECK(bsg_field_get_bool(visibility, &visible) && !visible, "visibility field false round trip");
    CHECK(!bsg_node_ref_visible(root_node), "visibility field mirrors node visibility");
    bsg_node_ref_set_visible(root_node, 1);
    CHECK(bsg_field_get_bool(visibility, &visible) && visible, "node visibility setter updates field");

    bsg_field_ref color_field = bsg_node_ref_color_field(root_node);
    color[0] = 9;
    color[1] = 18;
    color[2] = 27;
    CHECK(bsg_field_set_color3(color_field, color), "set color field");
    color[0] = color[1] = color[2] = 0;
    CHECK(bsg_field_get_color3(color_field, color), "get color field");
    CHECK(color[0] == 9 && color[1] == 18 && color[2] == 27, "color field round trip");
    bsg_node_ref_color(root_node, &color[0], &color[1], &color[2]);
    CHECK(color[0] == 9 && color[1] == 18 && color[2] == 27, "color field mirrors node color");

    bsg_field_ref transform = bsg_node_ref_transform_field(root_node);
    MAT_IDN(mat);
    mat[MDX] = 5.0;
    CHECK(bsg_field_set_matrix(transform, mat), "set transform field");
    MAT_ZERO(mat);
    CHECK(bsg_field_get_matrix(transform, mat), "get transform field");
    CHECK(EQUAL(mat[MDX], 5.0), "transform field round trip");

    bsg_field_ref bounds = bsg_node_ref_bounds_field(root_node);
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 1.0, 2.0, 3.0);
    CHECK(bsg_field_set_bbox(bounds, bmin, bmax, 1), "set bounds field");
    VSETALL(bmin, 0.0);
    VSETALL(bmax, 0.0);
    CHECK(bsg_field_get_bbox(bounds, bmin, bmax, &valid), "get bounds field");
    CHECK(valid && EQUAL(bmin[X], -1.0) && EQUAL(bmax[Z], 3.0), "bounds field round trip");

    free_view(v);
    return 0;
}

static bsg_node *
raw_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(bsg_node_ref_object(ref));
}

static int
test_explicit_fields_ignore_stale_legacy_mirrors(void)
{
    struct bsg_view *v = make_view();
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    bsg_switch_ref sw = bsg_switch_ref_create(v, "switch");
    bsg_material_ref material = bsg_material_ref_create(v, "material");
    bsg_draw_style_ref style = bsg_draw_style_ref_create(v, "style");
    bsg_node_ref shape_node = bsg_shape_ref_as_node(shape);
    bsg_node_ref switch_node = bsg_switch_ref_as_node(sw);
    bsg_node_ref material_node = bsg_material_ref_as_node(material);
    bsg_node_ref style_node = bsg_draw_style_ref_as_node(style);
    bsg_node *raw_shape = raw_node_from_ref(shape_node);
    bsg_node *raw_switch = raw_node_from_ref(switch_node);
    bsg_node *raw_material = raw_node_from_ref(material_node);
    bsg_node *raw_style = raw_node_from_ref(style_node);
    const char *name = NULL;
    int visible = 1;
    unsigned char color[3] = {1, 2, 3};
    unsigned char got_color[3] = {0, 0, 0};
    mat_t mat;
    point_t bmin, bmax, got_min, got_max;
    int valid = 0;

    CHECK(raw_shape && raw_switch && raw_material && raw_style, "raw nodes available for mirror test");

    CHECK(bsg_field_set_string(bsg_node_ref_name_field(shape_node), "canonical-shape"),
	    "set canonical name field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_visibility_field(shape_node)), "force shape field sync");
    CHECK(bsg_field_get_string(bsg_node_ref_name_field(shape_node), &name) &&
	    name && BU_STR_EQUAL(name, "canonical-shape"), "name field keeps canonical value");

    CHECK(bsg_field_set_bool(bsg_node_ref_visibility_field(shape_node), 0),
	    "set canonical visibility field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(shape_node)), "force visibility field sync");
    CHECK(bsg_field_get_bool(bsg_node_ref_visibility_field(shape_node), &visible) && !visible,
	    "visibility field keeps canonical value");

    CHECK(bsg_field_set_color3(bsg_node_ref_color_field(shape_node), color),
	    "set canonical color field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(shape_node)), "force color field sync");
    CHECK(bsg_field_get_color3(bsg_node_ref_color_field(shape_node), got_color) &&
	    got_color[0] == color[0] && got_color[1] == color[1] && got_color[2] == color[2],
	    "color field keeps canonical value");

    MAT_IDN(mat);
    mat[MDX] = 7.0;
    CHECK(bsg_field_set_matrix(bsg_node_ref_transform_field(shape_node), mat),
	    "set canonical transform field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(shape_node)), "force transform field sync");
    MAT_ZERO(mat);
    CHECK(bsg_field_get_matrix(bsg_node_ref_transform_field(shape_node), mat) &&
	    EQUAL(mat[MDX], 7.0), "transform field keeps canonical value");

    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);
    CHECK(bsg_field_set_bbox(bsg_node_ref_bounds_field(shape_node), bmin, bmax, 1),
	    "set canonical bounds field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(shape_node)), "force bounds field sync");
    CHECK(bsg_field_get_bbox(bsg_node_ref_bounds_field(shape_node), got_min, got_max, &valid) &&
	    valid && VNEAR_EQUAL(got_min, bmin, SMALL_FASTF) &&
	    VNEAR_EQUAL(got_max, bmax, SMALL_FASTF), "bounds field keeps canonical value");

    CHECK(bsg_field_set_int(bsg_switch_ref_which_child_field(sw), 2), "set canonical switch field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(switch_node)), "force switch field sync");
    CHECK(bsg_switch_ref_which_child(sw) == 2, "switch field keeps canonical value");

    CHECK(bsg_material_ref_set_alpha(material, 0.25), "set canonical material alpha field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(material_node)), "force material field sync");
    CHECK(NEAR_ZERO(bsg_material_ref_alpha(material) - 0.25, SMALL_FASTF),
	    "material alpha field keeps canonical value");

    CHECK(bsg_draw_style_ref_set_line_width(style, 7), "set canonical line width field");
    CHECK(bsg_draw_style_ref_set_line_style(style, 3), "set canonical line style field");
    CHECK(bsg_draw_style_ref_set_fill_mode(style, 2), "set canonical fill mode field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(style_node)), "force draw-style field sync");
    CHECK(bsg_draw_style_ref_line_width(style) == 7 &&
	    bsg_draw_style_ref_line_style(style) == 3 &&
	    bsg_draw_style_ref_fill_mode(style) == 2,
	    "draw-style fields ignore stale legacy mirrors");

    bsg_node_ref_destroy(style_node);
    bsg_node_ref_destroy(material_node);
    bsg_node_ref_destroy(switch_node);
    bsg_node_ref_destroy(shape_node);
    free_view(v);
    return 0;
}

static int
test_unset_fields_ignore_raw_legacy_mirrors(void)
{
    struct bsg_view *v = make_view();
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    bsg_switch_ref sw = bsg_switch_ref_create(v, "switch");
    bsg_material_ref material = bsg_material_ref_create(v, "material");
    bsg_node_ref shape_node = bsg_shape_ref_as_node(shape);
    bsg_node_ref switch_node = bsg_switch_ref_as_node(sw);
    bsg_node_ref material_node = bsg_material_ref_as_node(material);
    bsg_node *raw_shape = raw_node_from_ref(shape_node);
    bsg_node *raw_switch = raw_node_from_ref(switch_node);
    bsg_node *raw_material = raw_node_from_ref(material_node);
    const char *name = NULL;
    unsigned char color[3] = {0, 0, 0};
    point_t got_min = VINIT_ZERO;
    point_t got_max = VINIT_ZERO;
    mat_t got, ident;
    int valid = 1;

    CHECK(raw_shape && raw_switch && raw_material, "raw nodes available for unset mirror test");

    bsg_node_fields_reset(raw_shape);
    bsg_node_fields_reset(raw_switch);
    bsg_node_fields_reset(raw_material);

    CHECK(bsg_field_get_string(bsg_node_ref_name_field(shape_node), &name) &&
	    name && BU_STR_EQUAL(name, ""), "unset name field uses typed default");
    CHECK(bsg_field_get_color3(bsg_node_ref_color_field(shape_node), color) &&
	    color[0] == 255 && color[1] == 0 && color[2] == 0,
	    "unset color field uses typed default");
    MAT_ZERO(got);
    MAT_IDN(ident);
    CHECK(bsg_field_get_matrix(bsg_node_ref_transform_field(shape_node), got) &&
	    memcmp(got, ident, sizeof(mat_t)) == 0,
	    "unset transform field uses typed default");
    CHECK(bsg_field_get_bbox(bsg_node_ref_bounds_field(shape_node), got_min, got_max, &valid) &&
	    !valid && NEAR_ZERO(got_min[X], SMALL_FASTF) && NEAR_ZERO(got_max[Z], SMALL_FASTF),
	    "unset bounds field uses typed default");
    CHECK(bsg_switch_ref_which_child(sw) == BSG_SWITCH_NONE,
	    "unset switch field uses typed default");
    CHECK(bsg_material_ref_diffuse_color(material, color) &&
	    color[0] == 255 && color[1] == 0 && color[2] == 0,
	    "unset material diffuse field uses typed default");
    CHECK(bsg_material_ref_line_color(material, color) &&
	    color[0] == 255 && color[1] == 0 && color[2] == 0,
	    "unset material line field uses typed default");

    bsg_node_ref_destroy(material_node);
    bsg_node_ref_destroy(switch_node);
    bsg_node_ref_destroy(shape_node);
    free_view(v);
    return 0;
}

static int
test_material_color_fields_are_independent(void)
{
    struct bsg_view *v = make_view();
    bsg_material_ref material = bsg_material_ref_create(v, "material");
    bsg_node_ref material_node = bsg_material_ref_as_node(material);
    unsigned char diffuse[3] = {200, 40, 20};
    unsigned char line[3] = {10, 220, 30};
    unsigned char got_diffuse[3] = {0, 0, 0};
    unsigned char got_line[3] = {0, 0, 0};
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;

    CHECK(bsg_material_ref_set_diffuse_color(material, diffuse), "set material diffuse field");
    CHECK(bsg_material_ref_set_line_color(material, line), "set material line field");
    CHECK(!bsg_field_ref_is_null(bsg_node_ref_name_field(material_node)), "force field lookup");
    CHECK(bsg_material_ref_diffuse_color(material, got_diffuse), "get material diffuse field");
    CHECK(bsg_material_ref_line_color(material, got_line), "get material line field");
    CHECK(got_diffuse[0] == diffuse[0] && got_diffuse[1] == diffuse[1] && got_diffuse[2] == diffuse[2],
	    "diffuse field keeps independent value");
    CHECK(got_line[0] == line[0] && got_line[1] == line[1] && got_line[2] == line[2],
	    "line field keeps independent value");
    bsg_node_ref_color(material_node, &r, &g, &b);
    CHECK(r == diffuse[0] && g == diffuse[1] && b == diffuse[2],
	    "legacy node color bridge follows diffuse field");

    bsg_node_ref_destroy(material_node);
    free_view(v);
    return 0;
}

static int
test_connections_and_children(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_switch_ref sw_a = bsg_switch_ref_create(v, "switch-a");
    bsg_switch_ref sw_b = bsg_switch_ref_create(v, "switch-b");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "shape");
    int which_child = 0;

    bsg_field_ref children = bsg_group_ref_children_field(bsg_separator_ref_as_group(root));
    CHECK(!bsg_field_ref_is_null(children), "children field exists");
    CHECK(bsg_field_ref_value_type(children) == BSG_FIELD_VALUE_MULTI_NODE, "children field type");
    CHECK(bsg_field_multi_count(children) == 0, "initial child count");
    CHECK(bsg_field_multi_node_append(children, bsg_switch_ref_as_node(sw_a)), "append switch through children field");
    CHECK(bsg_field_multi_count(children) == 1, "child count after append");

    bsg_node_ref child = BSG_NODE_REF_NULL_INIT;
    CHECK(bsg_field_multi_node_at(children, 0, &child), "children field child lookup");
    CHECK(bsg_node_ref_equal(child, bsg_switch_ref_as_node(sw_a)), "children field child identity");

    bsg_switch_ref_append_child(sw_a, bsg_shape_ref_as_node(shape));
    bsg_field_ref which_a = bsg_switch_ref_which_child_field(sw_a);
    bsg_field_ref which_b = bsg_switch_ref_which_child_field(sw_b);
    CHECK(bsg_field_set_int(which_a, 0), "set switch field");
    CHECK(bsg_field_connect(which_b, which_a), "connect compatible switch fields");
    CHECK(bsg_field_get_int(which_b, &which_child) && which_child == 0, "connected switch field reads source");
    CHECK(bsg_switch_ref_which_child(sw_a) == 0, "switch field mirrors source switch");
    CHECK(bsg_switch_ref_which_child(sw_b) == 0, "switch accessor follows connected field source");
    CHECK(bsg_field_set_int(which_a, BSG_SWITCH_ALL), "update connected switch source");
    CHECK(bsg_field_get_int(which_b, &which_child) && which_child == BSG_SWITCH_ALL,
	    "connected switch field tracks source changes");
    CHECK(bsg_switch_ref_which_child(sw_b) == BSG_SWITCH_ALL,
	    "connected switch accessor tracks source changes");
    CHECK(!bsg_field_connect(which_a, bsg_node_ref_name_field(bsg_switch_ref_as_node(sw_a))),
	    "connection rejects mismatched field types");
    CHECK(bsg_field_disconnect(which_b), "disconnect field");

    CHECK(bsg_field_multi_node_remove(children, bsg_switch_ref_as_node(sw_a)), "remove switch through children field");
    CHECK(bsg_field_multi_count(children) == 0, "child count after remove");

    bsg_node_ref_destroy(bsg_shape_ref_as_node(shape));
    bsg_node_ref_destroy(bsg_switch_ref_as_node(sw_b));
    bsg_node_ref_destroy(bsg_switch_ref_as_node(sw_a));
    free_view(v);
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    if (test_node_property_fields())
	return 1;
    if (test_explicit_fields_ignore_stale_legacy_mirrors())
	return 1;
    if (test_unset_fields_ignore_raw_legacy_mirrors())
	return 1;
    if (test_material_color_fields_are_independent())
	return 1;
    if (test_connections_and_children())
	return 1;
    return 0;
}
