/*                    T E S T _ F E A T U R E . C
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
/** @file libbsg/tests/test_feature.c
 *
 * Typed feature lifecycle regression tests.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bsg/appearance.h"
#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/material.h"
#include "bsg/polygon.h"
#include "bsg/render.h"
#include "../polygon_private.h"
#include "bsg/scene_object.h"
#include "bsg/tcl_data.h"
#include "bsg/util.h"
#include "../appearance_private.h"
#include "../field_private.h"
#include "../geometry_private.h"
#include "../object_private.h"
#include "../payload_private.h"
#include "../scene_object_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "feature_view");
    (void)bsg_view_scene_root_ensure(v);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "feature view");
}

struct feature_preview_stub {
    uint64_t revision;
    int bounds_calls;
    int snap_calls;
    point_t bmin;
    point_t bmax;
    point_t snap_out;
};

static uint64_t
feature_preview_revision(void *ctx)
{
    struct feature_preview_stub *stub = (struct feature_preview_stub *)ctx;
    return stub ? stub->revision : 0;
}

static int
feature_preview_bounds(void *ctx, point_t *bmin, point_t *bmax)
{
    struct feature_preview_stub *stub = (struct feature_preview_stub *)ctx;
    if (!stub)
	return 0;
    stub->bounds_calls++;
    VMOVE(*bmin, stub->bmin);
    VMOVE(*bmax, stub->bmax);
    return 1;
}

static int
feature_preview_snap(void *ctx, struct bsg_view *UNUSED(v),
	const point_t UNUSED(sample_pt), point_t out_pt)
{
    struct feature_preview_stub *stub = (struct feature_preview_stub *)ctx;
    if (!stub)
	return 0;
    stub->snap_calls++;
    VMOVE(out_pt, stub->snap_out);
    return 1;
}

static int
test_feature_create_record_payload(void)
{
    printf("=== Test 1: feature create / record / payload ===\n");

    struct bsg_view *v = make_view();
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_LINES;
    opts.local = 1;
    opts.visible = 1;
    opts.color_valid = 1;
    opts.color[0] = 10;
    opts.color[1] = 20;
    opts.color[2] = 30;
    opts.line_width = 3;
    opts.display_name = "Line Feature";
    opts.owner = "test";

    bsg_feature_ref ref = bsg_feature_create(v, "line_feature", &opts);
    if (bsg_feature_ref_is_null(ref))
	FAIL("feature_create returned null ref");
    bsg_feature_ref found = bsg_feature_find(v, "line_feature");
    if (bsg_feature_ref_is_null(found) || found.token != ref.token)
	FAIL("feature_find did not return created feature");
    point_t points[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    point_t p1 = VINIT_ZERO;
    VSET(p1, 1.0, 0.0, 0.0);
    VMOVE(points[1], p1);
    if (!bsg_feature_points_replace(ref, BSG_FEATURE_LINES, (const point_t *)points, cmds, 2))
	FAIL("feature point replacement failed");

    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record))
	FAIL("feature_record_get failed");
    if (record.family != BSG_FEATURE_LINES)
	FAIL("wrong feature family");
    if (record.scope != BSG_FEATURE_SCOPE_LOCAL)
	FAIL("wrong feature scope");
    if (!record.display_name || !BU_STR_EQUAL(record.display_name, "Line Feature"))
	FAIL("display name not recorded");
    if (!record.owner || !BU_STR_EQUAL(record.owner, "test"))
	FAIL("owner not recorded");
    if (record.line_width != 3)
	FAIL("line width not recorded");
    if (record.geometry_command_count != 2)
	FAIL("geometry command count not recorded");

    if (!bsg_feature_remove(v, "line_feature"))
	FAIL("feature_remove failed");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "line_feature")))
	FAIL("feature still findable after remove");

    free_view(v);
    PASS("feature create / record / payload");
    return 0;
}

static int
test_feature_scopes_and_typed_defaults(void)
{
    printf("=== Test 2: feature scopes and typed defaults ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref axes = bsg_feature_create_axes(v, "axes_feature", 0);
    bsg_feature_ref local = bsg_feature_create_arrow(v, "arrow_feature", 1);
    if (bsg_feature_ref_is_null(axes) || bsg_feature_ref_is_null(local))
	FAIL("feature creation failed");
    struct bsg_feature_record local_record;
    if (!bsg_feature_record_get(local, &local_record) ||
	    local_record.family != BSG_FEATURE_ARROW ||
	    local_record.geometry_command_count != 0)
	FAIL("arrow feature default record invalid");
    struct bsg_axes axes_state;
    struct bsg_axes axes_check;
    if (!bsg_feature_axes_state_get(axes, &axes_check))
	FAIL("axes feature missing typed axes state");
    memset(&axes_state, 0, sizeof(axes_state));
    VSET(axes_state.axes_pos, 1.0, 2.0, 3.0);
    axes_state.axes_size = 4.0;
    axes_state.line_width = 2;
    VSET(axes_state.axes_color, 100, 110, 120);
    if (!bsg_feature_axes_state_replace(axes, &axes_state))
	FAIL("axes state replacement failed");
    memset(&axes_check, 0, sizeof(axes_check));
    if (!bsg_feature_axes_state_get(axes, &axes_check))
	FAIL("axes state copy failed");
    if (!NEAR_EQUAL(axes_check.axes_pos[X], 1.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(axes_check.axes_pos[Y], 2.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(axes_check.axes_pos[Z], 3.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(axes_check.axes_size, 4.0, SMALL_FASTF) ||
	    axes_check.line_width != 2 ||
	    axes_check.axes_color[0] != 100 ||
	    axes_check.axes_color[1] != 110 ||
	    axes_check.axes_color[2] != 120)
	FAIL("axes state round trip mismatch");

    struct bsg_feature_record axes_record;
    if (!bsg_feature_record_get(axes, &axes_record) ||
	    axes_record.family != BSG_FEATURE_AXES ||
	    axes_record.scope != BSG_FEATURE_SCOPE_SHARED)
	FAIL("axes feature record mismatch");
    if (!bsg_feature_record_get(local, &local_record) ||
	    local_record.family != BSG_FEATURE_ARROW ||
	    local_record.scope != BSG_FEATURE_SCOPE_LOCAL)
	FAIL("arrow feature record mismatch");

    if (bsg_feature_remove_all(v, BSG_FEATURE_SCOPE_LOCAL) != 1)
	FAIL("remove local scope count");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "arrow_feature")))
	FAIL("local feature still present");
    if (bsg_feature_ref_is_null(bsg_feature_find(v, "axes_feature")))
	FAIL("shared feature was removed by local remove");

    if (bsg_feature_remove_all(v, BSG_FEATURE_SCOPE_SHARED) != 1)
	FAIL("remove shared scope count");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "axes_feature")))
	FAIL("shared feature still present");

    free_view(v);
    PASS("feature scopes and typed defaults");
    return 0;
}

static int
test_feature_geometry_update(void)
{
    printf("=== Test 3: feature typed geometry replacement by ref ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref ref = bsg_feature_create_lines(v, "update_feature", 1);
    if (!ref.token)
	FAIL("feature_create_lines returned null ref");

    point_t points[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(points[1], 1.0, 0.0, 0.0);
    if (!bsg_feature_points_replace(ref, BSG_FEATURE_MEASUREMENT,
		(const point_t *)points, cmds, 2))
	FAIL("feature typed geometry replacement failed");

    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record))
	FAIL("record get after geometry update failed");
    if (record.family != BSG_FEATURE_MEASUREMENT)
	FAIL("geometry update did not apply family");
    if (record.geometry_command_count != 2)
	FAIL("geometry replacement did not copy commands");
    struct bsg_node *node = (struct bsg_node *)ref.token;
    if (bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("measurement update did not use line-set geometry kind");
    if (!(bsg_node_get_payload_type(node) & BSG_PAYLOAD_OVERLAY))
	FAIL("feature line geometry should be marked as overlay payload");
    if (!bsg_shape_is_non_database_source(node))
	FAIL("feature line geometry should be marked as non-database");
    bsg_node_ref node_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(node));
    bsg_line_set_ref lines = bsg_node_ref_as_line_set(node_ref);
    int line_cmd = -1;
    if (!bsg_line_set_ref_command_at(lines, 0, &line_cmd) ||
	    line_cmd != BSG_GEOMETRY_LINE_MOVE)
	FAIL("measurement replacement did not preserve move command");
    if (!bsg_line_set_ref_command_at(lines, 1, &line_cmd) ||
	    line_cmd != BSG_GEOMETRY_LINE_DRAW)
	FAIL("measurement replacement did not preserve draw command");

    if (!bsg_feature_points_replace(ref, BSG_FEATURE_UNKNOWN, NULL, NULL, 0))
	FAIL("feature geometry clear failed");
    if (!bsg_feature_record_get(ref, &record))
	FAIL("record get after geometry clear failed");
    if (record.geometry_command_count != 0)
	FAIL("geometry clear did not reset command count");

    free_view(v);
    PASS("feature typed geometry replacement by ref");
    return 0;
}

static int
test_feature_line_update_uses_geometry_fields(void)
{
    printf("=== Test 4: line-like feature update uses geometry fields ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref ref = bsg_feature_create_preview(v, "preview_feature", 1);
    if (!ref.token)
	FAIL("feature_create_preview returned null ref");

    point_t preview_points[2] = {VINIT_ZERO, VINIT_ZERO};
    int preview_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(preview_points[1], 2.0, 0.0, 0.0);
    if (!bsg_feature_points_replace(ref, BSG_FEATURE_TRANSIENT_PREVIEW,
		(const point_t *)preview_points, preview_cmds, 2))
	FAIL("line-like feature geometry replacement failed");

    struct bsg_node *node = (struct bsg_node *)ref.token;
    if (bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("line-like update did not keep line-set geometry kind");
    if (!bsg_geometry_node_clear_private_realization(node))
	FAIL("line-like feature private realization clear failed");

    struct feature_preview_stub preview_stub;
    memset(&preview_stub, 0, sizeof(preview_stub));
    preview_stub.revision = 17;
    VSET(preview_stub.bmin, -1.0, -2.0, -3.0);
    VSET(preview_stub.bmax, 4.0, 5.0, 6.0);
    VSET(preview_stub.snap_out, 0.25, 0.5, 0.75);
    struct bsg_edit_preview_ops ops = BSG_EDIT_PREVIEW_OPS_INIT;
    ops.preview_ctx = &preview_stub;
    ops.revision_cb = feature_preview_revision;
    ops.bounds_cb = feature_preview_bounds;
    ops.snap_cb = feature_preview_snap;
    if (!bsg_feature_edit_preview_attach(ref, NULL, NULL, &ops))
	FAIL("edit-preview payload attach failed");
    if (bsg_feature_edit_preview_revision(ref) != preview_stub.revision)
	FAIL("edit-preview revision callback not attached");
    point_t preview_bmin = VINIT_ZERO;
    point_t preview_bmax = VINIT_ZERO;
    if (bsg_feature_edit_preview_bounds(ref, NULL, &preview_bmax) != 0 ||
	    bsg_feature_edit_preview_bounds(ref, &preview_bmin, NULL) != 0 ||
	    preview_stub.bounds_calls != 0)
	FAIL("edit-preview invalid feature bounds should not dispatch");
    if (!bsg_feature_edit_preview_bounds(ref, &preview_bmin, &preview_bmax) ||
	    preview_stub.bounds_calls != 1 ||
	    !VNEAR_EQUAL(preview_bmin, preview_stub.bmin, SMALL_FASTF) ||
	    !VNEAR_EQUAL(preview_bmax, preview_stub.bmax, SMALL_FASTF))
	FAIL("edit-preview feature bounds callback not attached");
    point_t sample = VINIT_ZERO;
    point_t snapped = VINIT_ZERO;
    if (bsg_feature_edit_preview_snap(ref, NULL, NULL, snapped) != 0 ||
	    bsg_feature_edit_preview_snap(ref, NULL, sample, NULL) != 0 ||
	    preview_stub.snap_calls != 0)
	FAIL("edit-preview invalid feature snap should not dispatch");
    if (!bsg_feature_edit_preview_snap(ref, NULL, sample, snapped) ||
	    preview_stub.snap_calls != 1 ||
	    !VNEAR_EQUAL(snapped, preview_stub.snap_out, SMALL_FASTF))
	FAIL("edit-preview feature snap callback not attached");
    struct bsg_feature_record ops_before;
    if (!bsg_feature_record_get(ref, &ops_before))
	FAIL("edit-preview set-ops preflight record failed");
    if (!bsg_feature_edit_preview_set_ops(ref, &ops))
	FAIL("edit-preview set_ops failed");
    struct bsg_feature_record ops_after;
    if (!bsg_feature_record_get(ref, &ops_after) ||
	    ops_after.ref.revision <= ops_before.ref.revision)
	FAIL("edit-preview set_ops should touch feature record");
    if (!bsg_feature_points_replace(ref, BSG_FEATURE_TRANSIENT_PREVIEW,
		(const point_t *)preview_points, preview_cmds, 2))
	FAIL("edit-preview line geometry replacement failed");
    if (bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("edit-preview line update should publish line-set geometry");
    if (bsg_feature_edit_preview_revision(ref) != preview_stub.revision)
	FAIL("edit-preview payload was lost during line geometry update");
    if (!bsg_feature_edit_preview_touch(ref))
	FAIL("edit-preview touch should still reach the payload");
    struct bsg_feature_record stale_before;
    if (!bsg_feature_record_get(ref, &stale_before))
	FAIL("edit-preview stale-mark preflight record failed");
    bsg_feature_edit_preview_mark_source_revision(ref, 91,
	    BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED);
    if (!bsg_feature_edit_preview_is_stale(ref))
	FAIL("edit-preview source revision should mark payload stale");
    struct bsg_feature_record stale_after;
    if (!bsg_feature_record_get(ref, &stale_after) ||
	    stale_after.ref.revision <= stale_before.ref.revision)
	FAIL("edit-preview source revision should touch feature record");
    if (!bsg_geometry_node_clear_private_realization(node))
	FAIL("line-like feature edit-preview realization clear failed");

    point_t *points = NULL;
    int *cmds = NULL;
    size_t point_count = 0;
    if (!bsg_feature_points_copy(ref, &points, &cmds, &point_count))
	FAIL("line-like feature points copy failed");
    if (point_count != 2 || !points || !cmds ||
	    !NEAR_EQUAL(points[1][X], 2.0, SMALL_FASTF) ||
	    cmds[1] != BSG_GEOMETRY_LINE_DRAW)
	FAIL("line-like feature geometry fields not readable");

    if (points)
	bu_free(points, "bsg feature points copy");
    if (cmds)
	bu_free(cmds, "bsg feature cmds copy");

    if (!bsg_feature_points_replace(ref, BSG_FEATURE_UNKNOWN, NULL, NULL, 0))
	FAIL("line-like feature geometry clear failed");
    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record) || record.geometry_command_count != 0)
	FAIL("line-like feature clear did not reset command count");

    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_SKETCH;
    bsg_feature_ref sketch = bsg_feature_create(v, "sketch_feature", &opts);
    if (!sketch.token)
	FAIL("sketch feature create failed");
    point_t sketch_points[2];
    int sketch_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(sketch_points[0], 0.0, 0.0, 0.0);
    VSET(sketch_points[1], 3.0, 0.0, 0.0);
    if (!bsg_feature_points_replace(sketch, BSG_FEATURE_SKETCH, (const point_t *)sketch_points, sketch_cmds, 2))
	FAIL("sketch feature points replace failed");
    struct bsg_node *sketch_node = (struct bsg_node *)sketch.token;
    if (bsg_geometry_node_kind_get(sketch_node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("sketch feature did not use line-set geometry");
    if (!bsg_geometry_node_clear_private_realization(sketch_node))
	FAIL("sketch feature private realization clear failed");
    points = NULL;
    cmds = NULL;
    point_count = 0;
    if (!bsg_feature_points_copy(sketch, &points, &cmds, &point_count) ||
	    point_count != 2 || !points || !cmds ||
	    !NEAR_EQUAL(points[1][X], 3.0, SMALL_FASTF) ||
	    cmds[1] != BSG_GEOMETRY_LINE_DRAW)
	FAIL("sketch feature geometry fields not readable");
    if (points)
	bu_free(points, "bsg sketch feature points copy");
    if (cmds)
	bu_free(cmds, "bsg sketch feature cmds copy");

    opts.family = BSG_FEATURE_POLYGON;
    bsg_feature_ref polygon = bsg_feature_create(v, "polygon_feature", &opts);
    if (!polygon.token)
	FAIL("polygon feature create failed");
    point_t polygon_points[2];
    int polygon_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(polygon_points[0], 0.0, 0.0, 0.0);
    VSET(polygon_points[1], 4.0, 0.0, 0.0);
    if (!bsg_feature_points_replace(polygon, BSG_FEATURE_POLYGON, (const point_t *)polygon_points, polygon_cmds, 2))
	FAIL("polygon feature points replace failed");
    struct bsg_node *polygon_node = (struct bsg_node *)polygon.token;
    if (bsg_geometry_node_kind_get(polygon_node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("polygon feature did not use line-set geometry");

    free_view(v);
    PASS("line-like feature update uses geometry fields");
    return 0;
}

static int
test_polygon_realization_uses_geometry_fields(void)
{
    printf("=== Test 5: polygon realization uses geometry fields ===\n");

    struct bsg_view *v = make_view();
    struct bsg_polygon *poly;
    BU_GET(poly, struct bsg_polygon);
    memset(poly, 0, sizeof(*poly));
    poly->type = BSG_POLYGON_GENERAL;
    poly->curr_contour_i = -1;
    poly->curr_point_i = -1;
    poly->polygon.num_contours = 1;
    poly->polygon.hole = (int *)bu_calloc(1, sizeof(int), "feature polygon hole");
    poly->polygon.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "feature polygon contour");
    poly->polygon.contour[0].num_points = 3;
    poly->polygon.contour[0].open = 0;
    poly->polygon.contour[0].point = (point_t *)bu_calloc(3, sizeof(point_t), "feature polygon points");
    VSET(poly->polygon.contour[0].point[0], 0.0, 0.0, 0.0);
    VSET(poly->polygon.contour[0].point[1], 1.0, 0.0, 0.0);
    VSET(poly->polygon.contour[0].point[2], 0.0, 1.0, 0.0);

    bsg_node *node = bsg_create_polygon_obj(v, BSG_SOURCE_VIEW, poly);
    if (!node)
	FAIL("polygon create");
    if (!bsg_node_polygon(node))
	FAIL("polygon semantic payload missing");
    if (bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("polygon realization is not field-backed line-set geometry");

    if (bsg_appearance_line_width(node) != 1)
	FAIL("polygon line width should remain field-backed");
    bsg_polygon_ref pref = { (uintptr_t)node, 0 };
    struct bsg_polygon_record record;
    if (!bsg_polygon_record_get(pref, &record))
	FAIL("polygon record get failed");
    if (record.edge_color[0] != 255 || record.edge_color[1] != 255 || record.edge_color[2] != 0)
	FAIL("polygon record color should remain field-backed");

    struct bu_ptbl polygon_nodes;
    bu_ptbl_init(&polygon_nodes, 1, "polygon role test nodes");
    bu_ptbl_ins(&polygon_nodes, (long *)node);
    point_t select_pt = VINIT_ZERO;
    VSET(select_pt, 0.5, 0.0, 0.0);
    if (bsg_select_polygon(&polygon_nodes, &select_pt) != node)
	FAIL("polygon role should remain metadata-backed");
    bu_ptbl_free(&polygon_nodes);

    bsg_geometry_ref geometry = BSG_GEOMETRY_REF_NULL_INIT;
    geometry.shape.node = bsg_node_ref_from_object(bsg_object_ref_from_node(node));
    size_t coord_count = bsg_field_multi_count(bsg_geometry_ref_coordinates_field(geometry));
    size_t command_count = bsg_field_multi_count(bsg_geometry_ref_primitive_sets_field(geometry));
    if (coord_count != 5) {
	printf("  polygon coordinate field count: %zu\n", coord_count);
	FAIL("polygon coordinate field count");
    }
    if (command_count != 5) {
	printf("  polygon command field count: %zu\n", command_count);
	FAIL("polygon command field count");
    }
    int cmd = -1;
    if (!bsg_field_multi_int_at(bsg_geometry_ref_primitive_sets_field(geometry), 0, &cmd) ||
	    cmd != BSG_GEOMETRY_LINE_MOVE)
	FAIL("polygon first command");
    if (!bsg_field_multi_int_at(bsg_geometry_ref_primitive_sets_field(geometry), 4, &cmd) ||
	    cmd != BSG_GEOMETRY_LINE_DRAW)
	FAIL("polygon close command");

    bsg_scene_node_release(node);
    free_view(v);
    PASS("polygon realization uses geometry fields");
    return 0;
}

static int
test_feature_label_sync(void)
{
    printf("=== Test 6: feature label sync ===\n");

    struct bsg_view *v = make_view();
    struct bsg_data_label_state labels;
    memset(&labels, 0, sizeof(labels));
    labels.gdls_draw = 1;
    labels.gdls_num_labels = 1;
    labels.gdls_labels = (char **)bu_calloc(1, sizeof(char *), "feature test labels");
    labels.gdls_points = (point_t *)bu_calloc(1, sizeof(point_t), "feature test label points");
    labels.gdls_labels[0] = bu_strdup("origin");
    VSET(labels.gdls_points[0], 0.0, 0.0, 0.0);
    VSET(labels.gdls_color, 255, 255, 0);

    bsg_feature_labels_sync(v, &labels, "labels_feature");
    bsg_feature_ref ref = bsg_feature_find(v, "labels_feature");
    if (bsg_feature_ref_is_null(ref))
	FAIL("label feature parent missing");
    if (bsg_feature_label_count(ref) != 1)
	FAIL("label feature count");
    struct bu_vls text = BU_VLS_INIT_ZERO;
    point_t point = VINIT_ZERO;
    unsigned char rgb[3] = {0, 0, 0};
    if (!bsg_feature_label_copy(ref, 0, &text, point, rgb))
	FAIL("label copy failed");
    if (!BU_STR_EQUAL(bu_vls_cstr(&text), "origin"))
	FAIL("label text mismatch");
    if (!VNEAR_EQUAL(point, labels.gdls_points[0], VUNITIZE_TOL))
	FAIL("label point mismatch");
    if (rgb[0] != 255 || rgb[1] != 255 || rgb[2] != 0)
	FAIL("label color mismatch");
    bu_vls_free(&text);

    labels.gdls_draw = 0;
    bsg_feature_labels_sync(v, &labels, "labels_feature");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "labels_feature")))
	FAIL("label feature not removed when draw disabled");

    bu_free(labels.gdls_labels[0], "feature test label");
    bu_free(labels.gdls_labels, "feature test labels");
    bu_free(labels.gdls_points, "feature test label points");
    free_view(v);
    PASS("feature label sync");
    return 0;
}

static int
test_feature_fields_ignore_raw_mirrors(void)
{
    printf("=== Test 7: feature field state ignores raw mirrors ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref ref = bsg_feature_create_lines(v, "canonical_feature", 0);
    if (bsg_feature_ref_is_null(ref))
	FAIL("feature create failed");

    struct bsg_node *node = (struct bsg_node *)ref.token;
    if (!node)
	FAIL("feature node missing");
    if (!bsg_node_name(node) || !BU_STR_EQUAL(bsg_node_name(node), "canonical_feature"))
	FAIL("feature name should remain field-backed");
    if (!bsg_node_visible(node))
	FAIL("feature visibility should remain field-backed");
    struct bsg_node *scope = bsg_node_parent(node);
    if (!scope)
	FAIL("feature scope missing");
    if (bsg_feature_find(v, "canonical_feature").token != ref.token)
	FAIL("feature find should use field-backed name and scope type");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "raw_feature")))
	FAIL("feature find should ignore unknown raw-era name");

    struct bsg_feature_label_data label;
    memset(&label, 0, sizeof(label));
    label.text = "field label";
    VSET(label.point, 1.0, 2.0, 3.0);
    label.color_valid = 1;
    VSET(label.color, 9, 8, 7);
    if (!bsg_feature_labels_replace(ref, &label, 1))
	FAIL("feature labels replace failed");
    struct bsg_node *child = bsg_node_child_at(node, 0);
    if (!child)
	FAIL("label child missing");
    if (!bsg_node_visible(child))
	FAIL("label child visibility should remain field-backed");

    unsigned char rgb[3] = {0, 0, 0};
    if (!bsg_feature_label_copy(ref, 0, NULL, NULL, rgb))
	FAIL("label copy failed");
    if (rgb[0] != 9 || rgb[1] != 8 || rgb[2] != 7)
	FAIL("label child color should remain field-backed");

    unsigned char new_color[3] = {4, 5, 6};
    if (!bsg_feature_labels_color_set(ref, new_color))
	FAIL("feature label color set failed");
    if (!bsg_feature_label_copy(ref, 0, NULL, NULL, rgb))
	FAIL("label copy after color set failed");
    if (rgb[0] != 4 || rgb[1] != 5 || rgb[2] != 6)
	FAIL("label color set should remain field-backed");

    if (!bsg_feature_remove(v, "canonical_feature"))
	FAIL("feature erase should use field-backed name");
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "canonical_feature")))
	FAIL("feature should be erased");

    free_view(v);
    PASS("feature field state ignores raw mirrors");
    return 0;
}

static int
test_feature_line_layers_replace(void)
{
    printf("=== Test 8: feature line layer replacement ===\n");

    struct bsg_view *v = make_view();
    point_t red_points[2] = {VINIT_ZERO, VINIT_ZERO};
    point_t blue_points[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    int red_cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    int blue_cmds[3] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW, BSG_GEOMETRY_POINT_DRAW};
    VSET(red_points[1], 1.0, 0.0, 0.0);
    VSET(blue_points[1], 0.0, 1.0, 0.0);
    VSET(blue_points[2], 0.0, 1.0, 1.0);

    struct bsg_feature_line_layer layers[2];
    struct bsg_feature_line_layer init = BSG_FEATURE_LINE_LAYER_INIT;
    layers[0] = init;
    layers[0].name = "line_layers_red";
    layers[0].points = (const point_t *)red_points;
    layers[0].commands = red_cmds;
    layers[0].point_count = 2;
    layers[0].style.color_valid = 1;
    layers[0].style.color[0] = 255;

    layers[1] = init;
    layers[1].name = "line_layers_blue";
    layers[1].points = (const point_t *)blue_points;
    layers[1].commands = blue_cmds;
    layers[1].point_count = 3;
    layers[1].style.color_valid = 1;
    layers[1].style.color[2] = 255;

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    style.line_width = 5;
    bsg_feature_ref ref = bsg_feature_replace_line_layers(v, "line_layers", 0, layers, 2, &style);
    if (bsg_feature_ref_is_null(ref))
	FAIL("line layer replace returned null ref");

    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record))
	FAIL("line layer feature record failed");
    if (record.family != BSG_FEATURE_OVERLAY)
	FAIL("line layer parent should be an overlay feature");
    if (record.child_count != 2)
	FAIL("line layer parent should have two children");
    if (record.geometry_command_count != 5)
	FAIL("line layer parent should report aggregate command count");

    struct bsg_node *node = (struct bsg_node *)ref.token;
    struct bsg_node *red_child = bsg_node_child_at(node, 0);
    struct bsg_node *blue_child = bsg_node_child_at(node, 1);
    if (!red_child || !blue_child)
	FAIL("line layer children missing");
    if (!(bsg_node_get_payload_type(node) & BSG_PAYLOAD_OVERLAY))
	FAIL("line layer parent should be marked as overlay payload");
    if (!bsg_shape_is_non_database_source(node))
	FAIL("line layer parent should be marked as non-database");
    if (bsg_geometry_node_kind_get(red_child) != BSG_GEOMETRY_NODE_LINE_SET ||
	    bsg_geometry_node_kind_get(blue_child) != BSG_GEOMETRY_NODE_LINE_SET)
	FAIL("line layer children should be line-set geometry");
    if (!(bsg_node_get_payload_type(red_child) & BSG_PAYLOAD_OVERLAY) ||
	    !(bsg_node_get_payload_type(blue_child) & BSG_PAYLOAD_OVERLAY))
	FAIL("line layer children should be marked as overlay payload");
    if (!bsg_shape_is_non_database_source(red_child) ||
	    !bsg_shape_is_non_database_source(blue_child))
	FAIL("line layer children should be marked as non-database");
    if (bsg_appearance_line_width(red_child) != 5 ||
	    bsg_appearance_line_width(blue_child) != 5)
	FAIL("collection style should cascade to children");

    unsigned char rgb[3] = {0, 0, 0};
    bsg_node_get_color(red_child, &rgb[0], &rgb[1], &rgb[2]);
    if (rgb[0] != 255 || rgb[1] != 0 || rgb[2] != 0)
	FAIL("first line layer color not applied");
    bsg_node_get_color(blue_child, &rgb[0], &rgb[1], &rgb[2]);
    if (rgb[0] != 0 || rgb[1] != 0 || rgb[2] != 255)
	FAIL("second line layer color not applied");

    ref = bsg_feature_replace_line_layers(v, "line_layers", 0, layers, 1, NULL);
    if (bsg_feature_ref_is_null(ref))
	FAIL("line layer replacement returned null ref");
    if (!bsg_feature_record_get(ref, &record))
	FAIL("line layer replacement record failed");
    if (record.child_count != 1 || record.geometry_command_count != 2)
	FAIL("line layer replacement should remove stale children");

    (void)bsg_feature_replace_line_layers(v, "line_layers", 0, NULL, 0, NULL);
    if (!bsg_feature_ref_is_null(bsg_feature_find(v, "line_layers")))
	FAIL("empty line layer replacement should remove feature");

    free_view(v);
    PASS("feature line layer replacement");
    return 0;
}

static int
test_feature_line_layer_builder(void)
{
    printf("=== Test 9: feature line layer builder ===\n");

    struct bsg_view *v = make_view();
    struct bsg_line_layer_builder *builder = bsg_line_layer_builder_create();
    if (!builder)
	FAIL("line layer builder create returned null");

    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    VSET(p1, 1.0, 0.0, 0.0);
    VSET(p2, 0.0, 2.0, 0.0);
    if (!bsg_line_layer_builder_add(builder, 255, 0, 0, p0, BSG_GEOMETRY_LINE_MOVE) ||
	    !bsg_line_layer_builder_add(builder, 255, 0, 0, p1, BSG_GEOMETRY_LINE_DRAW) ||
	    !bsg_line_layer_builder_add(builder, 0, 0, 255, p2, BSG_GEOMETRY_POINT_DRAW))
	FAIL("line layer builder failed to add typed commands");

    if (!bsg_line_layer_builder_add(builder, 0, 255, 0, p1, BSG_GEOMETRY_LINE_MOVE) ||
	    !bsg_line_layer_builder_add(builder, 0, 255, 0, p2, BSG_GEOMETRY_LINE_DRAW))
	FAIL("line layer builder failed to add green layer commands");

    if (bsg_line_layer_builder_layer_count(builder) != 3 ||
	    bsg_line_layer_builder_point_count(builder) != 5)
	FAIL("line layer builder counts are wrong");

    struct bsg_line_layer *red_layer = bsg_line_layer_builder_find(builder, 255, 0, 0);
    if (bsg_line_layer_builder_layer_at(builder, 3))
	FAIL("line layer builder out-of-range access should return null");
    unsigned char lr = 0;
    unsigned char lg = 0;
    unsigned char lb = 0;
    if (!bsg_line_layer_color(red_layer, &lr, &lg, &lb) ||
	    lr != 255 || lg != 0 || lb != 0)
	FAIL("line layer color accessor is wrong");

    point_t got_point = VINIT_ZERO;
    int got_command = 0;
    if (bsg_line_layer_point_count(red_layer) != 2 ||
	    !bsg_line_layer_point_at(red_layer, 1, got_point) ||
	    !VNEAR_EQUAL(got_point, p1, SMALL_FASTF) ||
	    !bsg_line_layer_command_at(red_layer, 1, &got_command) ||
	    got_command != BSG_GEOMETRY_LINE_DRAW)
	FAIL("line layer typed accessors are wrong");
    if (bsg_line_layer_point_at(red_layer, 2, got_point) ||
	    bsg_line_layer_command_at(red_layer, 2, &got_command))
	FAIL("line layer out-of-range typed access should fail");

    bsg_feature_ref ref = bsg_feature_replace_line_layer_builder(v, "line_builder", 0, builder, NULL);
    if (bsg_feature_ref_is_null(ref))
	FAIL("line layer builder feature replace returned null");
    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record))
	FAIL("line layer builder feature record failed");
    if (record.family != BSG_FEATURE_OVERLAY ||
	    record.child_count != 3 ||
	    record.geometry_command_count != 5)
	FAIL("line layer builder feature record is wrong");

    bsg_line_layer_builder_free(builder);
    free_view(v);
    PASS("feature line layer builder");
    return 0;
}

static int
test_feature_indexed_face_set_replace(void)
{
    printf("=== Test 10: feature indexed face set replacement ===\n");

    struct bsg_view *v = make_view();
    point_t points[3];
    vect_t normals[4];
    int indices[4] = {0, 1, 2, -1};

    VSET(points[0], 0.0, 0.0, 0.0);
    VSET(points[1], 1.0, 0.0, 0.0);
    VSET(points[2], 0.0, 1.0, 0.0);
    VSET(normals[0], 0.0, 0.0, 1.0);
    VSET(normals[1], 0.0, 0.0, 1.0);
    VSET(normals[2], 0.0, 0.0, 1.0);

    struct bsg_feature_style style = BSG_FEATURE_STYLE_INIT;
    style.color_valid = 1;
    style.color[0] = 200;
    style.color[1] = 160;
    style.color[2] = 40;

    bsg_feature_ref ref = bsg_feature_replace_indexed_face_set(v, "face_feature", 1,
	    (const point_t *)points, 3, (const vect_t *)normals, 3, indices, 4, &style);
    if (bsg_feature_ref_is_null(ref))
	FAIL("indexed face-set replacement returned null");

    struct bsg_node *node = (struct bsg_node *)ref.token;
    if (bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_INDEXED_FACE_SET)
	FAIL("feature did not install indexed-face geometry");

    struct bsg_feature_record record;
    if (!bsg_feature_record_get(ref, &record))
	FAIL("indexed face-set feature record failed");
    if (record.family != BSG_FEATURE_FACE_SET || record.scope != BSG_FEATURE_SCOPE_LOCAL)
	FAIL("indexed face-set feature metadata wrong");
    if (record.color[0] != 200 || record.color[1] != 160 || record.color[2] != 40)
	FAIL("indexed face-set feature color wrong");

    struct bsg_export_result *result = bsg_export_scene(v, BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1)
	FAIL("indexed face-set export count wrong");
    const struct bsg_export_record *export_record = bsg_export_result_get(result, 0);
    if (!export_record || export_record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	FAIL("indexed face-set export geometry kind wrong");
    if (export_record->geometry.arrays.point_count != 3 ||
	    export_record->geometry.arrays.normal_count != 3 ||
	    export_record->geometry.arrays.index_count != 4)
	FAIL("indexed face-set export arrays wrong");
    if (export_record->geometry.surface.point_count != 3 ||
	    export_record->geometry.surface.normal_count != 3 ||
	    export_record->geometry.surface.index_count != 4 ||
	    export_record->geometry.surface.face_count != 1 ||
	    export_record->geometry.surface.normal_kind != BSG_RENDER_SURFACE_NORMALS_PER_INDEX ||
	    !export_record->geometry.surface.material_valid ||
	    export_record->geometry.surface.material.color[0] != 200 ||
	    export_record->geometry.surface.material.color[1] != 160 ||
	    export_record->geometry.surface.material.color[2] != 40)
	FAIL("indexed face-set export surface wrong");
    bsg_export_result_free(result);

    free_view(v);
    PASS("feature indexed face set replacement");
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	fprintf(stderr, "Unexpected arguments\n");

    int ret = 0;
    ret |= test_feature_create_record_payload();
    ret |= test_feature_scopes_and_typed_defaults();
    ret |= test_feature_geometry_update();
    ret |= test_feature_line_update_uses_geometry_fields();
    ret |= test_polygon_realization_uses_geometry_fields();
    ret |= test_feature_label_sync();
    ret |= test_feature_fields_ignore_raw_mirrors();
    ret |= test_feature_line_layers_replace();
    ret |= test_feature_line_layer_builder();
    ret |= test_feature_indexed_face_set_replace();
    return ret;
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
