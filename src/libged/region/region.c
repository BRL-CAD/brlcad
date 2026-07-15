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
    static const char *usage = "reg_name <op obj ...>";
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


    operand_index = bu_cmd_schema_parse_complete(region_schema_for_command(argv[0]),
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

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

static void
region_op_candidates(struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;
    for (size_t i = 0; region_op_keywords[i]; i++)
	if (BU_STR_EMPTY(prefix) || bu_strncmp(region_op_keywords[i], prefix, strlen(prefix)) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1, sizeof(char *), "region operation candidates");
    for (size_t i = 0, oi = 0; region_op_keywords[i]; i++)
	if (BU_STR_EMPTY(prefix) || bu_strncmp(region_op_keywords[i], prefix, strlen(prefix)) == 0)
	    result->completion_candidates[oi++] = bu_strdup(region_op_keywords[i]);
    result->completion_count = count;
}

static int
region_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}


static int
region_schema_validate(const struct bu_cmd_schema *cmd, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    size_t member_tokens;
    int cursor_is_token;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc < 1)
	return ret;

    /* Validate every already-confirmed operation.  The cursor operation is
     * handled below as a potentially incomplete completion seed. */
    for (size_t i = 1; i < argc; i += 2) {
	if (i == cursor_arg && cursor_arg < argc)
	    continue;
	if (db_str2op(argv[i]) == DB_OP_NULL)
	    return region_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_KEYWORD, "invalid boolean operation", NULL);
    }

    member_tokens = argc - 1;
    cursor_is_token = cursor_arg < argc;

    if (cursor_arg >= argc) {
	if (member_tokens < 2) {
	    if (member_tokens == 0) {
		region_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		    BU_CMD_VALUE_KEYWORD, "boolean operation expected", NULL);
		region_op_candidates(result, "");
	    } else {
		region_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		    BU_CMD_VALUE_DB_OBJECT, "member object expected", "ged.db_object");
	    }
	} else if (member_tokens % 2) {
	    region_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		BU_CMD_VALUE_DB_OBJECT, "member object expected", "ged.db_object");
	}
	return 0;
    }

    if (cursor_arg == 0)
	return 0;

    if (((cursor_arg - 1) % 2) == 0) {
	const char *prefix = cursor_is_token ? argv[cursor_arg] : "";
	bu_cmd_validate_state_t state = db_str2op(prefix) != DB_OP_NULL ?
	    BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE;
	region_validation_result(result, state, cursor_arg, BU_CMD_VALUE_KEYWORD,
	    state == BU_CMD_VALIDATE_VALID ? "boolean operation" : "boolean operation expected", NULL);
	region_op_candidates(result, prefix);
	return 0;
    }

    region_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	BU_CMD_VALUE_DB_OBJECT, "member object", "ged.db_object");
    return 0;
}

static const struct bu_cmd_operand region_schema_operands[] = {
    BU_CMD_OPERAND("output_region", BU_CMD_VALUE_STRING, 1, 1,
	"Region to create or extend", NULL),
    BU_CMD_OPERAND("operation_members", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Repeated boolean-operation/object pairs", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema region_cmd_schema = {
    "region", "Create or extend a region from boolean-operation/object pairs",
    NULL, region_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    {region_schema_validate}
};
static const struct bu_cmd_schema r_cmd_schema = {
    "r", "Create or extend a region from boolean-operation/object pairs",
    NULL, region_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    {region_schema_validate}
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
