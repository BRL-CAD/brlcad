/*                T E S T _ C M D S C H E M A . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 */

#include "common.h"

#include <stddef.h>

#include "bu.h"
#include "bu/cmdschema.h"


struct test_args {
    int list_mode;
};


static size_t
attached_only_tokens(size_t UNUSED(available), const char **UNUSED(argv))
{
    return 0;
}


static const struct bu_cmd_arg_shape attached_only_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 0, 1, "[=json]", attached_only_tokens
};


static int
list_mode_parse(struct bu_vls *UNUSED(msg), const char *arg, void *storage)
{
    int *mode = (int *)storage;

    if (!arg) {
	if (mode)
	    *mode = 1;
	return 0;
    }
    if (!BU_STR_EQUAL(arg, "json"))
	return -1;
    if (mode)
	*mode = 2;
    return 0;
}


static const struct bu_cmd_option test_options[] = {
    {NULL, "list", "list", "[=json]", "List values, optionally as JSON",
	BU_CMD_VALUE_CUSTOM, offsetof(struct test_args, list_mode), list_mode_parse,
	NULL, NULL, NULL, 0, 0, NULL, BU_CMD_ARG_OPTIONAL,
	&attached_only_shape, NULL, NULL},
    BU_CMD_OPTION_NULL
};


static int
test_context_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int *calls = (int *)context;

    if (calls)
	(*calls)++;
    flat.validation.constraint_data.context_validate = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}


static const struct bu_cmd_schema test_schema = {
    "test", "Native command-schema regression fixture", test_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONTEXT_VALIDATOR(test_context_validate)
};


int
main(int UNUSED(argc), char **UNUSED(argv))
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
    struct test_args args = {0};
    int context_calls = 0;
    const char *bare[] = {"--list"};
    const char *json[] = {"--list=json"};
    const char *invalid[] = {"--list=xml"};

    if (bu_cmd_schema_parse(&test_schema, &args, &msg, 1, bare) != 1 ||
	args.list_mode != 1) {
	bu_log("optional custom option did not apply its absent argument form\n");
	bu_vls_free(&msg);
	return 1;
    }
    args.list_mode = 0;
    if (bu_cmd_schema_parse(&test_schema, &args, &msg, 1, json) != 1 ||
	args.list_mode != 2) {
	bu_log("optional custom option did not apply its attached argument form\n");
	bu_vls_free(&msg);
	return 1;
    }
    if (bu_cmd_schema_parse(&test_schema, &args, &msg, 1, invalid) >= 0) {
	bu_log("optional custom option accepted an invalid attached argument\n");
	bu_vls_free(&msg);
	return 1;
    }

    if (bu_cmd_schema_validate(&test_schema, 0, NULL, 0, &result) != 0 ||
	context_calls != 0) {
	bu_log("context-free validation unexpectedly invoked the context hook\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate_ctx(&test_schema, 0, NULL, 0, &context_calls,
	&result) != 0 || context_calls != 1) {
	bu_log("context-aware validation did not invoke the context hook\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }

    bu_cmd_validate_result_clear(&result);
    bu_vls_free(&msg);
    return 0;
}
