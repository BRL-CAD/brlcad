/*            T E S T _ P A Y L O A D _ T Y P E D . C
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
/** @file libbsg/tests/test_payload_typed.c
 *
 * Phase D1 typed payload regression tests.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/draw_source.h"
#include "bsg/faceplate.h"
#include "bsg/geometry.h"
#include "../node_private.h"
#include "../payload_typed_private.h"
#include "bsg/polygon.h"
#include "../polygon_private.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "../payload_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "payload_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "payload_view");
}

struct sketch_preview_stub {
    uint64_t revision;
    int update_calls;
    int bounds_calls;
    int pick_calls;
    int snap_calls;
    point_t bmin;
    point_t bmax;
    point_t snap_out;
    int pick_id;
};

static int g_sketch_preview_free_calls = 0;

static uint64_t
sketch_stub_revision(void *preview_ctx)
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    return stub->revision;
}

static int
sketch_stub_update(void *preview_ctx, struct bsg_view *UNUSED(v))
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    stub->update_calls++;
    stub->revision++;
    return 1;
}

static int
sketch_stub_bounds(void *preview_ctx, point_t *bmin, point_t *bmax)
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    stub->bounds_calls++;
    VMOVE((*bmin), stub->bmin);
    VMOVE((*bmax), stub->bmax);
    return 1;
}

static int
sketch_stub_pick(void *preview_ctx, struct bsg_view *UNUSED(v), int UNUSED(x), int UNUSED(y), void *pick_out)
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    stub->pick_calls++;
    if (pick_out)
	*((int *)pick_out) = stub->pick_id;
    return 1;
}

static int
sketch_stub_snap(void *preview_ctx, struct bsg_view *UNUSED(v), const point_t UNUSED(sample_pt), point_t out_pt)
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    stub->snap_calls++;
    VMOVE(out_pt, stub->snap_out);
    return 1;
}

static void
sketch_stub_free(void *preview_ctx)
{
    struct sketch_preview_stub *stub = (struct sketch_preview_stub *)preview_ctx;
    g_sketch_preview_free_calls++;
    bu_free(stub, "sketch preview stub");
}

static int
test_polygon_payload(void)
{
    printf("=== Test 2: polygon payload ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("bsg_scene_root_create returned NULL");
    point_t origin = VINIT_ZERO;
    bsg_node *poly = bsg_create_polygon(v, BSG_SOURCE_VIEW, BSG_POLYGON_RECTANGLE, &origin);
    if (!poly) FAIL("bsg_create_polygon returned NULL");
    if (!bsg_node_polygon(poly)) FAIL("node polygon accessor returned NULL");
    if (!bsg_node_get_payload(poly) || bsg_node_get_payload(poly)->pl_type != BSG_PL_POLYGON)
	FAIL("polygon node missing typed payload");

    bsg_shape_destroy(poly);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("polygon payload");
    return 0;
}

static int
test_remaining_payload_builders(void)
{
    printf("=== Test 3: remaining payload builders ===\n");

    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    BU_VLS_INIT(&label->label);
    bu_vls_sprintf(&label->label, "hud");

    struct bsg_payload *hud = bsg_payload_hud_text_create(label);
    if (!hud || !bsg_payload_hud_text_get(hud)) FAIL("hud text payload");
    bsg_payload_free(hud);

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    struct bsg_payload *line_set = bsg_payload_line_set_create(pts, cmds, 2);
    if (!line_set || !bsg_payload_line_set_get(line_set)) FAIL("line set payload");
    bsg_payload_free(line_set);

    unsigned char px[4] = {255, 0, 0, 255};
    struct bsg_payload *image = bsg_payload_image_create(1, 1, 4, px);
    if (!image || !bsg_payload_image_get(image)) FAIL("image payload");
    bsg_payload_free(image);

    struct bsg_payload *fb = bsg_payload_framebuffer_create(NULL, 7);
    if (!fb || !bsg_payload_framebuffer_get(fb) || bsg_payload_framebuffer_get(fb)->mode != 7)
	FAIL("framebuffer payload");
    bsg_payload_free(fb);

    struct bsg_grid_state grid;
    memset(&grid, 0, sizeof(grid));
    grid.draw = 1;
    struct bsg_payload *gpl = bsg_payload_grid_create(&grid);
    if (!gpl || !bsg_payload_grid_get(gpl) || !bsg_payload_grid_get(gpl)->draw)
	FAIL("grid payload");
    bsg_payload_free(gpl);

    struct bsg_payload *ann = bsg_payload_annotation_create("measure", pts, 2);
    if (!ann || !bsg_payload_annotation_get(ann)) FAIL("annotation payload");
    bsg_payload_free(ann);

    struct bsg_payload *mesh = bsg_payload_mesh_create(NULL);
    struct bsg_payload *csg = bsg_payload_csg_create(NULL);
    struct bsg_payload *brep = bsg_payload_brep_create(NULL);
    if (!mesh || mesh->pl_type != BSG_PL_MESH) FAIL("mesh payload");
    if (!csg || csg->pl_type != BSG_PL_CSG) FAIL("csg payload");
    if (!brep || brep->pl_type != BSG_PL_BREP) FAIL("brep payload");
    bsg_payload_free(mesh);
    bsg_payload_free(csg);
    bsg_payload_free(brep);

    PASS("remaining payload builders");
    return 0;
}

static int
test_lifecycle_hooks(void)
{
    printf("=== Test 5: lifecycle hook dispatch ===\n");

    /* ---- LINE_SET payload: real bounds plus default lifecycle hooks ---- */
    struct bsg_view *v = make_view();
    bsg_node *shape = bsg_shape_create(v);
    if (!shape) FAIL("bsg_shape_create");

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    int cmds[3] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[0], 0.0, 0.0, 0.0);
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 0.5, 1.0, 0.0);
    struct bsg_payload *line_payload = bsg_payload_line_set_create(pts, cmds, 3);
    if (!line_payload) FAIL("line-set payload create");
    bsg_node_set_payload(shape, line_payload);

    struct bsg_payload *pl = bsg_node_get_payload(shape);
    if (!pl || pl->pl_type != BSG_PL_LINE_SET) FAIL("line-set payload missing");

    /* bounds hook should return 1 and give sensible extents */
    if (!pl->pl_bounds) FAIL("line-set pl_bounds is NULL");
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    int bounds_ok = pl->pl_bounds(pl, &bmin, &bmax);
    if (!bounds_ok) FAIL("line-set pl_bounds returned 0 (expected 1)");
    if (bmin[0] > 0.0 || bmax[0] < 1.0) FAIL("line-set bounds X range wrong");
    if (bmin[1] > 0.0 || bmax[1] < 1.0) FAIL("line-set bounds Y range wrong");

    /* export/backend hooks use the default no-op lifecycle hook. */
    if (!pl->pl_export) FAIL("line-set pl_export is NULL");
    struct bu_vls export_out = BU_VLS_INIT_ZERO;
    int export_rc = pl->pl_export(pl, &export_out);
    if (export_rc != 0) FAIL("line-set pl_export returned non-zero (expected 0)");
    if (bu_vls_strlen(&export_out) != 0) FAIL("line-set pl_export produced output");
    bu_vls_free(&export_out);

    if (!pl->pl_backend_prepare) FAIL("line-set pl_backend_prepare is NULL");
    if (pl->pl_backend_prepare(pl, NULL) != 0) FAIL("line-set backend_prepare sentinel returned non-zero");

    bsg_shape_destroy(shape);
    free_view(v);

    /* ---- Non-VLIST types: sentinel hooks return 0 ---- */

    /* TEXT sentinel */
    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    BU_VLS_INIT(&label->label);
    bu_vls_sprintf(&label->label, "sentinel test");
    struct bsg_payload *text_pl = bsg_payload_hud_text_create(label);
    if (!text_pl) FAIL("hud_text payload create");
    if (!text_pl->pl_bounds) FAIL("text pl_bounds is NULL");
    point_t tbmin = VINIT_ZERO, tbmax = VINIT_ZERO;
    if (text_pl->pl_bounds(text_pl, &tbmin, &tbmax) != 0)
	FAIL("text pl_bounds sentinel returned non-zero");
    if (!text_pl->pl_export) FAIL("text pl_export is NULL");
    struct bu_vls text_export = BU_VLS_INIT_ZERO;
    if (text_pl->pl_export(text_pl, &text_export) != 0)
	FAIL("text pl_export sentinel returned non-zero");
    bu_vls_free(&text_export);
    if (!text_pl->pl_backend_prepare) FAIL("text pl_backend_prepare is NULL");
    if (text_pl->pl_backend_prepare(text_pl, NULL) != 0)
	FAIL("text pl_backend_prepare sentinel returned non-zero");
    bsg_payload_free(text_pl);

    /* IMAGE sentinel */
    unsigned char px[4] = {128, 64, 32, 255};
    struct bsg_payload *img_pl = bsg_payload_image_create(1, 1, 4, px);
    if (!img_pl) FAIL("image payload create");
    if (!img_pl->pl_bounds) FAIL("image pl_bounds is NULL");
    point_t ibmin = VINIT_ZERO, ibmax = VINIT_ZERO;
    if (img_pl->pl_bounds(img_pl, &ibmin, &ibmax) != 0)
	FAIL("image pl_bounds sentinel returned non-zero");
    bsg_payload_free(img_pl);

    PASS("lifecycle hook dispatch");
    return 0;
}


