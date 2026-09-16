/*                 P L A N E _ D I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "bn.h"

static int
test_bg_dist_pnt3_line3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist;
    fastf_t actual_dist;
    point_t expected_pca = VINIT_ZERO;
    point_t actual_pca = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    vect_t dir = VINIT_ZERO;
    point_t p = VINIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az DIRx,DIRy,DIRz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &dir[X], &dir[Y], &dir[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist);
    sscanf(argv[7], "%lf,%lf,%lf", &expected_pca[X], &expected_pca[Y], &expected_pca[Z]);

    actual_return = bg_dist_pnt3_line3(&actual_dist, actual_pca, a, dir, p, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %lf\n", actual_dist);
    bu_log("pca: %lf,%lf,%lf\n", actual_pca[X], actual_pca[Y], actual_pca[Z]);

    return (expected_return != actual_return
	    || !NEAR_EQUAL(expected_dist,actual_dist, BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[X], actual_pca[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[Y], actual_pca[Y], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[Z], actual_pca[Z], BN_TOL_DIST));
}


static int
test_bg_dist_line3_line3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist[2] = {0.0, 0.0};
    fastf_t actual_dist[2] = {0.0, 0.0};
    point_t p1 = VINIT_ZERO;
    vect_t d1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    vect_t d2 = VINIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (argc != 9) {
	bu_exit(1, "ERROR: input format is P1x,P1y,P1z D1x,D1y,D1z P2x,P2y,P2z D2x,D2y,D2z expected_return expected_dist_0 expected_dist_1 [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p1[X], &p1[Y], &p1[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &d1[X], &d1[Y], &d1[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p2[X], &p2[Y], &p2[Z]);
    sscanf(argv[5], "%lf,%lf,%lf", &d2[X], &d2[Y], &d2[Z]);
    sscanf(argv[6], "%d", &expected_return);
    sscanf(argv[7], "%lf", &expected_dist[0]);
    sscanf(argv[8], "%lf", &expected_dist[1]);

    actual_return = bg_dist_line3_line3(actual_dist, p1, d1, p2, d2, &tol);

    bu_log("return: %d\n", actual_return);
    if (actual_return >= 0) {
	bu_log("dist[0]: %lf\n", actual_dist[0]);
	bu_log("dist[1]: %lf\n", actual_dist[1]);
    }

    if (expected_return != actual_return)
	return 1;
    if (actual_return < 0)
	return 0;

    return (!NEAR_EQUAL(expected_dist[0], actual_dist[0], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_dist[1], actual_dist[1], BN_TOL_DIST));
}


static int
test_bg_dist_line3_lseg3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist[2] = {0.0, 0.0};
    fastf_t actual_dist[2] = {0.0, 0.0};
    point_t p = VINIT_ZERO;
    vect_t d = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (argc != 9) {
	bu_exit(1, "ERROR: input format is Px,Py,Pz Dx,Dy,Dz Ax,Ay,Az Bx,By,Bz expected_return expected_dist_0 expected_dist_1 [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &d[X], &d[Y], &d[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[5], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[6], "%d", &expected_return);
    sscanf(argv[7], "%lf", &expected_dist[0]);
    sscanf(argv[8], "%lf", &expected_dist[1]);

    actual_return = bg_dist_line3_lseg3(actual_dist, p, d, a, b, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist[0]: %lf\n", actual_dist[0]);
    bu_log("dist[1]: %lf\n", actual_dist[1]);

    return (expected_return != actual_return
	    || !NEAR_EQUAL(expected_dist[0], actual_dist[0], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_dist[1], actual_dist[1], BN_TOL_DIST));
}


static int
test_bg_distsq_line3_line3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist[3] = {0.0, 0.0, 0.0};
    fastf_t actual_dist[3] = {0.0, 0.0, 0.0};
    point_t p = VINIT_ZERO;
    vect_t d = VINIT_ZERO;
    point_t q = VINIT_ZERO;
    vect_t e = VINIT_ZERO;
    point_t expected_pt1 = VINIT_ZERO;
    point_t expected_pt2 = VINIT_ZERO;
    point_t actual_pt1 = VINIT_ZERO;
    point_t actual_pt2 = VINIT_ZERO;

    if (argc != 12) {
	bu_exit(1, "ERROR: input format is Px,Py,Pz Dx,Dy,Dz Qx,Qy,Qz Ex,Ey,Ez expected_return expected_dist_0 expected_dist_1 expected_dist_2 PT1x,PT1y,PT1z PT2x,PT2y,PT2z [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &d[X], &d[Y], &d[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &q[X], &q[Y], &q[Z]);
    sscanf(argv[5], "%lf,%lf,%lf", &e[X], &e[Y], &e[Z]);
    sscanf(argv[6], "%d", &expected_return);
    sscanf(argv[7], "%lf", &expected_dist[0]);
    sscanf(argv[8], "%lf", &expected_dist[1]);
    sscanf(argv[9], "%lf", &expected_dist[2]);
    sscanf(argv[10], "%lf,%lf,%lf", &expected_pt1[X], &expected_pt1[Y], &expected_pt1[Z]);
    sscanf(argv[11], "%lf,%lf,%lf", &expected_pt2[X], &expected_pt2[Y], &expected_pt2[Z]);

    actual_return = bg_distsq_line3_line3(actual_dist, p, d, q, e, actual_pt1, actual_pt2);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %lf,%lf,%lf\n", actual_dist[0], actual_dist[1], actual_dist[2]);
    bu_log("pt1: %lf,%lf,%lf\n", actual_pt1[X], actual_pt1[Y], actual_pt1[Z]);
    bu_log("pt2: %lf,%lf,%lf\n", actual_pt2[X], actual_pt2[Y], actual_pt2[Z]);

    return (expected_return != actual_return
	    || !NEAR_EQUAL(expected_dist[0], actual_dist[0], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_dist[1], actual_dist[1], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_dist[2], actual_dist[2], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt1[X], actual_pt1[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt1[Y], actual_pt1[Y], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt1[Z], actual_pt1[Z], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt2[X], actual_pt2[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt2[Y], actual_pt2[Y], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pt2[Z], actual_pt2[Z], BN_TOL_DIST));
}


static int
test_bg_dist_pnt3_lseg3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist;
    fastf_t actual_dist;
    point_t expected_pca = VINIT_ZERO;
    point_t actual_pca = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    point_t p = VINIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist);
    sscanf(argv[7], "%lf,%lf,%lf", &expected_pca[X], &expected_pca[Y], &expected_pca[Z]);

    actual_return = bg_dist_pnt3_lseg3(&actual_dist, actual_pca, a, b, p, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %lf\n", actual_dist);
    bu_log("pca: %lf,%lf,%lf\n", actual_pca[X], actual_pca[Y], actual_pca[Z]);

    return (expected_return != actual_return
	    || !NEAR_EQUAL(expected_dist,actual_dist, BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[X], actual_pca[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[Y], actual_pca[Y], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[Z], actual_pca[Z], BN_TOL_DIST));
}


static int
test_bg_dist_pnt2_lseg2(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist_sq = 0.0;
    fastf_t actual_dist_sq = 0.0;
    fastf_t expected_pca[2] = {0.0, 0.0};
    fastf_t actual_pca[2] = {0.0, 0.0};
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    point_t p = VINIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Ax,Ay Bx,By Px,Py expected_return expected_dist_sq PCAx,PCAy [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf", &a[X], &a[Y]);
    sscanf(argv[3], "%lf,%lf", &b[X], &b[Y]);
    sscanf(argv[4], "%lf,%lf", &p[X], &p[Y]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist_sq);
    sscanf(argv[7], "%lf,%lf", &expected_pca[X], &expected_pca[Y]);

    actual_return = bg_dist_pnt2_lseg2(&actual_dist_sq, actual_pca, a, b, p, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("distance squared: %lf\n", actual_dist_sq);
    bu_log("pca: %lf,%lf\n", actual_pca[X], actual_pca[Y]);

    return (expected_return != actual_return
	    || !NEAR_EQUAL(expected_dist_sq, actual_dist_sq, BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[X], actual_pca[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_pca[Y], actual_pca[Y], BN_TOL_DIST));
}


static int
test_bg_dist_pnt3_pnt3(int argc, char **argv)
{
    double expected_return = 0;
    double actual_return = 0;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az Bx,By,Bz expected_return [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[4], "%lf", &expected_return);

    actual_return = bg_dist_pnt3_pnt3(a, b);

    bu_log("return: %.30lf\n", actual_return);
    bu_log("er: %.30lf\n", expected_return);
    bu_log("retval: %d\n", NEAR_EQUAL(expected_return, actual_return, BN_TOL_DIST));
    bu_log("error: %f\n", fabs(expected_return-actual_return)/expected_return);

    return !NEAR_EQUAL(expected_return, actual_return, BN_TOL_DIST);
}


int
plane_dist_main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);
    if (function_num < 1 || function_num > 7)
	function_num = 0;

    switch (function_num) {
	case 1:
	    return test_bg_dist_pnt3_line3(argc, argv);
	case 2:
	    return test_bg_dist_pnt3_lseg3(argc, argv);
	case 3:
	    return test_bg_dist_pnt3_pnt3(argc, argv);
	case 4:
	    return test_bg_dist_line3_line3(argc, argv);
	case 5:
	    return test_bg_dist_line3_lseg3(argc, argv);
	case 6:
	    return test_bg_distsq_line3_line3(argc, argv);
	case 7:
	    return test_bg_dist_pnt2_lseg2(argc, argv);
    }
    return 1;
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
