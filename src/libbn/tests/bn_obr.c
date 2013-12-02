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
    int retval = 0;

    int do_plotting = 0;

    if (argc == 2 && BU_STR_EQUAL(argv[1], "-p")) do_plotting = 1;

    /* 2D input */
    {
	point2d_t center;
	vect2d_t u, v;
	point2d_t pnts[4+1] = {{0}};
	point2d_t output_pnts[4+1] = {{0}};
	point_t output_3d_pnts[4+1] = {{0}};
	int n = 4;

	V2SET(pnts[0], 1.5, 1.5);
	V2SET(pnts[1], 3.0, 2.0);
	V2SET(pnts[2], 2.0, 2.5);
	V2SET(pnts[3], 1.0, 2.0);

	retval = bn_2d_obr(&center, &u, &v, (const point2d_t *)pnts, n);
	if (retval) return -1;
	V2ADD3(output_pnts[2], center, u, v);
	V2SCALE(u, u, -1);
	V2ADD3(output_pnts[3], center, u, v);
	V2SCALE(v, v, -1);
	V2ADD3(output_pnts[0], center, u, v);
	V2SCALE(u, u, -1);
	V2ADD3(output_pnts[1], center, u, v);

	bu_log("input : p1     (%f, %f)\n", V2ARGS(pnts[0]));
	bu_log("      : p2     (%f, %f)\n", V2ARGS(pnts[1]));
	bu_log("      : p3     (%f, %f)\n", V2ARGS(pnts[2]));
	bu_log("      : p4     (%f, %f)\n", V2ARGS(pnts[3]));
	bu_log("result: center (%f, %f)\n", V2ARGS(center));
	bu_log("        u      (%f, %f)\n", V2ARGS(u));
	bu_log("        v      (%f, %f)\n", V2ARGS(v));
	bu_log("        op1    (%f, %f)\n", V2ARGS(output_pnts[0]));
	bu_log("        op2    (%f, %f)\n", V2ARGS(output_pnts[1]));
	bu_log("        op3    (%f, %f)\n", V2ARGS(output_pnts[2]));
	bu_log("        op4    (%f, %f)\n", V2ARGS(output_pnts[3]));
	if (do_plotting) {
	    VSET(output_3d_pnts[0], output_pnts[0][0], output_pnts[0][1], 0);
	    VSET(output_3d_pnts[1], output_pnts[1][0], output_pnts[1][1], 0);
	    VSET(output_3d_pnts[2], output_pnts[2][0], output_pnts[2][1], 0);
	    VSET(output_3d_pnts[3], output_pnts[3][0], output_pnts[3][1], 0);
	    plot_obr(1, (const point_t *)output_3d_pnts, 4);
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