static int
test_sketch_live_contract(void)
{
    printf("=== Test 4: sketch edit-preview contract ===\n");

    int rt_edit_placeholder = 0;
    int grid_placeholder = 0;
    struct bsg_payload *sketch =
	bsg_payload_sketch_create((void *)&rt_edit_placeholder, (void *)&grid_placeholder);
    if (!sketch || sketch->pl_type != BSG_PL_SKETCH) FAIL("sketch payload create");

    struct sketch_preview_stub *stub =
	(struct sketch_preview_stub *)bu_calloc(1, sizeof(struct sketch_preview_stub), "sketch preview stub");
    stub->revision = 7;
    stub->pick_id = 42;
    VSET(stub->bmin, -1.0, -2.0, -3.0);
    VSET(stub->bmax, 4.0, 5.0, 6.0);
    VSET(stub->snap_out, 0.25, 0.5, 0.75);

    g_sketch_preview_free_calls = 0;
    if (bsg_payload_sketch_set_live_ops(
	    sketch,
	    stub,
	    1,
	    sketch_stub_revision,
	    sketch_stub_update,
	    sketch_stub_bounds,
	    sketch_stub_pick,
	    sketch_stub_snap,
	    sketch_stub_free))
	{
	}
    else
	FAIL("set sketch live ops");

    if (bsg_payload_sketch_revision(sketch) != 7)
	FAIL("initial sketch revision");

    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    if (!bsg_payload_sketch_bounds(sketch, &bmin, &bmax))
	FAIL("sketch bounds callback");
    if (!NEAR_EQUAL(bmin[0], -1.0, SMALL_FASTF) || !NEAR_EQUAL(bmax[2], 6.0, SMALL_FASTF))
	FAIL("sketch bounds values");

    int pick_id = -1;
    if (!bsg_payload_sketch_pick(sketch, NULL, 10, 20, &pick_id))
	FAIL("sketch pick callback");
    if (pick_id != 42)
	FAIL("sketch pick value");

    point_t sample_pt = VINIT_ZERO;
    point_t snap_pt = VINIT_ZERO;
    if (!bsg_payload_sketch_snap(sketch, NULL, sample_pt, snap_pt))
	FAIL("sketch snap callback");
    if (!NEAR_EQUAL(snap_pt[1], 0.5, SMALL_FASTF))
	FAIL("sketch snap value");

    if (bsg_payload_sketch_realize(sketch, NULL) != 1)
	FAIL("sketch realize should report revision change");
    if (stub->update_calls != 1)
	FAIL("sketch update callback count");
    if (bsg_payload_sketch_revision(sketch) != 8)
	FAIL("sketch revision after realize");
    if (stub->bounds_calls != 1 || stub->pick_calls != 1 || stub->snap_calls != 1)
	FAIL("sketch callback counts");

    struct sketch_preview_stub *replacement =
	(struct sketch_preview_stub *)bu_calloc(1, sizeof(struct sketch_preview_stub), "replacement sketch preview stub");
    replacement->revision = 12;
    replacement->pick_id = 84;
    VSET(replacement->bmin, -7.0, -8.0, -9.0);
    VSET(replacement->bmax, 10.0, 11.0, 12.0);
    VSET(replacement->snap_out, 1.25, 1.5, 1.75);

    if (!bsg_payload_sketch_set_live_ops(
	    sketch,
	    replacement,
	    1,
	    sketch_stub_revision,
	    sketch_stub_update,
	    sketch_stub_bounds,
	    sketch_stub_pick,
	    sketch_stub_snap,
	    sketch_stub_free))
	FAIL("replace sketch live ops");
    if (g_sketch_preview_free_calls != 1)
	FAIL("replacing owned sketch live context should free old context");
    if (bsg_payload_sketch_revision(sketch) != 12)
	FAIL("replacement sketch revision");

    if (bsg_payload_sketch_bounds(sketch, NULL, &bmax))
	FAIL("sketch bounds with NULL bmin should fail");
    if (bsg_payload_sketch_bounds(sketch, &bmin, NULL))
	FAIL("sketch bounds with NULL bmax should fail");
    if (replacement->bounds_calls != 0)
	FAIL("invalid sketch bounds output should not call producer");

    if (bsg_payload_sketch_snap(sketch, NULL, NULL, snap_pt))
	FAIL("sketch snap with NULL sample should fail");
    if (bsg_payload_sketch_snap(sketch, NULL, sample_pt, NULL))
	FAIL("sketch snap with NULL output should fail");
    if (replacement->snap_calls != 0)
	FAIL("invalid sketch snap arguments should not call producer");

    bsg_payload_free(sketch);
    if (g_sketch_preview_free_calls != 2)
	FAIL("sketch live free callback");

    PASS("sketch edit-preview contract");
    return 0;
}

