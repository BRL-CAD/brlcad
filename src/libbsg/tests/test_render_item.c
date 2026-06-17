/*          T E S T _ R E N D E R _ I T E M . C
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
/** @file libbsg/tests/test_render_item.c
 *
 * Phase D5 unit tests: render items, phase classification, backend adapter
 * dispatch, and transform accumulation via bsg_render_request_execute.
 */

#include "common.h"
#include "../lod_private.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn/mat.h"
#include "bn/tol.h"

#include "bsg/defines.h"
#include "bsg/geometry.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/material.h"
#include "../material_private.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../node_private.h"
#include "../object_private.h"
#include "bsg/hud.h"
#include "../hud_private.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/scene_builder.h"
#include "bsg/backend_adapter.h"
#include "bsg/interaction.h"
#include "bsg/selection.h"
#include "bsg/view_state.h"
#include "bsg/payload.h"
#include "../payload_typed_private.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/draw_source.h"
#include "../draw_source_private.h"
#include "bsg/lod.h"
#include "bsg/view_scope.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

static bsg_node *
test_line_node_create(struct bsg_view *v, bsg_node *root, const char *name,
		      const point_t a, const point_t b)
{
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, name);
    bsg_node *node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!node)
	return NULL;

    point_t pts[2];
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VMOVE(pts[0], a);
    VMOVE(pts[1], b);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2))
	return NULL;

    bsg_node_add_child(root, node);
    return node;
}


/* ------------------------------------------------------------------ */
/* View helpers (same pattern as test_pick.c)                          */
/* ------------------------------------------------------------------ */

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "ri_test_view");
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "ri_test_view");
}


/* ------------------------------------------------------------------ */
/* Test 1: bsg_render_item_create / free                               */
/* ------------------------------------------------------------------ */

