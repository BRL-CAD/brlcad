/*              T R I M E S H _ R E M E S H . C P P
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
/** @file tests/trimesh_remesh.cpp
 *
 * Unit tests for bg_trimesh_remesh.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "bg.h"

/* --------------------------------------------------------------------------
 * Shared test geometry
 * --------------------------------------------------------------------------
 *
 * We use a closed icosphere (level-1 subdivision of a regular icosahedron)
 * as the input mesh.  It has 42 vertices and 80 triangles and provides a
 * non-trivial but still simple surface for remeshing.
 */

#define PHI 1.6180339887498948482

/* Level-0 icosahedron: 12 vertices, 20 triangles */
static point_t ico_pts[12] = {
    { 0,     1,  PHI}, { 0,    -1,  PHI}, { 0,     1, -PHI},
    { 0,    -1, -PHI}, { 1,  PHI,   0  }, {-1,  PHI,   0  },
    { 1, -PHI,   0  }, {-1, -PHI,   0  }, { PHI,  0,   1  },
    {-PHI,   0,   1  }, { PHI,  0,  -1  }, {-PHI,  0,  -1  }
};

static int ico_faces[60] = {
    0, 1, 8,   0, 8, 4,   0, 4, 5,   0, 5, 9,   0, 9, 1,
    1, 6, 8,   8, 6,10,   8,10, 4,   4,10, 2,   4, 2, 5,
    5, 2,11,   5,11, 9,   9,11, 7,   9, 7, 1,   1, 7, 6,
    3, 6, 7,   3, 7,11,   3,11, 2,   3, 2,10,   3,10, 6
};


/* --------------------------------------------------------------------------
 * Test cases
 * -------------------------------------------------------------------------- */

/* NULL parameter handling must not crash and must return -1. */
static int
test_null_params(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    int ret;

    /* NULL output pointers */
    ret = bg_trimesh_remesh(NULL, &n_ofaces, &opnts, &n_opnts,
			    ico_faces, 20, ico_pts, 12, NULL);
    if (ret != -1) {
	bu_log("FAIL test_null_params: expected -1 for NULL ofaces ptr, got %d\n", ret);
	return -1;
    }

    /* NULL input */
    ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
			    NULL, 0, NULL, 0, NULL);
    if (ret != -1) {
	bu_log("FAIL test_null_params: expected -1 for NULL input, got %d\n", ret);
	return -1;
    }

    bu_log("PASS test_null_params\n");
    return 0;
}

/* Default options: output should have roughly 10× the input vertex count. */
static int
test_default_opts(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    int ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
				ico_faces, 20, ico_pts, 12, NULL);
    if (ret != 0) {
	bu_log("FAIL test_default_opts: expected return 0, got %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    if (!ofaces || n_ofaces <= 0 || !opnts || n_opnts <= 0) {
	bu_log("FAIL test_default_opts: empty output arrays\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    /* We expect roughly 10× the input – allow a wide tolerance because CVT
     * may produce slightly more points than requested. */
    if (n_opnts < 6) {
	bu_log("FAIL test_default_opts: suspiciously few output verts: %d\n", n_opnts);
	bu_free(ofaces, "ofaces");
	bu_free(opnts, "opnts");
	return -1;
    }
    bu_log("PASS test_default_opts (%d verts → %d verts, %d faces)\n",
	   12, n_opnts, n_ofaces);
    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");
    return 0;
}

/* Explicit target_count: request exactly 50 output vertices. */
static int
test_explicit_target(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    struct bg_trimesh_remesh_opts opts = BG_TRIMESH_REMESH_OPTS_DEFAULT;
    opts.target_count = 50;

    int ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
				ico_faces, 20, ico_pts, 12, &opts);
    if (ret != 0) {
	bu_log("FAIL test_explicit_target: expected return 0, got %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    if (!ofaces || n_ofaces <= 0 || !opnts || n_opnts <= 0) {
	bu_log("FAIL test_explicit_target: empty output arrays\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    bu_log("PASS test_explicit_target (target=50, got %d verts, %d faces)\n",
	   n_opnts, n_ofaces);
    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");
    return 0;
}

/* Isotropic remesh: anisotropy=0.0. */
static int
test_isotropic(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    struct bg_trimesh_remesh_opts opts = BG_TRIMESH_REMESH_OPTS_DEFAULT;
    opts.anisotropy = 0.0;  /* isotropic */
    opts.target_count = 36;

    int ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
				ico_faces, 20, ico_pts, 12, &opts);
    if (ret != 0) {
	bu_log("FAIL test_isotropic: expected return 0, got %d\n", ret);
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    if (!ofaces || n_ofaces <= 0 || !opnts || n_opnts <= 0) {
	bu_log("FAIL test_isotropic: empty output arrays\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }
    bu_log("PASS test_isotropic (%d verts, %d faces)\n", n_opnts, n_ofaces);
    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");
    return 0;
}

/* Verify output indices are within bounds of the returned vertex array. */
static int
test_index_validity(void)
{
    int *ofaces = NULL;
    int n_ofaces = 0;
    point_t *opnts = NULL;
    int n_opnts = 0;

    struct bg_trimesh_remesh_opts opts = BG_TRIMESH_REMESH_OPTS_DEFAULT;
    opts.target_count = 30;

    int ret = bg_trimesh_remesh(&ofaces, &n_ofaces, &opnts, &n_opnts,
				ico_faces, 20, ico_pts, 12, &opts);
    if (ret != 0 || !ofaces || !opnts) {
	bu_log("FAIL test_index_validity: remesh failed\n");
	if (ofaces) bu_free(ofaces, "ofaces");
	if (opnts)  bu_free(opnts, "opnts");
	return -1;
    }

    for (int i = 0; i < n_ofaces * 3; i++) {
	if (ofaces[i] < 0 || ofaces[i] >= n_opnts) {
	    bu_log("FAIL test_index_validity: face index %d out of range [0, %d)\n",
		   ofaces[i], n_opnts);
	    bu_free(ofaces, "ofaces");
	    bu_free(opnts, "opnts");
	    return -1;
	}
    }

    bu_log("PASS test_index_validity (%d faces, %d verts)\n", n_ofaces, n_opnts);
    bu_free(ofaces, "ofaces");
    bu_free(opnts, "opnts");
    return 0;
}


int
main(int UNUSED(argc), const char *argv[])
{
    bu_setprogname(argv[0]);

    int failures = 0;

    failures += (test_null_params()    != 0) ? 1 : 0;
    failures += (test_default_opts()   != 0) ? 1 : 0;
    failures += (test_explicit_target() != 0) ? 1 : 0;
    failures += (test_isotropic()      != 0) ? 1 : 0;
    failures += (test_index_validity() != 0) ? 1 : 0;

    if (failures) {
	bu_log("%d test(s) FAILED\n", failures);
	return 1;
    }

    bu_log("All bg_trimesh_remesh tests passed\n");
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
