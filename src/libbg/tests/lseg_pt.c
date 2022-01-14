/*                      L S E G _ P T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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

// Unless we want to require more precise input for expected values,
// be fairly loose with the validation tolerance
#define DCHECK_TOL 1e-5

int
main(int argc, char **argv)
{
    point_t P0, P1, Q, E, C;
    double edist, cdist;

    bu_setprogname(argv[0]);

    if (argc != 6)
	bu_exit(1, "ERROR: [%s] input format is: P0x,P0y,P0z P1x,P1y,P1z Qx,Qy,Qz Ex,Ey,Ez expected_dist \n", argv[0]);

    sscanf(argv[1], "%lf,%lf,%lf", &P0[X], &P0[Y], &P0[Z]);
    sscanf(argv[2], "%lf,%lf,%lf", &P1[X], &P1[Y], &P1[Z]);
    sscanf(argv[3], "%lf,%lf,%lf", &Q[X], &Q[Y], &Q[Z]);
    sscanf(argv[4], "%lf,%lf,%lf", &E[X], &E[Y], &E[Z]);
    sscanf(argv[5], "%lf", &edist);

    cdist = sqrt(bg_distsq_lseg3_pt(&C, P0, P1, Q));

    if (!NEAR_EQUAL(edist, cdist, DCHECK_TOL)) {
	bu_log("expected %g, got %g\n", edist, cdist);
	bu_exit(-1, "Fatal error - expected distance equality test failed\n");
    }

    if (DIST_PNT_PNT(C, E) > DCHECK_TOL) {
	bu_log("Distance: %f\n", DIST_PNT_PNT(C, E));
	bu_log("E       : %f %f %f\n", V3ARGS(E));
	bu_log("C       : %f %f %f\n", V3ARGS(C));
	bu_exit(-1, "Fatal error - expected and calculated points differ\n");
    }

    bu_log("%g: %g,%g,%g\n", cdist, V3ARGS(C));

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
