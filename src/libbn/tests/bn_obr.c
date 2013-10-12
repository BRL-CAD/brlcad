/*                        B N _ O B R . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
#include "vmath.h"
#include "bn.h"

int
main()
{
    point_t p0, p1, p2, p3;
    point_t pnts[4+1] = {{0}};
    int n = 4;

    pnts[0][X] = 1.5; pnts[0][Y] = 1.5; pnts[0][Z] = 0;
    pnts[1][X] = 3.0; pnts[1][Y] = 2.0; pnts[1][Z] = 0;
    pnts[2][X] = 2.0; pnts[2][Y] = 2.5; pnts[2][Z] = 0;
    pnts[3][X] = 1.0; pnts[3][Y] = 2.0; pnts[3][Z] = 0;

    (void)bn_obr((const point_t *)pnts, n, &p0, &p1, &p2, &p3);

    bu_log("result: P1 (%f, %f, %f)\n", V3ARGS(p0));
    bu_log("        P2 (%f, %f, %f)\n", V3ARGS(p1));
    bu_log("        P3 (%f, %f, %f)\n", V3ARGS(p2));
    bu_log("        P4 (%f, %f, %f)\n", V3ARGS(p3));
/*
    if (expected_result == actual_result) {
	return 0;
    } else {
	return -1;
    }
*/
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
