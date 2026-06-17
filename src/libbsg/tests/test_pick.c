/*               T E S T _ P I C K . C
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
/** @file libbsg/tests/test_pick.c
 *
 * Phase D3 unit tests: typed pick records and pick actions.
 *
 * Tests that do not require a real view use NULL-view guards and
 * result-lifecycle checks.  The scene-pick tests build a minimal
 * bsg_view with a flat draw root and shape nodes to exercise
 * bsg_pick_point(), bsg_pick_rect(), bsg_pick_nearest(), and the
 * bsg_pick_apply() helpers.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bsg/defines.h"
#include "bsg/draw_intent.h"
#include "../draw_intent_private.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/interaction.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../payload_typed_private.h"
#include "bsg/pick.h"
#include "bsg/selection.h"
#include "bsg/separator.h"
#include "bsg/util.h"
#include "bsg/view_state.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)


/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "pick_test_view");
    /* Set a non-zero viewport so view pick-candidate guards pass. */
    v->gv_width  = 512;
    v->gv_height = 512;
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "pick_test_view");
}

static int
append_record(struct bsg_pick_result *res, bsg_node *node, struct bsg_view *v,
	int sx, int sy, const char *path, fastf_t hit_dist)
{
    if (!res || !node)
	return 0;

    struct bsg_pick_record *pr;
    BU_GET(pr, struct bsg_pick_record);
    bu_vls_init(&pr->pr_source_path);
    bu_vls_init(&pr->pr_instance_path);
    pr->pr_scene.opaque = node;
    pr->pr_feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    pr->pr_valid = 1;
    pr->pr_view = v;
    pr->pr_screen_x = sx;
    pr->pr_screen_y = sy;
    pr->pr_hit_dist = hit_dist;
    pr->pr_primitive_id = -1;
    pr->pr_subelement_id = -1;
    bu_vls_sprintf(&pr->pr_source_path, "%s", path ? path : bsg_node_name(node));
    bu_vls_sprintf(&pr->pr_instance_path, "%s", bu_vls_cstr(&pr->pr_source_path));
    if (bu_vls_strlen(&pr->pr_source_path) == 0)
	pr->pr_valid = 0;
    bu_ptbl_ins(&res->pr_records, (long *)pr);
    return 1;
}


/* -----------------------------------------------------------------------
 * Test 1: bsg_pick_result lifecycle — no view
 * ----------------------------------------------------------------------- */

