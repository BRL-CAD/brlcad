/*                         O T R A N S L A T E . C
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
/** @file libged/otranslate.c
 *
 * The otranslate command.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"

#include "../ged_private.h"


static const struct bu_cmd_operand otranslate_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Object path to translate", "ged.db_path"),
    BU_CMD_OPERAND("translation", BU_CMD_VALUE_NUMBER, 3, 3,
	"X Y Z translation", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema otranslate_cmd_schema = {
    "otranslate", "Translate an object", NULL, otranslate_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_otranslate_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct _ged_trace_data gtd;
    struct rt_db_internal intern;
    vect_t delta;
    double scan[3];
    mat_t dmat;
    mat_t emat;
    mat_t tmpMat;
    mat_t invXform;
    point_t rpp_min;
    point_t rpp_max;
    static const char *usage = "obj dx dy dz";
    int operand_index = 0;
    int parse_dummy = 0;
    const char *object = NULL;
    const char *translation[3] = {NULL, NULL, NULL};
    const char *object_argv[1] = {NULL};

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

	operand_index = bu_cmd_schema_parse_complete(&otranslate_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    object = argv[1 + operand_index];
    object_argv[0] = object;
    translation[0] = argv[2 + operand_index];
    translation[1] = argv[3 + operand_index];
    translation[2] = argv[4 + operand_index];

	if (_ged_get_obj_bounds2(gedp, 1, object_argv, &gtd, rpp_min, rpp_max) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    dp = gtd.gtd_obj[gtd.gtd_objpos-1];
    if (!(dp->d_flags & RT_DIR_SOLID)) {
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, object_argv, 1, rpp_min, rpp_max) == BRLCAD_ERROR)
	    return BRLCAD_ERROR;
    }

	if (sscanf(translation[0], "%lf", &scan[X]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad x value - %s", argv[0], translation[0]);
	return BRLCAD_ERROR;
    }

	if (sscanf(translation[1], "%lf", &scan[Y]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad y value - %s", argv[0], translation[1]);
	return BRLCAD_ERROR;
    }

	if (sscanf(translation[2], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad z value - %s", argv[0], translation[2]);
	return BRLCAD_ERROR;
    }

    MAT_IDN(dmat);
    VSCALE(delta, scan, gedp->dbip->dbi_local2base);
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

#define GED_OTRANSLATE_COMMANDS(X, XID) \
    X(otranslate, ged_otranslate_core, GED_CMD_DEFAULT, &otranslate_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_OTRANSLATE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_otranslate", 1, GED_OTRANSLATE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
