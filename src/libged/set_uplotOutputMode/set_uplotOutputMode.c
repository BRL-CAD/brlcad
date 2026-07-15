/*                         S E T _ U P L O T O U T P U T M O D E . C
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
/** @file libged/set_uplotOutputMode.c
 *
 * The set_uplotOutputMode command.
 *
 */

#include "common.h"
#include <string.h>
#include "bv/plot3.h"
#include "bu/cmdschema.h"
#include "ged.h"
#include "../ged_private.h"

static const char *set_uplotOutputMode_keywords[] = {"binary", "text", NULL};

static const struct bu_cmd_operand set_uplotOutputMode_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("mode", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Plot output mode", NULL, set_uplotOutputMode_keywords),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema set_uplotOutputMode_cmd_schema = {
    "set_uplotOutputMode", "Get or set the plot output mode", NULL,
    set_uplotOutputMode_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};

/*
 * Set/get the unix plot output mode
 *
 * Usage:
 * set_uplotOutputMode [binary|text]
 *
 */
int
ged_set_uplotOutputMode_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[binary|text]";
    int parse_dummy = 0;
    int operand_index = 0;
    const char *mode = NULL;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

	operand_index = bu_cmd_schema_parse_complete(&set_uplotOutputMode_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* Get the plot output mode */
    if (argc == 1) {
	if (gedp->i->ged_gdp->gd_uplotOutputMode == PL_OUTPUT_MODE_BINARY)
	    bu_vls_printf(gedp->ged_result_str, "binary");
	else
	    bu_vls_printf(gedp->ged_result_str, "text");

	return BRLCAD_OK;
    }

	mode = argv[1 + operand_index];
    if (BU_STR_EQUAL("binary", mode))
	gedp->i->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
	else if (BU_STR_EQUAL("text", mode))
	gedp->i->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_TEXT;
    else {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SET_UPLOTOUTPUTMODE_COMMANDS(X, XID) \
    X(set_uplotOutputMode, ged_set_uplotOutputMode_core, GED_CMD_DEFAULT, &set_uplotOutputMode_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SET_UPLOTOUTPUTMODE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_set_uplotOutputMode", 1, GED_SET_UPLOTOUTPUTMODE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
