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
#include <string.h>

#include "bu.h"
#include "bu/cmdschema.h"


struct test_args {
    int list_mode;
    int level;
};

struct short_only_args {
    int selected;
};

static int custom_calls = 0;


/* This must remain a file-scope initializer: MSVC rejects a conditional
 * expression used to choose its canonical spelling (C2099). */
static const struct bu_cmd_option short_only_options[] = {
    BU_CMD_FLAG("s", NULL, struct short_only_args, selected, "Short-only flag"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_schema short_only_schema = {
    "short-only", "Short-only option regression fixture", short_only_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
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
	&attached_only_shape, NULL, NULL, BU_CMD_VALUE_RANGE_NONE},
    BU_CMD_OPTION_NULL
};


static int
test_context_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    int *calls = (int *)context;

    (void)schema;
    (void)argc;
    (void)argv;
    (void)cursor_arg;
    (void)result;
    if (calls)
	(*calls)++;
    return 0;
}


static const struct bu_cmd_schema test_schema = {
    "test", "Native command-schema regression fixture", test_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONTEXT_VALIDATOR(test_context_validate)
};

static const struct bu_cmd_option ranged_options[] = {
    BU_CMD_INTEGER_RANGE("l", "level", struct test_args, level, 1, 5,
	"level", "Level from one through five"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema ranged_schema = {
    "ranged", "Declarative numeric range fixture", ranged_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int
cannot_bypass_structure(const struct bu_cmd_schema *UNUSED(schema),
	size_t UNUSED(argc), const char **UNUSED(argv),
	size_t UNUSED(cursor_arg), struct bu_cmd_validate_result *result)
{
    custom_calls++;
    result->state = BU_CMD_VALIDATE_VALID;
    return 0;
}

static const struct bu_cmd_option guarded_options[] = {
    BU_CMD_FLAG_UNBOUND("v", "verbose", "verbose", "Enable verbose output"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema guarded_schema = {
    "guarded", "Structural validation must precede callbacks", guarded_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(cannot_bypass_structure, NULL)
};

static const struct bu_cmd_operand raw_operands[] = {
    BU_CMD_OPERAND("words", BU_CMD_VALUE_RAW, 1, BU_CMD_COUNT_UNLIMITED,
	"Raw words", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema raw_schema = {
    "raw", "End-marker scanner fixture", NULL, raw_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema options_raw_schema = {
    "options-raw", "Options-first end-marker scanner fixture",
    short_only_options, raw_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const char * const operation_keywords[] = {"u", "-", "+", NULL};
static const struct bu_cmd_operand grouped_head[] = {
    BU_CMD_OPERAND("target", BU_CMD_VALUE_DB_OBJECT, 1, 1, "Target", "test.object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand grouped_roles[] = {
    BU_CMD_OPERAND_KEYWORDS("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Boolean operation", NULL, operation_keywords),
    BU_CMD_OPERAND("member", BU_CMD_VALUE_DB_OBJECT, 1, 1, "Member", "test.object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand_group grouped_operands[] = {
    BU_CMD_OPERAND_GROUP("expression", grouped_roles, 0, BU_CMD_COUNT_UNLIMITED,
	"Repeated operation/member pairs"),
    BU_CMD_OPERAND_GROUP_NULL
};
static const struct bu_cmd_schema grouped_schema = {
    "grouped\"schema", "Repeated\noperand fixture", NULL, grouped_head,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_GROUPS(NULL, NULL, grouped_operands)
};


int
main(int UNUSED(argc), char **UNUSED(argv))
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
    struct test_args args = {0, 0};
    struct short_only_args short_only_args = {0};
    int context_calls = 0;
    const char *bare[] = {"--list"};
    const char *json[] = {"--list=json"};
    const char *invalid[] = {"--list=xml"};
    const char *short_only[] = {"-s"};
    const char *unknown[] = {"--unknown"};
    const char *leading_marker[] = {"--", "-third"};
    const char *late_marker[] = {"first", "--", "-third"};
    const char *group_partial[] = {"target", "u"};
    const char *group_complete[] = {"target", "u", "member"};
    const char *group_invalid[] = {"target", "q", "member"};
    const char *range_low[] = {"--level", "0"};
    const char *range_high[] = {"--level", "6"};
    const char *range_valid[] = {"--level", "5"};
    const char *range_parse_low[] = {"--level", "0"};
    const char *known_layer[] = {"--foreign", "--level", "3", "word"};

    if (!BU_STR_EQUAL(bu_cmd_option_canonical(&short_only_options[0]), "s") ||
	bu_cmd_schema_parse(&short_only_schema, &short_only_args, &msg, 1, short_only) != 1 ||
	!short_only_args.selected) {
	bu_log("short-only option did not retain its canonical spelling or parse correctly\n");
	bu_vls_free(&msg);
	return 1;
    }

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
    if (bu_cmd_schema_validate(&test_schema, 1, bare, 1, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID ||
	(result.expected & BU_CMD_EXPECT_OPTION_ARG)) {
	bu_log("attached-only optional option incorrectly required a separate argument\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

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

    if (bu_cmd_schema_validate(&guarded_schema, 1, unknown, 1, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INVALID || custom_calls != 0) {
	bu_log("custom validation bypassed structural option validation\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&guarded_schema, 0, NULL, 0, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID || custom_calls != 1) {
	bu_log("compositional custom validator was not invoked after structural validation\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    if (bu_cmd_schema_parse(&raw_schema, NULL, &msg, 2, leading_marker) != 1 ||
	bu_cmd_schema_operand_count(&raw_schema, 2, leading_marker) != 1 ||
	bu_cmd_schema_validate(&raw_schema, 2, leading_marker, 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("leading optionless end marker was not classified consistently\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_parse(&raw_schema, NULL, &msg, 3, late_marker) != 0 ||
	bu_cmd_schema_operand_count(&raw_schema, 3, late_marker) != 3 ||
	bu_cmd_schema_validate(&raw_schema, 3, late_marker, 3, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("late optionless end marker disagreed between parsing and validation\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    short_only_args.selected = 0;
    if (bu_cmd_schema_parse_complete(&options_raw_schema, &short_only_args, &msg,
	1, short_only) >= 0 || short_only_args.selected) {
	bu_log("complete parsing mutated option storage before rejecting missing operands\n");
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_parse(&options_raw_schema, &short_only_args, &msg, 3,
	late_marker) != 0 ||
	bu_cmd_schema_operand_count(&options_raw_schema, 3, late_marker) != 3 ||
	bu_cmd_schema_validate(&options_raw_schema, 3, late_marker, 3, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("late options-first end marker disagreed between parsing and validation\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    if (bu_cmd_schema_validate(&ranged_schema, 2, range_low, 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("declarative integer range accepted a value below its minimum\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&ranged_schema, 2, range_high, 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("declarative integer range accepted a value above its maximum\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    args.level = 4;
    if (bu_cmd_schema_parse(&ranged_schema, &args, &msg, 2,
	range_parse_low) != -1 || args.level != 4) {
	bu_log("execution parser accepted or stored an out-of-range integer\n");
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_parse_complete(&ranged_schema, &args, &msg, 2,
	range_valid) != 2 || args.level != 5) {
	bu_log("declarative integer range rejected or failed to store a valid value\n");
	bu_vls_free(&msg);
	return 1;
    }
    args.level = 0;
    if (bu_cmd_schema_parse_known(&ranged_schema, &args, &msg, 4,
	known_layer) != 2 || args.level != 3 ||
	!BU_STR_EQUAL(known_layer[0], "--level") ||
	!BU_STR_EQUAL(known_layer[1], "3") ||
	!BU_STR_EQUAL(known_layer[2], "--foreign") ||
	!BU_STR_EQUAL(known_layer[3], "word")) {
	bu_log("known-option parsing did not preserve layered-parser leftovers\n");
	bu_vls_free(&msg);
	return 1;
    }

    if (bu_cmd_schema_validate(&grouped_schema, 2, group_partial, 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	result.completion_type != BU_CMD_VALUE_DB_OBJECT) {
	bu_log("partial repeated operand group was not incomplete at its member role\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&grouped_schema, 3, group_complete, 3, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("complete repeated operand group was not valid\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&grouped_schema, 3, group_invalid, 3, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("repeated operand group accepted an invalid typed role\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    {
	char *schema_json = bu_cmd_schema_describe_json(&grouped_schema);
	if (!schema_json || !strstr(schema_json, "\"operand_groups\":[{") ||
	    !strstr(schema_json, "grouped\\\"schema") ||
	    !strstr(schema_json, "Repeated\\noperand fixture")) {
	    bu_log("repeated groups or escaped strings are missing from schema JSON\n");
	    if (schema_json)
		bu_free(schema_json, "grouped schema JSON");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(schema_json, "grouped schema JSON");
    }
    {
	char *schema_json = bu_cmd_schema_describe_json(&ranged_schema);
	if (!schema_json ||
	    !strstr(schema_json, "\"range\":{\"kind\":\"integer\"") ||
	    !strstr(schema_json, "\"minimum\":1") ||
	    !strstr(schema_json, "\"maximum\":5")) {
	    bu_log("declarative numeric range is missing from schema JSON\n");
	    if (schema_json)
		bu_free(schema_json, "ranged schema JSON");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(schema_json, "ranged schema JSON");
    }
    if (bu_cmd_schema_lint(&grouped_schema, &msg) != 0) {
	bu_log("valid repeated operand schema failed lint: %s\n", bu_vls_addr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    if (bu_cmd_schema_lint(&ranged_schema, &msg) != 0) {
	bu_log("valid declarative range failed lint: %s\n", bu_vls_addr(&msg));
	bu_vls_free(&msg);
	return 1;
    }

    bu_cmd_validate_result_clear(&result);
    bu_vls_free(&msg);
    return 0;
}
