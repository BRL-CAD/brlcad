/*               T E S T _ A C T I O N . C
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
/** @file libbsg/tests/test_action.c
 *
 * Phase 5 unit tests: BSG action framework (BBOX, COLLECT, EXPORT).
 */

#include "common.h"
#include "../action_private.h"
#include "../node_private.h"
#include "../object_private.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/camera.h"
#include "bsg/complexity.h"
#include "bsg/export.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../node_private.h"
#include "bsg/render.h"
#include "bsg/render_settings.h"
#include "bsg/separator.h"
#include "bsg/switch.h"
#include "bsg/action.h"

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

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "test_view");
}

static bsg_node_ref
test_node_ref(bsg_node *node)
{
    return bsg_node_ref_from_object(bsg_object_ref_from_node(node));
}

static const struct bsg_export_record *
export_record_named(const struct bsg_export_result *result, const char *name)
{
    size_t count = bsg_export_result_count(result);

    if (!name)
	return NULL;

    for (size_t i = 0; i < count; i++) {
	const struct bsg_export_record *record = bsg_export_result_get(result, i);
	if (record && record->source.name &&
		bu_strcmp(record->source.name, name) == 0)
	    return record;
    }

    return NULL;
}

static bsg_node *
test_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(bsg_node_ref_object(ref));
}

/** Create a shape with a bounding box set */
static bsg_node *
make_bbox_shape(struct bsg_view *v,
		double x0, double y0, double z0,
		double x1, double y1, double z1)
{
    bsg_node *s = bsg_shape_create(v);
    if (!s) return NULL;
    point_t bmin, bmax;
    VSET(bmin, x0, y0, z0);
    VSET(bmax, x1, y1, z1);
    bsg_node_set_bounds(s, bmin, bmax, 1);
    return s;
}


/* ------------------------------------------------------------------ */
/* Test 1: BBOX action — no shapes → invalid result                    */
/* ------------------------------------------------------------------ */

