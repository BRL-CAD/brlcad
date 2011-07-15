/*                           T E A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file proc-db/tea.c
 *
 * Convert the Utah Teapot description from the IEEE CG&A database to
 * the BRL-CAD spline format. (Note that this has the closed bottom)
 *
 */

#include "common.h"

#include "bio.h"

#include "vmath.h"		/* BRL-CAD Vector macros */
#include "nurb.h"		/* BRL-CAD Spline data structures */
#include "raytrace.h"
#include "wdb.h"

#include "./tea.h"		/* Teapot Data */


/* IEEE patch number of the Bi-Cubic Bezier patch and convert it
 * to a B-Spline surface (Bezier surfaces are a subset of B-spline surfaces
 * and output it to a BRL-CAD binary format.
 */
void
dump_patch(struct face_g_snurb **surfp, pt patch)
{
    struct face_g_snurb *b_patch;
    int i, j, pt_type;
    fastf_t *mesh_pointer;

    /* U and V parametric Direction Spline parameters
     * Cubic = order 4,
     * knot size is Control point + order = 4
     * control point size is 4
     * point size is 3
     */

    pt_type = RT_NURB_MAKE_PT_TYPE(3, 2, 0); /* see nurb.h for details */

    b_patch = (struct face_g_snurb *) rt_nurb_new_snurb(4, 4, 8, 8, 4, 4, pt_type, &rt_uniresource);
    *surfp = b_patch;

    /* Now fill in the pieces */

    /* Both u and v knot vectors are [ 0 0 0 0 1 1 1 1]
     * spl_kvknot(order, lower parametric value, upper parametric value,
     * Number of interior knots)
     */

    bu_free((char *)b_patch->u.knots, "dumping u knots I'm about to realloc");
    rt_nurb_kvknot(&b_patch->u, 4, 0.0, 1.0, 0, &rt_uniresource);

    bu_free((char *)b_patch->v.knots, "dumping v_kv knots I'm about to realloc");
    rt_nurb_kvknot(&b_patch->v, 4, 0.0, 1.0, 0, &rt_uniresource);

    /* Copy the control points */

    mesh_pointer = b_patch->ctl_points;

    for (i = 0; i< 4; i++) {
	for (j = 0; j < 4; j++) {
	    *mesh_pointer = ducks[patch[i][j]-1].x * 1000;
	    *(mesh_pointer+1) = ducks[patch[i][j]-1].y * 1000;
	    *(mesh_pointer+2) = ducks[patch[i][j]-1].z * 1000;
	    mesh_pointer += 3;
	}
    }
}


int
main(int argc, char **argv)
{
    const char *id_name = "Spline Example";
    const char *tea_name = "UtahTeapot";

    int i;
    struct rt_wdb *outfp;
    struct face_g_snurb **surfaces;

    rt_init_resource(&rt_uniresource, 0, NULL);

    outfp = wdb_fopen("teapot.g");

    while ((i=bu_getopt(argc, argv, "d")) != -1) {
	switch (i) {
	    case 'd':
		rt_g.debug |= DEBUG_MEM | DEBUG_MEM_FULL;
		break;
	    default:
		bu_exit(-1, "Usage: %s [-d]\n", *argv);
	}
    }

    /* Setup information
     * Database header record
     * File name
     * B-Spline Solid record
     * Name, Number of Surfaces, resolution (not used)
     */

    mk_id(outfp, id_name);

    /* Step through each patch and create a B_SPLINE surface
     * representing the patch then dump them out.
     */
    surfaces = (struct face_g_snurb **)bu_calloc(PATCH_COUNT+2, sizeof(struct face_g_snurb *), "surfaces");

    for (i = 0; i < PATCH_COUNT; i++) {
	dump_patch(&surfaces[i], patches[i]);
    }
    surfaces[PATCH_COUNT] = NULL;

    mk_bspline(outfp, tea_name, surfaces);

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
