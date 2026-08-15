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

static int cannot_bypass_structure(const struct bu_cmd_schema *, size_t,
	const char **, size_t, struct bu_cmd_validate_result *);


static int
candidate_present(const struct bu_cmd_validate_result *result, const char *candidate)
{
    if (!result || !candidate)
	return 0;
    for (size_t i = 0; i < result->completion_count; i++)
	if (result->completion_candidates[i] &&
		BU_STR_EQUAL(result->completion_candidates[i], candidate))
	    return 1;
    return 0;
}


static void
seed_validation_result(struct bu_cmd_validate_result *result)
{
    static const char * const candidates[] = {"stale-candidate", NULL};

    bu_cmd_validate_result_set(result, BU_CMD_VALIDATE_INCOMPLETE, 3,
	BU_CMD_EXPECT_OPTION, BU_CMD_VALUE_STRING, "stale hint",
	"stale.provider");
    bu_cmd_keyword_candidates(result, candidates, NULL);
}


static int
validation_result_empty(const struct bu_cmd_validate_result *result)
{
    return result && result->state == BU_CMD_VALIDATE_UNKNOWN &&
	result->token_start == 0 && result->token_end == 0 &&
	result->expected == BU_CMD_EXPECT_NONE && !result->hint &&
	!result->completion_count && !result->completion_candidates &&
	result->completion_type == BU_CMD_VALUE_UNKNOWN &&
	!result->semantic_provider && !result->candidate_validate;
}


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

static const struct bu_cmd_operand case_default_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_STRING, 1, 1,
	"Value used without --list", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const case_list_options[] = {"s", NULL};
static const struct bu_cmd_schema_case case_rows[] = {
    BU_CMD_SCHEMA_CASE("list", "List without a positional value",
	BU_CMD_CONDITION_ALL_OPTIONS_PRESENT, case_list_options, NULL, NULL),
    BU_CMD_SCHEMA_CASE_DEFAULT("value", "Supply one value",
	case_default_operands, NULL),
    BU_CMD_SCHEMA_CASE_NULL
};
static const struct bu_cmd_schema case_schema =
    BU_CMD_SCHEMA_CASED("case", "Option-selected operand fixture",
	short_only_options, BU_CMD_PARSE_OPTIONS_FIRST, case_rows);


static size_t
attached_only_tokens(size_t UNUSED(available), const char **UNUSED(argv))
{
    return 0;
}


static const struct bu_cmd_arg_shape attached_only_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 0, 1, "[=json]", attached_only_tokens, NULL
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


static int
failing_validate(const struct bu_cmd_schema *UNUSED(schema),
	size_t UNUSED(argc), const char **UNUSED(argv),
	size_t UNUSED(cursor_arg), struct bu_cmd_validate_result *result)
{
    static const char * const candidates[] = {"discard-me", NULL};

    bu_cmd_keyword_candidates(result, candidates, NULL);
    result->hint = "discard this partial result";
    return -1;
}


static int
failing_context_validate(const struct bu_cmd_schema *UNUSED(schema),
	size_t UNUSED(argc), const char **UNUSED(argv),
	size_t UNUSED(cursor_arg), void *UNUSED(context),
	struct bu_cmd_validate_result *result)
{
    return failing_validate(NULL, 0, NULL, 0, result);
}


static const struct bu_cmd_schema test_schema =
    BU_CMD_SCHEMA_BOUND("test", "Native command-schema regression fixture",
	test_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL,
	test_context_validate);

static const struct bu_cmd_schema failing_schema =
    BU_CMD_SCHEMA_EXTERNAL("failing", "Failure transaction fixture", NULL,
	NULL, BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, failing_validate);

static const struct bu_cmd_schema failing_context_schema =
    BU_CMD_SCHEMA("failing-context", "Context failure transaction fixture",
	NULL, NULL, BU_CMD_PARSE_OPTIONS_FIRST,
	BU_CMD_SCHEMA_META(NULL, NULL, NULL, failing_context_validate));

