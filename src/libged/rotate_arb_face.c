/*                         R O T A T E _ A R B _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file rotate_arb_face.c
 *
 * The rotate_arb_face command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "rtgeom.h"
#include "ged_private.h"

static short int ged_arb_vertices[5][24] = {
    { 1, 2, 3, 0, 1, 2, 4, 0, 2, 3, 4, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* arb4 */
    { 1, 2, 3, 4, 1, 2, 5, 0, 2, 3, 5, 0, 3, 4, 5, 0, 1, 4, 5, 0, 0, 0, 0, 0 },	/* arb5 */
    { 1, 2, 3, 4, 2, 3, 6, 5, 1, 5, 6, 4, 1, 2, 5, 0, 3, 4, 6, 0, 0, 0, 0, 0 },	/* arb6 */
    { 1, 2, 3, 4, 5, 6, 7, 0, 1, 4, 5, 0, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 5 },	/* arb7 */
    { 1, 2, 3, 4, 5, 6, 7, 8, 1, 5, 8, 4, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 8 }	/* arb8 */
};

int
ged_rotate_arb_face(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    int vi;
    point_t pt;
    register int i;
    int pnt5;		/* special arb7 case */
    static const char *usage = "arb face pt";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc != 5) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (gedp->ged_wdbp->dbip == 0) {
	bu_vls_printf(&gedp->ged_result_str, "db does not support lookup operations");
	return BRLCAD_ERROR;
    }

    if (wdb_import_from_path(&gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp) == BRLCAD_ERROR)
	return BRLCAD_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(&gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	bu_vls_printf(&gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &vi) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "bad vertex index - %s", argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }


    /*XXX need better checking of the vertex index */
    vi -= 1;
    if (vi < 0 || 7 < vi) {
	bu_vls_printf(&gedp->ged_result_str, "bad vertex - %s", argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	bu_vls_printf(&gedp->ged_result_str, "bad point - %s", argv[3]);
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);

    if (rt_arb_calc_planes(&gedp->ged_result_str, arb, arb_type, planes, &gedp->ged_wdbp->wdb_tol)) {
	rt_db_free_internal(&intern, &rt_uniresource);

	return BRLCAD_ERROR;
    }

    /* check if point 5 is in the face */
    pnt5 = 0;
    for (i=0; i<4; i++)  {
	if (ged_arb_vertices[arb_type-4][face*4+i]==5)
	    pnt5=1;
    }

    /* special case for arb7 */
    if (arb_type == ARB7  && pnt5)
	vi = 4;

    {
	/* Apply incremental changes */
	vect_t tempvec;
	vect_t work;
	fastf_t	*plane;
	mat_t rmat;

	bn_mat_angles(rmat, pt[X], pt[Y], pt[Z]);

	plane = &planes[face][0];
	VMOVE(work, plane);
	MAT4X3VEC(plane, rmat, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[vi]);

	/* set D of planar equation to anchor at fixed vertex */
	planes[face][3]=VDOT(plane, tempvec);
    }

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, planes, &gedp->ged_wdbp->wdb_tol);

    {
	register int i;

	for (i = 0; i < 8; ++i) {
	    bu_vls_printf(&gedp->ged_result_str, "V%d {%g %g %g} ",
			  i + 1,
			  arb->pt[i][X],
			  arb->pt[i][Y],
			  arb->pt[i][Z]);
	}
    }

    rt_db_free_internal(&intern, &rt_uniresource);
    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
