/*                         M O D E L 2 V I E W . C
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

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


/**
 * Given a point in model space coordinates (in mm) convert it to view
 * (screen) coordinates which must be scaled to correspond to actual
 * screen coordinates. If no input coordinates are supplied, the
 * model2view matrix is displayed.
 */
int
ged_model2view_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t view_pt;
    double model_pt[3]; /* intentionally double for scan */
    static const char *usage = "[x y z]";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the model2view matrix */
    if (argc == 1) {
	bn_encode_mat(gedp->ged_result_str, gedp->ged_gvp->gv_model2view, 1);
	return BRLCAD_OK;
    }

    if (argc != 4
	|| sscanf(argv[1], "%lf", &model_pt[X]) != 1
	|| sscanf(argv[2], "%lf", &model_pt[Y]) != 1
	|| sscanf(argv[3], "%lf", &model_pt[Z]) != 1)
    {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    bn_encode_vect(gedp->ged_result_str, view_pt, 1);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_operand model2view_schema_operands[] = {
    BU_CMD_OPERAND("point", BU_CMD_VALUE_NUMBER, 0, 3, "Optional X Y Z model point", NULL),
    BU_CMD_OPERAND_NULL
};
GED_DEFINE_NATIVE_DISCRETE_COUNT_VALIDATOR(model2view, 0, 3, GED_SCHEMA_COUNT_NONE)
static const struct bu_cmd_schema model2view_cmd_schema = {"model2view", "Query the transform or convert a model point", NULL, model2view_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {model2view_schema_validate}};

#define GED_MODEL2VIEW_COMMANDS(X, XID) \
    X(model2view, ged_model2view_core, GED_CMD_DEFAULT, &model2view_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MODEL2VIEW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_model2view", 1, GED_MODEL2VIEW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
