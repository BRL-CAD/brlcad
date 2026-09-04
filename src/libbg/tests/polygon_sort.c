/*               P O L Y G O N _ S O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbg/tests/polygon_sort.c
 *
 * Verify angular sorting of a translated, non-quadrilateral polygon.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bg/polygon.h"
#include "bu/app.h"


int
main(int argc, char **argv)
{
    const fastf_t expected_area = 0.875;
    point_t points[5] = {
	{1.0, 1.0, 0.0},
	{1.0, 1.0, 0.5},
	{1.0, 0.0, 1.0},
	{1.0, 0.0, 0.0},
	{1.0, 0.5, 1.0}
    };
    plane_t plane = {1.0, 0.0, 0.0, 1.0};
    fastf_t area = 0.0;

    bu_setprogname(argv[0]);
    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    if (bg_3d_polygon_sort_ccw(5, points, plane) != 0 ||
	    bg_3d_polygon_area(&area, 5, (const point_t *)points) != 0) {
	fprintf(stderr, "Unable to sort and measure test polygon\n");
	return 1;
    }
    if (fabs(area - expected_area) > SMALL_FASTF) {
	fprintf(stderr, "Area is %.17g, expected %.17g\n", area,
		expected_area);
	return 1;
    }

    return 0;
}
