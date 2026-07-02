/*                      C O N V S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file iges/convsurf.c
 *
 * This routine loops through all the directory entries and calls
 * appropriate routines to convert solid entities to BRL-CAD
 * equivalents.
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "./iges_brep.h"


void
Convsurfs(void)
{

    size_t i;
    size_t totsurfs = 0;
    size_t convsurf = 0;
    struct face_g_snurb **surfs;
    struct face_g_snurb *srf;

    bu_log("\n\nConverting NURB entities:\n");

    /* First count the number of surfaces */
    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 128)
	    totsurfs++;
    }

    surfs = (struct face_g_snurb **)bu_calloc(totsurfs+1, sizeof(struct face_g_snurb *), "surfs");

    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 128) {
	    if (iges_spline(i, &srf))
		surfs[convsurf++] = srf;
	}
    }

    if (totsurfs) {
	const char *nm = !BU_STR_EMPTY(curr_file->obj_name) ? curr_file->obj_name : "nurb.s";

	/* Preferred output is a faithful rt_brep (OpenNURBS) surface model;
	 * fall back to the older bspline solid if requested (-m/-p) or if brep
	 * construction fails. */
	if (do_brep && iges_snurbs_to_brep(fdout, nm, surfs, (int)convsurf)) {
	    for (i = 0; i < convsurf; i++)
		nmg_nurb_free_snurb(surfs[i]);
	} else {
	    if (do_brep)
		bu_log("Falling back to a bspline solid for %s\n", nm);
	    mk_bspline(fdout, nm, surfs);	/* consumes surfs */
	}
    }

    bu_log("Converted %zu NURBS successfully out of %zu total NURBS\n", convsurf, totsurfs);
    if (convsurf)
	bu_log("\tCaution: All NURBS are assumed to be part of the same solid\n");
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
