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

static const struct bu_cmd_option bot_repair_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_repair_args, print_help,
	"Print command help"),
    BU_CMD_NUMBER_RANGE("p", "max-hole-percent", struct bot_repair_args,
	max_hole_area_percent, 0.0, 100.0, "percent",
	"Maximum hole area as a percentage of mesh area"),
    BU_CMD_NONNEGATIVE_NUMBER("a", "max-hole-area", struct bot_repair_args,
	max_hole_area, "area", "Maximum hole area in mm"),
    BU_CMD_VLS_APPEND("o", "output-name", struct bot_repair_args, output_name,
	"name", "Output object name instead of overwriting the input"),
    BU_CMD_OPTION_NULL
};
static const char * const bot_repair_output_option[] = {"output-name", NULL};
static const struct bu_cmd_constraint bot_repair_constraints[] = {
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT,
	bot_repair_output_option, 1, 1,
	"--output-name requires exactly one input BoT"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_operand bot_repair_operands[] = {
    BU_CMD_OPERAND("input_bot", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Input BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_bot_repair_subcommand_schema = {
    "repair", "Repair manifold BoT geometry", bot_repair_options,
    bot_repair_operands, BU_CMD_PARSE_INTERSPERSED,
	BU_CMD_SCHEMA_META_HELP(NULL, bot_repair_constraints, NULL, NULL, NULL)
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
