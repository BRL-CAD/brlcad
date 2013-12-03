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
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], VUNITIZE_TOL) ||
		    !NEAR_ZERO(output_pnts[i][1] - expected[i][1], VUNITIZE_TOL)) {
		retval += 1;
	    }
	}
	if (retval) {return -1;} else {bu_log("Test #001 Passed!\n");}
	if (do_plotting) {
	    VSET(output_3d_pnts[0], output_pnts[0][0], output_pnts[0][1], 0);
	    VSET(output_3d_pnts[1], output_pnts[1][0], output_pnts[1][1], 0);
	    VSET(output_3d_pnts[2], output_pnts[2][0], output_pnts[2][1], 0);
	    VSET(output_3d_pnts[3], output_pnts[3][0], output_pnts[3][1], 0);
	    if (do_plotting) {
		plot_obr(1, (const point_t *)output_3d_pnts, 4);
	    }
	}
    }

    /* Triangle input */
    {
	point2d_t center;
	vect2d_t u, v;
	point2d_t pnts[3+1] = {{0}};
	point2d_t expected[4+1] = {{0}};
	point2d_t output_pnts[4+1] = {{0}};
	point_t output_3d_pnts[4+1] = {{0}};
	int n = 3;

	V2SET(pnts[0], 1.0, 0.0);
	V2SET(pnts[1], 3.0, 0.0);
	V2SET(pnts[2], 2.0, 4.0);

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
	bu_log("Test #001:  polyline_2d_hull - triangle test:\n");
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
	if (do_plotting) {
	    VSET(output_3d_pnts[0], output_pnts[0][0], output_pnts[0][1], 0);
	    VSET(output_3d_pnts[1], output_pnts[1][0], output_pnts[1][1], 0);
	    VSET(output_3d_pnts[2], output_pnts[2][0], output_pnts[2][1], 0);
	    VSET(output_3d_pnts[3], output_pnts[3][0], output_pnts[3][1], 0);
	    if (do_plotting) {
		plot_obr(8, (const point_t *)output_3d_pnts, 4);
	    }
	}
	if (retval) {return -1;} else {bu_log("Triangle Test Passed!\n");}
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
	VSET(expected[0], 0.5, 0.5, 0.5);
	VSET(expected[1], 0.5, -0.5, 0.5);
	VSET(expected[2], -0.5, -0.5, 0.5);
	VSET(expected[3], -0.5, 0.5, 0.5);
	retval = bn_3d_coplanar_obr(&center, &u, &v, (const point_t *)test2_points, 17);
	if (retval) return -1;
	VADD3(output_pnts[2], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[3], center, u, v);
	VSCALE(v, v, -1);
	VADD3(output_pnts[0], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[1], center, u, v);
	bu_log("Test #002:  3d obr - points in XY plane at Z=0.5, duplicate points:\n");
	for (i = 0; i < 4; i++) {
	    bu_log("    expected[%d]: (%f, %f, %f)\n", i, V3ARGS(expected[i]));
	    bu_log("      actual[%d]: (%f, %f, %f)\n", i, V3ARGS(output_pnts[i]));
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][1] - expected[i][1], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][2] - expected[i][2], VUNITIZE_TOL)) {
		retval += 1;
	    }
	}
	if (retval) {return -1;} else {bu_log("Test #002 Passed!\n");}
	if (do_plotting) {
	    plot_obr(2, (const point_t *)output_pnts, 4);
	}

    }

    {
	point_t center;
	vect_t u, v;
	point_t output_pnts[4+1] = {{0}};
	point_t test3_points[17+1] = {{0}};
	point_t expected[4+1] = {{0}};

	VSET(test3_points[0],-0.2615997126297299746333636,0.9692719821506950994560725,1.113297221058902053414386);
	VSET(test3_points[1],-0.6960082873702697625617475,-0.3376479821506951362053428,0.791972778941099408989146);
	VSET(test3_points[2],0.08930731496876459507561208,0.2282231541335035251982788,0.5408368359872989250547448);
	VSET(test3_points[3],-1.046915314968769772363544,0.4034008458664972707197194,1.364433164012702537348787);
	VSET(test3_points[4],0.08930731496876459507561208,0.2282231541335035251982788,0.5408368359872989250547448);
	VSET(test3_points[5],-1.046915314968769772363544,0.4034008458664972707197194,1.364433164012702537348787);
	VSET(test3_points[6],-0.2615997126297299746333636,0.9692719821506950994560725,1.113297221058902053414386);
	VSET(test3_points[7],-0.6960082873702697625617475,-0.3376479821506951362053428,0.791972778941099408989146);
	VSET(test3_points[8],-0.2894335616770786212548217,0.2866157180445003671565019,0.8153689453291005362345345);
	VSET(test3_points[9],-0.6681744383229220041187091,0.3450082819554983748489008,1.089901054670902258436627);
	VSET(test3_points[10],-0.6588964886404735654679143,0.5725603699908965449338893,1.189210479914168061554847);
	VSET(test3_points[11],-0.5295568798643752739252477,0.628946878032363931865234,1.13080291854799019901634);
	VSET(test3_points[12],-0.6558038387463227536500199,0.6484110660026961570068238,1.222313621661925475692101);
	VSET(test3_points[13],-0.1539086531126804824332055,0.4947036181095669227225642,0.823167667458433838234555);
	VSET(test3_points[14],-0.2987115113595283921732459,0.05906363000910182931013637,0.7160595200858336228932899);
	VSET(test3_points[15],-0.1662792526892814537475829,0.1913008340623683078973727,0.6907551004674108430236856);
	VSET(test3_points[16],-0.5419274794409739692824246,0.3255440939851655945957987,0.9983903515569694242515197);
	VSET(expected[0], 0.089307314968765, 0.228223154133504, 0.540836835987299);
	VSET(expected[1], -0.696008287370264, -0.337647982150691, 0.791972778941097);
	VSET(expected[2], -1.046915314968751, 0.403400845866492, 1.364433164012691);
	VSET(expected[3], -0.261599712629723, 0.969271982150686, 1.113297221058893);

	retval = bn_3d_coplanar_obr(&center, &u, &v, (const point_t *)test3_points, 17);
	if (retval) return -1;
	VADD3(output_pnts[2], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[3], center, u, v);
	VSCALE(v, v, -1);
	VADD3(output_pnts[0], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[1], center, u, v);
	bu_log("Test #003:  3d obr - points in tilted plane, duplicate points:\n");
	for (i = 0; i < 4; i++) {
	    bu_log("    expected[%d]: (%.15f, %.15f, %.15f)\n", i, V3ARGS(expected[i]));
	    bu_log("      actual[%d]: (%.15f, %.15f, %.15f)\n", i, V3ARGS(output_pnts[i]));
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][1] - expected[i][1], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][2] - expected[i][2], VUNITIZE_TOL)) {
		retval += 1;
	    }
	}
	if (retval) {return -1;} else {bu_log("Test #003 Passed!\n");}
	if (do_plotting) {
	    plot_obr(3, (const point_t *)output_pnts, 4);
	}


    }

    {
	point_t center;
	vect_t u, v;
	point_t output_pnts[4+1] = {{0}};
	point_t test4_points[9+1] = {{0}};
	point_t expected[4+1] = {{0}};

	VSET(test4_points[0],-0.2894335616770786212548217,0.2866157180445003671565019,0.8153689453291005362345345);
	VSET(test4_points[1],-0.6681744383229220041187091,0.3450082819554983748489008,1.089901054670902258436627);
	VSET(test4_points[2],-0.6588964886404735654679143,0.5725603699908965449338893,1.189210479914168061554847);
	VSET(test4_points[3],-0.5295568798643752739252477,0.628946878032363931865234,1.13080291854799019901634);
	VSET(test4_points[4],-0.6558038387463227536500199,0.6484110660026961570068238,1.222313621661925475692101);
	VSET(test4_points[5],-0.1539086531126804824332055,0.4947036181095669227225642,0.823167667458433838234555);
	VSET(test4_points[6],-0.2987115113595283921732459,0.05906363000910182931013637,0.7160595200858336228932899);
	VSET(test4_points[7],-0.1662792526892814537475829,0.1913008340623683078973727,0.6907551004674108430236856);
	VSET(test4_points[8],-0.5419274794409739692824246,0.3255440939851655945957987,0.9983903515569694242515197);
	VSET(expected[0], -0.804068848240491, 0.450183326349575, 1.227405986932079);
	VSET(expected[1], -0.277478009809908, 0.042630029322528, 0.694574374420025);
	VSET(expected[2], -0.018014243195113, 0.389528573715492, 0.685662735197256);
	VSET(expected[3], -0.544605081625695, 0.797081870742539, 1.218494347709310);

	retval = bn_3d_coplanar_obr(&center, &u, &v, (const point_t *)test4_points, 9);
	if (retval) return -1;
	VADD3(output_pnts[2], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[3], center, u, v);
	VSCALE(v, v, -1);
	VADD3(output_pnts[0], center, u, v);
	VSCALE(u, u, -1);
	VADD3(output_pnts[1], center, u, v);
	bu_log("Test #004:  3d obr - points from test 4 sans square corners, no duplicate points:\n");
	for (i = 0; i < 4; i++) {
	    bu_log("    expected[%d]: (%.15f, %.15f, %.15f)\n", i, V3ARGS(expected[i]));
	    bu_log("      actual[%d]: (%.15f, %.15f, %.15f)\n", i, V3ARGS(output_pnts[i]));
	    if (!NEAR_ZERO(output_pnts[i][0] - expected[i][0], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][1] - expected[i][1], VUNITIZE_TOL) ||
		!NEAR_ZERO(output_pnts[i][2] - expected[i][2], VUNITIZE_TOL)) {
		retval += 1;
	    }
	}
	if (retval) {return -1;} else {bu_log("Test #004 Passed!\n");}
	if (do_plotting) {
	    plot_obr(4, (const point_t *)output_pnts, 4);
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
