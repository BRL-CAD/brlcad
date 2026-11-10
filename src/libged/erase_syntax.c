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

#include "bu/arg.h"

#include "ged_private.h"


static const struct bu_arg_option erase_legacy_options[] = {
    BU_ARG_FLAG("r", NULL, struct ged_erase_legacy_args, recursive,
	"Erase all displayed paths associated with each object"),
    BU_ARG_FLAG("A", NULL, struct ged_erase_legacy_args, attributes,
	"Select objects by attribute name/value pairs"),
    BU_ARG_FLAG("o", NULL, struct ged_erase_legacy_args, match_any,
	"In attribute mode, match any supplied pair"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand erase_legacy_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_PATH, 1,
	BU_ARG_COUNT_UNLIMITED, "Displayed paths, or attribute name/value pairs", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand erase_attribute_roles[] = {
    BU_ARG_OPERAND("attribute", BU_ARG_VALUE_STRING, 1, 1,
	"Attribute name", NULL),
    BU_ARG_OPERAND("value", BU_ARG_VALUE_STRING, 1, 1,
	"Attribute value", NULL),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand_group erase_attribute_groups[] = {
    BU_ARG_OPERAND_GROUP("attribute_value", erase_attribute_roles, 1,
	BU_ARG_COUNT_UNLIMITED, "Repeated attribute name/value pairs"),
    BU_ARG_OPERAND_GROUP_NULL
};
static const char * const erase_attribute_option[] = {"A", NULL};
static const char * const erase_match_any_requires[] = {"o", "A", NULL};
static const char * const erase_recursive_conflicts[] = {"r", "A", "o", NULL};
static const struct bu_arg_schema_case erase_legacy_cases[] = {
    BU_ARG_SCHEMA_CASE("attributes", "Select displayed paths by attributes",
	BU_ARG_CONDITION_ALL_OPTIONS_PRESENT, erase_attribute_option, NULL,
	erase_attribute_groups),
    BU_ARG_SCHEMA_CASE_DEFAULT("objects", "Erase named displayed paths",
	erase_legacy_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_constraint erase_legacy_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES, BU_ARG_CONDITION_ALWAYS, erase_match_any_requires, 0, 0,
	"-o requires -A attribute selection"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_CONFLICTS, BU_ARG_CONDITION_ALWAYS, erase_recursive_conflicts, 0, 0,
	"-r cannot be combined with -A or -o"),
    BU_ARG_CONSTRAINT_NULL
};
GED_EXPORT const struct bu_arg_schema ged_erase_legacy_schema = {
    "erase", "Erase database paths from the display (legacy form)",
    erase_legacy_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_CASES(NULL, erase_legacy_constraints, NULL, NULL,
	erase_legacy_cases)
};


static const struct bu_arg_option erase_new_options[] = {
    {"V", "view", "view", "name", "Specify the independent view to modify",
	BU_ARG_VALUE_STRING, offsetof(struct ged_erase_new_args, view), NULL, NULL,
	"ged.view", NULL, 0, 0, NULL, BU_ARG_REQUIRED, NULL, NULL, NULL,
	BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_VALUE("m", "mode", struct ged_erase_new_args, mode, BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "number",
	"Erase objects drawn in the specified drawing mode"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand erase_new_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_PATH, 1, BU_ARG_COUNT_UNLIMITED,
	"Displayed database paths to erase", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};
GED_EXPORT const struct bu_arg_schema ged_erase_new_schema = {
    "erase", "Erase database paths from the display (new form)",
    erase_new_options, erase_new_operands, BU_ARG_PARSE_INTERSPERSED, BU_ARG_SCHEMA_CONSTRAINTS(NULL, NULL)
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
