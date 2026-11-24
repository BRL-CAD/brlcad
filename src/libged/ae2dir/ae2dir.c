/*                         A E 2 D I R . C
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
/** @file libged/ae2dir.c
 *
 * The ae2dir command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/arg.h"

#include "../ged_private.h"


struct ae2dir_args {
    int inverse;
};

static const struct bu_arg_option ae2dir_options[] = {
    BU_ARG_FLAG("i", NULL, struct ae2dir_args, inverse,
	"Return the inverse direction"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand ae2dir_operands[] = {
    BU_ARG_OPERAND("angles", BU_ARG_VALUE_NUMBER, 2, 2,
	"Azimuth and elevation in degrees", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema ae2dir_cmd_schema =
    BU_ARG_SCHEMA_BOUND("ae2dir",
	"Convert azimuth/elevation to a direction vector", ae2dir_options,
	ae2dir_operands, BU_ARG_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);


int
ged_ae2dir_core(struct ged *gedp, int argc, const char *argv[])
{
    double az, el;
    vect_t dir;
    struct ae2dir_args args = {0};
    int operand_index;
    const char *command = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&ae2dir_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    argv += operand_index;

    if (sscanf(argv[0], "%lf", &az) != 1 ||
	sscanf(argv[1], "%lf", &el) != 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }

    az *= DEG2RAD;
    el *= DEG2RAD;
    bn_vec_ae(dir, az, el);

	if (args.inverse)
	VSCALE(dir, dir, -1);

    bn_encode_vect(gedp->ged_result_str, dir, 1);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_AE2DIR_COMMANDS(X, XID) \
    X(ae2dir, ged_ae2dir_core, GED_CMD_DEFAULT, &ae2dir_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_AE2DIR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_ae2dir", 1, GED_AE2DIR_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
