/*                         M O V E _ A R B _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/move_arb_face.c
 *
 * The move_arb_face command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


/* The arbX_faces arrays are used for relative face movement. */
static const int arb8_faces_first_vertex[6] = {
    0, 4, 0, 1, 0, 2
};


static const int arb7_faces_first_vertex[6] = {
    0, 0, 1, 1, 1
};


static const int arb6_faces_first_vertex[5] = {
    0, 1, 0, 0, 2
};


static const int arb5_faces_first_vertex[5] = {
    0, 0, 1, 2, 0
};


static const int arb4_faces_first_vertex[4] = {
    0, 0, 1, 0
};


int
ged_move_arb_face(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    int rflag = 0;
    point_t pt;
    mat_t mat;
    char *last;
    struct directory *dp;
    static const char *usage = "[-r] arb face pt";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	rflag = 1;
	--argc;
	++argv;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(&gedp->ged_result_str, "illegal input - %s", argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s not found", argv[1]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(&gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(&gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern);

	return TCL_OK;
    }

    if (sscanf(argv[2], "%d", &face) != 1) {
	bu_vls_printf(&gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    /*XXX need better checking of the face */
    face -= 1;
    if (face < 0 || 5 < face) {
	bu_vls_printf(&gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    if (sscanf(argv[3], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	bu_vls_printf(&gedp->ged_result_str, "bad point - %s", argv[3]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);

    if (rt_arb_calc_planes(&gedp->ged_result_str, arb, arb_type, planes, &gedp->ged_wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    VSCALE(pt, pt, gedp->ged_wdbp->dbip->dbi_local2base);

    if (rflag) {
	int arb_pt_index;

	switch (arb_type) {
	    case ARB4:
		arb_pt_index = arb4_faces_first_vertex[face];
		break;
	    case ARB5:
		arb_pt_index = arb5_faces_first_vertex[face];
		break;
	    case ARB6:
		arb_pt_index = arb6_faces_first_vertex[face];
		break;
	    case ARB7:
		arb_pt_index = arb7_faces_first_vertex[face];
		break;
	    case ARB8:
		arb_pt_index = arb8_faces_first_vertex[face];
		break;
	    default:
		bu_vls_printf(&gedp->ged_result_str, "unrecognized arb type");
		rt_db_free_internal(&intern);

		return GED_ERROR;
	}

	VADD2(pt, pt, arb->pt[arb_pt_index]);
    }

    /* change D of planar equation */
    planes[face][3] = VDOT(&planes[face][0], pt);

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, planes, &gedp->ged_wdbp->wdb_tol);

    {
	int i;
	mat_t invmat;

	bn_mat_inv(invmat, mat);

	for (i = 0; i < 8; ++i) {
	    point_t arb_pt;

	    MAT4X3PNT(arb_pt, invmat, arb->pt[i]);
	    VMOVE(arb->pt[i], arb_pt);
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    return GED_OK;
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
