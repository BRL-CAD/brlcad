/*                         M O V E _ A R B _ E D G E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "rt/primitives/arb8.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "../move_arb.h"

struct move_arb_edge_args {
    int relative;
};

static const struct bu_cmd_option move_arb_edge_schema_options[] = {
    BU_CMD_FLAG("r", NULL, struct move_arb_edge_args, relative,
	"Interpret point relative to current edge"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand move_arb_edge_schema_operands[] = {
    BU_CMD_OPERAND("arb", BU_CMD_VALUE_DB_PATH, 1, 1, "ARB object or path", "ged.db_path"),
    BU_CMD_OPERAND_VALIDATE("edge", BU_CMD_VALUE_INTEGER, 1, 1,
	ged_arb_positive_integer_validate, "One-based edge index", NULL),
    BU_CMD_OPERAND_VALIDATE("point", BU_CMD_VALUE_VECTOR, 1, 1,
	ged_arb_vector3_validate, "Packed XYZ point", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema move_arb_edge_cmd_schema = {
    "move_arb_edge", "Move an ARB edge", move_arb_edge_schema_options,
    move_arb_edge_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_operand find_arb_edge_schema_operands[] = {
    BU_CMD_OPERAND("arb", BU_CMD_VALUE_DB_PATH, 1, 1, "ARB object or path", "ged.db_path"),
    BU_CMD_OPERAND_VALIDATE("view_point", BU_CMD_VALUE_VECTOR, 1, 1,
	ged_arb_vector3_validate, "Packed view XYZ point", NULL),
    BU_CMD_OPERAND("tolerance", BU_CMD_VALUE_NUMBER, 1, 1, "Point tolerance", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema find_arb_edge_cmd_schema = {
    "find_arb_edge", "Find the ARB edge nearest a view point", NULL,
    find_arb_edge_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


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
    struct move_arb_edge_args args = {0};
    int operand_index;
    const char *arb_path;
    const char *edge_arg;
    const char *point_arg;
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

    operand_index = bu_cmd_schema_parse_complete(&move_arb_edge_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    arb_path = argv[operand_index + 1];
    edge_arg = argv[operand_index + 2];
    point_arg = argv[operand_index + 3];

    if ((last = strrchr(arb_path, '/')) == NULL)
	last = (char *)arb_path;
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "illegal input - %s", arb_path);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s not found", arb_path);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, arb_path, wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    edge = (int)strtol(edge_arg, NULL, 0);
    edge -= 1;

    (void)ged_arb_vector3_parse(point_arg, scan);
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
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", edge_arg);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (rt_arb_calc_planes(gedp->ged_result_str, arb, arb_type, planes, &wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    VSCALE(pt, pt, gedp->dbip->dbi_local2base);

    if (args.relative) {
	VADD2(pt, pt, arb->pt[arb_pt_index]);
    }

    if (rt_arb_edit(gedp->ged_result_str, arb, NULL, arb_type, edge, RT_ARB_EDIT_DEFAULT, pt, planes, &wdbp->wdb_tol)) {
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

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
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
    int operand_index;
    const char *arb_path;
    const char *view_point;
    const char *tolerance;

    double scan[ELEMENTS_PER_VECT];

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

    operand_index = bu_cmd_schema_parse_complete(&find_arb_edge_cmd_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    arb_path = argv[operand_index + 1];
    view_point = argv[operand_index + 2];
    tolerance = argv[operand_index + 3];

    (void)ged_arb_vector3_parse(view_point, scan);
    VMOVE(view, scan); /* convert double to fastf_t */

    ptol = (fastf_t)strtod(tolerance, NULL);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, arb_path, wdbp, mat) == BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], arb_path);
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


#include "../include/plugin.h"

#define GED_MOVE_ARB_EDGE_COMMANDS(X, XID) \
    X(move_arb_edge, ged_move_arb_edge_core, GED_CMD_DEFAULT, &move_arb_edge_cmd_schema) \
    X(find_arb_edge, ged_find_arb_edge_nearest_pnt_core, GED_CMD_DEFAULT, &find_arb_edge_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MOVE_ARB_EDGE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_move_arb_edge", 1, GED_MOVE_ARB_EDGE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
