/*                           S A T . C
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "bg.h"

void
obb_arb(point_t c, vect_t E[3]) {
    point_t arb[8];
    vect_t NE[3];
    VSCALE(NE[0], E[0], -1);
    VSCALE(NE[1], E[1], -1);
    VSCALE(NE[2], E[2], -1);
    VADD4(arb[0], c, E[0], E[1], E[2]);
    VADD4(arb[1], c, E[0], NE[1], E[2]);
    VADD4(arb[2], c, E[0], NE[1], NE[2]);
    VADD4(arb[3], c, E[0], E[1], NE[2]);
    VADD4(arb[4], c, NE[0], E[1], E[2]);
    VADD4(arb[5], c, NE[0], NE[1], E[2]);
    VADD4(arb[6], c, NE[0], NE[1], NE[2]);
    VADD4(arb[7], c, NE[0], E[1], NE[2]);
    bu_log("in obb.s arb8 %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n", V3ARGS(arb[0]), V3ARGS(arb[1]), V3ARGS(arb[2]), V3ARGS(arb[3]), V3ARGS(arb[4]), V3ARGS(arb[5]), V3ARGS(arb[6]), V3ARGS(arb[7]));
}

void
line_pts(point_t origin, vect_t dir)
{
    point_t p;
    VADD2(p, origin, dir);
    bu_log("in p1.s sph %f %f %f 0.01\nin p2.s sph %f %f %f 0.01\n", V3ARGS(origin), V3ARGS(p));
}

int
line_aabb_test(int expected, point_t origin, vect_t ldir,
	point_t aabb_c, vect_t aabb_e)
{
    if (bg_sat_line_aabb(origin, ldir, aabb_c, aabb_e) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("LINE - origin: %f %f %f  dir: %f %f %f\n", V3ARGS(origin), V3ARGS(ldir));
	bu_log("AABB -      c: %f %f %f  ext: %f %f %f\n", V3ARGS(aabb_c), V3ARGS(aabb_e));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_line_aabb(origin, ldir, aabb_c, aabb_e);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
line_obb_test(int expected, point_t origin, vect_t ldir,
	point_t obb_c, vect_t obb_e1, vect_t obb_e2, vect_t obb_e3)
{
    if (bg_sat_line_obb(origin, ldir, obb_c, obb_e1, obb_e2, obb_e3) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("LINE - origin: %f %f %f  dir: %f %f %f\n", V3ARGS(origin), V3ARGS(ldir));
	bu_log(" OBB - c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb_c), V3ARGS(obb_e1), V3ARGS(obb_e2), V3ARGS(obb_e3));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_line_obb(origin, ldir, obb_c, obb_e1, obb_e2, obb_e3);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
tri_aabb_test(
	int expected,
	point_t v1, point_t v2, point_t v3,
	point_t aabb_c, vect_t aabb_e)
{
    if (bg_sat_tri_aabb(v1, v2, v3, aabb_c, aabb_e) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("TRI  - v1: %f %f %f  v2: %f %f %f v3: %f %f %f\n", V3ARGS(v1), V3ARGS(v2), V3ARGS(v3));
	bu_log("AABB -  c: %f %f %f  ext: %f %f %f\n", V3ARGS(aabb_c), V3ARGS(aabb_e));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_tri_aabb(v1, v2, v3, aabb_c, aabb_e);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
tri_obb_test(
	int expected,
	point_t v1, point_t v2, point_t v3,
	point_t obb_c, vect_t obb_e1, vect_t obb_e2, vect_t obb_e3
	)
{
    if (bg_sat_tri_obb(v1, v2, v3, obb_c, obb_e1, obb_e2, obb_e3) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("TRI - v1: %f %f %f  v2: %f %f %f v3: %f %f %f\n", V3ARGS(v1), V3ARGS(v2), V3ARGS(v3));
	bu_log("OBB -  c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb_c), V3ARGS(obb_e1), V3ARGS(obb_e2), V3ARGS(obb_e3));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_tri_obb(v1, v2, v3, obb_c, obb_e1, obb_e2, obb_e3);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
abb_obb_test(
	int expected,
	point_t aabb_min, point_t aabb_max,
	point_t obb_c, vect_t obb_e1, vect_t obb_e2, vect_t obb_e3
	)
{

    if (bg_sat_aabb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("AABB - min: %f %f %f  max: %f %f %f\n", V3ARGS(aabb_min), V3ARGS(aabb_max));
	bu_log(" OBB -   c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb_c), V3ARGS(obb_e1), V3ARGS(obb_e2), V3ARGS(obb_e3));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_aabb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
obb_obb_test(
	int expected,
	point_t obb1_c, vect_t obb1_e1, vect_t obb1_e2, vect_t obb1_e3,
	point_t obb2_c, vect_t obb2_e1, vect_t obb2_e2, vect_t obb2_e3
	)
{
    if (bg_sat_obb_obb(obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("OBB1 - c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb1_c), V3ARGS(obb1_e1), V3ARGS(obb1_e2), V3ARGS(obb1_e3));
	bu_log("OBB2 - c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb2_c), V3ARGS(obb2_e1), V3ARGS(obb2_e2), V3ARGS(obb2_e3));
	// Repeat the operation so an attached debugger can follow what happened
	bg_sat_obb_obb(obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
	bu_exit(1, "test failure\n");
    }
    return 0;
}

#define NO_ISECT 0
#define ISECT 1

void
line_abb_run_tests()
{
    point_t bcenter;
    point_t origin;
    vect_t dir, bextent;

    VSET(bcenter, 0, 0, 0);
    VSET(bextent, 2, 2, 2);
    VSET(origin, 0, 0, 0);

    VSET(dir, 1, 0, 0);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 1, 0);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 0, 1);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);

    VSET(dir, 1, 1, 1);
    VUNITIZE(dir);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0.5, 0.5, 2);
    VUNITIZE(dir);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);


    VSET(origin, -2, 0, 0);
    VSET(dir, 1, 0, 0);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 1, 0);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 0, 1);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);

    VSET(origin, -2.1, 0, 0);

    VSET(dir, 1, 0, 0);
    line_aabb_test(ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 1, 0);
    line_aabb_test(NO_ISECT, origin, dir, bcenter, bextent);
    VSET(dir, 0, 0, 1);
    line_aabb_test(NO_ISECT, origin, dir, bcenter, bextent);

    mat_t rmat;
    vect_t r;
    bn_mat_angles(rmat, 20, 50, 30);
    VSET(dir, 1, 0, 0);
    MAT3X3VEC(r, rmat, dir);
    VSET(origin, -3.597, 0, 0);
    line_aabb_test(ISECT, origin, r, bcenter, bextent);
    VSET(origin, -3.598, 0, 0);
    line_aabb_test(NO_ISECT, origin, r, bcenter, bextent);
}

void
line_obb_run_tests()
{
    point_t bcenter;
    point_t origin;
    vect_t dir, AE[3], E[3];

    VSET(bcenter, 0, 0, 0);
    VSET(AE[0], 2, 0, 0);
    VSET(AE[1], 0, 2, 0);
    VSET(AE[2], 0, 0, 2);

    mat_t rmat;
    bn_mat_angles(rmat, 20, 50, 30);
    MAT3X3VEC(E[0], rmat, AE[0]);
    MAT3X3VEC(E[1], rmat, AE[1]);
    MAT3X3VEC(E[2], rmat, AE[2]);

    //obb_arb(bcenter, E);

    vect_t r;
    bn_mat_angles(rmat, 10, 10, 10);
    VSET(dir, 1, 0, 0);
    MAT3X3VEC(r, rmat, dir);
    VSET(origin, -3, 0, 0);
    line_obb_test(ISECT, origin, r, bcenter, E[0], E[1], E[2]);
    VSET(origin, -3, 0, -2.80);
    line_obb_test(ISECT, origin, r, bcenter, E[0], E[1], E[2]);
    VSET(origin, -3, 0, -2.81);
    //line_pts(origin, r);
    line_obb_test(NO_ISECT, origin, r, bcenter, E[0], E[1], E[2]);
}

// Under the hood this is the obb test - just run it here to make sure the
// wires stay connected.  Heavy duty testing in the obb version.
void
tri_aabb_run_tests()
{

}

void
tri_obb_run_tests()
{

}

void
aabb_obb_run_tests()
{
    point_t aabb_min, aabb_max;
    vect_t obb_c, obb_e1, obb_e2, obb_e3;

    // Start simple - box at origin
    VSET(aabb_min, -2, -2, -2);
    VSET(aabb_max, 2, 2, 2);
    VSET(obb_c, 0, 0, 0);
    VSET(obb_e1, 2, 0, 0);
    VSET(obb_e2, 0, 2, 0);
    VSET(obb_e3, 0, 0, 2);

    // Start out with some trivial checks - an axis-aligned test box, and move
    // the center point to trigger different responses
    abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);

    for (int i = -4; i <= 4; i++) {
	for (int j = -4; j <= 4; j++) {
	    for (int k = -4; k <= 4; k++) {
		VSET(obb_c, i, j, k);
		abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
	    }
	}
    }

    // Check no-isect on all sides
    VSET(obb_c, -9, 0, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, -9, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 0, -9);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 9, 0, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 9, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 0, 9);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);

    // Rotate obb vectors.  First, check that intersection is detected at
    // a variety of rotation angles.
    mat_t rmat;
    VSET(obb_c, 0, 0, 0);
    for (int alpha = 0; alpha < 90; alpha++) {
	for (int beta = 0; beta < 90; beta++) {
	    for (int gamma = 0; gamma < 90; gamma++) {
		vect_t r1, r2, r3;
		bn_mat_angles(rmat, (double)alpha, (double)beta, (double)gamma);
		MAT3X3VEC(r1, rmat, obb_e1);
		MAT3X3VEC(r2, rmat, obb_e2);
		MAT3X3VEC(r3, rmat, obb_e3);
		abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	    }
	}
    }
    // Check that non-intersection is detected at a variety of rotation angles.
    VSET(obb_c, 0, 0, 10);
    for (int alpha = 0; alpha < 90; alpha++) {
	for (int beta = 0; beta < 90; beta++) {
	    for (int gamma = 0; gamma < 90; gamma++) {
		vect_t r1, r2, r3;
		bn_mat_angles(rmat, (double)alpha, (double)beta, (double)gamma);
		MAT3X3VEC(r1, rmat, obb_e1);
		MAT3X3VEC(r2, rmat, obb_e2);
		MAT3X3VEC(r3, rmat, obb_e3);
		abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	    }
	}
    }

    // Check no-isect detection with a rotated obb on all sides
    {
	vect_t r1, r2, r3;
	bn_mat_angles(rmat, 30, 10, 50);
	MAT3X3VEC(r1, rmat, obb_e1);
	MAT3X3VEC(r2, rmat, obb_e2);
	MAT3X3VEC(r3, rmat, obb_e3);
	VSET(obb_c, -9, 0, 0);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 0, -9, 0);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 0, 0, -9);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 9, 0, 0);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 0, 9, 0);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 0, 0, 9);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
    }


    // Hand-constructed tests
    {
	vect_t r1, r2, r3;
	bn_mat_angles(rmat, 30, 50, 10);
	MAT3X3VEC(r1, rmat, obb_e1);
	MAT3X3VEC(r2, rmat, obb_e2);
	MAT3X3VEC(r3, rmat, obb_e3);
	VSET(obb_c, 4, 0, 0);
	abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 5, 0, 0);
	abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 5.02, 0, 0);
	abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
	VSET(obb_c, 5.022, 0, 0);
	abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, r1, r2, r3);
    }
}

void
obb_obb_run_tests()
{
    vect_t obb1_c, obb1_e1, obb1_e2, obb1_e3;
    vect_t obb2_c, obb2_e1, obb2_e2, obb2_e3;

    // Start simple - boxes at origin
    VSET(obb1_c, 0, 0, 0);
    VSET(obb1_e1, 2, 0, 0);
    VSET(obb1_e2, 0, 2, 0);
    VSET(obb1_e3, 0, 0, 2);
    VSET(obb2_c, 0, 0, 0);
    VSET(obb2_e1, 2, 0, 0);
    VSET(obb2_e2, 0, 2, 0);
    VSET(obb2_e3, 0, 0, 2);

    // Start out with some trivial checks - move the center point to trigger
    // different responses
    obb_obb_test(ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);

    for (int i = -4; i <= 4; i++) {
	for (int j = -4; j <= 4; j++) {
	    for (int k = -4; k <= 4; k++) {
		VSET(obb1_c, i, j, k);
		obb_obb_test(ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
	    }
	}
    }

    // Check no-isect on all sides
    VSET(obb1_c, -9, 0, 0);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
    VSET(obb1_c, 0, -9, 0);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
    VSET(obb1_c, 0, 0, -9);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
    VSET(obb1_c, 9, 0, 0);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
    VSET(obb1_c, 0, 9, 0);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);
    VSET(obb1_c, 0, 0, 9);
    obb_obb_test(NO_ISECT, obb1_c, obb1_e1, obb1_e2, obb1_e3, obb2_c, obb2_e1, obb2_e2, obb2_e3);

    // Rotate obb1 and obb2 vectors.  First, check that intersection is detected at
    // a variety of rotation angles.
    mat_t rmat;
    VSET(obb1_c, 0, 0, 0);
    VSET(obb2_c, 0, 0, 0);
    for (int alpha = 0; alpha < 90; alpha++) {
	for (int beta = 0; beta < 90; beta++) {
	    for (int gamma = 0; gamma < 90; gamma++) {
		vect_t r1, r2, r3, r4, r5, r6;
		bn_mat_angles(rmat, (double)alpha, (double)beta, (double)gamma);
		MAT3X3VEC(r1, rmat, obb1_e1);
		MAT3X3VEC(r2, rmat, obb1_e2);
		MAT3X3VEC(r3, rmat, obb1_e3);
		bn_mat_angles(rmat, (double)beta, (double)gamma, (double)alpha);
		MAT3X3VEC(r4, rmat, obb2_e1);
		MAT3X3VEC(r5, rmat, obb2_e2);
		MAT3X3VEC(r6, rmat, obb2_e3);
		obb_obb_test(ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	    }
	}
    }
    // Check that non-intersection is detected at a variety of rotation angles.
    VSET(obb1_c, 0, 0, 10);
    VSET(obb2_c, 0, 0, 0);
    for (int alpha = 0; alpha < 90; alpha++) {
	for (int beta = 0; beta < 90; beta++) {
	    for (int gamma = 0; gamma < 90; gamma++) {
		vect_t r1, r2, r3, r4, r5, r6;
		bn_mat_angles(rmat, (double)alpha, (double)beta, (double)gamma);
		MAT3X3VEC(r1, rmat, obb1_e1);
		MAT3X3VEC(r2, rmat, obb1_e2);
		MAT3X3VEC(r3, rmat, obb1_e3);
		bn_mat_angles(rmat, (double)beta, (double)gamma, (double)alpha);
		MAT3X3VEC(r4, rmat, obb2_e1);
		MAT3X3VEC(r5, rmat, obb2_e2);
		MAT3X3VEC(r6, rmat, obb2_e3);
		obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	    }
	}
    }

    // Check no-isect detection with rotated obbs on all sides
    {
	vect_t r1, r2, r3, r4, r5, r6;
	VSET(obb2_c, 0, 0, 0);
	bn_mat_angles(rmat, 30, 10, 50);
	MAT3X3VEC(r1, rmat, obb1_e1);
	MAT3X3VEC(r2, rmat, obb1_e2);
	MAT3X3VEC(r3, rmat, obb1_e3);
	bn_mat_angles(rmat, 10, 50, 30);
	MAT3X3VEC(r4, rmat, obb2_e1);
	MAT3X3VEC(r5, rmat, obb2_e2);
	MAT3X3VEC(r6, rmat, obb2_e3);
	VSET(obb1_c, -9, 0, 0);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb1_c, 0, -9, 0);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb1_c, 0, 0, -9);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb1_c, 9, 0, 0);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb1_c, 0, 9, 0);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb1_c, 0, 0, 9);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
    }

    // Hand-constructed tests
    {
	vect_t r1, r2, r3, r4, r5, r6;
	bn_mat_angles(rmat, 40, 15, 20);
	MAT3X3VEC(r1, rmat, obb1_e1);
	MAT3X3VEC(r2, rmat, obb1_e2);
	MAT3X3VEC(r3, rmat, obb1_e3);
	VSET(obb1_c, 3, 0, 0);
	bn_mat_angles(rmat, 5, 12, 72);
	MAT3X3VEC(r4, rmat, obb2_e1);
	MAT3X3VEC(r5, rmat, obb2_e2);
	MAT3X3VEC(r6, rmat, obb2_e3);
	VSET(obb2_c, 0, 0, 0);
	obb_obb_test(ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb2_c, 7, 0, 0);
	obb_obb_test(ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb2_c, 7, 0, -3);
	obb_obb_test(ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb2_c, 7, 0, -4);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb2_c, 7, 0, -5);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
	VSET(obb2_c, 7, 0, -10);
	obb_obb_test(NO_ISECT, obb1_c, r1, r2, r3, obb2_c, r4, r5, r6);
    }
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: %s does not accept arguments\n", argv[0]);

    line_abb_run_tests();
    line_obb_run_tests();
    aabb_obb_run_tests();
    obb_obb_run_tests();

    bu_log("OK\n");
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