static int
test_item_create_free(void)
{
    printf("=== Test 1: item_create_free ===\n");

    struct bsg_render_item *item = bsg_render_item_create();
    if (!item) FAIL("bsg_render_item_create returned NULL");
    if (!NEAR_EQUAL(item->appearance.transparency, 1.0, BN_TOL_DIST))               FAIL("default transparency should be 1.0");
    if (item->visible      != 1)                 FAIL("default effective visibility should be true");
    if (item->lod_level    != -1)                FAIL("default lod_level should be -1");
    if (item->lod_level_count != 0)              FAIL("default lod_level_count should be 0");
    if (item->phase        != BSG_RENDER_PHASE_OPAQUE) FAIL("default phase should be OPAQUE");

    bsg_render_item_free(item);
    bsg_render_item_free(NULL);  /* must not crash */

    PASS("item_create_free");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Synthetic LoD source policy for backend-facing item tests           */
/* ------------------------------------------------------------------ */

struct render_item_lod_state {
    int level;
};

static int
render_item_lod_select(const struct bsg_lod_source_record *UNUSED(source),
		       struct bsg_view *UNUSED(v),
		       void *policy_data)
{
    struct render_item_lod_state *st =
	(struct render_item_lod_state *)policy_data;
    return st ? st->level : 0;
}

static void
render_item_lod_activate(const struct bsg_lod_source_record *UNUSED(source),
			 struct bsg_view *UNUSED(v),
			 int level,
			 struct bsg_lod_view_state *state,
			 void *UNUSED(policy_data))
{
    if (state)
	state->level = level;
}

static int
render_item_lod_stale(const struct bsg_lod_source_record *UNUSED(source),
		      struct bsg_view *UNUSED(v),
		      const struct bsg_lod_view_state *UNUSED(state),
		      void *UNUSED(policy_data))
{
    return 1;
}

struct render_item_failing_preview_state {
    int update_calls;
    int bounds_calls;
};

static int
render_item_failing_live_update(void *ctx, struct bsg_view *UNUSED(v))
{
    struct render_item_failing_preview_state *state =
	(struct render_item_failing_preview_state *)ctx;
    if (state)
	state->update_calls++;
    return -1;
}

static int
render_item_preview_bounds(void *ctx, point_t *bmin, point_t *bmax)
{
    struct render_item_failing_preview_state *state =
	(struct render_item_failing_preview_state *)ctx;
    if (state)
	state->bounds_calls++;
    if (!bmin || !bmax)
	return 0;
    VSET(*bmin, -2.0, -4.0, -6.0);
    VSET(*bmax, 2.0, 4.0, 6.0);
    return 1;
}

struct render_item_edit_preview_fanout_state {
    uint64_t revision;
    int update_calls;
    int update_returns;
};

struct render_item_preview_bounds_snapshot_state {
    uint64_t revision;
    int update_calls;
    int bounds_calls;
};

static uint64_t
render_item_edit_preview_revision(void *ctx)
{
    struct render_item_edit_preview_fanout_state *state =
	(struct render_item_edit_preview_fanout_state *)ctx;
    return state ? state->revision : 0;
}

static int
render_item_edit_preview_update(void *ctx, struct bsg_view *UNUSED(v))
{
    struct render_item_edit_preview_fanout_state *state =
	(struct render_item_edit_preview_fanout_state *)ctx;
    if (!state)
	return -1;
    state->update_calls++;
    if (state->update_returns > 0)
	state->revision++;
    return state->update_returns;
}

static uint64_t
render_item_preview_bounds_snapshot_revision(void *ctx)
{
    struct render_item_preview_bounds_snapshot_state *state =
	(struct render_item_preview_bounds_snapshot_state *)ctx;
    return state ? state->revision : 0;
}

static int
render_item_preview_bounds_snapshot_update(void *ctx,
					   struct bsg_view *UNUSED(v))
{
    struct render_item_preview_bounds_snapshot_state *state =
	(struct render_item_preview_bounds_snapshot_state *)ctx;
    if (!state)
	return -1;
    state->update_calls++;
    state->revision++;
    return 1;
}

static int
render_item_preview_bounds_snapshot_bounds(void *ctx,
					   point_t *bmin,
					   point_t *bmax)
{
    struct render_item_preview_bounds_snapshot_state *state =
	(struct render_item_preview_bounds_snapshot_state *)ctx;
    if (!state || !bmin || !bmax)
	return 0;
    state->bounds_calls++;
    if (state->bounds_calls == 1) {
	VSET(*bmin, -1.0, -2.0, -3.0);
	VSET(*bmax, 1.0, 2.0, 3.0);
    } else {
	VSET(*bmin, -10.0, -20.0, -30.0);
	VSET(*bmax, 10.0, 20.0, 30.0);
    }
    return 1;
}


/* ------------------------------------------------------------------ */
/* Test 2: bsg_render_phase enum ordering                              */
/* ------------------------------------------------------------------ */

static int
test_phase_enum_values(void)
{
    printf("=== Test 2: phase_enum_values ===\n");

    if (BSG_RENDER_PHASE_OPAQUE      != 0) FAIL("OPAQUE must be 0");
    if (BSG_RENDER_PHASE_TRANSPARENT != 1) FAIL("TRANSPARENT must be 1");
    if (BSG_RENDER_PHASE_OVERLAY     != 2) FAIL("OVERLAY must be 2");
    if (BSG_RENDER_PHASE_HUD         != 3) FAIL("HUD must be 3");
    if (BSG_RENDER_PHASE_COUNT       != 4) FAIL("COUNT must be 4");

    PASS("phase_enum_values");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: execute on empty tree returns 0                             */
/* ------------------------------------------------------------------ */

static int
test_empty_tree(void)
{
    printf("=== Test 3: empty_tree ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    int n = bsg_render_request_execute(req);
    if (n != 0) FAIL("empty tree should return 0");

    bsg_render_request_destroy(req);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("empty_tree");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: visible-only flag skips DOWN shapes                         */
/* ------------------------------------------------------------------ */

static int
test_visible_only(void)
{
    printf("=== Test 4: visible_only ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node_set_name(s1, "semantic_s1");
    bsg_node_set_name(s2, "semantic_s2");
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);

    bsg_node_set_visible_state(s1, 1);
    bsg_node_set_visible_state(s2, 0);

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("only 1 UP shape should be counted");

    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("visible_only");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4b: hidden parent suppresses visible child                     */
/* ------------------------------------------------------------------ */

static int
test_hidden_parent_visible_child(void)
{
    printf("=== Test 4b: hidden_parent_visible_child ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *group = bsg_group_create(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, group);
    bsg_node_add_child(group, s);
    bsg_node_set_visible_state(group, 0);
    bsg_node_set_visible_state(s, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "hidden parent items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 0) FAIL("hidden parent should suppress visible child");
    if (BU_PTBL_LEN(&items) != 0) FAIL("items table should be empty");

    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_node_destroy(group);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("hidden_parent_visible_child");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4c: per-view scope filters render traversal                    */
/* ------------------------------------------------------------------ */

static int
test_view_scope_render_filter(void)
{
    printf("=== Test 4c: view_scope_render_filter ===\n");

    struct bsg_view *v1 = _make_view();
    struct bsg_view *v2 = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v1);
    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v1, "scope");
    bsg_node *scope = (bsg_node *)scope_ref.opaque;
    bsg_node *s = bsg_shape_create(v1);
    bsg_node_add_child(root, scope);
    bsg_node_add_child(scope, s);
    bsg_node_set_visible_state(s, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "view scope items");

    struct bsg_render_request *req1 =
	bsg_render_request_create(v1, NULL);
    bsg_render_request_set_flags(req1,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req1, &items);

    int n = bsg_render_request_execute(req1);
    if (n != 1) FAIL("owner view should collect scoped child");
    if (BU_PTBL_LEN(&items) != 1) FAIL("owner items table should have one entry");
    bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, 0));
    bu_ptbl_reset(&items);
    bsg_render_request_destroy(req1);

    struct bsg_render_request *req2 =
	bsg_render_request_create(v2, NULL);
    bsg_render_request_set_flags(req2,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req2, &items);

    n = bsg_render_request_execute(req2);
    if (n != 0) FAIL("non-owner view should not collect scoped child");
    if (BU_PTBL_LEN(&items) != 0) FAIL("non-owner items table should be empty");

    bu_ptbl_free(&items);
    bsg_render_request_destroy(req2);
    bsg_shape_destroy(s);
    bsg_scene_ref_destroy(scope_ref);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v1);
    _free_view(v2);
    PASS("view_scope_render_filter");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: BSG_RENDER_FLAG_COLLECT_ITEMS populates output item table   */
/* ------------------------------------------------------------------ */

static int
test_collect_items(void)
{
    printf("=== Test 5: collect_items ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);
    bsg_node_set_visible_state(s1, 1);
    bsg_node_set_visible_state(s2, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "test items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 2) FAIL("should collect 2 items");
    if ((size_t)BU_PTBL_LEN(&items) != 2) FAIL("items table should have 2 entries");

    struct bsg_render_item *item0 =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    struct bsg_render_item *item1 =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 1);
    if (!item0->source.source_id || !item1->source.source_id)
	FAIL("items should carry semantic source identities");
    if (item0->source.source_id == item1->source.source_id)
	FAIL("distinct items should have distinct source identities");
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));

    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("collect_items");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 6: transparent shape lands in TRANSPARENT phase                */
/* ------------------------------------------------------------------ */

static int
test_transparent_phase(void)
{
    printf("=== Test 6: transparent_phase ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);

    bsg_appearance_set_transparency(s, 0.5);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "trans items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    bsg_render_request_execute(req);
    if (BU_PTBL_LEN(&items) != 1) FAIL("should collect 1 item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->phase != BSG_RENDER_PHASE_TRANSPARENT)
	FAIL("transparent shape should be in TRANSPARENT phase");
    if (!NEAR_EQUAL(item->appearance.transparency, 0.5, BN_TOL_DIST))
	FAIL("transparency value not preserved");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("transparent_phase");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 7: adapter draw callback is invoked once per visible shape     */
/* ------------------------------------------------------------------ */

static int g_draw_count = 0;

static void
_test_draw_cb(void *dmp, const struct bsg_render_item *item)
{
    (void)dmp;
    (void)item;
    g_draw_count++;
}

static int
test_adapter_draw(void)
{
    printf("=== Test 7: adapter_draw ===\n");
    g_draw_count = 0;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node *s3 = bsg_shape_create(v);
    bsg_node_set_name(s1, "near_alpha");
    bsg_node_set_name(s2, "far_alpha");
    bsg_node_set_name(s3, "middle_alpha");
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);
    bsg_node_add_child(root, s3);
    bsg_node_set_visible_state(s1, 1);
    bsg_node_set_visible_state(s2, 0);
    bsg_node_set_visible_state(s3, 1);

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.draw = _test_draw_cb;

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_adapter(req, &adapter);

    int n = bsg_render_request_execute(req);
    if (n != 2)          FAIL("should dispatch 2 visible shapes");
    if (g_draw_count != 2) FAIL("draw callback should be called twice");

    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_shape_destroy(s3);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("adapter_draw");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 8: adapter prepare + draw both invoked                         */
/* ------------------------------------------------------------------ */

static int g_prepare_count = 0;
static int g_draw2_count   = 0;

static int
_test_prepare_cb(void *dmp, const struct bsg_render_item *item)
{
    (void)dmp;
    (void)item;
    g_prepare_count++;
    return 1;
}

static void
_test_draw2_cb(void *dmp, const struct bsg_render_item *item)
{
    (void)dmp;
    (void)item;
    g_draw2_count++;
}

static int
test_adapter_prepare_draw(void)
{
    printf("=== Test 8: adapter_prepare_draw ===\n");
    g_prepare_count = 0;
    g_draw2_count   = 0;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.prepare = _test_prepare_cb;
    adapter.draw    = _test_draw2_cb;

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_adapter(req, &adapter);

    bsg_render_request_execute(req);
    if (g_prepare_count != 1) FAIL("prepare should be called once");
    if (g_draw2_count   != 1) FAIL("draw should be called once");

    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("adapter_prepare_draw");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 9: items in collect mode reflect model_mat identity by default */
/* ------------------------------------------------------------------ */

static int
test_item_model_mat_identity(void)
{
    printf("=== Test 9: item_model_mat_identity ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "mat items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    bsg_render_request_execute(req);
    if (BU_PTBL_LEN(&items) != 1) FAIL("should have 1 item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);

    mat_t eye;
    MAT_IDN(eye);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    if (!bn_mat_is_equal(item->model_mat, eye, &tol))
	FAIL("model_mat should be identity when no transform node present");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("item_model_mat_identity");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 10: NULL request returns -1                                    */
/* ------------------------------------------------------------------ */

static int
test_null_request(void)
{
    printf("=== Test 10: null_request ===\n");
    int n = bsg_render_request_execute(NULL);
    if (n != -1) FAIL("execute(NULL) should return -1");
    PASS("null_request");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 11: sorted alpha orders transparent items back-to-front        */
/* ------------------------------------------------------------------ */

static int
test_sorted_alpha(void)
{
    printf("=== Test 11: sorted_alpha ===\n");

    struct bsg_view *v = _make_view();
    MAT_IDN(v->gv_model2view);

    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *xf1 = bsg_transform_create(v);
    bsg_node *xf2 = bsg_transform_create(v);
    bsg_node *xf3 = bsg_transform_create(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node *s3 = bsg_shape_create(v);
    bsg_node_add_child(root, xf1);
    bsg_node_add_child(root, xf2);
    bsg_node_add_child(root, xf3);
    bsg_node_add_child(xf1, s1);
    bsg_node_add_child(xf2, s2);
    bsg_node_add_child(xf3, s3);

    bsg_node_set_visible_state(s1, 1);
    bsg_node_set_visible_state(s2, 1);
    bsg_node_set_visible_state(s3, 1);
    bsg_appearance_set_transparency(s1, 0.5);
    bsg_appearance_set_transparency(s2, 0.5);
    bsg_appearance_set_transparency(s3, 0.5);

    mat_t m1, m2, m3;
    MAT_IDN(m1);
    MAT_IDN(m2);
    MAT_IDN(m3);
    /* Use three distinct negative view-space Z depths so the expected
     * back-to-front order is unambiguous: far (-5), middle (-3), near (-1). */
    MAT_DELTAS(m1, 0.0, 0.0, -1.0);
    MAT_DELTAS(m2, 0.0, 0.0, -5.0);
    MAT_DELTAS(m3, 0.0, 0.0, -3.0);
    bsg_transform_set_matrix(xf1, m1);
    bsg_transform_set_matrix(xf2, m2);
    bsg_transform_set_matrix(xf3, m3);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "sorted alpha items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_SORTED_ALPHA);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 3) FAIL("should collect 3 transparent items");
    if (BU_PTBL_LEN(&items) != 3) FAIL("items table should have 3 entries");

    struct bsg_render_item *i0 =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    struct bsg_render_item *i1 =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 1);
    struct bsg_render_item *i2 =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 2);

    /* With the identity gv_model2view, points in front of the camera have
     * negative Z.  Back-to-front sorting therefore expects the items table
     * to contain Z=-5 at index 0, Z=-3 at index 1, and Z=-1 at index 2. */
    if (!(i0->sort_key > i1->sort_key && i1->sort_key > i2->sort_key))
	FAIL("transparent items should sort back-to-front by transformed depth");
    if (!(i0->sort_key > i1->sort_key && i1->sort_key > i2->sort_key))
	FAIL("sort keys should be in descending order");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	bsg_render_item_free(item);
    }
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_shape_destroy(s3);
    bsg_transform_destroy(xf1);
    bsg_transform_destroy(xf2);
    bsg_transform_destroy(xf3);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("sorted_alpha");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 12: HUD pass sorts by hud sort_order                           */
/* ------------------------------------------------------------------ */

static int
test_hud_phase_sort(void)
{
    printf("=== Test 12: hud_phase_sort ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root) FAIL("failed to create HUD root");

    bsg_node *c0 = bsg_node_child_at(root, BSG_HUD_FEATURE_CENTER_DOT);
    bsg_node *c1 = bsg_node_child_at(root, BSG_HUD_FEATURE_MODEL_AXES);
    bsg_node *c2 = bsg_node_child_at(root, BSG_HUD_FEATURE_VIEW_PARAMS);
    if (!c0 || !c1 || !c2) FAIL("missing HUD children");

    bsg_node_set_visible_state(c0, 1);
    bsg_node_set_visible_state(c1, 1);
    bsg_node_set_visible_state(c2, 1);

    struct bsg_hud_node_meta *m0 = bsg_hud_node_get_meta(c0);
    struct bsg_hud_node_meta *m1 = bsg_hud_node_get_meta(c1);
    struct bsg_hud_node_meta *m2 = bsg_hud_node_get_meta(c2);
    if (!m0 || !m1 || !m2) FAIL("missing HUD metadata");
    m0->sort_order = 30;
    m1->sort_order = 10;
    m2->sort_order = 20;

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "hud phase items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_HUD_PASS);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 3) FAIL("should collect 3 HUD items");
    if (BU_PTBL_LEN(&items) != 3) FAIL("items table should have 3 entries");

    struct bsg_render_item *i0 = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    struct bsg_render_item *i1 = (struct bsg_render_item *)BU_PTBL_GET(&items, 1);
    struct bsg_render_item *i2 = (struct bsg_render_item *)BU_PTBL_GET(&items, 2);

    if (i0->features.hud_feature_type != BSG_HUD_FEATURE_MODEL_AXES ||
	    i1->features.hud_feature_type != BSG_HUD_FEATURE_VIEW_PARAMS ||
	    i2->features.hud_feature_type != BSG_HUD_FEATURE_CENTER_DOT)
	FAIL("HUD items should sort in ascending sort_order");
    if (!(i0->sort_key < i1->sort_key && i1->sort_key < i2->sort_key))
	FAIL("HUD sort keys should be ascending");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	bsg_render_item_free(item);
    }
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_hud_root_destroy(v);
    _free_view(v);
    PASS("hud_phase_sort");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Test 13: capabilities callback is queried during execute            */
/* ------------------------------------------------------------------ */

static int g_caps_count = 0;

static unsigned int
_test_caps_cb(void *UNUSED(dmp))
{
    g_caps_count++;
    return BSG_ADAPTER_CAP_SORTED_ALPHA | BSG_ADAPTER_CAP_TRANSPARENCY;
}

static int
test_adapter_capabilities(void)
{
    printf("=== Test 13: adapter_capabilities ===\n");
    g_caps_count = 0;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.capabilities = _test_caps_cb;

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_SORTED_ALPHA);
    bsg_render_request_set_adapter(req, &adapter);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should dispatch one shape");
    if (g_caps_count != 1) FAIL("capabilities should be called once per execute");

    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("adapter_capabilities");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Test 14: invalidate/free lifecycle callbacks are callable           */
/* ------------------------------------------------------------------ */

static int g_invalidate_count = 0;
static int g_free_count = 0;

static void
_test_invalidate_cb(void *UNUSED(dmp), const struct bsg_render_item *UNUSED(item),
		    unsigned int UNUSED(reason_mask))
{
    g_invalidate_count++;
}

static void
_test_free_cb(void *UNUSED(dmp), const struct bsg_render_item *UNUSED(item))
{
    g_free_count++;
}

static int
test_adapter_invalidate_free(void)
{
    printf("=== Test 14: adapter_invalidate_free ===\n");
    g_invalidate_count = 0;
    g_free_count = 0;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "invalidate_free items");
    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);
    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one shape item");

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.invalidate = _test_invalidate_cb;
    adapter.free = _test_free_cb;

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	adapter.invalidate(NULL, item, BSG_INVALIDATE_ALL);
	adapter.free(NULL, item);
	bsg_render_item_free(item);
    }
    if (g_invalidate_count != 1) FAIL("invalidate callback should be called once");
    if (g_free_count != 1) FAIL("free callback should be called once");

    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("adapter_invalidate_free");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 15: render traversal applies inherited appearance stack         */
/* ------------------------------------------------------------------ */

static int
test_render_inherited_appearance(void)
{
    printf("=== Test 15: render_inherited_appearance ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *group = bsg_group_create(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, group);
    bsg_node_add_child(group, s);
    bsg_node_set_visible_state(s, 1);

    bsg_material_set_rgb(s, 25, 50, 75);

    const unsigned char group_color[3] = {100, 150, 200};
    bsg_appearance_set_color_override(group, group_color, 1);
    bsg_appearance_set_transparency(group, 0.5);
    bsg_appearance_set_dmode(group, 2);
    bsg_appearance_set_line_width(group, 4);
    bsg_appearance_set_inherited_by_children(group, 1);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "inherited appearance items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one inherited item");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one entry");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->appearance.color[0] != 100) FAIL("inherited color[0] should resolve");
    if (item->appearance.color[1] != 150) FAIL("inherited color[1] should resolve");
    if (item->appearance.color[2] != 200) FAIL("inherited color[2] should resolve");
    if (!NEAR_EQUAL(item->appearance.transparency, 0.5, BN_TOL_DIST))
	FAIL("inherited transparency should resolve");
    if (item->appearance.draw_mode != 2) FAIL("inherited draw mode should resolve");
    if (item->appearance.line_width != 4) FAIL("inherited line width should resolve");
    if (!(item->appearance.active_layers & BSG_ALAY_INHERITED))
	FAIL("inherited layer should be recorded");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_node_destroy(group);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("render_inherited_appearance");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 16: local shape appearance overrides inherited group style      */
/* ------------------------------------------------------------------ */

static int
test_render_local_appearance_priority(void)
{
    printf("=== Test 16: render_local_appearance_priority ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *group = bsg_group_create(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, group);
    bsg_node_add_child(group, s);
    bsg_node_set_visible_state(s, 1);

    const unsigned char group_color[3] = {100, 150, 200};
    bsg_appearance_set_color_override(group, group_color, 1);
    bsg_appearance_set_transparency(group, 0.5);
    bsg_appearance_set_dmode(group, 2);
    bsg_appearance_set_line_width(group, 4);
    bsg_appearance_set_inherited_by_children(group, 1);

    const unsigned char shape_color[3] = {10, 20, 30};
    bsg_appearance_set_color_override(s, shape_color, 1);
    bsg_appearance_set_transparency(s, 0.25);
    bsg_appearance_set_dmode(s, 4);
    bsg_appearance_set_line_width(s, 7);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "local appearance items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one local-priority item");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one entry");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->appearance.color[0] != 10) FAIL("local color[0] should resolve");
    if (item->appearance.color[1] != 20) FAIL("local color[1] should resolve");
    if (item->appearance.color[2] != 30) FAIL("local color[2] should resolve");
    if (!NEAR_EQUAL(item->appearance.transparency, 0.25, BN_TOL_DIST))
	FAIL("local transparency should resolve");
    if (item->appearance.draw_mode != 4) FAIL("local draw mode should resolve");
    if (item->appearance.line_width != 7) FAIL("local line width should resolve");
    if (!(item->appearance.active_layers & BSG_ALAY_INHERITED))
	FAIL("inherited layer should still be recorded");
    if (!(item->appearance.active_layers & BSG_ALAY_COMMAND))
	FAIL("local layer should be recorded");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_node_destroy(group);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("render_local_appearance_priority");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 17: render item carries resolved backend semantics             */
/* ------------------------------------------------------------------ */

static int
test_backend_item_semantics(void)
{
    printf("=== Test 17: backend_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    vect_t center = VINIT_ZERO;
    point_t end = VINIT_ZERO;
    VSET(center, 1.0, 2.0, 3.0);
    VSET(end, 2.0, 2.0, 3.0);
    bsg_node *s = test_line_node_create(v, root, "backend_semantics", center, end);
    if (!s)
	FAIL("backend semantics line node create");
    bsg_node_set_name(s, "backend_semantics");
    bsg_node_set_visible_state(s, 1);
    bsg_appearance_set_force_draw(s, 1);
    bsg_appearance_set_highlighted(s, 1);
    bsg_shape_set_non_database_source(s, 0);
    if (bsg_shape_is_non_database_source(s))
	FAIL("non-database source metadata should be clear");
    bsg_shape_set_non_database_source(s, 1);
    if (!bsg_shape_is_non_database_source(s))
	FAIL("non-database source metadata should be set");
    struct bsg_payload *payload = bsg_node_get_payload(s);
    if (payload)
	FAIL("typed line node should not need payload");
    bsg_node_set_draw_size(s, 42.0);
    bsg_node_set_draw_center(s, center);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "backend semantics items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one semantic item");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one entry");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item->name || bu_strcmp(item->name, "backend_semantics") != 0)
	FAIL("source name should be carried");
    if (item->source.scope != BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED)
	FAIL("source scope should describe a view-scoped source");
    if (item->source.geometry_role != BSG_RENDER_GEOMETRY_ROLE_UNKNOWN &&
	    item->source.geometry_role != BSG_RENDER_GEOMETRY_ROLE_LINE_SET)
	FAIL("source geometry role should describe line geometry when known");
    if (!item->cache_identity) FAIL("cache identity should be populated");
    if (!item->source.source_id) FAIL("source id should be populated");
    if (!item->source.geometry_id) FAIL("geometry source handle should be populated");
    if (!item->context.settings_hash) FAIL("render settings hash should be populated");
    if (item->context.request_flags != bsg_render_request_flags(req)) FAIL("request flags should be captured");
    if (item->context.viewport_width != v->gv_width ||
	    item->context.viewport_height != v->gv_height)
	FAIL("viewport should be captured");
    if (!item->visible) FAIL("effective visibility should be carried");
    if (!item->force_draw) FAIL("force-draw should be carried");
    if (!item->highlighted) FAIL("highlight should be carried");
    if (item->selected) FAIL("explicit highlight should not imply selection");
    if (!item->non_database_source) FAIL("non-database source flag should be carried");
    if (!NEAR_EQUAL(item->bounds_radius, 42.0, BN_TOL_DIST))
	FAIL("bounds radius should be carried");
    if (!NEAR_EQUAL(item->bounds_center[X], 1.0, BN_TOL_DIST) ||
	!NEAR_EQUAL(item->bounds_center[Y], 2.0, BN_TOL_DIST) ||
	!NEAR_EQUAL(item->bounds_center[Z], 3.0, BN_TOL_DIST))
	FAIL("bounds center should be carried");
    if (item->source.lod_id != 0) FAIL("non-LoD item should not report LoD identity");
    if (item->lod_level != -1) FAIL("non-LoD item should report lod_level -1");
    if (item->lod_level_count != 0) FAIL("non-LoD item should report zero levels");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend_item_semantics");
    return 0;
}

static int
test_feature_item_semantics(void)
{
    printf("=== Test 18: feature_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    VSET(p1, 1.0, 0.0, 0.0);
    bsg_node *s = test_line_node_create(v, root, "feature_semantics",
	    p0, p1);
    if (!s)
	FAIL("feature line node create");
    bsg_node_set_visible_state(s, 1);

    bsg_shape_set_arrows_enabled(s, 1);
    bsg_shape_set_arrow_tip_length(s, 0.25);
    bsg_shape_set_arrow_tip_width(s, 0.125);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "feature semantics items");
    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);
    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one arrow feature item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!(item->features.flags & BSG_RENDER_FEATURE_ARROWS))
	FAIL("arrow feature flag should be carried");
    if (!NEAR_EQUAL(item->features.arrow_tip_length, 0.25, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->features.arrow_tip_width, 0.125, BN_TOL_DIST))
	FAIL("arrow dimensions should be carried");
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("arrow feature should retain typed line geometry");
    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);

    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);

    root = bsg_view_scene_root_ensure(v);
    s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_visible_state(s, 1);
    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    bu_vls_init(&label->label);
    bu_vls_strcpy(&label->label, "feature label");
    VSET(label->p, 1.0, 2.0, 3.0);
    VSET(label->target, 4.0, 5.0, 6.0);
    label->size = 1;
    label->line_flag = 1;
    label->anchor = BSG_ANCHOR_TOP_RIGHT;
    label->arrow = 1;
    bsg_node_set_payload(s, bsg_payload_text_create(label));

    bu_ptbl_init(&items, 4, "text feature semantics items");
    req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);
    n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one text feature item");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_TEXT)
	FAIL("text geometry kind should be carried");
    if (!item->geometry.text.label ||
	    !BU_STR_EQUAL(item->geometry.text.label, "feature label") ||
	    !VNEAR_EQUAL(item->geometry.text.position, label->p, SMALL_FASTF) ||
	    !VNEAR_EQUAL(item->geometry.text.target, label->target, SMALL_FASTF) ||
	    item->geometry.text.size != label->size ||
	    item->geometry.text.line_flag != label->line_flag ||
	    item->geometry.text.anchor != label->anchor ||
	    item->geometry.text.arrow != label->arrow)
	FAIL("text geometry record should snapshot label fields");
    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);

    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("feature_item_semantics");
    return 0;
}

static int
test_image_framebuffer_item_semantics(void)
{
    printf("=== Test 18b: image_framebuffer_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root)
	FAIL("image/framebuffer root create");

    bsg_image_ref image = bsg_image_ref_create(v, "render_image");
    bsg_node *image_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_image_ref_as_node(image)));
    if (!image_node || !bsg_image_ref_set_metadata(image, 64, 32, 4))
	FAIL("image metadata setup");
    bsg_node_add_child(root, image_node);

    unsigned char px[8] = {255, 0, 0, 255, 0, 255, 0, 255};
    struct bsg_payload *payload_image = bsg_payload_image_create(2, 1, 4, px);
    struct bsg_payload_image *source_image =
	bsg_payload_image_get(payload_image);
    if (!payload_image || !source_image || !source_image->pixels)
	FAIL("payload image setup");
    bsg_node *payload_shape = bsg_shape_create(v);
    bsg_node_set_name(payload_shape, "render_payload_image");
    bsg_node_add_child(root, payload_shape);
    bsg_node_set_payload(payload_shape, payload_image);

    bsg_framebuffer_ref framebuffer =
	bsg_framebuffer_ref_create(v, "render_framebuffer");
    bsg_node *framebuffer_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_framebuffer_ref_as_node(framebuffer)));
    if (!framebuffer_node || !bsg_framebuffer_ref_set_mode(framebuffer, 2))
	FAIL("framebuffer setup");
    bsg_node_add_child(root, framebuffer_node);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "image/framebuffer items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 3 || BU_PTBL_LEN(&items) != 3)
	FAIL("should collect image, payload image, and framebuffer items");

    int saw_image = 0;
    int saw_payload_image = 0;
    int saw_framebuffer = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	if (item->geometry.kind != BSG_RENDER_GEOMETRY_IMAGE)
	    FAIL("image/framebuffer items should use image geometry kind");

	if (item->name && BU_STR_EQUAL(item->name, "render_image")) {
	    saw_image = 1;
	    if (item->geometry.image.kind != BSG_RENDER_IMAGE_RASTER ||
		    item->geometry.image.width != 64 ||
		    item->geometry.image.height != 32 ||
		    item->geometry.image.channels != 4 ||
		    item->geometry.image.pixel_count != 0 ||
		    item->geometry.image.pixels)
		FAIL("image metadata should render as a typed raster record");
	}
	if (item->name && BU_STR_EQUAL(item->name, "render_payload_image")) {
	    saw_payload_image = 1;
	    if (item->geometry.image.kind != BSG_RENDER_IMAGE_RASTER ||
		    item->geometry.image.width != 2 ||
		    item->geometry.image.height != 1 ||
		    item->geometry.image.channels != 4 ||
		    item->geometry.image.pixel_count != sizeof(px) ||
		    !item->geometry.image.pixels ||
		    item->geometry.image.pixels == source_image->pixels ||
		    memcmp(item->geometry.image.pixels, px, sizeof(px)) != 0)
		FAIL("payload image should render as an owned raster record");
	}
	if (item->name && BU_STR_EQUAL(item->name, "render_framebuffer")) {
	    saw_framebuffer = 1;
	    if (item->geometry.image.kind != BSG_RENDER_IMAGE_FRAMEBUFFER ||
		    item->geometry.image.framebuffer_mode != 2 ||
		    item->geometry.image.pixel_count != 0 ||
		    item->geometry.image.pixels)
		FAIL("framebuffer should render as a typed framebuffer record");
	}
    }
    if (!saw_image || !saw_payload_image || !saw_framebuffer)
	FAIL("missing expected image/framebuffer item");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(payload_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("image_framebuffer_item_semantics");
    return 0;
}

static int
test_annotation_item_semantics(void)
{
    printf("=== Test 18c: annotation_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape)
	FAIL("annotation scene setup");
    bsg_node_set_name(shape, "render_annotation");
    bsg_node_add_child(root, shape);
    bsg_scene_ref shape_ref = { shape };
    bsg_scene_set_color(shape_ref, 17, 34, 51);
    bsg_scene_set_transparency(shape_ref, 0.5);
    bsg_scene_set_dmode(shape_ref, 2);
    bsg_scene_set_line_width(shape_ref, 3);

    point_t pts[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 1.0, 2.0, 3.0);
    VSET(pts[1], 4.0, 5.0, 6.0);
    VSET(pts[2], 7.0, 8.0, 9.0);
    VSET(pts[3], 10.0, 11.0, 12.0);
    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    MAT_DELTAS(model_mat, 0.25, 0.5, 0.75);
    MAT_DELTAS(display_mat, 1.25, 1.5, 1.75);
    fastf_t nurb_knots[6] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    int nurb_controls[3] = {0, 1, 2};
    fastf_t nurb_weights[3] = {1.0, 0.5, 1.0};
    int bezier_controls[4] = {0, 1, 2, 3};
    struct bsg_annotation_segment segs[5];
    memset(segs, 0, sizeof(segs));
    segs[0].kind = BSG_ANNOTATION_SEGMENT_LINE;
    segs[0].data.line.start = 0;
    segs[0].data.line.end = 1;
    segs[1].kind = BSG_ANNOTATION_SEGMENT_CARC;
    segs[1].reverse = 1;
    segs[1].data.carc.start = 1;
    segs[1].data.carc.end = 2;
    segs[1].data.carc.radius = 2.5;
    segs[1].data.carc.center_is_left = 1;
    segs[1].data.carc.orientation = 1;
    segs[1].data.carc.center = 3;
    segs[2].kind = BSG_ANNOTATION_SEGMENT_NURB;
    segs[2].data.nurb.order = 3;
    segs[2].data.nurb.point_type = 3;
    segs[2].data.nurb.knot_count = 6;
    segs[2].data.nurb.knots = nurb_knots;
    segs[2].data.nurb.control_point_count = 3;
    segs[2].data.nurb.control_points = nurb_controls;
    segs[2].data.nurb.weights = nurb_weights;
    segs[3].kind = BSG_ANNOTATION_SEGMENT_BEZIER;
    segs[3].data.bezier.degree = 3;
    segs[3].data.bezier.control_point_count = 4;
    segs[3].data.bezier.control_points = bezier_controls;
    segs[4].kind = BSG_ANNOTATION_SEGMENT_TEXT;
    segs[4].data.text.ref_pt = 3;
    segs[4].data.text.relative_position = 1;
    segs[4].data.text.text = (char *)"annotation note";
    segs[4].data.text.size = 12.0;
    segs[4].data.text.rotation = 45.0;
    struct bsg_payload *payload =
	bsg_payload_annotation_create_record("annotation note",
		BSG_ANNOTATION_SPACE_DISPLAY, pts[0], model_mat, display_mat,
		(const point_t *)pts, 4, segs, 5);
    struct bsg_payload_annotation *source_annotation =
	bsg_payload_annotation_get(payload);
    if (!payload || !source_annotation || !source_annotation->points)
	FAIL("annotation payload setup");
    bsg_node_set_payload(shape, payload);

    bsg_feature_ref null_feature = BSG_FEATURE_REF_NULL_INIT;
    struct bsg_interaction_record *selection_record =
	bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
		null_feature, "render_annotation", NULL);
    if (!selection_record || !bsg_view_selection(v))
	FAIL("annotation selection setup");
    bsg_selection_add_record(bsg_view_selection(v), selection_record);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "annotation items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1 || BU_PTBL_LEN(&items) != 1)
	FAIL("should collect one annotation item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_ANNOTATION)
	FAIL("annotation should render as annotation geometry");
    if (item->source.source_id == 0 ||
	    item->source.geometry_id != item->geometry.source_identity ||
	    item->geometry.source_identity !=
	    (uint64_t)(uintptr_t)source_annotation)
	FAIL("annotation should preserve source identity");
    if (item->phase != BSG_RENDER_PHASE_TRANSPARENT ||
	    item->selected != 1 ||
	    item->highlighted != 1 ||
	    item->appearance.highlighted != 1 ||
	    item->appearance.color[0] != 17 ||
	    item->appearance.color[1] != 34 ||
	    item->appearance.color[2] != 51 ||
	    !NEAR_EQUAL(item->appearance.transparency, 0.5, SMALL_FASTF) ||
	    item->appearance.draw_mode != 2 ||
	    item->appearance.line_width != 3)
	FAIL("annotation should preserve appearance and selection semantics");
    if (!item->geometry.annotation.text ||
	    !BU_STR_EQUAL(item->geometry.annotation.text, "annotation note") ||
	    item->geometry.annotation.text == bu_vls_cstr(&source_annotation->text) ||
	    item->geometry.annotation.space != BSG_ANNOTATION_SPACE_DISPLAY ||
	    !VNEAR_EQUAL(item->geometry.annotation.anchor, pts[0], SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.model_mat[MDX], 0.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.model_mat[MDY], 0.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.model_mat[MDZ], 0.75, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.display_mat[MDX], 1.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.display_mat[MDY], 1.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.display_mat[MDZ], 1.75, SMALL_FASTF) ||
	    item->geometry.annotation.point_count != 4 ||
	    !item->geometry.annotation.points ||
	    item->geometry.annotation.points == source_annotation->points ||
	    !VNEAR_EQUAL(item->geometry.annotation.points[0], pts[0], SMALL_FASTF) ||
	    !VNEAR_EQUAL(item->geometry.annotation.points[3], pts[3], SMALL_FASTF) ||
	    item->geometry.annotation.segment_count != 5 ||
	    !item->geometry.annotation.segments ||
	    item->geometry.annotation.segments == source_annotation->segments ||
	    item->geometry.annotation.segments[0].kind != BSG_ANNOTATION_SEGMENT_LINE ||
	    item->geometry.annotation.segments[0].data.line.start != 0 ||
	    item->geometry.annotation.segments[0].data.line.end != 1 ||
	    item->geometry.annotation.segments[1].kind != BSG_ANNOTATION_SEGMENT_CARC ||
	    item->geometry.annotation.segments[1].reverse != 1 ||
	    !NEAR_EQUAL(item->geometry.annotation.segments[1].data.carc.radius, 2.5, SMALL_FASTF) ||
	    item->geometry.annotation.segments[1].data.carc.center != 3 ||
	    item->geometry.annotation.segments[2].kind != BSG_ANNOTATION_SEGMENT_NURB ||
	    item->geometry.annotation.segments[2].data.nurb.knots == source_annotation->segments[2].data.nurb.knots ||
	    item->geometry.annotation.segments[2].data.nurb.control_points == source_annotation->segments[2].data.nurb.control_points ||
	    item->geometry.annotation.segments[2].data.nurb.weights == source_annotation->segments[2].data.nurb.weights ||
	    item->geometry.annotation.segments[2].data.nurb.knot_count != 6 ||
	    item->geometry.annotation.segments[2].data.nurb.control_point_count != 3 ||
	    !NEAR_EQUAL(item->geometry.annotation.segments[2].data.nurb.knots[3], 1.0, SMALL_FASTF) ||
	    item->geometry.annotation.segments[2].data.nurb.control_points[2] != 2 ||
	    !NEAR_EQUAL(item->geometry.annotation.segments[2].data.nurb.weights[1], 0.5, SMALL_FASTF) ||
	    item->geometry.annotation.segments[3].kind != BSG_ANNOTATION_SEGMENT_BEZIER ||
	    item->geometry.annotation.segments[3].data.bezier.control_points == source_annotation->segments[3].data.bezier.control_points ||
	    item->geometry.annotation.segments[3].data.bezier.degree != 3 ||
	    item->geometry.annotation.segments[3].data.bezier.control_point_count != 4 ||
	    item->geometry.annotation.segments[3].data.bezier.control_points[3] != 3 ||
	    item->geometry.annotation.segments[4].kind != BSG_ANNOTATION_SEGMENT_TEXT ||
	    item->geometry.annotation.segments[4].data.text.text == source_annotation->segments[4].data.text.text ||
	    !BU_STR_EQUAL(item->geometry.annotation.segments[4].data.text.text, "annotation note") ||
	    item->geometry.annotation.segments[4].data.text.ref_pt != 3 ||
	    item->geometry.annotation.segments[4].data.text.relative_position != 1 ||
	    !NEAR_EQUAL(item->geometry.annotation.segments[4].data.text.size, 12.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(item->geometry.annotation.segments[4].data.text.rotation, 45.0, SMALL_FASTF))
	FAIL("annotation should render as an owned annotation record");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_interaction_record_free(selection_record);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("annotation_item_semantics");
    return 0;
}

static int
test_indexed_face_surface_item_semantics(void)
{
    printf("=== Test 18d: indexed_face_surface_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_indexed_face_set_ref faces =
	bsg_indexed_face_set_ref_create(v, "render_surface_faces");
    bsg_node *face_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_indexed_face_set_ref_as_node(faces)));
    if (!root || !face_node)
	FAIL("indexed face surface scene setup");

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t normals[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    int indices[4] = {0, 1, 2, -1};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 0.0, 1.0, 0.0);
    for (size_t i = 0; i < 3; i++)
	VSET(normals[i], 0.0, 0.0, 1.0);

    if (!bsg_geometry_ref_set_indexed_face_set(
		bsg_indexed_face_set_ref_as_geometry(faces),
		(const point_t *)pts, 3, (const vect_t *)normals, 3,
		indices, 4))
	FAIL("indexed face surface geometry setup");

    unsigned char color[3] = {30, 80, 160};
    bsg_appearance_set_color_override(face_node, color, 1);
    bsg_appearance_set_transparency(face_node, 0.75);
    bsg_appearance_set_dmode(face_node, 2);
    bsg_appearance_set_line_width(face_node, 5);
    bsg_node_add_child(root, face_node);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "indexed face surface items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1 || BU_PTBL_LEN(&items) != 1)
	FAIL("should collect one indexed face surface item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	FAIL("indexed face should render as indexed-face geometry");
    if (item->geometry.surface.point_count != 3 ||
	    item->geometry.surface.normal_count != 3 ||
	    item->geometry.surface.index_count != 4 ||
	    item->geometry.surface.face_count != 1 ||
	    item->geometry.surface.normal_kind != BSG_RENDER_SURFACE_NORMALS_PER_INDEX ||
	    !item->geometry.surface.points ||
	    !item->geometry.surface.normals ||
	    !item->geometry.surface.indices)
	FAIL("indexed face should render as an owned surface record");
    if (item->geometry.surface.points == item->geometry.arrays.points ||
	    item->geometry.surface.normals == (vect_t *)item->geometry.arrays.normals ||
	    item->geometry.surface.indices == item->geometry.arrays.indices)
	FAIL("surface record should not alias compatibility arrays");
    if (!VNEAR_EQUAL(item->geometry.surface.points[2], pts[2], SMALL_FASTF) ||
	    !VNEAR_EQUAL(item->geometry.surface.normals[1], normals[1], SMALL_FASTF) ||
	    item->geometry.surface.indices[3] != -1)
	FAIL("surface record should preserve indexed-face arrays");
    if (!item->geometry.surface.material_valid ||
	    item->geometry.surface.material.color[0] != color[0] ||
	    item->geometry.surface.material.color[1] != color[1] ||
	    item->geometry.surface.material.color[2] != color[2] ||
	    !NEAR_EQUAL(item->geometry.surface.material.transparency, 0.75, SMALL_FASTF) ||
	    item->geometry.surface.material.draw_mode != 2 ||
	    item->geometry.surface.material.line_width != 5)
	FAIL("surface record should carry resolved material intent");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("indexed_face_surface_item_semantics");
    return 0;
}

static int
test_indexed_face_surface_normal_kinds(void)
{
    printf("=== Test 18e: indexed_face_surface_normal_kinds ===\n");

    point_t pts[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 1.0, 0.0);
    VSET(pts[3], 0.0, 1.0, 0.0);
    int indices[8] = {0, 1, 2, -1, 0, 2, 3, -1};
    vect_t normals[6] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO,
	VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    for (size_t i = 0; i < 6; i++)
	VSET(normals[i], 0.0, 0.0, 1.0 + (fastf_t)i);

    struct normal_kind_case {
	const char *name;
	size_t normal_count;
	bsg_render_surface_normal_kind expected;
    } cases[] = {
	{"per-face", 2, BSG_RENDER_SURFACE_NORMALS_PER_FACE},
	{"per-vertex", 4, BSG_RENDER_SURFACE_NORMALS_PER_VERTEX},
	{"per-index", 6, BSG_RENDER_SURFACE_NORMALS_PER_INDEX},
	{"none", 0, BSG_RENDER_SURFACE_NORMALS_NONE}
    };

    for (size_t ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ci++) {
	struct bsg_view *v = _make_view();
	bsg_node *root = bsg_view_scene_root_ensure(v);
	bsg_indexed_face_set_ref faces =
	    bsg_indexed_face_set_ref_create(v, cases[ci].name);
	bsg_node *face_node = bsg_object_ref_node(
		bsg_node_ref_object(bsg_indexed_face_set_ref_as_node(faces)));
	if (!root || !face_node)
	    FAIL("indexed face normal-kind scene setup");

	const vect_t *case_normals = cases[ci].normal_count ?
	    (const vect_t *)normals : NULL;
	if (!bsg_geometry_ref_set_indexed_face_set(
		    bsg_indexed_face_set_ref_as_geometry(faces),
		    (const point_t *)pts, 4, case_normals,
		    cases[ci].normal_count, indices, 8))
	    FAIL("indexed face normal-kind geometry setup");
	bsg_node_add_child(root, face_node);

	struct bu_ptbl items;
	bu_ptbl_init(&items, 4, "indexed face normal-kind items");
	struct bsg_render_request *req = bsg_render_request_create(v, NULL);
	bsg_render_request_set_flags(req,
		BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
	bsg_render_request_set_output_items(req, &items);

	int n = bsg_render_request_execute(req);
	if (n != 1 || BU_PTBL_LEN(&items) != 1)
	    FAIL("should collect one indexed face normal-kind item");

	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
	if (item->geometry.surface.face_count != 2 ||
		item->geometry.surface.normal_count != cases[ci].normal_count ||
		item->geometry.surface.normal_kind != cases[ci].expected)
	    FAIL("indexed face normal-kind classification wrong");
	if (cases[ci].normal_count &&
		!VNEAR_EQUAL(item->geometry.surface.normals[
			cases[ci].normal_count - 1],
		    normals[cases[ci].normal_count - 1], SMALL_FASTF))
	    FAIL("indexed face normal-kind normals not preserved");

	bsg_render_item_free(item);
	bu_ptbl_free(&items);
	bsg_render_request_destroy(req);
	bsg_view_scene_root_detach_from_root(root);
	_free_view(v);
    }

    PASS("indexed_face_surface_normal_kinds");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 18: render item derives highlight from active view selection   */
/* ------------------------------------------------------------------ */

static int
test_selection_render_semantics(void)
{
    printf("=== Test 18: selection_render_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s = bsg_shape_create(v);
    bsg_node_add_child(root, s);
    bsg_node_set_name(s, "selected_semantics");
    bsg_appearance_set_highlighted(s, 0);
    struct bsg_interaction_record *rec =
	bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
		(bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT, "selected_semantics", NULL);
    bsg_selection_add_record(bsg_view_selection(v), rec);
    bsg_interaction_record_free(rec);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "selection semantics items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one selected item");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one entry");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item->selected) FAIL("selected should come from the active view selection set");
    if (!item->highlighted) FAIL("selected item should render highlighted");
    if (!item->appearance.highlighted) FAIL("resolved appearance should render selection highlight");
    if (bsg_appearance_is_highlighted(s)) FAIL("selection render should not mutate node highlight storage");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("selection_render_semantics");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 19: render item carries selected LoD decision                  */
/* ------------------------------------------------------------------ */

static int
test_backend_item_lod_decision(void)
{
    printf("=== Test 19: backend_item_lod_decision ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s0 = bsg_shape_create(v);
    bsg_node_add_child(root, s0);
    bsg_node_set_name(s0, "lod_source_shape");

    struct render_item_lod_state st = {1};
    struct bsg_lod_source_policy policy = BSG_LOD_SOURCE_POLICY_INIT;
    policy.kind = BSG_LOD_SOURCE_TEST;
    policy.identity = 0x1901;
    policy.name = "render_item_lod";
    policy.policy_data = &st;
    policy.select = render_item_lod_select;
    policy.activate = render_item_lod_activate;
    policy.stale = render_item_lod_stale;
    bsg_lod_source_ref lod_ref = bsg_lod_source_insert_above(s0, v, &policy);
    if (bsg_lod_source_ref_is_null(lod_ref)) FAIL("LoD source should be created");

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "backend lod items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect selected LoD item only");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one LoD item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item->source.name || bu_strcmp(item->source.name, "lod_source_shape") != 0) FAIL("selected LoD source should render its representation");
    if (item->source.lod_id != 0x1901) FAIL("item should carry source LoD identity");
    if (item->lod_level != 1) FAIL("item should carry active LoD level");
    if (item->lod_level_count != 1) FAIL("item should carry LoD representation count");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s0);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend_item_lod_decision");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 20: logical LoD level with one physical child                   */
/* ------------------------------------------------------------------ */

static int
test_backend_item_lod_logical_level(void)
{
    printf("=== Test 20: backend_item_lod_logical_level ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s0 = bsg_shape_create(v);
    bsg_node_add_child(root, s0);
    bsg_node_set_name(s0, "single_child_lod");

    struct render_item_lod_state st = {7};
    struct bsg_lod_source_policy policy = BSG_LOD_SOURCE_POLICY_INIT;
    policy.kind = BSG_LOD_SOURCE_TEST;
    policy.identity = 0x2001;
    policy.name = "logical_lod";
    policy.policy_data = &st;
    policy.select = render_item_lod_select;
    policy.activate = render_item_lod_activate;
    policy.stale = render_item_lod_stale;
    bsg_lod_source_ref lod_ref = bsg_lod_source_insert_above(s0, v, &policy);
    if (bsg_lod_source_ref_is_null(lod_ref)) FAIL("logical LoD source should be created");

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "backend logical lod items");

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect selected single-child LoD item");
    if (BU_PTBL_LEN(&items) != 1) FAIL("items table should have one logical LoD item");

    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item->source.name || bu_strcmp(item->source.name, "single_child_lod") != 0) FAIL("single-child LoD should traverse child 0");
    if (item->source.lod_id != 0x2001) FAIL("item should carry source LoD identity");
    if (item->lod_level != 7) FAIL("item should carry logical LoD level");
    if (item->lod_level_count != 1) FAIL("item should carry physical level count");
    uint64_t cache_level_7 = item->cache_identity;

    bsg_render_item_free(item);
    bu_ptbl_reset(&items);

    st.level = 3;
    n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect item after logical level changes");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->lod_level != 3) FAIL("updated item should carry new logical LoD level");
    if (item->cache_identity == cache_level_7)
	FAIL("cache identity should change with logical LoD level");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s0);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);

    PASS("backend_item_lod_logical_level");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 21: render item carries proxy payload metadata                 */
/* ------------------------------------------------------------------ */

static int
test_proxy_payload_item_semantics(void)
{
    printf("=== Test 21: proxy_payload_item_semantics ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *csg_shape = bsg_shape_create(v);
    bsg_node *null_csg_shape = bsg_shape_create(v);
    bsg_node *brep_shape = bsg_shape_create(v);
    bsg_node *external_shape = bsg_shape_create(v);
    bsg_node *null_external_shape = bsg_shape_create(v);
    if (!root || !csg_shape || !null_csg_shape || !brep_shape ||
	    !external_shape || !null_external_shape)
	FAIL("proxy payload scene setup");
    bsg_node_add_child(root, csg_shape);
    bsg_node_add_child(root, null_csg_shape);
    bsg_node_add_child(root, brep_shape);
    bsg_node_add_child(root, external_shape);
    bsg_node_add_child(root, null_external_shape);

    int csg_source = 42;
    int brep_source = 84;
    int external_source = 126;
    struct bsg_payload *csg_payload = bsg_payload_csg_create(&csg_source);
    struct bsg_payload *null_csg_payload = bsg_payload_csg_create(NULL);
    struct bsg_payload *brep_payload = bsg_payload_brep_create(&brep_source);
    struct bsg_payload *external_payload = bsg_payload_create(BSG_PL_NONE);
    struct bsg_payload *null_external_payload = bsg_payload_create(BSG_PL_NONE);
    if (!csg_payload || !null_csg_payload || !brep_payload ||
	    !external_payload || !null_external_payload)
	FAIL("proxy payload create");
    external_payload->pl.opaque = &external_source;
    const unsigned char external_color[3] = {91, 42, 17};
    bsg_appearance_set_color_override(external_shape, external_color, 1);
    bsg_appearance_set_transparency(external_shape, 0.375);
    bsg_appearance_set_dmode(external_shape, 4);
    bsg_appearance_set_line_width(external_shape, 7);
    bsg_appearance_set_line_style(external_shape, 1);
    bsg_payload_mark_source_revision(csg_payload, 11,
	    BSG_PAYLOAD_STALE_SOURCE_CHANGED);
    bsg_payload_mark_source_revision(brep_payload, 22,
	    BSG_PAYLOAD_STALE_SOURCE_CHANGED);
    bsg_payload_mark_source_revision(external_payload, 33,
	    BSG_PAYLOAD_STALE_SOURCE_CHANGED);
    bsg_node_set_payload(csg_shape, csg_payload);
    bsg_node_set_payload(null_csg_shape, null_csg_payload);
    bsg_node_set_payload(brep_shape, brep_payload);
    bsg_node_set_payload(external_shape, external_payload);
    bsg_node_set_payload(null_external_shape, null_external_payload);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 6, "proxy payload items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 5 || BU_PTBL_LEN(&items) != 5)
	FAIL("should collect five proxy payload items");

    int saw_csg = 0;
    int saw_null_csg = 0;
    int saw_brep = 0;
    int saw_external = 0;
    int saw_null_external = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	if (!item->geometry.proxy.source_identity ||
		item->geometry.source_identity != item->geometry.proxy.source_identity ||
		item->geometry.proxy.revision != item->payload_revision)
	    FAIL("proxy metadata identity/revision wrong");
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_CSG_PROXY) {
	    if (item->geometry.proxy.kind != BSG_RENDER_PROXY_CSG)
		FAIL("CSG proxy metadata wrong");
	    if (item->geometry.proxy.source_identity == (uint64_t)(uintptr_t)&csg_source) {
		if (item->realization_kind != BSG_REALIZE_ADAPTIVE_WIREFRAME ||
			item->realization_status != BSG_REALIZE_STATUS_STALE ||
			item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		    FAIL("stale CSG proxy realization state wrong");
		saw_csg = 1;
	    } else {
		if (item->realization_status != BSG_REALIZE_STATUS_CURRENT ||
			item->stale_reason != BSG_PAYLOAD_STALE_NONE)
		    FAIL("null CSG proxy should remain current");
		saw_null_csg = 1;
	    }
	} else if (item->geometry.kind == BSG_RENDER_GEOMETRY_BREP_PROXY) {
	    saw_brep = 1;
	    if (item->geometry.proxy.kind != BSG_RENDER_PROXY_BREP ||
		    item->geometry.proxy.source_identity != (uint64_t)(uintptr_t)&brep_source)
		FAIL("BREP proxy metadata wrong");
	    if (item->realization_kind != BSG_REALIZE_BREP_DISPLAY ||
		    item->realization_status != BSG_REALIZE_STATUS_STALE ||
		    item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		FAIL("stale BREP proxy realization state wrong");
	} else if (item->geometry.kind == BSG_RENDER_GEOMETRY_EXTERNAL_PROXY) {
	    if (item->geometry.proxy.kind != BSG_RENDER_PROXY_EXTERNAL)
		FAIL("external proxy metadata wrong");
	    if (item->geometry.proxy.source_identity == (uint64_t)(uintptr_t)&external_source) {
		if (item->realization_kind != BSG_REALIZE_NONE ||
			item->realization_status != BSG_REALIZE_STATUS_STALE ||
			item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		    FAIL("stale external proxy realization state wrong");
		if (item->phase != BSG_RENDER_PHASE_TRANSPARENT ||
			item->appearance.color[0] != external_color[0] ||
			item->appearance.color[1] != external_color[1] ||
			item->appearance.color[2] != external_color[2] ||
			!NEAR_EQUAL(item->appearance.transparency, 0.375, SMALL_FASTF) ||
			item->appearance.draw_mode != 4 ||
			item->appearance.line_width != 7 ||
			item->appearance.line_style != 1)
		    FAIL("styled external proxy appearance/phase wrong");
		saw_external = 1;
	    } else {
		if (item->geometry.proxy.source_identity !=
			(uint64_t)(uintptr_t)null_external_payload ||
			item->realization_status != BSG_REALIZE_STATUS_CURRENT ||
			item->stale_reason != BSG_PAYLOAD_STALE_NONE)
		    FAIL("null external proxy fallback state wrong");
		saw_null_external = 1;
	    }
	} else {
	    FAIL("unexpected proxy payload geometry kind");
	}
    }
    if (!saw_csg || !saw_null_csg || !saw_brep || !saw_external ||
	    !saw_null_external)
	FAIL("missing proxy item");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));
    bu_ptbl_reset(&items);

    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS |
	    BSG_RENDER_FLAG_VISIBLE_ONLY |
	    BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    n = bsg_render_request_execute(req);
    if (n != 5 || BU_PTBL_LEN(&items) != 5)
	FAIL("should collect five prepared proxy payload items");
    saw_csg = 0;
    saw_brep = 0;
    saw_external = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_CSG_PROXY &&
		item->geometry.proxy.source_identity == (uint64_t)(uintptr_t)&csg_source) {
	    saw_csg = 1;
	    if (item->realization_status != BSG_REALIZE_STATUS_STALE ||
		    item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		FAIL("prepared CSG proxy without updater must remain stale");
	} else if (item->geometry.kind == BSG_RENDER_GEOMETRY_BREP_PROXY) {
	    saw_brep = 1;
	    if (item->realization_status != BSG_REALIZE_STATUS_STALE ||
		    item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		FAIL("prepared BREP proxy without updater must remain stale");
	} else if (item->geometry.kind == BSG_RENDER_GEOMETRY_EXTERNAL_PROXY &&
		item->geometry.proxy.source_identity == (uint64_t)(uintptr_t)&external_source) {
	    saw_external = 1;
	    if (item->realization_status != BSG_REALIZE_STATUS_STALE ||
		    item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
		FAIL("prepared external proxy without updater must remain stale");
	}
    }
    if (!saw_csg || !saw_brep || !saw_external)
	FAIL("prepared proxy stale coverage missing");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(csg_shape);
    bsg_shape_destroy(null_csg_shape);
    bsg_shape_destroy(brep_shape);
    bsg_shape_destroy(external_shape);
    bsg_shape_destroy(null_external_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("proxy_payload_item_semantics");
    return 0;
}

static int
test_proxy_geometry_node_payload_revision_cache(void)
{
    printf("=== Test 21b: proxy_geometry_node_payload_revision_cache ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_csg_proxy_ref csg =
	bsg_csg_proxy_ref_create(v, "typed_proxy_cache");
    bsg_node *node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_csg_proxy_ref_as_node(csg)));
    if (!root || !node)
	FAIL("typed proxy cache scene setup");

    int csg_source = 17;
    struct bsg_payload *payload = bsg_payload_csg_create(&csg_source);
    if (!payload)
	FAIL("typed proxy cache payload create");
    bsg_node_set_payload(node, payload);
    bsg_node_add_child(root, node);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "typed proxy cache items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_COLLECT_ITEMS | BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1 || BU_PTBL_LEN(&items) != 1)
	FAIL("typed proxy cache should collect one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item ||
	    item->geometry.kind != BSG_RENDER_GEOMETRY_CSG_PROXY ||
	    item->geometry.proxy.kind != BSG_RENDER_PROXY_CSG ||
	    item->geometry.proxy.source_identity !=
		(uint64_t)(uintptr_t)&csg_source)
	FAIL("typed proxy cache initial metadata");
    uint64_t first_cache_identity = item->cache_identity;
    uint64_t first_source_id = item->source.source_id;
    uint64_t first_geometry_revision = item->geometry.revision;
    uint64_t first_proxy_source = item->geometry.proxy.source_identity;
    if (!first_cache_identity || !first_source_id || !first_proxy_source ||
	    item->geometry.proxy.revision != item->payload_revision)
	FAIL("typed proxy cache initial identity fields");
    bsg_render_item_free(item);
    bu_ptbl_reset(&items);

    bsg_payload_bump_revision(payload);
    n = bsg_render_request_execute(req);
    if (n != 1 || BU_PTBL_LEN(&items) != 1)
	FAIL("typed proxy cache should collect one item after payload bump");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item || item->payload_revision == 0)
	FAIL("typed proxy cache payload revision should advance");
    if (item->source.source_id != first_source_id ||
	    item->geometry.proxy.source_identity != first_proxy_source)
	FAIL("typed proxy cache should preserve stable source identity");
    if (item->geometry.revision != first_geometry_revision)
	FAIL("typed proxy geometry node revision should stay unchanged");
    if (item->geometry.proxy.revision != item->payload_revision)
	FAIL("typed proxy metadata should report current payload revision");
    if (item->cache_identity == first_cache_identity)
	FAIL("typed proxy cache identity should change with payload revision");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("proxy_geometry_node_payload_revision_cache");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 21: render item carries realization state                      */
/* ------------------------------------------------------------------ */

static int
test_render_item_realization_status(void)
{
    printf("=== Test 21: render_item_realization_status ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    bsg_node_add_child(root, shape);
    struct render_item_failing_preview_state preview_state;
    memset(&preview_state, 0, sizeof(preview_state));

    struct bsg_payload *pl = bsg_payload_edit_preview_create(&preview_state, NULL);
    if (!pl) FAIL("edit-preview payload create");
    bsg_payload_edit_preview_set_ops(pl, &preview_state, 0,
	    NULL, render_item_failing_live_update,
	    render_item_preview_bounds, NULL, NULL, NULL);
    bsg_payload_edit_preview_mark_source_revision(pl, 3, BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED);
    bsg_node_set_payload(shape, pl);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "realization status items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS |
	BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->realization_kind != BSG_REALIZE_EDIT_PREVIEW)
	FAIL("item should carry edit-preview realization kind");
    if (item->realization_status != BSG_REALIZE_STATUS_STALE)
	FAIL("item should carry stale realization status before prepare");
    if (item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
	FAIL("item should carry source stale reason before prepare");
    if (!item->has_bounds ||
	    !NEAR_EQUAL(item->bounds_center[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_center[Y], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_center[Z], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_radius, 7.483314773547883, BN_TOL_DIST))
	FAIL("unprepared edit-preview item should carry payload bounds");
    if (preview_state.update_calls != 0 || preview_state.bounds_calls != 1)
	FAIL("unprepared edit-preview bounds should not update payload");
    bsg_render_item_free(item);
    bu_ptbl_reset(&items);

    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS |
	BSG_RENDER_FLAG_VISIBLE_ONLY |
	BSG_RENDER_FLAG_PAYLOAD_PREPARE);

    n = bsg_render_request_execute(req);
    if (n != 1) FAIL("should collect one prepared item");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (item->realization_kind != BSG_REALIZE_EDIT_PREVIEW)
	FAIL("prepared item should carry edit-preview realization kind");
    if (item->realization_status != BSG_REALIZE_STATUS_FAILED)
	FAIL("item should carry failed realization status");
    if (item->stale_reason != BSG_PAYLOAD_STALE_UPDATE_FAILED)
	FAIL("item should carry failed stale reason");
    if (!item->has_bounds ||
	    !NEAR_EQUAL(item->bounds_radius, 7.483314773547883, BN_TOL_DIST))
	FAIL("failed prepared edit-preview item should retain payload bounds");
    if (preview_state.update_calls != 1 || preview_state.bounds_calls != 2)
	FAIL("prepared edit-preview should update once and query bounds twice");
    struct bsg_edit_preview_source *preview_source =
	bsg_payload_edit_preview_get_data(pl);
    if (item->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
	    item->geometry.proxy.kind != BSG_RENDER_PROXY_EDIT_PREVIEW ||
	    item->geometry.proxy.source_identity !=
		(uint64_t)(uintptr_t)preview_source ||
	    item->geometry.proxy.revision != item->payload_revision)
	FAIL("edit-preview item should carry typed proxy metadata");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("render_item_realization_status");
    return 0;
}

static int
test_render_item_prepared_edit_preview_bounds_snapshot(void)
{
    printf("=== Test 21c: prepared_edit_preview_bounds_snapshot ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape)
	FAIL("prepared edit-preview bounds scene setup");
    bsg_node_add_child(root, shape);

    struct render_item_preview_bounds_snapshot_state state;
    memset(&state, 0, sizeof(state));
    state.revision = 12;

    struct bsg_payload *pl = bsg_payload_edit_preview_create(&state, NULL);
    if (!pl)
	FAIL("prepared edit-preview bounds payload create");
    if (!bsg_payload_edit_preview_set_ops(pl, &state, 0,
		render_item_preview_bounds_snapshot_revision,
		render_item_preview_bounds_snapshot_update,
		render_item_preview_bounds_snapshot_bounds,
		NULL, NULL, NULL))
	FAIL("prepared edit-preview bounds set_ops");
    if (!bsg_payload_edit_preview_mark_source_revision(pl, 21,
		BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED))
	FAIL("prepared edit-preview bounds stale mark");
    bsg_node_set_payload(shape, pl);

    struct bu_ptbl items;
    bu_ptbl_init(&items, 4, "prepared edit-preview bounds items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS |
	BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 1 || BU_PTBL_LEN(&items) != 1)
	FAIL("prepared edit-preview bounds should collect one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (state.update_calls != 1)
	FAIL("prepared edit-preview bounds should update once");
    if (state.bounds_calls != 1)
	FAIL("prepared edit-preview bounds should use realization snapshot");
    if (!item || item->realization_status != BSG_REALIZE_STATUS_CURRENT ||
	    item->stale_reason != BSG_PAYLOAD_STALE_NONE)
	FAIL("prepared edit-preview bounds should be current");
    if (!item->has_bounds ||
	    !NEAR_EQUAL(item->bounds_center[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_center[Y], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_center[Z], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(item->bounds_radius, 3.7416573867739413, BN_TOL_DIST))
	FAIL("prepared edit-preview bounds should come from snapshot");

    bsg_render_item_free(item);
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("prepared_edit_preview_bounds_snapshot");
    return 0;
}

static int
test_render_item_edit_preview_fanout(void)
{
    printf("=== Test 22: render_item_edit_preview_fanout ===\n");

    enum { FANOUT = 6 };
    struct render_item_edit_preview_fanout_state state;
    memset(&state, 0, sizeof(state));
    state.revision = 20;
    state.update_returns = 1;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shapes[FANOUT];
    memset(shapes, 0, sizeof(shapes));

    for (int i = 0; i < FANOUT; i++) {
	char name[64];
	snprintf(name, sizeof(name), "edit_preview_fanout_%d", i);
	shapes[i] = bsg_shape_create(v);
	if (!shapes[i])
	    FAIL("edit-preview fanout shape create");
	bsg_node_set_name(shapes[i], name);
	bsg_node_add_child(root, shapes[i]);
	struct bsg_payload *pl =
	    bsg_payload_edit_preview_create(&state, NULL);
	if (!pl)
	    FAIL("edit-preview fanout payload create");
	if (!bsg_payload_edit_preview_set_ops(pl, &state, 0,
		    render_item_edit_preview_revision,
		    render_item_edit_preview_update,
		    NULL, NULL, NULL, NULL))
	    FAIL("edit-preview fanout set_ops");
	if (!bsg_payload_edit_preview_mark_source_revision(pl, 100 + (uint64_t)i,
		    BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED))
	    FAIL("edit-preview fanout source stale mark");
	bsg_node_set_payload(shapes[i], pl);
    }

    struct bu_ptbl items;
    bu_ptbl_init(&items, FANOUT, "edit-preview fanout render items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS |
	BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != FANOUT)
	FAIL("edit-preview fanout should collect stale items");
    for (int i = 0; i < FANOUT; i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, (size_t)i);
	if (!item ||
		item->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
		item->realization_kind != BSG_REALIZE_EDIT_PREVIEW ||
		item->realization_status != BSG_REALIZE_STATUS_STALE ||
		item->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
	    FAIL("edit-preview fanout stale render item state");
	bsg_render_item_free(item);
    }
    bu_ptbl_reset(&items);
    if (state.update_calls != 0)
	FAIL("unprepared edit-preview fanout should not call update");

    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_COLLECT_ITEMS |
	BSG_RENDER_FLAG_VISIBLE_ONLY |
	BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    n = bsg_render_request_execute(req);
    if (n != FANOUT)
	FAIL("edit-preview fanout should collect prepared items");
    if (state.update_calls != FANOUT)
	FAIL("prepared edit-preview fanout should update every payload");
    for (int i = 0; i < FANOUT; i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, (size_t)i);
	if (!item ||
		item->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
		item->realization_kind != BSG_REALIZE_EDIT_PREVIEW ||
		item->realization_status != BSG_REALIZE_STATUS_CURRENT ||
		item->stale_reason != BSG_PAYLOAD_STALE_NONE)
	    FAIL("edit-preview fanout prepared render item state");
	bsg_render_item_free(item);
    }

    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);
    for (int i = 0; i < FANOUT; i++)
	bsg_shape_destroy(shapes[i]);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("render_item_edit_preview_fanout");
    return 0;
}

struct render_item_segment_check {
    size_t count;
    point_t first_a;
    point_t first_b;
    point_t last_b;
};

static int
render_item_segment_check_cb(const point_t a, const point_t b, void *data)
{
    struct render_item_segment_check *check =
	(struct render_item_segment_check *)data;
    if (!check)
	return 0;
    if (check->count == 0) {
	VMOVE(check->first_a, a);
	VMOVE(check->first_b, b);
    }
    VMOVE(check->last_b, b);
    check->count++;
    return 1;
}

static int
test_render_item_segment_iteration(void)
{
    printf("=== Test 23: render_item_segment_iteration ===\n");

    struct bsg_render_item *item = bsg_render_item_create();
    if (!item)
	FAIL("segment item allocation failed");
    item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
    item->geometry.arrays.point_count = 3;
    item->geometry.arrays.command_count = 3;
    item->geometry.arrays.points = (point_t *)bu_calloc(3, sizeof(point_t),
	    "render item segment test points");
    item->geometry.arrays.commands = (int *)bu_calloc(3, sizeof(int),
	    "render item segment test commands");
    VSET(item->geometry.arrays.points[1], 1.0, 0.0, 0.0);
    VSET(item->geometry.arrays.points[2], 2.0, 0.0, 0.0);
    item->geometry.arrays.commands[0] = BSG_GEOMETRY_LINE_MOVE;
    item->geometry.arrays.commands[1] = BSG_GEOMETRY_LINE_DRAW;
    item->geometry.arrays.commands[2] = BSG_GEOMETRY_LINE_DRAW;

    struct render_item_segment_check check;
    memset(&check, 0, sizeof(check));
    if (!bsg_render_item_has_line_segments(item) ||
	    bsg_render_item_foreach_line_segment(item,
		render_item_segment_check_cb, &check) != 2 ||
	    check.count != 2 ||
	    !VNEAR_EQUAL(check.first_a, item->geometry.arrays.points[0], SMALL_FASTF) ||
	    !VNEAR_EQUAL(check.first_b, item->geometry.arrays.points[1], SMALL_FASTF) ||
	    !VNEAR_EQUAL(check.last_b, item->geometry.arrays.points[2], SMALL_FASTF))
	FAIL("line-set segment iteration failed");
    memset(&check, 0, sizeof(check));
    if (!bsg_render_item_has_wire_segments(item) ||
	    bsg_render_item_foreach_wire_segment(item,
		render_item_segment_check_cb, &check) != 2 ||
	    check.count != 2)
	FAIL("line-set wire segment iteration failed");
    bsg_render_item_free(item);

    item = bsg_render_item_create();
    if (!item)
	FAIL("indexed segment item allocation failed");
    item->geometry.kind = BSG_RENDER_GEOMETRY_INDEXED_LINE_SET;
    item->geometry.arrays.point_count = 3;
    item->geometry.arrays.index_count = 5;
    item->geometry.arrays.points = (point_t *)bu_calloc(3, sizeof(point_t),
	    "render item indexed segment test points");
    item->geometry.arrays.indices = (int *)bu_calloc(5, sizeof(int),
	    "render item indexed segment test indices");
    VSET(item->geometry.arrays.points[1], 1.0, 0.0, 0.0);
    VSET(item->geometry.arrays.points[2], 1.0, 2.0, 0.0);
    item->geometry.arrays.indices[0] = 0;
    item->geometry.arrays.indices[1] = 1;
    item->geometry.arrays.indices[2] = -1;
    item->geometry.arrays.indices[3] = 1;
    item->geometry.arrays.indices[4] = 2;
    memset(&check, 0, sizeof(check));
    if (!bsg_render_item_has_line_segments(item) ||
	    bsg_render_item_foreach_line_segment(item,
		render_item_segment_check_cb, &check) != 2 ||
	    check.count != 2 ||
	    !VNEAR_EQUAL(check.first_b, item->geometry.arrays.points[1], SMALL_FASTF) ||
	    !VNEAR_EQUAL(check.last_b, item->geometry.arrays.points[2], SMALL_FASTF))
	FAIL("indexed line-set segment iteration failed");
    memset(&check, 0, sizeof(check));
    if (!bsg_render_item_has_wire_segments(item) ||
	    bsg_render_item_foreach_wire_segment(item,
		render_item_segment_check_cb, &check) != 2 ||
	    check.count != 2)
	FAIL("indexed line-set wire segment iteration failed");
    bsg_render_item_free(item);

    item = bsg_render_item_create();
    if (!item)
	FAIL("surface segment item allocation failed");
    item->geometry.kind = BSG_RENDER_GEOMETRY_INDEXED_FACE_SET;
    item->geometry.surface.point_count = 3;
    item->geometry.surface.index_count = 4;
    item->geometry.surface.points = (point_t *)bu_calloc(3, sizeof(point_t),
	    "render item surface segment test points");
    item->geometry.surface.indices = (int *)bu_calloc(4, sizeof(int),
	    "render item surface segment test indices");
    VSET(item->geometry.surface.points[1], 1.0, 0.0, 0.0);
    VSET(item->geometry.surface.points[2], 0.0, 1.0, 0.0);
    item->geometry.surface.indices[0] = 0;
    item->geometry.surface.indices[1] = 1;
    item->geometry.surface.indices[2] = 2;
    item->geometry.surface.indices[3] = -1;
    memset(&check, 0, sizeof(check));
    if (bsg_render_item_has_line_segments(item) ||
	    bsg_render_item_foreach_line_segment(item,
		render_item_segment_check_cb, &check) != 0)
	FAIL("surface item should not report line segments");
    memset(&check, 0, sizeof(check));
    if (!bsg_render_item_has_wire_segments(item) ||
	    bsg_render_item_foreach_wire_segment(item,
		render_item_segment_check_cb, &check) != 3 ||
	    check.count != 3 ||
	    !VNEAR_EQUAL(check.first_b, item->geometry.surface.points[1], SMALL_FASTF) ||
	    !VNEAR_EQUAL(check.last_b, item->geometry.surface.points[0], SMALL_FASTF))
	FAIL("surface wire segment iteration failed");
    bsg_render_item_free(item);

    item = bsg_render_item_create();
    if (!item)
	FAIL("surface segment item allocation failed");
    item->geometry.kind = BSG_RENDER_GEOMETRY_INDEXED_FACE_SET;
    if (bsg_render_item_has_line_segments(item) ||
	    bsg_render_item_foreach_line_segment(item,
		render_item_segment_check_cb, &check) != 0 ||
	    bsg_render_item_has_wire_segments(item) ||
	    bsg_render_item_foreach_wire_segment(item,
		render_item_segment_check_cb, &check) != 0)
	FAIL("empty surface item should not report render-item segments");
    bsg_render_item_free(item);

    PASS("render_item_segment_iteration");
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

    failures += test_item_create_free();
    failures += test_phase_enum_values();
    failures += test_empty_tree();
    failures += test_visible_only();
    failures += test_hidden_parent_visible_child();
    failures += test_view_scope_render_filter();
    failures += test_collect_items();
    failures += test_transparent_phase();
    failures += test_adapter_draw();
    failures += test_adapter_prepare_draw();
    failures += test_item_model_mat_identity();
    failures += test_null_request();
    failures += test_sorted_alpha();
    failures += test_hud_phase_sort();
    failures += test_adapter_capabilities();
    failures += test_adapter_invalidate_free();
    failures += test_render_inherited_appearance();
    failures += test_render_local_appearance_priority();
    failures += test_backend_item_semantics();
    failures += test_feature_item_semantics();
    failures += test_image_framebuffer_item_semantics();
    failures += test_annotation_item_semantics();
    failures += test_indexed_face_surface_item_semantics();
    failures += test_indexed_face_surface_normal_kinds();
    failures += test_selection_render_semantics();
    failures += test_backend_item_lod_decision();
    failures += test_backend_item_lod_logical_level();
    failures += test_proxy_payload_item_semantics();
    failures += test_proxy_geometry_node_payload_revision_cache();
    failures += test_render_item_realization_status();
    failures += test_render_item_prepared_edit_preview_bounds_snapshot();
    failures += test_render_item_edit_preview_fanout();
    failures += test_render_item_segment_iteration();

    if (failures == 0)
	printf("RESULT: all Phase D5 render-item tests PASSED\n");
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
