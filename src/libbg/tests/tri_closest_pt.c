/*                 T R I _ C L O S E S T _ P T . C
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


int
main(int argc, char **argv)
{
    double expected_dist = 0.0;
    double actual_dist = 0.0;
    point_t V0 = VINIT_ZERO;
    point_t V1 = VINIT_ZERO;
    point_t V2 = VINIT_ZERO;
    point_t TP = VINIT_ZERO;
    point_t expected_closest = VINIT_ZERO;
    point_t actual_closest = VINIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc != 7)
	bu_exit(1, "ERROR: [%s] input format is TPx,TPy,TPz V0x,V0y,V0z V1x,V1y,V1z V2x,V2y,V2z expected_dist closest_x,closest_y,closest_z\n", argv[0]);

    sscanf(argv[1], "%lf,%lf,%lf", &TP[X], &TP[Y], &TP[Z]);
    sscanf(argv[2], "%lf,%lf,%lf", &V0[X], &V0[Y], &V0[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &V1[X], &V1[Y], &V1[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &V2[X], &V2[Y], &V2[Z]);
    sscanf(argv[5], "%lf", &expected_dist);
    sscanf(argv[6], "%lf,%lf,%lf", &expected_closest[X], &expected_closest[Y], &expected_closest[Z]);

    actual_dist = bg_tri_closest_pt(&actual_closest, TP, V0, V1, V2);

    bu_log("distance: %g\n", actual_dist);
    bu_log("closest point: %g,%g,%g\n", V3ARGS(actual_closest));

    return (!NEAR_EQUAL(expected_dist, actual_dist, BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_closest[X], actual_closest[X], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_closest[Y], actual_closest[Y], BN_TOL_DIST)
	    || !NEAR_EQUAL(expected_closest[Z], actual_closest[Z], BN_TOL_DIST));
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
