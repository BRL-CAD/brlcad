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

#include "bu/arg.h"
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

static const struct bu_arg_option bot_repair_options[] = {
    BU_ARG_FLAG("h", "help", struct bot_repair_args, print_help,
	"Print command help"),
    BU_ARG_NUMBER_RANGE("p", "max-hole-percent", struct bot_repair_args,
	max_hole_area_percent, 1, 0.0, 1, 100.0, "percent",
	"Maximum hole area as a percentage of mesh area"),
    BU_ARG_VALUE_VALIDATE("a", "max-hole-area", struct bot_repair_args,
	max_hole_area, BU_ARG_VALUE_NUMBER, bu_arg_nonnegative_number_validate, "area", "Maximum hole area in mm"),
    BU_ARG_VALUE("o", "output-name", struct bot_repair_args, output_name, BU_ARG_VALUE_VLS, BU_ARG_REQUIRED,
	"name", "Output object name instead of overwriting the input"),
    BU_ARG_OPTION_NULL
};
static const char * const bot_repair_output_option[] = {"output-name", NULL};
static const struct bu_arg_constraint bot_repair_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPERAND_COUNT, BU_ARG_CONDITION_ANY_OPTION_PRESENT,
	bot_repair_output_option, 1, 1,
	"--output-name requires exactly one input BoT"),
    BU_ARG_CONSTRAINT_NULL
};
static const struct bu_arg_operand bot_repair_operands[] = {
    BU_ARG_OPERAND("input_bot", BU_ARG_VALUE_DB_OBJECT, 1, BU_ARG_COUNT_UNLIMITED,
	"Input BoT object", "ged.db_object"),
    BU_ARG_OPERAND_NULL
};
extern "C" const struct bu_arg_schema ged_bot_repair_subcommand_schema = {
    "repair", "Repair manifold BoT geometry", bot_repair_options,
    bot_repair_operands, BU_ARG_PARSE_INTERSPERSED,
	BU_ARG_SCHEMA_META_HELP(NULL, bot_repair_constraints, NULL, NULL, NULL)
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
