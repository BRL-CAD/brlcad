/*                       P O L Y 2 T R I . C
 *
 * BRL-CAD
 *
 * Copyright (c) 2015-2024 United States Government as represented by
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
/** @file bg_poly2tri.c
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
#include "bv/plot3.h"

static int plot_files;

static void
_tess_report(int *faces, int num_faces, const point2d_t *points, int test_num, int method_num)
{
    bu_log("Test %d, method %d: generated %d faces: \n", test_num, method_num, num_faces);
    for (int i = 0; i < num_faces; i++) {
	bu_log("%d: %d -> %d -> %d\n", i, faces[3*i], faces[3*i+1], faces[3*i+2]);
    }
    if (plot_files) {
	struct bu_vls tfile = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&tfile, "test_%d-%d_triangles.plot3", test_num, method_num);
	bg_tri_plot_2d(bu_vls_cstr(&tfile), faces, num_faces, points, 0, 255, 0);
	bu_vls_free(&tfile);
    }
}

int
main(int argc, const char **argv)
{
    int ret;

    bu_setprogname(argv[0]);

    plot_files = 0;
    if (argc > 1 && BU_STR_EQUAL(argv[1], "--plot")) {
	plot_files = 1;
    }
#if 0
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
	if (plot_files) {
	    bg_polygon_plot_2d("test_1_polygon.plot3", (const point2d_t *)points, num_points, 255, 0, 0);
	}

	int *verts_ind = NULL;
	verts_ind = (int *)bu_calloc(44, sizeof(int), "vert indices");
	for (size_t i = 0; i < 44; i++) {
	    verts_ind[i] = (int)i;
	}

	ret = bg_poly2tri_test(&faces, &num_faces, NULL, NULL, verts_ind, 44, NULL, NULL, 0, NULL, 0, (const point2d_t *)points);
	if (ret) {
	    bu_log("test 1 ear clipping failure!\n");
	    return 1;
	} else {
	    _tess_report(faces, num_faces, (const point2d_t *)points, 1, 1);
	}
    }
#endif
    {
	int num_faces = 0;
	int *faces = NULL;
	int outer[4] = {0, 1, 2, 3};
	int hole1[4] = {4, 5, 6, 7};
	int hole2[4] = {8, 9, 10, 11};
	int hole3[4] = {12, 13, 14, 15};
	size_t hole_cnt = 3;
	size_t *hcnts = (size_t *)bu_calloc(3, sizeof(size_t), "hole_cnt_array");
	hcnts[0] = 4;
	hcnts[1] = 4;
	hcnts[2] = 4;
	int **hole_array = (int **)bu_calloc(3, sizeof(int *), "hole_array");
	hole_array[0] = hole1;
	hole_array[1] = hole2;
	hole_array[2] = hole3;

	point2d_t points[16] = {{0}};
	V2SET(points[0],   0, 0);
	V2SET(points[1],   0, 6);
	V2SET(points[2],  11, 6);
	V2SET(points[3],  11, 0);
	V2SET(points[4],   1, 1 );
	V2SET(points[5],   1, 5);
	V2SET(points[6],   3, 5);
	V2SET(points[7],   3, 1);
	V2SET(points[8],   5, -1);
	V2SET(points[9],   5, 7);
	V2SET(points[10],  6, 7);
	V2SET(points[11],  6, -1);
	V2SET(points[12],  8, 1);
	V2SET(points[13],  8, 4);
	V2SET(points[14], 10, 4);
	V2SET(points[15], 10, 1);

#if 0
	size_t num_points = sizeof(points) / sizeof(point2d_t);
	if (plot_files)
	    bg_polygon_plot_2d("test_1_polygon.plot3", (const point2d_t *)points, num_points, 255, 0, 0);
#endif

	point2d_t *opnts = NULL;
	int noutpnts = 0;
	ret = bg_poly2tri_test(&faces, &num_faces, &opnts, &noutpnts, (const int *)outer, 4, (const int **)hole_array, (const size_t *)hcnts, hole_cnt, NULL, 0, (const point2d_t *)points);
	if (ret) {
	    bu_log("multiple hole splitting failure!\n");
	    return 1;
	} else {
	    bg_trimesh_2d_plot3("poly2tri_new_pnts.plot3", faces, num_faces, (const point2d_t *)opnts, noutpnts);
	    _tess_report(faces, num_faces, (const point2d_t *)opnts, 1, 1);
	}
    }

#if 0
    /* Nested test case 1 */
    {
	int num_faces;
	int *faces;
	int poly[12] = {11,10,9,8,7,6,5,4,3,2,1,0};
	int hole[4] = {15,14,13,12};
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

	if (plot_files) {
	    bg_polygon_plot_2d("test_3_polygon.plot3", (const point2d_t *)points, 12, 255, 0, 0);
	    bg_polygon_plot_2d("test_3_polygon_hole.plot3", (const point2d_t *)(points+12), 4, 0, 0, 255);
	}

	ret = bg_nested_poly_triangulate(&faces, &num_faces, NULL, NULL,
					    (const int *)poly, 12, (const int **)hole_array, (const size_t *)&hole_cnt, 1, NULL, 0,
					    (const point2d_t *)points, 16, TRI_EAR_CLIPPING);
	bu_free(hole_array, "free hole array");
	if (ret) {
	    bu_log("Nested clipping 1 fail\n");
	    return 1;
	} else {
	    _tess_report(faces, num_faces, (const point2d_t *)points, 3, 1);
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

	if (plot_files) {
	    bg_polygon_plot_2d("test_4_polygon.plot3", (const point2d_t *)points, 8, 255, 0, 0);
	    bg_polygon_plot_2d("test_4_polygon_hole.plot3", (const point2d_t *)(points+8), 4, 0, 0, 255);
	}

	ret = bg_nested_poly_triangulate(&faces, &num_faces, NULL, NULL,
					    (const int *)poly, 8, (const int **)hole_array, (const size_t *)&hole_cnt, 1, NULL, 0,
					    (const point2d_t *)points, 12, TRI_EAR_CLIPPING);
	bu_free(hole_array, "free hole array");
	if (ret) {
	    bu_log("Nested clipping 2 fail\n");
	    return 1;
	} else {
	    _tess_report(faces, num_faces, (const point2d_t *)points, 4, 1);
	}
    }

    /* Four point triangle */
    {
	size_t num_points = 0;
	int num_faces = 0;
	int *faces = NULL;

	/* 44 point polygon derived from NIST MBE PMI sample 1 shape */
	point2d_t points[4] = {{0}};
	V2SET(points[0], 2.08348167919269223, -0.69232852214032814);
	V2SET(points[1], 1.60267821476360961, -0.00000000000000006);
	V2SET(points[2], 1.20200866111591842, 1.15388087023390984);
	V2SET(points[3], 0.80133910746824888, 2.30776174046776017);

	if (plot_files) {
	    bg_polygon_plot_2d("test_5_polygon.plot3", (const point2d_t *)points, 4, 255, 0, 0);
	}

	num_points = sizeof(points) / sizeof(point2d_t);
	ret = bg_poly_triangulate(&faces, &num_faces, NULL, NULL, NULL, 0, (const point2d_t *)points, num_points, TRI_EAR_CLIPPING);
	if (ret) {
	    bu_log("4 point triangle failure!\n");
	    return 1;
	} else {
	    _tess_report(faces, num_faces, (const point2d_t *)points, 5, 1);
	}
    }
#endif

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
