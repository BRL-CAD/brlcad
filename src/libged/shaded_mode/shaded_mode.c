/*                         S H A D E D _ M O D E . C
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
/** @file libged/shaded_mode.c
 *
 * The shaded_mode command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/arg.h"

#include "../ged_private.h"

static const char * const shaded_mode_keywords[] = {"0", "1", "2", NULL};
static const struct bu_arg_operand shaded_mode_operands[] = {
    BU_ARG_OPERAND_KEYWORDS("mode", BU_ARG_VALUE_KEYWORD, 0, 1,
	"Display shading mode", NULL, shaded_mode_keywords),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_schema shaded_mode_cmd_schema =
    BU_ARG_SCHEMA("shaded_mode", "Query or set display shading mode", NULL,
	shaded_mode_operands, BU_ARG_PARSE_OPTIONS_FIRST, NULL);

/*
 * Set/get the shaded mode.
 *
 * Usage:
 * shaded_mode [0|1|2]
 *
 */
int
ged_shaded_mode_core(struct ged *gedp, int argc, const char *argv[])
{
    int operand_index;
    const char *command = argv[0];

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&shaded_mode_cmd_schema, NULL,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    argv += operand_index;

    /* get shaded mode */
	if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%d", gedp->i->ged_gdp->gd_shaded_mode);
	return BRLCAD_OK;
    }

    /* set shaded mode */
	if (argc == 1) {
	int shaded_mode;

	if (sscanf(argv[0], "%d", &shaded_mode) != 1)
	    return BRLCAD_ERROR;

	gedp->i->ged_gdp->gd_shaded_mode = shaded_mode;
	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_SHADED_MODE_COMMANDS(X, XID) \
    X(shaded_mode, ged_shaded_mode_core, GED_CMD_DEFAULT, &shaded_mode_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SHADED_MODE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_shaded_mode", 1, GED_SHADED_MODE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
