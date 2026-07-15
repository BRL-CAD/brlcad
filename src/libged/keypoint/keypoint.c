/*                         K E Y P O I N T . C
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
/** @file libged/keypoint.c
 *
 * The keypoint command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_keypoint_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t keypoint;
    double scan[3];
    static const char *usage = "[x y z]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get keypoint */
    if (argc == 1) {
	VSCALE(keypoint, gedp->ged_gvp->gv_keypoint, gedp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, keypoint, 1);

	return BRLCAD_OK;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* set view keypoint */
    if (argc == 2) {
	if (bn_decode_vect(keypoint, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint_core: bad X value - %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint_core: bad Y value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_keypoint_core: bad Z value - %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(keypoint, scan);
    }

    VSCALE(gedp->ged_gvp->gv_keypoint, keypoint, gedp->dbip->dbi_local2base);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_operand keypoint_schema_operands[] = {
    BU_CMD_OPERAND("point", BU_CMD_VALUE_VECTOR, 0, 3, "Packed vector or X Y Z point", NULL),
    BU_CMD_OPERAND_NULL
};
GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(keypoint, 0, 1, 3)
static const struct bu_cmd_schema keypoint_cmd_schema = {"keypoint", "Query or set the view keypoint", NULL, keypoint_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {keypoint_schema_validate}};

#define GED_KEYPOINT_COMMANDS(X, XID) \
    X(keypoint, ged_keypoint_core, GED_CMD_DEFAULT, &keypoint_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_KEYPOINT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_keypoint", 1, GED_KEYPOINT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
