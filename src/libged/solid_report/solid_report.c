/*                    S O L I D _ R E P O R T . C
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
/** @file libged/solid_report.c
 *
 * Compatibility front-end for the detailed solid listing command.
 *
 */

#include "common.h"

#include "ged.h"

#include "../who/who_solids.h"

extern int ged_solid_report_shared_core(struct ged *gedp, int argc, const char *argv[]);

int
ged_solid_report_core(struct ged *gedp, int argc, const char *argv[])
{
    return ged_solid_report_shared_core(gedp, argc, argv);
}


#include "../include/plugin.h"

static const struct bu_cmd_option solid_report_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct ged_solid_report_args, print_help,
	"Print help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_VLS_APPEND("V", "view", struct ged_solid_report_args, view,
	"name", "Specify the view"),
    BU_CMD_INTEGER("m", "mode", struct ged_solid_report_args, mode,
	"number", "Restrict the drawing mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand solid_report_schema_operands[] = {
    BU_CMD_OPERAND("detail_level", BU_CMD_VALUE_INTEGER, 0, 1,
	"Report detail level", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema solid_report_cmd_schema = {
    "solid_report", "Report displayed solid internals", solid_report_schema_options,
    solid_report_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema x_cmd_schema = {
    "x", "Report displayed solid internals", solid_report_schema_options,
    solid_report_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
const struct bu_cmd_schema ged_who_solids_schema = {
    "solids", "Report displayed solid internals", solid_report_schema_options,
    solid_report_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


const struct bu_cmd_schema *
ged_solid_report_native_schema(void)
{
    return &solid_report_cmd_schema;
}

#define GED_SOLID_REPORT_COMMANDS(X, XID) \
    X(solid_report, ged_solid_report_core, GED_CMD_DEFAULT, &solid_report_cmd_schema) \
    X(x, ged_solid_report_core, GED_CMD_DEFAULT, &x_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_SOLID_REPORT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_solid_report", 1, GED_SOLID_REPORT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
