/*           T E S T _ S N A P _ A C T I O N . C
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
/** @file libbsg/tests/test_snap_action.c
 *
 * Unit tests for bsg/snap_action.h API.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/str.h"
#include "bsg/interaction.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_storage_private.h"
#include "bsg/snap.h"
#include "bsg/snap_action.h"
#include "bsg/scene_object.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "../object_private.h"
#include "../scene_object_private.h"
#include "../node_private.h"

static int
test_null_guards(void)
{
    struct bsg_snap_result out = {0};
    point_t p = VINIT_ZERO;
    if (bsg_snap_candidates(NULL, p, 0.0, BSG_SNAP_KIND_GRID, &out) != 0) {
	printf("FAIL: NULL view guard\n");
	return 1;
    }
    bsg_snap_result_free(&out);
    return 0;
}

static int
test_grid_candidate(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;
    struct bsg_grid_state grid;
    bsg_view_grid_get(&v, &grid);
    grid.res_h = 1.0;
    grid.res_v = 1.0;
    bsg_view_grid_set(&v, &grid);

    point_t sample = {0.1, 0.1, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 0.0, BSG_SNAP_KIND_GRID, &out);
    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    if (cnt < 0) {
	printf("FAIL: grid snap count negative\n");
	return 1;
    }
    return 0;
}

static int
test_line_candidate_action_visibility(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;

    bsg_separator_ref root = bsg_view_scene_separator_ref(&v, 1);
    bsg_line_set_ref visible_lines = bsg_line_set_ref_create(&v, "visible_snap_source");
    bsg_line_set_ref hidden_lines = bsg_line_set_ref_create(&v, "hidden_snap_source");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(visible_lines)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(hidden_lines))) {
	printf("FAIL: scene setup\n");
	return 1;
    }

    bsg_node *visible = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(visible_lines)));
    bsg_node *hidden = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(hidden_lines)));
    if (!visible || !hidden) {
	printf("FAIL: line node setup\n");
	return 1;
    }
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(visible_lines));
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(hidden_lines));
    bsg_node_set_source_flags(visible, BSG_SOURCE_DB);
    bsg_node_set_source_flags(hidden, BSG_SOURCE_DB);
    bsg_node_set_visible_state(hidden, 0);

    point_t visible_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(visible_pts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(visible_lines, (const point_t *)visible_pts, cmds, 2)) {
	printf("FAIL: visible line geometry setup\n");
	return 1;
    }

    point_t hidden_pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(hidden_pts[0], 0.0, 0.05, 0.0);
    VSET(hidden_pts[1], 1.0, 0.05, 0.0);
    if (!bsg_line_set_ref_set_points(hidden_lines, (const point_t *)hidden_pts, cmds, 2)) {
	printf("FAIL: hidden line geometry setup\n");
	return 1;
    }

    point_t sample = {0.4, 0.06, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 1.0, BSG_SNAP_KIND_ENDPOINT, &out);
    if (cnt != 1) {
	printf("FAIL: line snap candidate count %d\n", cnt);
	return 1;
    }
    const char *candidate_path = bu_vls_cstr(&out.sr_candidates[0].sc_source_path);
    if (!candidate_path || !candidate_path[0]) {
	printf("FAIL: visible snap candidate should expose stable source identity\n");
	return 1;
    }
    struct bsg_interaction_result *ir = bsg_interaction_from_snap_result(&v, &out);
    if (!ir || bsg_interaction_result_count(ir) != 1) {
	printf("FAIL: snap interaction conversion count\n");
	return 1;
    }
    const struct bsg_interaction_record *rec = bsg_interaction_result_get(ir, 0);
    const char *path = bsg_interaction_record_path(rec);
    if (!rec || rec->kind != BSG_INTERACTION_SNAP_CANDIDATE ||
	    !path || !path[0]) {
	printf("FAIL: snap interaction record mismatch\n");
	return 1;
    }
    bsg_interaction_result_free(ir);

    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    return 0;
}

static int
test_geometry_line_candidate_action(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;

    bsg_separator_ref root = bsg_view_scene_separator_ref(&v, 1);
    bsg_line_set_ref lines = bsg_line_set_ref_create(&v, "geometry_snap_source");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines))) {
	printf("FAIL: geometry snap setup\n");
	return 1;
    }

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    VSET(pts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2)) {
	printf("FAIL: geometry snap line points\n");
	return 1;
    }
    bsg_node *line_node = bsg_object_ref_node(bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    if (!line_node) {
	printf("FAIL: geometry snap raw line node\n");
	return 1;
    }
    bsg_node_set_source_flags(line_node, BSG_SOURCE_DB);
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root), bsg_line_set_ref_as_node(lines));

    point_t sample = {0.4, 0.06, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 1.0, BSG_SNAP_KIND_ENDPOINT, &out);
    if (cnt != 1) {
	printf("FAIL: geometry line snap candidate count %d\n", cnt);
	return 1;
    }
    if (!NEAR_EQUAL(out.sr_candidates[0].sc_point[X], 0.4, BN_TOL_DIST) ||
	    !NEAR_EQUAL(out.sr_candidates[0].sc_point[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: geometry line snap point %g %g %g\n", V3ARGS(out.sr_candidates[0].sc_point));
	return 1;
    }

    point_t old_pt = VINIT_ZERO;
    int old_cnt = bsg_snap_lines_3d(&old_pt, &v, &sample);
    if (old_cnt != 1) {
	printf("FAIL: legacy geometry line snap count %d\n", old_cnt);
	return 1;
    }
    if (!NEAR_EQUAL(old_pt[X], 0.4, BN_TOL_DIST) ||
	    !NEAR_EQUAL(old_pt[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: legacy geometry line snap point %g %g %g\n", V3ARGS(old_pt));
	return 1;
    }

    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    return 0;
}

static int
test_geometry_indexed_line_candidate_action(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;

    bsg_separator_ref root = bsg_view_scene_separator_ref(&v, 1);
    bsg_indexed_line_set_ref lines =
	bsg_indexed_line_set_ref_create(&v, "geometry_indexed_snap_source");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_line_set_ref_as_node(lines))) {
	printf("FAIL: indexed geometry snap setup\n");
	return 1;
    }

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 1.0, 2.0, 0.0);
    int indices[5] = {0, 1, -1, 1, 2};
    if (!bsg_indexed_line_set_ref_set_geometry(lines,
		(const point_t *)pts, 3, indices, 5)) {
	printf("FAIL: indexed geometry snap line points\n");
	return 1;
    }
    bsg_node *line_node =
	bsg_object_ref_node(bsg_node_ref_object(bsg_indexed_line_set_ref_as_node(lines)));
    if (!line_node) {
	printf("FAIL: indexed geometry snap raw line node\n");
	return 1;
    }
    bsg_node_set_source_flags(line_node, BSG_SOURCE_DB);
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_line_set_ref_as_node(lines));

    point_t sample = {0.94, 0.7, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 1.0, BSG_SNAP_KIND_ENDPOINT, &out);
    if (cnt != 1) {
	printf("FAIL: indexed geometry line snap candidate count %d\n", cnt);
	return 1;
    }
    if (!NEAR_EQUAL(out.sr_candidates[0].sc_point[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(out.sr_candidates[0].sc_point[Y], 0.7, BN_TOL_DIST)) {
	printf("FAIL: indexed geometry line snap point %g %g %g\n",
		V3ARGS(out.sr_candidates[0].sc_point));
	return 1;
    }

    point_t old_pt = VINIT_ZERO;
    int old_cnt = bsg_snap_lines_3d(&old_pt, &v, &sample);
    if (old_cnt != 1) {
	printf("FAIL: legacy indexed geometry line snap count %d\n", old_cnt);
	return 1;
    }
    if (!NEAR_EQUAL(old_pt[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(old_pt[Y], 0.7, BN_TOL_DIST)) {
	printf("FAIL: legacy indexed geometry line snap point %g %g %g\n",
		V3ARGS(old_pt));
	return 1;
    }

    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    return 0;
}

static int
test_geometry_point_candidate_action(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;

    bsg_separator_ref root = bsg_view_scene_separator_ref(&v, 1);
    bsg_point_set_ref points =
	bsg_point_set_ref_create(&v, "geometry_point_snap_source");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_point_set_ref_as_node(points))) {
	printf("FAIL: point geometry snap setup\n");
	return 1;
    }

    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 1.0, 0.0, 0.0);
    VSET(pts[1], 4.0, 0.0, 0.0);
    if (!bsg_point_set_ref_set_points(points, (const point_t *)pts, 2)) {
	printf("FAIL: point geometry snap points\n");
	return 1;
    }
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_point_set_ref_as_node(points));

    point_t sample = {1.1, 0.05, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 0.25, BSG_SNAP_KIND_ENDPOINT, &out);
    if (cnt != 1) {
	printf("FAIL: point geometry snap candidate count %d\n", cnt);
	return 1;
    }
    if (!NEAR_EQUAL(out.sr_candidates[0].sc_point[X], 1.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(out.sr_candidates[0].sc_point[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: point geometry snap point %g %g %g\n",
		V3ARGS(out.sr_candidates[0].sc_point));
	return 1;
    }
    if (!BU_STR_EQUAL(bu_vls_cstr(&out.sr_candidates[0].sc_source_path),
		"geometry_point_snap_source")) {
	printf("FAIL: point geometry snap source %s\n",
		bu_vls_cstr(&out.sr_candidates[0].sc_source_path));
	return 1;
    }

    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    return 0;
}

static int
test_geometry_indexed_face_candidate_action(void)
{
    struct bsg_view v;
    bsg_view_init(&v, NULL);
    v.gv_width = 512;
    v.gv_height = 512;

    bsg_separator_ref root = bsg_view_scene_separator_ref(&v, 1);
    bsg_indexed_face_set_ref faces =
	bsg_indexed_face_set_ref_create(&v, "geometry_face_snap_source");
    if (bsg_node_ref_is_null(bsg_separator_ref_as_node(root)) ||
	    bsg_node_ref_is_null(bsg_indexed_face_set_ref_as_node(faces))) {
	printf("FAIL: indexed-face geometry snap setup\n");
	return 1;
    }

    point_t pts[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 0.0, 0.0, 0.0);
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 0.0, 1.0, 0.0);
    int indices[3] = {0, 1, 2};
    if (!bsg_indexed_face_set_ref_set_geometry(faces, (const point_t *)pts, 3,
		indices, 3)) {
	printf("FAIL: indexed-face geometry snap points\n");
	return 1;
    }
    bsg_group_ref_append_child(bsg_separator_ref_as_group(root),
	    bsg_indexed_face_set_ref_as_node(faces));

    point_t sample = {0.08, 0.04, 0.0};
    struct bsg_snap_result out = {0};
    int cnt = bsg_snap_candidates(&v, sample, 0.25, BSG_SNAP_KIND_ENDPOINT, &out);
    if (cnt != 1) {
	printf("FAIL: indexed-face geometry snap candidate count %d\n", cnt);
	return 1;
    }
    if (!NEAR_EQUAL(out.sr_candidates[0].sc_point[X], 0.0, BN_TOL_DIST) ||
	    !NEAR_EQUAL(out.sr_candidates[0].sc_point[Y], 0.0, BN_TOL_DIST)) {
	printf("FAIL: indexed-face geometry snap point %g %g %g\n",
		V3ARGS(out.sr_candidates[0].sc_point));
	return 1;
    }
    if (!BU_STR_EQUAL(bu_vls_cstr(&out.sr_candidates[0].sc_source_path),
		"geometry_face_snap_source")) {
	printf("FAIL: indexed-face geometry snap source %s\n",
		bu_vls_cstr(&out.sr_candidates[0].sc_source_path));
	return 1;
    }

    bsg_snap_result_free(&out);
    bsg_view_free(&v);
    return 0;
}

static int
test_stale_snap_candidate_serializes_invalid(void)
{
    struct bsg_snap_candidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    VSET(candidate.sc_point, 1.0, 2.0, 3.0);
    candidate.sc_kind = BSG_SNAP_KIND_ENDPOINT;
    candidate.sc_distance = 4.0;
    candidate.sc_feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    bu_vls_init(&candidate.sc_source_path);
    bu_vls_sprintf(&candidate.sc_source_path, "%s", "/stale/source.s");
    candidate.sc_stale = 1;

    struct bsg_interaction_record *rec =
	bsg_interaction_from_snap_candidate(NULL, &candidate);
    if (!rec) {
	printf("FAIL: stale snap interaction create\n");
	bu_vls_free(&candidate.sc_source_path);
	return 1;
    }
    const char *path = bsg_interaction_record_path(rec);
    int ret = 0;
    if (rec->valid || !path || !BU_STR_EQUAL(path, "/stale/source.s")) {
	printf("FAIL: stale snap should serialize as invalid with source path\n");
	ret = 1;
    }
    bsg_interaction_record_free(rec);
    bu_vls_free(&candidate.sc_source_path);
    return ret;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;
    if (test_null_guards())
	return 1;
    if (test_grid_candidate())
	return 1;
    if (test_line_candidate_action_visibility())
	return 1;
    if (test_geometry_line_candidate_action())
	return 1;
    if (test_geometry_indexed_line_candidate_action())
	return 1;
    if (test_geometry_point_candidate_action())
	return 1;
    if (test_geometry_indexed_face_candidate_action())
	return 1;
    if (test_stale_snap_candidate_serializes_invalid())
	return 1;
    printf("PASS test_snap_action\n");
    return 0;
}
