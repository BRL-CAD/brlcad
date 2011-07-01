/*                  N U R B _ E X A M P L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file librt/nurb_example.c
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "vmath.h"
#include "raytrace.h"
#include "nurb.h"
#include "plot3.h"


extern double drand48(void);

fastf_t grid[10][10][3];
fastf_t ngrid[10][10][3];


/* Interpoate the data using b-splines */
void
interpolate_data()
{
    struct face_g_snurb srf;
    struct face_g_snurb *srf2, *srf3;
    struct knot_vector new_kv;

    rt_nurb_sinterp(&srf, 4, (const fastf_t *)grid, 10, 10);

    /* lets take a look at it.  Refine to 100 points in both directions. */
    rt_nurb_kvknot(&new_kv, srf.order[0], 0.0, 1.0, 100, (struct resource *)NULL);
    srf2 = (struct face_g_snurb *) rt_nurb_s_refine(&srf, 0, &new_kv, (struct resource *)NULL);
    srf3 = (struct face_g_snurb *) rt_nurb_s_refine(srf2, 1, &new_kv, (struct resource *)NULL);

    /* Draw refined mesh in yellow */
    pl_color(stdout, 200, 200, 50);
    rt_nurb_plot_snurb(stdout, srf3);
}


int
main(int argc, char *argv[])
{

    fastf_t hscale;
    fastf_t v;
    int i, j;

    if (argc > 1)
	printf("Usage: %s\n", argv[0]);

    hscale = 2.5;

    /* generate a 10 x 10 grid of random height data */

    pl_color(stdout, 155, 55, 55);

    for (i = 0; i < 10; i++) {
	for (j = 0; j < 10; j++) {
	    v = hscale * drand48() + 10.0;
	    grid[i][j][0] = i;
	    grid[i][j][1] = j;
	    grid[i][j][2] = v;

	    pd_3move(stdout,
		     grid[i][j][0],  grid[i][j][1], grid[i][j][2] - .14);
	    pd_3cont(stdout,
		     grid[i][j][0], grid[i][j][1], grid[i][j][2] + .14);

	    pd_3move(stdout,
		     grid[i][j][0] - .14, grid[i][j][1], grid[i][j][2]);
	    pd_3cont(stdout,
		     grid[i][j][0] + .14, grid[i][j][1], grid[i][j][2]);

	    pd_3move(stdout,
		     grid[i][j][0], grid[i][j][1]-.14, grid[i][j][2]);
	    pd_3cont(stdout,
		     grid[i][j][0], grid[i][j][1]+.14, grid[i][j][2]);
	}
    }


    interpolate_data();

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
