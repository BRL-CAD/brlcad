/*                 T E S T _ E X P O R T . C
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
/** @file libbsg/tests/test_export.c
 *
 * Unit tests for bsg/export.h semantic report records.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/draw_source.h"
#include "../draw_source_private.h"
#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/interaction.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../payload_typed_private.h"
#include "bsg/render.h"
#include "bsg/scene_builder.h"
#include "bsg/selection.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "../geometry_private.h"
#include "../object_private.h"

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "export_test_view");
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "export_test_view");
}

struct export_edit_preview_fanout_state {
    uint64_t revision;
    int update_calls;
    int update_returns;
};

static uint64_t
export_edit_preview_revision(void *ctx)
{
    struct export_edit_preview_fanout_state *state =
	(struct export_edit_preview_fanout_state *)ctx;
    return state ? state->revision : 0;
}

static int
export_edit_preview_update(void *ctx, struct bsg_view *UNUSED(v))
{
    struct export_edit_preview_fanout_state *state =
	(struct export_edit_preview_fanout_state *)ctx;
    if (!state)
	return -1;
    state->update_calls++;
    if (state->update_returns > 0)
	state->revision++;
    return state->update_returns;
}

static bsg_node *
_create_export_line_node(struct bsg_view *v, bsg_node *root, const char *name)
{
    bsg_line_set_ref lines;
    bsg_node *node;
    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};

    if (!v || !root || !name)
	return NULL;

    lines = bsg_line_set_ref_create(v, name);
    if (bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)))
	return NULL;

    node = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!node)
	return NULL;

    VSET(pts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2))
	return NULL;

    bsg_node_add_child(root, node);
    bsg_node_set_name(node, name);
    return node;
}

struct _segment_check {
    size_t count;
    point_t first_a;
    point_t first_b;
    point_t last_b;
};

struct _point_check {
    size_t count;
    point_t first;
    point_t last;
};

static int
_segment_check_cb(const point_t a, const point_t b, void *data)
{
    struct _segment_check *check = (struct _segment_check *)data;

    if (!check)
	return 0;

    if (!check->count) {
	VMOVE(check->first_a, a);
	VMOVE(check->first_b, b);
    }
    VMOVE(check->last_b, b);
    check->count++;
    return 1;
}

static int
_point_check_cb(const point_t pt, void *data)
{
    struct _point_check *check = (struct _point_check *)data;

    if (!check)
	return 0;

    if (!check->count)
	VMOVE(check->first, pt);
    VMOVE(check->last, pt);
    check->count++;
    return 1;
}

static int
test_export_visible_line_geometry_record(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) {
	printf("FAIL: scene setup\n");
	return 1;
    }
    bsg_node_add_child(root, shape);
    bsg_node_set_name(shape, "/exported/shape.s");
    bsg_node_set_visible_state(shape, 1);
    bsg_shape_mark_db_object(shape);

    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(p1, 1.0, 2.0, 3.0);
    VMOVE(pts[0], p0);
    VMOVE(pts[1], p1);
    if (!bsg_geometry_node_set_line_set_fields(shape, (const point_t *)pts, cmds, 2)) {
	printf("FAIL: export line geometry setup\n");
	return 1;
    }
    bsg_node_set_draw_center(shape, p1);
    bsg_node_set_draw_size(shape, 7.0);
    bsg_appearance_set_line_style(shape, 1);
    bsg_appearance_set_legacy_eval_flag(shape, 1);

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result) {
	printf("FAIL: export result\n");
	return 1;
    }
    if (bsg_export_result_count(result) != 1) {
	printf("FAIL: export record count %zu\n", bsg_export_result_count(result));
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record) {
	printf("FAIL: export record get\n");
	return 1;
    }
    if (bu_strcmp(bu_vls_cstr(&record->path), "/exported/shape.s") != 0) {
	printf("FAIL: export path %s\n", bu_vls_cstr(&record->path));
	return 1;
    }
    if (!record->source.source_id || !record->source.geometry_id) {
	printf("FAIL: export source identity\n");
	return 1;
    }
    if (record->source.scope != BSG_RENDER_SOURCE_SCOPE_DATABASE ||
	    record->source.geometry_role != BSG_RENDER_GEOMETRY_ROLE_DATABASE_OBJECT) {
	printf("FAIL: export source semantics scope=%d role=%d\n",
		(int)record->source.scope, (int)record->source.geometry_role);
	return 1;
    }
    if (!record->visible || record->non_database_source) {
	printf("FAIL: export visibility/display flags\n");
	return 1;
    }
    if (record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	printf("FAIL: export geometry kind %d\n", (int)record->geometry.kind);
	return 1;
    }
    if (record->realization_status != BSG_REALIZE_STATUS_CURRENT) {
	printf("FAIL: export realization status %d\n", (int)record->realization_status);
	return 1;
    }
    if (record->line_style != 1 || record->evaluated_region != 1) {
	printf("FAIL: export appearance facts %d/%d\n",
		record->line_style, record->evaluated_region);
	return 1;
    }
    if (record->vlist_command_count != 2 ||
	    record->vlist_point_count != 2) {
	printf("FAIL: export line geometry stats %zu/%zu/%zu\n",
		record->vlist_structure_count,
		record->vlist_command_count,
		record->vlist_point_count);
	return 1;
    }
    if (!record->has_bounds ||
	    !NEAR_EQUAL(record->bounds_center[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(record->bounds_center[Y], 2.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(record->bounds_center[Z], 3.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(record->bounds_radius, 7.0, BN_TOL_DIST)) {
	printf("FAIL: export bounds\n");
	return 1;
    }

    struct _segment_check check;
    memset(&check, 0, sizeof(check));
    if (!bsg_export_record_has_segments(record) ||
	    bsg_export_record_foreach_segment(record, _segment_check_cb, &check) != 1 ||
	    check.count != 1 ||
	    !NEAR_EQUAL(check.first_a[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_a[Y], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_a[Z], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[Y], 2.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[Z], 3.0, BN_TOL_DIST)) {
	printf("FAIL: export line segment helper count=%zu\n", check.count);
	return 1;
    }

    struct _point_check point_check;
    memset(&point_check, 0, sizeof(point_check));
    if (bsg_export_record_foreach_point(record, _point_check_cb, &point_check) != 2 ||
	    point_check.count != 2 ||
	    !NEAR_EQUAL(point_check.first[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Y], 2.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Z], 3.0, BN_TOL_DIST)) {
	printf("FAIL: export geometry point helper count=%zu\n", point_check.count);
	return 1;
    }

    struct bu_vls report = BU_VLS_INIT_ZERO;
    bsg_export_record_geometry_report(record, &report);
    if (!strstr(bu_vls_cstr(&report), "line move") ||
	    !strstr(bu_vls_cstr(&report), "line draw")) {
	printf("FAIL: export geometry report %s\n", bu_vls_cstr(&report));
	return 1;
    }
    bu_vls_free(&report);

    struct bu_vls serialized = BU_VLS_INIT_ZERO;
    bsg_export_result_serialize(result, &serialized);
    if (!strstr(bu_vls_cstr(&serialized), "\"path\":\"/exported/shape.s\"") ||
	    !strstr(bu_vls_cstr(&serialized), "\"line_style\":1") ||
	    !strstr(bu_vls_cstr(&serialized), "\"evaluated_region\":1") ||
	    !strstr(bu_vls_cstr(&serialized), "\"vlist_structures\":0") ||
	    !strstr(bu_vls_cstr(&serialized), "\"vlist_commands\":2")) {
	printf("FAIL: export serialization %s\n", bu_vls_cstr(&serialized));
	return 1;
    }
    bu_vls_free(&serialized);

    bsg_export_result_free(result);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_geometry_line_set_record(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, "geometry-lines");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines))) {
	printf("FAIL: geometry line-set setup\n");
	return 1;
    }

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[1], 4.0, 5.0, 6.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2)) {
	printf("FAIL: geometry line-set points\n");
	return 1;
    }
    struct bsg_node *line_node =
	bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!bsg_geometry_node_clear_private_realization(line_node)) {
	printf("FAIL: geometry line-set private realization clear\n");
	return 1;
    }
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(lines));

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: geometry export count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	printf("FAIL: geometry export semantics kind=%d roles=%u\n",
		record ? (int)record->geometry.kind : -1,
		record ? record->roles : 0);
	return 1;
    }
    if (record->geometry.arrays.point_count != 2 ||
	    !record->geometry.arrays.points ||
	    record->geometry.arrays.command_count != 2 ||
	    !record->geometry.arrays.commands) {
	printf("FAIL: geometry line-set export did not snapshot fields\n");
	return 1;
    }

    struct _segment_check check;
    memset(&check, 0, sizeof(check));
    if (!bsg_export_record_has_segments(record) ||
	    bsg_export_record_foreach_segment(record, _segment_check_cb, &check) != 1 ||
	    check.count != 1 ||
	    !NEAR_EQUAL(check.first_b[X], 4.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[Y], 5.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[Z], 6.0, BN_TOL_DIST)) {
	printf("FAIL: geometry line-set segment helper count=%zu\n", check.count);
	return 1;
    }

    struct _point_check point_check;
    memset(&point_check, 0, sizeof(point_check));
    if (bsg_export_record_foreach_point(record, _point_check_cb, &point_check) != 2 ||
	    point_check.count != 2 ||
	    !NEAR_EQUAL(point_check.last[X], 4.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Y], 5.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Z], 6.0, BN_TOL_DIST)) {
	printf("FAIL: geometry line-set point helper count=%zu\n", point_check.count);
	return 1;
    }

    struct bu_vls report = BU_VLS_INIT_ZERO;
    bsg_export_record_geometry_report(record, &report);
    if (!strstr(bu_vls_cstr(&report), "line move") ||
	    !strstr(bu_vls_cstr(&report), "line draw")) {
	printf("FAIL: geometry line-set report %s\n", bu_vls_cstr(&report));
	return 1;
    }
    bu_vls_free(&report);

    bsg_export_result_free(result);
    _free_view(v);
    return 0;
}

static int
test_export_indexed_line_set_segments(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_line_set_ref lines = bsg_indexed_line_set_ref_create(v, "indexed-lines");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(lines))) {
	printf("FAIL: indexed line-set setup\n");
	return 1;
    }

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 2.0, 0.0);
    int indices[5] = {0, 1, -1, 1, 2};
    if (!bsg_indexed_line_set_ref_set_geometry(lines, (const point_t *)pts, 3, indices, 5)) {
	printf("FAIL: indexed line-set geometry\n");
	return 1;
    }
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_indexed_line_set_ref_as_node(lines));

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: indexed line-set export count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY) ||
	    record->geometry.arrays.point_count != 3 ||
	    record->geometry.arrays.index_count != 5) {
	printf("FAIL: indexed line-set export semantics kind=%d roles=%u points=%zu indices=%zu\n",
		record ? (int)record->geometry.kind : -1,
		record ? record->roles : 0,
		record ? record->geometry.arrays.point_count : 0,
		record ? record->geometry.arrays.index_count : 0);
	return 1;
    }

    struct _segment_check check;
    memset(&check, 0, sizeof(check));
    if (!bsg_export_record_has_segments(record) ||
	    bsg_export_record_foreach_segment(record, _segment_check_cb, &check) != 2 ||
	    check.count != 2 ||
	    !NEAR_EQUAL(check.first_b[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(check.first_b[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: indexed line-set segment helper count=%zu\n", check.count);
	return 1;
    }
    struct _point_check point_check;
    memset(&point_check, 0, sizeof(point_check));
    if (bsg_export_record_foreach_point(record, _point_check_cb, &point_check) != 3 ||
	    point_check.count != 3 ||
	    !NEAR_EQUAL(point_check.last[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Y], 2.0, BN_TOL_DIST)) {
	printf("FAIL: indexed line-set point helper count=%zu\n", point_check.count);
	return 1;
    }

    struct bu_vls report = BU_VLS_INIT_ZERO;
    bsg_export_record_geometry_report(record, &report);
    if (!strstr(bu_vls_cstr(&report), "line move") ||
	    !strstr(bu_vls_cstr(&report), "line draw")) {
	printf("FAIL: indexed line-set report %s\n", bu_vls_cstr(&report));
	return 1;
    }
    bu_vls_free(&report);

    bsg_export_result_free(result);
    _free_view(v);
    return 0;
}

static int
test_export_indexed_face_set_report(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_indexed_face_set_ref faces = bsg_indexed_face_set_ref_create(v, "indexed-faces");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(faces))) {
	printf("FAIL: indexed face-set setup\n");
	return 1;
    }

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t normals[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 0.0, 1.0, 0.0);
    for (size_t i = 0; i < 3; i++)
	VSET(normals[i], 0.0, 0.0, 1.0);
    int indices[4] = {0, 1, 2, -1};
    if (!bsg_geometry_ref_set_indexed_face_set(
		bsg_indexed_face_set_ref_as_geometry(faces),
		(const point_t *)pts, 3, (const vect_t *)normals, 3,
		indices, 4)) {
	printf("FAIL: indexed face-set geometry\n");
	return 1;
    }
    struct bsg_node *face_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_indexed_face_set_ref_as_node(faces)));
    unsigned char color[3] = {45, 90, 135};
    bsg_appearance_set_color_override(face_node, color, 1);
    bsg_appearance_set_dmode(face_node, 2);
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_face_set_ref_as_node(faces));

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: indexed face-set export count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY) ||
	    record->geometry.surface.point_count != 3 ||
	    record->geometry.surface.normal_count != 3 ||
	    record->geometry.surface.index_count != 4 ||
	    record->geometry.surface.face_count != 1 ||
	    record->geometry.surface.normal_kind != BSG_RENDER_SURFACE_NORMALS_PER_INDEX ||
	    !record->geometry.surface.material_valid ||
	    record->geometry.surface.material.color[0] != color[0] ||
	    record->geometry.surface.material.color[1] != color[1] ||
	    record->geometry.surface.material.color[2] != color[2] ||
	    record->geometry.surface.material.draw_mode != 2) {
	printf("FAIL: indexed face-set export semantics kind=%d roles=%u points=%zu indices=%zu\n",
		record ? (int)record->geometry.kind : -1,
		record ? record->roles : 0,
		record ? record->geometry.surface.point_count : 0,
		record ? record->geometry.surface.index_count : 0);
	return 1;
    }
    struct _segment_check segment_check;
    memset(&segment_check, 0, sizeof(segment_check));
    if (!bsg_export_record_has_segments(record) ||
	    bsg_export_record_foreach_segment(record, _segment_check_cb, &segment_check) != 3 ||
	    segment_check.count != 3 ||
	    !NEAR_EQUAL(segment_check.first_b[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(segment_check.last_b[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(segment_check.last_b[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: indexed face-set segment helper count=%zu\n", segment_check.count);
	return 1;
    }

    struct _point_check point_check;
    memset(&point_check, 0, sizeof(point_check));
    if (bsg_export_record_foreach_point(record, _point_check_cb, &point_check) != 3 ||
	    point_check.count != 3 ||
	    !NEAR_EQUAL(point_check.last[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(point_check.last[Y], 1.0, BN_TOL_DIST)) {
	printf("FAIL: indexed face-set point helper count=%zu\n", point_check.count);
	return 1;
    }

    struct bu_vls report = BU_VLS_INIT_ZERO;
    bsg_export_record_geometry_report(record, &report);
    if (!strstr(bu_vls_cstr(&report), "indexed face 0") ||
	    !strstr(bu_vls_cstr(&report), "indexed face 2")) {
	printf("FAIL: indexed face-set report %s\n", bu_vls_cstr(&report));
	return 1;
    }
    bu_vls_free(&report);

    bsg_export_result_free(result);
    _free_view(v);
    return 0;
}

static int
test_export_text_record_snapshot(void)
{
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) {
	printf("FAIL: text export scene setup\n");
	return 1;
    }
    bsg_node_add_child(root, shape);
    bsg_node_set_visible_state(shape, 1);

    struct bsg_label *label;
    BU_GET(label, struct bsg_label);
    memset(label, 0, sizeof(*label));
    bu_vls_init(&label->label);
    bu_vls_strcpy(&label->label, "export label");
    VSET(label->p, 1.0, 2.0, 3.0);
    VSET(label->target, 4.0, 5.0, 6.0);
    label->size = 2;
    label->line_flag = 1;
    label->anchor = BSG_ANCHOR_BOTTOM_CENTER;
    label->arrow = 1;
    const char *source_label = bu_vls_cstr(&label->label);
    bsg_node_set_payload(shape, bsg_payload_text_create(label));

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: text export count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_TEXT ||
	    !(record->roles & BSG_EXPORT_RECORD_LABEL)) {
	printf("FAIL: text export semantics kind=%d roles=%u\n",
		record ? (int)record->geometry.kind : -1,
		record ? record->roles : 0);
	return 1;
    }
    if (!record->geometry.text.label ||
	    record->geometry.text.label == source_label ||
	    !BU_STR_EQUAL(record->geometry.text.label, "export label") ||
	    !VNEAR_EQUAL(record->geometry.text.position, label->p, SMALL_FASTF) ||
	    !VNEAR_EQUAL(record->geometry.text.target, label->target, SMALL_FASTF) ||
	    record->geometry.text.size != label->size ||
	    record->geometry.text.line_flag != label->line_flag ||
	    record->geometry.text.anchor != label->anchor ||
	    record->geometry.text.arrow != label->arrow) {
	printf("FAIL: text export should snapshot label fields\n");
	return 1;
    }

    bsg_export_result_free(result);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_proxy_record_snapshot(void)
{
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *csg_shape = bsg_shape_create(v);
    bsg_node *null_csg_shape = bsg_shape_create(v);
    bsg_node *brep_shape = bsg_shape_create(v);
    bsg_node *external_shape = bsg_shape_create(v);
    bsg_node *null_external_shape = bsg_shape_create(v);
    bsg_node *preview_shape = bsg_shape_create(v);
    if (!root || !csg_shape || !null_csg_shape || !brep_shape ||
	    !external_shape || !null_external_shape || !preview_shape) {
	printf("FAIL: proxy export scene setup\n");
	return 1;
    }
    bsg_node_add_child(root, csg_shape);
    bsg_node_add_child(root, null_csg_shape);
    bsg_node_add_child(root, brep_shape);
    bsg_node_add_child(root, external_shape);
    bsg_node_add_child(root, null_external_shape);
    bsg_node_add_child(root, preview_shape);
    bsg_node_set_visible_state(csg_shape, 1);
    bsg_node_set_visible_state(null_csg_shape, 1);
    bsg_node_set_visible_state(brep_shape, 1);
    bsg_node_set_visible_state(external_shape, 1);
    bsg_node_set_visible_state(null_external_shape, 1);
    bsg_node_set_visible_state(preview_shape, 1);

    int csg_source = 33;
    int brep_source = 44;
    int external_source = 66;
    int preview_source = 55;
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
	    !preview_payload || !preview_data) {
	printf("FAIL: proxy export payload setup\n");
	return 1;
    }
    external_payload->pl.opaque = &external_source;
    const unsigned char external_color[3] = {91, 42, 17};
    bsg_appearance_set_color_override(external_shape, external_color, 1);
    bsg_appearance_set_transparency(external_shape, 0.375);
    bsg_appearance_set_dmode(external_shape, 4);
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

    struct proxy_export_expect {
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

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 6) {
	printf("FAIL: proxy export count %zu\n",
		result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *record = bsg_export_result_get(result, i);
	int matched = 0;
	if (!record || !(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	    printf("FAIL: proxy export missing geometry role\n");
	    return 1;
	}
	for (size_t j = 0; j < sizeof(expected) / sizeof(expected[0]); j++) {
	    if (record->geometry.kind != expected[j].geometry_kind ||
		    record->geometry.proxy.kind != expected[j].proxy_kind ||
		    record->geometry.proxy.source_identity != expected[j].source_identity)
		continue;
	    matched = 1;
	    expected[j].seen = 1;
	    if (record->geometry.source_identity !=
		    record->geometry.proxy.source_identity ||
		    record->geometry.proxy.revision != record->payload_revision) {
		printf("FAIL: proxy export metadata kind=%d\n",
			(int)record->geometry.kind);
		return 1;
	    }
	    if (record->realization_kind != expected[j].realization_kind ||
		    record->realization_status != expected[j].realization_status ||
		    record->stale_reason != expected[j].stale_reason) {
		printf("FAIL: proxy export realization kind=%d\n",
			(int)record->geometry.kind);
		return 1;
	    }
	    if (expected[j].styled &&
		    (record->phase != BSG_RENDER_PHASE_TRANSPARENT ||
		    record->color[0] != external_color[0] ||
		    record->color[1] != external_color[1] ||
		    record->color[2] != external_color[2] ||
		    !NEAR_EQUAL(record->transparency, 0.375, SMALL_FASTF) ||
		    record->draw_mode != 4 ||
		    record->line_style != 1)) {
		printf("FAIL: styled proxy export material/phase kind=%d\n",
			(int)record->geometry.kind);
		return 1;
	    }
	}
	if (!matched) {
	    printf("FAIL: unexpected proxy export kind=%d proxy=%d\n",
		    record ? (int)record->geometry.kind : -1,
		    record ? (int)record->geometry.proxy.kind : -1);
	    return 1;
	}
    }
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
	if (!expected[i].seen) {
	    printf("FAIL: missing proxy export kind=%d\n",
		    (int)expected[i].geometry_kind);
	    return 1;
	}
    }

    bsg_export_result_free(result);
    bsg_shape_destroy(csg_shape);
    bsg_shape_destroy(null_csg_shape);
    bsg_shape_destroy(brep_shape);
    bsg_shape_destroy(external_shape);
    bsg_shape_destroy(null_external_shape);
    bsg_shape_destroy(preview_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_edit_preview_fanout(void)
{
    enum { FANOUT = 5 };
    struct export_edit_preview_fanout_state state;
    memset(&state, 0, sizeof(state));
    state.revision = 60;
    state.update_returns = 1;

    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shapes[FANOUT];
    memset(shapes, 0, sizeof(shapes));
    for (int i = 0; i < FANOUT; i++) {
	char name[64];
	snprintf(name, sizeof(name), "export_preview_fanout_%d", i);
	shapes[i] = bsg_shape_create(v);
	if (!shapes[i]) {
	    printf("FAIL: export edit-preview fanout shape create\n");
	    return 1;
	}
	bsg_node_set_name(shapes[i], name);
	bsg_node_set_visible_state(shapes[i], 1);
	bsg_node_add_child(root, shapes[i]);
	struct bsg_payload *payload =
	    bsg_payload_edit_preview_create(&state, NULL);
	if (!payload) {
	    printf("FAIL: export edit-preview fanout payload create\n");
	    return 1;
	}
	if (!bsg_payload_edit_preview_set_ops(payload, &state, 0,
		    export_edit_preview_revision,
		    export_edit_preview_update,
		    NULL, NULL, NULL, NULL)) {
	    printf("FAIL: export edit-preview fanout set_ops\n");
	    return 1;
	}
	if (!bsg_payload_edit_preview_mark_source_revision(payload,
		    500 + (uint64_t)i,
		    BSG_EDIT_PREVIEW_STALE_SOURCE_CHANGED)) {
	    printf("FAIL: export edit-preview fanout stale mark\n");
	    return 1;
	}
	bsg_node_set_payload(shapes[i], payload);
    }

    struct bsg_export_result *result =
	bsg_export_scene(v, BSG_RENDER_FLAG_VISIBLE_ONLY);
    if (!result || bsg_export_result_count(result) != FANOUT) {
	printf("FAIL: export edit-preview fanout stale count %zu\n",
		result ? bsg_export_result_count(result) : 0);
	return 1;
    }
    if (state.update_calls != 0) {
	printf("FAIL: export edit-preview fanout should not update without prepare\n");
	return 1;
    }
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *record =
	    bsg_export_result_get(result, i);
	if (!record ||
		!(record->roles & BSG_EXPORT_RECORD_GEOMETRY) ||
		record->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
		record->geometry.proxy.kind != BSG_RENDER_PROXY_EDIT_PREVIEW ||
		record->geometry.proxy.source_identity == 0 ||
		record->geometry.proxy.revision != record->payload_revision ||
		record->realization_kind != BSG_REALIZE_EDIT_PREVIEW ||
		record->realization_status != BSG_REALIZE_STATUS_STALE ||
		record->stale_reason != BSG_PAYLOAD_STALE_SOURCE_CHANGED) {
	    printf("FAIL: export edit-preview fanout stale record\n");
	    return 1;
	}
    }
    bsg_export_result_free(result);

    result = bsg_export_scene(v, BSG_RENDER_FLAG_VISIBLE_ONLY |
	    BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != FANOUT) {
	printf("FAIL: export edit-preview fanout prepared count %zu\n",
		result ? bsg_export_result_count(result) : 0);
	return 1;
    }
    if (state.update_calls != FANOUT) {
	printf("FAIL: export edit-preview fanout should update every payload\n");
	return 1;
    }
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *record =
	    bsg_export_result_get(result, i);
	if (!record ||
		record->geometry.kind != BSG_RENDER_GEOMETRY_EDIT_PREVIEW ||
		record->geometry.proxy.kind != BSG_RENDER_PROXY_EDIT_PREVIEW ||
		record->realization_kind != BSG_REALIZE_EDIT_PREVIEW ||
		record->realization_status != BSG_REALIZE_STATUS_CURRENT ||
		record->stale_reason != BSG_PAYLOAD_STALE_NONE) {
	    printf("FAIL: export edit-preview fanout prepared record\n");
	    return 1;
	}
    }

    bsg_export_result_free(result);
    for (int i = 0; i < FANOUT; i++)
	bsg_shape_destroy(shapes[i]);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_image_framebuffer_record_snapshot(void)
{
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) {
	printf("FAIL: image export root setup\n");
	return 1;
    }

    unsigned char px[8] = {1, 2, 3, 255, 4, 5, 6, 255};
    struct bsg_payload *image_payload = bsg_payload_image_create(2, 1, 4, px);
    struct bsg_payload_image *source_image =
	bsg_payload_image_get(image_payload);
    if (!image_payload || !source_image || !source_image->pixels) {
	printf("FAIL: image export payload setup\n");
	return 1;
    }
    bsg_node *image_shape = bsg_shape_create(v);
    bsg_node_set_name(image_shape, "export_payload_image");
    bsg_node_set_payload(image_shape, image_payload);
    bsg_node_add_child(root, image_shape);

    bsg_framebuffer_ref framebuffer =
	bsg_framebuffer_ref_create(v, "export_framebuffer");
    bsg_node *framebuffer_node = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_framebuffer_ref_as_node(framebuffer)));
    if (!framebuffer_node || !bsg_framebuffer_ref_set_mode(framebuffer, 4)) {
	printf("FAIL: framebuffer export setup\n");
	return 1;
    }
    bsg_node_add_child(root, framebuffer_node);

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 2) {
	printf("FAIL: image/framebuffer export count %zu\n",
		result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    int saw_image = 0;
    int saw_framebuffer = 0;
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *record =
	    bsg_export_result_get(result, i);
	const char *path = record ? bu_vls_cstr(&record->path) : NULL;
	if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_IMAGE ||
		!(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	    printf("FAIL: image export semantics kind=%d roles=%u\n",
		    record ? (int)record->geometry.kind : -1,
		    record ? record->roles : 0);
	    return 1;
	}
	if (path && BU_STR_EQUAL(path, "export_payload_image")) {
	    saw_image = 1;
	    if (record->geometry.image.kind != BSG_RENDER_IMAGE_RASTER ||
		    record->geometry.image.width != 2 ||
		    record->geometry.image.height != 1 ||
		    record->geometry.image.channels != 4 ||
		    record->geometry.image.pixel_count != sizeof(px) ||
		    !record->geometry.image.pixels ||
		    record->geometry.image.pixels == source_image->pixels ||
		    memcmp(record->geometry.image.pixels, px, sizeof(px)) != 0) {
		printf("FAIL: image export should own raster pixel snapshot\n");
		return 1;
	    }
	}
	if (path && BU_STR_EQUAL(path, "export_framebuffer")) {
	    saw_framebuffer = 1;
	    if (record->geometry.image.kind != BSG_RENDER_IMAGE_FRAMEBUFFER ||
		    record->geometry.image.framebuffer_mode != 4 ||
		    record->geometry.image.pixel_count != 0 ||
		    record->geometry.image.pixels) {
		printf("FAIL: framebuffer export should snapshot mode\n");
		return 1;
	    }
	}
    }
    if (!saw_image || !saw_framebuffer) {
	printf("FAIL: image/framebuffer export missing expected records\n");
	return 1;
    }

    bsg_export_result_free(result);
    bsg_shape_destroy(image_shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_annotation_record_snapshot(void)
{
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = bsg_shape_create(v);
    if (!root || !shape) {
	printf("FAIL: annotation export scene setup\n");
	return 1;
    }
    bsg_node_set_name(shape, "export_annotation");
    bsg_node_add_child(root, shape);
    bsg_scene_ref shape_ref = { shape };
    bsg_scene_set_color(shape_ref, 31, 63, 95);
    bsg_scene_set_transparency(shape_ref, 0.75);
    bsg_scene_set_dmode(shape_ref, 4);

    point_t pts[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 2.0, 3.0, 4.0);
    VSET(pts[1], 5.0, 6.0, 7.0);
    VSET(pts[2], 8.0, 9.0, 10.0);
    VSET(pts[3], 11.0, 12.0, 13.0);
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
    segs[4].data.text.text = (char *)"export annotation";
    segs[4].data.text.size = 14.0;
    segs[4].data.text.rotation = 15.0;
    struct bsg_payload *payload =
	bsg_payload_annotation_create_record("export annotation",
		BSG_ANNOTATION_SPACE_DISPLAY, pts[0], model_mat, display_mat,
		(const point_t *)pts, 4, segs, 5);
    struct bsg_payload_annotation *source_annotation =
	bsg_payload_annotation_get(payload);
    if (!payload || !source_annotation || !source_annotation->points) {
	printf("FAIL: annotation export payload setup\n");
	return 1;
    }
    bsg_node_set_payload(shape, payload);

    bsg_feature_ref null_feature = BSG_FEATURE_REF_NULL_INIT;
    struct bsg_interaction_record *selection_record =
	bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
		null_feature, "export_annotation", NULL);
    if (!selection_record || !bsg_view_selection(v)) {
	printf("FAIL: annotation export selection setup\n");
	return 1;
    }
    bsg_selection_add_record(bsg_view_selection(v), selection_record);

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: annotation export count %zu\n",
		result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record =
	bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_ANNOTATION ||
	    !(record->roles & BSG_EXPORT_RECORD_ANNOTATION)) {
	printf("FAIL: annotation export semantics kind=%d roles=%u\n",
		record ? (int)record->geometry.kind : -1,
		record ? record->roles : 0);
	return 1;
    }
    if (record->source.source_id == 0 ||
	    record->source.geometry_id != record->geometry.source_identity ||
	    record->geometry.source_identity !=
	    (uint64_t)(uintptr_t)source_annotation) {
	printf("FAIL: annotation export identity should be stable\n");
	return 1;
    }
    if (record->phase != BSG_RENDER_PHASE_TRANSPARENT ||
	    record->selected != 1 ||
	    record->highlighted != 1 ||
	    record->color[0] != 31 ||
	    record->color[1] != 63 ||
	    record->color[2] != 95 ||
	    !NEAR_EQUAL(record->transparency, 0.75, SMALL_FASTF) ||
	    record->draw_mode != 4) {
	printf("FAIL: annotation export should preserve appearance and selection\n");
	return 1;
    }
    if (!record->geometry.annotation.text ||
	    !BU_STR_EQUAL(record->geometry.annotation.text, "export annotation") ||
	    record->geometry.annotation.text == bu_vls_cstr(&source_annotation->text) ||
	    record->geometry.annotation.space != BSG_ANNOTATION_SPACE_DISPLAY ||
	    !VNEAR_EQUAL(record->geometry.annotation.anchor, pts[0], SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.model_mat[MDX], 0.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.model_mat[MDY], 0.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.model_mat[MDZ], 0.75, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.display_mat[MDX], 1.25, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.display_mat[MDY], 1.5, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.display_mat[MDZ], 1.75, SMALL_FASTF) ||
	    record->geometry.annotation.point_count != 4 ||
	    !record->geometry.annotation.points ||
	    record->geometry.annotation.points == source_annotation->points ||
	    !VNEAR_EQUAL(record->geometry.annotation.points[0], pts[0], SMALL_FASTF) ||
	    !VNEAR_EQUAL(record->geometry.annotation.points[3], pts[3], SMALL_FASTF) ||
	    record->geometry.annotation.segment_count != 5 ||
	    !record->geometry.annotation.segments ||
	    record->geometry.annotation.segments == source_annotation->segments ||
	    record->geometry.annotation.segments[0].kind != BSG_ANNOTATION_SEGMENT_LINE ||
	    record->geometry.annotation.segments[1].kind != BSG_ANNOTATION_SEGMENT_CARC ||
	    !NEAR_EQUAL(record->geometry.annotation.segments[1].data.carc.radius, 2.5, SMALL_FASTF) ||
	    record->geometry.annotation.segments[2].kind != BSG_ANNOTATION_SEGMENT_NURB ||
	    record->geometry.annotation.segments[2].data.nurb.knots == source_annotation->segments[2].data.nurb.knots ||
	    record->geometry.annotation.segments[2].data.nurb.control_points == source_annotation->segments[2].data.nurb.control_points ||
	    record->geometry.annotation.segments[2].data.nurb.weights == source_annotation->segments[2].data.nurb.weights ||
	    !NEAR_EQUAL(record->geometry.annotation.segments[2].data.nurb.weights[1], 0.5, SMALL_FASTF) ||
	    record->geometry.annotation.segments[3].kind != BSG_ANNOTATION_SEGMENT_BEZIER ||
	    record->geometry.annotation.segments[3].data.bezier.control_points == source_annotation->segments[3].data.bezier.control_points ||
	    record->geometry.annotation.segments[3].data.bezier.control_points[3] != 3 ||
	    record->geometry.annotation.segments[4].kind != BSG_ANNOTATION_SEGMENT_TEXT ||
	    record->geometry.annotation.segments[4].data.text.text == source_annotation->segments[4].data.text.text ||
	    !BU_STR_EQUAL(record->geometry.annotation.segments[4].data.text.text, "export annotation") ||
	    record->geometry.annotation.segments[4].data.text.ref_pt != 3 ||
	    record->geometry.annotation.segments[4].data.text.relative_position != 1 ||
	    !NEAR_EQUAL(record->geometry.annotation.segments[4].data.text.size, 14.0, SMALL_FASTF) ||
	    !NEAR_EQUAL(record->geometry.annotation.segments[4].data.text.rotation, 15.0, SMALL_FASTF)) {
	printf("FAIL: annotation export should own annotation snapshot\n");
	return 1;
    }

    bsg_export_result_free(result);
    bsg_interaction_record_free(selection_record);
    bsg_shape_destroy(shape);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_measurement_feature_geometry_record(void)
{
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) {
	printf("FAIL: measurement export scene root\n");
	return 1;
    }
    v->gv_scene = root;

    bsg_feature_ref ref = bsg_feature_create_lines(v, "measurement_line", 1);
    if (bsg_feature_ref_is_null(ref)) {
	printf("FAIL: measurement export feature setup\n");
	return 1;
    }
    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[1], 3.0, 0.0, 0.0);
    if (!bsg_feature_points_replace(ref, BSG_FEATURE_MEASUREMENT, pts, cmds, 2)) {
	printf("FAIL: measurement export feature points\n");
	return 1;
    }
    bsg_scene_ref scene_ref = bsg_feature_ref_as_scene(ref);
    bsg_node *feature = bsg_object_ref_node(bsg_object_ref_from_scene_ref(scene_ref));
    if (!feature) {
	printf("FAIL: measurement export feature node\n");
	return 1;
    }

    struct bsg_export_request request;
    bsg_export_request_init(&request, v);
    request.query_flags = BSG_EXPORT_QUERY_VIEW_OBJECTS | BSG_EXPORT_QUERY_LOCAL_ONLY;
    request.render_flags = BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: measurement export query count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }

    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    record->source.geometry_role != BSG_RENDER_GEOMETRY_ROLE_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	printf("FAIL: measurement export geometry semantics kind=%d role=%d roles=%u\n",
		record ? (int)record->geometry.kind : -1,
		record ? (int)record->source.geometry_role : -1,
		record ? record->roles : 0);
	return 1;
    }

    bsg_export_result_free(result);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_visible_filter(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *shape = _create_export_line_node(v, root, "hidden.s");
    if (!root || !shape) {
	printf("FAIL: scene setup\n");
	return 1;
    }
    bsg_node_set_visible_state(shape, 0);

    struct bsg_export_result *result = bsg_export_scene(v,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    if (!result) {
	printf("FAIL: export hidden result\n");
	return 1;
    }
    if (bsg_export_result_count(result) != 0) {
	printf("FAIL: hidden visible-only count %zu\n", bsg_export_result_count(result));
	return 1;
    }

    bsg_export_result_free(result);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

static int
test_export_query_filters(void)
{
    struct bsg_view *v = _make_view();
    v->gv_width = 640;
    v->gv_height = 480;

    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *db_shape = _create_export_line_node(v, root, "/query/db.s");
    if (!root || !db_shape) {
	printf("FAIL: query scene setup\n");
	return 1;
    }
    v->gv_scene = root;
    bsg_shape_mark_db_object(db_shape);

    bsg_feature_ref view_ref = bsg_feature_create_lines(v, "view_line", 0);
    bsg_feature_ref local_ref = bsg_feature_create_lines(v, "local_line", 1);
    if (!view_ref.token || !local_ref.token) {
	printf("FAIL: query feature setup\n");
	return 1;
    }

    bsg_feature_set_visible(view_ref, 0);

    struct bsg_export_request request;
    bsg_export_request_init(&request, v);
    request.query_flags = BSG_EXPORT_QUERY_VIEW_OBJECTS;
    request.render_flags = BSG_RENDER_FLAG_PAYLOAD_PREPARE;

    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result) {
	printf("FAIL: query result\n");
	return 1;
    }
    if (bsg_export_result_count(result) != 2) {
	printf("FAIL: view feature query count %zu\n", bsg_export_result_count(result));
	return 1;
    }
    bsg_export_result_free(result);

    request.query_flags = BSG_EXPORT_QUERY_VIEW_OBJECTS | BSG_EXPORT_QUERY_LOCAL_ONLY;
    result = bsg_export_query(&request);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: local view feature query count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }
    const struct bsg_export_record *record = bsg_export_result_get(result, 0);
    if (!record || bu_strcmp(bu_vls_cstr(&record->path), "local_line") != 0) {
	printf("FAIL: local query path\n");
	return 1;
    }
    if (record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET ||
	    !(record->roles & BSG_EXPORT_RECORD_GEOMETRY)) {
	printf("FAIL: local query geometry semantics kind=%d roles=%u\n",
		(int)record->geometry.kind, record->roles);
	return 1;
    }
    bsg_export_result_free(result);

    request.query_flags = BSG_EXPORT_QUERY_DB_OBJECTS;
    request.glob = "*/db.s";
    result = bsg_export_query(&request);
    if (!result || bsg_export_result_count(result) != 1) {
	printf("FAIL: db glob query count %zu\n", result ? bsg_export_result_count(result) : 0);
	return 1;
    }
    record = bsg_export_result_get(result, 0);
    if (!record || !(record->roles & BSG_EXPORT_RECORD_GEOMETRY) ||
	    !(record->roles & BSG_EXPORT_RECORD_SCENE)) {
	printf("FAIL: query record roles\n");
	return 1;
    }
    if (record->geometry.kind != BSG_RENDER_GEOMETRY_LINE_SET) {
	printf("FAIL: query db geometry kind %d\n",
		record ? (int)record->geometry.kind : -1);
	return 1;
    }

    bsg_export_result_free(result);
    bsg_view_scene_root_detach_from_root(root);
    _free_view(v);
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;
    if (test_export_visible_line_geometry_record())
	return 1;
    if (test_export_geometry_line_set_record())
	return 1;
    if (test_export_indexed_line_set_segments())
	return 1;
    if (test_export_indexed_face_set_report())
	return 1;
    if (test_export_text_record_snapshot())
	return 1;
    if (test_export_proxy_record_snapshot())
	return 1;
    if (test_export_edit_preview_fanout())
	return 1;
    if (test_export_image_framebuffer_record_snapshot())
	return 1;
    if (test_export_annotation_record_snapshot())
	return 1;
    if (test_export_measurement_feature_geometry_record())
	return 1;
    if (test_export_visible_filter())
	return 1;
    if (test_export_query_filters())
	return 1;
    printf("PASS test_export\n");
    return 0;
}
