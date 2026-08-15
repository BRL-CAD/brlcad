/*                         R E G I O N . C
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
/** @file libged/region.c
 *
 * The region command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "ged/commands.h"
#include "wdb.h"

#include "../ged_private.h"

static const struct bu_cmd_schema *region_schema_for_command(const char *command);

int
ged_region_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int ident, air;
    db_op_t oper;
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return GED_HELP;
    }


    operand_index = bu_cmd_schema_parse_complete(region_schema_for_command(argv[0]),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return BRLCAD_ERROR;
    }
    /* The schema is evaluated against argv + 1, so its operand index is
     * relative to the argument list without the command name.  Execution
     * retains the traditional argv layout used below. */

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ident = wdbp->wdb_item_default;
    air = wdbp->wdb_air_default;

    if (db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* will attempt to create the region */
	if (wdbp->wdb_item_default) {
	    wdbp->wdb_item_default++;
	    bu_vls_printf(gedp->ged_result_str, "Defaulting item number to %d\n",
			  wdbp->wdb_item_default);
	}
    }

    /* Get operation and solid name for each solid */
    for (i = 2; i < argc; i += 2) {
	if ((dp = db_lookup(gedp->dbip,  argv[i+1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "skipping %s\n", argv[i+1]);
	    continue;
	}

	oper = db_str2op(argv[i]);
	if (oper == DB_OP_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "bad operation: %c (0x%x) skip member: %s\n", argv[i][0], argv[i][0], dp->d_namep);
	    continue;
	}

	/* Adding region to region */
	if (dp->d_flags & RT_DIR_REGION) {
	    bu_vls_printf(gedp->ged_result_str, "Note: %s is a region\n", dp->d_namep);
	}

	if (_ged_combadd(gedp, dp, (char *)argv[1], 1, oper, ident, air) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "error in combadd");
	    return BRLCAD_ERROR;
	}
    }

    if (db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* failed to create region */
	if (wdbp->wdb_item_default > 1)
	    wdbp->wdb_item_default--;
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

static const char * const region_op_keywords[] = {"u", "-", "+", NULL};

static int
region_op_validate(struct bu_vls *msg, const char *arg)
{
    if (db_str2op(arg) != DB_OP_NULL)
	return 0;
    if (msg)
	bu_vls_printf(msg, "invalid Boolean operation: %s\n", arg ? arg : "");
    return -1;
}

static const struct bu_cmd_operand region_schema_operands[] = {
    BU_CMD_OPERAND("output_region", BU_CMD_VALUE_STRING, 1, 1,
	"Region to create or extend", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand region_member_roles[] = {
    BU_CMD_OPERAND_KEYWORDS_VALIDATE("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	region_op_validate, "Boolean operation", NULL, region_op_keywords),
    GED_CMD_OPERAND_DB_OBJECT("member", 1, 1, "Member object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand_group region_schema_groups[] = {
    BU_CMD_OPERAND_GROUP("member", region_member_roles, 1,
	BU_CMD_COUNT_UNLIMITED, "Repeated Boolean operation/object pairs"),
    BU_CMD_OPERAND_GROUP_NULL
};

static const struct bu_cmd_schema region_cmd_schema = {
    "region", "Create or extend a region from boolean-operation/object pairs",
    NULL, region_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	BU_CMD_SCHEMA_GROUPS(NULL, NULL, region_schema_groups)
};
static const struct bu_cmd_schema r_cmd_schema = {
    "r", "Create or extend a region from boolean-operation/object pairs",
    NULL, region_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	BU_CMD_SCHEMA_GROUPS(NULL, NULL, region_schema_groups)
};

static const struct bu_cmd_schema *
region_schema_for_command(const char *command)
{
    return BU_STR_EQUAL(command, "r") ? &r_cmd_schema : &region_cmd_schema;
}

#define GED_REGION_COMMANDS(X, XID) \
    X(r, ged_region_core, GED_CMD_DEFAULT, &r_cmd_schema) \
    X(region, ged_region_core, GED_CMD_DEFAULT, &region_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_REGION_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_region", 1, GED_REGION_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
