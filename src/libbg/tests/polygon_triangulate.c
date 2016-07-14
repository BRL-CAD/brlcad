/*        G M  _ P O L Y G O N _ T R I A N G U L A T E . C
 *
 * BRL-CAD
 *
 * Copyright (c) 2015-2016 United States Government as represented by
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
/** @file bg_polygon_triangulate.c
 *
 * Test polygon triangulation routines
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bg.h"
#include "bn/plot3.h"

/* TODO - the examples below are run, but no check is made to determine
 * if the faces generated are actually correct.*/

/* TODO - need to take args so we can run tests individually */

/* TODO - need some more stressful tests, particularly for hole removal */

int
main(int UNUSED(argc), const char **UNUSED(argv))
{
    int ret;
    {
	size_t num_points = 0;
	int num_faces = 0;
	int *faces = NULL;

	/* 44 point polygon derived from NIST MBE PMI sample 1 shape */
	point2d_t points[44] = {{0}};
	V2SET(points[0], 2757.83, 2422.83);
	V2SET(points[1], 2707.83, 2422.83);
	V2SET(points[2], 2657.83, 2372.83);
	V2SET(points[3], 2657.83, 2347.83);
	V2SET(points[4], 2632.83, 2322.83);
	V2SET(points[5], 2600.33, 2322.83);
	V2SET(points[6], 2600.33, 2307.83);
	V2SET(points[7], 2590.33, 2297.83);
	V2SET(points[8], 2525.33, 2297.83);
	V2SET(points[9], 2515.33, 2307.83);
	V2SET(points[10], 2515.33, 2322.83);
	V2SET(points[11], 2300.33, 2322.83);
	V2SET(points[12], 2300.33, 2307.83);
	V2SET(points[13], 2290.33, 2297.83);
	V2SET(points[14], 2225.33, 2297.83);
	V2SET(points[15], 2215.33, 2307.83);
	V2SET(points[16], 2215.33, 2322.83);
	V2SET(points[17], 2182.83, 2322.83);
	V2SET(points[18], 2157.83, 2347.83);
	V2SET(points[19], 2157.83, 2372.83);
	V2SET(points[20], 2107.83, 2422.83);
	V2SET(points[21], 2057.83, 2422.83);
	V2SET(points[22], 2007.83, 2372.83);
	V2SET(points[23], 2007.83, 2022.83);
	V2SET(points[24], 2057.83, 1972.83);
	V2SET(points[25], 2107.83, 1972.83);
	V2SET(points[26], 2157.83, 2022.83);
	V2SET(points[27], 2157.83, 2047.83);
	V2SET(points[28], 2182.83, 2072.83);
	V2SET(points[29], 2268.96, 2072.83);
	V2SET(points[30], 2289.17, 2107.83);
	V2SET(points[31], 2306.49, 2107.83);
	V2SET(points[32], 2326.69, 2072.83);
	V2SET(points[33], 2488.96, 2072.83);
	V2SET(points[34], 2509.17, 2107.83);
	V2SET(points[35], 2526.49, 2107.83);
	V2SET(points[36], 2546.69, 2072.83);
	V2SET(points[37], 2632.83, 2072.83);
	V2SET(points[38], 2657.83, 2047.83);
	V2SET(points[39], 2657.83, 2022.83);
	V2SET(points[40], 2707.83, 1972.83);
	V2SET(points[41], 2757.83, 1972.83);
	V2SET(points[42], 2807.83, 2022.83);
	V2SET(points[43], 2807.83, 2372.83);

	num_points = sizeof(points) / sizeof(point2d_t);
	ret = bg_polygon_triangulate(&faces, &num_faces, NULL, NULL, (const point2d_t *)points, num_points, EAR_CLIPPING);
	if (ret) {
	    return 1;
	}
    }
    {
	size_t num_points = 0;
	int num_faces = 0;
	int *faces = NULL;

	/* 14 point polygon derived from NIST MBE PMI sample 3 shape */
	point2d_t points[14] = {{0}};
	V2SET(points[0], 801.392008, 748.924528);
	V2SET(points[1], 801.392008, 729.874528);
	V2SET(points[2], 1164.916808, 729.874528);
	V2SET(points[3], 1164.916808, 748.924528);
	V2SET(points[4], 1152.216808, 761.624528);
	V2SET(points[5], 1125.394408, 761.624528);
	V2SET(points[6], 1110.154408, 746.384528);
	V2SET(points[7], 1046.654408, 746.384528);
	V2SET(points[8], 1046.654408, 758.994272);
	V2SET(points[9], 919.654408, 758.994272);
	V2SET(points[10], 919.654408, 746.384528);
	V2SET(points[11], 856.154408, 746.384528);
	V2SET(points[12], 840.914408, 761.624528);
	V2SET(points[13], 814.092008, 761.624528);

	num_points = sizeof(points) / sizeof(point2d_t);
	ret = bg_polygon_triangulate(&faces, &num_faces, NULL, NULL, (const point2d_t *)points, num_points, EAR_CLIPPING);
	if (ret) {
	    return 1;
	}
    }

    /* Nested test case 1 */
    {
	int num_faces;
	int *faces;
	int poly[12] = {11,10,9,8,7,6,5,4,3,2,1,0};
	int hole[4] = {12,13,14,15};
	int **hole_array = (int **)bu_calloc(1, sizeof(int *), "hole_array");
	size_t hole_cnt = 4;

	point2d_t points[16] = {{0}};
	V2SET(points[0], 2, -3);
	V2SET(points[1], 3, -2);
	V2SET(points[2], 4, -1.5);
	V2SET(points[3], 6, -1.5);
	V2SET(points[4], 7, -2);
	V2SET(points[5], 8, -3);
	V2SET(points[6], 8, -5);
	V2SET(points[7], 7, -6);
	V2SET(points[8], 6, -6.5);
	V2SET(points[9], 4, -6.5);
	V2SET(points[10], 3, -6);
	V2SET(points[11], 2, -5);
	V2SET(points[12], 4.5, -3.5);
	V2SET(points[13], 5.5, -3.5);
	V2SET(points[14], 5.5, -4.5);
	V2SET(points[15], 4.5, -5.5);

	hole_array[0] = (int *)hole;

	ret = bg_nested_polygon_triangulate(&faces, &num_faces, NULL, NULL,
	       	(const int *)poly, 12, (const int **)hole_array, (const size_t *)&hole_cnt, 1,
	       	(const point2d_t *)points, 16, EAR_CLIPPING);
	bu_free(hole_array, "free hole array");
	if (ret) {
	    bu_log("Nested clipping 1 fail\n");
	    return 1;
	}
    }

   /* Nested test case 2 */
    {
	int num_faces;
	int *faces;
	int poly[8] = {0,1,2,3,4,5,6,7};
	int hole[4] = {8,9,10,11};
	int **hole_array = (int **)bu_calloc(1, sizeof(int *), "hole_array");
	size_t hole_cnt = 4;

	point2d_t points[12] = {{0}};
	V2SET(points[0], 3, -5.5);
	V2SET(points[1], 4.5, -5.5);
	V2SET(points[2], 6, -4.75);
	V2SET(points[3], 4.625, -4.875);
	V2SET(points[4], 5.75, -3.75);
	V2SET(points[5], 4.875, -3);
	V2SET(points[6], 4.25, -4.625);
	V2SET(points[7], 4.125, -3.125);
	V2SET(points[8], 3.875, -3.875);
	V2SET(points[9], 4.125, -3.875);
	V2SET(points[10], 4.125, -4.125);
	V2SET(points[11], 3.875, -4.125);

	hole_array[0] = (int *)hole;

	ret = bg_nested_polygon_triangulate(&faces, &num_faces, NULL, NULL,
	       	(const int *)poly, 8, (const int **)hole_array, (const size_t *)&hole_cnt, 1,
	       	(const point2d_t *)points, 12, EAR_CLIPPING);
	bu_free(hole_array, "free hole array");
	if (ret) {
	    bu_log("Nested clipping 2 fail\n");
	    return 1;
	}
    }

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
