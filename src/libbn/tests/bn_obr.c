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
    point_t *pnts = (point_t *)bu_calloc(4+1, sizeof(fastf_t)*3, "pnt array");
    int n = 4;
    pnts[0][0] = 1.5; pnts[0][1] = 1.5; pnts[0][2] = 0;
    pnts[1][0] = 3.0; pnts[1][1] = 2.0; pnts[1][2] = 0;
    pnts[2][0] = 2.0; pnts[2][1] = 2.5; pnts[2][2] = 0;
    pnts[3][0] = 1.0; pnts[3][1] = 2.0; pnts[3][2] = 0;

    (void)bn_obr((const point_t *)pnts, n, &p0, &p1, &p2, &p3);

    bu_log("result: P1 (%f, %f, %f)\n", p0[0], p0[1], p0[2]);
    bu_log("        P2 (%f, %f, %f)\n", p1[0], p1[1], p1[2]);
    bu_log("        P3 (%f, %f, %f)\n", p2[0], p2[1], p2[2]);
    bu_log("        P4 (%f, %f, %f)\n", p3[0], p3[1], p3[2]);
/*
    if(expected_result == actual_result) {
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
