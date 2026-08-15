/*                    E R A S E _ S Y N T A X . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 */
/** @file libged/erase_syntax.c
 *
 * Native syntax shared by the legacy and new-form erase executors and the
 * context-selecting GED grammar adapter.
 */

#include "common.h"

#include "bu/cmdschema.h"

#include "ged_private.h"


static const struct bu_cmd_option erase_legacy_options[] = {
    BU_CMD_FLAG("r", NULL, struct ged_erase_legacy_args, recursive,
	"Erase all displayed paths associated with each object"),
    BU_CMD_FLAG("A", NULL, struct ged_erase_legacy_args, attributes,
	"Select objects by attribute name/value pairs"),
    BU_CMD_FLAG("o", NULL, struct ged_erase_legacy_args, match_any,
	"In attribute mode, match any supplied pair"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand erase_legacy_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_PATH, 1,
	BU_CMD_COUNT_UNLIMITED, "Displayed paths, or attribute name/value pairs", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand erase_attribute_roles[] = {
    BU_CMD_OPERAND("attribute", BU_CMD_VALUE_STRING, 1, 1,
	"Attribute name", NULL),
    BU_CMD_OPERAND("value", BU_CMD_VALUE_STRING, 1, 1,
	"Attribute value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand_group erase_attribute_groups[] = {
    BU_CMD_OPERAND_GROUP("attribute_value", erase_attribute_roles, 1,
	BU_CMD_COUNT_UNLIMITED, "Repeated attribute name/value pairs"),
    BU_CMD_OPERAND_GROUP_NULL
};
static const char * const erase_attribute_option[] = {"A", NULL};
static const char * const erase_match_any_requires[] = {"o", "A", NULL};
static const char * const erase_recursive_conflicts[] = {"r", "A", "o", NULL};
static const struct bu_cmd_schema_case erase_legacy_cases[] = {
    BU_CMD_SCHEMA_CASE("attributes", "Select displayed paths by attributes",
	BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, erase_attribute_option, NULL,
	erase_attribute_groups),
    BU_CMD_SCHEMA_CASE_DEFAULT("objects", "Erase named displayed paths",
	erase_legacy_operands, NULL),
    BU_CMD_SCHEMA_CASE_NULL
};
static const struct bu_cmd_constraint erase_legacy_constraints[] = {
    BU_CMD_CONSTRAINT_REQUIRES(erase_match_any_requires,
	"-o requires -A attribute selection"),
    BU_CMD_CONSTRAINT_CONFLICTS(erase_recursive_conflicts,
	"-r cannot be combined with -A or -o"),
    BU_CMD_CONSTRAINT_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_erase_legacy_schema = {
    "erase", "Erase database paths from the display (legacy form)",
    erase_legacy_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_META_CASES(NULL, erase_legacy_constraints, NULL, NULL,
	erase_legacy_cases)
};


static const struct bu_cmd_option erase_new_options[] = {
    {"V", "view", "view", "name", "Specify the independent view to modify",
	BU_CMD_VALUE_STRING, offsetof(struct ged_erase_new_args, view), NULL, NULL,
	"ged.view", NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL,
	BU_CMD_VALUE_RANGE_NONE},
    BU_CMD_INTEGER("m", "mode", struct ged_erase_new_args, mode, "number",
	"Erase objects drawn in the specified drawing mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand erase_new_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_PATH, 1, BU_CMD_COUNT_UNLIMITED,
	"Displayed database paths to erase", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_erase_new_schema = {
    "erase", "Erase database paths from the display (new form)",
    erase_new_options, erase_new_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
