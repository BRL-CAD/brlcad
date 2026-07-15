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


static void
erase_legacy_validation_error(struct bu_cmd_validate_result *result, size_t token,
	const char *hint)
{
    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_INVALID;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_NONE;
    result->completion_type = BU_CMD_VALUE_STRING;
    result->hint = hint;
}


static int
erase_legacy_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operands = 0;
    int attr_mode = 0;
    int recursive = 0;
    int match_any = 0;
    int ret = 0;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;

    attr_mode = bu_cmd_schema_option_present(schema, argc, argv, "A");
    recursive = bu_cmd_schema_option_present(schema, argc, argv, "r");
    match_any = bu_cmd_schema_option_present(schema, argc, argv, "o");
    operands = bu_cmd_schema_operand_count(schema, argc, argv);

    if (match_any && !attr_mode) {
	erase_legacy_validation_error(result, cursor_arg < argc ? cursor_arg : argc,
	    "-o requires -A attribute selection");
	return 0;
    }
    if (recursive && (attr_mode || match_any)) {
	erase_legacy_validation_error(result, cursor_arg < argc ? cursor_arg : argc,
	    "-r cannot be combined with -A or -o");
	return 0;
    }
    if (!attr_mode)
	return 0;

    if (cursor_arg >= argc && (operands < 2 || operands % 2)) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INCOMPLETE;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_STRING;
	result->hint = operands % 2 ? "attribute value required" :
	    "at least one attribute name/value pair required";
	return 0;
    }
    if (result->expected & BU_CMD_EXPECT_OPERAND) {
	result->completion_type = BU_CMD_VALUE_STRING;
	result->semantic_provider = NULL;
	result->hint = operands % 2 ? "attribute value" : "attribute name";
    }

    return 0;
}


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
    BU_CMD_OPERAND("objects_or_attributes", BU_CMD_VALUE_DB_PATH, 1,
	BU_CMD_COUNT_UNLIMITED, "Displayed paths, or attribute name/value pairs", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_erase_legacy_schema = {
    "erase", "Erase database paths from the display (legacy form)",
    erase_legacy_options, erase_legacy_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    {erase_legacy_schema_validate}
};


static const struct bu_cmd_option erase_new_options[] = {
    {"V", "view", "view", "name", "Specify the independent view to modify",
	BU_CMD_VALUE_STRING, offsetof(struct ged_erase_new_args, view), NULL, NULL,
	"ged.view", NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
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
    erase_new_options, erase_new_operands, BU_CMD_PARSE_INTERSPERSED, {NULL}
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
