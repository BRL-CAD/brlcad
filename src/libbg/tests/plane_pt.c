/*                    P L A N E _ P T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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
test_bg_3pnts_collinear(int argc, char **argv)
{
    int expected_result = 0;
    int actual_result = 0;
    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    struct bn_tol tol = BG_TOL_INIT;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is P0x,P0y,P0z P1x,P1y,P1z P2x,P2y,P2z expected_result [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p0[X], &p0[Y], &p0[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &p1[X], &p1[Y], &p1[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p2[X], &p2[Y], &p2[Z]);
    sscanf(argv[5], "%d", &expected_result);

    actual_result = bg_3pnts_collinear(p0, p1, p2, &tol);

    bu_log("result: %d\n", actual_result);

    return (expected_result != actual_result);
}


static int
test_bg_3pnts_distinct(int argc, char **argv)
{
    int expected_result = 0;
    int actual_result = 0;
    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    struct bn_tol tol = BG_TOL_INIT;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is P0x,P0y,P0z P1x,P1y,P1z P2x,P2y,P2z expected_result [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p0[X], &p0[Y], &p0[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &p1[X], &p1[Y], &p1[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p2[X], &p2[Y], &p2[Z]);
    sscanf(argv[5], "%d", &expected_result);

    actual_result = bg_3pnts_distinct(p0, p1, p2, &tol);

    bu_log("result: %d\n", actual_result);

    return (expected_result != actual_result);
}


static int
test_bg_distsq_line3_pnt3(int argc, char **argv)
{
    float expected_result = 0;
    float actual_result = 0;
    point_t pt = VINIT_ZERO;
    vect_t dir = VINIT_ZERO;
    point_t a = VINIT_ZERO;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is PTx,PTy,PTz DIRx,DIRy,DIRz Ax,Ay,Az expected_result [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &pt[X], &pt[Y], &pt[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &dir[X], &dir[Y], &dir[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[5], "%f", &expected_result);

    actual_result = bg_distsq_line3_pnt3(pt, dir, a);

    bu_log("result: %f\n", actual_result);

    return !EQUAL(expected_result, actual_result);
}


static int
test_bg_distsq_pnt3_lseg3_v2(int argc, char **argv)
{
    int expected_return = 0;
    fastf_t expected_dist = 0;
    int actual_return = 0;
    fastf_t actual_dist = 0;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    point_t p = VINIT_ZERO;
    struct bn_tol tol = BG_TOL_INIT;

    if (argc != 7) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist);

    actual_return = bg_distsq_pnt3_lseg3_v2(&actual_dist, a, b, p, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %f\n", actual_dist);

    if (expected_return == actual_return && NEAR_EQUAL(expected_dist, actual_dist, BN_TOL_DIST)) {
	return 0;
    } else {
	return -1;
    }
}


static int
test_bg_make_plane_3pnts(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    point_t c = VINIT_ZERO;
    plane_t plane = HINIT_ZERO;
    struct bn_tol tol = BG_TOL_INIT;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az Bx,By,Bz Cx,Cy,Cz expected_return [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &c[X], &c[Y], &c[Z]);
    sscanf(argv[5], "%d", &expected_return);

    actual_return = bg_make_plane_3pnts(plane, a, b, c, &tol);

    bu_log("return: %d\n", actual_return);

    return (expected_return != actual_return);
}

static int
test_bg_plane_closest_pt(int argc, char **argv)
{
    point_t p = VINIT_ZERO;
    plane_t plane = HINIT_ZERO;

    argc--;argv++;
    argc--;argv++;

    if (argc != 2) {
	bu_exit(1, "ERROR: input format is Nx,Ny,Nz,Nw Px,Py,Pz\n");
    }

    sscanf(argv[0], "%lf,%lf,%lf,%lf", &plane[0], &plane[1], &plane[2], &plane[3]);
    sscanf(argv[1], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);

    fastf_t u, v;
    point_t npt;
    if (bg_plane_closest_pt(&u, &v, plane, p)) {
	bu_log("closest pt calculation failed\n");
	return -1;
    }
    if (bg_plane_pt_at(&npt, plane, u, v)) {
	bu_log("pt at u,v calculation failed\n");
	return -1;
    }

    if (!VNEAR_EQUAL(p, npt, VUNITIZE_TOL)) {
	bu_log("point differs after calculations!\n");
	bu_log("original : %.15f %.15f %.15f\n", V3ARGS(p));
	bu_log("post-calc: %.15f %.15f %.15f\n", V3ARGS(npt));
	return -1;
    }

    return 0;
}

static int
test_bg_coplanar_pts(int argc, char **argv)
{
    point_t pp[4] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    int is_coplanar = 0;
    struct bn_tol tol = BG_TOL_INIT;

    argc--;argv++;
    argc--;argv++;

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is P1x,P1y,P1z P2x,P2y,P2z P3x,P3y,P3z P4x,P4y,P4z [0,1]\n");
    }

    sscanf(argv[0], "%lf,%lf,%lf", &pp[0][X], &pp[0][Y], &pp[0][Z]);
    sscanf(argv[1], "%lf,%lf,%lf", &pp[1][X], &pp[1][Y], &pp[1][Z]);
    sscanf(argv[2], "%lf,%lf,%lf", &pp[2][X], &pp[2][Y], &pp[2][Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &pp[3][X], &pp[3][Y], &pp[3][Z]);

    if (BU_STR_EQUAL(argv[4], "1"))
	is_coplanar = 1;

    int cp_test = bg_coplanar_pts((const point_t *)pp, 4, &tol);
    if (cp_test < 0) {
	bu_log("bg_coplanar_pts ERROR\n");
	return -1;
    }

    if (is_coplanar != cp_test) {
	bu_log("unexpected coplanar test answer!\n");
	return 1;
    }

    return 0;
}

int
plane_pt_main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    return test_bg_3pnts_collinear(argc, argv);
	case 2:
	    return test_bg_3pnts_distinct(argc, argv);
	case 3:
	    return test_bg_distsq_line3_pnt3(argc, argv);
	case 4:
	    return test_bg_distsq_pnt3_lseg3_v2(argc, argv);
	case 5:
	    return test_bg_make_plane_3pnts(argc, argv);
	case 6:
	    return test_bg_plane_closest_pt(argc, argv);
	case 7:
	    return test_bg_coplanar_pts(argc, argv);
	default:
	    return 1;

    }

    // Shouldn't get here
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
