/*                         M A T C H . C
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
/** @file libged/match.c
 *
 * The match command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/path.h"

#include "../ged_private.h"


static const struct bu_cmd_arg_shape match_pattern_shape = {
    BU_CMD_ARG_SHAPE_RANGE_PATTERN, 1, 1, "Database name glob pattern", NULL
};
static const struct bu_cmd_operand match_schema_operands[] = {
    BU_CMD_OPERAND_SHAPED("expressions", BU_CMD_VALUE_STRING, 1,
	BU_CMD_COUNT_UNLIMITED, NULL, "Database name glob expressions",
	"ged.db_path_or_pattern", &match_pattern_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema match_cmd_schema = {
    "match", "List database names matching glob expressions", NULL,
    match_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_match_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "expression";
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&match_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    for (int whicharg = 0; whicharg < argc; whicharg++) {
	int num = 0;
	struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	    if (bu_path_match(argv[whicharg], dp->d_namep, 0) != 0)
		continue;
	    if (num == 0)
		bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
	    else {
		bu_vls_strcat(gedp->ged_result_str, " ");
		bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
	    }
	    ++num;
	FOR_ALL_DIRECTORY_END;

	if (num > 0)
	    bu_vls_strcat(gedp->ged_result_str, " ");
    }
    bu_vls_trimspace(gedp->ged_result_str);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_MATCH_COMMANDS(X, XID) \
    X(match, ged_match_core, GED_CMD_DEFAULT, &match_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MATCH_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_match", 1, GED_MATCH_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