static int
test_line_set_builders(void)
{
    printf("=== Test 6: LINE_SET builder helpers ===\n");

    point_t pts2[2] = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    int cmds2[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    struct bsg_payload *pl = bsg_payload_line_set_create(pts2, cmds2, 2);
    if (!pl) FAIL("line_set_create returned NULL");
    if (bsg_payload_line_set_point_count(pl) != 2) FAIL("initial point count");
    if (bsg_payload_line_set_cmd_at(pl, 0) != BSG_GEOMETRY_LINE_MOVE) FAIL("cmd_at(0)");
    if (bsg_payload_line_set_cmd_at(pl, 1) != BSG_GEOMETRY_LINE_DRAW) FAIL("cmd_at(1)");
    if (bsg_payload_line_set_cmd_at(pl, 99) != -1) FAIL("cmd_at out-of-range");

    /* append one more segment */
    point_t extra[1] = {{2.0, 0.0, 0.0}};
    int ecmd[1] = {BSG_GEOMETRY_LINE_DRAW};
    uint64_t rev_before = pl->pl_revision;
    if (!bsg_payload_line_set_append_segments(pl, (const point_t *)extra, ecmd, 1)) FAIL("append_segments");
    if (bsg_payload_line_set_point_count(pl) != 3) FAIL("point count after append");
    if (pl->pl_revision <= rev_before) FAIL("revision not bumped after append");

    /* replace with a single segment */
    point_t r_pts[2] = {{10.0, 0.0, 0.0}, {20.0, 0.0, 0.0}};
    int r_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    if (!bsg_payload_line_set_replace(pl, (const point_t *)r_pts, r_cmds, 2)) FAIL("replace");
    if (bsg_payload_line_set_point_count(pl) != 2) FAIL("point count after replace");

    /* verify bounds are updated */
    point_t bmin = VINIT_ZERO, bmax = VINIT_ZERO;
    if (!pl->pl_bounds || pl->pl_bounds(pl, &bmin, &bmax) != 1) FAIL("line_set pl_bounds after replace");
    if (bmin[0] > 10.0 || bmax[0] < 20.0) FAIL("line_set bounds X after replace");

    /* clear */
    if (!bsg_payload_line_set_clear(pl)) FAIL("clear");
    if (bsg_payload_line_set_point_count(pl) != 0) FAIL("point count after clear");
    if (bsg_payload_line_set_cmd_at(pl, 0) != -1) FAIL("cmd_at on empty");
    /* bounds of empty line set should return 0 */
    if (pl->pl_bounds(pl, &bmin, &bmax) != 0) FAIL("bounds of empty line set");

    bsg_payload_free(pl);
    PASS("LINE_SET builder helpers");
    return 0;
}

static int
test_line_set_render_arrays(void)
{
    printf("=== Test 7: LINE_SET render arrays ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) FAIL("line-set render shape");
    bsg_node_add_child(root, shape);
    bsg_node_set_visible_state(shape, 1);

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[1], 3.0, 4.0, 5.0);
    struct bsg_payload *pl = bsg_payload_line_set_create(pts, cmds, 2);
    if (!pl) FAIL("line-set payload create");
    bsg_node_set_payload(shape, pl);

    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!req || !batch) FAIL("line-set render request");
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (bsg_render_request_collect(req, batch) != 1 ||
	    bsg_render_batch_count(batch) != 1)
	FAIL("line-set render collection");

    const struct bsg_render_item *item = bsg_render_batch_get(batch, 0);
    if (!item || item->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("line-set render item type");
    if (item->geometry.arrays.point_count != 2 ||
	    !item->geometry.arrays.points ||
	    item->geometry.arrays.command_count != 2 ||
	    !item->geometry.arrays.commands ||
	    item->geometry.arrays.commands[1] != BSG_GEOMETRY_LINE_DRAW ||
	    !VNEAR_EQUAL(item->geometry.arrays.points[1], pts[1], SMALL_FASTF))
	FAIL("line-set render item arrays");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("LINE_SET render arrays");
    return 0;
}

static int
test_payload_realization_state(void)
{
    printf("=== Test 8: payload realization state ===\n");

    point_t pts[2] = {{0.0, 0.0, 0.0}, {1.0, 2.0, 0.0}};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    struct bsg_payload *pl = bsg_payload_line_set_create(pts, cmds, 2);
    if (!pl) FAIL("line_set_create");
    if (bsg_payload_default_realization_kind(BSG_PL_CSG) != BSG_REALIZE_ADAPTIVE_WIREFRAME)
	FAIL("default CSG realization kind");
    if (!BU_STR_EQUAL(bsg_payload_realization_kind_name(BSG_REALIZE_LOD_MESH), "lod-mesh"))
	FAIL("realization kind name");

    const struct bsg_payload_realization *state = bsg_payload_realization_state(pl);
    if (!state || state->status != BSG_REALIZE_STATUS_CURRENT)
	FAIL("initial realization state");
    if (state->kind != BSG_REALIZE_WIREFRAME)
	FAIL("line_set realization kind");

    bsg_payload_mark_source_revision(pl, 7, BSG_PAYLOAD_STALE_SOURCE_CHANGED);
    if (!bsg_payload_is_stale(pl))
	FAIL("source revision should make payload stale");
    state = bsg_payload_realization_state(pl);
    if (state->status != BSG_REALIZE_STATUS_STALE ||
	    state->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED)
	FAIL("source stale state not recorded");
    if (!BU_STR_EQUAL(bsg_payload_stale_reason_name(state->stale_reason), "source-changed"))
	FAIL("stale reason name");

    int rc = bsg_payload_realize(pl, NULL);
    if (rc != 0)
	FAIL("static line_set realize should not change geometry");
    state = bsg_payload_realization_state(pl);
    if (state->status != BSG_REALIZE_STATUS_CURRENT)
	FAIL("realize should clear stale state");
    if (state->realized_source_revision != 7)
	FAIL("realized source revision not stamped");

    bsg_payload_mark_stale(pl, BSG_PAYLOAD_STALE_UPDATE_FAILED, "synthetic failure");
    state = bsg_payload_realization_state(pl);
    if (state->status != BSG_REALIZE_STATUS_FAILED)
	FAIL("failed realization status");
    if (!BU_STR_EQUAL(state->failure_reason, "synthetic failure"))
	FAIL("failure reason not recorded");

    bsg_payload_free(pl);
    PASS("payload realization state");
    return 0;
}

static int
test_remaining_lifecycle_hooks(void)
{
    printf("=== Test 9: lifecycle hooks for remaining payload types ===\n");

    /* Verify every payload type has non-NULL pl_bounds/pl_export/pl_backend_prepare
     * (either a real implementation or the _no_* sentinel).  Also exercise the
     * return values so we confirm the sentinels actually run. */

#define CHECK_HOOKS(pl_, label_) do { \
    if (!(pl_)) FAIL(label_ " payload create"); \
    if (!(pl_)->pl_bounds) FAIL(label_ " pl_bounds NULL"); \
    if (!(pl_)->pl_export) FAIL(label_ " pl_export NULL"); \
    if (!(pl_)->pl_backend_prepare) FAIL(label_ " pl_backend_prepare NULL"); \
} while (0)

#define CHECK_SENTINEL_HOOKS(pl_, label_) do { \
    CHECK_HOOKS(pl_, label_); \
    point_t _bmin = VINIT_ZERO, _bmax = VINIT_ZERO; \
    if ((pl_)->pl_bounds((pl_), &_bmin, &_bmax) != 0) FAIL(label_ " pl_bounds sentinel"); \
    struct bu_vls _exp = BU_VLS_INIT_ZERO; \
    if ((pl_)->pl_export((pl_), &_exp) != 0) FAIL(label_ " pl_export sentinel"); \
    bu_vls_free(&_exp); \
    if ((pl_)->pl_backend_prepare((pl_), NULL) != 0) FAIL(label_ " pl_backend_prepare sentinel"); \
    bsg_payload_free(pl_); \
} while (0)

    /* TEXT */
    {
        struct bsg_label *lbl;
        BU_GET(lbl, struct bsg_label);
        memset(lbl, 0, sizeof(*lbl));
        BU_VLS_INIT(&lbl->label);
        bu_vls_sprintf(&lbl->label, "test");
        struct bsg_payload *pl = bsg_payload_text_create(lbl);
        CHECK_SENTINEL_HOOKS(pl, "TEXT");
    }

    /* LINE_SET (real bounds when non-empty, sentinel pl_export) */
    {
        point_t pts[2] = {{0,0,0}, {1,0,0}};
        int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
        struct bsg_payload *pl = bsg_payload_line_set_create(pts, cmds, 2);
        CHECK_HOOKS(pl, "LINE_SET");
        point_t bmin = VINIT_ZERO, bmax = VINIT_ZERO;
        if (pl->pl_bounds(pl, &bmin, &bmax) != 1) FAIL("LINE_SET real pl_bounds");
        if (bmax[0] < 1.0) FAIL("LINE_SET bounds value");
        struct bu_vls exp = BU_VLS_INIT_ZERO;
        if (pl->pl_export(pl, &exp) != 0) FAIL("LINE_SET pl_export sentinel");
        bu_vls_free(&exp);
        if (pl->pl_backend_prepare(pl, NULL) != 0) FAIL("LINE_SET pl_backend_prepare sentinel");
        bsg_payload_free(pl);
    }

    /* POLYGON (real bounds when contours present, sentinel export) */
    {
        struct bsg_view *v = make_view();
        bsg_node *root = bsg_view_scene_root_ensure(v);
        point_t origin = VINIT_ZERO;
        bsg_node *poly_node = bsg_create_polygon(v, BSG_SOURCE_VIEW, BSG_POLYGON_RECTANGLE, &origin);
        struct bsg_payload *pl = bsg_node_get_payload(poly_node);
        CHECK_HOOKS(pl, "POLYGON");
        /* empty polygon: bounds may return 0 */
        struct bu_vls exp = BU_VLS_INIT_ZERO;
        if (pl->pl_export(pl, &exp) != 0) FAIL("POLYGON pl_export sentinel");
        bu_vls_free(&exp);
        if (pl->pl_backend_prepare(pl, NULL) != 0) FAIL("POLYGON pl_backend_prepare sentinel");
        bsg_shape_destroy(poly_node);
        bsg_view_scene_root_detach_from_root(root);
        free_view(v);
    }

    /* MESH */
    {
        struct bsg_payload *pl = bsg_payload_mesh_create(NULL);
        CHECK_SENTINEL_HOOKS(pl, "MESH");
    }

    /* CSG */
    {
        struct bsg_payload *pl = bsg_payload_csg_create(NULL);
        CHECK_SENTINEL_HOOKS(pl, "CSG");
    }

    /* BREP */
    {
        struct bsg_payload *pl = bsg_payload_brep_create(NULL);
        CHECK_SENTINEL_HOOKS(pl, "BREP");
    }

    /* FRAMEBUFFER */
    {
        struct bsg_payload *pl = bsg_payload_framebuffer_create(NULL, 0);
        CHECK_SENTINEL_HOOKS(pl, "FRAMEBUFFER");
    }

    /* AXES */
    {
        struct bsg_axes *axes;
        BU_GET(axes, struct bsg_axes);
        memset(axes, 0, sizeof(*axes));
        struct bsg_payload *pl = bsg_payload_axes_create(axes);
        CHECK_SENTINEL_HOOKS(pl, "AXES");
    }

    /* GRID */
    {
        struct bsg_grid_state gs;
        memset(&gs, 0, sizeof(gs));
        gs.draw = 1;
        struct bsg_payload *pl = bsg_payload_grid_create(&gs);
        CHECK_SENTINEL_HOOKS(pl, "GRID");
    }

    /* HUD_TEXT */
    {
        struct bsg_label *lbl;
        BU_GET(lbl, struct bsg_label);
        memset(lbl, 0, sizeof(*lbl));
        BU_VLS_INIT(&lbl->label);
        bu_vls_sprintf(&lbl->label, "hud test");
        struct bsg_payload *pl = bsg_payload_hud_text_create(lbl);
        CHECK_SENTINEL_HOOKS(pl, "HUD_TEXT");
    }

    /* ANNOTATION */
    {
        point_t ann_pts[2] = {{0,0,0}, {1,1,0}};
        struct bsg_payload *pl = bsg_payload_annotation_create("check", ann_pts, 2);
        CHECK_HOOKS(pl, "ANNOTATION");
        point_t bmin = VINIT_ZERO, bmax = VINIT_ZERO;
        if (pl->pl_bounds(pl, &bmin, &bmax) != 1) FAIL("ANNOTATION real pl_bounds");
        if (bmax[0] < 1.0 || bmax[1] < 1.0) FAIL("ANNOTATION bounds value");
        struct bu_vls exp = BU_VLS_INIT_ZERO;
        if (pl->pl_export(pl, &exp) != 0) FAIL("ANNOTATION pl_export sentinel");
        bu_vls_free(&exp);
        if (pl->pl_backend_prepare(pl, NULL) != 0) FAIL("ANNOTATION pl_backend_prepare sentinel");
        bsg_payload_free(pl);

        mat_t model_mat, display_mat;
        MAT_IDN(model_mat);
        MAT_IDN(display_mat);
        MAT_DELTAS(model_mat, 10.0, 20.0, 30.0);
        point_t model_pts[2] = {{-1.0, -2.0, 0.0}, {3.0, 4.0, 0.0}};
        point_t anchor = VINIT_ZERO;
        pl = bsg_payload_annotation_create_record("model bounds",
		BSG_ANNOTATION_SPACE_MODEL, anchor, model_mat, display_mat,
		(const point_t *)model_pts, 2, NULL, 0);
        if (!pl || pl->pl_bounds(pl, &bmin, &bmax) != 1)
	    FAIL("ANNOTATION model-space bounds");
        point_t expected_min = {9.0, 18.0, 30.0};
        point_t expected_max = {13.0, 24.0, 30.0};
        if (!VNEAR_EQUAL(bmin, expected_min, SMALL_FASTF) ||
		!VNEAR_EQUAL(bmax, expected_max, SMALL_FASTF))
	    FAIL("ANNOTATION model-space bounds values");
        bsg_payload_free(pl);

        MAT_IDN(model_mat);
        MAT_IDN(display_mat);
        MAT_DELTAS(display_mat, 5.0, 6.0, 7.0);
        point_t display_pts[2] = {{0.0, 0.0, 0.0}, {2.0, 3.0, 0.0}};
        VSET(anchor, 1.0, 2.0, 3.0);
        pl = bsg_payload_annotation_create_record("display bounds",
		BSG_ANNOTATION_SPACE_DISPLAY, anchor, model_mat, display_mat,
		(const point_t *)display_pts, 2, NULL, 0);
        if (!pl || pl->pl_bounds(pl, &bmin, &bmax) != 1)
	    FAIL("ANNOTATION display-space bounds");
        VSET(expected_min, 1.0, 2.0, 3.0);
        VSET(expected_max, 8.0, 11.0, 10.0);
        if (!VNEAR_EQUAL(bmin, expected_min, SMALL_FASTF) ||
		!VNEAR_EQUAL(bmax, expected_max, SMALL_FASTF))
	    FAIL("ANNOTATION display-space bounds values");
        bsg_payload_free(pl);
    }

