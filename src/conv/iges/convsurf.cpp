/*                      C O N V S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2014 United States Government as represented by
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
/** @file iges/convsurf.cpp
 *
 * This routine loops through all the directory entries and calls
 * appropriate routines to convert solid entities to BRL-CAD
 * equivalents.
 *
 */


#define identity vector_identity_arg
#include "./iges_struct.h"
#undef identity
#include "./iges_extern.h"


ON_NurbsSurface *
snurb_to_surface(const face_g_snurb &snurb)
{
    const int dimension = RT_NURB_EXTRACT_COORDS(snurb.pt_type);
    const int rows = snurb.s_size[0];
    const int cols = snurb.s_size[1];

    if (dimension != 3) return NULL;

    ON_NurbsSurface *surface = ON_NurbsSurface::New();

    if (!surface->Create(dimension, RT_NURB_IS_PT_RATIONAL(snurb.pt_type),
			 snurb.order[0],
			 snurb.order[1], snurb.s_size[0],
			 snurb.s_size[1])) {
	delete surface;
	return NULL;
    }

    for (int i = 0; i < rows; ++i)
	for (int j = 0; j < cols; ++j) {
	    const fastf_t *point = &snurb.ctl_points[(i * rows + j) * dimension];

	    if (!surface->SetCV(i, j, ON_3dPoint(point[0], point[1], point[2]))) {
		delete surface;
		return NULL;
	    }
	}

    return surface;
}


void
Convsurfs()
{
    bu_log("\n\nConverting NURB entities:\n");

    int num_surfaces = 0, converted_surfs = 0;
    ON_Brep brep;

    for (int i = 0; i < totentities; ++i)
	if (dir[i]->type == 128) {
	    ++num_surfaces;
	    face_g_snurb *snurb;

	    if (!read_spline(i, &snurb)) continue;

	    ON_NurbsSurface *surface = snurb_to_surface(*snurb);
	    rt_nurb_free_snurb(snurb, NULL);

	    if (!surface || brep.AddSurface(surface) == -1) {
		delete surface;
		bu_log("failed to add surface\n");
		continue;
	    }

	    ++converted_surfs;
	}

    const char * const name = BU_STR_EMPTY(curr_file->obj_name) ? "nurb.s" :
			      curr_file->obj_name;

    if (mk_brep(fdout, name, &brep) != 0) {
	bu_log("mk_brep() failed\n");
	converted_surfs = 0;
    }

    bu_log("Converted %d NURBS successfully out of %d total NURBS\n",
	   converted_surfs, num_surfaces);

    if (converted_surfs)
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
