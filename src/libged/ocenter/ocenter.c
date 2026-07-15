/*                         O C E N T E R . C
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
/** @file libged/ocenter.c
 *
 * The ocenter command.
 *
 */

#include "common.h"


#include "../ged_private.h"


int
ged_ocenter_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct _ged_trace_data gtd;
    struct rt_db_internal intern;
    mat_t dmat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;
    point_t oldCenter;
    point_t center;
    point_t delta;
    double scan[3];
    static const char *usage = "object [x y z]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /*
     * One of the get bounds routines needs to be fixed to
     * work with all cases. In the meantime...
     */
    if (_ged_get_obj_bounds2(gedp, 1, argv+1, &gtd, rpp_min, rpp_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    dp = gtd.gtd_obj[gtd.gtd_objpos-1];
    if (!(dp->d_flags & RT_DIR_SOLID)) {
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, argv+1, 1, rpp_min, rpp_max) == BRLCAD_ERROR)
	    return BRLCAD_ERROR;
    }

    VADD2(oldCenter, rpp_min, rpp_max);
    VSCALE(oldCenter, oldCenter, 0.5);

    if (argc == 2) {
	VSCALE(center, oldCenter, gedp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, center, 1);

	return BRLCAD_OK;
    }

    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Read in the new center */
    if (sscanf(argv[2], "%lf", &scan[X]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad x value - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &scan[Y]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad y value - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad z value - %s", argv[0], argv[4]);
	return BRLCAD_ERROR;
    }

    VSCALE(center, scan, gedp->dbip->dbi_local2base);
    VSUB2(delta, center, oldCenter);
    MAT_IDN(dmat);
    MAT_DELTAS_VEC(dmat, delta);

    bn_mat_inv(invXform, gtd.gtd_xform);
    bn_mat_mul(tmpMat, invXform, dmat);
    bn_mat_mul(emat, tmpMat, gtd.gtd_xform);

    GED_DB_GET_INTERN(gedp, &intern, dp, emat, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);
    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_operand ocenter_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1, "Object path", NULL),
    BU_CMD_OPERAND("center", BU_CMD_VALUE_NUMBER, 0, 3, "Optional X Y Z center", NULL),
    BU_CMD_OPERAND_NULL
};
GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(ocenter, 1, 4, GED_SCHEMA_COUNT_NONE)
static const struct bu_cmd_schema ocenter_cmd_schema = {"ocenter", "Query or set an object center", NULL, ocenter_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {ocenter_schema_validate}};

#define GED_OCENTER_COMMANDS(X, XID) \
    X(ocenter, ged_ocenter_core, GED_CMD_DEFAULT, &ocenter_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_OCENTER_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_ocenter", 1, GED_OCENTER_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
