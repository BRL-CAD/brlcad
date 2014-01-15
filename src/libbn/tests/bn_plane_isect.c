/*                 B N _ P L A N E _ I S E C T . C
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
test_bn_isect_line_lseg(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_t;
    fastf_t actual_t;
    point_t p = VINIT_ZERO;
    vect_t d = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;
    struct bn_tol tol = TOL_INIT;

    if (argc != 8) {
	bu_exit(1, "ERROR: input format is Px,Py,Pz Dx,Dy,Dz Ax,Ay,Az Bx,By,Bz expected_return expected_t [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &d[X], &d[Y], &d[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &a[X], &a[Y], &a[Z]);
    sscanf(argv[5], "%lf,%lf,%lf", &b[X], &b[Y], &b[Z]);
    sscanf(argv[6], "%d", &expected_return);
    sscanf(argv[7], "%lf", &expected_t);

    actual_return = bn_isect_line_lseg(&actual_t, p, d, a, b, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("t: %f\n", actual_t);

    return !(expected_return == actual_return
	     && NEAR_EQUAL(expected_t, actual_t, BN_TOL_DIST));
}


static int
test_bn_isect_lseg3_lseg3(int argc, char **argv)
{
    int expected_return = 0;
    int actual_return = 0;
    fastf_t expected_dist[2] = {0.0, 0.0};
    fastf_t actual_dist[2] = {0.0, 0.0};
    point_t p = VINIT_ZERO;
    vect_t pdir = VINIT_ZERO;
    point_t q = VINIT_ZERO;
    vect_t qdir = VINIT_ZERO;
    struct bn_tol tol = TOL_INIT;

    if (argc != 9) {
	bu_exit(1, "ERROR: input format is Px,Py,Pz PDIRx,PDIRy,PDIRz Qx,Qy,Qz QDIRx,QDIRy,QDIRz expected_return expected_dist_0 expected_dist_1 [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf,%lf,%lf", &p[X], &p[Y], &p[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &pdir[X], &pdir[Y], &pdir[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &q[X], &q[Y], &q[Z]);
    sscanf(argv[5], "%lf,%lf,%lf", &qdir[X], &qdir[Y], &qdir[Z]);
    sscanf(argv[6], "%d", &expected_return);
    sscanf(argv[7], "%lf", &expected_dist[0]);
    sscanf(argv[8], "%lf", &expected_dist[1]);

    actual_return = bn_isect_lseg3_lseg3(actual_dist, p, pdir, q, qdir, &tol);

    bu_log("return: %d\n", actual_return);
    bu_log("dist[0]: %f\n", actual_dist[0]);
    bu_log("dist[1]: %f\n", actual_dist[1]);

    return !(expected_return == actual_return
	     && NEAR_EQUAL(expected_dist[0], actual_dist[0], BN_TOL_DIST)
	     && NEAR_EQUAL(expected_dist[1], actual_dist[1], BN_TOL_DIST));
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
	    return test_bn_isect_line_lseg(argc, argv);
        case 2:
	    return test_bn_isect_lseg3_lseg3(argc, argv);
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
