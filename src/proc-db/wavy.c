/*                          W A V Y . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file proc-db/wavy.c
 *
 * Create a random wavy surface spline model database.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"		/* BRL-CAD Vector macros */
#include "nurb.h"		/* BRL-CAD Spline data structures */
#include "raytrace.h"
#include "wdb.h"


/* Interpoate the data using b-splines */
struct face_g_snurb **
interpolate_data(fastf_t *grid)
{
    struct face_g_snurb **surfs;
    struct face_g_snurb *srf;
    fastf_t rt_nurb_par_edge();

    BU_GET(srf, struct face_g_snurb);

    rt_nurb_sinterp(srf, 4, grid, 10, 10);
    rt_nurb_kvnorm(&srf->u);
    rt_nurb_kvnorm(&srf->v);

    surfs = (struct face_g_snurb **)bu_calloc(2, sizeof(struct face_g_snurb *), "surfaces");
    surfs[0] = srf;
    surfs[1] = NULL;

    return surfs;
}


int
main(int argc, char **argv)
{
    char *id_name = "wavy database";
    char *nurb_name = "wavy";
    int i, j;
    fastf_t hscale;
    struct rt_wdb *outfp;
    fastf_t grid[10][10][3];
    struct face_g_snurb **surfaces;

    outfp = wdb_fopen("wavy.g");

    hscale = 2.5;

    while ((i=bu_getopt(argc, argv, "dh:")) != -1) {
	switch (i) {
	    case 'd':
		rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL;
		break;
	    case 'h':
		hscale = atof(bu_optarg);
		break;
	    default:
		bu_exit(1, "Usage: %s [-d]\n", *argv);
	}
    }

    /* Create the database header record.  this solid will consist of
     * three surfaces a top surface, bottom surface, and the sides (so
     * that it will be closed).
     */

    mk_id(outfp, id_name);

    for (i = 0; i < 10; i++) {
	for (j = 0; j < 10; j++) {
	    fastf_t v;

	    v = (hscale * drand48()) + 10.0;

	    grid[i][j][0] = i;
	    grid[i][j][1] = j;
	    grid[i][j][2] = v;
	}
    }

    surfaces = interpolate_data(&grid[0][0][0]);

    mk_bspline(outfp, nurb_name, surfaces);

    wdb_close(outfp);
    bu_log("Geometry saved to 'wavy.g'\n");

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
