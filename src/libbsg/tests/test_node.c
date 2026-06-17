/*                   T E S T _ N O D E . C
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
/** @file libbsg/tests/test_node.c
 *
 * Unit tests for the public generic bsg/node.h API.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/action.h"
#include "bsg/draw_source.h"
#include "../geometry_private.h"
#include "../draw_set_private.h"
#include "../draw_source_private.h"
#include "../draw_set_private.h"
#include "bsg/node.h"
#include "../object_private.h"
#include "../payload_typed_private.h"
#include "../scene_object_private.h"
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
    bu_vls_sprintf(&v->gv_name, "test_node_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test node view");
}

static int
test_lifecycle_and_kind(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_TRANSFORM);
    CHECK(n != NULL, "bsg_node_create");
    CHECK(bsg_node_ref_type(bsg_node_ref_from_object(bsg_object_ref_from_node(n))) ==
	    bsg_node_type(), "raw constructor publishes generic runtime type");
    CHECK(bsg_node_visible(n), "visible by default");

    mat_t got, ident;
    MAT_IDN(ident);
    bsg_node_transform(n, got);
    CHECK(memcmp(got, ident, sizeof(mat_t)) == 0, "default transform is identity");

    bsg_node_set_visible_state(n, 0);
    CHECK(!bsg_node_visible(n), "set invisible");
    bsg_scene_node_reset(n);
    CHECK(bsg_node_visible(n), "reset restores canonical visible state");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_name_user_bounds(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(n != NULL, "create shape");

    bsg_node_set_name(n, "nodeA");
    CHECK(BU_STR_EQUAL(bsg_node_name(n), "nodeA"), "name round trip");

    struct bsg_payload *pl = bsg_payload_create(BSG_PL_LINE_SET);
    CHECK(pl != NULL, "payload create");
    bsg_node_set_payload(n, pl);
    CHECK(bsg_node_get_payload(n) == pl, "payload round trip");

    point_t bmin, bmax, gmin, gmax;
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);
    bsg_node_set_bounds(n, bmin, bmax, 1);
    CHECK(bsg_node_bounds(n, gmin, gmax) == 1, "bounds valid");
    CHECK(VNEAR_EQUAL(gmin, bmin, SMALL_FASTF), "bounds min");
    CHECK(VNEAR_EQUAL(gmax, bmax, SMALL_FASTF), "bounds max");

    bsg_node_set_bounds(n, bmin, bmax, 0);
    CHECK(bsg_node_bounds(n, gmin, gmax) == 0, "bounds invalid");

    CHECK(bsg_node_get_view(n) == v, "initial view");
    bsg_node_set_view(n, NULL);
    CHECK(bsg_node_get_view(n) == NULL, "clear view");
    bsg_node_set_view(n, v);
    CHECK(bsg_node_get_view(n) == v, "set view");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_parent_child(void)
{
    struct bsg_view *v = make_view();
    bsg_node *p1 = bsg_node_create(v, BSG_NODE_GROUP);
    bsg_node *p2 = bsg_node_create(v, BSG_NODE_GROUP);
    bsg_node *c = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(p1 && p2 && c, "create parent/child");

    bsg_node_add_child(p1, c);
    CHECK(bsg_node_parent(c) == p1, "parent assigned");
    CHECK(bsg_node_child_count(p1) == 1, "child count");
    CHECK(bsg_node_child_at(p1, 0) == c, "child index");

    bsg_node_add_child(p2, c);
    CHECK(bsg_node_parent(c) == p2, "reparented");
    CHECK(bsg_node_child_count(p1) == 0, "old parent updated");
    CHECK(bsg_node_child_count(p2) == 1, "new parent count");

    bsg_node_remove_child(p2, c);
    CHECK(bsg_node_parent(c) == NULL, "parent cleared on remove");
    CHECK(bsg_node_child_count(p2) == 0, "remove child");

    bsg_node_destroy(c);
    bsg_node_destroy(p1);
    bsg_node_destroy(p2);
    free_view(v);
    return 0;
}

static int
test_update_bounds_uses_field_authority(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(n != NULL, "create shape for bounds update");

    point_t bmin, bmax;
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);
    bsg_node_set_bounds(n, bmin, bmax, 1);

    CHECK(bsg_scene_node_update_bounds(n, v), "update bounds from field");
    point_t cached_min, cached_max;
    CHECK(bsg_node_bbox_cache_get(n, cached_min, cached_max), "bbox cache set from field");
    CHECK(VNEAR_EQUAL(cached_min, bmin, SMALL_FASTF), "bbox cache min from field");
    CHECK(VNEAR_EQUAL(cached_max, bmax, SMALL_FASTF), "bbox cache max from field");
    vect_t center = VINIT_ZERO;
    bsg_node_get_draw_center(n, center);
    CHECK(NEAR_EQUAL(center[X], 1.5, SMALL_FASTF) &&
	    NEAR_EQUAL(center[Y], 1.5, SMALL_FASTF) &&
	    NEAR_EQUAL(center[Z], 1.5, SMALL_FASTF), "draw center metadata from field");
    CHECK(NEAR_EQUAL(bsg_node_draw_size(n), 9.0, SMALL_FASTF), "draw size metadata from field");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_update_bounds_uses_line_geometry(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_shape_create(v);
    CHECK(n != NULL, "create line geometry shape");

    point_t pts[3] = {
	{-4.0,  2.0,  1.0},
	{ 3.0, -5.0,  6.0},
	{ 1.0,  7.0, -2.0}
    };
    int cmds[3] = {
	BSG_GEOMETRY_LINE_MOVE,
	BSG_GEOMETRY_LINE_DRAW,
	BSG_GEOMETRY_LINE_DRAW
    };
    CHECK(bsg_geometry_node_set_line_set(n, (const point_t *)pts, cmds, 3),
	    "set typed line geometry");
    point_t stale_min, stale_max;
    VSET(stale_min, 100.0, 100.0, 100.0);
    VSET(stale_max, 101.0, 101.0, 101.0);
    bsg_node_set_bounds(n, stale_min, stale_max, 1);

    CHECK(bsg_scene_node_update_bounds(n, v),
	    "update bounds from line geometry");
    point_t got_min, got_max;
    CHECK(bsg_node_bounds(n, got_min, got_max),
	    "line geometry writes canonical bounds");
    CHECK(NEAR_EQUAL(got_min[X], -4.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Y], -5.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Z], -2.0, SMALL_FASTF),
	    "line geometry bounds min");
    CHECK(NEAR_EQUAL(got_max[X], 3.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Y], 7.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Z], 6.0, SMALL_FASTF),
	    "line geometry bounds max");

    vect_t center = VINIT_ZERO;
    bsg_node_get_draw_center(n, center);
    CHECK(NEAR_EQUAL(center[X], -0.5, SMALL_FASTF) &&
	    NEAR_EQUAL(center[Y], 1.0, SMALL_FASTF) &&
	    NEAR_EQUAL(center[Z], 2.0, SMALL_FASTF),
	    "line geometry draw center");
    CHECK(NEAR_EQUAL(bsg_node_draw_size(n), 12.0, SMALL_FASTF),
	    "line geometry draw size");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_subtree_bbox_uses_bounds_field(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(n != NULL, "create shape for subtree bbox");
    bsg_node_set_object_type(n, bsg_shape_type());

    point_t bmin, bmax;
    VSET(bmin, -1.0, -2.0, -3.0);
    VSET(bmax, 4.0, 5.0, 6.0);
    bsg_node_set_bounds(n, bmin, bmax, 1);

    vect_t got_min, got_max;
    CHECK(bsg_subtree_bbox(n, &got_min, &got_max, 1) == 0, "subtree bbox valid");
    CHECK(NEAR_EQUAL(got_min[X], -7.5, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Y], -7.5, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Z], -7.5, SMALL_FASTF), "subtree bbox min from field");
    CHECK(NEAR_EQUAL(got_max[X], 10.5, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Y], 10.5, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Z], 10.5, SMALL_FASTF), "subtree bbox max from field");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_subtree_bbox_caches_group_aggregate(void)
{
    struct bsg_view *v = make_view();
    bsg_node *group = bsg_group_create(v);
    bsg_node *a = bsg_shape_create(v);
    bsg_node *b = bsg_shape_create(v);
    CHECK(group != NULL && a != NULL && b != NULL,
	    "create group aggregate nodes");

    point_t amin, amax, bmin, bmax;
    VSET(amin, -1.0, -1.0, -1.0);
    VSET(amax,  1.0,  1.0,  1.0);
    VSET(bmin, 10.0, -2.0,  0.0);
    VSET(bmax, 12.0,  2.0,  3.0);
    bsg_node_set_bounds(a, amin, amax, 1);
    bsg_node_set_bounds(b, bmin, bmax, 1);
    bsg_node_add_child(group, a);
    bsg_node_add_child(group, b);

    vect_t got_min, got_max;
    CHECK(bsg_subtree_bbox(group, &got_min, &got_max, 0) == 0,
	    "group aggregate bbox valid");
    CHECK(NEAR_EQUAL(got_min[X], -2.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Y], -4.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_min[Z], -2.5, SMALL_FASTF),
	    "group aggregate bbox min");
    CHECK(NEAR_EQUAL(got_max[X], 15.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Y], 4.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[Z], 5.5, SMALL_FASTF),
	    "group aggregate bbox max");

    point_t cached_min, cached_max;
    CHECK(bsg_node_bbox_cache_get(group, cached_min, cached_max),
	    "group aggregate cached");
    CHECK(VNEAR_EQUAL(cached_min, got_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(cached_max, got_max, SMALL_FASTF),
	    "group aggregate cache matches computed bounds");

    VSET(bmin, -20.0, -1.0, -1.0);
    VSET(bmax, -18.0,  1.0,  1.0);
    bsg_node_set_bounds(b, bmin, bmax, 1);
    bsg_node_bbox_invalidate(b);
    CHECK(!bsg_node_bbox_cache_get(group, cached_min, cached_max),
	    "child invalidation clears group aggregate cache");
    CHECK(bsg_subtree_bbox(group, &got_min, &got_max, 0) == 0,
	    "group aggregate bbox recomputed");
    CHECK(NEAR_EQUAL(got_min[X], -21.0, SMALL_FASTF) &&
	    NEAR_EQUAL(got_max[X], 2.0, SMALL_FASTF),
	    "group aggregate reflects updated child bounds");

    bsg_node_destroy(group);
    free_view(v);
    return 0;
}

static int
test_unattached_transform_and_payload(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(n != NULL, "create unattached shape");

    struct bsg_payload *pl = bsg_payload_create(BSG_PL_LINE_SET);
    CHECK(pl != NULL, "payload create");
    bsg_node_set_payload(n, pl);
    CHECK(bsg_node_get_payload(n)->pl_type == BSG_PL_LINE_SET, "payload kind");

    mat_t mat;
    MAT_IDN(mat);
    mat[0] = 2.0;
    bsg_node_set_transform(n, mat);
    mat_t got;
    bsg_node_transform(n, got);
    CHECK(memcmp(got, mat, sizeof(mat_t)) == 0, "set_transform round trip");

    mat[MDX] = 5.0;
    bsg_node_set_draw_mat(n, mat);
    MAT_ZERO(got);
    bsg_node_get_draw_mat(n, got);
    CHECK(NEAR_EQUAL(got[MDX], 5.0, SMALL_FASTF), "draw matrix accessor uses canonical transform field");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_draw_extent_uses_metadata(void)
{
    struct bsg_view *v = make_view();
    bsg_node *n = bsg_node_create(v, BSG_NODE_SHAPE);
    CHECK(n != NULL, "create draw extent shape");

    vect_t got = VINIT_ZERO;
    vect_t zero = VINIT_ZERO;
    bsg_node_get_draw_center(n, got);
    CHECK(VNEAR_EQUAL(got, zero, SMALL_FASTF),
	    "unset draw center uses typed default");
    CHECK(NEAR_EQUAL(bsg_node_draw_size(n), 0.0, SMALL_FASTF),
	    "unset draw size uses typed default");

    vect_t center;
    VSET(center, 1.0, 2.0, 3.0);
    bsg_node_set_draw_center(n, center);
    bsg_node_set_draw_size(n, 4.0);
    bsg_node_get_draw_center(n, got);
    CHECK(VNEAR_EQUAL(got, center, SMALL_FASTF),
	    "draw center metadata stores canonical value");
    CHECK(NEAR_EQUAL(bsg_node_draw_size(n), 4.0, SMALL_FASTF),
	    "draw size metadata stores canonical value");

    bsg_node_destroy(n);
    free_view(v);
    return 0;
}

static int
test_collect_uses_runtime_type(void)
{
    struct bsg_view *v = make_view();
    bsg_node *root = bsg_node_create(v, BSG_NODE_GROUP);
    bsg_node *shape = bsg_node_create(v, BSG_NODE_SHAPE);
    bsg_node *raw_only = bsg_node_create(v, 0);
    CHECK(root != NULL && shape != NULL && raw_only != NULL, "collect setup");
    bsg_node_set_object_type(root, bsg_group_type());
    bsg_node_set_object_type(shape, bsg_shape_type());

    bsg_node_add_child(root, shape);
    bsg_node_add_child(root, raw_only);

    bsg_action_ref collect = bsg_collect_action_create(bsg_shape_type());
    CHECK(!bsg_action_ref_is_null(collect), "collect action created");
    bsg_node_ref root_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(root));
    CHECK(bsg_action_apply(collect, root_ref), "collect action applied");
    CHECK(bsg_collect_action_count(collect) == 1,
	    "collect action uses runtime type metadata only");
    CHECK(bsg_node_ref_equal(bsg_collect_action_get(collect, 0),
	    bsg_node_ref_from_object(bsg_object_ref_from_node(shape))),
	    "collect action returns the typed shape child");
    bsg_action_ref_destroy(collect);

    bsg_node_destroy(root);
    bsg_node_destroy(shape);
    bsg_node_destroy(raw_only);
    free_view(v);
    return 0;
}

static int
test_draw_set_group_fields_ignore_raw_mirrors(void)
{
    struct bsg_view *v = make_view();
    bsg_node *parent = bsg_node_create(v, BSG_NODE_GROUP);
    CHECK(parent != NULL, "create parent group");

    bsg_node *child = bsg_group_ensure_child(parent, v, "canonical-group", NULL);
    CHECK(child != NULL, "ensure child group");
    CHECK(BU_STR_EQUAL(bsg_node_name(child), "canonical-group"), "child field name");
    CHECK(bsg_node_visible(child), "child field visibility");

    CHECK(BU_STR_EQUAL(bsg_node_name(child), "canonical-group"),
	    "draw-set child keeps field-backed name");
    CHECK(bsg_node_visible(child), "draw-set child keeps field-backed visibility");
    CHECK(bsg_group_find_child(parent, "canonical-group") == child,
	    "draw-set lookup uses field-backed name and runtime group type");
    CHECK(bsg_group_find_child(parent, "raw-group") == NULL,
	    "draw-set lookup ignores unknown name");

    const char *path[] = {"canonical-group"};
    bsg_erase_nested_subpath(parent, path, 1, 0, NULL, NULL);
    CHECK(bsg_node_child_count(parent) == 0, "erase nested group by field-backed name");

    bsg_node_destroy(parent);
    free_view(v);
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_lifecycle_and_kind();
    failures += test_name_user_bounds();
    failures += test_parent_child();
    failures += test_update_bounds_uses_field_authority();
    failures += test_update_bounds_uses_line_geometry();
    failures += test_subtree_bbox_uses_bounds_field();
    failures += test_subtree_bbox_caches_group_aggregate();
    failures += test_unattached_transform_and_payload();
    failures += test_draw_extent_uses_metadata();
    failures += test_collect_uses_runtime_type();
    failures += test_draw_set_group_fields_ignore_raw_mirrors();

    if (!failures)
	printf("PASS: test_node\n");
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
