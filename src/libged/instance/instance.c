/*                         I N S T A N C E . C
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
/** @file libged/instance.c
 *
 * The instance command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "wdb.h"

#include "../ged_private.h"


static const char * const instance_subtract_aliases[] = {"\\", NULL};
static const char * const instance_intersect_aliases[] = {"n", "x", NULL};
static const struct bu_cmd_value_keyword instance_operation_values[] = {
    {"u", NULL, "Union (the default)"},
    {"-", instance_subtract_aliases, "Subtraction"},
    {"+", instance_intersect_aliases, "Intersection"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_operand instance_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Existing object to instance", "ged.db_object"),
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_STRING, 1, 1,
	"Combination to create or extend", NULL),
    BU_CMD_OPERAND_KEYWORD_VALUES("operation", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Boolean operation (union, subtraction, or intersection)", NULL,
	instance_operation_values),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema instance_cmd_schema = {
    "instance", "Add an object instance to a combination", NULL,
    instance_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema i_cmd_schema = {
    "i", "Add an object instance to a combination", NULL,
    instance_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_instance_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    db_op_t oper;
    static const char *usage = "obj comb [op]";
    const struct bu_cmd_schema *schema;
    int operand_index;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    schema = BU_STR_EQUAL(argv[0], "i") ? &i_cmd_schema : &instance_cmd_schema;
    operand_index = bu_cmd_schema_parse_complete(schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;


    if ((dp = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY)) == RT_DIR_NULL)
	return BRLCAD_ERROR;

    oper = WMOP_UNION;
    if (argc == 3)
	oper = db_str2op(argv[2]);

    if (oper == DB_OP_NULL) {
	bu_vls_printf(gedp->ged_result_str, "bad operation: %c (0x%x)\n", argv[2][0], argv[2][0]);
	return BRLCAD_ERROR;
    }

    if (_ged_combadd(gedp, dp, (char *)argv[1], 0, oper, 0, 0) == RT_DIR_NULL)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_INSTANCE_COMMANDS(X, XID) \
    X(i, ged_instance_core, GED_CMD_DEFAULT, &i_cmd_schema) \
    X(instance, ged_instance_core, GED_CMD_DEFAULT, &instance_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_INSTANCE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_instance", 1, GED_INSTANCE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
