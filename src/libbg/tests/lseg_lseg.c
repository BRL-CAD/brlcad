/*                   L S E G _ L S E G . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2026 United States Government as represented by
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

static int
check_lseg_distance(const char *name,
		    const point_t p0, const point_t p1,
		    const point_t q0, const point_t q1,
		    double expected_dist_sq,
		    const point_t expected_c1, const point_t expected_c2)
{
    point_t actual_c1 = VINIT_ZERO;
    point_t actual_c2 = VINIT_ZERO;
    double actual_dist_sq = bg_distsq_lseg3_lseg3(&actual_c1, &actual_c2, p0, p1, q0, q1);

    if (NEAR_EQUAL(expected_dist_sq, actual_dist_sq, BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c1[X], actual_c1[X], BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c1[Y], actual_c1[Y], BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c1[Z], actual_c1[Z], BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c2[X], actual_c2[X], BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c2[Y], actual_c2[Y], BN_TOL_DIST)
	&& NEAR_EQUAL(expected_c2[Z], actual_c2[Z], BN_TOL_DIST)) {
	return 0;
    }

    bu_log("%s: distance squared %g, closest points (%g, %g, %g) and (%g, %g, %g)\n",
	   name, actual_dist_sq, V3ARGS(actual_c1), V3ARGS(actual_c2));
    return 1;
}


int
main(int argc, char **argv)
{
    int test_num = 0;
    point_t p0 = VINIT_ZERO;
    point_t p1 = VINIT_ZERO;
    point_t q0 = VINIT_ZERO;
    point_t q1 = VINIT_ZERO;
    point_t expected_c1 = VINIT_ZERO;
    point_t expected_c2 = VINIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc != 2)
	bu_exit(1, "ERROR: [%s] input format is: test_number\n", argv[0]);

    sscanf(argv[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    VSET(p0, 0.0, 0.0, 0.0);
	    VSET(p1, 2.0, 0.0, 0.0);
	    VSET(q0, 1.0, -1.0, 0.0);
	    VSET(q1, 1.0, 1.0, 0.0);
	    VSET(expected_c1, 1.0, 0.0, 0.0);
	    VSET(expected_c2, 1.0, 0.0, 0.0);
	    return check_lseg_distance("crossing", p0, p1, q0, q1, 0.0, expected_c1, expected_c2);
	case 2:
	    VSET(p0, 0.0, 0.0, 0.0);
	    VSET(p1, 2.0, 0.0, 0.0);
	    VSET(q0, 1.0, 1.0, 3.0);
	    VSET(q1, 1.0, 1.0, 5.0);
	    VSET(expected_c1, 1.0, 0.0, 0.0);
	    VSET(expected_c2, 1.0, 1.0, 3.0);
	    return check_lseg_distance("skew", p0, p1, q0, q1, 10.0, expected_c1, expected_c2);
	case 3:
	    VSET(p0, 0.0, 0.0, 0.0);
	    VSET(p1, 1.0, 0.0, 0.0);
	    VSET(q0, 3.0, 0.0, 0.0);
	    VSET(q1, 4.0, 0.0, 0.0);
	    VSET(expected_c1, 1.0, 0.0, 0.0);
	    VSET(expected_c2, 3.0, 0.0, 0.0);
	    return check_lseg_distance("disjoint", p0, p1, q0, q1, 4.0, expected_c1, expected_c2);
	case 4:
	    VSET(p0, 0.0, 0.0, 0.0);
	    VSET(p1, 0.0, 0.0, 0.0);
	    VSET(q0, 1.0, 0.0, 0.0);
	    VSET(q1, 1.0, 1.0, 0.0);
	    VSET(expected_c1, 0.0, 0.0, 0.0);
	    VSET(expected_c2, 1.0, 0.0, 0.0);
	    return check_lseg_distance("degenerate", p0, p1, q0, q1, 1.0, expected_c1, expected_c2);
    }

    bu_log("Error: unknown test number %d\n", test_num);
    return -1;
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
