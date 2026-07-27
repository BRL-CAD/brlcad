/*                     R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "rt/db_internal.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

struct bot_repair_args {
    int print_help;
    fastf_t max_hole_area_percent;
    fastf_t max_hole_area;
    struct bu_vls output_name;
};

static int
bot_repair_percent_validate(struct bu_vls *msg, const char *arg)
{
    fastf_t value;

    if (bu_cmd_number_from_str(&value, arg) && value >= 0.0 && value <= 100.0)
	return 0;
    if (msg)
	bu_vls_printf(msg, "maximum hole percentage must be in [0,100]: %s\n",
		arg ? arg : "");
    return -1;
}

static int
bot_repair_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int help = bu_cmd_schema_option_present(schema, argc, argv, "help");
    int ret;

    flat.validation.custom_validate = NULL;
    if (help)
	flat.operands = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID || help || cursor_arg < argc)
	return ret;

    if (bu_cmd_schema_option_present(schema, argc, argv, "output-name") &&
	bu_cmd_schema_operand_count(schema, argc, argv) != 1) {
	size_t count = bu_cmd_schema_operand_count(schema, argc, argv);
	bu_cmd_validate_result_clear(result);
	result->state = count < 1 ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_DB_OBJECT;
	result->semantic_provider = "ged.db_object";
	result->hint = "--output-name requires exactly one input BoT";
    }
    return 0;
}

static const struct bu_cmd_option bot_repair_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_repair_args, print_help,
	"Print command help"),
    BU_CMD_NUMBER_VALIDATE("p", "max-hole-percent", struct bot_repair_args,
	max_hole_area_percent, bot_repair_percent_validate, "percent",
	"Maximum hole area as a percentage of mesh area"),
    BU_CMD_NONNEGATIVE_NUMBER("a", "max-hole-area", struct bot_repair_args,
	max_hole_area, "area", "Maximum hole area in mm"),
    BU_CMD_VLS_APPEND("o", "output-name", struct bot_repair_args, output_name,
	"name", "Output object name instead of overwriting the input"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_repair_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_bot_repair_subcommand_schema = {
    "repair", "Repair manifold BoT geometry", bot_repair_options,
    bot_repair_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_repair_schema_validate, NULL)
};

extern "C" int
_bot_cmd_repair(void *bs, int argc, const char **argv)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    struct ged *gedp = gb->gedp;
    (void)gedp;

    int ret = ged_repair(gb->gedp, argc, argv);
    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
