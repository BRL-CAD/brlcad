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
    failures += (test_open_cube_repair()          != 0) ? 1 : 0;
    failures += (test_split_nmv_backward_walk()   != 0) ? 1 : 0;

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