static int
test_bbox_empty(void)
{
    printf("=== Test 1: bbox_empty ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("scene_root_create");

    bsg_action_ref a = bsg_bbox_action_create();
    if (bsg_action_ref_is_null(a)) FAIL("bsg_bbox_action_create");

    bsg_action_apply(a, test_node_ref(root));
    point_t bmin, bmax;
    if (bsg_bbox_action_result(a, bmin, bmax) != 0)
	FAIL("expected invalid bbox for empty tree");

    bsg_action_ref_destroy(a);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("bbox_empty");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: BBOX action — two shapes → correct aggregate bounds         */
/* ------------------------------------------------------------------ */

static int
test_bbox_two_shapes(void)
{
    printf("=== Test 2: bbox_two_shapes ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("scene_root_create");

    bsg_node *s1 = make_bbox_shape(v,  0.0,  0.0,  0.0,  1.0,  1.0,  1.0);
    bsg_node *s2 = make_bbox_shape(v, -2.0, -2.0, -2.0,  3.0,  3.0,  3.0);
    if (!s1 || !s2) FAIL("make_bbox_shape");

    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);

    bsg_action_ref a = bsg_bbox_action_create();
    bsg_action_apply(a, test_node_ref(root));

    point_t bmin, bmax;
    if (!bsg_bbox_action_result(a, bmin, bmax)) FAIL("expected valid=1");
    if (bmin[0] > -1.9999) FAIL("bmin X should be ~ -2");
    if (bmax[0] < 2.9999)  FAIL("bmax X should be ~ 3");

    bsg_action_ref_destroy(a);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("bbox_two_shapes");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: BBOX action — inherited transform affects bounds            */
/* ------------------------------------------------------------------ */

static int
test_bbox_transform(void)
{
    printf("=== Test 3: bbox_transform ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *xf = bsg_transform_create(v);
    bsg_node *shape = make_bbox_shape(v, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    if (!root || !xf || !shape) FAIL("node creation");

    mat_t m;
    MAT_IDN(m);
    MAT_DELTAS(m, 10.0, 0.0, 0.0);
    bsg_transform_set_matrix(xf, m);
    bsg_node_add_child(root, xf);
    bsg_node_add_child(xf, shape);

    bsg_action_ref a = bsg_bbox_action_create();
    bsg_action_apply(a, test_node_ref(root));
    point_t bmin, bmax;
    if (!bsg_bbox_action_result(a, bmin, bmax)) FAIL("expected transformed bbox valid");
    if (fabs(bmin[X] - 10.0) > 0.0001)
	FAIL("transformed bbox min X");
    if (fabs(bmax[X] - 11.0) > 0.0001)
	FAIL("transformed bbox max X");

    bsg_action_ref_destroy(a);
    bsg_shape_destroy(shape);
    bsg_transform_destroy(xf);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("bbox_transform");
    return 0;
}

static int
test_bbox_uses_bounds_field_authority(void)
{
    printf("=== Test 4: bbox_bounds_field_authority ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = make_bbox_shape(v, -1.0, -2.0, -3.0, 4.0, 5.0, 6.0);
    if (!root || !shape) FAIL("node creation");

    bsg_node_add_child(root, shape);

    bsg_action_ref a = bsg_bbox_action_create();
    bsg_action_apply(a, test_node_ref(root));
    point_t bmin, bmax;
    if (!bsg_bbox_action_result(a, bmin, bmax)) FAIL("expected field-authoritative bbox valid");
    if (fabs(bmin[X] + 1.0) > 0.0001 || fabs(bmin[Y] + 2.0) > 0.0001 ||
	    fabs(bmin[Z] + 3.0) > 0.0001)
	FAIL("field-authoritative bbox min");
    if (fabs(bmax[X] - 4.0) > 0.0001 || fabs(bmax[Y] - 5.0) > 0.0001 ||
	    fabs(bmax[Z] - 6.0) > 0.0001)
	FAIL("field-authoritative bbox max");

    bsg_action_ref_destroy(a);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("bbox_bounds_field_authority");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: COLLECT action — collect all shapes                         */
/* ------------------------------------------------------------------ */

static int
test_collect_shapes(void)
{
    printf("=== Test 3: collect_shapes ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("scene_root_create");

    bsg_node *g   = bsg_group_create(v);
    bsg_node *s1  = bsg_shape_create(v);
    bsg_node *s2  = bsg_shape_create(v);
    if (!g || !s1 || !s2) FAIL("node creation");

    bsg_node_add_child(root, g);
    bsg_node_add_child(g, s1);
    bsg_node_add_child(g, s2);

    bsg_action_ref a = bsg_collect_action_create(bsg_shape_type());
    bsg_action_apply(a, test_node_ref(root));

    size_t cnt = bsg_collect_action_count(a);
    if (cnt != 2) {
	printf("  FAIL: collect_shapes expected 2, got %zu\n", cnt);
	bsg_action_ref_destroy(a);
	bsg_shape_destroy(s1);
	bsg_shape_destroy(s2);
	bsg_group_destroy(g);
	bsg_view_scene_root_detach_from_root(root);
	free_view(v);
	return 1;
    }

    bsg_action_ref_destroy(a);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_group_destroy(g);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("collect_shapes");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: COLLECT action — collect all (mask=0)                       */
/* ------------------------------------------------------------------ */

static int
test_collect_all(void)
{
    printf("=== Test 4: collect_all ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("scene_root_create");

    bsg_node *g  = bsg_group_create(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node_add_child(root, g);
    bsg_node_add_child(g, s1);

    /* mask=0 should collect all nodes: root + g + s1 */
    bsg_action_ref a = bsg_collect_action_create(BSG_TYPE_INVALID);
    bsg_action_apply(a, test_node_ref(root));

    size_t cnt = bsg_collect_action_count(a);
    if (cnt < 3) {
	printf("  FAIL: collect_all expected >= 3, got %zu\n", cnt);
	bsg_action_ref_destroy(a);
	bsg_shape_destroy(s1);
	bsg_group_destroy(g);
	bsg_view_scene_root_detach_from_root(root);
	free_view(v);
	return 1;
    }

    bsg_action_ref_destroy(a);
    bsg_shape_destroy(s1);
    bsg_group_destroy(g);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("collect_all");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: NULL-guard — no crash on NULL inputs                        */
/* ------------------------------------------------------------------ */

static int
test_null_guards(void)
{
    printf("=== Test 5: null_guards ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    bsg_action_apply(bsg_action_ref_null(), test_node_ref(root));
    bsg_action_apply(bsg_action_ref_null(), bsg_node_ref_null());

    bsg_action_ref a = bsg_bbox_action_create();
    bsg_action_apply(a, bsg_node_ref_null());
    point_t bmin, bmax;
    if (bsg_bbox_action_result(a, bmin, bmax) != 0)
	FAIL("NULL root should leave invalid bbox");
    bsg_action_ref_destroy(a);

    bsg_action_ref_destroy(bsg_action_ref_null());

    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("null_guards");
    return 0;
}

struct traverse_state {
    int shapes;
    fastf_t tx;
};

static int
traverse_shape_cb(bsg_node *UNUSED(node),
		  const struct bsg_action_state *state,
		  void *userdata)
{
    struct traverse_state *st = (struct traverse_state *)userdata;
    if (bsg_state_ref_is_null(state->state))
	return 0;
    st->shapes++;
    mat_t model_mat;
    bsg_state_ref_transform(state->state, model_mat);
    point_t origin = VINIT_ZERO;
    point_t moved = VINIT_ZERO;
    MAT4X3PNT(moved, model_mat, origin);
    st->tx = moved[X];
    return 1;
}

static int
test_stateful_traverse_transform_visibility(void)
{
    printf("=== Test 6: stateful_traverse_transform_visibility ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *t = bsg_transform_create(v);
    bsg_node *visible = bsg_shape_create(v);
    bsg_node *hidden = bsg_shape_create(v);
    if (!root || !t || !visible || !hidden) FAIL("node creation");

    mat_t m;
    MAT_IDN(m);
    MAT_DELTAS(m, 5.0, 0.0, 0.0);
    bsg_transform_set_matrix(t, m);
    bsg_node_set_visible_state(hidden, 0);

    bsg_node_add_child(root, t);
    bsg_node_add_child(t, visible);
    bsg_node_add_child(t, hidden);

    struct traverse_state st;
    memset(&st, 0, sizeof(st));
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = v;
    traversal.root = root;
    traversal.flags = BSG_ACTION_TRAVERSE_VISIBLE_ONLY;
    traversal.shape_cb = traverse_shape_cb;
    traversal.userdata = &st;
    if (!bsg_action_traverse(&traversal))
	FAIL("bsg_action_traverse returned false");
    if (st.shapes != 1)
	FAIL("visible traversal should visit only one shape");
    if (fabs(st.tx - 5.0) > 0.0001)
	FAIL("state transform was not accumulated");

    bsg_shape_destroy(visible);
    bsg_shape_destroy(hidden);
    bsg_transform_destroy(t);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("stateful_traverse_transform_visibility");
    return 0;
}

struct independent_root_state {
    int shapes;
    char last_name[64];
};

static int
independent_root_shape_cb(bsg_node *node,
			  const struct bsg_action_state *UNUSED(state),
			  void *userdata)
{
    struct independent_root_state *st = (struct independent_root_state *)userdata;
    st->shapes++;
    bu_strlcpy(st->last_name, bsg_node_name(node), sizeof(st->last_name));
    return 1;
}

static int
test_independent_root_overlay_uses_field_name(void)
{
    printf("=== Test 7: independent_root_overlay_uses_field_name ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_scene_ref scope = bsg_view_independent_scope_ref(v, 1);
    bsg_node *overlay_group = bsg_group_create(v);
    bsg_node *overlay_shape = bsg_shape_create(v);
    bsg_node *shared_group = bsg_group_create(v);
    bsg_node *shared_shape = bsg_shape_create(v);
    if (!root || bsg_scene_ref_is_null(scope) || !overlay_group || !overlay_shape ||
	    !shared_group || !shared_shape)
	FAIL("node creation");

    bsg_node_set_name(overlay_group, "_overlays");
    bsg_node_set_name(overlay_shape, "overlay_shape");
    bsg_node_set_name(shared_group, "shared_group");
    bsg_node_set_name(shared_shape, "shared_shape");
    bsg_node_add_child(root, overlay_group);
    bsg_node_add_child(overlay_group, overlay_shape);
    bsg_node_add_child(root, shared_group);
    bsg_node_add_child(shared_group, shared_shape);

    struct independent_root_state st;
    memset(&st, 0, sizeof(st));
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = v;
    traversal.root = root;
    traversal.flags = BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT;
    traversal.shape_cb = independent_root_shape_cb;
    traversal.userdata = &st;
    if (!bsg_action_traverse(&traversal))
	FAIL("bsg_action_traverse returned false");
    if (st.shapes != 1)
	FAIL("independent root traversal should keep only overlay direct child");
    if (!BU_STR_EQUAL(st.last_name, "overlay_shape"))
	FAIL("independent root traversal should use canonical overlay name");

    bsg_shape_destroy(shared_shape);
    bsg_group_destroy(shared_group);
    bsg_shape_destroy(overlay_shape);
    bsg_group_destroy(overlay_group);
    bsg_view_independent_scope_destroy(v);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("independent_root_overlay_uses_field_name");
    return 0;
}

struct switch_traverse_state {
    int shapes;
    char last_name[64];
};

static int
switch_shape_cb(bsg_node *node,
		const struct bsg_action_state *UNUSED(state),
		void *userdata)
{
    struct switch_traverse_state *st = (struct switch_traverse_state *)userdata;
    st->shapes++;
    bu_strlcpy(st->last_name, bsg_node_name(node), sizeof(st->last_name));
    return 1;
}

static int
_count_switch_shapes(struct bsg_view *v, bsg_node *root, struct switch_traverse_state *st)
{
    memset(st, 0, sizeof(*st));
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = v;
    traversal.root = root;
    traversal.shape_cb = switch_shape_cb;
    traversal.userdata = st;
    return bsg_action_traverse(&traversal);
}

static int
test_switch_traverse_selection(void)
{
    printf("=== Test 7: switch_traverse_selection ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_switch_ref sw = bsg_switch_ref_create(v, "switch");
    bsg_shape_ref a = bsg_shape_ref_create(v, "a");
    bsg_shape_ref b = bsg_shape_ref_create(v, "b");
    bsg_node *root_node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(root)));
    if (!root_node) FAIL("root extraction");

    bsg_separator_ref_append_child(root, bsg_switch_ref_as_node(sw));
    bsg_switch_ref_append_child(sw, bsg_shape_ref_as_node(a));
    bsg_switch_ref_append_child(sw, bsg_shape_ref_as_node(b));

    struct switch_traverse_state st;
    if (!_count_switch_shapes(v, root_node, &st))
	FAIL("switch none traversal failed");
    if (st.shapes != 0)
	FAIL("switch default NONE should visit no children");

    bsg_switch_ref_set_which_child(sw, BSG_SWITCH_ALL);
    if (!_count_switch_shapes(v, root_node, &st))
	FAIL("switch all traversal failed");
    if (st.shapes != 2)
	FAIL("switch ALL should visit both children");

    bsg_switch_ref_set_which_child(sw, 1);
    if (!_count_switch_shapes(v, root_node, &st))
	FAIL("switch single traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "b"))
	FAIL("switch child 1 should visit only b");

    free_view(v);
    PASS("switch_traverse_selection");
    return 0;
}

struct separator_traverse_state {
    int inside_seen;
    int outside_seen;
};

static int
separator_shape_cb(bsg_node *node,
		   const struct bsg_action_state *state,
		   void *userdata)
{
    struct separator_traverse_state *st = (struct separator_traverse_state *)userdata;
    point_t origin = VINIT_ZERO;
    point_t moved = VINIT_ZERO;
    MAT4X3PNT(moved, state->model_mat, origin);

    if (BU_STR_EQUAL(bsg_node_name(node), "inside")) {
	if (fabs(moved[X] - 9.0) > 0.0001)
	    return 0;
	st->inside_seen = 1;
    } else if (BU_STR_EQUAL(bsg_node_name(node), "outside")) {
	if (fabs(moved[X]) > 0.0001)
	    return 0;
	st->outside_seen = 1;
    }
    return 1;
}

static int
test_separator_traverse_isolation(void)
{
    printf("=== Test 8: separator_traverse_isolation ===\n");

    struct bsg_view *v = make_view();
    mat_t mat;
    MAT_IDN(mat);
    MAT_DELTAS(mat, 9.0, 0.0, 0.0);

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_separator_ref sep = bsg_separator_ref_create(v, "sep");
    bsg_transform_ref xf = bsg_transform_ref_create(v, mat, "xf");
    bsg_shape_ref inside = bsg_shape_ref_create(v, "inside");
    bsg_shape_ref outside = bsg_shape_ref_create(v, "outside");
    bsg_node *root_node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(root)));
    bsg_node *xf_node = bsg_object_ref_node(bsg_node_ref_object(bsg_transform_ref_as_node(xf)));
    bsg_node *inside_node = bsg_object_ref_node(bsg_node_ref_object(bsg_shape_ref_as_node(inside)));
    if (!root_node || !xf_node || !inside_node) FAIL("node extraction");

    bsg_separator_ref_append_child(root, bsg_separator_ref_as_node(sep));
    bsg_separator_ref_append_child(root, bsg_shape_ref_as_node(outside));
    bsg_separator_ref_append_child(sep, bsg_transform_ref_as_node(xf));
    bsg_node_add_child(xf_node, inside_node);

    struct separator_traverse_state st;
    memset(&st, 0, sizeof(st));
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = v;
    traversal.root = root_node;
    traversal.shape_cb = separator_shape_cb;
    traversal.userdata = &st;
    if (!bsg_action_traverse(&traversal))
	FAIL("separator traversal state was not isolated");
    if (!st.inside_seen || !st.outside_seen)
	FAIL("separator traversal did not visit both shapes");

    free_view(v);
    PASS("separator_traverse_isolation");
    return 0;
}

struct lod_traverse_state {
    int shapes;
    char last_name[64];
    int lod_level;
    int lod_level_count;
};

static int
lod_shape_cb(bsg_node *node,
	     const struct bsg_action_state *state,
	     void *userdata)
{
    struct lod_traverse_state *st = (struct lod_traverse_state *)userdata;
    st->shapes++;
    bu_strlcpy(st->last_name, bsg_node_name(node), sizeof(st->last_name));
    st->lod_level = state->lod_level;
    st->lod_level_count = state->lod_level_count;
    return 1;
}

static int
_count_lod_shapes(struct bsg_view *v, bsg_node *root, struct lod_traverse_state *st)
{
    memset(st, 0, sizeof(*st));
    st->lod_level = -99;
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.view = v;
    traversal.root = root;
    traversal.shape_cb = lod_shape_cb;
    traversal.userdata = st;
    return bsg_action_traverse(&traversal);
}

static int
test_lod_traverse_field_selection(void)
{
    printf("=== Test 9: lod_traverse_field_selection ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_lod_ref lod = bsg_lod_ref_create(v, "lod");
    bsg_shape_ref fine = bsg_shape_ref_create(v, "fine");
    bsg_shape_ref coarse = bsg_shape_ref_create(v, "coarse");
    bsg_node *root_node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(root)));
    if (!root_node) FAIL("root extraction");

    bsg_separator_ref_append_child(root, bsg_lod_ref_as_node(lod));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(fine));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(coarse));

    if (bsg_lod_ref_level_count(lod) != 2)
	FAIL("LoD node should report two public levels");
    if (!bsg_lod_ref_set_selection(lod, BSG_LOD_SELECT_FORCE_LEVEL) ||
	    !bsg_lod_ref_set_active_level(lod, 1))
	FAIL("set forced LoD level");

    struct lod_traverse_state st;
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("forced LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "coarse"))
	FAIL("forced LoD should visit level 1");
    if (st.lod_level != 1 || st.lod_level_count != 2)
	FAIL("forced LoD state not propagated");

    if (!bsg_lod_ref_set_selection(lod, BSG_LOD_SELECT_AUTO) ||
	    !bsg_lod_ref_set_active_level(lod, -1) ||
	    !bsg_lod_ref_append_range(lod, 50.0))
	FAIL("set automatic LoD range");
    v->gv_size = 25.0;
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("range LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "fine") || st.lod_level != 0)
	FAIL("small view scale should select level 0");

    v->gv_size = 75.0;
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("range LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "coarse") || st.lod_level != 1)
	FAIL("large view scale should select level 1");

    free_view(v);
    PASS("lod_traverse_field_selection");
    return 0;
}

static int
test_lod_traverse_camera_range_selection(void)
{
    printf("=== Test 10: lod_traverse_camera_range_selection ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_camera_ref camera = bsg_camera_ref_create(v, "camera");
    bsg_lod_ref lod = bsg_lod_ref_create(v, "lod");
    bsg_shape_ref fine = bsg_shape_ref_create(v, "fine");
    bsg_shape_ref coarse = bsg_shape_ref_create(v, "coarse");
    bsg_node *root_node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(root)));
    if (!root_node) FAIL("root extraction");

    point_t bmin;
    point_t bmax;
    point_t camera_pos;
    VSET(bmin, -1.0, -1.0, -1.0);
    VSET(bmax, 1.0, 1.0, 1.0);
    VSET(camera_pos, 5.0, 0.0, 0.0);
    bsg_node_ref_set_bounds(bsg_lod_ref_as_node(lod), bmin, bmax, 1);
    bsg_camera_ref_set_position(camera, camera_pos);
    bsg_lod_ref_append_range(lod, 10.0);
    v->gv_size = 75.0;

    bsg_separator_ref_append_child(root, bsg_camera_ref_as_node(camera));
    bsg_separator_ref_append_child(root, bsg_lod_ref_as_node(lod));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(fine));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(coarse));

    struct lod_traverse_state st;
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("camera LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "fine") || st.lod_level != 0)
	FAIL("near camera should select level 0 despite coarse view scale");

    VSET(camera_pos, 20.0, 0.0, 0.0);
    bsg_camera_ref_set_position(camera, camera_pos);
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("camera LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "coarse") || st.lod_level != 1)
	FAIL("far camera should select level 1");

    free_view(v);
    PASS("lod_traverse_camera_range_selection");
    return 0;
}

static int
test_complexity_forces_lod_selection(void)
{
    printf("=== Test 11: complexity_forces_lod_selection ===\n");

    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_complexity_ref complexity = bsg_complexity_ref_create(v, "complexity");
    bsg_lod_ref lod = bsg_lod_ref_create(v, "lod");
    bsg_shape_ref fine = bsg_shape_ref_create(v, "fine");
    bsg_shape_ref coarse = bsg_shape_ref_create(v, "coarse");
    bsg_node *root_node = bsg_object_ref_node(bsg_node_ref_object(bsg_separator_ref_as_node(root)));
    if (!root_node) FAIL("root extraction");

    bsg_complexity_ref_set_lod_policy(complexity, BSG_LOD_FORCE_LEVEL);
    bsg_complexity_ref_set_lod_level(complexity, 0);
    bsg_lod_ref_set_selection(lod, BSG_LOD_SELECT_FORCE_LEVEL);
    bsg_lod_ref_set_active_level(lod, 1);

    bsg_separator_ref_append_child(root, bsg_complexity_ref_as_node(complexity));
    bsg_separator_ref_append_child(root, bsg_lod_ref_as_node(lod));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(fine));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(coarse));

    struct lod_traverse_state st;
    if (!_count_lod_shapes(v, root_node, &st))
	FAIL("complexity-forced LoD traversal failed");
    if (st.shapes != 1 || !BU_STR_EQUAL(st.last_name, "fine"))
	FAIL("complexity forced level should override LoD active level");
    if (st.lod_level != 0 || st.lod_level_count != 2)
	FAIL("complexity-forced LoD state not propagated");

    free_view(v);
    PASS("complexity_forces_lod_selection");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 12: semantic export records preserve geometry and transforms   */
/* ------------------------------------------------------------------ */

static int
test_export_transform(void)
{
    printf("=== Test 9: export_record_transform ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *xf = bsg_transform_create(v);
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "transform-lines");
    bsg_node *line_node = test_node_from_ref(bsg_line_set_ref_as_node(lines));
    if (!root || !xf || !line_node) FAIL("node creation");

    mat_t m;
    MAT_IDN(m);
    MAT_DELTAS(m, 7.0, 0.0, 0.0);
    bsg_transform_set_matrix(xf, m);
    bsg_node_add_child(root, xf);
    bsg_node_add_child(xf, line_node);

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2))
	FAIL("line-set geometry");

    struct bsg_export_result *result = bsg_export_scene(v, BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result)
	FAIL("export result");
    const struct bsg_export_record *record =
	export_record_named(result, "transform-lines");
    if (!record)
	FAIL("export record missing");
    if (record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY))
	FAIL("export record line-set geometry");
    if (record->geometry.arrays.point_count != 2 ||
	    record->geometry.arrays.command_count != 2)
	FAIL("export record line-set stats");
    if (fabs(record->model_mat[MDX] - 7.0) > 0.0001)
	FAIL("export record transform");

    bsg_export_result_free(result);
    bsg_transform_destroy(xf);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("export_record_transform");
    return 0;
}

static int
test_export_geometry_fields(void)
{
    printf("=== Test 10: export_geometry_fields ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *xf = bsg_transform_create(v);
    if (!root || !xf) FAIL("node creation");

    mat_t m;
    MAT_IDN(m);
    MAT_DELTAS(m, 10.0, 0.0, 0.0);
    bsg_transform_set_matrix(xf, m);
    bsg_node_add_child(root, xf);

    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "lines");
    point_t lpts[2];
    int lcmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(lpts[0], 0.0, 0.0, 0.0);
    VSET(lpts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)lpts, lcmds, 2))
	FAIL("line-set geometry");

    bsg_point_set_ref points = bsg_point_set_ref_create(v, "points");
    point_t ppts[1];
    VSET(ppts[0], 2.0, 0.0, 0.0);
    if (!bsg_point_set_ref_set_points(points, (const point_t *)ppts, 1))
	FAIL("point-set geometry");

    bsg_indexed_line_set_ref ilines = bsg_indexed_line_set_ref_create(v, "ilines");
    point_t ipts[2];
    int iidx[3] = {0, 1, -1};
    VSET(ipts[0], 3.0, 0.0, 0.0);
    VSET(ipts[1], 4.0, 0.0, 0.0);
    if (!bsg_indexed_line_set_ref_set_geometry(ilines, (const point_t *)ipts, 2, iidx, 3))
	FAIL("indexed-line geometry");

    bsg_indexed_face_set_ref faces = bsg_indexed_face_set_ref_create(v, "faces");
    point_t fpts[3];
    int fidx[4] = {0, 1, 2, -1};
    VSET(fpts[0], 5.0, 0.0, 0.0);
    VSET(fpts[1], 6.0, 0.0, 0.0);
    VSET(fpts[2], 5.0, 1.0, 0.0);
    if (!bsg_indexed_face_set_ref_set_geometry(faces, (const point_t *)fpts, 3, fidx, 4))
	FAIL("indexed-face geometry");

    bsg_node_add_child(xf, test_node_from_ref(bsg_line_set_ref_as_node(lines)));
    bsg_node_add_child(xf, test_node_from_ref(bsg_point_set_ref_as_node(points)));
    bsg_node_add_child(xf, test_node_from_ref(bsg_indexed_line_set_ref_as_node(ilines)));
    bsg_node_add_child(xf, test_node_from_ref(bsg_indexed_face_set_ref_as_node(faces)));

    struct bsg_export_result *result = bsg_export_scene(v, BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 4)
	FAIL("field geometry export record count");

    const struct bsg_export_record *line_record = export_record_named(result, "lines");
    const struct bsg_export_record *point_record = export_record_named(result, "points");
    const struct bsg_export_record *iline_record = export_record_named(result, "ilines");
    const struct bsg_export_record *face_record = export_record_named(result, "faces");
    if (!line_record || !point_record || !iline_record || !face_record)
	FAIL("field geometry export records missing");
    if (line_record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    point_record->geometry.kind != BSG_RENDER_GEOMETRY_POINT_SET ||
	    iline_record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_LINE_SET ||
	    face_record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	FAIL("field geometry export kinds");
    if (fabs(line_record->model_mat[MDX] - 10.0) > 0.0001 ||
	    fabs(point_record->model_mat[MDX] - 10.0) > 0.0001 ||
	    fabs(iline_record->model_mat[MDX] - 10.0) > 0.0001 ||
	    fabs(face_record->model_mat[MDX] - 10.0) > 0.0001)
	FAIL("field geometry export transforms");

    if (line_record->geometry.arrays.point_count != 2 ||
	    line_record->geometry.arrays.command_count != 2 ||
	    !line_record->geometry.arrays.points ||
	    !line_record->geometry.arrays.commands ||
	    line_record->geometry.arrays.commands[0] != BSG_GEOMETRY_LINE_MOVE ||
	    line_record->geometry.arrays.commands[1] != BSG_GEOMETRY_LINE_DRAW ||
	    fabs(line_record->geometry.arrays.points[1][X] - 1.0) > 0.0001)
	FAIL("line-set field record");
    if (point_record->geometry.arrays.point_count != 1 ||
	    !point_record->geometry.arrays.points ||
	    fabs(point_record->geometry.arrays.points[0][X] - 2.0) > 0.0001)
	FAIL("point-set field record");
    if (iline_record->geometry.arrays.point_count != 2 ||
	    iline_record->geometry.arrays.index_count != 3 ||
	    !iline_record->geometry.arrays.points ||
	    !iline_record->geometry.arrays.indices ||
	    iline_record->geometry.arrays.indices[0] != 0 ||
	    iline_record->geometry.arrays.indices[1] != 1 ||
	    iline_record->geometry.arrays.indices[2] != -1 ||
	    fabs(iline_record->geometry.arrays.points[1][X] - 4.0) > 0.0001)
	FAIL("indexed-line field record");
    if (face_record->geometry.arrays.point_count != 3 ||
	    face_record->geometry.arrays.index_count != 4 ||
	    !face_record->geometry.arrays.points ||
	    !face_record->geometry.arrays.indices ||
	    face_record->geometry.arrays.indices[0] != 0 ||
	    face_record->geometry.arrays.indices[1] != 1 ||
	    face_record->geometry.arrays.indices[2] != 2 ||
	    face_record->geometry.arrays.indices[3] != -1 ||
	    fabs(face_record->geometry.arrays.points[1][X] - 6.0) > 0.0001)
	FAIL("indexed-face field record");

    bsg_export_result_free(result);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("export_geometry_fields");
    return 0;
}

struct custom_method_state {
    int calls;
    int saw_shape;
    int saw_state;
};

static int
custom_method_cb(bsg_action_ref action,
		 bsg_node_ref node,
		 bsg_state_ref state,
		 void *userdata)
{
    struct custom_method_state *st = (struct custom_method_state *)userdata;
    if (bsg_action_ref_type(action) == BSG_TYPE_INVALID)
	return 0;
    st->calls++;
    if (bsg_node_is_a(node, bsg_shape_type()))
	st->saw_shape = 1;
    if (!bsg_state_ref_is_null(state))
	st->saw_state = 1;
    return 1;
}

static int
test_custom_action_method_dispatch(void)
{
    printf("=== Test 11: custom_action_method_dispatch ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) FAIL("node creation");
    bsg_node_add_child(root, shape);

    bsg_type_id action_type = bsg_type_register("TestCustomAction", bsg_action_type());
    if (action_type == BSG_TYPE_INVALID)
	FAIL("custom action type registration");

    struct custom_method_state st;
    memset(&st, 0, sizeof(st));
    if (!bsg_action_method_register(action_type, bsg_shape_type(),
		custom_method_cb, &st))
	FAIL("custom action method registration");

    bsg_action_ref action = bsg_action_ref_create(action_type);
    if (!bsg_action_apply(action, test_node_ref(root)))
	FAIL("custom action apply");
    if (st.calls != 1 || !st.saw_shape || !st.saw_state)
	FAIL("custom action method did not see shape state");

    bsg_action_ref_destroy(action);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("custom_action_method_dispatch");
    return 0;
}

static int
test_search_action(void)
{
    printf("=== Test 12: search_action ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *a = bsg_shape_create(v);
    bsg_node *b = bsg_shape_create(v);
    if (!root || !a || !b) FAIL("node creation");
    bsg_node_set_name(a, "target_alpha");
    bsg_node_set_name(b, "other");
    bsg_node_add_child(root, a);
    bsg_node_add_child(root, b);

    bsg_action_ref action = bsg_search_action_create("target_*");
    if (!bsg_action_apply(action, test_node_ref(root)))
	FAIL("search action apply");
    if (bsg_search_action_count(action) != 1)
	FAIL("search action should find one node");
    bsg_node_ref found = bsg_search_action_get(action, 0);
    if (!BU_STR_EQUAL(bsg_node_ref_name(found), "target_alpha"))
	FAIL("search action returned wrong node");

    if (!bsg_search_action_set_name(action, "other") ||
	    !bsg_action_apply(action, test_node_ref(root)) ||
	    bsg_search_action_count(action) != 1 ||
	    !BU_STR_EQUAL(bsg_node_ref_name(bsg_search_action_get(action, 0)), "other"))
	FAIL("search action set name");

    bsg_action_ref_destroy(action);
    bsg_shape_destroy(a);
    bsg_shape_destroy(b);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("search_action");
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

    failures += test_bbox_empty();
    failures += test_bbox_two_shapes();
    failures += test_bbox_transform();
    failures += test_bbox_uses_bounds_field_authority();
    failures += test_collect_shapes();
    failures += test_collect_all();
    failures += test_null_guards();
    failures += test_stateful_traverse_transform_visibility();
    failures += test_independent_root_overlay_uses_field_name();
    failures += test_switch_traverse_selection();
    failures += test_separator_traverse_isolation();
    failures += test_lod_traverse_field_selection();
    failures += test_lod_traverse_camera_range_selection();
    failures += test_complexity_forces_lod_selection();
    failures += test_export_transform();
    failures += test_export_geometry_fields();
    failures += test_custom_action_method_dispatch();
    failures += test_search_action();

    if (failures == 0)
	printf("RESULT: all Phase 5 action tests PASSED\n");
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