#undef CHECK_HOOKS
#undef CHECK_SENTINEL_HOOKS

    PASS("lifecycle hooks for remaining payload types");
    return 0;
}

static int
test_sketch_generated_geometry_payload(void)
{
    printf("=== Test 10: sketch generated geometry payload ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) FAIL("bsg_shape_create returned NULL");
    bsg_node_add_child(root, shape);
    bsg_node_set_visible_state(shape, 1);

    struct bsg_payload *sketch = bsg_payload_sketch_create(NULL, NULL);
    if (!sketch) FAIL("sketch payload create");
    bsg_node_set_payload(shape, sketch);

    point_t pts[2] = { VINIT_ZERO, VINIT_ZERO };
    int cmds[2] = { BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW };
    VSET(pts[0], -2.0, 0.0, 0.0);
    VSET(pts[1], 2.0, 0.0, 0.0);

    struct bsg_payload *geometry = bsg_payload_sketch_geometry(sketch);
    if (!geometry || geometry->pl_type != BSG_PL_LINE_SET)
	FAIL("sketch geometry payload");
    if (!bsg_payload_line_set_replace(geometry, (const point_t *)pts, cmds, 2))
	FAIL("sketch geometry replace");
    bsg_payload_bump_revision(sketch);
    struct bsg_payload_line_set *ls = bsg_payload_line_set_get(geometry);
    if (!ls || ls->point_cnt != 2)
	FAIL("sketch geometry command count");
    if (ls->cmds[0] != BSG_GEOMETRY_LINE_MOVE ||
	    ls->cmds[1] != BSG_GEOMETRY_LINE_DRAW)
	FAIL("sketch geometry command normalization");
    if (!sketch->pl_revision || !geometry->pl_revision)
	FAIL("sketch geometry revision");

    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!req || !batch)
	FAIL("sketch render request");
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (bsg_render_request_collect(req, batch) != 1 ||
	    bsg_render_batch_count(batch) != 1)
	FAIL("sketch render collection");
    const struct bsg_render_item *item = bsg_render_batch_get(batch, 0);
    if (!item || item->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET)
	FAIL("sketch render line-set routing");
    if (item->geometry.arrays.point_count != 2 ||
	    !item->geometry.arrays.points ||
	    item->geometry.arrays.command_count != 2 ||
	    !item->geometry.arrays.commands ||
	    item->geometry.arrays.commands[0] != BSG_GEOMETRY_LINE_MOVE ||
	    item->geometry.arrays.commands[1] != BSG_GEOMETRY_LINE_DRAW ||
	    !VNEAR_EQUAL(item->geometry.arrays.points[1], pts[1], SMALL_FASTF))
	FAIL("sketch render line-set arrays");
    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);

    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);

    PASS("sketch generated geometry payload");
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	fprintf(stderr, "Unexpected arguments\n");

    int ret = 0;
    ret |= test_polygon_payload();
    ret |= test_remaining_payload_builders();
    ret |= test_lifecycle_hooks();
    ret |= test_sketch_live_contract();
    ret |= test_line_set_builders();
    ret |= test_line_set_render_arrays();
    ret |= test_remaining_lifecycle_hooks();
    ret |= test_sketch_generated_geometry_payload();
    ret |= test_payload_realization_state();

    return ret;
}
