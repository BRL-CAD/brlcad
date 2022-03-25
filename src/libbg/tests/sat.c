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

    VSET(obb_c, 0, 0, 0);
    if (!bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3))
	return -1;

    VSET(obb_c, 2, 0, 0);
    if (!bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3))
	return -1;

    VSET(obb_c, 5, 0, 0);
    if (bg_sat_abb_obb(aabb_min, aabb_max, obb_c, obb_e1, obb_e2, obb_e3))
	return -1;

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
