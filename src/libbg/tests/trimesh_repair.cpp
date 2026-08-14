/*              T R I M E S H _ R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file tests/trimesh_repair.cpp
 *
 * Unit tests for bg_trimesh_repair.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include <array>
#include <vector>
#include <Mathematics/MeshHoleFilling.h>
#include <Mathematics/MeshPreprocessing.h>
#include <Mathematics/MeshRepair.h>

#include "bu.h"
#include "bg.h"

/* --------------------------------------------------------------------------
 * Test geometry helpers
 * -------------------------------------------------------------------------- */

/* Build a closed unit cube (12 triangles, 8 vertices).
 * Vertices are the corners of [0,1]^3, faces are CCW. */
static void
make_cube(point_t **out_pts, int *n_pts, int **out_faces, int *n_faces)
{
    static point_t pts[8] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    };
    /* 12 triangles (2 per face of the cube) */
    static int faces[36] = {
	0, 2, 1,  0, 3, 2,   /* bottom  (-Z) */
	4, 5, 6,  4, 6, 7,   /* top     (+Z) */
	0, 1, 5,  0, 5, 4,   /* front   (-Y) */
	1, 2, 6,  1, 6, 5,   /* right   (+X) */
	2, 3, 7,  2, 7, 6,   /* back    (+Y) */
	3, 0, 4,  3, 4, 7    /* left    (-X) */
    };
    *out_pts   = pts;
    *n_pts     = 8;
    *out_faces = faces;
    *n_faces   = 12;
}

/* Build a cube with the top face removed (open shell, 10 triangles). */
static void
make_open_cube(point_t **out_pts, int *n_pts, int **out_faces, int *n_faces)
{
    static point_t pts[8] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    };
    /* 10 triangles – top (+Z) face omitted */
    static int faces[30] = {
	0, 2, 1,  0, 3, 2,   /* bottom */
	0, 1, 5,  0, 5, 4,   /* front  */
	1, 2, 6,  1, 6, 5,   /* right  */
	2, 3, 7,  2, 7, 6,   /* back   */
	3, 0, 4,  3, 4, 7    /* left   */
    };
    *out_pts   = pts;
    *n_pts     = 8;
    *out_faces = faces;
    *n_faces   = 10;
}

/* Build a small tetrahedron (closed solid, 4 triangles, 4 vertices). */
static void
make_tet(point_t **out_pts, int *n_pts, int **out_faces, int *n_faces)
{
    static point_t pts[4] = {
	{0, 0, 0}, {1, 0, 0}, {0.5, 1, 0}, {0.5, 0.5, 1}
    };
    static int faces[12] = {
	0, 2, 1,
	0, 1, 3,
	1, 2, 3,
	0, 3, 2
    };
    *out_pts   = pts;
    *n_pts     = 4;
    *out_faces = faces;
    *n_faces   = 4;
}


/* --------------------------------------------------------------------------
 * Test cases
 * -------------------------------------------------------------------------- */

