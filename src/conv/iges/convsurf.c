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
#include "./iges_surf.h"


/* Analytic / spline surface types (other than 128) that Get_iges_nurb_surf()
 * can convert to a NURBS surface: ruled (118), surface of revolution (120),
 * tabulated cylinder (122), offset (140), and parametric spline (114). */
static int
is_analytic_surf(int type)
{
    return (type == 114 || type == 118 || type == 120 ||
	    type == 122 || type == 140);
}


void
Convsurfs(void)
{

    size_t i;
    size_t totsurfs = 0;
    size_t convsurf = 0;
    struct face_g_snurb **surfs;
    struct face_g_snurb *srf;

    bu_log("\n\nConverting NURB entities:\n");

    /* First count the number of surfaces (rational B-spline plus the analytic
     * and spline surface types we can convert to NURBS) */
    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 128 || is_analytic_surf(dir[i]->type))
	    totsurfs++;
    }

    surfs = (struct face_g_snurb **)bu_calloc(totsurfs+1, sizeof(*surfs), "surfs");

    for (i = 0; i < totentities; i++) {
	if (dir[i]->type == 128) {
	    if (iges_spline(i, &srf))
		surfs[convsurf++] = srf;
	} else if (is_analytic_surf(dir[i]->type)) {
	    /* built as an independent snurb (no NMG model needed here) */
	    srf = Get_iges_nurb_surf(i, (struct model *)NULL);
	    if (srf)
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
