/*                         E C H O . C
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
/** @file libged/echo.c
 *
 * The echo command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_operand echo_operands[] = {
    BU_CMD_OPERAND("text", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Text to echo", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema echo_cmd_schema = {
    "echo", "Echo text to the command result", NULL, echo_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


int
ged_echo_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int parse_dummy = 0;
    int operand_index = 0;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&echo_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	return BRLCAD_ERROR;
    }

	for (i = 1 + operand_index; i < argc; i++) {
	bu_vls_printf(gedp->ged_result_str, "%s%s", i == 1 + operand_index ? "" : " ", argv[i]);
    }

    bu_vls_printf(gedp->ged_result_str, "\n");

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_ECHO_COMMANDS(X, XID) \
    X(echo, ged_echo_core, GED_CMD_DEFAULT, &echo_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ECHO_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_echo", 1, GED_ECHO_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
