/*  T E S T _ B A C K E N D _ A D A P T E R . C
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
/** @file libbsg/tests/test_backend_adapter.c
 *
 * Phase D5 unit tests: bsg_backend_adapter — invalidation reason mask
 * coverage (BSG_INVALIDATE_* flags) and adapter capability flags.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "vmath.h"
#include "bsg/defines.h"
#include "bsg/action.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/backend_scene.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/interaction.h"
#include "../node_private.h"
#include "bsg/overlay.h"
#include "bsg/scene_builder.h"
#include "bsg/selection.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_storage_private.h"
#include "../payload_typed_private.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/backend_adapter.h"
#include "bsg/scene_object.h"
#include "../geometry_private.h"
#include "../object_private.h"
#include "../payload_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

/* Accumulate the last reason_mask seen in the invalidate callback. */
static unsigned int g_last_reason = 0;
static int g_invalidate_count = 0;

static void
_invalidate_cb(void *UNUSED(dmp), const struct bsg_render_item *UNUSED(item),
	       unsigned int reason_mask)
{
    g_last_reason = reason_mask;
    g_invalidate_count++;
}

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "bsg_view");
}

struct backend_edit_preview_fanout_state {
    uint64_t revision;
    int update_calls;
    int update_returns;
    int bounds_calls;
    point_t bounds_min;
    point_t bounds_max;
};

static uint64_t
backend_edit_preview_revision(void *ctx)
{
    struct backend_edit_preview_fanout_state *state =
	(struct backend_edit_preview_fanout_state *)ctx;
    return state ? state->revision : 0;
}

static int
backend_edit_preview_update(void *ctx, struct bsg_view *UNUSED(v))
{
    struct backend_edit_preview_fanout_state *state =
	(struct backend_edit_preview_fanout_state *)ctx;
    if (!state)
	return -1;
    state->update_calls++;
    if (state->update_returns > 0)
	state->revision++;
    return state->update_returns;
}

static int
backend_edit_preview_bounds(void *ctx, point_t *bmin, point_t *bmax)
{
    struct backend_edit_preview_fanout_state *state =
	(struct backend_edit_preview_fanout_state *)ctx;
    if (!state || !bmin || !bmax)
	return 0;
    state->bounds_calls++;
    VMOVE(*bmin, state->bounds_min);
    VMOVE(*bmax, state->bounds_max);
    return 1;
}


static bsg_node *
_create_backend_line_node(struct bsg_view *v, bsg_node *root, const char *name,
	const point_t a, const point_t b)
{
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, name);
    if (bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)))
	return NULL;

    bsg_node *node = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!node)
	return NULL;

    point_t pts[2];
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VMOVE(pts[0], a);
    VMOVE(pts[1], b);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2))
	return NULL;

    bsg_node_add_child(root, node);
    bsg_node_set_source_flags(node, BSG_SOURCE_DB);
    bsg_node_set_visible_state(node, 1);
    return node;
}


/* -----------------------------------------------------------------------
 * Test 1: BSG_INVALIDATE_* constants are distinct non-overlapping bits
 * ----------------------------------------------------------------------- */
