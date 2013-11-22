/*                      B N _ C H U L L . C
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
    int i = 0;
    int retval = 0;
    point_t test1_points[4+1] = {{0}};
    point_t test1_results[5+1] = {{0}};
    int n = 4;
    point_t *hull_polyline = NULL;
    point_t *hull_pnts = NULL;

    VSET(test1_points[0], 1.5, 1.5, 0.0);
    VSET(test1_points[1], 3.0, 2.0, 0.0);
    VSET(test1_points[2], 2.0, 2.5, 0.0);
    VSET(test1_points[3], 1.0, 2.0, 0.0);

    VSET(test1_results[0], 1.0, 2.0, 0.0);
    VSET(test1_results[1], 1.5, 1.5, 0.0);
    VSET(test1_results[2], 3.0, 2.0, 0.0);
    VSET(test1_results[3], 2.0, 2.5, 0.0);
    VSET(test1_results[4], 1.0, 2.0, 0.0);

    retval = bn_polyline_2d_hull(&hull_polyline, (const point_t *)test1_points, n);
    if (!retval) return -1;
    bu_log("Test #001:  polyline_2d_hull - 4 point test:\n");
    for (i = 0; i < retval; i++) {
	bu_log("    expected[%d]: (%f, %f, %f)\n", i, test1_results[i][0], test1_results[i][1], test1_results[i][2]);
	bu_log("      actual[%d]: (%f, %f, %f)\n", i, hull_polyline[i][0], hull_polyline[i][1], hull_polyline[i][2]);
	if (!NEAR_ZERO(test1_results[i][0] - hull_polyline[i][0], SMALL_FASTF) ||
	    !NEAR_ZERO(test1_results[i][1] - hull_polyline[i][1], SMALL_FASTF) ||
	    !NEAR_ZERO(test1_results[i][2] - hull_polyline[i][2], SMALL_FASTF) ) {
	    retval = 0;
	}
    }
    if (!retval) {return -1;} else {bu_log("Test #001 Passed!\n");}


    retval = bn_2d_hull(&hull_pnts, (const point_t *)test1_points, n);
    if (!retval) return -1;
    bu_log("Test #002:  2d_hull - 4 point test:\n");
    for (i = 0; i < retval; i++) {
	bu_log("    expected[%d]: (%f, %f, %f)\n", i, test1_results[i][0], test1_results[i][1], test1_results[i][2]);
	bu_log("      actual[%d]: (%f, %f, %f)\n", i, hull_pnts[i][0], hull_pnts[i][1], hull_pnts[i][2]);
	if (!NEAR_ZERO(test1_results[i][0] - hull_pnts[i][0], SMALL_FASTF) ||
	    !NEAR_ZERO(test1_results[i][1] - hull_pnts[i][1], SMALL_FASTF) ||
	    !NEAR_ZERO(test1_results[i][2] - hull_pnts[i][2], SMALL_FASTF) ) {
	    retval = 0;
	}
    }
    if (!retval) {return -1;} else {bu_log("Test #002 Passed!\n");}

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

