/*                         A D J U S T . C
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
/** @file libged/adjust.c
 *
 * The adjust command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "ged.h"
#include "ged/commands.h"

static const struct bu_cmd_operand adjust_schema_operands[] = {
    GED_CMD_OPERAND_DB_OBJECT("object", 1, 1, "Primitive object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adjust_attribute_roles[] = {
    BU_CMD_OPERAND("attribute", BU_CMD_VALUE_STRING, 1, 1,
	"Primitive attribute", NULL),
    BU_CMD_OPERAND("value", BU_CMD_VALUE_STRING, 1, 1,
	"Attribute value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand_group adjust_schema_groups[] = {
    BU_CMD_OPERAND_GROUP("attribute_value", adjust_attribute_roles, 1,
	BU_CMD_COUNT_UNLIMITED, "Repeated attribute/value pairs"),
    BU_CMD_OPERAND_GROUP_NULL
};
static const struct bu_cmd_schema adjust_cmd_schema = {
    "adjust", "Adjust primitive attributes", NULL, adjust_schema_operands,
	BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	BU_CMD_SCHEMA_GROUPS(NULL, NULL, adjust_schema_groups)
};


int
ged_adjust_core(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    struct directory *dp;
    char *name;
    struct rt_db_internal intern;
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


    operand_index = bu_cmd_schema_parse_complete(&adjust_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    name = (char *)argv[0];

    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_QUIET, BRLCAD_ERROR);

    GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    if (!intern.idb_meth->ft_adjust) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) adjust failure", name);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    status = intern.idb_meth->ft_adjust(gedp->ged_result_str, &intern, argc - 1, argv + 1);
    if (status == BRLCAD_OK && wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_ADJUST_COMMANDS(X, XID) \
    X(adjust, ged_adjust_core, GED_CMD_DEFAULT, &adjust_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ADJUST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_adjust", 1, GED_ADJUST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