static int
test_result_lifecycle(void)
{
    printf("=== Test 1: pick_result lifecycle ===\n");

    struct bsg_pick_result *res = bsg_pick_result_create();
    if (!res) FAIL("bsg_pick_result_create returned NULL");
    if (bsg_pick_result_count(res) != 0) FAIL("initial count != 0");
    if (bsg_pick_result_get(res, 0) != NULL) FAIL("get(0) on empty != NULL");

    bsg_pick_result_free(res);
    bsg_pick_result_free(NULL); /* must not crash */

    PASS("pick_result lifecycle");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 2: bsg_pick_point with NULL view returns empty result
 * ----------------------------------------------------------------------- */

static int
test_pick_point_null_view(void)
{
    printf("=== Test 2: pick_point NULL view ===\n");

    struct bsg_pick_result *res = bsg_pick_point(NULL, 100, 100, 0);
    if (res) FAIL("pick_point(NULL) should return NULL");

    PASS("pick_point NULL view");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 3: bsg_pick_rect with NULL view returns NULL
 * ----------------------------------------------------------------------- */

static int
test_pick_rect_null_view(void)
{
    printf("=== Test 3: pick_rect NULL view ===\n");

    struct bsg_pick_result *res = bsg_pick_rect(NULL, 0, 0, 100, 100);
    if (res) FAIL("pick_rect(NULL) should return NULL");

    PASS("pick_rect NULL view");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 4: bsg_pick_point on empty scene returns empty result
 * ----------------------------------------------------------------------- */

static int
test_pick_empty_scene(void)
{
    printf("=== Test 4: pick_point on empty scene ===\n");

    struct bsg_view *v = make_view();
    if (!v) FAIL("make_view failed");

    struct bsg_pick_result *res = bsg_pick_point(v, 256, 256, 0);
    if (!res) FAIL("bsg_pick_point returned NULL on empty scene");
    if (bsg_pick_result_count(res) != 0)
	FAIL("expected 0 records on empty scene");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_point on empty scene");
    return 0;
}



/* -----------------------------------------------------------------------
 * Test 6: bsg_pick_apply with empty result is a no-op
 * ----------------------------------------------------------------------- */

static int
test_pick_apply_empty(void)
{
    printf("=== Test 6: pick_apply empty result ===\n");

    struct bsg_selection *sel = bsg_selection_create();
    struct bsg_pick_result *res = bsg_pick_result_create();

    bsg_pick_apply(sel, res, BSG_PICK_OP_SET);
    if (bsg_selection_count(sel) != 0) FAIL("set with empty result should leave sel empty");

    bsg_pick_apply(sel, res, BSG_PICK_OP_ADD);
    if (bsg_selection_count(sel) != 0) FAIL("add with empty result should leave sel empty");

    bsg_pick_apply(sel, res, BSG_PICK_OP_REMOVE);
    if (bsg_selection_count(sel) != 0) FAIL("remove with empty result should leave sel empty");

    /* NULL guards */
    bsg_pick_apply(NULL, res, BSG_PICK_OP_SET);
    bsg_pick_apply(sel, NULL, BSG_PICK_OP_SET);

    bsg_pick_result_free(res);
    bsg_selection_destroy(sel);

    PASS("pick_apply empty result");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 7: bsg_pick_nearest returns NULL on NULL view
 * ----------------------------------------------------------------------- */

static int
test_pick_nearest_null(void)
{
    printf("=== Test 7: pick_nearest NULL view ===\n");

    struct bsg_pick_result *res = bsg_pick_nearest(NULL, 256, 256);
    if (res) FAIL("pick_nearest(NULL) should return NULL");

    PASS("pick_nearest NULL view");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 8: pick_op enum values are sane (compile-time style check)
 * ----------------------------------------------------------------------- */

static int
test_pick_op_values(void)
{
    printf("=== Test 8: pick_op enum values ===\n");

    if (BSG_PICK_OP_SET != 0) FAIL("BSG_PICK_OP_SET should be 0");
    if (BSG_PICK_OP_ADD != 1) FAIL("BSG_PICK_OP_ADD should be 1");
    if (BSG_PICK_OP_REMOVE != 2) FAIL("BSG_PICK_OP_REMOVE should be 2");

    PASS("pick_op enum values");
    return 0;
}

static int
test_pick_apply_nonempty(void)
{
    printf("=== Test 9: pick_apply non-empty result ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    if (!root || !s1 || !s2) FAIL("scene setup failed");
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);

    struct bsg_pick_result *res = bsg_pick_result_create();
    struct bsg_selection *sel = bsg_selection_create();
    if (!append_record(res, s1, v, 10, 20, "/a", 1.0)) FAIL("append_record s1");
    if (!append_record(res, s2, v, 10, 20, "/b", 2.0)) FAIL("append_record s2");
    struct bsg_interaction_result *ir = bsg_interaction_from_pick_result(res);
    const struct bsg_interaction_record *r1 = bsg_interaction_result_get(ir, 0);
    const struct bsg_interaction_record *r2 = bsg_interaction_result_get(ir, 1);

    bsg_pick_apply(sel, res, BSG_PICK_OP_SET);
    if (bsg_selection_count(sel) != 2) FAIL("SET should populate both records");
    if (!bsg_selection_contains_record(sel, r1) || !bsg_selection_contains_record(sel, r2))
	FAIL("selection should contain both records after SET");

    bsg_pick_apply(bsg_view_selection(v), res, BSG_PICK_OP_SET);
    if (bsg_selection_count(bsg_view_selection(v)) != 2)
	FAIL("view selection storage should accept pick_apply");

    bsg_pick_apply(sel, res, BSG_PICK_OP_REMOVE);
    if (bsg_selection_count(sel) != 0) FAIL("REMOVE should clear both records");

    bsg_interaction_result_free(ir);
    bsg_selection_destroy(sel);
    bsg_pick_result_free(res);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("pick_apply non-empty result");
    return 0;
}

static int
test_pick_ray_null_view(void)
{
    printf("=== Test 11: pick_ray NULL view ===\n");
    point_t o = VINIT_ZERO;
    vect_t d = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res = bsg_pick_ray(NULL, o, d, BSG_PICK_INCLUDE_SCENE);
    if (res) FAIL("pick_ray(NULL) should return NULL");
    PASS("pick_ray NULL view");
    return 0;
}

static int
test_pick_ray_action_visible_shape(void)
{
    printf("=== Test 12: pick_ray action-visible shape ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *g = bsg_group_create(v);
    bsg_node *visible = bsg_shape_create(v);
    bsg_node *hidden = bsg_shape_create(v);
    if (!v || !root || !g || !visible || !hidden) FAIL("scene setup failed");
    v->gv_scene = root;

    bsg_node_set_draw_intent(g, bsg_draw_intent_create("ray/main", BSG_DRAW_MODE_WIRE));
    bsg_node_add_child(root, g);
    bsg_node_add_child(g, visible);
    bsg_node_add_child(g, hidden);

    point_t bmin, bmax;
    VSET(bmin, -1.0, -1.0, -1.0);
    VSET(bmax,  1.0,  1.0,  1.0);
    bsg_node_set_bounds(visible, bmin, bmax, 1);
    VSET(bmin, -2.0, -2.0, -2.0);
    VSET(bmax,  2.0,  2.0,  2.0);
    bsg_node_set_bounds(hidden, bmin, bmax, 1);
    bsg_node_set_visible_state(hidden, 0);

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL");
    if (bsg_pick_result_count(res) != 1)
	FAIL("pick_ray should return only visible action candidate");
    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || pr->pr_scene.opaque != visible)
	FAIL("pick_ray should report visible shape node");
    if (!BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "ray/main"))
	FAIL("pick_ray should inherit nearest draw-intent path");

    bsg_pick_result_free(res);
    bsg_shape_destroy(visible);
    bsg_shape_destroy(hidden);
    bsg_group_destroy(g);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("pick_ray action-visible shape");
    return 0;
}

static int
test_pick_ray_line_geometry_fields(void)
{
    printf("=== Test 13: pick_ray line geometry fields ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_line_set_ref hit = bsg_line_set_ref_create(v, "line-hit");
    bsg_line_set_ref miss = bsg_line_set_ref_create(v, "line-bbox-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(hit)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(miss)))
	FAIL("typed line geometry setup failed");

    point_t hit_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(hit_pts[0], -1.0, 0.0, 0.0);
    VSET(hit_pts[1],  1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(hit, (const point_t *)hit_pts, cmds, 2))
	FAIL("hit line field setup failed");

    point_t miss_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(miss_pts[0], -1.0, 0.0, 0.0);
    VSET(miss_pts[1],  0.0, 1.0, 0.0);
    if (!bsg_line_set_ref_set_points(miss, (const point_t *)miss_pts, cmds, 2))
	FAIL("miss line field setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(miss));

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL for line geometry");
    if (bsg_pick_result_count(res) != 1)
	FAIL("line geometry pick should reject bbox-only false positive");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "line-hit"))
	FAIL("line geometry pick should report exact field hit");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("line geometry pick should report ray hit distance");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_ray line geometry fields");
    return 0;
}

static int
test_pick_ray_indexed_line_geometry_fields(void)
{
    printf("=== Test 14: pick_ray indexed-line geometry fields ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_line_set_ref hit = bsg_indexed_line_set_ref_create(v, "indexed-hit");
    bsg_indexed_line_set_ref miss = bsg_indexed_line_set_ref_create(v, "indexed-bbox-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(hit)) ||
	    bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(miss)))
	FAIL("typed indexed-line geometry setup failed");

    point_t hit_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(hit_pts[0], -1.0, 0.0, 0.0);
    VSET(hit_pts[1],  1.0, 0.0, 0.0);
    int hit_indices[2] = {0, 1};
    if (!bsg_indexed_line_set_ref_set_geometry(hit, (const point_t *)hit_pts, 2,
		hit_indices, 2))
	FAIL("hit indexed-line field setup failed");

    point_t miss_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(miss_pts[0], -1.0, 0.0, 0.0);
    VSET(miss_pts[1],  0.0, 1.0, 0.0);
    int miss_indices[2] = {0, 1};
    if (!bsg_indexed_line_set_ref_set_geometry(miss, (const point_t *)miss_pts, 2,
		miss_indices, 2))
	FAIL("miss indexed-line field setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_line_set_ref_as_node(hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_line_set_ref_as_node(miss));

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL for indexed-line geometry");
    if (bsg_pick_result_count(res) != 1)
	FAIL("indexed-line geometry pick should reject bbox-only false positive");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "indexed-hit"))
	FAIL("indexed-line geometry pick should report exact field hit");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("indexed-line geometry pick should report ray hit distance");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_ray indexed-line geometry fields");
    return 0;
}

static int
test_pick_ray_point_geometry_fields(void)
{
    printf("=== Test 15: pick_ray point geometry fields ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_point_set_ref hit = bsg_point_set_ref_create(v, "point-hit");
    bsg_point_set_ref miss = bsg_point_set_ref_create(v, "point-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_point_set_ref_as_node(hit)) ||
	    bsg_node_ref_is_null(bsg_point_set_ref_as_node(miss)))
	FAIL("typed point geometry setup failed");

    point_t hit_pts[1] = {VINIT_ZERO};
    VSET(hit_pts[0], 0.0, 0.0, 0.0);
    if (!bsg_point_set_ref_set_points(hit, (const point_t *)hit_pts, 1))
	FAIL("hit point field setup failed");

    point_t miss_pts[1] = {VINIT_ZERO};
    VSET(miss_pts[0], 1.0, 1.0, 0.0);
    if (!bsg_point_set_ref_set_points(miss, (const point_t *)miss_pts, 1))
	FAIL("miss point field setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_point_set_ref_as_node(hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_point_set_ref_as_node(miss));

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL for point geometry");
    if (bsg_pick_result_count(res) != 1)
	FAIL("point geometry pick should report only exact field hits");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "point-hit"))
	FAIL("point geometry pick should report field hit");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("point geometry pick should report ray hit distance");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_ray point geometry fields");
    return 0;
}

static int
test_pick_ray_indexed_face_geometry_fields(void)
{
    printf("=== Test 16: pick_ray indexed-face geometry fields ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_face_set_ref hit = bsg_indexed_face_set_ref_create(v, "face-hit");
    bsg_indexed_face_set_ref miss = bsg_indexed_face_set_ref_create(v, "face-bbox-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(hit)) ||
	    bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(miss)))
	FAIL("typed indexed-face geometry setup failed");

    point_t hit_pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(hit_pts[0], -1.0, -1.0, 0.0);
    VSET(hit_pts[1],  1.0, -1.0, 0.0);
    VSET(hit_pts[2],  0.0,  1.0, 0.0);
    int hit_indices[3] = {0, 1, 2};
    if (!bsg_indexed_face_set_ref_set_geometry(hit, (const point_t *)hit_pts, 3,
		hit_indices, 3))
	FAIL("hit indexed-face field setup failed");

    point_t miss_pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(miss_pts[0], 0.2, 0.0, 0.0);
    VSET(miss_pts[1], 1.0, 0.0, 0.0);
    VSET(miss_pts[2], 0.0, 1.0, 0.0);
    int miss_indices[3] = {0, 1, 2};
    if (!bsg_indexed_face_set_ref_set_geometry(miss, (const point_t *)miss_pts, 3,
		miss_indices, 3))
	FAIL("miss indexed-face field setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_face_set_ref_as_node(hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_face_set_ref_as_node(miss));

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL for indexed-face geometry");
    if (bsg_pick_result_count(res) != 1)
	FAIL("indexed-face geometry pick should reject bbox-only false positive");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "face-hit"))
	FAIL("indexed-face geometry pick should report exact field hit");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("indexed-face geometry pick should report ray hit distance");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_ray indexed-face geometry fields");
    return 0;
}

static int
test_pick_ray_annotation_geometry(void)
{
    printf("=== Test 16a: pick_ray annotation geometry ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_annotation_ref hit = bsg_annotation_ref_create(v, "annotation-hit");
    bsg_annotation_ref miss = bsg_annotation_ref_create(v, "annotation-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_annotation_ref_as_node(hit)) ||
	    bsg_node_ref_is_null(bsg_annotation_ref_as_node(miss)))
	FAIL("typed annotation geometry setup failed");

    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    point_t anchor = VINIT_ZERO;

    point_t hit_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(hit_pts[0], -1.0, 0.0, 0.0);
    VSET(hit_pts[1],  1.0, 0.0, 0.0);
    struct bsg_annotation_segment hit_seg;
    memset(&hit_seg, 0, sizeof(hit_seg));
    hit_seg.kind = BSG_ANNOTATION_SEGMENT_LINE;
    hit_seg.data.line.start = 0;
    hit_seg.data.line.end = 1;
    if (!bsg_annotation_ref_set_record(hit, "annotation hit",
		BSG_ANNOTATION_SPACE_MODEL, anchor, model_mat, display_mat,
		(const point_t *)hit_pts, 2, &hit_seg, 1))
	FAIL("hit annotation setup failed");

    point_t miss_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(miss_pts[0], 0.5, 0.5, 0.0);
    VSET(miss_pts[1], 1.0, 0.5, 0.0);
    struct bsg_annotation_segment miss_seg;
    memset(&miss_seg, 0, sizeof(miss_seg));
    miss_seg.kind = BSG_ANNOTATION_SEGMENT_LINE;
    miss_seg.data.line.start = 0;
    miss_seg.data.line.end = 1;
    if (!bsg_annotation_ref_set_record(miss, "annotation miss",
		BSG_ANNOTATION_SPACE_MODEL, anchor, model_mat, display_mat,
		(const point_t *)miss_pts, 2, &miss_seg, 1))
	FAIL("miss annotation setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_annotation_ref_as_node(hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_annotation_ref_as_node(miss));

    point_t orig = {0.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("pick_ray returned NULL for annotation geometry");
    if (bsg_pick_result_count(res) != 1)
	FAIL("annotation geometry pick should reject bbox-only false positive");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path),
		"annotation-hit"))
	FAIL("annotation geometry pick should report exact segment hit");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("annotation geometry pick should report ray hit distance");

    bsg_pick_result_free(res);
    free_view(v);

    PASS("pick_ray annotation geometry");
    return 0;
}

static int
test_pick_ray_annotation_display_text_and_curves(void)
{
    printf("=== Test 16b: pick_ray annotation display text and curves ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_annotation_ref text_hit = bsg_annotation_ref_create(v, "annotation-display-text");
    bsg_annotation_ref curve_hit = bsg_annotation_ref_create(v, "annotation-curves");
    bsg_annotation_ref curve_miss = bsg_annotation_ref_create(v, "annotation-curve-miss");
    if (!v ||
	    bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_annotation_ref_as_node(text_hit)) ||
	    bsg_node_ref_is_null(bsg_annotation_ref_as_node(curve_hit)) ||
	    bsg_node_ref_is_null(bsg_annotation_ref_as_node(curve_miss)))
	FAIL("typed annotation display/curve setup failed");

    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    point_t anchor = VINIT_ZERO;
    VSET(anchor, 3.0, 0.0, 0.0);

    point_t text_pts[1] = {VINIT_ZERO};
    VSET(text_pts[0], 2.0, 0.0, 0.0);
    struct bsg_annotation_segment text_seg;
    memset(&text_seg, 0, sizeof(text_seg));
    text_seg.kind = BSG_ANNOTATION_SEGMENT_TEXT;
    text_seg.data.text.ref_pt = 0;
    text_seg.data.text.text = (char *)"display note";
    text_seg.data.text.size = 8.0;
    if (!bsg_annotation_ref_set_record(text_hit, "display note",
		BSG_ANNOTATION_SPACE_DISPLAY, anchor, model_mat, display_mat,
		(const point_t *)text_pts, 1, &text_seg, 1))
	FAIL("display-space text annotation setup failed");

    point_t curve_pts[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(curve_pts[0], 0.0, 3.0, 0.0);
    VSET(curve_pts[1], 0.0, 5.0, 0.0);
    VSET(curve_pts[2], 0.0, 7.0, 0.0);
    VSET(curve_pts[3], 0.0, 9.0, 0.0);
    int nurb_controls[3] = {0, 1, 2};
    int bezier_controls[4] = {0, 1, 2, 3};
    struct bsg_annotation_segment curve_segs[3];
    memset(curve_segs, 0, sizeof(curve_segs));
    curve_segs[0].kind = BSG_ANNOTATION_SEGMENT_CARC;
    curve_segs[0].data.carc.start = 0;
    curve_segs[0].data.carc.end = 1;
    curve_segs[0].data.carc.radius = 2.0;
    curve_segs[1].kind = BSG_ANNOTATION_SEGMENT_NURB;
    curve_segs[1].data.nurb.control_point_count = 3;
    curve_segs[1].data.nurb.control_points = nurb_controls;
    curve_segs[2].kind = BSG_ANNOTATION_SEGMENT_BEZIER;
    curve_segs[2].data.bezier.control_point_count = 4;
    curve_segs[2].data.bezier.control_points = bezier_controls;
    VSET(anchor, 0.0, 0.0, 0.0);
    if (!bsg_annotation_ref_set_record(curve_hit, "curve hit",
		BSG_ANNOTATION_SPACE_MODEL, anchor, model_mat, display_mat,
		(const point_t *)curve_pts, 4, curve_segs, 3))
	FAIL("curve annotation setup failed");

    point_t miss_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(miss_pts[0], 2.0, 3.0, 0.0);
    VSET(miss_pts[1], 2.0, 5.0, 0.0);
    struct bsg_annotation_segment miss_seg;
    memset(&miss_seg, 0, sizeof(miss_seg));
    miss_seg.kind = BSG_ANNOTATION_SEGMENT_CARC;
    miss_seg.data.carc.start = 0;
    miss_seg.data.carc.end = 1;
    miss_seg.data.carc.radius = 2.0;
    if (!bsg_annotation_ref_set_record(curve_miss, "curve miss",
		BSG_ANNOTATION_SPACE_MODEL, anchor, model_mat, display_mat,
		(const point_t *)miss_pts, 2, &miss_seg, 1))
	FAIL("miss curve annotation setup failed");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_annotation_ref_as_node(text_hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_annotation_ref_as_node(curve_hit));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_annotation_ref_as_node(curve_miss));

    point_t text_orig = {5.0, 0.0, -5.0};
    vect_t dir = {0.0, 0.0, 1.0};
    struct bsg_pick_result *res =
	bsg_pick_ray(v, text_orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("text pick_ray returned NULL");
    if (bsg_pick_result_count(res) != 1)
	FAIL("display-space text pick should hit exactly the anchored text point");
    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path),
		"annotation-display-text"))
	FAIL("display-space text pick should report the text annotation");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("display-space text pick should report ray hit distance");
    bsg_pick_result_free(res);

    point_t curve_orig = {0.0, 4.0, -5.0};
    res = bsg_pick_ray(v, curve_orig, dir, BSG_PICK_INCLUDE_SCENE);
    if (!res) FAIL("curve pick_ray returned NULL");
    if (bsg_pick_result_count(res) != 1)
	FAIL("curve annotation pick should hit exactly one curve approximation");
    pr = bsg_pick_result_get(res, 0);
    if (!pr || !BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path),
		"annotation-curves"))
	FAIL("curve annotation pick should report curve record");
    if (!NEAR_EQUAL(pr->pr_hit_dist, 5.0, BN_TOL_DIST))
	FAIL("curve annotation pick should report ray hit distance");
    bsg_pick_result_free(res);

    free_view(v);
    PASS("pick_ray annotation display text and curves");
    return 0;
}

