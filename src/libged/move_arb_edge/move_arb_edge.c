/*                         M O V E _ A R B _ E D G E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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

#include "bu/cmd.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "raytrace.h"

#include "../ged_private.h"


int
ged_move_arb_edge_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    plane_t planes[6];  /* ARBs defining plane equations */
    int arb_type;
    int arb_pt_index;
    int edge;
    int bad_edge_id = 0;
    int rflag = 0;
    point_t pt;
    double scan[3];
    mat_t mat;
    char *last;
    struct directory *dp;
    static const char *usage = "[-r] arb edge pt";
    const short arb8_evm[12][2] = arb8_edge_vertex_mapping;
    const short arb7_evm[12][2] = arb7_edge_vertex_mapping;
    const short arb6_evm[10][2] = arb6_edge_vertex_mapping;
    const short arb5_evm[9][2] = arb5_edge_vertex_mapping;
    const short arb4_evm[5][2] = arb4_edge_vertex_mapping;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
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
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &edge) != 1) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[2]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }
    edge -= 1;

    if (sscanf(argv[3], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "bad point - %s", argv[3]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(pt, scan);

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    /* check the arb type */
    switch (arb_type) {
	case ARB4:
	    if (edge < 0 || 4 < edge) {
		bad_edge_id = 1;
		goto bad_edge;
	    }

	    arb_pt_index = arb4_evm[edge][0];
	    break;
	case ARB5:
	    if (edge < 0 || 8 < edge) {
		bad_edge_id = 1;
		goto bad_edge;
	    }

	    arb_pt_index = arb5_evm[edge][0];
	    break;
	case ARB6:
	    if (edge < 0 || 9 < edge) {
		bad_edge_id = 1;
		goto bad_edge;
	    }

	    arb_pt_index = arb6_evm[edge][0];
	    break;
	case ARB7:
	    if (edge < 0 || 11 < edge) {
		bad_edge_id = 1;
		goto bad_edge;
	    }

	    arb_pt_index = arb7_evm[edge][0];
	    break;
	case ARB8:
	    if (edge < 0 || 11 < edge) {
		bad_edge_id = 1;
		goto bad_edge;
	    }

	    arb_pt_index = arb8_evm[edge][0];
	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "unrecognized arb type");
	    rt_db_free_internal(&intern);

	    return BRLCAD_ERROR;
    }

bad_edge:

    /* check the edge id */
    if (bad_edge_id) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[2]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (rt_arb_calc_planes(gedp->ged_result_str, arb, arb_type, planes, &wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    VSCALE(pt, pt, gedp->dbip->dbi_local2base);

    if (rflag) {
	VADD2(pt, pt, arb->pt[arb_pt_index]);
    }

    if (rt_arb_edit(gedp->ged_result_str, arb, arb_type, edge, pt, planes, &wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
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

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);
    }

    return BRLCAD_OK;
}


int
ged_find_arb_edge_nearest_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "arb view_xyz ptol";
    struct rt_db_internal intern;
    mat_t mat;
    int edge, vi1, vi2;
    vect_t view;
    fastf_t ptol;

    /* must be double for scanf */
    double scan[ELEMENTS_PER_VECT];
    double ptol_scan;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (bu_sscanf(argv[2], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad view location - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }
    VMOVE(view, scan); /* convert double to fastf_t */

    if (bu_sscanf(argv[3], "%lf", &ptol_scan) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad ptol - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }
    ptol = ptol_scan;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) == BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "Object is not an ARB");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    (void)rt_arb_find_e_nearest_pt2(&edge, &vi1, &vi2, &intern, view, gedp->ged_gvp->gv_model2view, ptol);
    bu_vls_printf(gedp->ged_result_str, "%d %d %d", edge, vi1, vi2);

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl move_arb_edge_cmd_impl = {
    "move_arb_edge",
    ged_move_arb_edge_core,
    GED_CMD_DEFAULT
};
const struct ged_cmd move_arb_edge_cmd = { &move_arb_edge_cmd_impl };

struct ged_cmd_impl find_arb_edge_cmd_impl = {
    "find_arb_edge",
    ged_find_arb_edge_nearest_pnt_core,
    GED_CMD_DEFAULT
};
const struct ged_cmd find_arb_edge_cmd = { &find_arb_edge_cmd_impl };

const struct ged_cmd *move_arb_edge_cmds[] = { &move_arb_edge_cmd, &find_arb_edge_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  move_arb_edge_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
