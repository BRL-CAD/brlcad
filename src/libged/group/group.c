/*                         G R O U P . C
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
/** @file libged/group.c
 *
 * The group command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/str.h"
#include "wdb.h"

#include "../ged_private.h"


static const struct bu_cmd_operand group_schema_operands[] = {
    BU_CMD_OPERAND("output_group", BU_CMD_VALUE_STRING, 1, 1,
	"Group to create or extend", NULL),
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Database objects to add", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema group_cmd_schema = {
    "group", "Create or extend a union group", NULL, group_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema g_cmd_schema = {
    "g", "Create or extend a union group", NULL, group_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
group_show_help(struct ged *gedp, const char *command)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s group_name object ...", command);
}


int
ged_group_core(struct ged *gedp, int argc, const char *argv[])
{
    int operand_index;
    int parse_dummy = 0;
    const struct bu_cmd_schema *schema;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	group_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    schema = BU_STR_EQUAL(argv[0], "g") ? &g_cmd_schema : &group_cmd_schema;
    operand_index = bu_cmd_schema_parse_complete(schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index < 2) {
	group_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;
    argc -= operand_index + 1;

    return _ged_combadd2(gedp, (char *)argv[0], argc-1, argv+1, 0, WMOP_UNION, 0, 0, NULL, 1);
}

#include "../include/plugin.h"

#define GED_GROUP_COMMANDS(X, XID) \
    X(g, ged_group_core, GED_CMD_DEFAULT, &g_cmd_schema) \
    X(group, ged_group_core, GED_CMD_DEFAULT, &group_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_GROUP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_group", 1, GED_GROUP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