static int
test_pick_ray_proxy_payload_identity(void)
{
    printf("=== Test 16c: pick_ray proxy payload identity ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *csg = bsg_shape_create(v);
    bsg_node *brep = bsg_shape_create(v);
    bsg_node *preview = bsg_shape_create(v);
    bsg_node *external = bsg_shape_create(v);
    bsg_node *null_external = bsg_shape_create(v);
    bsg_line_set_ref miss = bsg_line_set_ref_create(v, "proxy-line-miss");
    if (!v || !root || !csg || !brep || !preview || !external ||
	    !null_external ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(miss)))
	FAIL("proxy pick scene setup failed");

    int csg_source = 10;
    int brep_source = 20;
    int preview_source = 30;
    int external_source = 40;
    struct bsg_payload *external_payload = bsg_payload_create(BSG_PL_NONE);
    struct bsg_payload *null_external_payload = bsg_payload_create(BSG_PL_NONE);
    if (!external_payload || !null_external_payload)
	FAIL("external proxy payload setup failed");
    external_payload->pl.opaque = &external_source;
    bsg_node_set_payload(csg, bsg_payload_csg_create(&csg_source));
    bsg_node_set_payload(brep, bsg_payload_brep_create(&brep_source));
    bsg_node_set_payload(preview,
	    bsg_payload_edit_preview_create(&preview_source, NULL));
    bsg_node_set_payload(external, external_payload);
    bsg_node_set_payload(null_external, null_external_payload);
    bsg_node_set_draw_intent(csg,
	    bsg_draw_intent_create("proxy/csg", BSG_DRAW_MODE_SHADED));
    bsg_node_set_draw_intent(brep,
	    bsg_draw_intent_create("proxy/brep", BSG_DRAW_MODE_SHADED));
    bsg_node_set_draw_intent(preview,
	    bsg_draw_intent_create("proxy/preview", BSG_DRAW_MODE_WIRE));
    bsg_node_set_draw_intent(external,
	    bsg_draw_intent_create("proxy/external", BSG_DRAW_MODE_WIRE));
    bsg_node_set_draw_intent(null_external,
	    bsg_draw_intent_create("proxy/external-null", BSG_DRAW_MODE_WIRE));

    point_t bmin, bmax;
    VSET(bmin, -0.5, -0.5, -1.0);
    VSET(bmax,  0.5,  0.5,  1.0);
    bsg_node_set_bounds(csg, bmin, bmax, 1);
    VSET(bmin, 2.5, -0.5, -1.0);
    VSET(bmax, 3.5,  0.5,  1.0);
    bsg_node_set_bounds(brep, bmin, bmax, 1);
    VSET(bmin, 5.5, -0.5, -1.0);
    VSET(bmax, 6.5,  0.5,  1.0);
    bsg_node_set_bounds(preview, bmin, bmax, 1);
    VSET(bmin, 8.5, -0.5, -1.0);
    VSET(bmax, 9.5,  0.5,  1.0);
    bsg_node_set_bounds(external, bmin, bmax, 1);
    VSET(bmin, 11.5, -0.5, -1.0);
    VSET(bmax, 12.5,  0.5,  1.0);
    bsg_node_set_bounds(null_external, bmin, bmax, 1);

    point_t miss_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int miss_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(miss_pts[0], 0.0, 2.0, 0.0);
    VSET(miss_pts[1], 12.0, 2.0, 0.0);
    if (!bsg_line_set_ref_set_points(miss, (const point_t *)miss_pts,
		miss_cmds, 2))
	FAIL("proxy pick miss line setup failed");

    bsg_node_add_child(root, csg);
    bsg_node_add_child(root, brep);
    bsg_node_add_child(root, preview);
    bsg_node_add_child(root, external);
    bsg_node_add_child(root, null_external);
    bsg_group_ref_append_child(bsg_separator_ref_as_group(
		bsg_view_scene_separator_ref(v, 1)), bsg_line_set_ref_as_node(miss));

    struct {
	bsg_node *node;
	const char *path;
	point_t orig;
    } cases[5] = {
	{csg, "proxy/csg", {0.0, 0.0, -5.0}},
	{brep, "proxy/brep", {3.0, 0.0, -5.0}},
	{preview, "proxy/preview", {6.0, 0.0, -5.0}},
	{external, "proxy/external", {9.0, 0.0, -5.0}},
	{null_external, "proxy/external-null", {12.0, 0.0, -5.0}}
    };
    vect_t dir = {0.0, 0.0, 1.0};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	struct bsg_pick_result *res =
	    bsg_pick_ray(v, cases[i].orig, dir, BSG_PICK_INCLUDE_SCENE);
	if (!res)
	    FAIL("proxy pick_ray returned NULL");
	if (bsg_pick_result_count(res) != 1)
	    FAIL("proxy pick should hit exactly one bounded proxy");
	struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
	if (!pr || pr->pr_scene.opaque != cases[i].node)
	    FAIL("proxy pick should report the retained proxy node");
	if (!BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), cases[i].path) ||
		!BU_STR_EQUAL(bu_vls_cstr(&pr->pr_instance_path), cases[i].path))
	    FAIL("proxy pick should preserve draw-intent source identity");
	if (!NEAR_EQUAL(pr->pr_hit_dist, 4.0, BN_TOL_DIST))
	    FAIL("proxy pick should report bounded ray hit distance");
	bsg_pick_result_free(res);
    }

    bsg_shape_destroy(csg);
    bsg_shape_destroy(brep);
    bsg_shape_destroy(preview);
    bsg_shape_destroy(external);
    bsg_shape_destroy(null_external);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("pick_ray proxy payload identity");
    return 0;
}

