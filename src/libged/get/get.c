/*                         G E T . C
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
/** @file libged/get.c
 *
 * The get command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "ged.h"


static const struct bu_cmd_operand get_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Database object or path to query", "ged.db_path"),
    BU_CMD_OPERAND("attribute", BU_CMD_VALUE_STRING, 0, 1,
	"Optional primitive property name", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema get_cmd_schema = {
    "get", "Query a database object's primitive properties", NULL,
    get_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
get_show_help(struct ged *gedp, const char *command)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s object [attribute]", command);
}


int
ged_get_core(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    struct rt_db_internal intern;
    int operand_index = 0;
    int parse_dummy = 0;
    const char *object = NULL;
    const char *attribute = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	get_show_help(gedp, argv[0]);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(&get_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index < 1 ||
	argc - 1 - operand_index > 2) {
	get_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;
    argc -= operand_index + 1;
    object = argv[0];
    attribute = argc > 1 ? argv[1] : NULL;

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    if (gedp->dbip == 0) {
	bu_vls_printf(gedp->ged_result_str, "dbip does not support lookup operations");
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	if (wdb_import_from_path(gedp->ged_result_str, &intern, object, wdbp) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (!intern.idb_meth->ft_get) {
	return BRLCAD_ERROR;
    }

    status = intern.idb_meth->ft_get(gedp->ged_result_str, &intern, attribute);
    rt_db_free_internal(&intern);

    return status;
}


#include "../include/plugin.h"

#define GED_GET_COMMANDS(X, XID) \
    X(get, ged_get_core, GED_CMD_DEFAULT, &get_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_GET_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_get", 1, GED_GET_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