static int
test_reason_bits_distinct(void)
{
    printf("=== Test 1: reason bits are distinct ===\n");

    unsigned int bits[] = {
	BSG_INVALIDATE_PAYLOAD,
	BSG_INVALIDATE_APPEARANCE,
	BSG_INVALIDATE_TRANSFORM,
	BSG_INVALIDATE_RENDER_MODE
    };
    int nbits = (int)(sizeof(bits) / sizeof(bits[0]));

    for (int i = 0; i < nbits; i++) {
	if (bits[i] == 0) FAIL("reason bit must not be 0");
	for (int j = i + 1; j < nbits; j++) {
	    if (bits[i] & bits[j])
		FAIL("reason bits must not overlap");
	}
    }

    /* BSG_INVALIDATE_ALL must be the OR of all individual bits */
    unsigned int all = (BSG_INVALIDATE_PAYLOAD | BSG_INVALIDATE_APPEARANCE |
			BSG_INVALIDATE_TRANSFORM | BSG_INVALIDATE_RENDER_MODE);
    if (BSG_INVALIDATE_ALL != all)
	FAIL("BSG_INVALIDATE_ALL must equal OR of all individual bits");

    PASS("reason bits are distinct and BSG_INVALIDATE_ALL is correct");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 2: invalidate callback receives the reason mask unchanged
 * ----------------------------------------------------------------------- */
static int
test_reason_passed_through(void)
{
    printf("=== Test 2: reason mask passed through callback ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *s = bsg_shape_create(v);
    bsg_node_set_visible_state(s, 1);

    struct bsg_render_item *item = bsg_render_item_create();
    item->view = v;

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    adapter.invalidate = _invalidate_cb;

    /* Test each reason individually */
    unsigned int reasons[] = {
	BSG_INVALIDATE_PAYLOAD,
	BSG_INVALIDATE_APPEARANCE,
	BSG_INVALIDATE_TRANSFORM,
	BSG_INVALIDATE_RENDER_MODE,
	BSG_INVALIDATE_ALL,
	BSG_INVALIDATE_PAYLOAD | BSG_INVALIDATE_APPEARANCE
    };
    int nreasons = (int)(sizeof(reasons) / sizeof(reasons[0]));

    for (int i = 0; i < nreasons; i++) {
	g_last_reason = 0;
	adapter.invalidate(NULL, item, reasons[i]);
	if (g_last_reason != reasons[i])
	    FAIL("callback must receive exact reason_mask");
    }

    bsg_render_item_free(item);
    bsg_node_destroy(s);
    _free_view(v);
    PASS("reason mask passed through callback");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 3: adapter capability flags are distinct
 * ----------------------------------------------------------------------- */
static int
test_capability_bits_distinct(void)
{
    printf("=== Test 3: capability bits distinct ===\n");

    unsigned int caps[] = {
	BSG_ADAPTER_CAP_TRANSPARENCY,
	BSG_ADAPTER_CAP_WIREFRAME,
	BSG_ADAPTER_CAP_SHADED,
	BSG_ADAPTER_CAP_HUD,
	BSG_ADAPTER_CAP_SORTED_ALPHA,
	BSG_ADAPTER_CAP_BREP
    };
    int ncaps = (int)(sizeof(caps) / sizeof(caps[0]));

    for (int i = 0; i < ncaps; i++) {
	if (caps[i] == 0) FAIL("capability bit must not be 0");
	for (int j = i + 1; j < ncaps; j++) {
	    if (caps[i] & caps[j])
		FAIL("capability bits must not overlap");
	}
    }

    PASS("capability bits are distinct");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 4: NULL adapter invalidate callback is safe to skip
 * ----------------------------------------------------------------------- */
static int
test_null_invalidate_safe(void)
{
    printf("=== Test 4: NULL invalidate is safe ===\n");

    struct bsg_backend_adapter adapter;
    memset(&adapter, 0, sizeof(adapter));
    /* adapter.invalidate == NULL */

    /* Mimick what the executor does: check for NULL before calling */
    if (adapter.invalidate)
	adapter.invalidate(NULL, NULL, BSG_INVALIDATE_ALL);
    /* Reaching here means no crash */

    PASS("NULL invalidate is safe to skip");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 5: render request carries per-frame settings (render_settings smoke)
 * ----------------------------------------------------------------------- */
static int
test_request_settings_populated(void)
{
    printf("=== Test 5: bsg_render_request settings populated ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    if (!req) FAIL("bsg_render_request_create returned NULL");
    const struct bsg_render_settings *settings = bsg_render_request_settings_const(req);
    if (!settings) FAIL("request settings must not be NULL after create");

    /* Defaults: transparency_policy = sorted, LoD source policy = auto */
    if (settings->transparency_policy != BSG_TRANSPARENCY_SORTED)
	FAIL("default transparency_policy should be BSG_TRANSPARENCY_SORTED");
    if (settings->lod_source_policy.policy != BSG_LOD_AUTO)
	FAIL("default LoD source policy should be BSG_LOD_AUTO");
    if (!settings->hud_enabled)
	FAIL("default hud_enabled should be 1");

    bsg_render_request_destroy(req);
    bsg_node_destroy(root);
    _free_view(v);
    PASS("render request settings populated at creation");
    return 0;
}

static void
_free_items(struct bu_ptbl *items)
{
    if (!items)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(items, i));
    bu_ptbl_free(items);
}

static int
_render_to_backend_scene(struct bsg_view *v,
			 struct bsg_backend_scene *scene)
{
    return bsg_backend_scene_render_request(v, scene,
	    BSG_RENDER_FLAG_VISIBLE_ONLY);
}

static int
_collect_render_items(struct bsg_view *v, struct bu_ptbl *items)
{
    bu_ptbl_init(items, 4, "backend scene compare items");
    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    if (!req)
	return -1;
    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!batch) {
	bsg_render_request_destroy(req);
	return -1;
    }
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    int ret = bsg_render_request_collect(req, batch);
    if (ret >= 0) {
	(void)bsg_render_batch_move_items(batch, items);
    }
    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    return ret;
}

/* -----------------------------------------------------------------------
 * Test 6: retained backend scene creates, reuses, updates, and compares
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_retained_cache(void)
{
    printf("=== Test 6: backend scene retained cache ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    point_t pa = VINIT_ZERO;
    point_t pb = VINIT_ZERO;
    VSET(pb, 1.0, 0.0, 0.0);
    bsg_node *s = _create_backend_line_node(v, root, "backend_scene_wire", pa, pb);
    if (!s) FAIL("backend line node create failed");
    bsg_appearance_set_line_style(s, 1);
    bsg_appearance_set_legacy_eval_flag(s, 1);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene) FAIL("backend scene create failed");

    if (_render_to_backend_scene(v, scene) != 1)
	FAIL("first render should draw one item");
    struct bsg_backend_scene_stats stats;
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.node_count != 1 || stats.created != 1 || stats.drawn != 1)
	FAIL("first render should create and draw one retained node");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("collect render items failed");
    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("backend scene should match collected render stream");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node) FAIL("retained node lookup failed");
    if (node->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("retained node should record line-set geometry kind");
    if (node->scene_kind != BSG_BACKEND_SCENE_NODE_GEOMETRY)
	FAIL("retained node should be geometry scene kind");
    if (!NEAR_EQUAL(node->transform.bounds_radius, item->bounds_radius,
		SMALL_FASTF))
	FAIL("retained node should record transform bounds");
    if (node->material.draw_mode != item->appearance.draw_mode)
	FAIL("retained node should record material draw mode");
    if (node->material.line_style != item->appearance.line_style ||
	    node->material.evaluated_region != item->appearance.evaluated_region)
	FAIL("retained node should record material line/evaluated facts");
    if (node->selection.visible != item->visible)
	FAIL("retained node should record visibility state");
    if (node->lod.level != item->lod_level)
	FAIL("retained node should record LoD state");
    if (node->draw_count != 1)
	FAIL("retained node draw count should be one after first render");
    uint64_t source_identity = node->source_identity;
    _free_items(&items);
    bu_vls_free(&report);

    if (_render_to_backend_scene(v, scene) != 1)
	FAIL("second render should draw one item");
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.reused != 1 || stats.created != 0 || stats.updated != 0)
	FAIL("second render should reuse retained node");

    bsg_appearance_set_transparency(s, 0.5);
    if (_render_to_backend_scene(v, scene) != 1)
	FAIL("transparency render should draw one item");
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.updated != 1)
	FAIL("appearance change should update retained node");
    if (bsg_backend_scene_count(scene) != 1)
	FAIL("update should not create duplicate retained nodes");

    bsg_backend_scene_release_source(scene, source_identity);
    if (bsg_backend_scene_count(scene) != 0)
	FAIL("release_source should remove retained node");

    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene retained cache");
    return 0;
}

/* -----------------------------------------------------------------------
 * Test 7: retained backend scene owns field-backed geometry snapshots
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_geometry_snapshots(void)
{
    printf("=== Test 7: backend scene geometry snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "backend_scene_lines");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)))
	FAIL("field-backed scene setup should create root and line-set");

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 1.0, 0.0);
    int cmds[3] = {
	BSG_GEOMETRY_LINE_MOVE,
	BSG_GEOMETRY_LINE_DRAW,
	BSG_GEOMETRY_LINE_DRAW
    };
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 3))
	FAIL("line-set fields should accept points");

    struct bsg_node *line_node =
	bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!bsg_geometry_node_clear_private_realization(line_node))
	FAIL("line-set private realization should clear");

    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_line_set_ref_as_node(lines));

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("field-backed line-set should render into retained scene");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("field-backed line-set collect should find one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node)
	FAIL("retained line-set node lookup failed");
    if (node->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("retained geometry kind should be line-set");
    if (node->geometry.arrays.point_count != 3 ||
	    !node->geometry.arrays.points ||
	    node->geometry.arrays.command_count != 3 ||
	    !node->geometry.arrays.commands)
	FAIL("retained line-set should own field snapshot arrays");
    if (node->geometry.arrays.points == item->geometry.arrays.points ||
	    node->geometry.arrays.commands == item->geometry.arrays.commands)
	FAIL("retained line-set arrays should be deep copies");
    if (!VNEAR_EQUAL(node->geometry.arrays.points[2], pts[2], SMALL_FASTF) ||
	    node->geometry.arrays.commands[2] != BSG_GEOMETRY_LINE_DRAW)
	FAIL("retained line-set snapshot should preserve field data");

    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained field snapshots should compare against render items");
    bu_vls_free(&report);
    _free_items(&items);

    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2))
	FAIL("line-set field update should accept shorter snapshot");
    if (!bsg_geometry_node_clear_private_realization(line_node))
	FAIL("updated line-set private realization should clear");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("updated line-set should render into retained scene");
    struct bsg_backend_scene_stats stats;
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.node_count != 1 || (stats.updated + stats.created) != 1)
	FAIL("retained scene should refresh when field snapshot changes");
    if (_collect_render_items(v, &items) != 1)
	FAIL("updated field-backed line-set collect should find one item");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    node = bsg_backend_scene_find(scene, item->cache_identity);
    if (!node || node->geometry.arrays.point_count != 2 ||
	    !node->geometry.arrays.points ||
	    !VNEAR_EQUAL(node->geometry.arrays.points[1], pts[1], SMALL_FASTF))
	FAIL("retained scene should expose updated field snapshot");
    _free_items(&items);

    bsg_backend_scene_destroy(scene);
    _free_view(v);
    PASS("backend scene geometry snapshots");
    return 0;
}

static int
test_backend_scene_indexed_geometry_snapshots(void)
{
    printf("=== Test 8: backend scene indexed geometry snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_line_set_ref lines =
	bsg_indexed_line_set_ref_create(v, "backend_scene_indexed_lines");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(lines)))
	FAIL("field-backed indexed scene setup should create root and line-set");

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 2.0, 0.0);
    int indices[5] = {0, 1, -1, 1, 2};
    if (!bsg_indexed_line_set_ref_set_geometry(lines,
		(const point_t *)pts, 3, indices, 5))
	FAIL("indexed line-set fields should accept points and indices");
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_line_set_ref_as_node(lines));

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("field-backed indexed line-set should render into retained scene");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("field-backed indexed line-set collect should find one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node)
	FAIL("retained indexed line-set node lookup failed");
    if (node->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_LINE_SET)
	FAIL("retained geometry kind should be indexed line-set");
    if (node->geometry.arrays.point_count != 3 ||
	    !node->geometry.arrays.points ||
	    node->geometry.arrays.index_count != 5 ||
	    !node->geometry.arrays.indices)
	FAIL("retained indexed line-set should own field snapshot arrays");
    if (node->geometry.arrays.points == item->geometry.arrays.points ||
	    node->geometry.arrays.indices == item->geometry.arrays.indices)
	FAIL("retained indexed line-set arrays should be deep copies");
    if (!VNEAR_EQUAL(node->geometry.arrays.points[2], pts[2], SMALL_FASTF) ||
	    node->geometry.arrays.indices[2] != -1 ||
	    node->geometry.arrays.indices[4] != 2)
	FAIL("retained indexed line-set snapshot should preserve field data");

    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained indexed snapshots should compare against render items");
    bu_vls_free(&report);
    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    _free_view(v);
    PASS("backend scene indexed geometry snapshots");
    return 0;
}

static int
test_backend_scene_indexed_face_surface_snapshots(void)
{
    printf("=== Test 8a: backend scene indexed face surface snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_face_set_ref faces =
	bsg_indexed_face_set_ref_create(v, "backend_scene_indexed_faces");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(faces)))
	FAIL("field-backed indexed face scene setup should create root and face-set");

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
	FAIL("indexed face-set fields should accept points, normals, and indices");

    struct bsg_node *face_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_indexed_face_set_ref_as_node(faces)));
    unsigned char color[3] = {120, 60, 30};
    bsg_appearance_set_color_override(face_node, color, 1);
    bsg_appearance_set_dmode(face_node, 2);
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_face_set_ref_as_node(faces));

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("field-backed indexed face-set should render into retained scene");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("field-backed indexed face-set collect should find one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node)
	FAIL("retained indexed face-set node lookup failed");
    if (node->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	FAIL("retained geometry kind should be indexed face-set");
    if (node->geometry.surface.point_count != 3 ||
	    !node->geometry.surface.points ||
	    node->geometry.surface.normal_count != 3 ||
	    !node->geometry.surface.normals ||
	    node->geometry.surface.index_count != 4 ||
	    !node->geometry.surface.indices ||
	    node->geometry.surface.face_count != 1 ||
	    node->geometry.surface.normal_kind != BSG_RENDER_SURFACE_NORMALS_PER_INDEX)
	FAIL("retained indexed face-set should own a surface snapshot");
    if (node->geometry.surface.points == item->geometry.surface.points ||
	    node->geometry.surface.normals == item->geometry.surface.normals ||
	    node->geometry.surface.indices == item->geometry.surface.indices)
	FAIL("retained indexed face-set surface should be a deep copy");
    if (!VNEAR_EQUAL(node->geometry.surface.points[2], pts[2], SMALL_FASTF) ||
	    !VNEAR_EQUAL(node->geometry.surface.normals[0], normals[0], SMALL_FASTF) ||
	    node->geometry.surface.indices[3] != -1)
	FAIL("retained indexed face-set surface should preserve field data");
    if (!node->geometry.surface.material_valid ||
	    node->geometry.surface.material.color[0] != color[0] ||
	    node->geometry.surface.material.color[1] != color[1] ||
	    node->geometry.surface.material.color[2] != color[2] ||
	    node->geometry.surface.material.draw_mode != 2)
	FAIL("retained indexed face-set surface should snapshot material intent");

    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained indexed face-set snapshots should compare against render items");
    bu_vls_free(&report);

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    _free_view(v);
    PASS("backend scene indexed face surface snapshots");
    return 0;
}

static int
test_backend_scene_image_snapshots(void)
{
    printf("=== Test 8b: backend scene image snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root)
	FAIL("image snapshot root setup failed");

    unsigned char px[8] = {10, 20, 30, 255, 40, 50, 60, 255};
    struct bsg_payload *image_payload = bsg_payload_image_create(2, 1, 4, px);
    struct bsg_payload_image *source_image =
	bsg_payload_image_get(image_payload);
    if (!image_payload || !source_image || !source_image->pixels)
	FAIL("image payload setup failed");
    bsg_node *image_shape = bsg_shape_create(v);
    bsg_node_set_name(image_shape, "backend_scene_payload_image");
    bsg_node_set_payload(image_shape, image_payload);
    bsg_node_add_child(root, image_shape);

    bsg_framebuffer_ref framebuffer =
	bsg_framebuffer_ref_create(v, "backend_scene_framebuffer");
    bsg_node *framebuffer_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_framebuffer_ref_as_node(framebuffer)));
    if (!framebuffer_node || !bsg_framebuffer_ref_set_mode(framebuffer, 3))
	FAIL("framebuffer setup failed");
    bsg_node_add_child(root, framebuffer_node);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 2)
	FAIL("image/framebuffer scene should render two items");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 2)
	FAIL("image/framebuffer collect should find two items");
    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained image snapshots should compare against render items");
    bu_vls_free(&report);

    int saw_image = 0;
    int saw_framebuffer = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	const struct bsg_backend_scene_node *node =
	    bsg_backend_scene_find(scene, item->cache_identity);
	if (!node)
	    FAIL("retained image node lookup failed");
	if (node->scene_kind != BSG_BACKEND_SCENE_NODE_IMAGE ||
		node->geometry.kind != BSG_RENDER_GEOMETRY_IMAGE)
	    FAIL("retained image node should record image scene kind");
	if (item->geometry.image.kind == BSG_RENDER_IMAGE_RASTER) {
	    saw_image = 1;
	    if (node->geometry.image.kind != BSG_RENDER_IMAGE_RASTER ||
		    node->geometry.image.width != 2 ||
		    node->geometry.image.height != 1 ||
		    node->geometry.image.channels != 4 ||
		    node->geometry.image.pixel_count != sizeof(px) ||
		    !node->geometry.image.pixels ||
		    node->geometry.image.pixels == item->geometry.image.pixels ||
		    node->geometry.image.pixels == source_image->pixels ||
		    memcmp(node->geometry.image.pixels, px, sizeof(px)) != 0)
		FAIL("retained image node should own raster pixel snapshot");
	}
	if (item->geometry.image.kind == BSG_RENDER_IMAGE_FRAMEBUFFER) {
	    saw_framebuffer = 1;
	    if (node->geometry.image.kind != BSG_RENDER_IMAGE_FRAMEBUFFER ||
		    node->geometry.image.framebuffer_mode != 3 ||
		    node->geometry.image.pixel_count != 0 ||
		    node->geometry.image.pixels)
		FAIL("retained framebuffer node should snapshot framebuffer mode");
	}
    }
    if (!saw_image || !saw_framebuffer)
	FAIL("missing retained image/framebuffer item");

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(image_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene image snapshots");
    return 0;
}

static int
test_backend_scene_annotation_snapshots(void)
{
    printf("=== Test 8c: backend scene annotation snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape)
	FAIL("annotation scene setup failed");
    bsg_node_set_name(shape, "backend_scene_annotation");
    bsg_node_add_child(root, shape);
    bsg_scene_ref shape_ref = { shape };
    bsg_scene_set_color(shape_ref, 23, 45, 67);
    bsg_scene_set_transparency(shape_ref, 0.625);
    bsg_scene_set_dmode(shape_ref, 3);
    bsg_scene_set_line_width(shape_ref, 5);

    point_t pts[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 1.0, 0.0, 0.0);
    VSET(pts[1], 1.0, 2.0, 0.0);
    VSET(pts[2], 2.0, 2.0, 0.0);
    VSET(pts[3], 3.0, 3.0, 0.0);
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
    segs[4].data.text.text = (char *)"backend annotation";
    segs[4].data.text.size = 10.0;
    segs[4].data.text.rotation = 30.0;
    struct bsg_payload *payload =
	bsg_payload_annotation_create_record("backend annotation",
		BSG_ANNOTATION_SPACE_DISPLAY, pts[0], model_mat, display_mat,
		(const point_t *)pts, 4, segs, 5);
    struct bsg_payload_annotation *source_annotation =
	bsg_payload_annotation_get(payload);
    if (!payload || !source_annotation || !source_annotation->points)
	FAIL("annotation payload setup failed");
    bsg_node_set_payload(shape, payload);

    bsg_feature_ref null_feature = BSG_FEATURE_REF_NULL_INIT;
    struct bsg_interaction_record *selection_record =
	bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
		null_feature, "backend_scene_annotation", NULL);
    if (!selection_record || !bsg_view_selection(v))
	FAIL("annotation selection setup failed");
    bsg_selection_add_record(bsg_view_selection(v), selection_record);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("annotation scene should render one item");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("annotation collect should find one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node)
	FAIL("retained annotation node lookup failed");
    if (node->scene_kind != BSG_BACKEND_SCENE_NODE_OVERLAY ||
	    node->geometry.kind != BSG_RENDER_GEOMETRY_ANNOTATION)
	FAIL("retained annotation node should record annotation scene kind");
    if (node->source_identity != item->source.source_id ||
	    node->geometry.source_identity != item->geometry.source_identity ||
	    node->source.geometry_id != item->source.geometry_id)
	FAIL("retained annotation node should preserve source identity");
    if (node->overlay.phase != BSG_RENDER_PHASE_TRANSPARENT ||
	    node->selection.selected != 1 ||
	    node->selection.highlighted != 1 ||
	    node->material.color[0] != 23 ||
	    node->material.color[1] != 45 ||
	    node->material.color[2] != 67 ||
	    !NEAR_EQUAL(node->material.transparency, 0.625, SMALL_FASTF) ||
	    node->material.draw_mode != 3 ||
	    node->material.line_width != 5)
	FAIL("retained annotation node should preserve appearance and selection");
    if (!node->geometry.annotation.text ||
	    !BU_STR_EQUAL(node->geometry.annotation.text, "backend annotation") ||
	    node->geometry.annotation.text == item->geometry.annotation.text ||
	    node->geometry.annotation.text == bu_vls_cstr(&source_annotation->text) ||
	    node->geometry.annotation.space != BSG_ANNOTATION_SPACE_DISPLAY ||
	    !VNEAR_EQUAL(node->geometry.annotation.anchor, pts[0], SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.model_mat[MDX], 0.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.model_mat[MDY], 0.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.model_mat[MDZ], 0.75, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.display_mat[MDX], 1.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.display_mat[MDY], 1.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.display_mat[MDZ], 1.75, SMALL_FASTF) ||
	    node->geometry.annotation.point_count != 4 ||
	    !node->geometry.annotation.points ||
	    node->geometry.annotation.points == item->geometry.annotation.points ||
	    node->geometry.annotation.points == source_annotation->points ||
	    !VNEAR_EQUAL(node->geometry.annotation.points[3], pts[3], SMALL_FASTF) ||
	    node->geometry.annotation.segment_count != 5 ||
	    !node->geometry.annotation.segments ||
	    node->geometry.annotation.segments == item->geometry.annotation.segments ||
	    node->geometry.annotation.segments == source_annotation->segments ||
	    node->geometry.annotation.segments[0].kind != BSG_ANNOTATION_SEGMENT_LINE ||
	    node->geometry.annotation.segments[1].kind != BSG_ANNOTATION_SEGMENT_CARC ||
	    !NEAR_EQUAL(node->geometry.annotation.segments[1].data.carc.radius, 2.5, SMALL_FASTF) ||
	    node->geometry.annotation.segments[2].kind != BSG_ANNOTATION_SEGMENT_NURB ||
	    node->geometry.annotation.segments[2].data.nurb.knots == item->geometry.annotation.segments[2].data.nurb.knots ||
	    node->geometry.annotation.segments[2].data.nurb.knots == source_annotation->segments[2].data.nurb.knots ||
	    node->geometry.annotation.segments[2].data.nurb.control_points == item->geometry.annotation.segments[2].data.nurb.control_points ||
	    node->geometry.annotation.segments[2].data.nurb.control_points == source_annotation->segments[2].data.nurb.control_points ||
	    node->geometry.annotation.segments[2].data.nurb.weights == item->geometry.annotation.segments[2].data.nurb.weights ||
	    node->geometry.annotation.segments[2].data.nurb.weights == source_annotation->segments[2].data.nurb.weights ||
	    !NEAR_EQUAL(node->geometry.annotation.segments[2].data.nurb.weights[1], 0.5, SMALL_FASTF) ||
	    node->geometry.annotation.segments[3].kind != BSG_ANNOTATION_SEGMENT_BEZIER ||
	    node->geometry.annotation.segments[3].data.bezier.control_points == item->geometry.annotation.segments[3].data.bezier.control_points ||
	    node->geometry.annotation.segments[3].data.bezier.control_points == source_annotation->segments[3].data.bezier.control_points ||
	    node->geometry.annotation.segments[3].data.bezier.control_points[3] != 3 ||
	    node->geometry.annotation.segments[4].kind != BSG_ANNOTATION_SEGMENT_TEXT ||
	    node->geometry.annotation.segments[4].data.text.text == item->geometry.annotation.segments[4].data.text.text ||
	    node->geometry.annotation.segments[4].data.text.text == source_annotation->segments[4].data.text.text ||
	    !BU_STR_EQUAL(node->geometry.annotation.segments[4].data.text.text, "backend annotation") ||
	    node->geometry.annotation.segments[4].data.text.ref_pt != 3 ||
	    node->geometry.annotation.segments[4].data.text.relative_position != 1 ||
	    !NEAR_EQUAL(node->geometry.annotation.segments[4].data.text.size, 10.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->geometry.annotation.segments[4].data.text.rotation, 30.0, SMALL_FASTF))
	FAIL("retained annotation node should own annotation snapshot");

    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained annotation snapshot should compare against render item");
    bu_vls_free(&report);

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    bsg_interaction_record_free(selection_record);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene annotation snapshots");
    return 0;
}

static int
test_backend_scene_proxy_snapshots(void)
{
    printf("=== Test 8d: backend scene proxy snapshots ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *csg_shape = bsg_shape_create(v);
    bsg_node *null_csg_shape = bsg_shape_create(v);
    bsg_node *brep_shape = bsg_shape_create(v);
    bsg_node *external_shape = bsg_shape_create(v);
    bsg_node *null_external_shape = bsg_shape_create(v);
    bsg_node *preview_shape = bsg_shape_create(v);
    if (!root || !csg_shape || !null_csg_shape || !brep_shape ||
	    !external_shape || !null_external_shape || !preview_shape)
	FAIL("proxy scene setup failed");
    bsg_node_add_child(root, csg_shape);
    bsg_node_add_child(root, null_csg_shape);
    bsg_node_add_child(root, brep_shape);
    bsg_node_add_child(root, external_shape);
    bsg_node_add_child(root, null_external_shape);
    bsg_node_add_child(root, preview_shape);

    int csg_source = 63;
    int brep_source = 64;
    int external_source = 66;
    int preview_source = 65;
    struct bsg_payload *csg_payload = bsg_payload_csg_create(&csg_source);
    struct bsg_payload *null_csg_payload = bsg_payload_csg_create(NULL);
    struct bsg_payload *brep_payload = bsg_payload_brep_create(&brep_source);
    struct bsg_payload *external_payload = bsg_payload_create(BSG_PL_NONE);
    struct bsg_payload *null_external_payload = bsg_payload_create(BSG_PL_NONE);
    struct bsg_payload *preview_payload =
	bsg_payload_edit_preview_create(&preview_source, NULL);
    struct bsg_edit_preview_source *preview_data =
	preview_payload ? bsg_payload_edit_preview_get_data(preview_payload) : NULL;
    if (!csg_payload || !null_csg_payload || !brep_payload ||
	    !external_payload || !null_external_payload ||
	    !preview_payload || !preview_data)
	FAIL("proxy payload setup failed");
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
    bsg_node_set_payload(preview_shape, preview_payload);

    struct proxy_backend_expect {
	bsg_render_geometry_kind geometry_kind;
	bsg_render_proxy_kind proxy_kind;
	uint64_t source_identity;
	bsg_payload_realization_kind realization_kind;
	bsg_payload_realization_status realization_status;
	bsg_payload_stale_reason stale_reason;
	int styled;
	int seen;
    } expected[6] = {
	{BSG_RENDER_GEOMETRY_CSG_PROXY, BSG_RENDER_PROXY_CSG,
	    (uint64_t)(uintptr_t)&csg_source, BSG_REALIZE_ADAPTIVE_WIREFRAME,
	    BSG_REALIZE_STATUS_STALE, BSG_PAYLOAD_STALE_SOURCE_CHANGED, 0, 0},
	{BSG_RENDER_GEOMETRY_CSG_PROXY, BSG_RENDER_PROXY_CSG,
	    (uint64_t)(uintptr_t)null_csg_payload, BSG_REALIZE_ADAPTIVE_WIREFRAME,
	    BSG_REALIZE_STATUS_CURRENT, BSG_PAYLOAD_STALE_NONE, 0, 0},
	{BSG_RENDER_GEOMETRY_BREP_PROXY, BSG_RENDER_PROXY_BREP,
	    (uint64_t)(uintptr_t)&brep_source, BSG_REALIZE_BREP_DISPLAY,
	    BSG_REALIZE_STATUS_STALE, BSG_PAYLOAD_STALE_SOURCE_CHANGED, 0, 0},
	{BSG_RENDER_GEOMETRY_EXTERNAL_PROXY, BSG_RENDER_PROXY_EXTERNAL,
	    (uint64_t)(uintptr_t)&external_source, BSG_REALIZE_NONE,
	    BSG_REALIZE_STATUS_STALE, BSG_PAYLOAD_STALE_SOURCE_CHANGED, 1, 0},
	{BSG_RENDER_GEOMETRY_EXTERNAL_PROXY, BSG_RENDER_PROXY_EXTERNAL,
	    (uint64_t)(uintptr_t)null_external_payload, BSG_REALIZE_NONE,
	    BSG_REALIZE_STATUS_CURRENT, BSG_PAYLOAD_STALE_NONE, 0, 0},
	{BSG_RENDER_GEOMETRY_EDIT_PREVIEW, BSG_RENDER_PROXY_EDIT_PREVIEW,
	    (uint64_t)(uintptr_t)preview_data, BSG_REALIZE_EDIT_PREVIEW,
	    BSG_REALIZE_STATUS_CURRENT, BSG_PAYLOAD_STALE_NONE, 0, 0}
    };

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 6)
	FAIL("proxy scene should render six items");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 6)
	FAIL("proxy collect should find six items");
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	const struct bsg_backend_scene_node *node =
	    bsg_backend_scene_find(scene, item->cache_identity);
	int matched = 0;
	if (!node)
	    FAIL("retained proxy node lookup failed");
	if (node->scene_kind != BSG_BACKEND_SCENE_NODE_GEOMETRY)
	    FAIL("retained proxy node should be geometry");
	for (size_t j = 0; j < sizeof(expected) / sizeof(expected[0]); j++) {
	    if (node->geometry.kind != expected[j].geometry_kind ||
		    node->geometry.proxy.kind != expected[j].proxy_kind ||
		    node->geometry.proxy.source_identity != expected[j].source_identity)
		continue;
	    matched = 1;
	    expected[j].seen = 1;
	    if (node->geometry.source_identity !=
		    node->geometry.proxy.source_identity ||
		    node->geometry.proxy.revision != item->payload_revision)
		FAIL("retained proxy node should snapshot proxy metadata");
	    if (node->geometry.realization_kind != expected[j].realization_kind ||
		    node->geometry.realization_status != expected[j].realization_status ||
		    node->geometry.stale_reason != expected[j].stale_reason)
		FAIL("retained proxy node should snapshot realization state");
	    if (expected[j].styled &&
		    (node->overlay.phase != BSG_RENDER_PHASE_TRANSPARENT ||
		    node->material.color[0] != external_color[0] ||
		    node->material.color[1] != external_color[1] ||
		    node->material.color[2] != external_color[2] ||
		    !NEAR_EQUAL(node->material.transparency, 0.375, SMALL_FASTF) ||
		    node->material.draw_mode != 4 ||
		    node->material.line_width != 7 ||
		    node->material.line_style != 1))
		FAIL("retained styled proxy node should snapshot material/phase");
	}
	if (!matched)
	    FAIL("retained proxy node should record expected proxy geometry kind");
    }
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
	if (!expected[i].seen)
	    FAIL("retained proxy scene should include every proxy kind");
    }

    struct bu_vls report = BU_VLS_INIT_ZERO;
    if (bsg_backend_scene_compare_items(scene, &items, &report) != 0)
	FAIL("retained proxy snapshot should compare against render item");
    bu_vls_free(&report);

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(csg_shape);
    bsg_shape_destroy(null_csg_shape);
    bsg_shape_destroy(brep_shape);
    bsg_shape_destroy(external_shape);
    bsg_shape_destroy(null_external_shape);
    bsg_shape_destroy(preview_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene proxy snapshots");
    return 0;
}

static int
test_backend_scene_proxy_revision_replaces_cache(void)
{
    printf("=== Test 8d2: backend scene proxy revision replaces cache ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_csg_proxy_ref csg =
	bsg_csg_proxy_ref_create(v, "backend_proxy_revision");
    bsg_node *node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_csg_proxy_ref_as_node(csg)));
    if (!root || !node)
	FAIL("proxy revision scene setup failed");

    int csg_source = 2718;
    struct bsg_payload *payload = bsg_payload_csg_create(&csg_source);
    if (!payload)
	FAIL("proxy revision payload setup failed");
    bsg_node_set_payload(node, payload);
    bsg_node_add_child(root, node);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend scene create failed");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("initial proxy revision render should produce one item");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("initial proxy revision collect should find one item");
    struct bsg_render_item *item =
	(struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item || item->geometry.kind != BSG_RENDER_GEOMETRY_CSG_PROXY ||
	    item->geometry.proxy.kind != BSG_RENDER_PROXY_CSG)
	FAIL("initial proxy revision item metadata wrong");
    uint64_t first_cache_identity = item->cache_identity;
    uint64_t first_source_identity = item->source.source_id;
    uint64_t first_geometry_identity = item->source.geometry_id;
    uint64_t first_proxy_source = item->geometry.proxy.source_identity;
    uint64_t first_payload_revision = item->payload_revision;
    if (!first_cache_identity || !first_source_identity ||
	    !first_geometry_identity || !first_proxy_source)
	FAIL("initial proxy revision identities should be populated");
    const struct bsg_backend_scene_node *retained =
	bsg_backend_scene_find(scene, first_cache_identity);
    if (!retained)
	FAIL("initial retained proxy cache lookup failed");
    if (retained->source_identity != first_source_identity ||
	    retained->source.geometry_id != first_geometry_identity ||
	    retained->geometry.proxy.source_identity != first_proxy_source ||
	    retained->payload_revision != first_payload_revision)
	FAIL("initial retained proxy cache snapshot wrong");
    if (bsg_backend_scene_count(scene) != 1)
	FAIL("initial retained proxy scene should have one node");
    _free_items(&items);

    bsg_payload_bump_revision(payload);
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != 1)
	FAIL("updated proxy revision render should produce one item");

    if (_collect_render_items(v, &items) != 1)
	FAIL("updated proxy revision collect should find one item");
    item = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    if (!item || item->payload_revision <= first_payload_revision)
	FAIL("updated proxy payload revision should advance");
    if (item->cache_identity == first_cache_identity)
	FAIL("updated proxy cache identity should change with payload revision");
    if (item->source.source_id != first_source_identity ||
	    item->source.geometry_id != first_geometry_identity ||
	    item->geometry.proxy.source_identity != first_proxy_source)
	FAIL("updated proxy source identities should stay stable");
    if (bsg_backend_scene_find(scene, first_cache_identity) != NULL)
	FAIL("old proxy cache entry should be pruned after revision update");
    retained = bsg_backend_scene_find(scene, item->cache_identity);
    if (!retained)
	FAIL("updated retained proxy cache lookup failed");
    if (bsg_backend_scene_count(scene) != 1)
	FAIL("updated retained proxy scene should have one node");
    if (retained->source_identity != first_source_identity ||
	    retained->source.geometry_id != first_geometry_identity ||
	    retained->geometry.proxy.source_identity != first_proxy_source ||
	    retained->payload_revision != item->payload_revision ||
	    retained->geometry.proxy.revision != item->payload_revision)
	FAIL("updated retained proxy cache snapshot wrong");
    struct bsg_backend_scene_stats stats;
    memset(&stats, 0, sizeof(stats));
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.created != 1 || stats.removed != 1 || stats.node_count != 1)
	FAIL("proxy revision update should create new cache entry and prune old one");

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene proxy revision replaces cache");
    return 0;
}

struct backend_edit_preview_fanout_check {
    size_t expected;
    size_t seen;
    bsg_payload_realization_status status;
    bsg_payload_stale_reason reason;
    int require_bounds;
    point_t bounds_center;
    fastf_t bounds_radius;
};

static int
_backend_edit_preview_fanout_check_cb(const struct bsg_backend_scene_node *node,
				      void *userdata)
{
    struct backend_edit_preview_fanout_check *check =
	(struct backend_edit_preview_fanout_check *)userdata;
    if (!node || !check)
	return 0;
    if (node->scene_kind != BSG_BACKEND_SCENE_NODE_GEOMETRY ||
	    node->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
	    node->geometry.proxy.kind != BSG_RENDER_PROXY_EDIT_PREVIEW ||
	    node->geometry.realization_kind != BSG_REALIZE_EDIT_PREVIEW ||
	    node->geometry.realization_status != check->status ||
	    node->geometry.stale_reason != check->reason ||
	    node->geometry.proxy.source_identity == 0)
	return 0;
    if (check->require_bounds &&
	    (!VNEAR_EQUAL(node->transform.bounds_center, check->bounds_center, SMALL_FASTF) ||
	    !NEAR_EQUAL(node->transform.bounds_radius, check->bounds_radius, SMALL_FASTF)))
	return 0;
    check->seen++;
    return 1;
}

static int
test_backend_scene_edit_preview_fanout(void)
{
    printf("=== Test 8e: backend scene edit-preview fanout ===\n");

    enum { FANOUT = 5 };
    struct backend_edit_preview_fanout_state state;
    memset(&state, 0, sizeof(state));
    state.revision = 40;
    state.update_returns = 1;
    VSET(state.bounds_min, -1.0, 0.0, 0.0);
    VSET(state.bounds_max, 1.0, 0.0, 0.0);

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shapes[FANOUT];
    memset(shapes, 0, sizeof(shapes));
    for (int i = 0; i < FANOUT; i++) {
	char name[64];
	snprintf(name, sizeof(name), "backend_preview_fanout_%d", i);
	shapes[i] = bsg_shape_create(v);
	if (!shapes[i])
	    FAIL("backend edit-preview fanout shape create");
	bsg_node_set_name(shapes[i], name);
	bsg_node_add_child(root, shapes[i]);
	struct bsg_payload *payload =
	    bsg_payload_edit_preview_create(&state, NULL);
	if (!payload)
	    FAIL("backend edit-preview fanout payload create");
	if (!bsg_payload_edit_preview_set_ops(payload, &state, 0,
		    backend_edit_preview_revision,
		    backend_edit_preview_update,
		    backend_edit_preview_bounds, NULL, NULL, NULL))
	    FAIL("backend edit-preview fanout set_ops");
	if (!bsg_payload_edit_preview_mark_source_revision(payload,
		    300 + (uint64_t)i,
		    BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED))
	    FAIL("backend edit-preview fanout stale mark");
	bsg_node_set_payload(shapes[i], payload);
    }

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene)
	FAIL("backend edit-preview fanout scene create");
    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY) != FANOUT)
	FAIL("backend edit-preview fanout stale render count");
    if (state.update_calls != 0)
	FAIL("backend edit-preview fanout should not update without prepare");
    struct backend_edit_preview_fanout_check check;
    memset(&check, 0, sizeof(check));
    check.expected = FANOUT;
    check.status = BSG_REALIZE_STATUS_STALE;
    check.reason = BSG_PAYLOAD_STALE_SOURCE_CHANGED;
    check.require_bounds = 1;
    VSET(check.bounds_center, 0.0, 0.0, 0.0);
    check.bounds_radius = 1.0;
    if (!bsg_backend_scene_foreach_node(scene,
		_backend_edit_preview_fanout_check_cb, &check) ||
	    check.seen != check.expected)
	FAIL("backend edit-preview fanout stale retained state");
    if (state.bounds_calls != FANOUT)
	FAIL("backend edit-preview fanout stale render should query live bounds once per payload");

    if (bsg_backend_scene_render_request(v, scene,
		BSG_RENDER_FLAG_VISIBLE_ONLY |
		BSG_RENDER_FLAG_PAYLOAD_PREPARE) != FANOUT)
	FAIL("backend edit-preview fanout prepared render count");
    if (state.update_calls != FANOUT)
	FAIL("backend edit-preview fanout should update every payload");
    memset(&check, 0, sizeof(check));
    check.expected = FANOUT;
    check.status = BSG_REALIZE_STATUS_CURRENT;
    check.reason = BSG_PAYLOAD_STALE_NONE;
    check.require_bounds = 1;
    VSET(check.bounds_center, 0.0, 0.0, 0.0);
    check.bounds_radius = 1.0;
    if (bsg_backend_scene_count(scene) != FANOUT ||
	    !bsg_backend_scene_foreach_node(scene,
		_backend_edit_preview_fanout_check_cb, &check) ||
	    check.seen != check.expected)
	FAIL("backend edit-preview fanout current retained state");
    if (state.bounds_calls != FANOUT * 2)
	FAIL("prepared current edit previews should reuse cached realization bounds");

    bsg_backend_scene_destroy(scene);
    for (int i = 0; i < FANOUT; i++)
	bsg_shape_destroy(shapes[i]);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene edit-preview fanout");
    return 0;
}

/* -----------------------------------------------------------------------
 * Test 9: retained backend scene maps text/overlay semantics
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_node_free_item(void)
{
    printf("=== Test 10: backend scene node-free item ===\n");

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene) FAIL("backend scene create failed");
    struct bsg_backend_adapter *adapter = bsg_backend_scene_adapter(scene);
    if (!adapter) FAIL("backend scene adapter missing");

    struct bsg_render_item *item = bsg_render_item_create();
    item->name = "node-free";
    item->source.name = item->name;
    item->source.source_id = 1001;
    item->source.geometry_id = 3003;
    item->source.lod_id = 5005;
    item->source.scope = BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL;
    item->source.geometry_role = BSG_RENDER_GEOMETRY_ROLE_LINE_SET;
    item->source.draw_intent = BSG_RENDER_DRAW_INTENT_VIEW_FEATURE;
    item->source.feature_family = BSG_FEATURE_LINES;
    item->source.overlay_role = BSG_OVERLAY_ROLE_MODEL;
    item->source.overlay_class = BSG_OVERLAY_CLASS_COMMAND_RESULT;
    item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
    item->geometry.source_identity = item->source.geometry_id;
    item->geometry.revision = 4;
    item->payload_revision = 5;
    item->non_database_source = 1;
    item->graph_depth = 8;
    item->sequence = 9;
    item->order_flags = BSG_RENDER_ORDER_OPAQUE_CHILD_FIRST;
    item->features.flags = BSG_RENDER_FEATURE_ARROWS;
    item->features.arrow_tip_length = 0.25;
    item->features.arrow_tip_width = 0.125;
    item->features.hud_feature_type = 3;
    item->cache_identity = 7007;
    item->context.request_flags = BSG_RENDER_FLAG_VISIBLE_ONLY;
    item->context.backend_capabilities = BSG_ADAPTER_CAP_WIREFRAME;
    item->context.settings_hash = 8008;

    if (!adapter->begin_request(scene, NULL))
	FAIL("begin_request should succeed");
    if (!adapter->prepare(scene, item))
	FAIL("node-free item prepare should succeed");
    adapter->draw(scene, item);
    adapter->end_request(scene, NULL);

    const struct bsg_backend_scene_node *node =
	bsg_backend_scene_find(scene, item->cache_identity);
    if (!node) FAIL("node-free retained node lookup failed");
    if (node->source_identity != item->source.source_id)
	FAIL("retained source identity should use item source id");
    if (node->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("retained geometry kind should come from item");
    if (node->settings_hash != item->context.settings_hash)
	FAIL("retained settings hash should come from item context");
    if (node->backend_capabilities != item->context.backend_capabilities)
	FAIL("retained backend capabilities should come from item context");
    if (node->source.geometry_id != item->source.geometry_id ||
	    node->source.lod_id != item->source.lod_id ||
	    node->source.scope != item->source.scope ||
	    node->source.geometry_role != item->source.geometry_role ||
	    node->source.draw_intent != item->source.draw_intent ||
	    node->source.feature_family != item->source.feature_family ||
	    node->source.overlay_role != item->source.overlay_role ||
	    node->source.overlay_class != item->source.overlay_class ||
	    node->source.non_database_source != item->non_database_source ||
	    node->source.graph_depth != item->graph_depth ||
	    node->source.sequence != item->sequence ||
	    node->source.order_flags != item->order_flags)
	FAIL("retained source diagnostics should come from item source");
    if (node->features.flags != item->features.flags ||
	    !NEAR_EQUAL(node->features.arrow_tip_length, item->features.arrow_tip_length, BN_TOL_DIST) ||
	    !NEAR_EQUAL(node->features.arrow_tip_width, item->features.arrow_tip_width, BN_TOL_DIST) ||
	    node->features.hud_feature_type != item->features.hud_feature_type)
	FAIL("retained feature diagnostics should come from item features");
    if (node->draw_count != 1)
	FAIL("node-free retained node should draw once");

    bsg_backend_scene_release_source(scene, item->source.source_id);
    if (bsg_backend_scene_count(scene) != 0)
	FAIL("release_source should remove node-free retained node");

    bsg_render_item_free(item);
    bsg_backend_scene_destroy(scene);
    PASS("backend scene node-free item");
    return 0;
}


static int
test_backend_scene_semantic_kinds(void)
{
    printf("=== Test 9: backend scene semantic kinds ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_scene_ref root_ref = BSG_SCENE_REF_NULL_INIT;
    root_ref.opaque = root;
    bsg_node *text_shape = bsg_shape_create(v);
    bsg_scene_ref overlay_ref = bsg_scene_shape_create(v, "pilot-grid");
    bsg_node *overlay_shape = (bsg_node *)overlay_ref.opaque;
    bsg_node_add_child(root, text_shape);
    bsg_scene_append_child(root_ref, overlay_ref);

    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    bu_vls_init(&label->label);
    bu_vls_strcpy(&label->label, "pilot text");
    label->size = 1;
    bsg_node_set_payload(text_shape, bsg_payload_text_create(label));

    struct bsg_grid_state grid;
    memset(&grid, 0, sizeof(grid));
    grid.draw = 1;
    grid.res_h = 2.0;
    grid.res_v = 3.0;
    grid.color[0] = 10;
    grid.color[1] = 20;
    grid.color[2] = 30;
    if (!bsg_geometry_node_set_grid_overlay(overlay_shape, &grid))
	FAIL("semantic grid overlay setup failed");
    bsg_overlay_register_scene_owner(overlay_ref, root, BSG_OVERLAY_ROLE_SCREEN,
	    BSG_OVERLAY_CLASS_DIAGNOSTIC, BSG_OVERLAY_LC_PERSISTENT,
	    BSG_OVERLAY_ORDER_SCREEN, "pilot/grid", 0);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (_render_to_backend_scene(v, scene) != 2)
	FAIL("semantic render should draw two items");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 2)
	FAIL("semantic collect should find two items");
    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++) {
	struct bsg_render_item *item =
	    (struct bsg_render_item *)BU_PTBL_GET(&items, i);
	const struct bsg_backend_scene_node *node =
	    bsg_backend_scene_find(scene, item->cache_identity);
	if (!node) FAIL("missing semantic retained node");
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_TEXT &&
		node->scene_kind != BSG_BACKEND_SCENE_NODE_TEXT)
	    FAIL("text item should map to text scene kind");
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_TEXT &&
		(!node->text.label ||
		 !BU_STR_EQUAL(node->text.label, "pilot text") ||
		 !BU_STR_EQUAL(node->text.label, item->geometry.text.label) ||
		 !VNEAR_EQUAL(node->text.position, item->geometry.text.position, SMALL_FASTF) ||
		 node->text.size != item->geometry.text.size))
	    FAIL("text scene node should snapshot text fields");
	if (item->phase == BSG_RENDER_PHASE_OVERLAY &&
		node->scene_kind != BSG_BACKEND_SCENE_NODE_OVERLAY)
	    FAIL("overlay item should map to overlay scene kind");
	if (node->overlay.phase != item->phase)
	    FAIL("retained overlay phase should mirror item phase");
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_OVERLAY &&
		item->geometry.overlay.kind != BSG_RENDER_OVERLAY_GEOMETRY_GRID)
	    FAIL("overlay item should carry typed grid geometry");
	if (item->geometry.kind == BSG_RENDER_GEOMETRY_OVERLAY &&
		(node->overlay.geometry_kind != BSG_RENDER_OVERLAY_GEOMETRY_GRID ||
		 !VNEAR_EQUAL(node->overlay.grid.anchor, item->geometry.overlay.grid.anchor, SMALL_FASTF) ||
		 !NEAR_EQUAL(node->overlay.grid.res_h, 2.0, SMALL_FASTF) ||
		 !NEAR_EQUAL(node->overlay.grid.res_v, 3.0, SMALL_FASTF) ||
		 node->overlay.grid.color[0] != 10 ||
		 node->overlay.grid.color[1] != 20 ||
		 node->overlay.grid.color[2] != 30))
	    FAIL("retained overlay should snapshot grid fields");
    }

    _free_items(&items);
    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(text_shape);
    bsg_shape_destroy(overlay_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene semantic kinds");
    return 0;
}

static int
_count_nodes_cb(const struct bsg_backend_scene_node *node, void *userdata)
{
    size_t *count = (size_t *)userdata;
    if (!node || !count)
	return 0;
    (*count)++;
    return 1;
}

/* -----------------------------------------------------------------------
 * Test 11: retained scene exposes frame state and iterable nodes
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_frame_and_iteration(void)
{
    printf("=== Test 11: backend scene frame and iteration ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    point_t pa = VINIT_ZERO;
    point_t pb = VINIT_ZERO;
    VSET(pb, 0.0, 1.0, 0.0);
    bsg_node *s = _create_backend_line_node(v, root, "backend_scene_frame_line", pa, pb);
    if (!s) FAIL("backend frame line node create failed");

    v->gv_width = 640;
    v->gv_height = 480;
    bsg_view_set_zclip(v, 1);

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (_render_to_backend_scene(v, scene) != 1)
	FAIL("frame render should draw one item");

    const struct bsg_backend_scene_camera *camera =
	bsg_backend_scene_get_camera(scene);
    const struct bsg_backend_scene_clip *clip =
	bsg_backend_scene_get_clip(scene);
    const struct bsg_backend_scene_lights *lights =
	bsg_backend_scene_get_lights(scene);
    if (!camera || camera->viewport_width != 640 ||
	    camera->viewport_height != 480)
	FAIL("retained camera should record viewport");
    if (!clip || !clip->enabled)
	FAIL("retained clip state should record zclip");
    if (!lights)
	FAIL("retained light state should be available");

    size_t visited = 0;
    if (bsg_backend_scene_foreach_node(scene, _count_nodes_cb, &visited) != 1 ||
	    visited != 1)
	FAIL("foreach should expose retained nodes");

    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene frame and iteration");
    return 0;
}

/* -----------------------------------------------------------------------
 * Test 12: retained scene reports backend capability gaps explicitly
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_capability_gap(void)
{
    printf("=== Test 12: backend scene capability gaps ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    point_t pa = VINIT_ZERO;
    point_t pb = VINIT_ZERO;
    VSET(pb, 1.0, 1.0, 0.0);
    bsg_node *s = _create_backend_line_node(v, root, "backend_scene_transparent_line", pa, pb);
    if (!s) FAIL("backend transparent line node create failed");
    bsg_appearance_set_transparency(s, 0.5);

    struct bsg_backend_scene *scene =
	bsg_backend_scene_create_with_capabilities(BSG_ADAPTER_CAP_WIREFRAME);
    if (!scene) FAIL("capability-limited scene create failed");
    if (_render_to_backend_scene(v, scene) != 1)
	FAIL("capability-gap render should draw one item");

    struct bsg_backend_scene_stats stats;
    bsg_backend_scene_get_stats(scene, &stats);
    if (stats.capability_gaps != 1)
	FAIL("transparent item should produce a retained capability gap");

    struct bu_ptbl items;
    if (_collect_render_items(v, &items) != 1)
	FAIL("capability-gap collect should find one item");
    struct bu_vls report = BU_VLS_INIT_ZERO;
    int mismatches = bsg_backend_scene_compare_items(scene, &items, &report);
    if (mismatches < 1)
	FAIL("compare should report capability gap as a backend difference");
    if (!strstr(bu_vls_cstr(&report), "capability-gap") ||
	    !strstr(bu_vls_cstr(&report), "transparency"))
	FAIL("capability gap report should name missing transparency");

    _free_items(&items);
    bu_vls_free(&report);
    bsg_backend_scene_destroy(scene);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene capability gaps");
    return 0;
}

/* -----------------------------------------------------------------------
 * Test 13: production retained update API and adapter selection
 * ----------------------------------------------------------------------- */
static int
test_backend_scene_update_and_selection(void)
{
    printf("=== Test 13: backend scene update and adapter selection ===\n");

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    point_t pa = VINIT_ZERO;
    point_t pb = VINIT_ZERO;
    VSET(pb, 2.0, 0.0, 0.0);
    bsg_node *s = _create_backend_line_node(v, root, "backend_scene_update_line", pa, pb);
    if (!s) FAIL("backend update line node create failed");

    struct bsg_backend_scene *scene = bsg_backend_scene_create();
    if (!scene) FAIL("retained scene create failed");

    struct bu_vls diagnostics = BU_VLS_INIT_ZERO;
    struct bsg_backend_adapter *adapter =
	bsg_backend_scene_select_adapter(
		BSG_BACKEND_SCENE_ADAPTER_RETAINED_REFERENCE,
		scene, NULL, &diagnostics);
    if (adapter != bsg_backend_scene_adapter(scene))
	FAIL("retained adapter selection should return scene adapter");

    struct bsg_backend_adapter external;
    memset(&external, 0, sizeof(external));
    if (bsg_backend_scene_select_adapter(BSG_BACKEND_SCENE_ADAPTER_EXTERNAL,
		NULL, &external, &diagnostics) != &external)
	FAIL("external adapter selection should return supplied adapter");

    if (bsg_backend_scene_select_adapter(BSG_BACKEND_SCENE_ADAPTER_OBOL_RESERVED,
		scene, NULL, &diagnostics) != NULL)
	FAIL("reserved Obol selection should not synthesize an adapter");
    if (!strstr(bu_vls_cstr(&diagnostics), "obol"))
	FAIL("reserved Obol selection should explain the integration boundary");

    struct bsg_backend_scene_update_result result;
    bu_vls_trunc(&diagnostics, 0);
    if (bsg_backend_scene_update(scene, v,
		BSG_RENDER_FLAG_VISIBLE_ONLY, &result, &diagnostics) != 1)
	FAIL("retained update should render one item");
    if (result.rendered != 1 || result.scene != scene ||
	    result.stats.node_count != 1 || result.stats.created != 1)
	FAIL("retained update result should report the updated scene and created node");
    if (result.capabilities != bsg_backend_scene_capabilities(scene))
	FAIL("retained update result should report capabilities");
    if (!strstr(bu_vls_cstr(&diagnostics), "retained-scene") ||
	    !strstr(bu_vls_cstr(&diagnostics), "retained-node"))
	FAIL("retained diagnostics should describe scene and nodes");

    bu_vls_trunc(&diagnostics, 0);
    bsg_action_ref action = bsg_backend_scene_action_create(NULL, v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY, NULL, &diagnostics);
    if (bsg_action_ref_is_null(action))
	FAIL("backend scene action should create");
    if (bsg_action_apply(action, bsg_node_ref_null()) != 1)
	FAIL("backend scene action should render one item");
    struct bsg_backend_scene_update_result action_result;
    struct bsg_backend_scene *action_scene =
	bsg_backend_scene_action_result(action, &action_result);
    if (!action_scene || action_result.scene != action_scene ||
	    action_result.rendered != 1 || action_result.stats.node_count != 1)
	FAIL("backend scene action should expose a retained scene result");
    if (!strstr(bu_vls_cstr(&diagnostics), "retained-scene") ||
	    !strstr(bu_vls_cstr(&diagnostics), "retained-node"))
	FAIL("backend scene action diagnostics should describe result records");
    bsg_action_ref_destroy(action);
    if (bsg_backend_scene_count(action_scene) != 1)
	FAIL("transferred backend scene action result should survive action destroy");
    bsg_backend_scene_destroy(action_scene);

    struct bsg_backend_scene_update_result wrong_result;
    memset(&wrong_result, 0xff, sizeof(wrong_result));
    bsg_action_ref wrong_action = bsg_bbox_action_create();
    if (bsg_backend_scene_action_result(wrong_action, &wrong_result) != NULL ||
	    wrong_result.scene != NULL || wrong_result.rendered != 0)
	FAIL("backend scene action result should reject non-backend-scene actions");
    bsg_action_ref_destroy(wrong_action);

    bsg_backend_scene_destroy(scene);
    bu_vls_free(&diagnostics);
    bsg_shape_destroy(s);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    PASS("backend scene update and adapter selection");
    return 0;
}


/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;

    int failures = 0;
    failures += test_reason_bits_distinct();
    failures += test_reason_passed_through();
    failures += test_capability_bits_distinct();
    failures += test_null_invalidate_safe();
    failures += test_request_settings_populated();
    failures += test_backend_scene_retained_cache();
    failures += test_backend_scene_geometry_snapshots();
    failures += test_backend_scene_indexed_geometry_snapshots();
    failures += test_backend_scene_indexed_face_surface_snapshots();
    failures += test_backend_scene_image_snapshots();
    failures += test_backend_scene_annotation_snapshots();
    failures += test_backend_scene_proxy_snapshots();
    failures += test_backend_scene_proxy_revision_replaces_cache();
    failures += test_backend_scene_edit_preview_fanout();
    failures += test_backend_scene_semantic_kinds();
    failures += test_backend_scene_node_free_item();
    failures += test_backend_scene_frame_and_iteration();
    failures += test_backend_scene_capability_gap();
    failures += test_backend_scene_update_and_selection();

    if (failures) {
	printf("\n%d test(s) FAILED\n", failures);
	return 1;
    }
    printf("\nAll backend_adapter tests PASSED\n");
    return 0;
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
