/*                         S E T _ O U T P U T _ S C R I P T . C
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
/** @file libged/set_output_script.c
 *
 * The set_output_script command.
 *
 */

#include "common.h"
#include <string.h>
#include "bu/cmdschema.h"
#include "ged.h"

static const struct bu_cmd_operand set_output_script_operands[] = {
    BU_CMD_OPERAND("script", BU_CMD_VALUE_STRING, 0, 1,
	"Output handler script", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema set_output_script_cmd_schema = {
    "set_output_script", "Get or set the output handler script", NULL,
    set_output_script_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};

/*
 * Get/set the output handler script
 *
 * Usage:
 * set_output_script [script]
 *
 */
int
ged_set_output_script_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[script]";
    int parse_dummy = 0;
    int operand_index = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&set_output_script_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Get the output handler script */
	if (argc - 1 - operand_index == 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", gedp->ged_output_script);
	return BRLCAD_OK;
    }

    /* We're now going to set the output handler script */
    /* First, we zap any previous script */
    if (gedp->ged_output_script != NULL) {
	bu_free((void *)gedp->ged_output_script, "ged_set_output_script_core: zap");
	gedp->ged_output_script = NULL;
    }

    if (argv[1 + operand_index] != NULL && argv[1 + operand_index][0] != '\0')
	gedp->ged_output_script = bu_strdup(argv[1 + operand_index]);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SET_OUTPUT_SCRIPT_COMMANDS(X, XID) \
    X(set_output_script, ged_set_output_script_core, GED_CMD_DEFAULT, &set_output_script_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SET_OUTPUT_SCRIPT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_set_output_script", 1, GED_SET_OUTPUT_SCRIPT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
