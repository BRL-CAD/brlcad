/*                         C O P Y . C
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
/** @file libged/copy.c
 *
 * The copy command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/str.h"
#include "bu/units.h"

#include "../ged_private.h"


static const struct bu_cmd_operand copy_schema_operands[] = {
    BU_CMD_OPERAND("source_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Existing database object", "ged.db_object"),
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the copied object", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema copy_cmd_schema = {
    "copy", "Copy a database object", NULL, copy_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema cp_cmd_schema = {
    "cp", "Copy a database object", NULL, copy_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
copy_show_help(struct ged *gedp, const char *command)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s source_object output_object", command);
}


int
ged_copy_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *from_dp;
    struct bu_external external;
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
	copy_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    schema = BU_STR_EQUAL(argv[0], "cp") ? &cp_cmd_schema : &copy_cmd_schema;
    operand_index = bu_cmd_schema_parse_complete(schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 2) {
	copy_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    GED_DB_LOOKUP(gedp, from_dp, argv[0], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_CHECK_EXISTS(gedp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    if (db_get_external(&external, from_dp, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_export_external(wdbp, &external, argv[1],
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      argv[1]);
	return BRLCAD_ERROR;
    }

    bu_free_external(&external);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_COPY_COMMANDS(X, XID) \
    X(copy, ged_copy_core, GED_CMD_DEFAULT, &copy_cmd_schema) \
    X(cp, ged_copy_core, GED_CMD_DEFAULT, &cp_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COPY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_copy", 1, GED_COPY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
