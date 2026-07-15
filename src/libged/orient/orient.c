/*                         O R I E N T . C
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
/** @file libged/orient.c
 *
 * The orient command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_orient_core(struct ged *gedp, int argc, const char *argv[])
{
    quat_t quat;
    static const char *usage = "quat";

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

    if (argc != 2 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* set view orientation */
    if (argc == 2) {
	if (bn_decode_quat(quat, argv[1]) != 4) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	int i;

	for (i = 1; i < 5; ++i) {
	    double scan;
	    if (sscanf(argv[i], "%lf", &scan) != 1) {
		bu_vls_printf(gedp->ged_result_str, "ged_orient_core: bad value - %s\n", argv[i-1]);
		return BRLCAD_ERROR;
	    }
	    /* convert from double to fastf_t */
	    quat[i-1] = scan;
	}
    }

    quat_quat2mat(gedp->ged_gvp->gv_rotation, quat);
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_operand orient_schema_operands[] = {
    BU_CMD_OPERAND("quaternion", BU_CMD_VALUE_VECTOR, 1, 4, "Packed quaternion or four components", NULL),
    BU_CMD_OPERAND_NULL
};
GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(orient, 1, 4, GED_SCHEMA_COUNT_NONE)
static const struct bu_cmd_schema orient_cmd_schema = {"orient", "Set view orientation from a quaternion", NULL, orient_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {orient_schema_validate}};
static const struct bu_cmd_schema orientation_cmd_schema = {"orientation", "Set view orientation from a quaternion", NULL, orient_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {orient_schema_validate}};

#define GED_ORIENT_COMMANDS(X, XID) \
    X(orient, ged_orient_core, GED_CMD_DEFAULT, &orient_cmd_schema) \
    X(orientation, ged_orient_core, GED_CMD_DEFAULT, &orientation_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ORIENT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_orient", 1, GED_ORIENT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