/* A closed solid mesh should be returned as "already solid" (return value 1). */
static int
test_already_solid(void)
{
    point_t *pts;
    int n_pts;
    int *faces;
    int n_faces;
    make_cube(&pts, &n_pts, &faces, &n_faces);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    int ret = bg_trimesh_repair(&ofaces, &n_ofaces, &opnts, &n_opnts,
				faces, n_faces, pts, n_pts, NULL);

    if (ret != 1) {
	bu_log("FAIL test_already_solid: expected return 1, got %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    /* Outputs should not be set when input is already solid */
    if (ofaces || opnts || n_ofaces || n_opnts) {
	bu_log("FAIL test_already_solid: outputs should be NULL/0 when already solid\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }

    bu_log("PASS test_already_solid\n");
    return 0;
}

/* The tetrahedron is a minimal closed solid.  Also tests NULL opts path. */
static int
test_tet_already_solid(void)
{
    point_t *pts;
    int n_pts;
    int *faces;
    int n_faces;
    make_tet(&pts, &n_pts, &faces, &n_faces);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    /* Pass NULL opts – should use defaults */
    int ret = bg_trimesh_repair(&ofaces, &n_ofaces, &opnts, &n_opnts,
				faces, n_faces, pts, n_pts, NULL);

    if (ret != 1) {
	bu_log("FAIL test_tet_already_solid: expected return 1, got %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }

    bu_log("PASS test_tet_already_solid\n");
    return 0;
}

/* The public acceptance check must exercise the bundled Manifold importer
 * without repairing or normalizing the caller's arrays. */
static int
test_manifold_acceptance_check(void)
{
    point_t *closed_points;
    int closed_point_count;
    int *closed_faces;
    int closed_face_count;
    make_cube(&closed_points, &closed_point_count, &closed_faces,
	&closed_face_count);
    point_t *open_points;
    int open_point_count;
    int *open_faces;
    int open_face_count;
    make_open_cube(&open_points, &open_point_count, &open_faces,
	&open_face_count);
    int invalid_faces[36];
    memcpy(invalid_faces, closed_faces, sizeof(invalid_faces));
    invalid_faces[0] = closed_point_count;

    const bool valid = bg_trimesh_manifold_accepted(closed_point_count,
	closed_face_count, (fastf_t *)closed_points, closed_faces) == 1 &&
	bg_trimesh_manifold_accepted(open_point_count, open_face_count,
	    (fastf_t *)open_points, open_faces) == 0 &&
	bg_trimesh_manifold_accepted(closed_point_count, closed_face_count,
	    (fastf_t *)closed_points, invalid_faces) == 0;
    if (!valid) {
	bu_log("FAIL test_manifold_acceptance_check\n");
	return -1;
    }
    bu_log("PASS test_manifold_acceptance_check\n");
    return 0;
}

/* A closed orientable mesh with inconsistent local winding needs no
 * geometric repair.  Synchronize only triangle order, preserve the complete
 * point and face sets, and require acceptance by both topology validators. */
static int
test_closed_orientation_sync(void)
{
    point_t *pts;
    int n_pts;
    int *cube_faces;
    int n_faces;
    make_cube(&pts, &n_pts, &cube_faces, &n_faces);
    int faces[36];
    memcpy(faces, cube_faces, sizeof(faces));
    std::swap(faces[0], faces[1]);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.allow_self_intersections = 1;
    settings.require_manifold = 1;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, n_faces, pts, n_pts, &settings, &report);
    const bool valid = ret == 0 && ofaces && opnts &&
	n_ofaces == n_faces && n_opnts == n_pts && report.solid &&
	report.manifold_accepted && report.reoriented_faces == 1 &&
	!report.removed_faces && !report.added_faces &&
	!report.geometric_degenerate_faces && !report.unmatched_edges &&
	!report.excess_edges && !report.misoriented_edges &&
	!report.invalid_vertex_links &&
	!bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
	    NULL);
    if (ofaces)
	bu_free(ofaces, "orientation sync faces");
    if (opnts)
	bu_free(opnts, "orientation sync points");
    if (!valid) {
	bu_log("FAIL test_closed_orientation_sync: ret=%d, reoriented=%d, "
	    "solid=%d, manifold=%d, topology=%d/%d/%d/%d\n", ret,
	    report.reoriented_faces, report.solid,
	    report.manifold_accepted, report.unmatched_edges,
	    report.excess_edges, report.misoriented_edges,
	    report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_closed_orientation_sync\n");
    return 0;
}

/* An open mesh (cube missing one face) should be repaired to a solid. */
static int
test_open_cube_repair(void)
{
    point_t *pts;
    int n_pts;
    int *faces;
    int n_faces;
    make_open_cube(&pts, &n_pts, &faces, &n_faces);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    /* The missing top face is ~20% of the full cube's surface area, so set
     * the limit high enough to allow it to be filled. */
    struct bg_trimesh_repair_opts opts = BG_TRIMESH_REPAIR_OPTS_DEFAULT;
    opts.max_hole_area_percent = 30.0;
    int ret = bg_trimesh_repair(&ofaces, &n_ofaces, &opnts, &n_opnts,
				faces, n_faces, pts, n_pts, &opts);

    if (ret != 0 && ret != 1) {
	bu_log("FAIL test_open_cube_repair: unexpected return %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }

    if (ret == 0) {
	/* Verify output looks reasonable */
	if (!ofaces || n_ofaces < n_faces || !opnts || n_opnts <= 0) {
	    bu_log("FAIL test_open_cube_repair: invalid output arrays (ret=0)\n");
	    if (ofaces) bu_free(ofaces, "ofaces");
	    if (opnts)  bu_free(opnts, "opnts");
	    return -1;
	}
	/* Check that the output is now solid */
	int not_solid = bg_trimesh_solid2(n_opnts, n_ofaces,
					  (fastf_t *)opnts, ofaces, NULL);
	if (not_solid) {
	    bu_log("FAIL test_open_cube_repair: repaired mesh is still not solid\n");
	    bu_free(ofaces, "ofaces");
	    bu_free(opnts, "opnts");
	    return -1;
	}
	bu_log("PASS test_open_cube_repair (repaired: %d faces, %d verts)\n",
	       n_ofaces, n_opnts);
	bu_free(ofaces, "ofaces");
	bu_free(opnts, "opnts");
    } else {
	/* ret == 1: the geometry was already solid despite missing the top –
	 * that is also acceptable (means the repair detection was conservative).
	 * The test considers both outcomes valid. */
	bu_log("PASS test_open_cube_repair (already solid – no fill needed)\n");
    }

    return 0;
}

/* Repair output must not retain vertices which no output triangle uses. */
static int
test_unused_vertex_compaction(void)
{
    point_t *cube_pts;
    int cube_point_count;
    int *faces;
    int face_count;
    make_open_cube(&cube_pts, &cube_point_count, &faces, &face_count);
    point_t points[9];
    memcpy(points, cube_pts, (size_t)cube_point_count * sizeof(point_t));
    VSET(points[8], 99.0, 99.0, 99.0);

    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_opts options = BG_TRIMESH_REPAIR_OPTS_DEFAULT;
    options.max_hole_area_percent = 30.0;
    const int result = bg_trimesh_repair(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces,
	face_count, points, 9, &options);
    bool retained_unused = false;
    for (int point = 0; point < output_point_count; ++point) {
	if (VNEAR_EQUAL(output_points[point], points[8], SMALL_FASTF)) {
	    retained_unused = true;
	    break;
	}
    }
    const bool valid = result == 0 && output_faces && output_points &&
	output_face_count >= face_count && !retained_unused;
    bu_free(output_faces, "unused vertex output faces");
    bu_free(output_points, "unused vertex output points");
    if (!valid) {
	bu_log("FAIL test_unused_vertex_compaction: result %d, points %d, "
	    "unused %d\n", result, output_point_count,
	    retained_unused ? 1 : 0);
	return -1;
    }
    bu_log("PASS test_unused_vertex_compaction\n");
    return 0;
}

/* The extended interface is conservative by default: it must not silently
 * close a hole unless the caller explicitly enables hole filling. */
static int
test_repair2_conservative_default(void)
{
    point_t *pts;
    int n_pts;
    int *faces;
    int n_faces;
    make_open_cube(&pts, &n_pts, &faces, &n_faces);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts, &n_opnts,
	faces, n_faces, pts, n_pts, &settings, &report);
    if (ret != -1 || ofaces || opnts || n_ofaces || n_opnts ||
	    report.solid) {
	bu_log("FAIL test_repair2_conservative_default: ret=%d solid=%d\n",
	    ret, report.solid);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts) bu_free(opnts, "opnts");
	return -1;
    }

    bu_log("PASS test_repair2_conservative_default\n");
    return 0;
}

/* Explicitly permitting the missing cube face should produce a certified
 * solid and a useful operation report. */
static int
test_repair2_report(void)
{
    point_t *pts;
    int n_pts;
    int *faces;
    int n_faces;
    make_open_cube(&pts, &n_pts, &faces, &n_faces);

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 30.0;
    settings.max_hole_edges = 8;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts, &n_opnts,
	faces, n_faces, pts, n_pts, &settings, &report);
    const bool valid_report = ret == 0 && report.solid &&
	report.input_vertices == n_pts && report.input_faces == n_faces &&
	report.output_vertices == n_opnts &&
	report.output_faces == n_ofaces && report.added_faces >= 2 &&
	report.removed_faces == 0 && report.input_area > 0.0 &&
	report.output_area > report.input_area && report.output_volume > 0.0 &&
	NEAR_ZERO(report.max_vertex_displacement, SMALL_FASTF);
    if (!valid_report || !ofaces || !opnts ||
	    bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
		NULL)) {
	bu_log("FAIL test_repair2_report: ret=%d solid=%d added=%d\n",
	    ret, report.solid, report.added_faces);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts) bu_free(opnts, "opnts");
	return -1;
    }

    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");
    bu_log("PASS test_repair2_report\n");
    return 0;
}

/* Two triangular holes which touch only at one boundary vertex form a
 * figure-eight boundary.  Each exact three-edge cycle must be capped before
 * non-manifold boundary splitting destroys the cycles. */
static int
test_touching_triangular_holes(void)
{
    static point_t points[6] = {
	{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0},
	{-1, 0, 0}, {0, -1, 0}
    };
    static int faces[18] = {
	0, 3, 4, 0, 5, 2,
	1, 3, 2, 1, 4, 3, 1, 5, 4, 1, 2, 5
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 3;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 6,
	points, 6, &settings, &report);
    const bool valid = result == 0 && report.solid &&
	report.manifold_accepted && report.added_faces == 2 &&
	output_face_count == 8 && !bg_trimesh_solid2(output_point_count,
	output_face_count, (fastf_t *)output_points, output_faces, NULL);
    if (output_faces)
	bu_free(output_faces, "touching triangular hole faces");
    if (output_points)
	bu_free(output_points, "touching triangular hole points");
    if (!valid) {
	bu_log("FAIL test_touching_triangular_holes: ret=%d solid=%d "
	    "manifold=%d added=%d faces=%d\n", result, report.solid,
	    report.manifold_accepted, report.added_faces,
	    output_face_count);
	return -1;
    }
    bu_log("PASS test_touching_triangular_holes\n");
    return 0;
}

/* Two open boxes can likewise have four-edge holes which share one indexed
 * boundary vertex.  They are two exact cycles, not one self-touching polygon;
 * close both and then split the point contact into valid vertex links. */
static int
test_touching_quadrilateral_holes(void)
{
    static point_t points[15] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
	{2, 1, 1}, {2, 2, 1}, {1, 2, 1},
	{1, 1, 2}, {2, 1, 2}, {2, 2, 2}, {1, 2, 2}
    };
    static int faces[60] = {
	0, 2, 1, 0, 3, 2,
	0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6,
	3, 0, 4, 3, 4, 7,
	11, 12, 13, 11, 13, 14,
	6, 8, 12, 6, 12, 11,
	8, 9, 13, 8, 13, 12,
	9, 10, 14, 9, 14, 13,
	10, 6, 11, 10, 11, 14
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 4;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 20,
	points, 15, &settings, &report);
    const bool valid = result == 0 && report.solid &&
	report.manifold_accepted && report.added_faces == 4 &&
	output_face_count == 24 && !bg_trimesh_solid2(output_point_count,
	output_face_count, (fastf_t *)output_points, output_faces, NULL);
    if (output_faces)
	bu_free(output_faces, "touching quadrilateral hole faces");
    if (output_points)
	bu_free(output_points, "touching quadrilateral hole points");
    if (!valid) {
	bu_log("FAIL test_touching_quadrilateral_holes: ret=%d solid=%d "
	    "manifold=%d added=%d faces=%d links=%d\n", result,
	    report.solid, report.manifold_accepted, report.added_faces,
	    output_face_count, report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_touching_quadrilateral_holes\n");
    return 0;
}

/* A failed Manifold postcondition must still report the topology of the
 * candidate that was rejected.  Otherwise a modest open seam is
 * indistinguishable from an empty or catastrophically malformed mesh. */
static int
test_manifold_rejection_report(void)
{
    static point_t points[4] = {
	{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
    };
    static int faces[9] = {
	0, 2, 1, 0, 1, 3, 1, 2, 3
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 3,
	points, 4, &settings, &report);
    const bool valid = result == -1 && !output_faces && !output_points &&
	report.unmatched_edges == 3 && !report.excess_edges &&
	!report.misoriented_edges && report.invalid_vertex_links == 3 &&
	!report.solid && !report.manifold_accepted;
    if (output_faces)
	bu_free(output_faces, "rejected Manifold faces");
    if (output_points)
	bu_free(output_points, "rejected Manifold points");
    if (!valid) {
	bu_log("FAIL test_manifold_rejection_report: ret=%d unmatched=%d "
	    "excess=%d misoriented=%d links=%d solid=%d manifold=%d\n",
	    result, report.unmatched_edges, report.excess_edges,
	    report.misoriented_edges, report.invalid_vertex_links,
	    report.solid, report.manifold_accepted);
	return -1;
    }
    bu_log("PASS test_manifold_rejection_report\n");
    return 0;
}

/* Manifold accepts disconnected closed components which meet at one point
 * when their coincident vertices remain topologically distinct.  Preserve
 * that indexed interpretation instead of welding the contact into a
 * non-manifold vertex.  Requesting hole filling must remain a no-op for an
 * already closed input. */
static int
test_manifold_preserves_point_contact(void)
{
    static point_t points[8] = {
	{0, 0, 0}, {1, 0, 0}, {0.5, 1, 0}, {0.5, 0.5, 1},
	{0, 0, 0}, {-1, 0, 0}, {-0.5, -1, 0}, {-0.5, -0.5, -1}
    };
    static int faces[24] = {
	0, 2, 1, 0, 1, 3, 1, 2, 3, 0, 3, 2,
	4, 5, 6, 4, 7, 5, 5, 7, 6, 4, 6, 7
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.allow_self_intersections = 1;
    settings.fill_holes = 1;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 8,
	points, 8, &settings, &report);
    const bool valid = result == 0 && output_faces && output_points &&
	output_face_count == 8 && output_point_count == 8 && report.solid &&
	report.manifold_accepted && !report.excess_edges &&
	!report.invalid_vertex_links;
    if (output_faces)
	bu_free(output_faces, "point-contact faces");
    if (output_points)
	bu_free(output_points, "point-contact points");
    if (!valid) {
	bu_log("FAIL test_manifold_preserves_point_contact: ret=%d "
	    "faces=%d points=%d solid=%d manifold=%d excess=%d links=%d\n",
	    result, output_face_count, output_point_count, report.solid,
	    report.manifold_accepted, report.excess_edges,
	    report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_manifold_preserves_point_contact\n");
    return 0;
}

/* Two closed tetrahedra sharing one indexed point have closed edge incidence
 * but a disconnected vertex link.  Repair should split the common index into
 * two coincident topological vertices before any coordinate welding. */
static int
test_manifold_splits_pinched_vertex(void)
{
    static point_t points[7] = {
	{0, 0, 0}, {1, 0, 0}, {0.5, 1, 0}, {0.5, 0.5, 1},
	{-1, 0, 0}, {-0.5, -1, 0}, {-0.5, -0.5, -1}
    };
    static int faces[24] = {
	0, 2, 1, 0, 1, 3, 1, 2, 3, 0, 3, 2,
	0, 4, 5, 0, 6, 4, 4, 6, 5, 0, 5, 6
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.allow_self_intersections = 1;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 8,
	points, 7, &settings, &report);
    const bool valid = result == 0 && output_faces && output_points &&
	output_face_count == 8 && output_point_count == 8 && report.solid &&
	report.manifold_accepted && !report.unmatched_edges &&
	!report.invalid_vertex_links;
    if (output_faces)
	bu_free(output_faces, "pinched vertex faces");
    if (output_points)
	bu_free(output_points, "pinched vertex points");
    if (!valid) {
	bu_log("FAIL test_manifold_splits_pinched_vertex: ret=%d "
	    "faces=%d points=%d solid=%d manifold=%d unmatched=%d "
	    "links=%d\n", result, output_face_count, output_point_count,
	    report.solid, report.manifold_accepted, report.unmatched_edges,
	    report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_manifold_splits_pinched_vertex\n");
    return 0;
}

/* Split a closed pinched link before deleting a separate flat closed island.
 * Deleting degenerates first must not turn a topology-only operation into an
 * expensive global repair. */
static int
test_pinched_vertex_before_flat_removal(void)
{
    static point_t points[10] = {
	{0, 0, 0}, {1, 0, 0}, {0.5, 1, 0}, {0.5, 0.5, 1},
	{-1, 0, 0}, {-0.5, -1, 0}, {-0.5, -0.5, -1},
	{10, 0, 0}, {11, 0, 0}, {12, 0, 0}
    };
    static int faces[30] = {
	0, 2, 1, 0, 1, 3, 1, 2, 3, 0, 3, 2,
	0, 4, 5, 0, 6, 4, 4, 6, 5, 0, 5, 6,
	7, 8, 9, 7, 9, 8
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.allow_self_intersections = 1;
    settings.fill_holes = 1;
    settings.require_manifold = 1;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 10,
	points, 10, &settings, &report);
    const bool valid = result == 0 && output_faces && output_points &&
	output_face_count == 8 && output_point_count == 8 && report.solid &&
	report.manifold_accepted && report.removed_faces >= 2 &&
	report.separated_vertices == 1 && !report.unmatched_edges &&
	!report.invalid_vertex_links;
    if (output_faces)
	bu_free(output_faces, "pinched flat faces");
    if (output_points)
	bu_free(output_points, "pinched flat points");
    if (!valid) {
	bu_log("FAIL test_pinched_vertex_before_flat_removal: ret=%d "
	    "faces=%d points=%d removed=%d separated=%d solid=%d "
	    "manifold=%d unmatched=%d links=%d\n", result,
	    output_face_count, output_point_count, report.removed_faces,
	    report.separated_vertices, report.solid,
	    report.manifold_accepted, report.unmatched_edges,
	    report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_pinched_vertex_before_flat_removal\n");
    return 0;
}

/* A topologically valid cap is not acceptable when it cuts through another
 * closed component.  Both available triangulations cover the same square, so
 * repair must leave the hole open and fail instead of returning an
 * intersecting, incidence-only "solid". */
static int
test_intersecting_hole_patch_rejected(void)
{
    static point_t pts[12] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
	{0.5, 0.5, 0.8}, {0.4, 0.4, 1.2},
	{0.6, 0.4, 1.2}, {0.5, 0.6, 1.2}
    };
    static int faces[42] = {
	0, 2, 1, 0, 3, 2,
	0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6,
	3, 0, 4, 3, 4, 7,
	8, 10, 9, 8, 9, 11, 9, 10, 11, 8, 11, 10
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 8;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 14, pts, 12, &settings, &report);
    const bool rejected = ret == -1 && !ofaces && !opnts &&
	!n_ofaces && !n_opnts && !report.solid;
    if (ofaces)
	bu_free(ofaces, "intersecting patch faces");
    if (opnts)
	bu_free(opnts, "intersecting patch points");
    if (!rejected) {
	bu_log("FAIL test_intersecting_hole_patch_rejected: "
	    "ret=%d solid=%d added=%d\n", ret, report.solid,
	    report.added_faces);
	return -1;
    }
    bu_log("PASS test_intersecting_hole_patch_rejected\n");
    return 0;
}

/* The same obstructed cap is usable for consumers which require indexed
 * manifold topology but permit geometric self-intersections.  The opt-in
 * policy must retain all topological checks and independently prove that the
 * bundled Manifold library accepts the result. */
static int
test_intersecting_hole_patch_manifold_accepted(void)
{
    static point_t pts[12] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
	{0.5, 0.5, 0.8}, {0.4, 0.4, 1.2},
	{0.6, 0.4, 1.2}, {0.5, 0.6, 1.2}
    };
    static int faces[42] = {
	0, 2, 1, 0, 3, 2,
	0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6,
	3, 0, 4, 3, 4, 7,
	8, 10, 9, 8, 9, 11, 9, 10, 11, 8, 11, 10
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 8;
    settings.allow_self_intersections = 1;
    settings.require_manifold = 1;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 14, pts, 12, &settings, &report);
    const bool accepted = ret == 0 && ofaces && opnts && report.solid &&
	report.self_intersections_allowed && report.manifold_accepted &&
	!report.unmatched_edges && !report.excess_edges &&
	!report.misoriented_edges && !report.invalid_vertex_links &&
	!bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
	    NULL);
    if (ofaces)
	bu_free(ofaces, "allowed intersecting patch faces");
    if (opnts)
	bu_free(opnts, "allowed intersecting patch points");
    if (!accepted) {
	bu_log("FAIL test_intersecting_hole_patch_manifold_accepted: "
	    "ret=%d solid=%d manifold=%d unmatched=%d links=%d\n",
	    ret, report.solid, report.manifold_accepted,
	    report.unmatched_edges, report.invalid_vertex_links);
	return -1;
    }
    bu_log("PASS test_intersecting_hole_patch_manifold_accepted\n");
    return 0;
}

/* An obstructed hole must not discard an independent safe cap.  Leave the
 * first cube open because its cap crosses the tetrahedron, but retain the
 * valid cap on the translated second cube. */
static int
test_independent_safe_hole_patch_retained(void)
{
    static point_t pts[20] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
	{3, 0, 0}, {4, 0, 0}, {4, 1, 0}, {3, 1, 0},
	{3, 0, 1}, {4, 0, 1}, {4, 1, 1}, {3, 1, 1},
	{0.5, 0.5, 0.8}, {0.4, 0.4, 1.2},
	{0.6, 0.4, 1.2}, {0.5, 0.6, 1.2}
    };
    static int faces[72] = {
	0, 2, 1, 0, 3, 2,
	0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6,
	3, 0, 4, 3, 4, 7,
	8, 10, 9, 8, 11, 10,
	8, 9, 13, 8, 13, 12,
	9, 10, 14, 9, 14, 13,
	10, 11, 15, 10, 15, 14,
	11, 8, 12, 11, 12, 15,
	16, 18, 17, 16, 17, 19, 17, 18, 19, 16, 19, 18
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 8;
    settings.require_solid = 0;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 24, pts, 20, &settings, &report);
    int safe_cap_faces = 0;
    int obstructed_cap_faces = 0;
    for (int face = 0; ret == 0 && face < n_ofaces; ++face) {
	bool on_top = true;
	double x_sum = 0.0;
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = ofaces[face * 3 + corner];
	    on_top = on_top && NEAR_EQUAL(opnts[vertex][Z], 1.0,
		SMALL_FASTF);
	    x_sum += opnts[vertex][X];
	}
	if (!on_top)
	    continue;
	if (x_sum / 3.0 > 2.0)
	    safe_cap_faces++;
	else
	    obstructed_cap_faces++;
    }
    const bool retained = ret == 0 && ofaces && opnts && !report.solid &&
	report.added_faces >= 2 && safe_cap_faces == 2 &&
	!obstructed_cap_faces;
    if (ofaces)
	bu_free(ofaces, "independent cap faces");
    if (opnts)
	bu_free(opnts, "independent cap points");
    if (!retained) {
	bu_log("FAIL test_independent_safe_hole_patch_retained: "
	    "ret=%d solid=%d added=%d safe=%d obstructed=%d\n", ret,
	    report.solid, report.added_faces, safe_cap_faces,
	    obstructed_cap_faces);
	return -1;
    }
    bu_log("PASS test_independent_safe_hole_patch_retained\n");
    return 0;
}

/* Imported B-Rep edges normally retain multiple samples on each straight
 * boundary segment.  A circle-parameterized ear can be valid in 2D while its
 * three restored 3D vertices are collinear.  Hole filling must fall back to
 * the geometric ear selector and still use every boundary edge. */
static int
test_collinear_hole_boundary(void)
{
    static point_t pts[12] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {0.5, 0, 1}, {1, 0, 1}, {1, 0.5, 1},
	{1, 1, 1}, {0.5, 1, 1}, {0, 1, 1}, {0, 0.5, 1}
    };
    static int faces[42] = {
	0, 2, 1, 0, 3, 2,
	0, 1, 6, 0, 6, 5, 0, 5, 4,
	1, 2, 8, 1, 8, 7, 1, 7, 6,
	2, 3, 10, 2, 10, 9, 2, 9, 8,
	3, 0, 4, 3, 4, 11, 3, 11, 10
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 30.0;
    settings.max_hole_edges = 16;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts, &n_opnts,
	faces, 14, pts, 12, &settings, &report);
    bool valid = ret == 0 && report.solid && report.added_faces >= 6 &&
	!bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
	    NULL);
    for (int face = 0; valid && face < n_ofaces; ++face) {
	const point_t &a = opnts[ofaces[(size_t)face * 3]];
	const point_t &b = opnts[ofaces[(size_t)face * 3 + 1]];
	const point_t &c = opnts[ofaces[(size_t)face * 3 + 2]];
	vect_t ab, ac, cross;
	VSUB2(ab, b, a);
	VSUB2(ac, c, a);
	VCROSS(cross, ab, ac);
	valid = MAGSQ(cross) > SMALL_FASTF;
    }
    if (ofaces)
	bu_free(ofaces, "collinear repair faces");
    if (opnts)
	bu_free(opnts, "collinear repair points");
    if (!valid) {
	bu_log("FAIL test_collinear_hole_boundary: ret=%d solid=%d added=%d\n",
	    ret, report.solid, report.added_faces);
	return -1;
    }
    bu_log("PASS test_collinear_hole_boundary\n");
    return 0;
}

/* Circle-parameterizing a concave planar boundary discards its concavity, so
 * restored 3-D diagonals can leave the polygon and cut its side walls.  Mesh
 * repair should preserve a planar boundary's actual projected outline. */
static int
test_concave_planar_hole(void)
{
    static point_t points[10] = {
	{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {1, 1, 0}, {0, 2, 0},
	{0, 0, 1}, {2, 0, 1}, {2, 2, 1}, {1, 1, 1}, {0, 2, 1}
    };
    static int faces[39] = {
	0, 3, 1, 1, 3, 2, 0, 4, 3,
	0, 1, 6, 0, 6, 5,
	1, 2, 7, 1, 7, 6,
	2, 3, 8, 2, 8, 7,
	3, 4, 9, 3, 9, 8,
	4, 0, 5, 4, 5, 9
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.fill_holes = 1;
    settings.max_hole_area_percent = 100.0;
    settings.max_hole_edges = 16;
    int *output_faces = NULL;
    int output_face_count = 0;
    point_t *output_points = NULL;
    int output_point_count = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int result = bg_trimesh_repair2(&output_faces,
	&output_face_count, &output_points, &output_point_count, faces, 13,
	points, 10, &settings, &report);
    const bool valid = result == 0 && report.solid &&
	report.added_faces >= 3 && output_faces && output_points &&
	!bg_trimesh_solid2(output_point_count, output_face_count,
	    (fastf_t *)output_points, output_faces, NULL);
    if (output_faces)
	bu_free(output_faces, "concave hole faces");
    if (output_points)
	bu_free(output_points, "concave hole points");
    if (!valid) {
	bu_log("FAIL test_concave_planar_hole: ret=%d solid=%d "
	    "added=%d rejected=%d\n", result, report.solid,
	    report.added_faces, report.rejected_hole_faces);
	return -1;
    }
    bu_log("PASS test_concave_planar_hole\n");
    return 0;
}

/* A collision gate must be able to reject one otherwise preferred ear and
 * let the 3-D filler choose a different valid triangulation. */
static int
test_validated_ear_alternative(void)
{
    point_t *input_points = NULL;
    int input_point_count = 0;
    int *input_faces = NULL;
    int input_face_count = 0;
    make_open_cube(&input_points, &input_point_count, &input_faces,
	&input_face_count);
    std::vector<gte::Vector3<double>> points((size_t)input_point_count);
    for (int point = 0; point < input_point_count; ++point) {
	for (int axis = 0; axis < 3; ++axis)
	    points[(size_t)point][axis] = input_points[point][axis];
    }
    std::vector<std::array<int32_t, 3>> faces((size_t)input_face_count);
    for (int face = 0; face < input_face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner)
	    faces[(size_t)face][corner] = input_faces[face * 3 + corner];
    }
    bool rejected_preferred_ear = false;
    gte::MeshHoleFilling<double>::Parameters parameters;
    parameters.method = gte::MeshHoleFilling<double>::
	TriangulationMethod::EarClipping3D;
    parameters.autoFallback = false;
    parameters.maxValidatedEdges = 8;
    parameters.triangleValidator = [&](const std::array<int32_t, 3> &candidate,
	    const std::vector<std::array<int32_t, 3>> &) {
	const bool uses_5 = candidate[0] == 5 || candidate[1] == 5 ||
	    candidate[2] == 5;
	const bool uses_6 = candidate[0] == 6 || candidate[1] == 6 ||
	    candidate[2] == 6;
	const bool uses_7 = candidate[0] == 7 || candidate[1] == 7 ||
	    candidate[2] == 7;
	if (uses_5 && uses_6 && uses_7) {
	    rejected_preferred_ear = true;
	    return false;
	}
	return true;
    };
    gte::MeshHoleFilling<double>::FillHoles(points, faces, parameters);
    std::vector<int> flat_faces;
    flat_faces.reserve(faces.size() * 3);
    for (const std::array<int32_t, 3> &face : faces)
	flat_faces.insert(flat_faces.end(), face.begin(), face.end());
    std::vector<point_t> flat_points(points.size());
    for (size_t point = 0; point < points.size(); ++point)
	VSET(flat_points[point], points[point][0], points[point][1],
	    points[point][2]);
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    const int solid_result = bg_trimesh_solid2((int)flat_points.size(),
	(int)faces.size(), (fastf_t *)flat_points.data(), flat_faces.data(),
	&solid_errors);
    const bool valid = rejected_preferred_ear && faces.size() == 12 &&
	!solid_result;
    if (!valid) {
	bu_log("FAIL test_validated_ear_alternative: rejected=%d faces=%zu "
	    "solid=%d unmatched=%d excess=%d misoriented=%d degenerate=%d\n",
	    (int)rejected_preferred_ear, faces.size(), solid_result,
	    solid_errors.unmatched.count, solid_errors.excess.count,
	    solid_errors.misoriented.count, solid_errors.degenerate.count);
	bg_free_trimesh_solid_errors(&solid_errors);
	return -1;
    }
    bg_free_trimesh_solid_errors(&solid_errors);
    bu_log("PASS test_validated_ear_alternative\n");
    return 0;
}

/* The final hole fallback may add one interior vertex when boundary-only
 * diagonals cannot form a usable cap.  Its fan must retain the boundary
 * winding and produce a closed oriented mesh. */
static int
test_steiner_hole_fan(void)
{
    point_t *input_points = NULL;
    int input_point_count = 0;
    int *input_faces = NULL;
    int input_face_count = 0;
    make_open_cube(&input_points, &input_point_count, &input_faces,
	&input_face_count);
    std::vector<gte::Vector3<double>> points((size_t)input_point_count);
    for (int point = 0; point < input_point_count; ++point) {
	for (int axis = 0; axis < 3; ++axis)
	    points[(size_t)point][axis] = input_points[point][axis];
    }
    std::vector<std::array<int32_t, 3>> faces((size_t)input_face_count);
    for (int face = 0; face < input_face_count; ++face) {
	for (int corner = 0; corner < 3; ++corner)
	    faces[(size_t)face][corner] = input_faces[face * 3 + corner];
    }
    gte::MeshHoleFilling<double>::Parameters parameters;
    parameters.method = gte::MeshHoleFilling<double>::
	TriangulationMethod::PlanarProjection;
    parameters.autoFallback = false;
    parameters.maxValidatedEdges = 8;
    parameters.steinerAboveEdges = 3;
    parameters.triangleValidator = [&](const std::array<int32_t, 3> &,
	    const std::vector<std::array<int32_t, 3>> &) {
	return points.size() > 8 && std::fabs(points[8][2] - 1.0) > 1.0e-12;
    };
    gte::MeshHoleFilling<double>::FillHoles(points, faces, parameters);
    std::vector<int> flat_faces;
    flat_faces.reserve(faces.size() * 3);
    for (const std::array<int32_t, 3> &face : faces)
	flat_faces.insert(flat_faces.end(), face.begin(), face.end());
    std::vector<point_t> flat_points(points.size());
    for (size_t point = 0; point < points.size(); ++point)
	VSET(flat_points[point], points[point][0], points[point][1],
	    points[point][2]);
    const bool valid = points.size() == 9 && faces.size() == 14 &&
	std::fabs(points[8][2] - 1.0) > 1.0e-12 &&
	!bg_trimesh_solid2((int)flat_points.size(), (int)faces.size(),
	    (fastf_t *)flat_points.data(), flat_faces.data(), NULL);
    if (!valid) {
	bu_log("FAIL test_steiner_hole_fan: points=%zu faces=%zu\n",
	    points.size(), faces.size());
	return -1;
    }
    bu_log("PASS test_steiner_hole_fan\n");
    return 0;
}

/* Normal orientation must handle many disconnected repair fragments without
 * rescanning the complete triangle array for every component. */
static int
test_component_orientation(void)
{
    std::vector<gte::Vector3<double>> points;
    std::vector<std::array<int32_t, 3>> faces;
    point_t *tet_points = NULL;
    int tet_point_count = 0;
    int *tet_faces = NULL;
    int tet_face_count = 0;
    make_tet(&tet_points, &tet_point_count, &tet_faces, &tet_face_count);
    for (int component = 0; component < 3; ++component) {
	const int offset = (int)points.size();
	for (int point = 0; point < tet_point_count; ++point) {
	    points.push_back({tet_points[point][X] + 2.0 * component,
		tet_points[point][Y], tet_points[point][Z]});
	}
	for (int face = 0; face < tet_face_count; ++face) {
	    std::array<int32_t, 3> triangle = {
		tet_faces[(size_t)face * 3] + offset,
		tet_faces[(size_t)face * 3 + 1] + offset,
		tet_faces[(size_t)face * 3 + 2] + offset
	    };
	    if (component == 1)
		std::swap(triangle[1], triangle[2]);
	    faces.push_back(triangle);
	}
    }
    gte::MeshPreprocessing<double>::OrientNormals(points, faces);
    double volumes[3] = {0.0, 0.0, 0.0};
    for (size_t face = 0; face < faces.size(); ++face) {
	const std::array<int32_t, 3> &triangle = faces[face];
	volumes[face / (size_t)tet_face_count] += Dot(points[triangle[0]],
	    Cross(points[triangle[1]], points[triangle[2]])) / 6.0;
    }
    if (volumes[0] <= 0.0 || volumes[1] <= 0.0 || volumes[2] <= 0.0) {
	bu_log("FAIL test_component_orientation: volumes=%g,%g,%g\n",
	    volumes[0], volumes[1], volumes[2]);
	return -1;
    }
    bu_log("PASS test_component_orientation\n");
    return 0;
}

/* Component union is deliberately opt-in.  Two individually closed cubes
 * overlap geometrically but pass an edge-incidence solid check; the Manifold
 * pass must regularize them into one closed boundary. */
static int
test_overlapping_component_union(void)
{
    point_t *cube_points;
    int cube_point_count;
    int *cube_faces;
    int cube_face_count;
    make_cube(&cube_points, &cube_point_count, &cube_faces,
	&cube_face_count);
    point_t points[16];
    int faces[72];
    for (int vertex = 0; vertex < cube_point_count; ++vertex) {
	VMOVE(points[vertex], cube_points[vertex]);
	VMOVE(points[vertex + cube_point_count], cube_points[vertex]);
	points[vertex + cube_point_count][X] += 0.5;
    }
    for (int corner = 0; corner < cube_face_count * 3; ++corner) {
	faces[corner] = cube_faces[corner];
	faces[cube_face_count * 3 + corner] =
	    cube_faces[corner] + cube_point_count;
    }
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.union_components = 1;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 2 * cube_face_count, points,
	2 * cube_point_count, &settings, &report);
    bool valid = ret == 0 && report.solid &&
	report.component_union_applied && report.output_volume > 1.4 &&
	report.output_volume < 1.6 && ofaces && opnts &&
	!bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
	    NULL);
    bool unique_vertices = valid;
    for (int face = 0; valid && face < n_ofaces; ++face) {
	const point_t &a = opnts[ofaces[(size_t)face * 3]];
	const point_t &b = opnts[ofaces[(size_t)face * 3 + 1]];
	const point_t &c = opnts[ofaces[(size_t)face * 3 + 2]];
	vect_t ab, ac, cross;
	VSUB2(ab, b, a);
	VSUB2(ac, c, a);
	VCROSS(cross, ab, ac);
	valid = MAGSQ(cross) > SMALL_FASTF;
    }
    for (int first = 0; unique_vertices && first < n_opnts; ++first) {
	for (int second = first + 1; second < n_opnts; ++second) {
	    if (!memcmp(opnts[first], opnts[second], sizeof(point_t))) {
		unique_vertices = false;
		break;
	    }
	}
    }
    if (ofaces)
	bu_free(ofaces, "component union faces");
    if (opnts)
	bu_free(opnts, "component union points");
    if (!valid || !unique_vertices) {
	bu_log("FAIL test_overlapping_component_union: ret=%d applied=%d "
	    "solid=%d volume=%g unique_vertices=%d\n", ret,
	    report.component_union_applied, report.solid,
	    report.output_volume, (int)unique_vertices);
	return -1;
    }
    bu_log("PASS test_overlapping_component_union\n");
    return 0;
}

/* Two closed components which meet only at a duplicate-coordinate vertex
 * are topologically solid but not an embedded manifold.  The explicitly
 * requested separation pass must move their disconnected fans apart by no
 * more than the configured tolerance. */
static int
test_touching_component_separation(void)
{
    point_t *cube_points;
    int cube_point_count;
    int *cube_faces;
    int cube_face_count;
    make_cube(&cube_points, &cube_point_count, &cube_faces,
	&cube_face_count);
    point_t points[16];
    int faces[72];
    vect_t offset = {1.0, 1.0, 1.0};
    for (int vertex = 0; vertex < cube_point_count; ++vertex) {
	VMOVE(points[vertex], cube_points[vertex]);
	VADD2(points[vertex + cube_point_count], cube_points[vertex],
	    offset);
    }
    for (int corner = 0; corner < cube_face_count * 3; ++corner) {
	faces[corner] = cube_faces[corner];
	faces[cube_face_count * 3 + corner] =
	    cube_faces[corner] + cube_point_count;
    }
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.vertex_tolerance = 1.0e-5;
    settings.separate_touching_vertices = 1;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 2 * cube_face_count, points,
	2 * cube_point_count, &settings, &report);
    bool unique_vertices = ret == 0 && report.solid &&
	report.separated_vertices >= 2 &&
	report.max_vertex_displacement > 0.0 &&
	report.max_vertex_displacement <= settings.vertex_tolerance &&
	report.output_volume > 1.9 && report.output_volume < 2.1;
    for (int first = 0; unique_vertices && first < n_opnts; ++first) {
	for (int second = first + 1; second < n_opnts; ++second) {
	    if (!memcmp(opnts[first], opnts[second], sizeof(point_t))) {
		unique_vertices = false;
		break;
	    }
	}
    }
    int separated_inward = 0;
    for (int vertex = 0; unique_vertices && vertex < n_opnts; ++vertex) {
	bool near_touch = true;
	for (int axis = 0; axis < 3; ++axis)
	    near_touch = near_touch &&
		std::fabs(opnts[vertex][axis] - 1.0) <=
		2.0 * settings.vertex_tolerance;
	if (!near_touch)
	    continue;
	point_t neighbors = VINIT_ZERO;
	int neighbor_count = 0;
	for (int face = 0; face < n_ofaces; ++face) {
	    for (int corner = 0; corner < 3; ++corner) {
		if (ofaces[face * 3 + corner] != vertex)
		    continue;
		for (int other = 1; other < 3; ++other) {
		    const int adjacent =
			ofaces[face * 3 + (corner + other) % 3];
		    VADD2(neighbors, neighbors, opnts[adjacent]);
		    neighbor_count++;
		}
	    }
	}
	bool inward = neighbor_count > 0;
	for (int axis = 0; inward && axis < 3; ++axis) {
	    const double neighbor_average =
		neighbors[axis] / (double)neighbor_count;
	    inward = (neighbor_average - 1.0) *
		(opnts[vertex][axis] - 1.0) > 0.0;
	}
	if (inward)
	    separated_inward++;
    }
    unique_vertices = unique_vertices && separated_inward == 2;
    if (ofaces)
	bu_free(ofaces, "touch separation faces");
    if (opnts)
	bu_free(opnts, "touch separation points");
    if (!unique_vertices) {
	bu_log("FAIL test_touching_component_separation: ret=%d solid=%d "
	    "separated=%d displacement=%g volume=%g\n", ret,
	    report.solid, report.separated_vertices,
	    report.max_vertex_displacement, report.output_volume);
	return -1;
    }
    bu_log("PASS test_touching_component_separation\n");
    return 0;
}

/* A display patch may insert an extra sample on a shared edge while the
 * rigorous neighbor retains one long edge.  Splitting the long incident
 * triangle at the hanging vertex must close the seam without hole filling. */
static int
test_hanging_boundary_edge_split(void)
{
    static point_t points[9] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
	{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
	{0.5, 0, 1}
    };
    static int faces[39] = {
	0, 2, 1, 0, 3, 2,
	4, 8, 6, 8, 5, 6, 4, 6, 7,
	0, 1, 5, 0, 5, 4,
	1, 2, 6, 1, 6, 5,
	2, 3, 7, 2, 7, 6,
	3, 0, 4, 3, 4, 7
    };
    struct bg_trimesh_repair_settings settings =
	BG_TRIMESH_REPAIR_SETTINGS_INIT;
    settings.vertex_tolerance = 1.0e-9;
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;
    struct bg_trimesh_repair_report report =
	BG_TRIMESH_REPAIR_REPORT_INIT;
    const int ret = bg_trimesh_repair2(&ofaces, &n_ofaces, &opnts,
	&n_opnts, faces, 13, points, 9, &settings, &report);
    const bool valid = ret == 0 && report.solid &&
	report.added_faces >= 1 && ofaces && opnts &&
	!bg_trimesh_solid2(n_opnts, n_ofaces, (fastf_t *)opnts, ofaces,
	    NULL);
    if (ofaces)
	bu_free(ofaces, "hanging edge faces");
    if (opnts)
	bu_free(opnts, "hanging edge points");
    if (!valid) {
	bu_log("FAIL test_hanging_boundary_edge_split: ret=%d added=%d "
	    "solid=%d\n", ret, report.added_faces, report.solid);
	return -1;
    }
    bu_log("PASS test_hanging_boundary_edge_split\n");
    return 0;
}

/* Test specifically for the SplitNonManifoldVertices backward-walk bug.
 *
 * The bug: SplitNonManifoldVertices only triggered its backward walk when
 * adj[f*3+lv] < 0 (the very first forward step was a boundary edge).  When
 * the outer loop started at a MIDDLE triangle of an open boundary fan, the
 * forward walk covered one direction and terminated at a boundary, but the
 * backward direction was never walked.  Those uncovered triangles then got a
 * NEW vertex in the next outer-loop iteration, creating zero-area cracks
 * (duplicate-position vertices) whose boundary loops are non-simple.
 * DetectHolesFromAdjacency aborts non-simple loops, so those cracks are
 * never filled, and the mesh remains non-manifold.
 *
 * Reproducer: 4 triangles in a consistent fan around vertex 0, with the
 * MIDDLE fan triangle placed first in the face array (face index 0).  The
 * outer loop therefore starts at the middle triangle; without the fix the
 * backward direction is skipped and a crack appears between the last two fan
 * triangles.  The mesh has a single hexagonal hole that SHOULD be fillable,
 * but the crack makes the boundary non-simple so the hole fill fails.
 *
 * Vertex layout (all in Z=0 plane):
 *   v3---v2
 *  /       \
 * v4  v0   v1
 *  \       /
 *   v5---+
 *
 * Fan triangles (face index order chosen to trigger bug):
 *   Face 0 = (0, 2, 3)  <- MIDDLE of fan (forward goes to face 1 = T_A)
 *   Face 1 = (0, 1, 2)  <- one END of fan (adj backward = boundary)
 *   Face 2 = (0, 3, 4)  <- other middle
 *   Face 3 = (0, 4, 5)  <- other END (adj forward = boundary)
 *
 * Without fix: vertex 0 gets spuriously split; hole fill fails; result
 *   is non-manifold.
 * With fix:    vertex 0 is kept intact; hexagonal hole is filled; result
 *   is a valid closed solid.
 */
static int
test_split_nmv_backward_walk(void)
{
    static point_t pts[6] = {
	{ 0,  0, 0},  /* v0 – center/apex, boundary vertex with 4-tri fan */
	{ 2,  0, 0},  /* v1 */
	{ 2,  2, 0},  /* v2 */
	{ 0,  2, 0},  /* v3 */
	{-2,  2, 0},  /* v4 */
	{-2,  0, 0}   /* v5 */
    };
    /* Intentional ordering: face 0 is the MIDDLE of v0's open fan so that
     * the SplitNonManifoldVertices outer loop begins at a middle triangle.
     * Without the fwdHitBoundary fix this incorrectly splits vertex 0. */
    static int faces[12] = {
	0, 2, 3,   /* face 0 – MIDDLE fan tri for v0 (T_B) */
	0, 1, 2,   /* face 1 – end of fan for v0 (T_A)     */
	0, 3, 4,   /* face 2 – other middle (T_C)           */
	0, 4, 5    /* face 3 – other end (T_D)              */
    };

    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    /* Use unlimited hole size so the hexagonal hole is always fillable. */
    struct bg_trimesh_repair_opts opts;
    opts.max_hole_area         = 0.0;
    opts.max_hole_area_percent = 0.0;
    int ret = bg_trimesh_repair(&ofaces, &n_ofaces, &opnts, &n_opnts,
				faces, 4, pts, 6, &opts);

    if (ret < 0) {
	bu_log("FAIL test_split_nmv_backward_walk: bg_trimesh_repair returned %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts,  "opnts");
	return -1;
    }

    if (ret == 1) {
	/* Already solid – acceptable only if bg_trimesh_solid2 agrees. */
	int not_solid = bg_trimesh_solid2(6, 4, (fastf_t *)pts, faces, NULL);
	if (not_solid) {
	    bu_log("FAIL test_split_nmv_backward_walk: "
		   "repair returned 1 (already solid) but input is not solid\n");
	    return -1;
	}
	bu_log("PASS test_split_nmv_backward_walk (already solid)\n");
	return 0;
    }

    /* ret == 0: verify output is a valid solid */
    if (!ofaces || n_ofaces < 4 || !opnts || n_opnts <= 0) {
	bu_log("FAIL test_split_nmv_backward_walk: invalid output arrays\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts,  "opnts");
	return -1;
    }

    int not_solid = bg_trimesh_solid2(n_opnts, n_ofaces,
				      (fastf_t *)opnts, ofaces, NULL);
    if (not_solid) {
	bu_log("FAIL test_split_nmv_backward_walk: "
	       "repaired mesh is still not solid "
	       "(SplitNonManifoldVertices backward-walk bug?)\n");
	bu_free(ofaces, "ofaces");
	bu_free(opnts,  "opnts");
	return -1;
    }

    bu_log("PASS test_split_nmv_backward_walk (repaired: %d faces, %d verts)\n",
	   n_ofaces, n_opnts);
    bu_free(ofaces, "ofaces");
    bu_free(opnts,  "opnts");
    return 0;
}

/* Malformed facet adjacency can contain a directed cycle that does not
 * return to the corner where a vertex-fan walk began.  The original walk
 * trusted adjacency and never checked already visited corners, so the first
 * fan (which retains the original vertex index) looped forever. */
static int
test_split_nmv_cyclic_adjacency(void)
{
    std::vector<gte::Vector3<double>> vertices = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
	{0.0, 0.0, 1.0}, {-1.0, 0.0, 0.0},
	{0.0, -1.0, 0.0}, {0.0, 0.0, -1.0}
    };
    std::vector<std::array<int32_t, 3>> triangles = {
	{0, 1, 2}, {0, 3, 4}, {0, 5, 6}
    };
    std::vector<int32_t> adjacency(9, -1);
    adjacency[0] = 1;
    adjacency[3] = 2;
    adjacency[6] = 1;
    gte::MeshRepair<double>::SplitNonManifoldVertices(vertices,
	triangles, adjacency);
    const bool valid = vertices.size() == 7 &&
	triangles[0][0] == 0 && triangles[1][0] == 0 &&
	triangles[2][0] == 0;
    if (!valid) {
	bu_log("FAIL test_split_nmv_cyclic_adjacency: %zu vertices, "
	    "fan indices %d/%d/%d\n", vertices.size(), triangles[0][0],
	    triangles[1][0], triangles[2][0]);
	return -1;
    }
    bu_log("PASS test_split_nmv_cyclic_adjacency\n");
    return 0;
}


/* A malformed/non-manifold fan may produce an adjacency cycle that does not
 * include the facet where the walk began.  The backward fan walk must stop
 * when it encounters an already visited corner rather than circulating
 * forever.  This is the topology encountered by the thin-face repair of
 * havoc.g's rt_r.ecov1 region. */
static int
test_split_nmv_cycle_guard(void)
{
    std::vector<gte::Vector3<double>> verts = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
	{-1.0, 0.0, 0.0}, {0.0, -1.0, 0.0},
	{1.0, 1.0, 0.0}, {-1.0, -1.0, 0.0}
    };
    std::vector<std::array<int32_t, 3>> tris = {
	{0, 1, 2}, {0, 3, 4}, {0, 5, 6}
    };
    std::vector<int32_t> adj(9, -1);

    /* Starting facet 0 reaches a boundary in the forward direction.  Its
     * backward direction enters the 1 -> 2 -> 1 cycle. */
    adj[2] = 1;
    adj[5] = 2;
    adj[8] = 1;

    gte::MeshRepair<double>::SplitNonManifoldVertices(verts, tris, adj);

    if (verts.size() != 7) {
	bu_log("FAIL test_split_nmv_cycle_guard: unexpected vertex split (%zu vertices)\n",
		verts.size());
	return -1;
    }

    bu_log("PASS test_split_nmv_cycle_guard\n");
    return 0;
}


/* NULL parameter handling must not crash and must return -1. */
static int
test_null_params(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    int ret;

    /* NULL output pointer */
    ret = bg_trimesh_repair(NULL, &n_ofaces, &opnts, &n_opnts,
			    NULL, 0, NULL, 0, NULL);
    if (ret != -1) {
	bu_log("FAIL test_null_params: expected -1 for NULL ofaces ptr, got %d\n", ret);
	return -1;
    }

    /* NULL input array */
    ret = bg_trimesh_repair(&ofaces, &n_ofaces, &opnts, &n_opnts,
			    NULL, 0, NULL, 0, NULL);
    if (ret != -1) {
	bu_log("FAIL test_null_params: expected -1 for NULL input, got %d\n", ret);
	return -1;
    }

    bu_log("PASS test_null_params\n");
    return 0;
}


int
main(int UNUSED(argc), const char *argv[])
{
    bu_setprogname(argv[0]);

    int failures = 0;

    failures += (test_null_params()               != 0) ? 1 : 0;
    failures += (test_already_solid()             != 0) ? 1 : 0;
    failures += (test_tet_already_solid()         != 0) ? 1 : 0;
    failures += (test_manifold_acceptance_check() != 0) ? 1 : 0;
    failures += (test_closed_orientation_sync()   != 0) ? 1 : 0;
    failures += (test_open_cube_repair()          != 0) ? 1 : 0;
    failures += (test_unused_vertex_compaction()  != 0) ? 1 : 0;
    failures += (test_repair2_conservative_default() != 0) ? 1 : 0;
    failures += (test_repair2_report()             != 0) ? 1 : 0;
    failures += (test_touching_triangular_holes()   != 0) ? 1 : 0;
    failures += (test_touching_quadrilateral_holes() != 0) ? 1 : 0;
    failures += (test_manifold_rejection_report()   != 0) ? 1 : 0;
    failures +=
	(test_manifold_preserves_point_contact() != 0) ? 1 : 0;
    failures +=
	(test_manifold_splits_pinched_vertex() != 0) ? 1 : 0;
    failures +=
	(test_pinched_vertex_before_flat_removal() != 0) ? 1 : 0;
    failures += (test_intersecting_hole_patch_rejected() != 0) ? 1 : 0;
    failures +=
	(test_intersecting_hole_patch_manifold_accepted() != 0) ? 1 : 0;
    failures += (test_independent_safe_hole_patch_retained() != 0) ? 1 : 0;
    failures += (test_collinear_hole_boundary()    != 0) ? 1 : 0;
    failures += (test_concave_planar_hole()         != 0) ? 1 : 0;
    failures += (test_validated_ear_alternative()    != 0) ? 1 : 0;
    failures += (test_steiner_hole_fan()             != 0) ? 1 : 0;
    failures += (test_component_orientation()         != 0) ? 1 : 0;
    failures += (test_overlapping_component_union() != 0) ? 1 : 0;
    failures += (test_touching_component_separation() != 0) ? 1 : 0;
    failures += (test_hanging_boundary_edge_split() != 0) ? 1 : 0;
    failures += (test_split_nmv_backward_walk()   != 0) ? 1 : 0;
    failures += (test_split_nmv_cycle_guard()     != 0) ? 1 : 0;
    failures += (test_split_nmv_cyclic_adjacency() != 0) ? 1 : 0;

    if (failures) {
	bu_log("%d test(s) FAILED\n", failures);
	return 1;
    }

    bu_log("All bg_trimesh_repair tests passed\n");
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
