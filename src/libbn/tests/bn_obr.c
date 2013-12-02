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
#include "plot3.h"

HIDDEN
void plot_obr(int test_num, const point_t *pnt_array, int pnt_cnt)
{
    int i = 0;
    struct bu_vls name;
    FILE *plot_file = NULL;
    bu_vls_init(&name);
    bu_vls_printf(&name, "obr_test_%.3d.pl", test_num);
    plot_file = fopen(bu_vls_addr(&name), "w");
    pl_color(plot_file, 0, 255, 0);
    for (i = 0; i < pnt_cnt; i++) {
	pdv_3move(plot_file, pnt_array[i]);
	if (i < pnt_cnt - 1) {
	    pdv_3cont(plot_file, pnt_array[i+1]);
	} else {
	    pdv_3cont(plot_file, pnt_array[0]);
	}
    }
    fclose(plot_file);
    bu_vls_free(&name);
}



int
main(int argc, const char **argv)
{
    int i = 0;
    int retval = 0;

    int do_plotting = 0;

    if (argc == 2 && BU_STR_EQUAL(argv[1], "-p")) do_plotting = 1;

    /* 2D input */
    {
	point2d_t center;
	vect2d_t u, v;
	point2d_t pnts[4+1] = {{0}};
	point2d_t expected[4+1] = {{0}};
	point2d_t output_pnts[4+1] = {{0}};
	point_t output_3d_pnts[4+1] = {{0}};
	int n = 4;

	V2SET(pnts[0], 1.5, 1.5);
	V2SET(pnts[1], 3.0, 2.0);
	V2SET(pnts[2], 2.0, 2.5);
	V2SET(pnts[3], 1.0, 2.0);

	V2SET(expected[0], 1.2, 1.4);
	V2SET(expected[1], 3.0, 2.0);
	V2SET(expected[2], 2.75, 2.75);
	V2SET(expected[3], 0.95, 2.15);


	retval = bn_2d_obr(&center, &u, &v, (const point2d_t *)pnts, n);
	if (retval) return -1;
	V2ADD3(output_pnts[2], center, u, v);
	V2SCALE(u, u, -1);
	V2ADD3(output_pnts[3], center, u, v);
	V2SCALE(v, v, -1);
	V2ADD3(output_pnts[0], center, u, v);
	V2SCALE(u, u, -1);
	V2ADD3(output_pnts[1], center, u, v);
	bu_log("Test #001:  polyline_2d_hull - 4 point test:\n");
	bu_log("bn_2d_obr: center (%f, %f)\n", V2ARGS(center));
	bu_log("           u      (%f, %f)\n", V2ARGS(u));
	bu_log("           v      (%f, %f)\n", V2ARGS(v));

	for (i = 0; i < 4; i++) {
	    bu_log("    expected[%d]: (%f, %f)\n", i, V2ARGS(expected[i]));
	    bu_log("      actual[%d]: (%f, %f)\n", i, V2ARGS(output_pnts[i]));
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], SMALL_FASTF) ||
		    !NEAR_ZERO(output_pnts[i][1] - expected[i][1], SMALL_FASTF)) {
		retval += 1;
	    }
	}
	if (!retval) {return -1;} else {bu_log("Test #001 Passed!\n");}
	if (do_plotting) {
	    VSET(output_3d_pnts[0], output_pnts[0][0], output_pnts[0][1], 0);
	    VSET(output_3d_pnts[1], output_pnts[1][0], output_pnts[1][1], 0);
	    VSET(output_3d_pnts[2], output_pnts[2][0], output_pnts[2][1], 0);
	    VSET(output_3d_pnts[2], output_pnts[2][0], output_pnts[2][1], 0);
	    VSET(output_3d_pnts[3], output_pnts[3][0], output_pnts[3][1], 0);
	    plot_obr(1, (const point_t *)output_3d_pnts, 4);
	}
    }

    /* 3D input */
    {
	point_t center;
	vect_t u, v;
	point_t output_pnts[4+1] = {{0}};
	point_t test2_points[17+1] = {{0}};
	point_t expected[4+1] = {{0}};
	VSET(test2_points[0], -0.5, 0.5, 0.5);
	VSET(test2_points[1], 0.5, -0.5, 0.5);
	VSET(test2_points[2], 0.5, 0.5, 0.5);
	VSET(test2_points[3], -0.5, -0.5, 0.5);
	VSET(test2_points[4], 0.5, 0.5, 0.5);
	VSET(test2_points[5], -0.5, -0.5, 0.5);
	VSET(test2_points[6], -0.5, 0.5, 0.5);
	VSET(test2_points[7], 0.5, -0.5, 0.5);
	VSET(test2_points[8], 0.1666666666666666574148081, 0.1666666666666666574148081, 0.5);
	VSET(test2_points[9], -0.1666666666666666574148081, -0.1666666666666666574148081, 0.5);
	VSET(test2_points[10], -0.3888888888888888950567946, -0.05555555555555555247160271, 0.5);
	VSET(test2_points[11], -0.3518518518518518600757261, 0.09259259259259258745267118, 0.5);
	VSET(test2_points[12], -0.4629629629629629095077803, -0.01851851851851851749053424, 0.5);
	VSET(test2_points[13], 0.05555555555555555247160271, 0.3888888888888888950567946, 0.5);
	VSET(test2_points[14], 0.3888888888888888950567946, 0.05555555555555555247160271, 0.5);
	VSET(test2_points[15], 0.3518518518518518600757261, 0.2407407407407407273769451, 0.5);
	VSET(test2_points[16], -0.05555555555555555247160271, -0.05555555555555555247160271, 0.5);
	retval = bn_3d_coplanar_obr(&center, &u, &v, (const point_t *)test2_points, 17);
	if (retval) return -1;
	VADD3(output_pnts[2], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[3], center, u, v);
	VSCALE(v, v, -1);
	VADD3(output_pnts[0], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[1], center, u, v);
	bu_log("Test #002:  3d_hull - points in XY plane at Z=0.5, duplicate points:\n");
	for (i = 0; i < 4; i++) {
	    bu_log("    expected[%d]: (%f, %f, %f)\n", i, V3ARGS(expected[i]));
	    bu_log("      actual[%d]: (%f, %f, %f)\n", i, V3ARGS(output_pnts[i]));
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], SMALL_FASTF) ||
		!NEAR_ZERO(output_pnts[i][1] - expected[i][1], SMALL_FASTF) ||
		!NEAR_ZERO(output_pnts[i][2] - expected[i][2], SMALL_FASTF)) {
		retval += 1;
	    }
	}
	if (retval) {return -1;} else {bu_log("Test #002 Passed!\n");}
	if (do_plotting) {
	    plot_obr(1, (const point_t *)output_pnts, 4);
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