static const struct bu_cmd_option ranged_options[] = {
    BU_CMD_INTEGER_RANGE("l", "level", struct test_args, level, 1, 5,
	"level", "Level from one through five"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema ranged_schema = {
    "ranged", "Declarative numeric range fixture", ranged_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand one_sided_operands[] = {
    BU_CMD_OPERAND_INTEGER_MIN("positive", 1, 1, 1,
	"Positive integer", NULL),
    BU_CMD_OPERAND_NUMBER_MAX("ceiling", 1, 1, 5.0,
	"Number no greater than five", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema one_sided_schema = {
    "one-sided", "One-sided numeric range fixture", NULL,
    one_sided_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_operand shaped_operands[] = {
    BU_CMD_OPERAND_SHAPED("point", BU_CMD_VALUE_VECTOR, 0, 3, NULL,
	"Packed point or three XYZ coordinates", NULL, &bu_cmd_vector3_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema shaped_schema = {
    "shaped", "Multi-token semantic operand fixture", NULL, shaped_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static int
keyword_alias_validate(struct bu_vls *UNUSED(msg), const char *arg)
{
    return (BU_STR_EQUAL(arg, "canonical") || BU_STR_EQUAL(arg, "alias")) ? 0 : -1;
}

static const char * const canonical_keyword[] = {"canonical", NULL};
static const struct bu_cmd_operand keyword_alias_operands[] = {
    BU_CMD_OPERAND_KEYWORDS_VALIDATE("value", BU_CMD_VALUE_KEYWORD, 0, 1,
	keyword_alias_validate, "Canonical keyword or accepted alias", NULL,
	canonical_keyword),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema keyword_alias_schema = {
    "keyword-alias", "Keyword validator fixture", NULL, keyword_alias_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const char * const rich_first_aliases[] = {"later", "repeat", "repeat", NULL};
static const struct bu_cmd_value_keyword invalid_rich_keywords[] = {
    {"first", rich_first_aliases, "First value"},
    {"later", NULL, "Canonical collides with an earlier alias"},
    {NULL, NULL, NULL}
};
static const struct bu_cmd_operand invalid_rich_operands[] = {
    BU_CMD_OPERAND_KEYWORD_VALUES("value", BU_CMD_VALUE_KEYWORD, 0, 1,
	"Invalid rich keywords", NULL, invalid_rich_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema invalid_rich_schema = {
    "invalid-rich", "Rich keyword collision fixture", NULL,
    invalid_rich_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_option invalid_execution_options[] = {
    {"x", "custom", "custom", "value", "Missing custom parser",
	BU_CMD_VALUE_CUSTOM, 0, NULL, NULL, NULL, NULL, 0, 0, NULL,
	BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE},
    {"f", "flag-value", "flag-value", "value", "Flag with an argument",
	BU_CMD_VALUE_FLAG, 0, NULL, NULL, NULL, NULL, 0, 0, NULL,
	BU_CMD_ARG_REQUIRED, NULL, NULL, NULL, BU_CMD_VALUE_RANGE_NONE},
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand invalid_execution_operands[] = {
    BU_CMD_OPERAND("custom", BU_CMD_VALUE_CUSTOM, 0, 1,
	"Option-only custom parser used positionally", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema invalid_execution_schema = {
    "invalid-execution", "Execution metadata lint fixture",
    invalid_execution_options, invalid_execution_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

struct relation_args {
    int dependent;
    int required;
    int conflict;
    int help;
    int version;
};

static const struct bu_cmd_option relation_options[] = {
    BU_CMD_FLAG("d", "dependent", struct relation_args, dependent,
	"Select the dependent mode"),
    BU_CMD_FLAG("r", "required", struct relation_args, required,
	"Satisfy the dependent mode"),
    BU_CMD_FLAG("c", "conflict", struct relation_args, conflict,
	"Select an incompatible mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand relation_operands[] = {
    BU_CMD_OPERAND("word", BU_CMD_VALUE_STRING, 0, 1, "Optional word", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const relation_requires[] = {
    "dependent", "required", NULL
};
static const char * const relation_conflicts[] = {
    "conflict", "required", NULL
};
static const struct bu_cmd_constraint relation_constraints[] = {
    BU_CMD_CONSTRAINT_REQUIRES(relation_requires,
	"--dependent requires --required"),
    BU_CMD_CONSTRAINT_CONFLICTS(relation_conflicts,
	"--conflict cannot be combined with --required"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema relation_schema = {
    "relation", "Declarative option relationship fixture", relation_options,
    relation_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, relation_constraints)
};
static const char * const duplicate_relation_options[] = {
    "dependent", "required", "required", NULL
};
static const struct bu_cmd_constraint duplicate_relation_constraints[] = {
    BU_CMD_CONSTRAINT_REQUIRES(duplicate_relation_options,
	"Malformed repeated relationship target"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema duplicate_relation_schema = {
    "duplicate-relation", "Malformed relationship fixture", relation_options,
    relation_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, duplicate_relation_constraints)
};
static const char * const occurrence_options[] = {"dependent", NULL};
static const struct bu_cmd_constraint occurrence_constraints[] = {
    BU_CMD_CONSTRAINT_OPTION_OCCURRENCES(occurrence_options, 0, 1,
	"--dependent may be specified only once"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema occurrence_schema = {
    "occurrence", "Option occurrence fixture", relation_options,
    relation_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, occurrence_constraints)
};

static const struct bu_cmd_option terminal_options[] = {
    BU_CMD_FLAG("h", "help", struct relation_args, help, "Print help"),
    BU_CMD_FLAG(NULL, "version", struct relation_args, version,
	"Print version information"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand terminal_operands[] = {
    BU_CMD_OPERAND("required", BU_CMD_VALUE_STRING, 1, 1,
	"Required during ordinary execution", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema terminal_schema = BU_CMD_SCHEMA(
    "terminal", "Terminal option fixture", terminal_options, terminal_operands,
    BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_META_HELP_VERSION(NULL, NULL, cannot_bypass_structure,
	test_context_validate, NULL));

static const struct bu_cmd_arg_variant incomplete_arg_variants[] = {
    BU_CMD_ARG_VARIANT("packed", "x/y/z", 1, "Only one bound is described"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_shape incomplete_arg_shape =
    BU_CMD_ARG_SHAPE_FORMS(BU_CMD_ARG_SHAPE_VECTOR3, 1, 3,
	"Malformed lint fixture", NULL, incomplete_arg_variants);
static const struct bu_cmd_operand incomplete_arg_operands[] = {
    BU_CMD_OPERAND_SHAPED("point", BU_CMD_VALUE_VECTOR, 1, 3, NULL,
	"Malformed argument-form fixture", NULL, &incomplete_arg_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema incomplete_arg_schema = {
    "incomplete-arg", "Malformed argument-form fixture", NULL,
    incomplete_arg_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
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
static const struct bu_cmd_schema guarded_schema =
    BU_CMD_SCHEMA_EXTERNAL("guarded", "Structural validation must precede callbacks",
	guarded_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL,
	cannot_bypass_structure);
static const struct bu_cmd_schema invalid_bound_unbound_schema =
    BU_CMD_SCHEMA_BOUND("invalid-unbound", "Unbound option lint fixture",
	guarded_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);

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
static const struct bu_cmd_schema grouped_schema =
    BU_CMD_SCHEMA_BOUND("grouped\"schema", "Repeated\noperand fixture", NULL,
	grouped_head, BU_CMD_PARSE_OPTIONS_FIRST, grouped_operands, NULL, NULL, NULL);

static const char * const combined_constraint_options[] = {"s", NULL};
static const struct bu_cmd_constraint combined_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(combined_constraint_options, 0, 1,
	"The short option may appear once"),
    BU_CMD_CONSTRAINT_NULL
};
/* The complete declaration form must permit every command-level facility. */
static const struct bu_cmd_schema combined_schema =
    BU_CMD_SCHEMA_BOUND("combined", "Full native-schema declaration fixture",
	short_only_options, grouped_head, BU_CMD_PARSE_OPTIONS_FIRST,
	grouped_operands, combined_constraints, cannot_bypass_structure,
	test_context_validate);

static int
packed_quaternion_validate(struct bu_vls *UNUSED(msg), const char *arg)
{
    return arg && BU_STR_EQUAL(arg, "1/2/3/4") ? 0 : -1;
}

static const struct bu_cmd_operand packed_quaternion_operands[] = {
    BU_CMD_OPERAND_VALIDATE("quaternion", BU_CMD_VALUE_VECTOR, 1, 1,
	packed_quaternion_validate, "Packed quaternion", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand split_quaternion_operands[] = {
    BU_CMD_OPERAND("component", BU_CMD_VALUE_NUMBER, 4, 4,
	"Four quaternion components", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema packed_quaternion_schema =
    BU_CMD_SCHEMA_BOUND("orient", "Set a packed quaternion", NULL,
	packed_quaternion_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	NULL, NULL, NULL, NULL);
static const struct bu_cmd_schema split_quaternion_schema =
    BU_CMD_SCHEMA_BOUND("orient", "Set four quaternion components", NULL,
	split_quaternion_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	NULL, NULL, NULL, NULL);
static const struct bu_cmd_form quaternion_form_rows[] = {
    BU_CMD_FORM_SCHEMA("packed", "Use one packed x/y/z/w value",
	&packed_quaternion_schema),
    BU_CMD_FORM_SCHEMA("components", "Use four numeric component words",
	&split_quaternion_schema),
    BU_CMD_FORM_NULL
};

static const struct bu_cmd_form *
quaternion_form_select(const struct bu_cmd_forms *forms, size_t argc,
	const char * const *UNUSED(argv), void *UNUSED(context))
{
    if (!forms || !forms->forms)
	return NULL;
    return argc <= 2 ? &forms->forms[0] : &forms->forms[1];
}

static const struct bu_cmd_forms quaternion_forms =
    BU_CMD_FORMS("orient", "Quaternion input alternatives",
	quaternion_form_rows, quaternion_form_select);
static const struct bu_cmd_forms automatic_quaternion_forms =
    BU_CMD_FORMS("orient", "Automatically selected quaternion alternatives",
	quaternion_form_rows, NULL);

static const char * const extension_keywords[] = {"next", NULL};
static const struct bu_cmd_operand extension_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("next", BU_CMD_VALUE_KEYWORD, 1, 1,
	"Optional longer-form word", NULL, extension_keywords),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema extension_short_schema =
    BU_CMD_SCHEMA_BOUND("extend", "Complete short form", NULL, NULL,
	BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const struct bu_cmd_schema extension_long_schema =
    BU_CMD_SCHEMA_BOUND("extend", "Longer form", NULL, extension_operands,
	BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const struct bu_cmd_form extension_form_rows[] = {
    BU_CMD_FORM_SCHEMA("short", "Short form", &extension_short_schema),
    BU_CMD_FORM_SCHEMA("long", "Long form", &extension_long_schema),
    BU_CMD_FORM_NULL
};
static const struct bu_cmd_forms extension_forms =
    BU_CMD_FORMS("extend", "Appendable form fixture", extension_form_rows, NULL);

struct dispatch_args { int verbose; int called; int argc; const char *name; };
static int
dispatch_execute(void *context, int argc, const char *argv[])
{
    struct dispatch_args *args = (struct dispatch_args *)context;
    if (!args || argc < 1 || !argv)
	return -1;
    args->called++;
    args->argc = argc;
    args->name = argv[0];
    return 27;
}
static const struct bu_cmd_option dispatch_root_options[] = {
    BU_CMD_FLAG("v", "verbose", struct dispatch_args, verbose, "Verbose"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema dispatch_root_schema =
    BU_CMD_SCHEMA_BOUND("dispatch", "Dispatch root", dispatch_root_options, NULL,
	BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const struct bu_cmd_schema dispatch_leaf_schema =
    BU_CMD_SCHEMA_BOUND("run", "Dispatch leaf", NULL, NULL,
	BU_CMD_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const char * const dispatch_aliases[] = {"r", NULL};
static const struct bu_cmd_tree_node dispatch_children[] = {
    BU_CMD_TREE_NODE(&dispatch_leaf_schema, dispatch_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, dispatch_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree dispatch_tree =
    BU_CMD_TREE(&dispatch_root_schema, dispatch_children,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS);
static const struct bu_cmd_tree dispatch_first_tree =
    BU_CMD_TREE(&dispatch_root_schema, dispatch_children,
	BU_CMD_TREE_CHILD_FIRST);

static const struct bu_cmd_schema view_root_schema =
    BU_CMD_SCHEMA_BOUND("view", "View command fixture", NULL, NULL,
	BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL, NULL, NULL, NULL);
static const struct bu_cmd_tree_node view_subcommands[] = {
    BU_CMD_TREE_NODE(&packed_quaternion_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree view_tree =
    BU_CMD_TREE(&view_root_schema, view_subcommands, BU_CMD_TREE_CHILD_FIRST);


int
main(int UNUSED(argc), char **argv)
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
    const char *case_value[] = {"word"};
    const char *case_extra[] = {"-s", "word"};
    const char *unknown[] = {"--unknown"};
    const char *marker_prefix[] = {"--"};
    const char *leading_marker[] = {"--", "-third"};
    const char *late_marker[] = {"first", "--", "-third"};
    const char *group_partial[] = {"target", "u"};
    const char *group_complete[] = {"target", "u", "member"};
    const char *group_invalid[] = {"target", "q", "member"};
    const char *range_low[] = {"--level", "0"};
    const char *range_high[] = {"--level", "6"};
    const char *range_valid[] = {"--level", "5"};
    const char *range_attached[] = {"-l5"};
    const char *range_parse_low[] = {"--level", "0"};
    const char *shape_packed[] = {"1/2/3"};
    const char *shape_partial[] = {"1", "2"};
    const char *shape_split[] = {"1", "2", "3"};
    const char *shape_invalid[] = {"one"};
    const char *keyword_alias[] = {"alias"};
    const char *keyword_invalid[] = {"other"};
    const char *relation_missing[] = {"--dependent"};
    const char *relation_late[] = {"--dependent", "word"};
    const char *relation_complete[] = {"--dependent", "--required", "word"};
    const char *relation_conflict[] = {"--conflict", "--required"};
    const char *occurrence_duplicate[] = {"-dd"};
    const char *terminal_help[] = {"--help"};
    const char *terminal_help_extra[] = {"--help", "ignored", "arguments"};
    const char *terminal_version_extra[] = {"--version", "ignored"};
    const char *one_sided_valid[] = {"1", "4.5"};
    const char *one_sided_low[] = {"0", "4.5"};
    const char *one_sided_high[] = {"1", "5.1"};
    const char *known_layer[] = {"--foreign", "--level", "3", "word"};
    const char *packed_quaternion[] = {"orient", "1/2/3/4"};
    const char *empty_quaternion[] = {"orient"};
    const char *split_quaternion_partial[] = {"orient", "1", "2"};
    const char *split_quaternion[] = {"orient", "1", "2", "3", "4"};

    bu_setprogname(argv[0]);

    {
	int transaction_failure = 0;
#define EXPECT_FAILURE_EMPTY(_call) do { \
	    seed_validation_result(&result); \
	    if ((_call) != -1 || !validation_result_empty(&result)) \
		transaction_failure = 1; \
	} while (0)
	EXPECT_FAILURE_EMPTY(bu_cmd_schema_validate(&failing_schema, 0, NULL, 0,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_schema_validate_ctx(&failing_context_schema,
		0, NULL, 0, &args, &result));
	EXPECT_FAILURE_EMPTY(bu_cmd_schema_validate_syntax(NULL, 0, NULL, 0,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_tree_validate_argv(NULL, 0, NULL, 0,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_forms_validate(NULL, 0, NULL, 0, NULL,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_rgb_optional_validate(1, NULL, 0, &result));
	EXPECT_FAILURE_EMPTY(bu_cmd_color_optional_validate(1, NULL, 0, &result));
	EXPECT_FAILURE_EMPTY(bu_cmd_vector3_optional_validate(1, NULL, 0,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_vector3_required_validate(1, NULL, 0,
		&result));
	EXPECT_FAILURE_EMPTY(bu_cmd_integer_pair_optional_validate(1, NULL, 0,
		&result));
#undef EXPECT_FAILURE_EMPTY
	if (transaction_failure) {
	    bu_log("validation APIs retained partial or stale results after failure\n");
	    bu_cmd_validate_result_clear(&result);
	    bu_vls_free(&msg);
	    return 1;
	}
    }

    if (!BU_STR_EQUAL(bu_cmd_option_canonical(&short_only_options[0]), "s") ||
	bu_cmd_schema_parse(&short_only_schema, &short_only_args, &msg, 1, short_only) != 1 ||
	!short_only_args.selected) {
	bu_log("short-only option did not retain its canonical spelling or parse correctly\n");
	bu_vls_free(&msg);
	return 1;
    }
    if (bu_cmd_schema_validate(&case_schema, 0, NULL, 0, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	result.completion_type != BU_CMD_VALUE_STRING) {
	bu_log("default operand case did not require its positional value\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&case_schema, 1, short_only, 1, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("selected option case did not replace the default operand layout\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&case_schema, 1, case_value, 1, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID ||
	bu_cmd_schema_validate(&case_schema, 2, case_extra, 2, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("operand case validation did not distinguish default and selected forms\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (!bu_cmd_schema_active_operand(&case_schema, 0, NULL, 0) ||
	!BU_STR_EQUAL(bu_cmd_schema_active_operand(&case_schema, 0, NULL, 0)->name,
	    "value") ||
	bu_cmd_schema_active_operand(&case_schema, 1, short_only, 0)) {
	bu_log("active operand lookup did not apply option-selected cases\n");
	bu_vls_free(&msg);
	return 1;
    }
    {
	char *case_json = bu_cmd_schema_describe_json(&case_schema);
	char *case_help = bu_cmd_schema_help(&case_schema, "case");
	if (!case_json || !strstr(case_json, "\"operand_cases\":[{") ||
	    !case_help || !strstr(case_help, "Forms:") ||
	    !strstr(case_help, "when -s") ||
	    bu_cmd_schema_lint(&case_schema, &msg) != 0) {
	    bu_log("operand case JSON, help, or lint omitted declared alternatives\n");
	    if (case_json)
		bu_free(case_json, "case schema JSON");
	    if (case_help)
		bu_free(case_help, "case schema help");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(case_json, "case schema JSON");
	bu_free(case_help, "case schema help");
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

    if (bu_cmd_schema_validate(&test_schema, 1, marker_prefix, 0, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	!(result.expected & BU_CMD_EXPECT_OPTION) ||
	!candidate_present(&result, "--list")) {
	bu_log("editable -- did not offer long option completions\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    if (bu_cmd_schema_validate(&short_only_schema, 1, marker_prefix, 0,
	&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
	result.completion_count != 0) {
	bu_log("-- was not an end marker for a short-option-only schema\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    args.level = 0;
    if (bu_cmd_schema_parse(&ranged_schema, &args, &msg, 1,
	range_attached) != 1 || args.level != 5 ||
	bu_cmd_schema_option_span(&ranged_schema, 1, range_attached) != 1 ||
	!bu_cmd_schema_option_present(&ranged_schema, 1, range_attached, "level") ||
	bu_cmd_schema_operand_count(&ranged_schema, 1, range_attached) != 0 ||
	bu_cmd_schema_validate(&ranged_schema, 1, range_attached, 1,
	    &result) != 0 || result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("attached short option argument did not parse consistently\n");
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

    {
	struct relation_args terminal_args = {0, 0, 0, 0, 0};
	char *terminal_json = NULL;
	if (bu_cmd_schema_validate(&terminal_schema, 1, terminal_help, 1,
		&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
	    result.expected != BU_CMD_EXPECT_NONE || custom_calls != 0 ||
	    bu_cmd_schema_parse_complete(&terminal_schema, &terminal_args, &msg,
		1, terminal_help) != 1 || !terminal_args.help) {
	    bu_log("terminal help did not bypass ordinary operands and validators\n");
	    bu_cmd_validate_result_clear(&result);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_cmd_validate_result_clear(&result);
	if (bu_cmd_schema_validate(&terminal_schema, 3, terminal_help_extra, 3,
		&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
	    bu_cmd_schema_validate(&terminal_schema, 2, terminal_version_extra, 2,
		&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
	    bu_cmd_schema_validate_ctx(&terminal_schema, 1, terminal_help, 1,
		&context_calls, &result) != 0 || context_calls != 0) {
	    bu_log("terminal actions did not bypass trailing positional syntax\n");
	    bu_cmd_validate_result_clear(&result);
	    bu_vls_free(&msg);
	    return 1;
	}
	terminal_json = bu_cmd_schema_describe_json(&terminal_schema);
	if (!terminal_json ||
	    !strstr(terminal_json,
		"\"terminal_options\":[\"help\",\"version\"]") ||
	    bu_cmd_schema_lint(&terminal_schema, &msg) != 0) {
	    bu_log("terminal help metadata was not described or linted correctly\n");
	    if (terminal_json)
		bu_free(terminal_json, "terminal schema JSON");
	    bu_cmd_validate_result_clear(&result);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(terminal_json, "terminal schema JSON");
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

    if (bu_cmd_schema_validate(&shaped_schema, 1, shape_packed, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
	result.completion_type != BU_CMD_VALUE_VECTOR) {
	bu_log("standard vector shape rejected its packed form\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&shaped_schema, 2, shape_partial, 2,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	result.completion_type != BU_CMD_VALUE_VECTOR) {
	bu_log("standard vector shape did not request its remaining component\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&shaped_schema, 3, shape_split, 3,
	&result) != 0 || result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("standard vector shape rejected its split form\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&shaped_schema, 1, shape_invalid, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("standard vector shape accepted an invalid value\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    if (bu_cmd_schema_validate(&keyword_alias_schema, 1, keyword_alias, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("keyword validator rejected a non-canonical accepted alias\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&keyword_alias_schema, 1, keyword_invalid, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("keyword validator accepted an invalid value\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&keyword_alias_schema, 0, NULL, 0,
	&result) != 0 || !candidate_present(&result, "canonical")) {
	bu_log("keyword validator schema omitted its canonical completion\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);

    if (bu_cmd_schema_validate(&relation_schema, 1, relation_missing, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	!(result.expected & BU_CMD_EXPECT_OPTION) ||
	!candidate_present(&result, "--required")) {
	bu_log("missing required option was not reported as completable\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&relation_schema, 2, relation_late, 2,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("missing required option remained completable after options closed\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&relation_schema, 3, relation_complete, 3,
	&result) != 0 || result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("satisfied required-option relationship was rejected\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&relation_schema, 2, relation_conflict, 2,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("conflicting options were accepted\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&occurrence_schema, 1, occurrence_duplicate, 1,
	&result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("option occurrence constraint accepted a repeated short flag\n");
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
    if (bu_cmd_schema_validate(&one_sided_schema, 2, one_sided_valid, 2,
	    &result) != 0 || result.state != BU_CMD_VALIDATE_VALID) {
	bu_log("one-sided numeric ranges rejected valid values\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&one_sided_schema, 2, one_sided_low, 2,
	    &result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("integer minimum accepted a value below its bound\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_schema_validate(&one_sided_schema, 2, one_sided_high, 2,
	    &result) != 0 || result.state != BU_CMD_VALIDATE_INVALID) {
	bu_log("number maximum accepted a value above its bound\n");
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
    {
	char *schema_json = bu_cmd_schema_describe_json(&shaped_schema);
	if (!schema_json || !strstr(schema_json, "\"variants\":[{") ||
	    !strstr(schema_json, "\"name\":\"packed\"") ||
	    !strstr(schema_json, "\"token_count\":3")) {
	    bu_log("argument forms are missing from schema JSON\n");
	    if (schema_json)
		bu_free(schema_json, "shaped schema JSON");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(schema_json, "shaped schema JSON");
    }
    {
	char *usage = bu_cmd_schema_usage(&grouped_schema, "comb edit");
	char *help = bu_cmd_schema_help(&combined_schema, NULL);
	char *grouped_help = bu_cmd_schema_help(&grouped_schema, NULL);
	char *ranged_help = bu_cmd_schema_help(&ranged_schema, NULL);
	char *one_sided_help = bu_cmd_schema_help(&one_sided_schema, NULL);
	char *shaped_usage = bu_cmd_schema_usage(&shaped_schema, NULL);
	if (!usage || !BU_STR_EQUAL(usage,
		"Usage: comb edit target [(operation member) ...]\n") ||
	    !help || !strstr(help,
		"Usage: combined [options] target [(operation member) ...]\n") ||
	    !strstr(help, "Full native-schema declaration fixture") ||
	    !strstr(help, "Options:\n") || !strstr(help, "-s") ||
	    !strstr(help, "Operands:\n") || !strstr(help, "Repeated groups:\n") ||
	    !strstr(help, "Constraints:\n") ||
	    !strstr(help, "The short option may appear once") ||
	    !grouped_help || !strstr(grouped_help, "Accepted values:\n") ||
	    !strstr(grouped_help, "expression.operation:") ||
	    !strstr(grouped_help, "u, -, +") ||
	    !ranged_help || !strstr(ranged_help, "--level: value >= 1 and <= 5") ||
	    !one_sided_help || !strstr(one_sided_help, "positive: value >= 1") ||
	    !strstr(one_sided_help, "ceiling: value <= 5") ||
	    !shaped_usage || !BU_STR_EQUAL(shaped_usage, "Usage: shaped [point]\n")) {
	    bu_log("schema-generated usage or help omitted structural metadata\n");
	    if (usage)
		bu_free(usage, "schema usage");
	    if (help)
		bu_free(help, "schema help");
	    if (grouped_help)
		bu_free(grouped_help, "grouped schema help");
	    if (ranged_help)
		bu_free(ranged_help, "ranged schema help");
	    if (one_sided_help)
		bu_free(one_sided_help, "one-sided schema help");
	    if (shaped_usage)
		bu_free(shaped_usage, "shaped schema usage");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(usage, "schema usage");
	bu_free(help, "schema help");
	bu_free(grouped_help, "grouped schema help");
	bu_free(ranged_help, "ranged schema help");
	bu_free(one_sided_help, "one-sided schema help");
	bu_free(shaped_usage, "shaped schema usage");
    }
    {
	char *shaped_help = bu_cmd_schema_help(&shaped_schema, NULL);
	if (!shaped_help || !strstr(shaped_help, "Argument forms:\n") ||
	    !strstr(shaped_help, "packed") ||
	    !strstr(shaped_help, "components")) {
	    bu_log("argument forms are missing from schema help\n");
	    if (shaped_help)
		bu_free(shaped_help, "shaped schema help");
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(shaped_help, "shaped schema help");
    }
    {
	struct bu_vls appended = BU_VLS_INIT_ZERO;
	bu_vls_strcat(&appended, "Command notes.\n\n");
	if (bu_cmd_schema_help_append(&appended, &relation_schema, NULL) != 0 ||
	    !strstr(bu_vls_cstr(&appended), "Command notes.\n\nUsage: relation") ||
	    !strstr(bu_vls_cstr(&appended),
		"--dependent requires --required")) {
	    bu_log("schema help append omitted existing text or generated metadata\n");
	    bu_vls_free(&appended);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_vls_free(&appended);
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

    if (bu_cmd_schema_lint(&combined_schema, &msg) != 0 ||
	bu_cmd_schema_lint(&relation_schema, &msg) != 0 ||
	bu_cmd_schema_lint(&occurrence_schema, &msg) != 0 ||
	bu_cmd_schema_lint(&guarded_schema, &msg) != 0) {
	bu_log("complete, relational, or external native schema failed lint: %s\n",
	    bu_vls_addr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_lint(&duplicate_relation_schema, &msg) == 0 ||
	!strstr(bu_vls_cstr(&msg), "constraint repeats option \"required\"")) {
	bu_log("duplicate relationship target was not diagnosed: %s\n",
	    bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);

    if (bu_cmd_forms_validate(&quaternion_forms, 3,
	(const char **)split_quaternion_partial, 3, NULL, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	result.completion_type != BU_CMD_VALUE_NUMBER) {
	bu_log("selected split command form did not retain its typed incomplete state\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_forms_validate(&automatic_quaternion_forms, 1,
	    (const char **)empty_quaternion, 1, NULL, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_INCOMPLETE ||
	result.completion_type != BU_CMD_VALUE_UNKNOWN ||
	!result.hint || !BU_STR_EQUAL(result.hint, "command form alternatives")) {
	bu_log("an ambiguous partial form was rejected before it could be completed "
	    "(state=%d, type=%d, hint=%s)\n", (int)result.state,
	    (int)result.completion_type,
	    result.hint ? result.hint : "(null)");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    if (bu_cmd_forms_validate(&quaternion_forms, 5,
	(const char **)split_quaternion, 5, NULL, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID ||
	bu_cmd_forms_parse(&quaternion_forms, NULL, &msg, 5,
	    split_quaternion, NULL, NULL) != 0 ||
	bu_cmd_forms_select(&automatic_quaternion_forms, 2,
	    packed_quaternion, NULL) != &quaternion_form_rows[0]) {
	bu_log("command form validation, parsing, or automatic selection failed\n");
	bu_cmd_validate_result_clear(&result);
	bu_vls_free(&msg);
	return 1;
    }
    bu_cmd_validate_result_clear(&result);
    {
	const char *extend[] = {"extend"};
	if (bu_cmd_forms_validate(&extension_forms, 1, extend, 1, NULL,
		&result) != 0 || result.state != BU_CMD_VALIDATE_VALID ||
		!candidate_present(&result, "next")) {
	    bu_log("valid short form hid a viable longer-form completion\n");
	    bu_cmd_validate_result_clear(&result);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_cmd_validate_result_clear(&result);
    }
    {
	const char *dispatch_argv[] = {"--verbose", "r", "argument"};
	const char *first_argv[] = {"r", "argument"};
	struct dispatch_args dispatch = {0, 0, 0, NULL};
	int command_result = 0;
	if (bu_cmd_tree_dispatch(&dispatch_tree, &dispatch, 3, dispatch_argv,
		&command_result) != 0 || dispatch.called != 1 || dispatch.argc != 2 ||
		!BU_STR_EQUAL(dispatch.name, "run") || command_result != 27) {
	    bu_log("tree dispatch did not honor root options or canonicalize an alias\n");
	    bu_vls_free(&msg);
	    return 1;
	}
	if (bu_cmd_tree_dispatch(&dispatch_first_tree, &dispatch, 2, first_argv,
		&command_result) != 0 || dispatch.called != 2 || dispatch.argc != 2 ||
		!BU_STR_EQUAL(dispatch.name, "run") || command_result != 27) {
	    bu_log("tree dispatch did not honor a first-argument child phase\n");
	    bu_vls_free(&msg);
	    return 1;
	}
    }
    {
	char *form_help = bu_cmd_forms_help(&quaternion_forms, "view orient");
	char *form_json = bu_cmd_forms_describe_json(&quaternion_forms);
	struct bu_vls appended = BU_VLS_INIT_ZERO;
	if (!form_help || !strstr(form_help, "packed form - Use one packed") ||
	    !strstr(form_help, "Usage: view orient quaternion") ||
	    !strstr(form_help, "components form - Use four numeric") ||
	    !strstr(form_help, "Usage: view orient component component component component") ||
	    !form_json || !strstr(form_json, "\"kind\":\"native_forms\"") ||
	    !strstr(form_json, "\"name\":\"components\"") ||
	    bu_cmd_forms_lint(&quaternion_forms, &msg) != 0 ||
	    bu_cmd_forms_help_append(&appended, &quaternion_forms,
		"view orient") != 0 ||
	    !strstr(bu_vls_cstr(&appended), "packed form - Use one packed")) {
	    bu_log("command form help, JSON, or lint omitted declared alternatives: %s\n",
		bu_vls_cstr(&msg));
	    if (form_help)
		bu_free(form_help, "command forms help");
	    if (form_json)
		bu_free(form_json, "command forms JSON");
	    bu_vls_free(&appended);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(form_help, "command forms help");
	bu_free(form_json, "command forms JSON");
	bu_vls_free(&appended);
    }
    {
	const char *path[] = {"orient"};
	char *path_help = bu_cmd_tree_help_path(&view_tree, "view", 1, path);
	struct bu_vls appended = BU_VLS_INIT_ZERO;
	if (!path_help || !strstr(path_help, "Usage: view orient quaternion") ||
	    !strstr(path_help, "Set a packed quaternion") ||
	    bu_cmd_tree_help_append(&appended, &view_tree, "view") != 0 ||
	    !strstr(bu_vls_cstr(&appended), "Subcommands:")) {
	    bu_log("selected tree-path help did not use the leaf schema\n");
	    if (path_help)
		bu_free(path_help, "tree path help");
	    bu_vls_free(&appended);
	    bu_vls_free(&msg);
	    return 1;
	}
	bu_free(path_help, "tree path help");
	bu_vls_free(&appended);
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_lint(&invalid_bound_unbound_schema, &msg) == 0) {
	bu_log("bound schema unexpectedly accepted an unbound option\n");
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_lint(&incomplete_arg_schema, &msg) == 0 ||
	!strstr(bu_vls_cstr(&msg), "do not describe the shape bounds")) {
	bu_log("lint accepted incomplete argument-form metadata: %s\n",
	    bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_lint(&invalid_rich_schema, &msg) == 0) {
	bu_log("lint accepted colliding rich keyword spellings\n");
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_trunc(&msg, 0);
    if (bu_cmd_schema_lint(&invalid_execution_schema, &msg) == 0 ||
	!strstr(bu_vls_cstr(&msg), "has no parser or consumer") ||
	!strstr(bu_vls_cstr(&msg), "incompatible value and argument") ||
	!strstr(bu_vls_cstr(&msg), "option-only custom parser")) {
	bu_log("shared lint accepted unusable execution metadata: %s\n",
	    bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }

    bu_cmd_validate_result_clear(&result);
    bu_vls_free(&msg);
    return 0;
}
