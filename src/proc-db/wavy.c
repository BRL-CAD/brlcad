/*                          W A V Y . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2021 United States Government as represented by
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

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "bu/app.h"
#include "bu/getopt.h"
#include "nmg.h"		/* BRL-CAD Vector macros */
#include "raytrace.h"
#include "wdb.h"


/* Interpolate the data using b-splines */
static struct face_g_snurb **
interpolate_data(fastf_t *grid)
{
    struct face_g_snurb **surfs;
    struct face_g_snurb *srf;
    fastf_t rt_nurb_par_edge();

    BU_ALLOC(srf, struct face_g_snurb);

    nmg_nurb_sinterp(srf, 4, grid, 10, 10);
    nmg_nurb_kvnorm(&srf->u);
    nmg_nurb_kvnorm(&srf->v);

    surfs = (struct face_g_snurb **)bu_calloc(2, sizeof(struct face_g_snurb *), "surfaces");
    surfs[0] = srf;
    surfs[1] = NULL;

    return surfs;
}


static void
printusage()
{
    bu_log("Usage: wavy [-d] [-H hscale]\n");
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

    bu_setprogname(argv[0]);

    hscale = 2.5;

    while ((i=bu_getopt(argc, argv, "H:h?")) != -1) {
	switch (i) {
	    case 'H':
		hscale = atof(bu_optarg);
		break;
	    default:
		printusage();
		bu_exit(1, NULL);
	}
    }

    bu_log("Writing out geometry to file [wavy.g] ...");

    outfp = wdb_fopen("wavy.g");

    /* Create the database header record.  This solid will consist of
     * three surfaces: a top surface, bottom surface, and the sides (so
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

    bu_log(" done.\n");

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
