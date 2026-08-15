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

#include "ged_arb.h"
#include "../ged_private.h"


struct arb_repair_args {
    int print_help;
    struct bu_vls out_name;
    fastf_t tolerance;
};


static const struct bu_cmd_option arb_repair_options[] = {
    BU_CMD_FLAG("h", "help", struct arb_repair_args, print_help,
	"Print help and exit"),
    BU_CMD_VLS_APPEND("o", "output-name", struct arb_repair_args, out_name, "name",
	"Write the repaired ARB to a new object"),
    BU_CMD_NUMBER("t", "tol", struct arb_repair_args, tolerance, "distance",
	"Distance tolerance for snapping vertices"),
    BU_CMD_OPTION_NULL
};
static const char * const arb_repair_output_option[] = {"output-name", NULL};
static const struct bu_cmd_constraint arb_repair_constraints[] = {
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT,
	arb_repair_output_option, 1, 1,
	"--output-name permits only one input ARB"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_operand arb_repair_operands[] = {
    BU_CMD_OPERAND("input_arb", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"ARB objects to repair", NULL),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_arb_repair_schema = {
    "repair", "Repair and canonicalize ARB objects", arb_repair_options,
    arb_repair_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_META_HELP(NULL, arb_repair_constraints, NULL, NULL, NULL)
};

extern "C" int
_arb_cmd_repair(void *bs, int argc, const char **argv)
{
    struct _ged_arb_info *gb = (struct _ged_arb_info *)bs;
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