static int
test_pick_semantic_path_basic(void)
{
    printf("=== Test 17: pick_semantic_path basic ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *g1 = bsg_group_create(v);
    bsg_node *g2 = bsg_group_create(v);
    if (!v || !root || !g1 || !g2) FAIL("scene setup failed");

    bsg_node_set_draw_intent(g1, bsg_draw_intent_create("hull/main", BSG_DRAW_MODE_WIRE));
    bsg_node_set_draw_intent(g2, bsg_draw_intent_create("engine/aux", BSG_DRAW_MODE_SHADED));
    bsg_node_add_child(root, g1);
    bsg_node_add_child(root, g2);

    struct bsg_pick_result *res = bsg_pick_semantic_path(v, "hull/*");
    if (!res) FAIL("pick_semantic_path returned NULL");
    if (bsg_pick_result_count(res) != 1) FAIL("semantic path should match exactly one group");

    struct bsg_pick_record *pr = bsg_pick_result_get(res, 0);
    if (!pr) FAIL("missing semantic pick record");
    if (!BU_STR_EQUAL(bu_vls_cstr(&pr->pr_source_path), "hull/main"))
	FAIL("semantic path source mismatch");
    if (!BU_STR_EQUAL(bu_vls_cstr(&pr->pr_instance_path), "hull/main"))
	FAIL("instance path should mirror semantic source path");
    if (pr->pr_primitive_id != -1 || pr->pr_subelement_id != -1)
	FAIL("default primitive/subelement ids should be -1");

    bsg_pick_result_free(res);
    bsg_group_destroy(g1);
    bsg_group_destroy(g2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("pick_semantic_path basic");
    return 0;
}

static int
test_pick_interaction_selection_apply(void)
{
    printf("=== Test 18: pick interaction selection apply ===\n");

    struct bsg_view *v = make_view();
    bsg_node *shape = bsg_shape_create(v);
    if (!v || !shape) FAIL("scene setup failed");
    bsg_node_set_draw_intent(shape, bsg_draw_intent_create("picked/path", BSG_DRAW_MODE_WIRE));

    struct bsg_pick_result *pr = bsg_pick_result_create();
    if (!append_record(pr, shape, v, 12, 34, "picked/path", 5.0))
	FAIL("pick record setup failed");

    struct bsg_interaction_result *ir = bsg_interaction_from_pick_result(pr);
    if (!ir || bsg_interaction_result_count(ir) != bsg_pick_result_count(pr))
	FAIL("pick interaction conversion count mismatch");
    const struct bsg_interaction_record *rec = bsg_interaction_result_get(ir, 0);
    if (!rec || rec->kind != BSG_INTERACTION_PICK_HIT)
	FAIL("converted record kind mismatch");
    if (!BU_STR_EQUAL(bsg_interaction_record_path(rec), "picked/path"))
	FAIL("converted record path mismatch");

    struct bsg_selection *sel = bsg_selection_create();
    bsg_interaction_selection_apply(sel, ir, BSG_INTERACTION_APPLY_SET);
    if (!bsg_selection_contains_record(sel, rec))
	FAIL("selection should contain applied interaction record");
    bsg_interaction_selection_apply(sel, ir, BSG_INTERACTION_APPLY_REMOVE);
    if (bsg_selection_contains_record(sel, rec))
	FAIL("selection should remove interaction record");

    bsg_selection_destroy(sel);
    bsg_interaction_result_free(ir);
    bsg_pick_result_free(pr);
    bsg_shape_destroy(shape);
    free_view(v);

    PASS("pick interaction selection apply");
    return 0;
}


/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    (void)argc;

    int failures = 0;
    failures += test_result_lifecycle();
    failures += test_pick_point_null_view();
    failures += test_pick_rect_null_view();
    failures += test_pick_empty_scene();
    failures += test_pick_apply_empty();
    failures += test_pick_nearest_null();
    failures += test_pick_op_values();
    failures += test_pick_apply_nonempty();
    failures += test_pick_ray_null_view();
    failures += test_pick_ray_action_visible_shape();
    failures += test_pick_ray_line_geometry_fields();
    failures += test_pick_ray_indexed_line_geometry_fields();
    failures += test_pick_ray_point_geometry_fields();
    failures += test_pick_ray_indexed_face_geometry_fields();
    failures += test_pick_ray_annotation_geometry();
    failures += test_pick_ray_annotation_display_text_and_curves();
    failures += test_pick_ray_proxy_payload_identity();
    failures += test_pick_semantic_path_basic();
    failures += test_pick_interaction_selection_apply();

    if (failures) {
	printf("FAILED: %d test(s) failed\n", failures);
	return 1;
    }
    printf("ALL TESTS PASSED\n");
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
