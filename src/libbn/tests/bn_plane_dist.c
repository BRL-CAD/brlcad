/*                 B N _ P L A N E _ D I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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


#define TOL_INIT { BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 }


static int
test_bn_dist_pt3_line3(int argc, char **argv)
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
    struct bn_tol tol = TOL_INIT;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az DIRx,DIRy,DIRz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &dir[X], &dir[Y], &dir[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist);
    sscanf(argv[7], "%lf,%lf,%lf", &expected_pca[X], &expected_pca[Y], &expected_pca[Z]);

    actual_return = bn_dist_pt3_line3(&actual_dist, actual_pca, a, p, dir, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %lf\n", actual_dist);
    bu_log("pca: %lf,%lf,%lf\n", actual_pca[X], actual_pca[Y], actual_pca[Z]);

    return (expected_return != actual_return
	    && !NEAR_EQUAL(expected_dist,actual_dist, BN_TOL_DIST)
 	    && !NEAR_EQUAL(expected_pca[X], actual_pca[X], BN_TOL_DIST)
	    && !NEAR_EQUAL(expected_pca[Y], actual_pca[Y], BN_TOL_DIST)
	    && !NEAR_EQUAL(expected_pca[Z], actual_pca[Z], BN_TOL_DIST));
}


static int
test_bn_dist_pt3_lseg3(int argc, char **argv)
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
    struct bn_tol tol = TOL_INIT;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Ax,Ay,Az Bx,By,Bz Px,Py,Pz expected_return expected_dist PCAx,PCAy,PCAz [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[5], "%d", &expected_return);
    sscanf(argv[6], "%lf", &expected_dist);
    sscanf(argv[7], "%lf,%lf,%lf", &expected_pca[X], &expected_pca[Y], &expected_pca[Z]);

    actual_return = bn_dist_pt3_lseg3(&actual_dist, actual_pca, a, b, p, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist: %lf\n", actual_dist);
    bu_log("pca: %lf,%lf,%lf\n", actual_pca[X], actual_pca[Y], actual_pca[Z]);

    return (expected_return != actual_return
	    && !NEAR_EQUAL(expected_dist,actual_dist, BN_TOL_DIST)
 	    && !NEAR_EQUAL(expected_pca[X], actual_pca[X], BN_TOL_DIST)
	    && !NEAR_EQUAL(expected_pca[Y], actual_pca[Y], BN_TOL_DIST)
	    && !NEAR_EQUAL(expected_pca[Z], actual_pca[Z], BN_TOL_DIST));
}


static int
test_bn_dist_pt3_pt3(int argc, char **argv)
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

    actual_return = bn_dist_pt3_pt3(a, b);

    bu_log("return: %.30lf\n", actual_return);
    bu_log("er: %.30lf\n", expected_return);
    bu_log("retval: %d\n", NEAR_EQUAL(expected_return, actual_return, BN_TOL_DIST));
    bu_log("error: %f\n", fabs(expected_return-actual_return)/expected_return);

    return !NEAR_EQUAL(expected_return, actual_return, BN_TOL_DIST);
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);
    if (function_num < 1 || function_num > 5)
	function_num = 0;

    switch (function_num) {
        case 1:
	    return test_bn_dist_pt3_line3(argc, argv);
        case 2:
	    return test_bn_dist_pt3_lseg3(argc, argv);
        case 3:
	    return test_bn_dist_pt3_pt3(argc, argv);
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
