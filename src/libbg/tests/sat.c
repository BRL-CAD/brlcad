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

int
line_abb_test(int expected, point_t origin, vect_t ldir, point_t aabb_c, vect_t aabb_e)
{
    if (bg_sat_line_abb(origin, ldir, aabb_c, aabb_e) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("LINE - origin: %f %f %f  dir: %f %f %f\n", V3ARGS(origin), V3ARGS(ldir));
	bu_log("AABB - c: %f %f %f  ext: %f %f %f\n", V3ARGS(aabb_c), V3ARGS(aabb_e));
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
	bu_log("OBB - c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb_c), V3ARGS(obb_e1), V3ARGS(obb_e2), V3ARGS(obb_e3));
	bu_exit(1, "test failure\n");
    }
    return 0;
}

int
abb_obb_test(
	int expected,
        point_t abb_min, point_t abb_max,
        point_t obb_c, vect_t obb_e1, vect_t obb_e2, vect_t obb_e3
        )
{

    if (bg_sat_abb_obb(abb_min, abb_max, obb_c, obb_e1, obb_e2, obb_e3) != expected) {
	if (expected) {
	    bu_log("Failed to detect intersection:\n");
	} else {
	    bu_log("Unexpected intersection:\n");
	}
	bu_log("ABB - min: %f %f %f  max: %f %f %f\n", V3ARGS(abb_min), V3ARGS(abb_max));
	bu_log("OBB - c: %f %f %f  ext1: %f %f %f ext2: %f %f %f ext3: %f %f %f\n", V3ARGS(obb_c), V3ARGS(obb_e1), V3ARGS(obb_e2), V3ARGS(obb_e3));
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
	bu_exit(1, "test failure\n");
    }
    return 0;
}

#define NO_ISECT 0
#define ISECT 1

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
    if (abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3))
	return -1;

    for (int i = -2; i < 3; i++) {
	for (int j = -2; j < 3; j++) {
	    for (int k = -3; k < 4; k++) {
		VSET(obb_c, i, j, k);
		abb_obb_test(ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
	    }
	}
    }

    VSET(obb_c, -5, 0, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, -5, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 0, -5);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 5, 0, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 5, 0);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);
    VSET(obb_c, 0, 0, 5);
    abb_obb_test(NO_ISECT, aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3);

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
