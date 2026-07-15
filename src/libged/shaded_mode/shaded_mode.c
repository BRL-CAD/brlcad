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

#include "bu/cmdschema.h"

#include "../ged_private.h"


static const char * const shaded_mode_keywords[] = {"0", "1", "2", NULL};
static const struct bu_cmd_operand shaded_mode_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("mode", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Shading mode", NULL, shaded_mode_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema shaded_mode_cmd_schema = {
    "shaded_mode", "Query or set display shading mode", NULL,
    shaded_mode_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

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
    static const char *usage = "[0|1|2]";
    int operand_index = 0;
    int parse_dummy = 0;
    int operand_count = 0;

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

	operand_index = bu_cmd_schema_parse_complete(&shaded_mode_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;

    /* get shaded mode */
	if (operand_count == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d", gedp->i->ged_gdp->gd_shaded_mode);
	return BRLCAD_OK;
    }

    /* set shaded mode */
	if (operand_count == 1) {
	int shaded_mode;

	if (sscanf(argv[1 + operand_index], "%d", &shaded_mode) != 1)
	    goto bad;

	if (shaded_mode < 0 || 2 < shaded_mode)
	    goto bad;

	gedp->i->ged_gdp->gd_shaded_mode = shaded_mode;
	return BRLCAD_OK;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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
