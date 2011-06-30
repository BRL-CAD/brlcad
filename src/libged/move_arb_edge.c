/*                         M O V E _ A R B _ E D G E . C
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
/** @file libged/move_arb_edge.c
 *
 * The move_arb_edge command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


int
ged_move_arb_edge(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int arb_pt_index;
    int edge;
    int bad_edge_id = 0;
    int rflag = 0;
    point_t pt;
    mat_t mat;
    char *last;
    struct directory *dp;
    static const char *usage = "[-r] arb edge pt";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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
	bu_vls_printf(gedp->ged_result_str, "illegal input - %s", argv[1]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s not found", argv[1]);
	return GED_ERROR;
    }

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &edge) != 1) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[2]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }
    edge -= 1;

    if (sscanf(argv[3], "%lf %lf %lf", &pt[X], &pt[Y], &pt[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "bad point - %s", argv[3]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);

    /* check the arb type */
    switch (arb_type) {
	case ARB4:
	    if (edge < 0 || 4 < edge)
		bad_edge_id = 1;
	    arb_pt_index = arb4_edge_vertex_mapping[edge][0];
	    break;
	case ARB5:
	    if (edge < 0 || 8 < edge)
		bad_edge_id = 1;
	    arb_pt_index = arb5_edge_vertex_mapping[edge][0];
	    break;
	case ARB6:
	    if (edge < 0 || 9 < edge)
		bad_edge_id = 1;
	    arb_pt_index = arb6_edge_vertex_mapping[edge][0];
	    break;
	case ARB7:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    arb_pt_index = arb7_edge_vertex_mapping[edge][0];
	    break;
	case ARB8:
	    if (edge < 0 || 11 < edge)
		bad_edge_id = 1;
	    arb_pt_index = arb8_edge_vertex_mapping[edge][0];
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "unrecognized arb type");
	    rt_db_free_internal(&intern);

	    return GED_ERROR;
    }

    /* check the edge id */
    if (bad_edge_id) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[2]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    if (rt_arb_calc_planes(gedp->ged_result_str, arb, arb_type, planes, &gedp->ged_wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    VSCALE(pt, pt, gedp->ged_wdbp->dbip->dbi_local2base);

    if (rflag) {
	VADD2(pt, pt, arb->pt[arb_pt_index]);
    }

    if (rt_arb_edit(gedp->ged_result_str, arb, arb_type, edge, pt, planes, &gedp->ged_wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

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
