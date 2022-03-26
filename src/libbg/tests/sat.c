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

#define EXPECT_NO_ISECT(_x, _y, _z) {\
    VSET(obb_c, _x, _y, _z);\
    if (bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3)) {\
	bu_log("Unexpected intersection at center pt: %f %f %f\n", V3ARGS(obb_c)); \
    } \
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: %s does not accept arguments\n", argv[0]);

    point_t aabb_min, aabb_max;
    vect_t obb_c, obb_e1, obb_e2, obb_e3;

    // Start simple - unit box at origin
    VSET(aabb_min, -1, -1, -1);
    VSET(aabb_max, 1, 1, 1);

    // Start out with some trivial checks - an axis-aligned test box, and move
    // the center point to trigger different responses
    VSET(obb_e1, -1, 0, 0);
    VSET(obb_e2, 0, 1, 0);
    VSET(obb_e3, 0, 0, 2);

    VSET(obb_c, 0, 0, 0);
    if (!bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3))
	return -1;

    for (int i = -2; i < 3; i++) {
	for (int j = -2; j < 3; j++) {
	    for (int k = -3; k < 4; k++) {
		VSET(obb_c, i, j, k);
		if (!bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3)) {
		    bu_log("Didn't find expected intersection at center pt: %f %f %f\n", V3ARGS(obb_c));
		    return -1;
		}
	    }
	}
    }

    EXPECT_NO_ISECT(-5, 0, 0);
    EXPECT_NO_ISECT(0, -5, 0);
    EXPECT_NO_ISECT(0, 0, -5);
    EXPECT_NO_ISECT(5, 0, 0);
    EXPECT_NO_ISECT(0, 5, 0);
    EXPECT_NO_ISECT(0, 0, 5);

    // TODO - rotate obb vectors for non-trivial testing


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
