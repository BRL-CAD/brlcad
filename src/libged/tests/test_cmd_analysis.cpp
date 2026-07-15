/*                    T E S T _ C M D _ A N A L Y S I S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file test_cmd_analysis.cpp
 *
 * Exercise libged command schema metadata and runtime semantic analysis.
 *
 */

#include "common.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <bu.h>
#include <bu/cmdschema.h>
#include <ged.h>
#include <rt/search.h>


#define CHECK(_cond, _msg) do { \
    if (!(_cond)) { \
	bu_log("ERROR: %s\n", (_msg)); \
	goto cleanup; \
    } \
} while (0)


struct native_sequence_args {
    int values[3];
};

static int
native_sequence_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    if (argc < 2 || argc > 3) {
	if (msg)
	    bu_vls_printf(msg, "two or three integer components required\n");
	return -1;
    }
    for (size_t i = 0; i < argc; i++) {
	char *end = NULL;
	long value = std::strtol(argv[i], &end, 0);
	if (!end || *end || value < INT_MIN || value > INT_MAX) {
	    if (msg)
		bu_vls_printf(msg, "invalid integer component: %s\n", argv[i]);
	    return -1;
	}
	if (storage)
	    static_cast<int *>(storage)[i] = static_cast<int>(value);
    }
    return 0;
}

static const struct bu_cmd_arg_shape native_sequence_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 2, 3, "two or three integer components");

static const struct bu_cmd_option native_sequence_options[] = {
    BU_CMD_OPTION_SHAPED("v", "values", "values", struct native_sequence_args, values,
	BU_CMD_VALUE_INTEGER, "integer ...", "Set two or three integer components",
	BU_CMD_ARG_REQUIRED, &native_sequence_shape, native_sequence_consume),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_schema native_sequence_schema = {
    "native_sequence", "Native bounded sequence parser proof", native_sequence_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_rgb_args {
    struct bu_color color;
};

static const struct bu_cmd_option native_rgb_options[] = {
    BU_CMD_RGB("c", "color", struct native_rgb_args, color, "r/g/b", "Set RGB color"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand native_rgb_operands[] = {
    BU_CMD_OPERAND("tail", BU_CMD_VALUE_STRING, 1, 1, "Trailing operand", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_rgb_schema = {
    "native_rgb", "Native RGB option parser proof", native_rgb_options,
    native_rgb_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_color_args {
    struct bu_color color;
};

static const struct bu_cmd_option native_color_options[] = {
    BU_CMD_COLOR_COMPAT("c", "color", struct native_color_args, color,
	"color", "Set compatible command color"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand native_color_operands[] = {
    BU_CMD_OPERAND("tail", BU_CMD_VALUE_STRING, 1, 1, "Trailing operand", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_color_schema = {
    "native_color", "Native compatible color parser proof", native_color_options,
    native_color_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_vector3_args {
    vect_t vector;
};

static const struct bu_cmd_option native_vector3_options[] = {
    BU_CMD_VECTOR3("v", "vector", struct native_vector3_args, vector,
	"x/y/z", "Set finite XYZ vector"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand native_vector3_operands[] = {
    BU_CMD_OPERAND("tail", BU_CMD_VALUE_STRING, 1, 1, "Trailing operand", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_vector3_schema = {
    "native_vector3", "Native finite XYZ vector parser proof", native_vector3_options,
    native_vector3_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_scalar_args {
    long verbosity;
    long value;
    long hex_value;
    char selector;
    struct bu_vls words;
};

static const struct bu_cmd_option native_scalar_options[] = {
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", struct native_scalar_args, verbosity,
	"Increase output detail"),
    BU_CMD_LONG("l", "long", struct native_scalar_args, value, "number",
	"Set a base-zero long integer"),
    BU_CMD_HEX_LONG(NULL, "hex", struct native_scalar_args, hex_value, "number",
	"Set a hexadecimal long integer"),
    BU_CMD_CHAR(NULL, "selector", struct native_scalar_args, selector, "character",
	"Set a selector character"),
    BU_CMD_VLS_APPEND("w", "word", struct native_scalar_args, words, "text",
	"Append text"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_schema native_scalar_schema = {
    "native_scalar", "Native standard scalar parser proof", native_scalar_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_vocabulary_args {
    struct bu_vls language;
    char section;
};

static const struct bu_cmd_option native_vocabulary_options[] = {
    BU_CMD_ISO639_1("L", "language", struct native_vocabulary_args, language, "code",
	"Set the manual language"),
    BU_CMD_MAN_SECTION("S", "section", struct native_vocabulary_args, section, "section",
	"Set the manual section"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_schema native_vocabulary_schema = {
    "native_vocabulary", "Native language and manual-section parser proof",
    native_vocabulary_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_cluster_args {
    int alpha;
    int beta;
};

static const struct bu_cmd_option native_cluster_options[] = {
    BU_CMD_FLAG("a", "alpha", struct native_cluster_args, alpha, "Set alpha mode"),
    BU_CMD_FLAG("b", "beta", struct native_cluster_args, beta, "Set beta mode"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand native_cluster_operands[] = {
    BU_CMD_OPERAND("tail", BU_CMD_VALUE_STRING, 1, 1, "Trailing operand", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_cluster_schema = {
    "native_cluster", "Native compact flag-cluster proof", native_cluster_options,
    native_cluster_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_interspersed_args {
    int alpha;
    int number;
};

static const struct bu_cmd_option native_interspersed_options[] = {
    BU_CMD_FLAG("a", "alpha", struct native_interspersed_args, alpha, "Set alpha mode"),
    BU_CMD_INTEGER("n", "number", struct native_interspersed_args, number, "number", "Set numeric mode"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand native_interspersed_operands[] = {
    BU_CMD_OPERAND("source", BU_CMD_VALUE_STRING, 1, 1, "Source operand", NULL),
    BU_CMD_OPERAND("destination", BU_CMD_VALUE_STRING, 1, 1, "Destination operand", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_interspersed_schema = {
    "native_interspersed", "Native interspersed-option parser proof",
    native_interspersed_options, native_interspersed_operands,
    BU_CMD_PARSE_INTERSPERSED, {NULL}
};


static const struct bu_cmd_operand native_raw_operands[] = {
    BU_CMD_OPERAND("text", BU_CMD_VALUE_RAW, 1, 1, "Literal text", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema native_raw_schema = {
    "native_raw", "Native optionless raw-operand proof", NULL,
    native_raw_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_keyword_args {
    const char *value;
};

static const char *native_keyword_primary_aliases[] = {"first", "one", NULL};
static const char *native_keyword_secondary_aliases[] = {"second", "two", NULL};
static const struct bu_cmd_value_keyword native_keyword_values[] = {
    {"primary", native_keyword_primary_aliases, "Primary selection"},
    {"secondary", native_keyword_secondary_aliases, "Secondary selection"},
    {NULL, NULL, NULL}
};

static const struct bu_cmd_option native_keyword_options[] = {
    BU_CMD_KEYWORD_VALUES("k", "kind", struct native_keyword_args, value, "kind",
	"Set the selection kind", native_keyword_values),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_schema native_keyword_schema = {
    "native_keyword", "Native canonical keyword proof", native_keyword_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


struct native_constraint_args {
    const char *file;
    int indexed;
    int named;
};

static const struct bu_cmd_option native_constraint_options[] = {
    BU_CMD_FILE("f", NULL, struct native_constraint_args, file, "file", "Read a mapping file"),
    BU_CMD_FLAG("i", NULL, struct native_constraint_args, indexed, "Indexed mode"),
    BU_CMD_FLAG("n", NULL, struct native_constraint_args, named, "Named mode"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand native_constraint_operands[] = {
    BU_CMD_OPERAND("source", BU_CMD_VALUE_DB_OBJECT, 0, 1, "Existing source", "ged.db_object"),
    BU_CMD_OPERAND("destination", BU_CMD_VALUE_STRING, 0, 1, "New destination", NULL),
    BU_CMD_OPERAND_NULL
};
static const char *native_constraint_modes[] = {"i", "n", NULL};
static const char *native_constraint_file[] = {"f", NULL};
static const struct bu_cmd_constraint native_constraint_rules[] = {
    BU_CMD_CONSTRAINT_OPTIONS(native_constraint_modes, 0, 1, "-i and -n are mutually exclusive"),
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT, native_constraint_file, 0, 0,
	"-f cannot be combined with source/destination operands"),
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_NO_OPTION_PRESENT, native_constraint_file, 2, 2,
	"source and destination are required without -f"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema native_constraint_schema = {
    "native_constraint", "Native declarative relationship proof", native_constraint_options,
    native_constraint_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, native_constraint_rules)
};


static const struct bu_cmd_schema native_tree_root_schema = {
    "native_tree", "Native command tree proof", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema native_tree_alpha_schema = {
    "alpha", "Run alpha", NULL, NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema native_tree_beta_schema = {
    "beta", "Run beta", NULL, NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema native_tree_group_schema = {
    "group", "Nested command group", NULL, NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_operand native_fixed_tree_root_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 1, 1, "Fixed parent operand", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema native_fixed_tree_root_schema = {
    "native_fixed_tree", "Native fixed-operand tree proof", NULL,
    native_fixed_tree_root_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const char *native_tree_beta_aliases[] = {"b", NULL};

static int
native_tree_alpha(void *context, int argc, const char *argv[])
{
    int *calls = static_cast<int *>(context);

    if (calls)
	*calls += argc;
    return argv && argv[0] && BU_STR_EQUAL(argv[0], "alpha") ? 17 : BRLCAD_ERROR;
}

static int
native_tree_beta(void *context, int argc, const char *argv[])
{
    int *calls = static_cast<int *>(context);

    if (calls)
	*calls += argc;
    return argv && argv[0] && BU_STR_EQUAL(argv[0], "beta") ? 23 : BRLCAD_ERROR;
}

static const struct bu_cmd_tree_node native_tree_group_children[] = {
    BU_CMD_TREE_NODE(&native_tree_beta_schema, native_tree_beta_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, native_tree_beta),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree_node native_tree_children[] = {
    BU_CMD_TREE_NODE(&native_tree_alpha_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, native_tree_alpha),
    BU_CMD_TREE_NODE(&native_tree_beta_schema, native_tree_beta_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, native_tree_beta),
    BU_CMD_TREE_NODE(&native_tree_group_schema, NULL, native_tree_group_children,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree native_tree = {
    &native_tree_root_schema, native_tree_children, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};
static const struct bu_cmd_tree native_fixed_tree = {
    &native_fixed_tree_root_schema, native_tree_children,
    BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS
};


static const struct ged_cmd_analysis_token *
operand_token(const struct ged_cmd_analysis *analysis)
{
    if (!analysis || !analysis->tokens)
	return NULL;

    for (size_t i = 0; i < analysis->token_count; i++) {
	if (analysis->tokens[i].role == GED_CMD_TOKEN_OPERAND)
	    return &analysis->tokens[i];
    }

    return NULL;
}


static const struct ged_cmd_analysis_token *
token_matching(const struct ged_cmd_analysis *analysis, const char *input, const char *text)
{
    size_t text_len = std::strlen(text);

    if (!analysis || !analysis->tokens || !input || !text)
	return NULL;

    for (size_t i = 0; i < analysis->token_count; i++) {
	const struct ged_cmd_analysis_token *token = &analysis->tokens[i];
	if (token->char_end < token->char_start)
	    continue;
	if (token->char_end - token->char_start != text_len)
	    continue;
	if (std::strncmp(input + token->char_start, text, text_len) == 0)
	    return token;
    }

    return NULL;
}


static int
analyze_operand(struct ged *gedp, struct ged_cmd_analysis *analysis, const char *input, ged_cmd_semantic_state_t expected_state)
{
    const struct ged_cmd_analysis_token *token = NULL;

    if (ged_cmd_analyze(gedp, input, analysis) != 0) {
	bu_log("ERROR: ged_cmd_analyze failed for [%s]\n", input);
	return 0;
    }

    token = operand_token(analysis);
    if (!token) {
	bu_log("ERROR: no operand token for [%s]\n", input);
	return 0;
    }

    if (token->role != GED_CMD_TOKEN_OPERAND) {
	bu_log("ERROR: operand role mismatch for [%s]\n", input);
	return 0;
    }

    if (token->value_type != BU_CMD_VALUE_DB_PATH) {
	bu_log("ERROR: operand value type mismatch for [%s]\n", input);
	return 0;
    }

    if (!token->validator || std::strcmp(token->validator, "ged.db_path") != 0) {
	bu_log("ERROR: operand validator mismatch for [%s]\n", input);
	return 0;
    }

    if (token->semantic_state != expected_state) {
	bu_log("ERROR: operand semantic state mismatch for [%s]: got %d, expected %d\n",
		input, (int)token->semantic_state, (int)expected_state);
	return 0;
    }

    if (token->char_start != std::strlen("draw ") || token->char_end != std::strlen(input)) {
	bu_log("ERROR: operand span mismatch for [%s]: got [%zu,%zu]\n",
		input, token->char_start, token->char_end);
	return 0;
    }

    return 1;
}


static const char *
find_completion(const char **completions, int count, const char *candidate)
{
    for (int i = 0; i < count; i++) {
	if (BU_STR_EQUAL(completions[i], candidate))
	    return completions[i];
    }

    return NULL;
}


static int
check_completion_order(struct ged *gedp, const char *input, const char **expected, size_t expected_cnt)
{
    struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
    int count = ged_cmd_complete_result(gedp, input, std::strlen(input), &result);

    if (count < (int)expected_cnt) {
	bu_log("ERROR: completion order test for [%s] found %d candidates, expected at least %zu\n",
		input, count, expected_cnt);
	ged_cmd_completion_result_clear(&result);
	return 0;
    }

    for (size_t i = 0; i < expected_cnt; i++) {
	if (!result.completion_candidates[i] || !BU_STR_EQUAL(result.completion_candidates[i], expected[i])) {
	    bu_log("ERROR: completion order mismatch for [%s] candidate %zu: got [%s], expected [%s]\n",
		    input, i, result.completion_candidates[i] ? result.completion_candidates[i] : "(null)", expected[i]);
	    ged_cmd_completion_result_clear(&result);
	    return 0;
	}
    }

    ged_cmd_completion_result_clear(&result);
    return 1;
}


static int
check_schema_exists(const char *cmd)
{
    if (!ged_cmd_schema_exists(cmd)) {
	bu_log("ERROR: %s schema is unavailable\n", cmd);
	return 0;
    }

    return 1;
}


static const struct ged_cmd_analysis_token *
final_analysis_token(const struct ged_cmd_analysis *analysis, size_t line_length)
{
    if (!analysis || !analysis->tokens)
	return NULL;
    for (size_t i = 0; i < analysis->token_count; i++) {
	if (analysis->tokens[i].char_end == line_length)
	    return &analysis->tokens[i];
    }
    return NULL;
}


static int
audit_schema_completion_roundtrips(struct ged *gedp)
{
    const char * const *commands = NULL;
    size_t command_count = ged_cmd_list(&commands);
    std::vector<std::string> pending;
    std::set<std::string> visited;
    size_t option_checks = 0;
    size_t argument_checks = 0;

    for (size_t i = 0; i < command_count; i++) {
	if (commands[i] && ged_cmd_schema_exists(commands[i]))
	    pending.push_back(commands[i]);
    }

    while (!pending.empty()) {
	std::string path = pending.back();
	pending.pop_back();
	if (!visited.insert(path).second)
	    continue;

	/* Discover nested schemas from the candidates offered at a clean token
	 * boundary.  Geometry and operand candidates are harmlessly ignored. */
	std::string phase_input = path + " ";
	struct ged_cmd_completion_result phase = GED_CMD_COMPLETION_RESULT_NULL;
	(void)ged_cmd_complete_result(gedp, phase_input.c_str(), phase_input.size(), &phase);
	for (size_t i = 0; i < phase.completion_count; i++) {
	    if (!phase.completion_candidates[i] || phase.completion_candidates[i][0] == '-')
		continue;
	    std::string rebuilt = phase_input;
	    if (phase.replacement_start > phase.replacement_end || phase.replacement_end > rebuilt.size())
		continue;
	    rebuilt.replace(phase.replacement_start, phase.replacement_end - phase.replacement_start,
		    phase.completion_candidates[i]);
	    struct ged_cmd_analysis analysis = {0, NULL};
	    if (ged_cmd_analyze(gedp, rebuilt.c_str(), &analysis) == 0) {
		const struct ged_cmd_analysis_token *token = final_analysis_token(&analysis, rebuilt.size());
		if (token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
			token->semantic_state == GED_CMD_SEMANTIC_VALID)
		    pending.push_back(rebuilt);
	    }
	    ged_cmd_analysis_clear(&analysis);
	}
	ged_cmd_completion_result_clear(&phase);

	std::string option_input = path + " -";
	struct ged_cmd_completion_result options = GED_CMD_COMPLETION_RESULT_NULL;
	(void)ged_cmd_complete_result(gedp, option_input.c_str(), option_input.size(), &options);
	for (size_t i = 0; i < options.completion_count; i++) {
	    const char *candidate = options.completion_candidates[i];
	    if (!candidate || candidate[0] != '-')
		continue;
	    if (options.replacement_start > options.replacement_end || options.replacement_end > option_input.size()) {
		bu_log("ERROR: schema audit: invalid replacement range for %s candidate %s\n", path.c_str(), candidate);
		ged_cmd_completion_result_clear(&options);
		return 0;
	    }
	    std::string rebuilt = option_input;
	    rebuilt.replace(options.replacement_start, options.replacement_end - options.replacement_start, candidate);
	    struct ged_cmd_analysis analysis = {0, NULL};
	    if (ged_cmd_analyze(gedp, rebuilt.c_str(), &analysis) != 0) {
		bu_log("ERROR: schema audit: unable to analyze completed option [%s]\n", rebuilt.c_str());
		ged_cmd_completion_result_clear(&options);
		return 0;
	    }
	    const struct ged_cmd_analysis_token *token = final_analysis_token(&analysis, rebuilt.size());
	    if (token && token->role == GED_CMD_TOKEN_OPERAND &&
		    token->semantic_state != GED_CMD_SEMANTIC_INVALID) {
		ged_cmd_analysis_clear(&analysis);
		continue;
	    }
	    if (!token || (token->role != GED_CMD_TOKEN_OPTION && token->role != GED_CMD_TOKEN_OPTION_ARG) ||
		    token->semantic_state == GED_CMD_SEMANTIC_INVALID) {
		bu_log("ERROR: schema audit: offered option [%s] does not reanalyze as an option (role %d, state %d)\n",
			rebuilt.c_str(), token ? (int)token->role : -1,
			token ? (int)token->semantic_state : -1);
		ged_cmd_analysis_clear(&analysis);
		ged_cmd_completion_result_clear(&options);
		return 0;
	    }
	    ged_cmd_analysis_clear(&analysis);
	    option_checks++;

	    struct ged_cmd_validate_result validation = GED_CMD_VALIDATE_RESULT_NULL;
	    (void)ged_cmd_validate(gedp, rebuilt.c_str(), rebuilt.size(), &validation);
	    bool needs_argument = validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
		(validation.expected & BU_CMD_EXPECT_OPTION_ARG);
	    ged_cmd_validate_result_clear(&validation);
	    if (!needs_argument || candidate[1] != '-')
		continue;

	    std::string argument_input = rebuilt + " ";
	    struct ged_cmd_completion_result arguments = GED_CMD_COMPLETION_RESULT_NULL;
	    (void)ged_cmd_complete_result(gedp, argument_input.c_str(), argument_input.size(), &arguments);
	    for (size_t ai = 0; ai < arguments.completion_count; ai++) {
		const char *argument = arguments.completion_candidates[ai];
		if (!argument)
		    continue;
		std::string separate = argument_input;
		if (arguments.replacement_start > arguments.replacement_end || arguments.replacement_end > separate.size())
		    continue;
		separate.replace(arguments.replacement_start, arguments.replacement_end - arguments.replacement_start, argument);
		struct ged_cmd_analysis separate_analysis = {0, NULL};
		(void)ged_cmd_analyze(gedp, separate.c_str(), &separate_analysis);
		const struct ged_cmd_analysis_token *separate_token = final_analysis_token(&separate_analysis, separate.size());
		if (!separate_token || separate_token->role != GED_CMD_TOKEN_OPTION_ARG ||
			separate_token->semantic_state == GED_CMD_SEMANTIC_INVALID) {
		    bu_log("ERROR: schema audit: option argument candidate [%s] is not valid (role %d, state %d)\n",
			    separate.c_str(), separate_token ? (int)separate_token->role : -1,
			    separate_token ? (int)separate_token->semantic_state : -1);
		    ged_cmd_analysis_clear(&separate_analysis);
		    ged_cmd_completion_result_clear(&arguments);
		    ged_cmd_completion_result_clear(&options);
		    return 0;
		}
		ged_cmd_analysis_clear(&separate_analysis);

		std::string equals = rebuilt + "=" + argument;
		struct ged_cmd_analysis equals_analysis = {0, NULL};
		(void)ged_cmd_analyze(gedp, equals.c_str(), &equals_analysis);
		const struct ged_cmd_analysis_token *equals_token = final_analysis_token(&equals_analysis, equals.size());
		if (!equals_token || equals_token->role != GED_CMD_TOKEN_OPTION_ARG ||
			equals_token->semantic_state == GED_CMD_SEMANTIC_INVALID) {
		    bu_log("ERROR: schema audit: equals-form option argument [%s] is not valid\n", equals.c_str());
		    ged_cmd_analysis_clear(&equals_analysis);
		    ged_cmd_completion_result_clear(&arguments);
		    ged_cmd_completion_result_clear(&options);
		    return 0;
		}
		ged_cmd_analysis_clear(&equals_analysis);
		argument_checks++;
	    }
	    ged_cmd_completion_result_clear(&arguments);
	}
	ged_cmd_completion_result_clear(&options);
    }

    bu_log("schema completion audit: %zu command paths, %zu options, %zu static option arguments\n",
	    visited.size(), option_checks, argument_checks);
    return option_checks > 0;
}


int
main(int ac, char *av[])
{
    int ret = 1;
    struct ged *gedp = NULL;
    struct ged_cmd_analysis analysis = {0, NULL};
    struct ged_cmd_completion_result completion_result = GED_CMD_COMPLETION_RESULT_NULL;
    struct ged_cmd_validate_result validation = GED_CMD_VALIDATE_RESULT_NULL;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    struct bu_vls lint_msgs = BU_VLS_INIT_ZERO;
    const char **completions = NULL;
    const char *completion = NULL;
    const struct ged_cmd_analysis_token *token = NULL;
    char *schema_json = NULL;
    char *tree_help = NULL;
    int completion_count = 0;
    int argv_completion_count = 0;
    std::string completed_input;

    bu_setprogname(av[0]);

    if (ac != 2 && ac != 3) {
	bu_log("Usage: %s moss.g [m35.g]\n", av[0]);
	return 1;
    }

    if (!bu_file_exists(av[1], NULL)) {
	bu_log("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    {
	const struct bu_cmd_tree_node *node = NULL;
	const char *tree_argv[] = {"alpha", NULL};
	const char *tree_alias_argv[] = {"b", NULL};
	const char *nested_tree_alias_argv[] = {"group", "b", NULL};
	const char *fixed_tree_prefix_argv[] = {"object", NULL};
	const char *fixed_tree_child_argv[] = {"object", "b", NULL};
	int tree_calls = 0;
	int tree_result = BRLCAD_ERROR;
	struct bu_cmd_validate_result tree_validation = BU_CMD_VALIDATE_RESULT_NULL;

	CHECK(bu_cmd_tree_lint(&native_tree, &lint_msgs) == 0,
	    "native command tree should pass structural lint");
	CHECK(bu_cmd_tree_lint(&native_fixed_tree, &lint_msgs) == 0,
	    "fixed-operand native command tree should pass structural lint");
	node = bu_cmd_tree_find_subcommand(&native_tree, "b");
	CHECK(node && node->schema == &native_tree_beta_schema,
	    "native command tree should resolve child aliases");
	CHECK(bu_cmd_tree_dispatch(&native_tree, &tree_calls, 1, tree_argv,
	    &tree_result) == 0 && tree_result == 17 && tree_calls == 1,
	    "native command tree should dispatch its schema-owned executor");
	CHECK(bu_cmd_tree_validate_argv(&native_tree, 0, NULL, 0, &tree_validation) == 0 &&
	    tree_validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (tree_validation.expected & BU_CMD_EXPECT_SUBCOMMAND) &&
	    find_completion(tree_validation.completion_candidates,
	    (int)tree_validation.completion_count, "alpha") != NULL,
	    "native command tree should offer canonical children from its argv validator");
	bu_cmd_validate_result_clear(&tree_validation);
	CHECK(bu_cmd_tree_validate_argv(&native_tree, 1, tree_alias_argv, 0, &tree_validation) == 0 &&
	    tree_validation.state == BU_CMD_VALIDATE_VALID &&
	    (tree_validation.expected & BU_CMD_EXPECT_SUBCOMMAND) &&
	    find_completion(tree_validation.completion_candidates,
	    (int)tree_validation.completion_count, "beta") != NULL,
	    "native command tree argv validation should canonicalize child aliases");
	bu_cmd_validate_result_clear(&tree_validation);
	{
	    CHECK(bu_cmd_tree_dispatch(&native_tree, &tree_calls, 2, nested_tree_alias_argv,
		&tree_result) == 0 && tree_result == 23 && tree_calls == 2,
		"native command tree should descend through a grammar-only node to a leaf executor");
	}
	CHECK(bu_cmd_tree_validate_argv(&native_tree, 2, nested_tree_alias_argv, 1,
	    &tree_validation) == 0 && tree_validation.state == BU_CMD_VALIDATE_VALID &&
	    tree_validation.token_start == 1 &&
	    find_completion(tree_validation.completion_candidates,
	    (int)tree_validation.completion_count, "beta") != NULL,
	    "native command tree argv validation should descend through nested children");
	bu_cmd_validate_result_clear(&tree_validation);
	CHECK(bu_cmd_tree_validate_argv(&native_fixed_tree, 1, fixed_tree_prefix_argv, 1,
	    &tree_validation) == 0 && tree_validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (tree_validation.expected & BU_CMD_EXPECT_SUBCOMMAND) &&
	    find_completion(tree_validation.completion_candidates,
	    (int)tree_validation.completion_count, "alpha") != NULL,
	    "fixed-operand native tree should request a child after its parent operand");
	bu_cmd_validate_result_clear(&tree_validation);
	CHECK(bu_cmd_tree_validate_argv(&native_fixed_tree, 2, fixed_tree_child_argv, 1,
	    &tree_validation) == 0 && tree_validation.state == BU_CMD_VALIDATE_VALID &&
	    find_completion(tree_validation.completion_candidates,
	    (int)tree_validation.completion_count, "beta") != NULL,
	    "fixed-operand native tree should complete a child after its parent operand");
	bu_cmd_validate_result_clear(&tree_validation);
	tree_help = bu_cmd_tree_describe(&native_tree);
	CHECK(tree_help && std::strstr(tree_help, "alpha") &&
	    std::strstr(tree_help, "Run beta"),
	    "native command tree help should come from child schemas");
	bu_free(tree_help, "native tree help");
	tree_help = NULL;
	schema_json = bu_cmd_tree_describe_json(&native_tree);
	CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") &&
	    std::strstr(schema_json, "\"aliases\":[\"b\"]"),
	    "native command tree JSON should publish structure and aliases");
	bu_free(schema_json, "native tree JSON");
	schema_json = NULL;
    }

    gedp = ged_open("db", av[1], 1);
    CHECK(gedp != NULL, "ged_open failed");
    CHECK(ged_cmd_schema_exists("draw"), "draw schema is unavailable");
    {
	const char *wave1_schema_cmds[] = {
	    "open", "opendb", "reopen",
	    "draw", "e", "E", "ev", "redraw", "loadview", "preview",
	    "search",
	    "Z", "zap",
	    "close", "closedb",
	    "clear",
	    "exit", "q", "quit",
	    "?", "apropos", "help", "info",
	    "man",
	    "echo",
	    "log",
	    "dbversion", "version",
	    "dbip", "get_dbip",
	    "env",
	    "units",
	    "title",
	    "delay",
	    "sync",
	    "set_output_script",
	    "set_uplotOutputMode",
	    NULL
	};

	for (size_t i = 0; wave1_schema_cmds[i]; i++) {
	    CHECK(check_schema_exists(wave1_schema_cmds[i]), "Wave 1 schema should be published");
	}
    }
    {
	const char *wave2_schema_cmds[] = {
	    "ls", "t",
	    "list", "l",
	    "cat",
	    "get",
	    "get_type",
	    "get_comb",
	    "get_autoview",
	    "get_eyemodel",
	    "exists",
	    "dbfind", "find",
	    "copy", "cp",
	    "move", "mv",
	    "prefix",
	    "kill", "killall", "killrefs",
	    "db_glob", "glob", "match", "expand", "tops",
	    "listeval", "paths", "pathsum",
	    "which_shader", "whatid", "how", "tree",
	    "who",
	    "hide", "unhide", "remove", "keep",
	    "rm", "killtree",
	    "d", "erase",
	    "comb_color", "copyeval", "copymat",
	    "showmats",
	    "g", "group", "i", "instance",
	    "which", "whichair", "whichid",
	    "pathlist",
	    "stat",
	    "attr",
	    "summary",
	    "move_all", "mvall",
	    "putmat",
	    "r", "region",
	    "comb",
	    "c", "comb_std",
	    "combmem",
	    NULL
	};

	for (size_t i = 0; wave2_schema_cmds[i]; i++) {
	    CHECK(check_schema_exists(wave2_schema_cmds[i]), "Wave 2 schema should be published");
	}
    }
    {
	const char *wave3_schema_cmds[] = {
	    "eye_pos", "grid2model_lu", "grid2view_lu", "m2v_point",
	    "model2grid_lu", "model2view_lu", "rot_point", "setview",
	    "v2m_point", "view2grid_lu", "view2model_lu",
	    "view2model_vec", "vrot",
	    "zoom", "perspective", "isize", "arot", "scale", "sca",
	    "otranslate", "set_transparency", "pset",
	    "ae2dir", "dir2ae",
	    "shaded_mode",
	    "dump", "rmater", "savekey", "wmater", "wcodes",
	    "eac", "form", "label", "shells", "xpush",
	    "cc", "cpi", "decompose", "fracture", "item", "regdef",
	    "sphgroup", "track",
	    "pull", "push", "lt",
	    "adjust", "ocenter", "oscale", "orotate", "keypoint",
	    "model2view", "slew", "sv", "vslew", "orient", "orientation",
	    "3ptarb", "rfarb",
	    "gdiff", "concat", "dbconcat", "garbage_collect",
	    "fbclear", "rmat", "rrt",
	    "dup", "heal", "rmap", "prcolor", "view2model",
	    "pmodel2view", "pmat",
	    "solids_on_ray", "solid_report", "x",
	    "idents", "regions", "solids", "tables",
	    "lod", "make_name",
	    "png", "pngwf", "postscript", "ps", "fb2pix", "png2fb",
	    "color", "edcolor",
	    "rcodes", "edcomb", "importFg4Section", "arced",
	    "illum", "labelvert",
	    "editit", "edmater", "red", "edcodes",
	    "bb", "voxelize",
	    "tra", "rot", "rot_about", "bo", "bev",
	    "shader", "dsp", "tol", "coil",
	    "mirror", "lc",
	    "npush", "overlay",
	    "plot",
	    "grid", "rect",
	    "put", "put_comb", "inside", "B", "blast",
	    "pix2fb",
	    NULL
	};

	for (size_t i = 0; wave3_schema_cmds[i]; i++)
	    CHECK(check_schema_exists(wave3_schema_cmds[i]), "Wave 3 schema should be published");
    }
    {
	const char *wave4_schema_cmds[] = {
	    "arb", "rotate_arb_face", "bot", "bot_condense", "bot_decimate", "bot_dump", "bot_exterior",
	    "bot_face_fuse", "bot_face_sort", "bot_flip", "bot_fuse", "bot_merge", "bot_smooth", "bot_split", "bot_sync",
	    "bot_vertex_fuse", "dbot_dump", "find_bot_edge", "find_bot_pnt", "get_bot_edges",
	    "brep", "dplot", "check", "constraint", "dm", "ert", "graph", "lint", "make_pnts", "material",
	    "labelface", "nmg", "nmg_cmface", "nmg_collapse", "nmg_fix_normals", "nmg_kill_f", "nmg_kill_v",
	    "nmg_make_v", "nmg_mm", "nmg_move_v", "nmg_simplify", "pnts", "process", "qray",
	    "find_pipe_pnt", "pipe_move_pnt", "pipe_append_pnt", "pipe_prepend_pnt", "pipe_delete_pnt",
	    "mouse_move_pipe_pnt", "mouse_append_pipe_pnt", "mouse_prepend_pipe_pnt", "rselect", "screen_grab", "screengrab", "select", "vdraw",
	    "ae", "aet", "autoview", "center", "data_lines", "eye", "eye_pt", "lookat", "print", "quat", "qvrot",
	    "saveview", "sdata_lines", "size", "view", "view2", "view_func", "viewdir", "ypr", NULL
	};
	for (size_t i = 0; wave4_schema_cmds[i]; i++)
	    CHECK(check_schema_exists(wave4_schema_cmds[i]), "Wave 4 schema should be published");
    }
    {
	const char *wave5_schema_cmds[] = {
	    "clone", "facetize", "facetize_old", "gqa", "mater", "nirt", "query_ray", "vnirt", "vquery_ray",
	    "art", "rt", "rtarea", "rtedge", "rtweight", "rtabort", "rtcheck", "rtwizard", "simulate", "tire", NULL
	};
	for (size_t i = 0; wave5_schema_cmds[i]; i++)
	    CHECK(check_schema_exists(wave5_schema_cmds[i]), "Wave 5 schema should be published");
    }
    {
	const char *wave6_schema_cmds[] = {
	    "analyze", "annotate", "adc", "debug", "debugbu", "debugdir", "debuglib", "debugnmg",
	    "edarb", "edit", "protate", "pscale", "ptranslate", "human", "joint", "joint2",
	    "mat4x3pnt", "mat_ae", "mat_mul", "mat_scale_about_pnt", "make",
	    "metaball_delete_pnt", "metaball_move_pnt", "mouse_add_metaball_pnt", "mouse_move_metaball_pnt",
	    "find_arb_edge", "move_arb_edge", "move_arb_face", "mrot", "in", NULL
	};
	for (size_t i = 0; wave6_schema_cmds[i]; i++)
	    CHECK(check_schema_exists(wave6_schema_cmds[i]), "Wave 6 schema should be published");
    }
    CHECK(ged_cmd_semantic_provider_exists("ged.db_path"), "ged.db_path provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.vector_group"), "ged.vector_group provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.search.type"), "ged.search.type provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.view"), "ged.view provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.command_name"), "ged.command_name provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.primitive_type"), "ged.primitive_type provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.geometry_or_primitive"), "ged.geometry_or_primitive provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.db_path_or_pattern"), "ged.db_path_or_pattern provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.db_attribute"), "ged.db_attribute provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.db_region_id"), "ged.db_region_id provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.display_mode"), "ged.display_mode provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.file_path"), "ged.file_path provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.unit"), "ged.unit provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.color"), "ged.color provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.matrix"), "ged.matrix provider is unavailable");
    CHECK(ged_cmd_semantic_provider_exists("ged.vector"), "ged.vector provider is unavailable");
    CHECK(ged_cmd_schema_lint(NULL, &lint_msgs) == 0, bu_vls_cstr(&lint_msgs));
    CHECK(audit_schema_completion_roundtrips(gedp), "schema completion round-trip audit failed");
    gedp->new_cmd_forms = 1;
    CHECK(audit_schema_completion_roundtrips(gedp), "new-form command schema completion round-trip audit failed");
    gedp->new_cmd_forms = 0;

    schema_json = ged_cmd_schema_json("brep");
    CHECK(schema_json && std::strstr(schema_json, "\"after_fixed_operands\"") &&
	std::strstr(schema_json, "\"long\":\"color\"") &&
	std::strstr(schema_json, "\"name\":\"split\""),
	"brep should publish its native fixed-object command tree");
    bu_free(schema_json, "brep native tree JSON");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "brep --color", std::strlen("brep --color"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPTION_ARG),
	"brep should retain an incomplete root color argument rather than offering subcommands");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "brep all.g sp", std::strlen("brep all.g sp"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "split") != NULL,
	"brep should complete a subcommand after its fixed object operand");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "brep all.g split -", std::strlen("brep all.g split -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-t") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-o") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--object-per-face") != NULL,
	"brep should complete options from a child schema after its fixed object operand");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "brep all.g brep -", std::strlen("brep all.g brep -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--no-evaluation") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--suffix") != NULL,
	"brep should complete options from its BREP-conversion child schema");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "brep all.g plot C", std::strlen("brep all.g plot C"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "C2") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "CDT") != NULL,
	"brep plot should complete its operation vocabulary");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "brep --color 1;2;3 all.g split -t 1 -o plates 0-3",
	std::strlen("brep --color 1;2;3 all.g split -t 1 -o plates 0-3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"brep should validate native color and split options before raw face indices");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "brep --plotres 0 all.g split",
	std::strlen("brep --plotres 0 all.g split"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"brep should reject a nonpositive plot resolution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "brep all.g intersect all.g nope 2",
	std::strlen("brep all.g intersect all.g nope 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"brep should validate intersect component indices before dispatch");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "pnts read", std::strlen("pnts read"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--format") == NULL,
	    "completion should not expose a subcommand's next phase before a separator");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "pnts read ", std::strlen("pnts read "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--format") != NULL,
	    "completion should expose subcommand options after a separator");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "erase -", std::strlen("erase -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--view") == NULL,
	    "legacy completion should filter new-form-only erase options");
    ged_cmd_completion_result_clear(&completion_result);
    gedp->new_cmd_forms = 1;
    completion_count = ged_cmd_complete_result(gedp, "erase -", std::strlen("erase -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--view") != NULL &&
	    find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-A") == NULL,
	    "new-form completion should filter legacy-only erase options");
    ged_cmd_completion_result_clear(&completion_result);
    gedp->new_cmd_forms = 0;

    completion_count = ged_cmd_complete_result(gedp, "erase -A ", std::strlen("erase -A "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING,
	    "legacy erase attribute mode should request raw attribute completion");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "erase -A region", std::strlen("erase -A region"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"legacy erase should require a value for every attribute name");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "erase -o all.g", std::strlen("erase -o all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"legacy erase should require -A when -o is specified");
    ged_cmd_validate_result_clear(&validation);

    gedp->new_cmd_forms = 1;
    CHECK(ged_cmd_validate(gedp, "erase -A region value", std::strlen("erase -A region value"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"new-form erase should reject legacy attribute selection");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "erase --mode nope", std::strlen("erase --mode nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"new-form erase should validate its numeric drawing-mode option");
    ged_cmd_validate_result_clear(&validation);
    gedp->new_cmd_forms = 0;

    schema_json = ged_cmd_schema_json("erase");
    CHECK(schema_json && std::strstr(schema_json, "\"parse_policy\":\"context_selected_form\"") != NULL,
	"erase JSON should publish its context-selected native forms");
    bu_free(schema_json, "erase grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "mater -", std::strlen("mater -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "-d") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "-s") != NULL,
	"mater should offer both native command-form short options at a prefix");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "mater -d ", std::strlen("mater -d "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "get") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "map") != NULL,
	"mater -d should complete native density subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "mater -d get --density >=1 ",
	std::strlen("mater -d get --density >=1 "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"mater get should accept a native density range filter");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mater -d map --ids-from-names --names-from-ids ",
	std::strlen("mater -d map --ids-from-names --names-from-ids "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"mater map should require exactly one native mapping mode");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("mater");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_forms\"") != NULL &&
	std::strstr(schema_json, "\"density_short\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"map\"") != NULL,
	"mater JSON should publish native shader and density command forms");
    bu_free(schema_json, "mater grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "who s", std::strlen("who s"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "solids") != NULL,
	"legacy who should complete its canonical solids child");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "who both", std::strlen("who both"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"legacy who should accept its canonical display-kind vocabulary");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "who nonsense", std::strlen("who nonsense"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"legacy who should reject an unknown display-kind value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "who solids -V current 1", std::strlen("who solids -V current 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"legacy who solids should reject new-form-only view options");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("who");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_forms\"") != NULL &&
	std::strstr(schema_json, "\"legacy_solids\"") != NULL &&
	std::strstr(schema_json, "\"new_solids\"") != NULL,
	"who JSON should publish both context-selected native forms");
    bu_free(schema_json, "who grammar JSON");
    schema_json = NULL;

    gedp->new_cmd_forms = 1;
    completion_count = ged_cmd_complete_result(gedp, "who --e", std::strlen("who --e"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--expand") != NULL,
	"new-form who should complete its expand option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "who extra", std::strlen("who extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"new-form who should reject a direct operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "who solids -V current 1", std::strlen("who solids -V current 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"new-form who solids should accept its view and detail-level fields");
    ged_cmd_validate_result_clear(&validation);
    gedp->new_cmd_forms = 0;

    completion_count = ged_cmd_complete_result(gedp, "dm ", std::strlen("dm "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "attach") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "get") != NULL,
	"dm should complete native child commands");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "dm --h", std::strlen("dm --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"dm should complete a native root option before its child phase");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "dm get -", std::strlen("dm get -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--dm") != NULL,
	"dm get should complete only its native display-manager option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "dm --verbose get --dm current variable",
	std::strlen("dm --verbose get --dm current variable"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"dm should validate root options, a child, and interspersed child options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dm debug nope", std::strlen("dm debug nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dm debug should validate its native integer level");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dm bg 1/2/3 4/5/6", std::strlen("dm bg 1/2/3 4/5/6"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID && validation.completion_type == BU_CMD_VALUE_COLOR,
	"dm bg should validate one or two native compatibility colors");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dm bg 1/2/3 4/5/6 extra", std::strlen("dm bg 1/2/3 4/5/6 extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dm bg should reject a third native color value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dm width current extra", std::strlen("dm width current extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dm width should enforce its native optional-name cardinality");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "ert -s512", std::strlen("ert -s512"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID && validation.completion_type == BU_CMD_VALUE_RAW,
	"ert should classify raytracer flags as native raw pass-through input");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("dm");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"attach\"") != NULL,
	"dm JSON should publish its executable native tree");
    bu_free(schema_json, "dm grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "npush -", std::strlen("npush -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--xpush") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--force") == NULL,
	"npush should expose its canonical native force option without the hidden alias");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "npush -v --max-depth 3 all.g",
	std::strlen("npush -v --max-depth 3 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"npush should validate native interspersed options and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "npush --max-depth nope all.g",
	std::strlen("npush --max-depth nope all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"npush should reject an invalid native max-depth value");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("npush");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"xpush\"") != NULL,
	"npush JSON should publish its executable native schema");
    bu_free(schema_json, "npush native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "graph --igraph ", std::strlen("graph --igraph "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "show") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "positions") != NULL,
	"graph should complete the selected native igraph operand vocabulary");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "graph --root-obj all.g output.svg",
	std::strlen("graph --root-obj all.g output.svg"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"graph should validate native repeatable root objects and SVG output");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "graph --igraph positions",
	std::strlen("graph --igraph positions"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"graph should validate its native legacy-mode keyword operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "graph output.txt", std::strlen("graph output.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"graph should reject a non-SVG native output operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("graph");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.db_object\"") != NULL,
	"graph JSON should publish its native repeatable root-object binding");
    bu_free(schema_json, "graph native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_exterior --v", std::strlen("bot_exterior --v"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--visibility-threshold") != NULL,
	"bot_exterior should complete its native option prefix");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_exterior --in-place all.g",
	std::strlen("bot_exterior --in-place all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_exterior should accept its native in-place single-object form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_exterior --in-place all.g copy",
	std::strlen("bot_exterior --in-place all.g copy"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_exterior should reject an in-place output name");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_exterior --visibility-threshold -0.1 all.g",
	std::strlen("bot_exterior --visibility-threshold -0.1 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_exterior should enforce the native visibility-threshold range");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_exterior");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"in-place\"") != NULL,
	"bot_exterior JSON should publish its executable native schema");
    bu_free(schema_json, "bot_exterior native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_dump -t g",
	std::strlen("bot_dump -t g"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "gltf") != NULL,
	"bot_dump should complete its native output-format keywords");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot_dump -",
	std::strlen("bot_dump -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "-t") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "gltf") == NULL,
	"bot_dump should complete an option prefix without leaking its format values");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot_dump -t",
	std::strlen("bot_dump -t"), &completion_result);
    CHECK(completion_count == 0,
	"bot_dump should wait for a separator before completing a format argument");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot_dump -t ",
	std::strlen("bot_dump -t "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "gltf") != NULL,
	"bot_dump should complete a format after its option separator");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_dump all.g -t stl -u in",
	std::strlen("bot_dump all.g -t stl -u in"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_dump should validate interspersed native options and a unit expression");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_dump -t mesh", std::strlen("bot_dump -t mesh"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_dump should reject an unsupported native output format");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_dump -u nonsense",
	std::strlen("bot_dump -u nonsense"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_dump should reject an unsupported native unit expression");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_dump -o one.stl -m many",
	std::strlen("bot_dump -o one.stl -m many"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_dump should reject mutually exclusive native output destinations");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_dump");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"format\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"gltf\"") != NULL,
	"bot_dump JSON should publish its executable native schema and format vocabulary");
    bu_free(schema_json, "bot_dump native JSON");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "bot extrude all.g", std::strlen("bot extrude all.g"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot extrude should report a missing output name as incomplete, not invalid");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot repair -o output.bot",
	std::strlen("bot repair -o output.bot"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "all.g") == NULL,
	"bot repair should not replace an edited output name with input-object completion");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot repair -o output.bot ",
	std::strlen("bot repair -o output.bot "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "all.g") != NULL,
	"bot repair should complete its input object after the output-name separator");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "find_bot_edge a",
	std::strlen("find_bot_edge a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_PATH &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "all.g") != NULL,
	"find_bot_edge should complete its native BoT path operand");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "find_bot_pnt all.g \"1 2 3\"",
	std::strlen("find_bot_pnt all.g \"1 2 3\""), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"find_bot_pnt should validate its packed native view vector");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "find_bot_edge all.g nonsense",
	std::strlen("find_bot_edge all.g nonsense"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"find_bot_edge should reject an invalid native view vector");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("get_bot_edges");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.db_path\"") != NULL,
	"get_bot_edges JSON should publish its executable native schema");
    bu_free(schema_json, "get_bot_edges native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_smooth --h",
	std::strlen("bot_smooth --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_smooth should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_smooth -t 60 smooth.bot all.g",
	std::strlen("bot_smooth -t 60 smooth.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_smooth should validate its finite native tolerance option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_smooth -t nope smooth.bot all.g",
	std::strlen("bot_smooth -t nope smooth.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_smooth should reject an invalid native tolerance option");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_smooth");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"t\"") != NULL,
	"bot_smooth JSON should publish its executable native schema");
    bu_free(schema_json, "bot_smooth native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_flip --h",
	std::strlen("bot_flip --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_flip should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_flip all.g",
	std::strlen("bot_flip all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_flip should validate its repeatable native BoT-object role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_flip ", std::strlen("bot_flip "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot_flip should require at least one native BoT-object operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_flip");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"max\":null") != NULL,
	"bot_flip JSON should publish its repeatable executable native schema");
    bu_free(schema_json, "bot_flip native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_face_sort --h",
	std::strlen("bot_face_sort --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_face_sort should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_face_sort 4 all.g",
	std::strlen("bot_face_sort 4 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_face_sort should validate its positive native piece size and BoT role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_face_sort 0 all.g",
	std::strlen("bot_face_sort 0 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_face_sort should reject a nonpositive native piece size");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_face_sort");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"triangles_per_piece\"") != NULL,
	"bot_face_sort JSON should publish its executable native schema");
    bu_free(schema_json, "bot_face_sort native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_split --h",
	std::strlen("bot_split --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_split should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_split all.g",
	std::strlen("bot_split all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_split should validate its repeatable native BoT-object role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_split ", std::strlen("bot_split "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot_split should require at least one native BoT-object operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_split");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"max\":null") != NULL,
	"bot_split JSON should publish its repeatable executable native schema");
    bu_free(schema_json, "bot_split native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_merge --h",
	std::strlen("bot_merge --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_merge should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_merge merged.bot all.g",
	std::strlen("bot_merge merged.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_merge should validate its destination and native source role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_merge merged.bot", std::strlen("bot_merge merged.bot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot_merge should require at least one native source operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_merge");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"destination\"") != NULL,
	"bot_merge JSON should publish its executable native schema");
    bu_free(schema_json, "bot_merge native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_sync --h",
	std::strlen("bot_sync --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_sync should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_sync all.g",
	std::strlen("bot_sync all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_sync should validate its repeatable native BoT-object role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_sync ", std::strlen("bot_sync "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot_sync should require at least one native BoT-object operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_sync");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"max\":null") != NULL,
	"bot_sync JSON should publish its repeatable executable native schema");
    bu_free(schema_json, "bot_sync native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_vertex_fuse --h",
	std::strlen("bot_vertex_fuse --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_vertex_fuse should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_vertex_fuse fused.bot all.g",
	std::strlen("bot_vertex_fuse fused.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_vertex_fuse should validate its native input/output roles");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_vertex_fuse fused.bot all.g extra",
	std::strlen("bot_vertex_fuse fused.bot all.g extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_vertex_fuse should reject an extra native operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_vertex_fuse");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.db_object\"") != NULL,
	"bot_vertex_fuse JSON should publish its executable native schema");
    bu_free(schema_json, "bot_vertex_fuse native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_fuse --h",
	std::strlen("bot_fuse --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_fuse should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_fuse fused.bot all.g -s",
	std::strlen("bot_fuse fused.bot all.g -s"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_fuse should validate interspersed native display options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_fuse -s -p fused.bot all.g",
	std::strlen("bot_fuse -s -p fused.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_fuse should reject mutually exclusive native output options");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_fuse");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"s\"") != NULL,
	"bot_fuse JSON should publish its executable native schema");
    bu_free(schema_json, "bot_fuse native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_condense --h",
	std::strlen("bot_condense --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_condense should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_condense condensed.bot all.g",
	std::strlen("bot_condense condensed.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_condense should validate its native input/output roles");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_face_fuse fused.bot all.g extra",
	std::strlen("bot_face_fuse fused.bot all.g extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_face_fuse should reject an extra native operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_face_fuse");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.db_object\"") != NULL,
	"bot_face_fuse JSON should publish its executable native schema");
    bu_free(schema_json, "bot_face_fuse native JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "bot_decimate --h",
	std::strlen("bot_decimate --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot_decimate should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot_decimate -f 0.5 copy.bot all.g",
	std::strlen("bot_decimate -f 0.5 copy.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot_decimate should validate its native GCT form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot_decimate -f 0.5 -c 0.1 copy.bot all.g",
	std::strlen("bot_decimate -f 0.5 -c 0.1 copy.bot all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot_decimate should reject conflicting native decimation methods");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("bot_decimate");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"help\"") != NULL,
	"bot_decimate JSON should publish its executable native schema");
    bu_free(schema_json, "bot_decimate native JSON");
    schema_json = NULL;

    CHECK(ged_cmd_schema_exists("analyze"), "analyze native conditional grammar is unavailable");
    completion_count = ged_cmd_complete_result(gedp, "analyze int", std::strlen("analyze int"),
	&completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "intersect") != NULL,
	"analyze should complete its explicit native subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "analyze all.g", std::strlen("analyze all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"analyze should retain its implicit typed summarize form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "analyze inside all.g 1 2",
	std::strlen("analyze inside all.g 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"analyze inside should require a complete packed or XYZ point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "analyze inside all.g 1/2/3",
	std::strlen("analyze inside all.g 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"analyze inside should accept a packed native XYZ point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "analyze inside all.g 1 2 3",
	std::strlen("analyze inside all.g 1 2 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"analyze inside should accept three separate native XYZ coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "analyze intersect -o result.g all.g",
	std::strlen("analyze intersect -o result.g all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"analyze intersect should require two typed Boolean operands");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "analyze subtract --o",
	std::strlen("analyze subtract --o"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--output") != NULL,
	"analyze Boolean forms should complete their executable output option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_analyze(gedp, "analyze all.g", &analysis) == 0,
	"implicit analyze summary analysis should succeed");
    token = token_matching(&analysis, "analyze all.g", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_DB_OBJECT &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"implicit analyze summary should retain database-object semantics");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("analyze");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_conditional_grammar\"") != NULL &&
	std::strstr(schema_json, "\"implicit_summary\"") != NULL,
	"analyze JSON should publish its explicit and implicit native forms");
    bu_free(schema_json, "analyze native conditional grammar JSON");
    schema_json = NULL;

    CHECK(ged_cmd_schema_exists("bot"), "bot native command tree is unavailable");
    completion_count = ged_cmd_complete_result(gedp, "bot re", std::strlen("bot re"),
	&completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "remesh") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "repair") != NULL,
	"bot should complete canonical native-tree subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot --color 1 2 3 subd --help",
	std::strlen("bot --color 1 2 3 subd --help"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot should consume a compatibility RGB triple before validating its child");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot nosuch", std::strlen("bot nosuch"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INVALID,
	"bot should reject an unknown native-tree subcommand");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot subd --a",
	std::strlen("bot subd --a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--algorithm") != NULL,
	"bot should complete options from its executable typed subcommand schema");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot subd -A nope all.g",
	std::strlen("bot subd -A nope all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot should reject an invalid typed subcommand option value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot exterior --visibility-threshold -0.1 all.g",
	std::strlen("bot exterior --visibility-threshold -0.1 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot should enforce the executable exterior child range rule");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot sync --h",
	std::strlen("bot sync --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot sync should complete its executable parent-child help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot split all.g extra",
	std::strlen("bot split all.g extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot split should reject a second object before execution");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot get m",
	std::strlen("bot get m"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "minEdge") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "maxEdge") != NULL,
	"bot get should complete canonical typed property names");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot get min all.g",
	std::strlen("bot get min all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot get should preserve its historical property-prefix spelling");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot get nope all.g",
	std::strlen("bot get nope all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot get should reject an unknown typed property");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot set or",
	std::strlen("bot set or"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "orientation") != NULL,
	"bot set should complete its canonical property names");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot set orientation all.g c",
	std::strlen("bot set orientation all.g c"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "ccw") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "cw") != NULL,
	"bot set should complete values selected by the orientation property");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot set mode all.g plate",
	std::strlen("bot set mode all.g plate"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot set should retain mode as the accepted type compatibility alias");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot set thickness all.g 2:0.5",
	std::strlen("bot set thickness all.g 2:0.5"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot set should validate its face-specific thickness grammar");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot set thickness all.g no",
	std::strlen("bot set thickness all.g no"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot set should reject an invalid typed thickness before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot set thickness all.g -1:0.5",
	std::strlen("bot set thickness all.g -1:0.5"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot set should reject a negative face-thickness index before execution");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot info all.g ",
	std::strlen("bot info all.g "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "F") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "V") != NULL,
	"bot info should complete its typed face and vertex selectors");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot info all.g F 1-3",
	std::strlen("bot info all.g F 1-3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot info should validate its established index-range mini-language");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot info all.g F bad",
	std::strlen("bot info all.g F bad"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot info should reject malformed typed indexes before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot info all.g bad",
	std::strlen("bot info all.g bad"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot info should reject an unknown detail selector before execution");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot pick all.g ",
	std::strlen("bot pick all.g "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "E") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "F") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "V") != NULL,
	"bot pick should complete its typed element selector");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot pick all.g V --f",
	std::strlen("bot pick all.g V --f"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--first") != NULL,
	"bot pick should offer --first only in its edge and vertex forms");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot pick all.g E --first 0 0 0 1 0 0",
	std::strlen("bot pick all.g E --first 0 0 0 1 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot pick should validate its explicit six-coordinate ray form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot pick all.g F --first",
	std::strlen("bot pick all.g F --first"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot pick should reject --first in the face form before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot pick all.g V 0 0 0 1 0",
	std::strlen("bot pick all.g V 0 0 0 1 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot pick should preserve a partial explicit ray as incomplete");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot check d",
	std::strlen("bot check d"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "degen_faces") != NULL,
	"bot check should complete its conditional check-kind vocabulary");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot check all.g",
	std::strlen("bot check all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot check should preserve its one-object default-solid form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot check manifold all.g",
	std::strlen("bot check manifold all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot check should validate a selected native check kind and BoT operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot check bogus all.g",
	std::strlen("bot check bogus all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot check should reject an unknown selected check kind before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot isect all.g",
	std::strlen("bot isect all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot isect should require both typed BoT operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot chull all.g copy.bot extra",
	std::strlen("bot chull all.g copy.bot extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot chull should reject more than its optional output operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot strip all.g",
	std::strlen("bot strip all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"bot strip should require its typed output name before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bot pca all.g copy.bot extra",
	std::strlen("bot pca all.g copy.bot extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bot pca should reject more than its optional output operand");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot stat --h",
	std::strlen("bot stat --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot stat should complete its native help option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bot vertex all.g 3",
	std::strlen("bot vertex all.g 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bot vertex should retain its typed object prefix and index tail");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "bot plot --h",
	std::strlen("bot plot --h"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--help") != NULL,
	"bot plot should complete its native help option before its index tail");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "bot dump --o",
	std::strlen("bot dump --o"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--output-file") != NULL,
	"bot dump should expose the executable native dump option schema");
    ged_cmd_completion_result_clear(&completion_result);
    schema_json = ged_cmd_schema_json("bot");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"repair\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"color\"") != NULL,
	"bot JSON should publish its root options and canonical child vocabulary");
    bu_free(schema_json, "bot native-tree JSON");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "select -z nope 1 2 3", std::strlen("select -z nope 1 2 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"legacy select should validate its native view-depth option");
    ged_cmd_validate_result_clear(&validation);
    gedp->new_cmd_forms = 1;
    completion_count = ged_cmd_complete_result(gedp, "select --set active ",
	std::strlen("select --set active "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "add") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "list") != NULL,
	"new-form select should complete its parser-owned subcommand vocabulary");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "select --set active a",
	std::strlen("select --set active a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "add") != NULL,
	"a partial select child command should retain the subcommand completion phase");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "select --set active frob", std::strlen("select --set active frob"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"new-form select should reject an unknown subcommand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "select --set active add all.g", std::strlen("select --set active add all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID && validation.completion_type == BU_CMD_VALUE_DB_PATH,
	"new-form select should validate add paths through its child schema");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "select list default extra", std::strlen("select list default extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"new-form select should enforce a child schema's operand cardinality");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "select --set active add all.g", &analysis) == 0,
	"new-form select grammar analysis should succeed");
    token = token_matching(&analysis, "select --set active add all.g", "add");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"new-form select should classify its selected subcommand");
    token = token_matching(&analysis, "select --set active add all.g", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND && token->value_type == BU_CMD_VALUE_DB_PATH &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"new-form select should analyze child operands through their native schema");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("select");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"add\"") != NULL,
	"select JSON should publish executable child schemas through a native tree");
    bu_free(schema_json, "select grammar JSON");
    schema_json = NULL;
    gedp->new_cmd_forms = 0;

    completion_count = ged_cmd_complete_result(gedp, "view --view list a",
	std::strlen("view --view list a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "ae") != NULL,
	"view tree completion should not mistake an option value for a subcommand");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view frob", std::strlen("view frob"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view tree should reject an unknown direct subcommand");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view faceplate grid ",
	std::strlen("view faceplate grid "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "draw") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "snap") != NULL,
	"view faceplate grid should complete its native nested vocabulary");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "view faceplate model_axes tick_",
	std::strlen("view faceplate model_axes tick_"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "tick_color") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "tick_major_color") != NULL,
	"view faceplate axes should retain the nested completion phase");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view faceplate frob",
	std::strlen("view faceplate frob"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view faceplate should reject an unknown native child");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "view faceplate grid draw", &analysis) == 0,
	"view faceplate nested grammar analysis should succeed");
    token = token_matching(&analysis, "view faceplate grid draw", "grid");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view faceplate should classify grid as a subcommand");
    token = token_matching(&analysis, "view faceplate grid draw", "draw");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view faceplate should classify grid draw as a nested subcommand");
    ged_cmd_analysis_clear(&analysis);
    completion_count = ged_cmd_complete_result(gedp, "view knob --or",
	std::strlen("view knob --or"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--origin") != NULL,
	"view knob should complete options from its executable native child schema");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "view knob a",
	std::strlen("view knob a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "ax") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "ay") != NULL,
	"view knob should complete action/value stream actions");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "view knob --origin ",
	std::strlen("view knob --origin "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "v") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "k") != NULL,
	"view knob should complete canonical rotation origins");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view knob y", std::strlen("view knob y"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view knob should classify a missing action value as incomplete");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view lod ca",
	std::strlen("view lod ca"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "cache") != NULL,
	"view lod should complete actions from its executable native child schema");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "view lod cache ",
	std::strlen("view lod cache "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "clear") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "exists") != NULL,
	"view lod should complete cache actions from its native grammar");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view lod scale nope", std::strlen("view lod scale nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view lod should reject a nonnumeric scale before execution");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view vZ --n",
	std::strlen("view vZ --n"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--near") != NULL,
	"view vZ should complete its native optional near-object option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view vZ --near --far",
	std::strlen("view vZ --near --far"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"view vZ should retain separately specified native optional flags");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view obj native_schema_axes axes ",
	std::strlen("view obj native_schema_axes axes "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "create") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "axes_color") != NULL,
	"view obj should delegate axes subcommand completion through its native child tree");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_axes axes pos 1 2",
	std::strlen("view obj native_schema_axes axes pos 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj should retain axes partial XYZ validation after its object-name prefix");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view obj native_schema_axes arrow ",
	std::strlen("view obj native_schema_axes arrow "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "width") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "length") != NULL,
	"view obj should complete native arrow actions after the object-name prefix");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_axes arrow width nope",
	std::strlen("view obj native_schema_axes arrow width nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view obj should reject an invalid native arrow size before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_axes update 1",
	std::strlen("view obj native_schema_axes update 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj should preserve a partial native screen-coordinate pair");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_axes update -1 2",
	std::strlen("view obj native_schema_axes update -1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view obj should reject negative native screen coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_axes color -r 0 0",
	std::strlen("view obj native_schema_axes color -r 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj should retain a partial three-component color after native options");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view obj native_schema_line line ",
	std::strlen("view obj native_schema_line line "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "create") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "append") != NULL,
	"view obj should delegate line operations through their native child tree");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_line line create 1 2",
	std::strlen("view obj native_schema_line line create 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj line should retain an incomplete native XYZ point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_line line create 1/2/3",
	std::strlen("view obj native_schema_line line create 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"view obj line should accept a packed native XYZ point");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view obj native_schema_label label ",
	std::strlen("view obj native_schema_label label "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "create") != NULL,
	"view obj should delegate label operations through their native child tree");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_label label create text 1",
	std::strlen("view obj native_schema_label label create text 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj label should require both native screen coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_label label create text nope 2",
	std::strlen("view obj native_schema_label label create text nope 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view obj label should reject an invalid native coordinate");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "view obj native_schema_polygon polygon ",
	std::strlen("view obj native_schema_polygon polygon "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "create") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "fill_color") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "select") != NULL,
	"view obj should delegate every polygon operation through its native child tree");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp,
	"view obj native_schema_polygon polygon create 10 20 c",
	std::strlen("view obj native_schema_polygon polygon create 10 20 c"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "circ") != NULL,
	"view obj polygon should complete native constrained-shape names");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon create 10",
	std::strlen("view obj native_schema_polygon polygon create 10"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj polygon should require both native screen coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon create -1 20",
	std::strlen("view obj native_schema_polygon polygon create -1 20"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view obj polygon should reject a negative native screen coordinate");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon append 1",
	std::strlen("view obj native_schema_polygon polygon append 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj polygon should classify a partial contour point as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon fill 1 0",
	std::strlen("view obj native_schema_polygon polygon fill 1 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj polygon should classify a partial fill triple as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon fill_color 0 0",
	std::strlen("view obj native_schema_polygon polygon fill_color 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"view obj polygon should classify a partial compatibility color as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "view obj native_schema_polygon polygon csg q other",
	std::strlen("view obj native_schema_polygon polygon csg q other"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"view obj polygon should reject an invalid native CSG operation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp,
	"view obj native_schema_axes axes axes_color 0/0/255", &analysis) == 0,
	"view obj nested native schema analysis should succeed");
    token = token_matching(&analysis,
	"view obj native_schema_axes axes axes_color 0/0/255", "axes");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view obj should classify the delegated axes command as a subcommand");
    token = token_matching(&analysis,
	"view obj native_schema_axes axes axes_color 0/0/255", "axes_color");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view obj should classify the axes operation as a nested subcommand");
    token = token_matching(&analysis,
	"view obj native_schema_axes axes axes_color 0/0/255", "0/0/255");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_COLOR &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view obj should retain the delegated color role for highlighting");
    ged_cmd_analysis_clear(&analysis);
    CHECK(ged_cmd_analyze(gedp, "view ae", &analysis) == 0,
	"view tree grammar analysis should succeed");
    token = token_matching(&analysis, "view ae", "ae");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"view tree should classify the selected subcommand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("view");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"faceplate\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"vZ\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"near\"") != NULL,
	"view JSON should publish its root and child schemas through a native tree");
    bu_free(schema_json, "view grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "material import --i", std::strlen("material import --i"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--id") != NULL,
	    "material import mode options should remain completable");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_analyze(gedp, "material import --id", &analysis) == 0,
	    "material import option analysis should succeed");
    token = token_matching(&analysis, "material import --id", "--id");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "material import --id should be a native option, not an unknown token");
    ged_cmd_analysis_clear(&analysis);
    completion_count = ged_cmd_complete_result(gedp, "material get all.g p",
	std::strlen("material get all.g p"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "parent") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "physical") != NULL,
	"material property/group vocabulary should be schema-completable");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "material get all.g physical",
	std::strlen("material get all.g physical"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"material grouped get should require a property key");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material get all.g name surplus",
	std::strlen("material get all.g name surplus"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"material direct get should reject a grouped-property argument");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material set all.g physical density",
	std::strlen("material set all.g physical density"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"material grouped set should require both key and value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material set all.g name new-name surplus",
	std::strlen("material set all.g name new-name surplus"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"material direct set should reject a property-group value position");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material import density.txt",
	std::strlen("material import density.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"material import should require one native naming-mode option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material import --id --name density.txt",
	std::strlen("material import --id --name density.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"material import should reject mutually selected naming-mode options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "material import --id density.txt",
	std::strlen("material import --id density.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"material import should accept one native naming-mode option and a file operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("material");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"values\":[\"name\",\"parent\",\"source\",\"physical\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"import\"") != NULL,
	"material JSON should publish its native tree and executable property vocabulary");
    bu_free(schema_json, "material grammar JSON");
    schema_json = NULL;

    CHECK(ged_cmd_analyze(gedp, "blast --mode=0", &analysis) == 0,
	    "delegated attached-option analysis should succeed");
    token = token_matching(&analysis, "blast --mode=0", "--mode=0");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG && token->semantic_state != GED_CMD_SEMANTIC_INVALID,
	"delegated --option=value syntax should remain an option argument");
    ged_cmd_analysis_clear(&analysis);
    CHECK(ged_cmd_validate(gedp, "B --mode=0", std::strlen("B --mode=0"), &validation) == 0 &&
	validation.state != BU_CMD_VALIDATE_INVALID,
	"the B alias should delegate draw validation with its shorter command prefix");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("blast");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"delegated_grammar\"") != NULL &&
	std::strstr(schema_json, "\"delegate\":\"draw\"") != NULL,
	"blast JSON should identify draw as its parser-owned delegate");
    bu_free(schema_json, "blast grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "put new.s sp", std::strlen("put new.s sp"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "sph") != NULL,
	    "put should complete primitive types from librt descriptors");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "edit sph set_", std::strlen("edit sph set_"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "set_radius") != NULL,
	    "edit should complete primitive-specific operations from librt descriptors");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "facetize --methods ", std::strlen("facetize --methods "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "NMG") != NULL,
	    "facetize should complete available method names");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "facetize --methods NMG", std::strlen("facetize --methods NMG"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"facetize should validate its native comma-list method option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "facetize --method-opts NMG", std::strlen("facetize --method-opts NMG"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"facetize should require METHOD key=value syntax for method options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "facetize --perturb --no-perturb", std::strlen("facetize --perturb --no-perturb"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"facetize should reject mutually exclusive perturb controls in its native schema");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("facetize");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"method-opts\"") != NULL,
	"facetize JSON should expose its executable native option schema");
    bu_free(schema_json, "facetize native schema JSON");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "facetize_old --SPSR --depth 9", std::strlen("facetize_old --SPSR --depth 9"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"facetize_old should validate former second-pass SPSR options in its native schema");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("facetize_old");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"depth\"") != NULL,
	"facetize_old JSON should expose its unified native schema");
    bu_free(schema_json, "facetize_old native schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "nirt --ce", std::strlen("nirt --ce"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--center") != NULL,
	"nirt should complete options from its shared native schema");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "nirt --xyz 1 2 3 ", std::strlen("nirt --xyz 1 2 3 "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"nirt should validate its documented --xyz alias and three-token vector form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "nirt --color_odd 256/2/3 ", std::strlen("nirt --color_odd 256/2/3 "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"nirt should reject out-of-range native color values");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("nirt");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"center\"") != NULL &&
	std::strstr(schema_json, "\"kind\":\"color\"") != NULL,
	"nirt JSON should publish its executable native option schema");
    bu_free(schema_json, "nirt native schema JSON");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "clone -s old ", std::strlen("clone -s old "), &validation) == 0,
	    "clone multi-token option validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE && validation.expected == BU_CMD_EXPECT_OPTION_ARG,
	    "clone --subs should require its second token");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "clone -t 1 2", std::strlen("clone -t 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE && validation.expected == BU_CMD_EXPECT_OPTION_ARG,
	"clone should retain a partial XYZ vector as an editable option argument");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "clone --depth reg all.g", std::strlen("clone --depth reg all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"clone should accept the documented short depth alias through its native keyword schema");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "clone --depth r", std::strlen("clone --depth r"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count,
	"regions") != NULL,
	"clone should complete canonical depth keywords from its native schema");
    ged_cmd_completion_result_clear(&completion_result);
    schema_json = ged_cmd_schema_json("clone");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"depth\"") != NULL &&
	std::strstr(schema_json, "\"kind\":\"axis_keyed\"") != NULL,
	"clone JSON should expose the native typed option schema");
    bu_free(schema_json, "clone native schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "env --s", std::strlen("env --s"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count,
	"--system") != NULL,
	"env should complete generated native option rows");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "env --memory", std::strlen("env --memory"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"env should validate generated native flags");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("env");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"canonical\":\"system\"") != NULL,
	"env JSON should publish the generated native option schema");
    bu_free(schema_json, "env native schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "adc anchor_", std::strlen("adc anchor_"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count,
	"anchor_pos") != NULL,
	"adc should complete native angle-distance cursor subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "adc hv 1", std::strlen("adc hv 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"adc hv should keep a one-coordinate query-or-set form incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "adc xyz 1 2", std::strlen("adc xyz 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"adc xyz should require all three coordinates when setting a point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "adc draw 2", std::strlen("adc draw 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"adc draw should reject values outside its documented binary vocabulary");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "adc -i anchor_pos 2", std::strlen("adc -i anchor_pos 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"adc should retain its native increment flag before a child schema");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("adc");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"anchorpoint_dst\"") != NULL,
	"adc JSON should publish its native subcommand tree");
    bu_free(schema_json, "adc grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "arb cr", std::strlen("arb cr"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "create") != NULL,
	    "arb should complete its create subcommand");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_analyze(gedp, "arb create native_arb 35 25", &analysis) == 0,
	    "arb modern native-form analysis should succeed");
    token = token_matching(&analysis, "arb create native_arb 35 25", "create");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"arb modern form should classify create as a native subcommand");
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "arb legacy_arb 35 25", &analysis) == 0,
	    "arb deprecated native-form analysis should succeed");
    token = token_matching(&analysis, "arb legacy_arb 35 25", "legacy_arb");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_STRING && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"arb deprecated form should classify the first word as its name operand");
    ged_cmd_analysis_clear(&analysis);

    schema_json = ged_cmd_schema_json("arb");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_forms\"") != NULL &&
	std::strstr(schema_json, "\"subcommands\"") != NULL &&
	std::strstr(schema_json, "\"deprecated_create\"") != NULL,
	"arb JSON should publish both its modern tree and deprecated native form");
    bu_free(schema_json, "arb grammar JSON");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "arb legacy.s 35 ", std::strlen("arb legacy.s 35 "), &validation) == 0,
	    "legacy arb validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "legacy arb syntax should require both angles");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "check sur", std::strlen("check sur"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "surf_area") != NULL,
	    "check should complete analysis subcommands");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "pnts tri sp", std::strlen("pnts tri sp"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "spsr") != NULL,
	    "pnts should complete nested triangulation subcommands");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "pnts tri all.g output.bot", std::strlen("pnts tri all.g output.bot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"pnts tri should retain the implicit unit-method form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pnts tri spsr --depth nope all.g output.bot", std::strlen("pnts tri spsr --depth nope all.g output.bot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"pnts tri spsr should validate typed option arguments");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "pnts tri all.g output.bot", &analysis) == 0,
	"implicit pnts tri analysis should succeed");
    token = token_matching(&analysis, "pnts tri all.g output.bot", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_DB_OBJECT && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"implicit pnts tri input should remain a typed operand");
    ged_cmd_analysis_clear(&analysis);
    CHECK(ged_cmd_analyze(gedp, "pnts tri spsr all.g output.bot", &analysis) == 0,
	"explicit nested pnts tri analysis should succeed");
    token = token_matching(&analysis, "pnts tri spsr all.g output.bot", "spsr");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"explicit pnts tri method should be a nested subcommand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("pnts");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"spsr\"") != NULL,
	"pnts JSON should publish the triangulation command tree");
    bu_free(schema_json, "pnts grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "simulate --view-", std::strlen("simulate --view-"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--view-eye") != NULL,
	"simulate should publish native camera option completion");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "simulate --steps 0 all.g 1", std::strlen("simulate --steps 0 all.g 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"simulate should reject a non-positive step count before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "simulate --view-size -1 all.g 1", &analysis) == 0,
	"simulate native option analysis should succeed");
    token = token_matching(&analysis, "simulate --view-size -1 all.g 1", "-1");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG &&
	token->semantic_state == GED_CMD_SEMANTIC_INVALID,
	"simulate should highlight an invalid view size");
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_validate(gedp, "debug bu", std::strlen("debug bu"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"debug's native raw-tail schema should accept its generated command language");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "joint2 all.g selection", &analysis) == 0,
	"joint2 native analysis should succeed");
    token = token_matching(&analysis, "joint2 all.g selection", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_DB_OBJECT && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"joint2 should classify its joint solid as a database object");
    token = token_matching(&analysis, "joint2 all.g selection", "selection");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_KEYWORD && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"joint2 should classify its operation as a native keyword");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("joint2");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"operation\"") != NULL,
	"joint2 JSON should publish its native operand roles");
    bu_free(schema_json, "joint2 schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "lint --ray", std::strlen("lint --ray"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "--raytrace") != NULL,
	"lint should complete native long options");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "lint -vv --invalid-shape bot:empty all.g",
	std::strlen("lint -vv --invalid-shape bot:empty all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"lint should validate its native counting flag, optional custom argument, and object role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lint --tol not-a-number", std::strlen("lint --tol not-a-number"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lint should reject an invalid native numeric tolerance");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "lint -I all.g", &analysis) == 0,
	"lint should analyze an optional invalid-shape selector before an object");
    token = token_matching(&analysis, "lint -I all.g", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_DB_PATH && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"lint -I should leave a non-selector word available as its object operand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("lint");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"long\":\"invalid-shape\"") != NULL,
	"lint JSON should publish the native optional custom option");
    bu_free(schema_json, "lint schema JSON");
    schema_json = NULL;
    {
	const char *lint_help_argv[] = {"lint", "--help"};
	CHECK(ged_exec(gedp, 2, lint_help_argv) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--invalid-shape") != NULL,
	    "lint execution should parse native options and generate native help");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    completion_count = ged_cmd_complete_result(gedp, "in new.s to", std::strlen("in new.s to"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "tor") != NULL,
	"in should complete primitive types through its native operand provider");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_analyze(gedp, "in new.s tor", &analysis) == 0,
	"in native analysis should succeed");
    token = token_matching(&analysis, "in new.s tor", "tor");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_KEYWORD && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"in should classify a native primitive type operand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("in");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"primitive_type\"") != NULL,
	"in JSON should publish native primitive-specific operand roles");
    bu_free(schema_json, "in schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "process p", std::strlen("process p"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "pabort") != NULL,
	"process should complete native tree subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "process pabort not-a-pid",
	std::strlen("process pabort not-a-pid"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"process should reject a non-integer native PID operand before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "process pabort 123", &analysis) == 0,
	"process native tree analysis should succeed");
    token = token_matching(&analysis, "process pabort 123", "pabort");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"process should classify pabort as a native subcommand");
    token = token_matching(&analysis, "process pabort 123", "123");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_INTEGER && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"process should classify PID values as native integer operands");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("process");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"pabort\"") != NULL,
	"process JSON should publish its native subcommand tree");
    bu_free(schema_json, "process grammar JSON");
    schema_json = NULL;
    {
	const char *process_bad_pid_argv[] = {"process", "pabort", "not-a-pid"};
	CHECK(ged_exec(gedp, 3, process_bad_pid_argv) == BRLCAD_ERROR,
	    "process execution should apply the native PID schema before dispatch");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    CHECK(ged_cmd_validate(gedp, "arb repair -o fixed.s arb4 arb5", std::strlen("arb repair -o fixed.s arb4 arb5"), &validation) == 0,
	"arb repair output validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	    "arb repair --output-name should reject multiple inputs");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "arb repair -h", std::strlen("arb repair -h"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"arb repair help should be a complete native-schema form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "arb repair -t nope arb4", std::strlen("arb repair -t nope arb4"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"arb repair should validate its native tolerance argument");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "rotate_arb_face all.g 1 1 1/2/3",
	std::strlen("rotate_arb_face all.g 1 1 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID && validation.completion_type == BU_CMD_VALUE_VECTOR,
	"rotate_arb_face should accept a packed native XYZ rotation vector");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rotate_arb_face all.g 7 1 1/2/3",
	std::strlen("rotate_arb_face all.g 7 1 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"rotate_arb_face should reject an out-of-range face before execution");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "vdraw vlist d", std::strlen("vdraw vlist d"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "delete") != NULL,
	    "vdraw should complete nested vlist subcommands");
    CHECK(completion_result.active_command_path && BU_STR_EQUAL(completion_result.active_command_path, "vdraw vlist d"),
	"completion metadata should report the selected nested command path");
    CHECK(completion_result.starts_new_phase,
	"completion metadata should report a pending nested subcommand phase");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "vdraw vlist delete ", std::strlen("vdraw vlist delete "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.expected == BU_CMD_EXPECT_OPERAND,
	"recursive native trees should require the nested vlist delete operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "vdraw vlist delete scratch", std::strlen("vdraw vlist delete scratch"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"recursive native trees should validate a nested leaf operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "vdraw vlist delete scratch", &analysis) == 0,
	"recursive native-tree analysis should succeed");
    token = token_matching(&analysis, "vdraw vlist delete scratch", "vlist");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"recursive native-tree analysis should classify the first child");
    token = token_matching(&analysis, "vdraw vlist delete scratch", "delete");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"recursive native-tree analysis should classify the nested child");
    token = token_matching(&analysis, "vdraw vlist delete scratch", "scratch");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_STRING && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"recursive native-tree analysis should classify the nested leaf operand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("vdraw");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"subcommands\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"vlist\"") != NULL,
	"vdraw JSON should publish its nested native command tree");
    bu_free(schema_json, "vdraw grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "constraint s", std::strlen("constraint s"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "set") != NULL,
	    "constraint should complete its subcommand tree");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "show") != NULL,
	    "constraint should offer all matching subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "constraint set all.g", std::strlen("constraint set all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"constraint set should require both the object and its attribute name");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "constraint set all.g key expression", &analysis) == 0,
	"constraint native tree analysis should succeed");
    token = token_matching(&analysis, "constraint set all.g key expression", "set");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"constraint should classify set as a native subcommand");
    token = token_matching(&analysis, "constraint set all.g key expression", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_DB_OBJECT && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"constraint should classify the target as a native database-object operand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("constraint");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"expression\"") != NULL,
	"constraint JSON should publish its native subcommand tree and raw expression tail");
    bu_free(schema_json, "constraint grammar JSON");
    schema_json = NULL;
    {
	const char *constraint_incomplete_argv[] = {"constraint", "set", "all.g"};
	CHECK(ged_exec(gedp, 3, constraint_incomplete_argv) == BRLCAD_ERROR,
	    "constraint execution should reject an incomplete native set form before mutation");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    completion_count = ged_cmd_complete_result(gedp, "qray odd", std::strlen("qray odd"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "oddcolor") != NULL,
	"qray should complete native color subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "qray oddcolor 1 2", std::strlen("qray oddcolor 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"qray should preserve a partial three-channel RGB value as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "qray oddcolor 1;2;3", std::strlen("qray oddcolor 1;2;3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"qray should accept the shared packed semicolon RGB spelling");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "qray oddcolor 1;2;3", &analysis) == 0,
	"qray native tree analysis should succeed");
    token = token_matching(&analysis, "qray oddcolor 1;2;3", "oddcolor");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"qray should classify oddcolor as a native subcommand");
    token = token_matching(&analysis, "qray oddcolor 1;2;3", "1;2;3");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_COLOR && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"qray should classify a packed RGB color through its native schema");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("qray");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"kind\":\"rgb\"") != NULL,
	"qray JSON should publish the shared RGB argument shape");
    bu_free(schema_json, "qray grammar JSON");
    schema_json = NULL;
    {
	const char *qray_color_set_argv[] = {"qray", "oddcolor", "1;2;3"};
	const char *qray_color_get_argv[] = {"qray", "oddcolor"};
	const char *qray_color_restore_argv[] = {"qray", "oddcolor", "0", "255", "255"};
	CHECK(ged_exec(gedp, 3, qray_color_set_argv) == BRLCAD_OK &&
	    ged_exec(gedp, 2, qray_color_get_argv) == BRLCAD_OK &&
	    BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "1 2 3"),
	    "qray execution should apply the shared packed RGB reader");
	CHECK(ged_exec(gedp, 5, qray_color_restore_argv) == BRLCAD_OK,
	    "qray test should restore its default odd-partition color");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    completion_count = ged_cmd_complete_result(gedp, "grid c", std::strlen("grid c"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "color") != NULL,
	"grid should complete native parameter subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "grid color 1 2", std::strlen("grid color 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"grid should preserve a partial RGB triple as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "grid color 1;2;3", std::strlen("grid color 1;2;3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"grid should accept a shared packed RGB color");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "grid anchor 1 2", std::strlen("grid anchor 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"grid should preserve a partial anchor vector as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "grid draw 2", std::strlen("grid draw 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"grid should reject a draw setting other than zero or one");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "grid color 1;2;3", &analysis) == 0,
	"grid native tree analysis should succeed");
    token = token_matching(&analysis, "grid color 1;2;3", "color");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"grid should classify color as a native subcommand");
    token = token_matching(&analysis, "grid color 1;2;3", "1;2;3");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_COLOR && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"grid should classify packed RGB input through its native schema");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("grid");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"anchor\"") != NULL &&
	std::strstr(schema_json, "\"kind\":\"rgb\"") != NULL,
	"grid JSON should publish its native tree and shared RGB shape");
    bu_free(schema_json, "grid grammar JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "rect col", std::strlen("rect col"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "color") != NULL,
	"rect should complete native parameter subcommands");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "rect color 1 2", std::strlen("rect color 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"rect should preserve a partial RGB triple as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rect color 1;2;3", std::strlen("rect color 1;2;3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"rect should accept a shared packed RGB color");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rect cdim 640", std::strlen("rect cdim 640"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"rect should preserve a partial canvas dimension pair as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rect cdim 640 0", std::strlen("rect cdim 640 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"rect should reject a nonpositive canvas dimension before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rect draw 2", std::strlen("rect draw 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"rect should reject a draw setting other than zero or one");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "rect rt ", std::strlen("rect rt "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"rect should require the raytrace framebuffer port");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "rect color 1;2;3", &analysis) == 0,
	"rect native tree analysis should succeed");
    token = token_matching(&analysis, "rect color 1;2;3", "color");
    CHECK(token && token->role == GED_CMD_TOKEN_SUBCOMMAND &&
	token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"rect should classify color as a native subcommand");
    token = token_matching(&analysis, "rect color 1;2;3", "1;2;3");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_COLOR && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"rect should classify packed RGB input through its native schema");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("rect");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native_tree\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"cdim\"") != NULL &&
	std::strstr(schema_json, "\"kind\":\"rgb\"") != NULL,
	"rect JSON should publish its native tree and shared RGB shape");
    bu_free(schema_json, "rect grammar JSON");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "metaball_move_pnt -r all.g 0 1/2/3",
	std::strlen("metaball_move_pnt -r all.g 0 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"metaball move should validate its native relative flag, index, and packed point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "metaball_move_pnt all.g bad-index 1/2/3",
	std::strlen("metaball_move_pnt all.g bad-index 1/2/3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"metaball move should reject a non-integer point index before execution");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "metaball_move_pnt -r all.g 0 1/2/3", &analysis) == 0,
	"metaball native analysis should succeed");
    token = token_matching(&analysis, "metaball_move_pnt -r all.g 0 1/2/3", "1/2/3");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	token->value_type == BU_CMD_VALUE_VECTOR && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	"metaball should classify a packed point as a native vector operand");
    ged_cmd_analysis_clear(&analysis);
    schema_json = ged_cmd_schema_json("metaball_move_pnt");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"native\"") != NULL &&
	std::strstr(schema_json, "\"name\":\"point\"") != NULL &&
	std::strstr(schema_json, "\"short\":\"r\"") != NULL,
	"metaball JSON should publish its native flat schema");
    bu_free(schema_json, "metaball schema JSON");
    schema_json = NULL;
    {
	const char *metaball_bad_index_argv[] = {"metaball_delete_pnt", "all.g", "bad-index"};
	CHECK(ged_exec(gedp, 3, metaball_bad_index_argv) == BRLCAD_ERROR,
	    "metaball execution should reject a malformed index before object mutation");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    CHECK(ged_cmd_validate(gedp, "m2v_point 1 2", std::strlen("m2v_point 1 2"), &validation) == 0,
	    "typed point validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "a two-component point should be incomplete");
    CHECK(validation.completion_type == BU_CMD_VALUE_NUMBER,
	    "an incomplete point should request a numeric component");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "bo -i u c new.bin data.raw ", std::strlen("bo -i u c new.bin data.raw "), &validation) == 0,
	    "binary-object import validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	    "bo import mode should accept its four mode-specific operands");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "bo -i u ", std::strlen("bo -i u "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "f") != NULL,
	"bo import mode should complete its native minor-type vocabulary");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_analyze(gedp, "bo -o /tmp/native_bo.out all.g", &analysis) == 0,
	"bo native export analysis should succeed");
    {
	const struct ged_cmd_analysis_token *file = token_matching(&analysis, "bo -o /tmp/native_bo.out all.g", "/tmp/native_bo.out");
	const struct ged_cmd_analysis_token *object = token_matching(&analysis, "bo -o /tmp/native_bo.out all.g", "all.g");
	CHECK(file && file->role == GED_CMD_TOKEN_OPERAND && file->value_type == BU_CMD_VALUE_FILE &&
	    object && object->role == GED_CMD_TOKEN_OPERAND && object->value_type == BU_CMD_VALUE_DB_OBJECT,
	    "bo export should distinguish its output file from source binary object");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_validate(gedp, "bo -i u nope new.bin data.raw", std::strlen("bo -i u nope new.bin data.raw"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bo should reject an unsupported native minor type");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "color -e ", std::strlen("color -e "), &validation) == 0,
	    "color edit-mode validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"color -e should be a complete alternative to numeric color records");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "color 100 200 255 0 1", std::strlen("color 100 200 255 0 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"color should accept five typed color-record components");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "color 100 200 256 0 1", std::strlen("color 100 200 256 0 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"color should reject an out-of-range native RGB channel");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "color -e 100", std::strlen("color -e 100"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"color should reject a record combined with edit mode");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "edcolor", std::strlen("edcolor"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"edcolor should support its documented default-editor invocation");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "edcolor -", std::strlen("edcolor -"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-E") != NULL,
	"native edcolor completion should offer its editor option");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_schema_exists("bb"), "native bb schema is unavailable");
    completion_count = ged_cmd_complete_result(gedp, "bb -", std::strlen("bb -"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-c") != NULL,
	"native bb completion should offer its bounding-box name option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "bb -c bounds.s all.g", std::strlen("bb -c bounds.s all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"bb should accept a new bounding-box name and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "bb -o all.g moss.g", std::strlen("bb -o all.g moss.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"bb should reject oriented-bounds requests with multiple objects");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "ocenter all.g 1 2 ", std::strlen("ocenter all.g 1 2 "), &validation) == 0,
	    "ocenter discrete-count validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "ocenter should reject a partial XYZ center as incomplete");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "adjust tor.s V ", std::strlen("adjust tor.s V "), &validation) == 0,
	    "adjust pair validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "adjust should require a value after an attribute name");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "pull -d all.g", std::strlen("pull -d all.g"), &validation) == 0,
	    "pull option validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	    "pull should accept its documented debug option before the root object");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "attr s", std::strlen("attr s"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "set") != NULL,
	    "attr should complete its published subcommand tree");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "show") != NULL,
	    "attr should offer all matching subcommands");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "exists -e a", std::strlen("exists -e a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "exists should classify expression operands as database objects");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "exists should complete object operands");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "exists ", std::strlen("exists "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"exists should require a native expression operand");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "comb_std new.c u a", std::strlen("comb_std new.c u a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "comb_std should classify boolean-expression objects");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "comb_std should complete boolean-expression objects");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "c native_comb_std.c", std::strlen("c native_comb_std.c"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"c should require a Boolean expression unless a region-kind flag selects the flag-only form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "c -r -c all.g", std::strlen("c -r -c all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"c should retain legacy last-region-flag semantics without a spurious conflict");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "combmem -r 0 all.g u a", std::strlen("combmem -r 0 all.g u a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "combmem should classify the member slot in a transform record");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "combmem should complete member objects");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "combmem -r 5 all.g u tor nope", std::strlen("combmem -r 5 all.g u tor nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"combmem should reject a non-numeric native transform component");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "erase a", std::strlen("erase a"), &completion_result);
    if (completion_result.completion_type != BU_CMD_VALUE_DB_PATH)
	bu_log("ERROR: legacy erase completion type is %d, expected %d (hint: %s)\n",
		(int)completion_result.completion_type, (int)BU_CMD_VALUE_DB_PATH,
		completion_result.hint ? completion_result.hint : "(null)");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_PATH,
	    "legacy erase should request database-path completion");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "list all.g/t", std::strlen("list all.g/t"), &completion_result);
    CHECK(completion_count > 0, "list should complete database paths");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	    "list path completion should offer tor.r");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search /", std::strlen("search /"), &completion_result);
    CHECK(completion_count > 0 && completion_result.completion_candidates[0] &&
	completion_result.completion_candidates[0][0],
	"search should complete at least one root database path");
    {
	std::string root_seed(1, completion_result.completion_candidates[0][0]);
	std::string search_root_input = "search /" + root_seed;
	ged_cmd_completion_result_clear(&completion_result);
	completion_count = ged_cmd_complete_result(gedp, search_root_input.c_str(),
	    search_root_input.size(), &completion_result);
	CHECK(completion_count > 0, "search should complete a seeded root database path");
	CHECK(completion_result.replacement_start == std::strlen("search /"),
	    "root database-path completion should preserve the leading slash");
	for (size_t i = 0; i < completion_result.completion_count; i++) {
	    CHECK(completion_result.completion_candidates[i][0] == root_seed[0],
		"root database-path completion candidates must match the active seed");
	}
	ged_cmd_completion_result_clear(&completion_result);
    }

    CHECK(ged_cmd_analyze(gedp, "region new.r u all.g", &analysis) == 0,
	    "custom region analysis should succeed");
    {
	const struct ged_cmd_analysis_token *op_token = token_matching(&analysis, "region new.r u all.g", "u");
	const struct ged_cmd_analysis_token *member_token = token_matching(&analysis, "region new.r u all.g", "all.g");
	CHECK(op_token && op_token->role == GED_CMD_TOKEN_OPERAND && op_token->value_type == BU_CMD_VALUE_KEYWORD,
		"region operation analysis should publish a keyword operand role");
	CHECK(member_token && member_token->role == GED_CMD_TOKEN_OPERAND && member_token->value_type == BU_CMD_VALUE_DB_OBJECT,
		"region member analysis should publish a database-object operand role");
	CHECK(member_token->semantic_state == GED_CMD_SEMANTIC_VALID,
		"region member analysis should validate existing database objects");
    }

    completion_count = ged_cmd_complete_result(gedp, "comb new.c u a", std::strlen("comb new.c u a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "comb should switch from operation to member-object completion");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "comb member completion should offer matching database objects");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "comb new.c ", std::strlen("comb new.c "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "u") != NULL,
	"comb should complete boolean operations after its target combination");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "comb -w all.g u tor.r", std::strlen("comb -w all.g u tor.r"), &validation) == 0,
	    "comb native validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	    "comb tree-action modes should reject operation/object pairs");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "comb -c -r all.g", std::strlen("comb -c -r all.g"), &validation) == 0,
	"comb region-flag conflict validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"comb should reject conflicting region-flag options");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "comb -d all.g u tor.r", std::strlen("comb -d all.g u tor.r"), &validation) == 0,
	"comb decimation validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"comb decimation should reject operation/object pairs");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "comb new.c q all.g", std::strlen("comb new.c q all.g"), &validation) == 0,
	"comb operation validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"comb should reject an invalid boolean operation before editing the database");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "comb new.c u", std::strlen("comb new.c u"), &validation) == 0,
	"comb incomplete-pair validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"comb should require a member object after every boolean operation");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "comb new.c u tor.r", &analysis) == 0,
	"native comb analysis should succeed");
    {
	const struct ged_cmd_analysis_token *operation = token_matching(&analysis, "comb new.c u tor.r", "u");
	const struct ged_cmd_analysis_token *member = token_matching(&analysis, "comb new.c u tor.r", "tor.r");
	CHECK(operation && operation->role == GED_CMD_TOKEN_OPERAND &&
	    operation->value_type == BU_CMD_VALUE_KEYWORD && operation->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native comb should classify a boolean operation");
	CHECK(member && member->role == GED_CMD_TOKEN_OPERAND &&
	    member->value_type == BU_CMD_VALUE_DB_OBJECT && member->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native comb should classify and validate a member object");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_validate(gedp, "region new.r q all.g", std::strlen("region new.r q all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"region should reject an invalid Boolean operation through its native schema");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "region new.r u", std::strlen("region new.r u"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"region should require an object after a Boolean operation");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "sphgroup native_sphgroup.g all.g", &analysis) == 0,
	"sphgroup native analysis should succeed");
    {
	const struct ged_cmd_analysis_token *group = token_matching(&analysis, "sphgroup native_sphgroup.g all.g", "native_sphgroup.g");
	const struct ged_cmd_analysis_token *sphere = token_matching(&analysis, "sphgroup native_sphgroup.g all.g", "all.g");
	CHECK(group && group->value_type == BU_CMD_VALUE_STRING &&
	    sphere && sphere->value_type == BU_CMD_VALUE_DB_OBJECT,
	    "sphgroup should distinguish its new group name from its existing sphere");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "track native_track 1 1 1 1 1 1 1 1 1 1 1 1 1", &analysis) == 0,
	"track native analysis should succeed");
    {
	const struct ged_cmd_analysis_token *dimension = token_matching(&analysis, "track native_track 1 1 1 1 1 1 1 1 1 1 1 1 1", "1");
	CHECK(dimension && dimension->value_type == BU_CMD_VALUE_NUMBER,
	    "track dimensions should be published as numeric native operands");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "annotate -n label -p 0 0 0 all.g", &analysis) == 0,
	"annotate native analysis should succeed");
    {
	const struct ged_cmd_analysis_token *point = token_matching(&analysis, "annotate -n label -p 0 0 0 all.g", "0");
	const struct ged_cmd_analysis_token *object = token_matching(&analysis, "annotate -n label -p 0 0 0 all.g", "all.g");
	CHECK(point && point->role == GED_CMD_TOKEN_OPTION_ARG && point->value_type == BU_CMD_VALUE_VECTOR &&
	    object && object->role == GED_CMD_TOKEN_OPERAND && object->value_type == BU_CMD_VALUE_DB_PATH,
	    "annotate should distinguish its vector option from database-path operands");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_validate(gedp, "annotate -p 0 nope 0 all.g", std::strlen("annotate -p 0 nope 0 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"annotate should reject a non-numeric native point component");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "move_all -f ", std::strlen("move_all -f "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_FILE,
	    "move_all -f should request filesystem completion");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "move_all all.g ", std::strlen("move_all all.g "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING,
	"move_all should classify its destination as a new-name string rather than an existing object");
    ged_cmd_completion_result_clear(&completion_result);

    {
	std::string moss_path(av[1]);
	size_t slash = moss_path.find_last_of("/\\");
	std::string moss_seed = (slash == std::string::npos) ? std::string("mo") : moss_path.substr(0, slash + 1) + "mo";
	std::string file_input = std::string("move_all -f ") + moss_seed;
	completion_count = ged_cmd_complete_result(gedp, file_input.c_str(), file_input.size(), &completion_result);
	CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, moss_path.c_str()) != NULL,
		"filesystem completion should offer moss.g while preserving its path prefix");
	ged_cmd_completion_result_clear(&completion_result);
    }

    CHECK(ged_cmd_validate(gedp, "move_all -f mappings.txt all.g renamed.g", std::strlen("move_all -f mappings.txt all.g renamed.g"), &validation) == 0,
	    "move_all custom validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	    "move_all should reject mapping-file mode combined with old/new operands");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "move_all all.g renamed.g", std::strlen("move_all all.g renamed.g"), &validation) == 0,
	"move_all native validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"move_all should accept exactly its source and destination form");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "move_all all.g", std::strlen("move_all all.g"), &validation) == 0,
	"move_all incomplete native validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"move_all should require a destination name when not using -f");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "move_all all.g renamed.g", &analysis) == 0,
	"move_all native analysis should succeed");
    {
	const struct ged_cmd_analysis_token *source = token_matching(&analysis, "move_all all.g renamed.g", "all.g");
	const struct ged_cmd_analysis_token *destination = token_matching(&analysis, "move_all all.g renamed.g", "renamed.g");
	CHECK(source && source->value_type == BU_CMD_VALUE_DB_OBJECT &&
	    destination && destination->value_type == BU_CMD_VALUE_STRING,
	    "move_all should distinguish its existing source from its new destination name");
    }
    ged_cmd_analysis_clear(&analysis);

    completion_count = ged_cmd_complete_result(gedp, "region new.r ", std::strlen("region new.r "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "u") != NULL,
	    "region should complete canonical boolean operations");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "region new.r u a", std::strlen("region new.r u a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "region should switch from operation to database-object completion");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "region member completion should offer matching database objects");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "putmat all.g/tor.r 1 0", std::strlen("putmat all.g/tor.r 1 0"), &validation) == 0,
	    "putmat custom validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "putmat should classify a partial 16-element matrix as incomplete");
    CHECK(validation.completion_type == BU_CMD_VALUE_NUMBER,
	    "putmat partial matrices should request numeric values");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "putmat all.g/tor.r ",
	std::strlen("putmat all.g/tor.r "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "I") != NULL,
	"putmat should offer its identity-matrix shorthand after a valid arc");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp,
	"putmat all.g/tor.r \"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\"",
	std::strlen("putmat all.g/tor.r \"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\""),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_VALID,
	"putmat should accept one packed finite sixteen-number matrix");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp,
	"putmat all.g/tor.r \"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 nope\"",
	std::strlen("putmat all.g/tor.r \"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 nope\""),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INVALID,
	"putmat should reject a nonnumeric packed matrix element");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "putmat all.g/tor.r/tor I",
	std::strlen("putmat all.g/tor.r/tor I"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"putmat should reject paths that do not identify exactly one arc");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "dup ", std::strlen("dup "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_FILE,
	"dup should require a database file and expose filesystem completion");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "put native_put_schema.s no_such_type",
	std::strlen("put native_put_schema.s no_such_type"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"put should reject an unknown primitive type before constructing an object");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp,
	"rfarb native_rfarb_for_schema.s 0 0 0 45 30 ",
	std::strlen("rfarb native_rfarb_for_schema.s 0 0 0 45 30 "),
	&completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "x") != NULL,
	"rfarb should complete coordinate selectors from its native schema");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "savekey key.txt 1oops",
	std::strlen("savekey key.txt 1oops"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"savekey should reject a partially numeric time before opening its output file");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "importFg4Section native_importfg4.s nope",
	std::strlen("importFg4Section native_importfg4.s nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"importFg4Section should reject a noninteger section before importing");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "mrot 0 0", std::strlen("mrot 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_NUMBER,
	"mrot should require a complete three-angle native form");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "rrt -M", std::strlen("rrt -M"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID &&
	validation.completion_type == BU_CMD_VALUE_RAW,
	"rrt should classify delegated raytracer flags as native raw pass-through arguments");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "arced all.g/tor.r ", std::strlen("arced all.g/tor.r "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_RAW,
	"arced should require a native delegated animation command after its arc path");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "tol a", std::strlen("tol a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "abs") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "absmax") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "absmin") != NULL,
	"tol should complete its native tolerance-name vocabulary");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "tol abs 0.1 rel ", std::strlen("tol abs 0.1 rel "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_NUMBER,
	"tol should require a value after every native setting keyword");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "tol rel 1", std::strlen("tol rel 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tol should reject an out-of-range native relative tolerance");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "tol abs NaN", std::strlen("tol abs NaN"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tol should reject a nonfinite native tolerance");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "slew 0", std::strlen("slew 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_NUMBER,
	"slew should require a second native coordinate after a scalar seed");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "slew 0 NaN", std::strlen("slew 0 NaN"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"slew should reject a nonfinite native coordinate");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "rot 0 0", std::strlen("rot 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_NUMBER,
	"rot should require a complete native three-angle vector");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "rot -m -v 0 0 0", std::strlen("rot -m -v 0 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"rot should reject mutually exclusive native coordinate flags");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "rot 0 0 NaN", std::strlen("rot 0 0 NaN"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"rot should reject a nonfinite native angle");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "rot_about ", std::strlen("rot_about "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "m") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "v") != NULL,
	"rot_about should complete its native pivot vocabulary");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "tra 0 0", std::strlen("tra 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_NUMBER,
	"tra should require a complete native three-component translation");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "tra -m -v 0 0 0", std::strlen("tra -m -v 0 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tra should reject mutually exclusive native coordinate flags");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "tra 0 0 NaN", std::strlen("tra 0 0 NaN"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tra should reject a nonfinite native translation component");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "edmater a", std::strlen("edmater a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	"edmater should classify its native combination operands as database objects");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "edmater -E", std::strlen("edmater -E"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPTION_ARG),
	"edmater should require an editor command after its native -E option");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "edmater -E editor ", std::strlen("edmater -E editor "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPERAND),
	"edmater should require a combination after its native editor option");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "editit -f ", std::strlen("editit -f "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_FILE,
	"editit should classify its native -f argument as a file path");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "editit -e editor ", std::strlen("editit -e editor "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPERAND),
	"editit should require a file after its native edit-string option");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "editit -f file positional", std::strlen("editit -f file positional"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"editit should reject a positional file when -f already supplied one");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "red -", std::strlen("red -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-f") != NULL,
	"red should complete its native force option");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "red -E", std::strlen("red -E"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPTION_ARG),
	"red should require an editor command after its native -E option");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "red -f ", std::strlen("red -f "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPERAND),
	"red should require a combination after its native force option");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "move_arb_edge ", std::strlen("move_arb_edge "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_PATH,
	"move_arb_edge should classify its native ARB input as a database path");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "move_arb_edge all.g nope \"0 0 0\"", std::strlen("move_arb_edge all.g nope \"0 0 0\""), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"move_arb_edge should reject a non-integer native edge index");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "move_arb_face all.g 1 \"0 0 NaN\"", std::strlen("move_arb_face all.g 1 \"0 0 NaN\""), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"move_arb_face should reject a nonfinite native point component");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "move_arb_face all.g 0 \"0 0 0\"", std::strlen("move_arb_face all.g 0 \"0 0 0\""), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"move_arb_face should reject a non-positive native face index");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "find_arb_edge all.g \"0 0 NaN\" 1", std::strlen("find_arb_edge all.g \"0 0 NaN\" 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"find_arb_edge should reject a nonfinite native view point");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "bev -", std::strlen("bev -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-t") != NULL,
	"bev should complete its native triangulation option");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "bev native_bev ", std::strlen("bev native_bev "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_DB_OBJECT,
	"bev should request an input object after its native output name");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "bev native_bev all.g + ", std::strlen("bev native_bev all.g + "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	validation.completion_type == BU_CMD_VALUE_DB_OBJECT,
	"bev should request an input object after a boolean operation");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "illum -", std::strlen("illum -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-n") != NULL,
	"illum should complete its native disable-illumination flag");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "illum -n ", std::strlen("illum -n "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPERAND),
	"illum should require an object after its native disable flag");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "illum -n -n all.g", std::strlen("illum -n -n all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"illum should reject repeated native disable flags");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "inside a", std::strlen("inside a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	"inside should classify its native outside primitive as a database object");
    ged_cmd_completion_result_clear(&completion_result);


    completion_count = ged_cmd_complete_result(gedp, "stat -C name,ty", std::strlen("stat -C name,ty"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "name,type") != NULL,
	    "stat column completion should replace the active comma-list element");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "stat -S name,!si", std::strlen("stat -S name,!si"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "name,!size") != NULL,
	    "stat sort completion should preserve per-column reverse markers");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "stat -S", std::strlen("stat -S"), &completion_result);
    CHECK(completion_count == 0,
	"stat should wait for a separator before completing sort columns");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "stat -S ", std::strlen("stat -S "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "size") != NULL,
	"stat should complete sort columns after its option separator");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "stat --col", std::strlen("stat --col"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--columns") != NULL,
	    "stat should complete its canonical columns option");
    ged_cmd_completion_result_clear(&completion_result);

    {
	const char *stat_av[] = {"stat", "-q", "all.g"};
	CHECK(ged_exec_stat(gedp, 3, stat_av) == BRLCAD_OK,
		"stat -q should remain available after fixing the duplicate option row");
    }

    completion_count = ged_cmd_complete_result(gedp, "pathlist --n", std::strlen("pathlist --n"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--noleaf") != NULL,
	    "pathlist should prefer canonical --noleaf completion");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "expand t", std::strlen("expand t"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor") != NULL,
	    "expand should use its native path-or-pattern operand provider");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "prefix native_ a", std::strlen("prefix native_ a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "prefix should complete its repeated native database-object role");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "kill -f -n all.g", std::strlen("kill -f -n all.g"), &validation) == 0,
	    "kill conflict validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	    "kill -f and -n should be classified as mutually exclusive");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "summary --obj all.g all.g", std::strlen("summary --obj all.g all.g"), &validation) == 0,
	    "summary custom validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"summary should reject simultaneous option and positional object specifications");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "summary pr", std::strlen("summary pr"), &validation) == 0,
	"summary legacy-type validation should run through its native semantic provider");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"summary should classify documented concatenated p/r/g selectors as valid");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "summary --obj a", std::strlen("summary --obj a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	"summary --obj should complete database objects through its native schema");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "whichid --ro", std::strlen("whichid --ro"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--root") != NULL,
	"whichid should offer its native database-root option");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "whichid 1:3", std::strlen("whichid 1:3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"whichid should validate a documented native inclusive range");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "whichid 1bad", std::strlen("whichid 1bad"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"whichid should reject malformed ranges before database traversal");
    ged_cmd_validate_result_clear(&validation);

    schema_json = ged_cmd_schema_json("whichid");
    CHECK(schema_json && std::strstr(schema_json, "\"shape\":{\"kind\":\"range_pattern\"") != NULL,
	"whichid native JSON should publish its positional range shape");
    bu_free(schema_json, "whichid schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "zap --view d", std::strlen("zap --view d"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "default") != NULL,
	    "ged.view provider should complete the default view");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "zap -V", std::strlen("zap -V"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPTION_ARG),
	"zap should require a native view name after -V");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "zap unexpected", std::strlen("zap unexpected"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"zap should reject a native positional operand");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "instance all.g new_comb ", std::strlen("instance all.g new_comb "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "u") != NULL,
	    "instance should offer canonical union operation");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "+") != NULL,
	    "instance should offer canonical intersection operation");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "instance all.g native_instance_for_schema.g x",
	std::strlen("instance all.g native_instance_for_schema.g x"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"instance should accept its native intersection-operation alias");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "comb_color all.g 12oops 0 0",
	std::strlen("comb_color all.g 12oops 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"comb_color should reject a non-integer RGB channel in native validation");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "adjust tor r_a", std::strlen("adjust tor r_a"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"adjust should require a value for every native primitive attribute name");
    ged_cmd_validate_result_clear(&validation);

    completion_count = ged_cmd_complete_result(gedp, "rm -", std::strlen("rm -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--force") != NULL,
	"rm should prefer canonical --force completion");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-F") == NULL,
	    "rm should accept but not suggest the legacy -F alias");
    CHECK(completion_result.replacement_start == std::strlen("rm ") &&
	    completion_result.replacement_end == std::strlen("rm -"),
	"option completion should replace its seed rather than append to it");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "rm --force --dry-run all.g", std::strlen("rm --force --dry-run all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"rm should validate its native force and dry-run options with a path operand");
    ged_cmd_validate_result_clear(&validation);
    schema_json = ged_cmd_schema_json("rm");
    CHECK(schema_json && std::strstr(schema_json, "\"shape\":{\"kind\":\"custom\"") != NULL,
	"rm native JSON should publish its path-or-glob operand shape");
    bu_free(schema_json, "rm schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "draw --mode 1", std::strlen("draw --mode 1"), &completion_result);
    CHECK(completion_count == 1 && find_completion(completion_result.completion_candidates,
	    (int)completion_result.completion_count, "1") != NULL,
	    "draw --mode should consume a separate argument and complete valid modes");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_INTEGER,
	    "draw --mode argument should remain an integer rather than a database path");
    CHECK(completion_result.replacement_start == std::strlen("draw --mode "),
	    "draw --mode completion should replace only its separate argument");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_analyze(gedp, "draw --mode 1 /all.g/tor.r", &analysis) == 0,
	    "draw spaced mode analysis should succeed");
    token = token_matching(&analysis, "draw --mode 1 /all.g/tor.r", "1");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG && token->value_type == BU_CMD_VALUE_INTEGER &&
	    token->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "draw --mode 1 should be a valid integer option argument");
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "draw --mode=1 /all.g/tor.r", &analysis) == 0,
	    "draw equals mode analysis should succeed");
    token = token_matching(&analysis, "draw --mode=1 /all.g/tor.r", "--mode=1");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "draw --mode=1 should also be valid");
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "draw --mode 5 /all.g/tor.r", &analysis) == 0,
	    "draw highest valid mode analysis should succeed");
    token = token_matching(&analysis, "draw --mode 5 /all.g/tor.r", "5");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG && token->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "draw mode 5 should remain valid");
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "draw --mode 6 /all.g/tor.r", &analysis) == 0,
	    "draw invalid mode analysis should succeed");
    token = token_matching(&analysis, "draw --mode 6 /all.g/tor.r", "6");
    CHECK(token && token->role == GED_CMD_TOKEN_OPTION_ARG && token->semantic_state == GED_CMD_SEMANTIC_INVALID,
	    "draw mode values outside 0..5 should be invalid");
    ged_cmd_analysis_clear(&analysis);

    schema_json = ged_cmd_schema_json("draw");
    CHECK(schema_json && std::strstr(schema_json, "\"parse_policy\":\"interspersed\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.draw_mode\"") != NULL &&
	std::strstr(schema_json, "\"semantic_provider\":\"ged.view\"") != NULL,
	"draw should publish its executable native option schema");
    bu_free(schema_json, "draw native schema JSON");
    schema_json = NULL;
    schema_json = ged_cmd_schema_json("loadview");
    CHECK(schema_json && std::strstr(schema_json, "\"type\":\"file\"") != NULL,
	"loadview should publish its native single-file schema");
    bu_free(schema_json, "loadview native schema JSON");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "pathsum all.g/t", std::strlen("pathsum all.g/t"), &completion_result);
    CHECK(completion_count > 0, "pathsum should complete database paths");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	    "pathsum path completion should offer tor.r");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "tree -o ", std::strlen("tree -o "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_FILE,
	    "tree -o should request filesystem completion");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "tree -", std::strlen("tree -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-v") != NULL,
	    "tree should offer its native repeatable verbosity option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "copy t", std::strlen("copy t"), &completion_result);
    CHECK(completion_count > 0, "copy should complete its source object operand");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "copy source completion should report a database object role");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "copy tor ", std::strlen("copy tor "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING,
	    "copy destination completion should report a new-name string role");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "move t", std::strlen("move t"), &completion_result);
    CHECK(completion_count > 0 && completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "move source completion should report an existing database object role");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "move tor ", std::strlen("move tor "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING,
	    "move destination completion should report a new-name string role");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "group native_group ", std::strlen("group native_group "), &completion_result);
    CHECK(completion_count > 0 && completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	    "group member completion should report an existing database object role");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "kill -", std::strlen("kill -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-f") != NULL,
	    "kill should offer -f");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-n") != NULL,
	    "kill should offer -n");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-q") != NULL,
	    "kill should offer -q");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "get all.g/t", std::strlen("get all.g/t"), &completion_result);
    CHECK(completion_count > 0, "get should complete its database path operand");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	    "get path completion should offer tor.r");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "showmats -", std::strlen("showmats -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-a") != NULL,
	    "showmats should offer its native -a option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "ae2dir -", std::strlen("ae2dir -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-i") != NULL,
	"ae2dir should offer its native inverse flag");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "lt -", std::strlen("lt -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-c") != NULL,
	"lt should offer its native separator option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "killtree -", std::strlen("killtree -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-n") != NULL,
	"killtree should offer its native no-delete option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "edcodes -", std::strlen("edcodes -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-n") != NULL,
	"edcodes should offer its native list-only mode");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "push -", std::strlen("push -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-P") != NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-d") != NULL,
	"push should offer its native processor and debug options");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "pull -", std::strlen("pull -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-d") != NULL,
	"pull should offer its native debug option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "gdiff --grid", std::strlen("gdiff --grid"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--grid-spacing") != NULL,
	"gdiff should offer its native grid-spacing option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "concat --la", std::strlen("concat --la"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--lazy-affix") != NULL,
	"concat should offer its native lazy-affix option");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "shaded_mode ", std::strlen("shaded_mode "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "1") != NULL,
	"shaded_mode should offer native shading-mode keywords");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "wcodes output.txt t", std::strlen("wcodes output.txt t"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	"wcodes should complete its database-object operands after the output file");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "cpi t", std::strlen("cpi t"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	"cpi should complete its source database-object operand");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "cat a", std::strlen("cat a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	"cat should complete native database-object operands");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "get a", std::strlen("get a"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_PATH &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	"get should complete its native database-path operand");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "find -", std::strlen("find -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-a") != NULL,
	"find should complete its native hidden-combination flag");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "glob ", std::strlen("glob "), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING,
	"glob should classify its quoted command string as a native string operand");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "list -", std::strlen("list -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-v") != NULL,
	"list should complete its native repeatable verbosity flag");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "ls --hu", std::strlen("ls --hu"), &completion_result);
    CHECK(completion_count > 0, "ls should complete long options");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--human-readable") != NULL,
	    "ls should offer canonical --human-readable spelling");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "ls --attributes re", std::strlen("ls --attributes re"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_STRING &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") == NULL,
	"ls attribute-pair completion should not offer database-object names");
    ged_cmd_completion_result_clear(&completion_result);

    {
	const char *get_autoview_av[] = {"get_autoview", "-p"};
	const char *get_autoview_bad_av[] = {"get_autoview", "-p", "unexpected"};
	const char *screengrab_help_av[] = {"screengrab", "--help"};
	const char *screen_grab_bad_format_av[] = {"screen_grab", "--format", "no_such_image", "capture.img"};
	const char *who_help_av[] = {"who", "--help"};
	const char *dm_help_av[] = {"dm", "--help"};
	const char *npush_help_av[] = {"npush", "--help"};
	const char *bot_condense_help_av[] = {"bot_condense", "--help"};
	const char *bot_fuse_help_av[] = {"bot_fuse", "--help"};
	const char *bot_merge_help_av[] = {"bot_merge", "--help"};
	const char *bot_smooth_direct_help_av[] = {"bot_smooth", "--help"};
	const char *find_bot_edge_help_av[] = {"find_bot_edge", "--help"};
	const char *find_bot_pnt_help_av[] = {"find_bot_pnt", "--help"};
	const char *get_bot_edges_help_av[] = {"get_bot_edges", "--help"};
	const char *bot_flip_help_av[] = {"bot_flip", "--help"};
	const char *bot_face_sort_help_av[] = {"bot_face_sort", "--help"};
	const char *bot_split_help_av[] = {"bot_split", "--help"};
	const char *bot_sync_help_av[] = {"bot_sync", "--help"};
	const char *bot_vertex_fuse_help_av[] = {"bot_vertex_fuse", "--help"};
	const char *bot_decimate_help_av[] = {"bot_decimate", "--help"};
	const char *bot_dump_help_av[] = {"bot_dump", "--help"};
	const char *bot_exterior_help_av[] = {"bot_exterior", "--help"};
	const char *bot_face_fuse_help_av[] = {"bot_face_fuse", "--help"};
	const char *bot_decimate_subcommand_help_av[] = {"bot", "decimate", "--help"};
	const char *bot_chull_help_av[] = {"bot", "chull", "--help"};
	const char *bot_strip_help_av[] = {"bot", "strip", "--help"};
	const char *bot_set_help_av[] = {"bot", "set", "--help"};
	const char *bot_info_help_av[] = {"bot", "info", "--help"};
	const char *bot_pick_help_av[] = {"bot", "pick", "--help"};
	const char *bot_check_help_av[] = {"bot", "check", "--help"};
	const char *bot_help_av[] = {"bot", "--help"};
	const char *bot_color_subd_help_av[] = {"bot", "--color", "1", "2", "3", "subd", "--help"};
	const char *bot_extrude_help_av[] = {"bot", "extrude", "--help"};
	const char *bot_remesh_help_av[] = {"bot", "remesh", "--help"};
	const char *bot_repair_help_av[] = {"bot", "repair", "--help"};
	const char *bot_smooth_help_av[] = {"bot", "smooth", "--help"};
	const char *bot_subd_help_av[] = {"bot", "subd", "--help"};
	const char *autoview_help_av[] = {"autoview", "--help"};
	const char *autoview_bad_scale_av[] = {"autoview", "--scale", "not-a-number"};
	const char *view_snap_help_av[] = {"view", "snap", "--help"};
	const char *view_snap_tol_query_av[] = {"view", "snap", "--tol"};
	const char *view_snap_tol_set_av[] = {"view", "snap", "--tol", "0.5"};
	const char *view_snap_incomplete_av[] = {"view", "snap", "1"};
	const char *view_knob_help_av[] = {"view", "knob", "--help"};
	const char *view_knob_help_extra_av[] = {"view", "knob", "--help", "frob"};
	const char *view_knob_set_av[] = {"view", "knob", "--view-coords", "y", "1"};
	const char *view_knob_missing_value_av[] = {"view", "knob", "y"};
	const char *view_knob_mixed_coords_av[] = {"view", "knob", "-v", "-m", "y", "1"};
	const char *view_lod_help_av[] = {"view", "lod", "--help"};
	const char *view_lod_scale_av[] = {"view", "lod", "scale", "0.75"};
	const char *view_lod_bad_cache_av[] = {"view", "lod", "cache", "clear", "wrong_scope"};
	const char *view_axes_create_av[] = {"view", "obj", "native_schema_axes", "axes",
	    "create", "1", "2", "3"};
	const char *view_axes_color_av[] = {"view", "obj", "native_schema_axes", "axes",
	    "axes_color", "0/0/255"};
	const char *view_axes_color_query_av[] = {"view", "obj", "native_schema_axes", "axes",
	    "axes_color"};
	const char *view_axes_partial_pos_av[] = {"view", "obj", "native_schema_axes", "axes",
	    "pos", "1", "2"};
	const char *view_axes_bad_linewidth_av[] = {"view", "obj", "native_schema_axes", "axes",
	    "line_width", "0"};
	const char *view_obj_color_av[] = {"view", "obj", "native_schema_axes", "color",
	    "11", "22", "33"};
	const char *view_obj_color_query_av[] = {"view", "obj", "native_schema_axes", "color"};
	const char *view_line_create_av[] = {"view", "obj", "native_schema_line", "line",
	    "create", "1", "2", "3"};
	const char *view_line_append_av[] = {"view", "obj", "native_schema_line", "line",
	    "append", "4/5/6"};
	const char *view_line_bad_create_av[] = {"view", "obj", "native_schema_line", "line",
	    "create", "1", "2"};
	const char *view_label_create_av[] = {"view", "obj", "native_schema_label", "label",
	    "create", "native label", "1", "2", "3"};
	const char *view_polygon_create_av[] = {"view", "obj", "native_schema_polygon", "polygon",
	    "create", "10", "20", "rect"};
	const char *view_polygon_fill_color_av[] = {"view", "obj", "native_schema_polygon", "polygon",
	    "fill_color", "1", "2", "3"};
	const char *view_polygon_fill_color_query_av[] = {"view", "obj", "native_schema_polygon", "polygon",
	    "fill_color"};
	const char *view_polygon_fill_disable_av[] = {"view", "obj", "native_schema_polygon", "polygon",
	    "fill", "0"};
	const char *view_polygon_bad_create_av[] = {"view", "obj", "native_schema_polygon_bad", "polygon",
	    "create", "-1", "20"};
	const char *adc_draw_query_av[] = {"adc", "draw"};
	const char *adc_set_xyz_av[] = {"adc", "xyz", "100", "0", "0"};
	const char *adc_bad_draw_av[] = {"adc", "draw", "2"};
	const char *adc_incomplete_hv_av[] = {"adc", "hv", "1"};
	const char *tops_cluster_av[] = {"tops", "-an"};
	const char *showmats_av[] = {"showmats", "-a", "all.g/tor.r"};
	const char *wcodes_av[] = {"wcodes", "/tmp/ged_test_cmd_analysis_wcodes.txt", "all.g"};
	const char *cc_bad_av[] = {"cc", "only_name"};
	const char *cpi_bad_av[] = {"cpi", "tor.r", "new.tgc", "unexpected"};
	const char *cat_help_av[] = {"cat"};
	const char *cat_object_av[] = {"cat", "all.g"};
	const char *cat_end_options_av[] = {"cat", "--", "all.g"};
	const char *get_help_av[] = {"get"};
	const char *get_property_av[] = {"get", "tor", "r_a"};
	const char *get_extra_operand_av[] = {"get", "tor", "r_a", "extra"};
	const char *find_help_av[] = {"find"};
	const char *find_object_av[] = {"find", "tor"};
	const char *find_all_av[] = {"find", "-a", "tor"};
	const char *find_missing_object_av[] = {"find", "-a"};
	const char *glob_help_av[] = {"glob"};
	const char *glob_expression_av[] = {"glob", "l t*"};
	const char *glob_extra_operand_av[] = {"glob", "l t*", "extra"};
	const char *list_help_av[] = {"list"};
	const char *list_terse_av[] = {"list", "-t", "tor"};
	const char *list_terse_then_verbose_av[] = {"list", "-tv", "tor"};
	const char *list_verbose_then_terse_av[] = {"list", "-vt", "tor"};
	const char *list_missing_object_av[] = {"list", "-t"};
	const char *tree_object_av[] = {"tree", "all.g"};
	const char *tree_verbose_av[] = {"tree", "-vv", "all.g"};
	const char *tree_negative_depth_av[] = {"tree", "-d", "-1", "all.g"};
	const char *copy_object_av[] = {"copy", "tor", "native_copy_for_schema.tor"};
	const char *copy_missing_destination_av[] = {"copy", "tor"};
	const char *move_object_av[] = {"move", "native_copy_for_schema.tor", "native_moved_for_schema.tor"};
	const char *move_path_destination_av[] = {"move", "tor", "invalid/destination"};
	const char *group_object_av[] = {"group", "native_group_for_schema.g", "all.g", "tor"};
	const char *group_missing_member_av[] = {"group", "native_group_for_schema.g"};
	const char *comb_bad_operation_av[] = {"comb", "native_comb_for_schema.g", "q", "all.g"};
	const char *comb_conflicting_flags_av[] = {"comb", "-c", "-r", "all.g"};
	const char *hide_missing_operand_av[] = {"hide", "--"};
	const char *unhide_missing_operand_av[] = {"unhide", "--"};
	const char *remove_missing_member_av[] = {"remove", "--", "all.g"};
	const char *expand_end_options_av[] = {"expand", "--", "tor"};
	const char *expand_missing_expression_av[] = {"expand", "--"};
	const char *pathlist_noleaf_av[] = {"pathlist", "-n", "all.g"};
	const char *pathlist_legacy_noleaf_av[] = {"pathlist", "-noleaf", "all.g"};
	const char *pathlist_missing_object_av[] = {"pathlist", "-n"};
	const char *prefix_missing_object_av[] = {"prefix", "native_prefix_"};
	const char *match_end_options_av[] = {"match", "--", "tor"};
	const char *match_missing_expression_av[] = {"match", "--"};
	const char *instance_bad_operation_av[] = {"instance", "all.g", "native_instance_for_schema.g", "q"};
	const char *instance_missing_combination_av[] = {"instance", "all.g"};
	const char *comb_color_bad_channel_av[] = {"comb_color", "all.g", "0", "0", "256"};
	const char *comb_color_junk_channel_av[] = {"comb_color", "all.g", "12oops", "0", "0"};
	const char *which_shader_missing_pattern_av[] = {"which_shader", "-s"};
	const char *copyeval_path_av[] = {"copyeval", "all.g/tor.r/tor", "native_copyeval_for_schema.tor"};
	const char *copyeval_missing_output_av[] = {"copyeval", "all.g/tor.r/tor"};
	const char *copymat_bad_arc_av[] = {"copymat", "all.g/tor.r", "not_an_arc"};
	const char *shader_query_av[] = {"shader", "all.g"};
	const char *shader_missing_combination_av[] = {"shader", "--"};
	const char *item_bad_ident_av[] = {"item", "all.g", "not-an-integer"};
	const char *edcomb_bad_region_id_av[] = {"edcomb", "all.g", "0", "not-an-integer", "0", "0", "0"};
	const char *adjust_missing_value_av[] = {"adjust", "tor", "r_a"};
	const char *wmater_missing_combination_av[] = {"wmater", "/tmp/ged_test_cmd_analysis_wmater.txt"};
	const char *region_bad_operation_av[] = {"region", "native_region_for_schema.r", "q", "all.g"};
	const char *region_missing_member_av[] = {"region", "native_region_for_schema.r", "u"};
	const char *sphgroup_missing_sphere_av[] = {"sphgroup", "native_sphgroup_for_schema.g"};
	const char *track_bad_dimension_av[] = {"track", "native_track_for_schema", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "not-a-number"};
	const char *annotate_valid_av[] = {"annotate", "-n", "label", "-p", "0", "0", "0", "all.g"};
	const char *annotate_bad_point_av[] = {"annotate", "-p", "0", "nope", "0", "all.g"};
	const char *bo_bad_minor_type_av[] = {"bo", "-i", "u", "nope", "native_bo_for_schema.bin", "/tmp/ged_test_cmd_analysis_missing.bin"};
	const char *bo_missing_output_source_av[] = {"bo", "-o", "/tmp/ged_test_cmd_analysis_bo.out"};
	const char *comb_std_missing_expr_av[] = {"comb_std", "native_comb_std_for_schema.c"};
	const char *comb_std_help_av[] = {"comb_std", "-?"};
	const char *combmem_get_av[] = {"combmem", "all.g"};
	const char *combmem_bad_type_av[] = {"combmem", "-r", "99", "all.g"};
	const char *combmem_bad_transform_av[] = {"combmem", "-r", "5", "all.g", "u", "tor", "nope"};
	const char *exists_expression_av[] = {"exists", "-e", "all.g"};
	const char *exists_bad_expression_av[] = {"exists", "(", "all.g"};
	const char *putmat_get_av[] = {"putmat", "all.g/tor.r"};
	const char *putmat_identity_av[] = {"putmat", "all.g/tor.r", "I"};
	const char *putmat_packed_av[] = {"putmat", "all.g/tor.r",
	    "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"};
	const char *putmat_bad_matrix_av[] = {"putmat", "all.g/tor.r", "NaN"};
	const char *dup_missing_file_av[] = {"dup"};
	const char *dup_file_av[] = {"dup", av[1]};
	const char *dup_prefix_av[] = {"dup", av[1], "native_"};
	const char *dup_extra_operand_av[] = {"dup", av[1], "native_", "unexpected"};
	const char *put_missing_type_av[] = {"put", "native_put_missing.s"};
	const char *put_unknown_type_av[] = {"put", "native_put_unknown.s", "no_such_type"};
	const char *put_default_sphere_av[] = {"put", "native_put_schema.s", "sph"};
	const char *put_comb_valid_av[] = {"put_comb", "native_put_comb_schema.c",
	    "0 220 220", "plastic", "n", "u all.g", "n"};
	const char *put_comb_missing_region_fields_av[] = {"put_comb", "native_put_comb_missing.r",
	    "0/220/220", "plastic", "n", "u all.g", "y", "1000"};
	const char *put_comb_nonregion_fields_av[] = {"put_comb", "native_put_comb_extra.c",
	    "0/220/220", "plastic", "n", "u all.g", "n", "1000"};
	const char *put_comb_bad_color_av[] = {"put_comb", "native_put_comb_bad.c",
	    "256/0/0", "plastic", "n", "u all.g", "n"};
	const char *pipe_append_bad_point_av[] = {"pipe_append_pnt", "all.g", "0", "0", "0"};
	const char *pipe_delete_bad_index_av[] = {"pipe_delete_pnt", "all.g", "not-an-index"};
	const char *rfarb_bad_coordinate_av[] = {"rfarb", "native_rfarb_bad_axis.s",
	    "0", "0", "0", "45", "30", "q", "1", "1", "y", "1", "1", "z", "1", "1", "1"};
	const char *rfarb_zero_thickness_av[] = {"rfarb", "native_rfarb_zero.s",
	    "0", "0", "0", "45", "30", "x", "1", "1", "y", "1", "1", "z", "1", "1", "0"};
	const char *rfarb_valid_av[] = {"rfarb", "native_rfarb_for_schema.s",
	    "0", "0", "0", "45", "30", "x", "1", "1", "y", "1", "1", "z", "1", "1", "1"};
	const char *savekey_bad_time_av[] = {"savekey", "/tmp/ged_test_cmd_analysis_savekey.txt", "1oops"};
	const char *savekey_valid_av[] = {"savekey", "/tmp/ged_test_cmd_analysis_savekey.txt", "1.5"};
	const char *importfg4_bad_section_av[] = {"importFg4Section", "native_importfg4.s", "nope"};
	const char *mrot_bad_angle_av[] = {"mrot", "0", "0", "nope"};
	const char *mrot_angles_av[] = {"mrot", "0", "0", "0"};
	const char *mrot_packed_av[] = {"mrot", "0 0 0"};
	const char *arced_missing_animation_av[] = {"arced", "all.g/tor.r"};
	const char *arced_bad_path_av[] = {"arced", "all.g", "matrix", "rarc", "rot", "0", "0", "0"};
	const char *tol_query_abs_av[] = {"tol", "abs"};
	const char *tol_bad_number_av[] = {"tol", "abs", "1oops"};
	const char *slew_bad_coordinate_av[] = {"slew", "0", "1oops"};
	const char *slew_packed_vector_av[] = {"slew", "0 0"};
	const char *sv_vector_av[] = {"sv", "0", "0", "0"};
	const char *rot_bad_angle_av[] = {"rot", "0", "0", "1oops"};
	const char *rot_packed_vector_av[] = {"rot", "0 0 0"};
	const char *rot_about_query_av[] = {"rot_about"};
	const char *rot_about_bad_av[] = {"rot_about", "bad"};
	const char *tra_bad_component_av[] = {"tra", "0", "0", "1oops"};
	const char *edmater_missing_combination_av[] = {"edmater", "-E", "editor"};
	const char *editit_missing_file_av[] = {"editit", "-e", "editor"};
	const char *editit_help_av[] = {"editit", "-h"};
	const char *red_missing_combination_av[] = {"red", "-E", "editor"};
	const char *move_arb_edge_bad_index_av[] = {"move_arb_edge", "all.g", "nope", "0 0 0"};
	const char *bev_missing_expression_av[] = {"bev", "-t", "native_bev"};
	const char *zap_missing_view_av[] = {"zap", "-V"};
	const char *zap_help_av[] = {"zap", "-h"};
	const char *illum_missing_object_av[] = {"illum", "-n"};
	const char *labelvert_help_av[] = {"labelvert"};
	const char *inside_prompt_av[] = {"inside"};
	const char *pathsum_slash_path_av[] = {"pathsum", "all.g/tor.r"};
	const char *pathsum_missing_path_av[] = {"pathsum", "--"};
	const char *listeval_terse_path_av[] = {"listeval", "-t", "all.g/tor.r"};
	const char *paths_path_av[] = {"paths", "all.g"};
	const char *erase_legacy_invalid_av[] = {"erase", "-o", "all.g"};
	const char *erase_new_invalid_mode_av[] = {"erase", "--mode", "not-a-number", "all.g"};
	std::string list_terse_result;
	std::string list_verbose_result;
	std::string list_reset_result;
	const char *heal_bad_av[] = {"heal", "tor.r", "not-a-number"};
	const char *ae2dir_av[] = {"ae2dir", "-i", "0", "0"};
	const char *ae2dir_bad_av[] = {"ae2dir", "0", "not-a-number"};
	const char *dir2ae_av[] = {"dir2ae", "-i", "1", "0", "0"};
	const char *pset_bad_av[] = {"pset", "all.g/tor.r", "m", "not-a-number"};
	const char *otranslate_bad_av[] = {"otranslate", "all.g/tor.r", "0", "not-a-number", "0"};
	const char *lt_av[] = {"lt", "-c", ",", "all.g"};
	const char *push_bad_av[] = {"push", "-P", "not-a-number", "all.g"};
	const char *pull_bad_av[] = {"pull", "-x", "all.g"};
	const char *kill_no_delete_av[] = {"kill", "-n", "all.g"};
	const char *keep_bad_av[] = {"keep", "-R", "/tmp/ged_test_cmd_analysis_keep.g"};
	const char *killtree_no_delete_av[] = {"killtree", "-n", "all.g"};
	const char *killtree_bad_av[] = {"killtree", "-x", "all.g"};
	const char *edcodes_names_av[] = {"edcodes", "-n", "all.g"};
	const char *edcodes_bad_av[] = {"edcodes", "-i", "-r", "all.g"};
	const char *move_all_no_change_av[] = {"move_all", "-n", "all.g", "all_renamed_for_native_test.g"};
	const char *move_all_bad_av[] = {"move_all", "-f", "mappings.txt", "all.g", "renamed.g"};
	const char *gdiff_interspersed_av[] = {"gdiff", "all.g", "-S", "all.g"};
	const char *gdiff_bad_number_av[] = {"gdiff", "all.g", "--grid-spacing", "not-a-number", "all.g"};
	const char *concat_help_interspersed_av[] = {"concat", "moss.g", "--help"};
	const char *dbconcat_help_av[] = {"dbconcat", "-h"};
	const char *concat_conflicting_affix_av[] = {"concat", "-p", "-s", "moss.g"};
	const char *summary_help_interspersed_av[] = {"summary", "all.g", "--help"};
	const char *summary_legacy_types_av[] = {"summary", "pr"};
	const char *summary_object_av[] = {"summary", "--obj", "all.g"};
	const char *summary_conflicting_object_av[] = {"summary", "--obj", "all.g", "all.g"};
	const char *whichid_help_interspersed_av[] = {"whichid", "1", "--help"};
	const char *whichid_range_av[] = {"whichid", "1:2"};
	const char *whichid_bad_range_av[] = {"whichid", "1bad"};
	const char *whichair_root_av[] = {"whichair", "--root", "all.g", "1"};
	const char *rm_help_interspersed_av[] = {"rm", "all.g", "--help"};
	const char *rm_dry_run_av[] = {"rm", "--force", "--dry-run", "all.g"};
	const char *rm_bad_option_av[] = {"rm", "--not-an-option", "all.g"};
	const char *ls_help_interspersed_av[] = {"ls", "all.g", "--help"};
	const char *ls_short_primitives_av[] = {"ls", "-s", "all.g"};
	const char *ls_attribute_pairs_av[] = {"ls", "--attributes", "region", "yes"};
	const char *ls_attribute_cluster_av[] = {"ls", "-Ao", "region", "yes"};
	const char *ls_bad_attribute_pairs_av[] = {"ls", "region", "--attributes"};
	const char *material_create_av[] = {"material", "create", "native_schema_material", "Native Schema Material"};
	const char *material_set_group_av[] = {"material", "set", "native_schema_material", "physical", "density", "12.5"};
	const char *material_get_group_av[] = {"material", "get", "native_schema_material", "physical", "density"};
	const char *material_bad_direct_av[] = {"material", "set", "native_schema_material", "name", "New Name", "surplus"};
	const char *material_bad_import_av[] = {"material", "import", "density.txt"};
	const char *material_remove_group_av[] = {"material", "remove", "native_schema_material", "physical", "density"};
	const char *material_destroy_av[] = {"material", "destroy", "native_schema_material"};
	const char *facetize_help_av[] = {"facetize", "--help"};
	const char *facetize_old_help_av[] = {"facetize_old", "--help"};
	const char *clone_help_av[] = {"clone", "--help"};
	const char *arb_legacy_create_av[] = {"arb", "native_schema_arb_legacy", "35", "25"};
	const char *arb_modern_create_av[] = {"arb", "create", "native_schema_arb_modern", "35", "25"};
	const char *arb_bad_create_av[] = {"arb", "create", "native_schema_arb_invalid", "nope", "25"};
	const char *rotate_arb_face_av[] = {"rotate_arb_face", "native_schema_arb_modern", "1", "1", "1/2/3"};
	const char *rotate_arb_face_non_arb_av[] = {"rotate_arb_face", "all.g", "1", "1", "1/2/3"};
	CHECK(ged_exec_get_autoview(gedp, 2, get_autoview_av) == BRLCAD_OK,
		"get_autoview -p should use its native option parser and succeed");
	CHECK(ged_exec_get_autoview(gedp, 3, get_autoview_bad_av) == BRLCAD_ERROR,
		"get_autoview should reject an operand that its native schema does not allow");
	gedp->new_cmd_forms = 1;
	CHECK(ged_exec(gedp, 2, autoview_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: autoview") != NULL,
	    "autoview should render help from its native option schema");
	CHECK(ged_exec(gedp, 3, autoview_bad_scale_av) == BRLCAD_ERROR,
	    "autoview should reject a nonnumeric native scale before adjusting a view");
	gedp->new_cmd_forms = 0;
	CHECK(ged_exec(gedp, 3, view_snap_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: snap") != NULL,
	    "view snap should render help from its native option schema");
	CHECK(ged_exec(gedp, 3, view_snap_tol_query_av) == BRLCAD_OK,
	    "view snap should query its native optional tolerance argument");
	CHECK(ged_exec(gedp, 4, view_snap_tol_set_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "0.5") != NULL,
	    "view snap should set and report a native optional tolerance argument");
	CHECK(ged_exec(gedp, 3, view_snap_incomplete_av) == BRLCAD_ERROR,
		"view snap should reject an incomplete native coordinate form");
	gedp->new_cmd_forms = 1;
	CHECK(ged_exec(gedp, 3, view_knob_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Actions with a finite numeric value") != NULL,
		"view knob should render help from its native action schema");
	CHECK(ged_exec(gedp, 4, view_knob_help_extra_av) == BRLCAD_OK,
		"view knob help should bypass action-stream validation");
	CHECK(ged_exec(gedp, 5, view_knob_set_av) == BRLCAD_OK,
		"view knob should execute a native action/value pair");
	CHECK(ged_exec(gedp, 3, view_knob_missing_value_av) == BRLCAD_ERROR,
		"view knob should reject an incomplete native action/value pair");
	CHECK(ged_exec(gedp, 6, view_knob_mixed_coords_av) == BRLCAD_ERROR,
		"view knob should reject conflicting native coordinate options");
	CHECK(ged_exec(gedp, 3, view_lod_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "view lod cache") != NULL,
		"view lod should render help from its executable native schema");
	CHECK(ged_exec(gedp, 4, view_lod_scale_av) == BRLCAD_OK,
		"view lod should execute a native finite scale value");
	CHECK(ged_exec(gedp, 5, view_lod_bad_cache_av) == BRLCAD_ERROR,
	    "view lod should reject an invalid native cache scope before cache mutation");
	CHECK(ged_exec(gedp, 8, view_axes_create_av) == BRLCAD_OK,
	    "view obj axes should create an axes object through its native child schema");
	CHECK(ged_exec(gedp, 6, view_axes_color_av) == BRLCAD_OK,
	    "view obj axes should accept a native compatibility color value");
	CHECK(ged_exec(gedp, 5, view_axes_color_query_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "0 0 255") != NULL,
	    "view obj axes should query the color applied by its native child schema");
	CHECK(ged_exec(gedp, 7, view_axes_partial_pos_av) == BRLCAD_ERROR,
	    "view obj axes should reject an incomplete XYZ update before mutation");
	CHECK(ged_exec(gedp, 6, view_axes_bad_linewidth_av) == BRLCAD_ERROR,
	    "view obj axes should reject a nonpositive native line width before mutation");
	CHECK(ged_exec(gedp, 7, view_obj_color_av) == BRLCAD_OK,
	    "view obj should execute its native three-component color schema");
	int view_obj_color_query_result = ged_exec(gedp, 4, view_obj_color_query_av);
	CHECK(view_obj_color_query_result == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "11/22/33") != NULL,
	    "view obj should query the color set through its native schema");
	CHECK(ged_exec(gedp, 8, view_line_create_av) == BRLCAD_OK,
	    "view obj line should create a polyline through its native child tree");
	CHECK(ged_exec(gedp, 6, view_line_append_av) == BRLCAD_OK,
	    "view obj line should append a packed native XYZ point");
	CHECK(ged_exec(gedp, 7, view_line_bad_create_av) == BRLCAD_ERROR,
	    "view obj line should reject a partial native XYZ point before mutation");
	CHECK(ged_exec(gedp, 9, view_label_create_av) == BRLCAD_OK,
	    "view obj label should create a label through its native child tree");
	CHECK(ged_exec(gedp, 8, view_polygon_create_av) == BRLCAD_OK,
	    "view obj polygon should create a constrained polygon through its native child tree");
	CHECK(ged_exec(gedp, 8, view_polygon_fill_color_av) == BRLCAD_OK,
	    "view obj polygon should set a compatibility fill color through its native schema");
	CHECK(ged_exec(gedp, 5, view_polygon_fill_color_query_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "1/2/3") != NULL,
	    "view obj polygon should query the fill color set through its native schema");
	CHECK(ged_exec(gedp, 6, view_polygon_fill_disable_av) == BRLCAD_OK,
	    "view obj polygon should accept its native fill-disable form");
	CHECK(ged_exec(gedp, 7, view_polygon_bad_create_av) == BRLCAD_ERROR,
	    "view obj polygon should reject an invalid create coordinate before mutation");
	CHECK(ged_exec(gedp, 2, screengrab_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: screengrab") != NULL,
	    "screengrab should render native help before a display manager is required");
	CHECK(ged_exec(gedp, 4, screen_grab_bad_format_av) == BRLCAD_ERROR,
	    "screen_grab should reject an invalid native format before display capture");
	gedp->new_cmd_forms = 0;
	CHECK(ged_exec(gedp, 2, who_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage:") != NULL,
	    "legacy who should render native help before a drawable is required");
	CHECK(ged_exec(gedp, 2, dm_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Subcommands:") != NULL,
	    "dm should render native tree help before a display manager is required");
	CHECK(ged_exec(gedp, 2, npush_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--max-depth") != NULL,
	"npush should render help from its native schema before a database is required");
	CHECK(ged_exec(gedp, 2, facetize_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--method-opts") != NULL,
	    "facetize should render help from its executable native schema");
	CHECK(ged_exec(gedp, 2, facetize_old_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--samples-per-node") != NULL,
	    "facetize_old should render unified native SPSR help");
	CHECK(ged_exec(gedp, 2, clone_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--depth top|regions|primitives") != NULL,
	    "clone should execute its native option parser before rendering help");
	CHECK(ged_exec(gedp, 2, bot_condense_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "new_bot old_bot") != NULL,
	    "bot_condense should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_fuse_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "new_bot old_bot") != NULL,
	    "bot_fuse should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_merge_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "destination source") != NULL,
	    "bot_merge should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_smooth_direct_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Maximum angle") != NULL,
	    "bot_smooth should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, find_bot_edge_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "view_x") != NULL,
	    "find_bot_edge should render native help before a view is required");
	CHECK(ged_exec(gedp, 2, find_bot_pnt_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "view_x") != NULL,
	    "find_bot_pnt should render native help before a view is required");
	CHECK(ged_exec(gedp, 2, get_bot_edges_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: get_bot_edges") != NULL,
	    "get_bot_edges should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_flip_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "bot [bot2 ...]") != NULL,
	    "bot_flip should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_face_sort_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "triangles_per_piece") != NULL,
	    "bot_face_sort should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_split_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "bot [bot2 ...]") != NULL,
	    "bot_split should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_sync_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "bot [bot2 ...]") != NULL,
	    "bot_sync should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_vertex_fuse_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "new_bot old_bot") != NULL,
	    "bot_vertex_fuse should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_decimate_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: bot_decimate") != NULL,
	    "bot_decimate should render help from its native schema");
	CHECK(ged_exec(gedp, 2, bot_dump_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--output-directory") != NULL,
	    "bot_dump should render help from its native schema before a database is required");
	CHECK(ged_exec(gedp, 2, bot_exterior_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--visibility-threshold") != NULL,
	    "bot_exterior should render help from its native schema");
	CHECK(ged_exec(gedp, 2, bot_face_fuse_help_av) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "new_bot old_bot") != NULL,
	    "bot_face_fuse should render native help before a database is required");
	CHECK(ged_exec(gedp, 2, bot_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Subcommands:") != NULL,
	    "bot should render root help after native root-option parsing");
	CHECK(ged_exec(gedp, 7, bot_color_subd_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Available subdivision algorithm types") != NULL,
	    "bot should preserve a native three-component root color before child dispatch");
	CHECK(ged_exec(gedp, 3, bot_decimate_subcommand_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Available decimation methods") != NULL,
	    "bot decimate should render help from its native child schema");
	CHECK(ged_exec(gedp, 3, bot_chull_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "input_bot") != NULL,
	    "bot chull should render native child help before a database is required");
	CHECK(ged_exec(gedp, 3, bot_strip_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "output_name") != NULL,
	    "bot strip should render native child help before a database is required");
	CHECK(ged_exec(gedp, 3, bot_set_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "face_index:number") != NULL,
	    "bot set should render property-specific native child help");
	CHECK(ged_exec(gedp, 3, bot_info_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "index_or_range") != NULL,
	    "bot info should render typed index-grammar child help");
	CHECK(ged_exec(gedp, 3, bot_pick_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--first") != NULL,
	    "bot pick should render typed ray-grammar child help");
	CHECK(ged_exec(gedp, 3, bot_check_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "manifold") != NULL,
	    "bot check should render native conditional-grammar child help");
	CHECK(ged_exec(gedp, 3, bot_extrude_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--max-area-delta") != NULL,
	    "bot extrude should render help from its native child schema");
	CHECK(ged_exec(gedp, 3, bot_remesh_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--output") != NULL,
	    "bot remesh should render help from its native child schema");
	CHECK(ged_exec(gedp, 3, bot_repair_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "--max-hole-percent") != NULL,
	    "bot repair should render help from its native child schema");
	CHECK(ged_exec(gedp, 3, bot_smooth_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Available continuity options") != NULL,
	    "bot smooth should render help from its native child schema");
	CHECK(ged_exec(gedp, 3, bot_subd_help_av) == BRLCAD_OK &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Available subdivision algorithm types") != NULL,
	    "bot subd should render help from its native child schema");
	CHECK(ged_exec(gedp, 2, adc_draw_query_av) == BRLCAD_OK,
	    "adc should execute a native query child schema");
	CHECK(ged_exec(gedp, 5, adc_set_xyz_av) == BRLCAD_OK,
	    "adc should execute a complete native XYZ child schema");
	CHECK(ged_exec(gedp, 3, adc_bad_draw_av) == BRLCAD_ERROR,
	    "adc should reject an invalid native binary value before changing the view");
	CHECK(ged_exec(gedp, 3, adc_incomplete_hv_av) == BRLCAD_ERROR,
	    "adc should reject an incomplete native coordinate pair before changing the view");
	CHECK(ged_exec_tops(gedp, 2, tops_cluster_av) == BRLCAD_OK,
		"tops should execute a compact native short-flag cluster");
	CHECK(ged_exec_showmats(gedp, 3, showmats_av) == BRLCAD_OK,
		"showmats should execute a native flag and database-path parse without mutating argv");
	CHECK(ged_exec_wcodes(gedp, 3, wcodes_av) == BRLCAD_OK,
		"wcodes should execute native file and object operand parsing");
	CHECK(std::remove(wcodes_av[1]) == 0,
		"wcodes test output should be removable");
	CHECK(ged_exec_material(gedp, 4, material_create_av) == BRLCAD_OK,
		"material create should execute through its native child schema");
	CHECK(ged_exec_material(gedp, 6, material_set_group_av) == BRLCAD_OK,
		"material grouped set should execute after native property validation");
	CHECK(ged_exec_material(gedp, 5, material_get_group_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "12.5"),
		"material grouped get should retrieve the value set through the native tree");
	CHECK(ged_exec_material(gedp, 6, material_bad_direct_av) == BRLCAD_ERROR,
		"material execution should reject a direct property with a group-value tail");
	CHECK(ged_exec_material(gedp, 3, material_bad_import_av) == BRLCAD_ERROR,
		"material execution should reject an import without exactly one naming-mode option");
	CHECK(ged_exec_material(gedp, 5, material_remove_group_av) == BRLCAD_OK,
		"material grouped remove should execute through the native property grammar");
	CHECK(ged_exec_material(gedp, 3, material_destroy_av) == BRLCAD_OK,
	    "material destroy should execute through its native child schema");
	CHECK(ged_exec(gedp, 4, arb_legacy_create_av) == BRLCAD_OK,
	    "arb should retain its deprecated native positional creation form");
	CHECK(ged_exec(gedp, 5, arb_modern_create_av) == BRLCAD_OK,
	    "arb create should execute through its modern native tree form");
	CHECK(ged_exec(gedp, 5, arb_bad_create_av) == BRLCAD_ERROR,
	    "arb create should reject invalid native numeric operands before creation");
	CHECK(ged_exec(gedp, 5, rotate_arb_face_av) == BRLCAD_OK,
	    "rotate_arb_face should execute a packed native XYZ rotation vector");
	CHECK(ged_exec(gedp, 5, rotate_arb_face_non_arb_av) == BRLCAD_ERROR,
	    "rotate_arb_face should reject a non-ARB object after native preflight");
	CHECK(ged_exec_cc(gedp, 2, cc_bad_av) == BRLCAD_ERROR,
		"cc should reject an incomplete native operand sequence before editing the database");
	CHECK(ged_exec_cpi(gedp, 4, cpi_bad_av) == BRLCAD_ERROR,
		"cpi should reject an extra native operand before editing the database");
	CHECK(ged_exec_cat(gedp, 1, cat_help_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: cat") != NULL,
		"cat should retain its native help result when no object is supplied");
    CHECK(ged_exec_cat(gedp, 2, cat_object_av) == BRLCAD_OK,
	"cat should execute a native database-object operand");
	CHECK(ged_exec_cat(gedp, 3, cat_end_options_av) == BRLCAD_OK,
		"cat should retain an operand after the native end-of-options marker");
	CHECK(ged_exec_get(gedp, 1, get_help_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: get") != NULL,
		"get should retain its native help result when no object is supplied");
	CHECK(ged_exec_get(gedp, 3, get_property_av) == BRLCAD_OK &&
		!BU_STR_EMPTY(bu_vls_cstr(gedp->ged_result_str)),
		"get should execute native object and property operands");
	CHECK(ged_exec_get(gedp, 4, get_extra_operand_av) == BRLCAD_ERROR,
		"get should reject a third native positional operand before import");
	CHECK(ged_exec_find(gedp, 1, find_help_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: find") != NULL,
		"find should retain its native help result when no object is supplied");
	CHECK(ged_exec_find(gedp, 2, find_object_av) == BRLCAD_OK,
		"find should execute a native database-object operand");
	CHECK(ged_exec_find(gedp, 3, find_all_av) == BRLCAD_OK,
		"find should execute its native include-hidden flag");
	CHECK(ged_exec_find(gedp, 2, find_missing_object_av) == BRLCAD_ERROR,
		"find should reject an option-only invocation before database traversal");
	CHECK(ged_exec_glob(gedp, 1, glob_help_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: glob") != NULL,
		"glob should retain its native help result when no command string is supplied");
	CHECK(ged_exec_glob(gedp, 2, glob_expression_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "tor") != NULL,
		"glob should execute its native quoted command-string operand");
	CHECK(ged_exec_glob(gedp, 3, glob_extra_operand_av) == BRLCAD_ERROR,
		"glob should reject an extra native positional operand");
	CHECK(ged_exec_list(gedp, 1, list_help_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: list") != NULL,
		"list should retain its native help result when no object is supplied");
	CHECK(ged_exec_list(gedp, 3, list_terse_av) == BRLCAD_OK,
		"list should execute its native terse flag");
	list_terse_result = bu_vls_cstr(gedp->ged_result_str);
	CHECK(ged_exec_list(gedp, 3, list_terse_then_verbose_av) == BRLCAD_OK,
		"list should execute a compact native terse/verbose flag cluster");
	list_verbose_result = bu_vls_cstr(gedp->ged_result_str);
	CHECK(ged_exec_list(gedp, 3, list_verbose_then_terse_av) == BRLCAD_OK,
		"list should execute the reverse compact native flag cluster");
	list_reset_result = bu_vls_cstr(gedp->ged_result_str);
	CHECK(list_terse_result != list_verbose_result && list_terse_result == list_reset_result,
		"list -t must reset verbosity at its position relative to repeatable -v flags");
	CHECK(ged_exec_list(gedp, 2, list_missing_object_av) == BRLCAD_ERROR,
		"list should reject an option-only invocation before object lookup");
	CHECK(ged_exec_tree(gedp, 2, tree_object_av) == BRLCAD_OK &&
		!BU_STR_EMPTY(bu_vls_cstr(gedp->ged_result_str)),
		"tree should execute a native database-object operand");
	CHECK(ged_exec_tree(gedp, 3, tree_verbose_av) == BRLCAD_OK,
		"tree should execute a compact native repeatable verbosity flag");
    CHECK(ged_exec_tree(gedp, 4, tree_negative_depth_av) == BRLCAD_ERROR,
		"tree should reject a negative native depth before tree traversal");
    /* This fixture exercises mutations.  Remove artifacts from a previous
     * run so the source/destination assertions stay repeatable when a test
     * runner reuses its database copy. */
    {
	const char *stale_copy_av[] = {"kill", "native_copy_for_schema.tor"};
	const char *stale_move_av[] = {"kill", "native_moved_for_schema.tor"};
	if (db_lookup(gedp->dbip, stale_copy_av[1], LOOKUP_QUIET) != RT_DIR_NULL)
	    CHECK(ged_exec_kill(gedp, 2, stale_copy_av) == BRLCAD_OK,
		"test setup should remove the prior native copy artifact");
	if (db_lookup(gedp->dbip, stale_move_av[1], LOOKUP_QUIET) != RT_DIR_NULL)
	    CHECK(ged_exec_kill(gedp, 2, stale_move_av) == BRLCAD_OK,
		"test setup should remove the prior native move artifact");
	bu_vls_trunc(gedp->ged_result_str, 0);
    }
    CHECK(ged_exec_copy(gedp, 3, copy_object_av) == BRLCAD_OK,
		"copy should execute native existing-source and new-destination operands");
	CHECK(ged_exec_copy(gedp, 2, copy_missing_destination_av) == BRLCAD_ERROR,
		"copy should reject an incomplete native source/destination form");
	CHECK(ged_exec_move(gedp, 3, move_object_av) == BRLCAD_OK,
		"move should execute native existing-source and new-destination operands");
	CHECK(ged_exec_move(gedp, 3, move_path_destination_av) == BRLCAD_ERROR,
		"move should reject a path-like destination during native parsing");
	CHECK(ged_exec_group(gedp, 4, group_object_av) == BRLCAD_OK,
		"group should execute native new-group and repeated object operands");
	CHECK(ged_exec_group(gedp, 2, group_missing_member_av) == BRLCAD_ERROR,
		"group should reject a native group form without a member");
	CHECK(ged_exec_comb(gedp, 4, comb_bad_operation_av) == BRLCAD_ERROR,
		"comb should reject an invalid native boolean operation before database edits");
	CHECK(ged_exec_comb(gedp, 4, comb_conflicting_flags_av) == BRLCAD_ERROR,
		"comb should reject conflicting native region-flag options before database edits");
	CHECK(ged_exec_hide(gedp, 2, hide_missing_operand_av) == BRLCAD_ERROR,
		"hide should reject a native end-of-options marker without an object");
	CHECK(ged_exec_unhide(gedp, 2, unhide_missing_operand_av) == BRLCAD_ERROR,
		"unhide should reject a native end-of-options marker without an object");
	CHECK(ged_exec_remove(gedp, 3, remove_missing_member_av) == BRLCAD_ERROR,
		"remove should reject a native end-of-options marker without a member");
	CHECK(ged_exec_expand(gedp, 3, expand_end_options_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "tor"),
		"expand should retain a database expression after the native end-of-options marker");
	CHECK(ged_exec_expand(gedp, 2, expand_missing_expression_av) == BRLCAD_ERROR,
		"expand should reject a native end-of-options marker without an expression");
	CHECK(ged_exec_pathlist(gedp, 3, pathlist_noleaf_av) == BRLCAD_OK,
		"pathlist should execute its native no-leaf flag and object operand");
	CHECK(ged_exec_pathlist(gedp, 3, pathlist_legacy_noleaf_av) == BRLCAD_OK,
		"pathlist should retain its hidden legacy single-dash no-leaf spelling");
	CHECK(ged_exec_pathlist(gedp, 2, pathlist_missing_object_av) == BRLCAD_ERROR,
		"pathlist should reject a native option-only invocation before walking the tree");
	CHECK(ged_exec_prefix(gedp, 2, prefix_missing_object_av) == BRLCAD_ERROR,
		"prefix should reject a native prefix form without an object before renaming");
	CHECK(ged_exec_match(gedp, 3, match_end_options_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "tor"),
		"match should retain a glob expression after the native end-of-options marker");
	CHECK(ged_exec_match(gedp, 2, match_missing_expression_av) == BRLCAD_ERROR,
		"match should reject a native end-of-options marker without an expression");
	CHECK(ged_exec_instance(gedp, 4, instance_bad_operation_av) == BRLCAD_ERROR,
		"instance should reject an invalid native boolean operation before creating a combination");
	CHECK(ged_exec_instance(gedp, 2, instance_missing_combination_av) == BRLCAD_ERROR,
		"instance should reject an incomplete native source/combination form before editing");
	CHECK(ged_exec_comb_color(gedp, 5, comb_color_bad_channel_av) == BRLCAD_ERROR,
		"comb_color should reject an out-of-range native RGB channel before editing a combination");
	CHECK(ged_exec_comb_color(gedp, 5, comb_color_junk_channel_av) == BRLCAD_ERROR,
		"comb_color should reject a partially numeric native RGB channel before editing a combination");
	CHECK(ged_exec_which_shader(gedp, 2, which_shader_missing_pattern_av) == BRLCAD_ERROR,
		"which_shader should reject a native script-output flag without a shader pattern");
	CHECK(ged_exec_copyeval(gedp, 3, copyeval_path_av) == BRLCAD_OK &&
		BU_STR_EQUAL(copyeval_path_av[1], "all.g/tor.r/tor"),
		"copyeval should execute a native slash path without tokenizing caller-owned argv text");
	CHECK(ged_exec_copyeval(gedp, 2, copyeval_missing_output_av) == BRLCAD_ERROR,
		"copyeval should reject an incomplete native path/output form before copying");
	CHECK(ged_exec_copymat(gedp, 3, copymat_bad_arc_av) == BRLCAD_ERROR,
		"copymat should reject a malformed native destination arc before editing a combination");
	CHECK(ged_exec_shader(gedp, 2, shader_query_av) == BRLCAD_OK,
		"shader should execute its native combination-only query form");
	CHECK(ged_exec_shader(gedp, 2, shader_missing_combination_av) == BRLCAD_ERROR,
		"shader should reject a native end-of-options marker without a combination");
	CHECK(ged_exec_item(gedp, 3, item_bad_ident_av) == BRLCAD_ERROR,
		"item should reject a non-integer native region identifier before editing");
	CHECK(ged_exec_edcomb(gedp, 7, edcomb_bad_region_id_av) == BRLCAD_ERROR,
		"edcomb should reject a non-integer native region ID before editing");
	CHECK(ged_exec_adjust(gedp, 3, adjust_missing_value_av) == BRLCAD_ERROR,
		"adjust should reject an incomplete native attribute/value pair before editing a primitive");
	CHECK(ged_exec_wmater(gedp, 2, wmater_missing_combination_av) == BRLCAD_ERROR,
		"wmater should reject a native output-file form without a combination before opening the file");
	CHECK(ged_exec_region(gedp, 4, region_bad_operation_av) == BRLCAD_ERROR,
		"region should reject an invalid native Boolean operation before creating a region");
	CHECK(ged_exec_region(gedp, 3, region_missing_member_av) == BRLCAD_ERROR,
		"region should reject an incomplete native operation/member pair before creating a region");
	CHECK(ged_exec_sphgroup(gedp, 2, sphgroup_missing_sphere_av) == BRLCAD_ERROR,
		"sphgroup should reject a native group form without a target sphere before traversal");
	CHECK(ged_exec_track(gedp, 15, track_bad_dimension_av) == BRLCAD_ERROR,
		"track should reject an invalid native geometry dimension before creating objects");
	CHECK(ged_exec_annotate(gedp, 8, annotate_valid_av) == BRLCAD_OK,
		"annotate should execute its native options-first name, point, and object parse");
	CHECK(ged_exec_annotate(gedp, 6, annotate_bad_point_av) == BRLCAD_ERROR,
		"annotate should reject an invalid native point component before processing objects");
	CHECK(ged_exec_bo(gedp, 6, bo_bad_minor_type_av) == BRLCAD_ERROR,
		"bo should reject an invalid native minor type before creating an object");
	CHECK(ged_exec_bo(gedp, 3, bo_missing_output_source_av) == BRLCAD_ERROR,
		"bo should reject an incomplete native export before opening its output file");
	CHECK(ged_exec_comb_std(gedp, 2, comb_std_missing_expr_av) == BRLCAD_ERROR,
		"comb_std should reject a native combination form without an expression or region-kind flag");
	CHECK(ged_exec_comb_std(gedp, 2, comb_std_help_av) == BRLCAD_OK,
		"comb_std should retain its native -? help form");
	CHECK(ged_exec_combmem(gedp, 2, combmem_get_av) == BRLCAD_OK,
		"combmem should preserve its native no-option query argv convention");
	CHECK(ged_exec_combmem(gedp, 4, combmem_bad_type_av) == BRLCAD_ERROR,
		"combmem should reject an out-of-range native representation type before editing a combination");
	CHECK(ged_exec_combmem(gedp, 7, combmem_bad_transform_av) == BRLCAD_ERROR,
		"combmem should reject an invalid native transform record before editing a combination");
	CHECK(ged_exec_exists(gedp, 3, exists_expression_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "1"),
		"exists should execute a native expression whose object exists");
	CHECK(ged_exec_exists(gedp, 3, exists_bad_expression_av) == BRLCAD_ERROR,
		"exists should retain its expression parser's malformed-grammar rejection");
	CHECK(ged_exec(gedp, 2, putmat_get_av) == BRLCAD_OK &&
		!BU_STR_EMPTY(bu_vls_cstr(gedp->ged_result_str)),
		"putmat should execute its native one-arc matrix retrieval form");
	CHECK(ged_exec(gedp, 3, putmat_identity_av) == BRLCAD_OK,
		"putmat should execute its native identity-matrix update form");
	CHECK(ged_exec(gedp, 3, putmat_packed_av) == BRLCAD_OK,
		"putmat should execute its native packed-matrix update form");
	CHECK(ged_exec(gedp, 3, putmat_bad_matrix_av) == BRLCAD_ERROR,
		"putmat should reject a nonfinite matrix before editing a combination");
	CHECK(ged_exec(gedp, 1, dup_missing_file_av) == GED_HELP,
		"dup should retain its native help result when no database file is supplied");
	CHECK(ged_exec(gedp, 2, dup_file_av) == BRLCAD_OK,
		"dup should execute its native required-file form without a prefix");
	CHECK(ged_exec(gedp, 3, dup_prefix_av) == BRLCAD_OK,
		"dup should execute its native optional-prefix form");
	CHECK(ged_exec(gedp, 4, dup_extra_operand_av) == BRLCAD_ERROR,
		"dup should reject an extra native operand before opening its input database");
	CHECK(ged_exec(gedp, 2, put_missing_type_av) == BRLCAD_ERROR,
		"put should reject an incomplete native object/type sequence before editing");
	CHECK(ged_exec(gedp, 3, put_unknown_type_av) == BRLCAD_ERROR,
		"put should reject an unknown native primitive type before editing");
	CHECK(ged_exec(gedp, 3, put_default_sphere_av) == BRLCAD_OK,
		"put should execute its native new-object and primitive-type form");
	CHECK(ged_exec(gedp, 7, put_comb_valid_av) == BRLCAD_OK,
		"put_comb should execute its native schema using a quoted spaced RGB color");
	CHECK(ged_exec(gedp, 8, put_comb_missing_region_fields_av) == BRLCAD_ERROR,
		"put_comb should reject an incomplete region tail before editing the database");
	CHECK(ged_exec(gedp, 8, put_comb_nonregion_fields_av) == BRLCAD_ERROR,
		"put_comb should reject region fields on a non-region combination");
	CHECK(ged_exec(gedp, 7, put_comb_bad_color_av) == BRLCAD_ERROR,
		"put_comb should reject an out-of-range RGB color before editing the database");
	CHECK(ged_exec(gedp, 5, pipe_append_bad_point_av) == BRLCAD_ERROR,
		"pipe_append_pnt should reject separate coordinates before importing a pipe");
	CHECK(ged_exec(gedp, 3, pipe_delete_bad_index_av) == BRLCAD_ERROR,
		"pipe_delete_pnt should reject a malformed index before importing a pipe");
	CHECK(ged_exec(gedp, 17, rfarb_bad_coordinate_av) == BRLCAD_ERROR,
		"rfarb should reject an invalid native coordinate selector before creating an ARB");
	CHECK(ged_exec(gedp, 17, rfarb_zero_thickness_av) == BRLCAD_ERROR,
		"rfarb should reject zero native thickness before creating an ARB");
	CHECK(ged_exec(gedp, 17, rfarb_valid_av) == BRLCAD_OK,
		"rfarb should accept its full sixteen-operand native creation form");
	CHECK(ged_exec(gedp, 3, savekey_bad_time_av) == BRLCAD_ERROR,
		"savekey should reject an invalid native time before writing a keyframe");
	CHECK(ged_exec(gedp, 3, savekey_valid_av) == BRLCAD_OK,
		"savekey should execute its native file and finite-time form");
	CHECK(std::remove(savekey_valid_av[1]) == 0,
		"savekey test output should be removable");
	CHECK(ged_exec(gedp, 3, importfg4_bad_section_av) == BRLCAD_ERROR,
		"importFg4Section should reject an invalid native section before importing");
	CHECK(ged_exec(gedp, 4, mrot_bad_angle_av) == BRLCAD_ERROR,
		"mrot should reject a nonnumeric native angle before rotating the view");
	CHECK(ged_exec(gedp, 4, mrot_angles_av) == BRLCAD_OK,
		"mrot should execute its native three-angle form");
	CHECK(ged_exec(gedp, 2, mrot_packed_av) == BRLCAD_OK,
		"mrot should retain its native packed-vector form");
	CHECK(ged_exec(gedp, 2, arced_missing_animation_av) == BRLCAD_ERROR,
		"arced should reject a missing native animation command before database edits");
	CHECK(ged_exec(gedp, 8, arced_bad_path_av) == BRLCAD_ERROR,
		"arced should reject a path without a native parent/member separator before database edits");
	CHECK(ged_exec(gedp, 2, tol_query_abs_av) == BRLCAD_OK,
		"tol should retain its native one-keyword query form");
	CHECK(ged_exec(gedp, 3, tol_bad_number_av) == BRLCAD_ERROR,
		"tol should reject a partially numeric tolerance before changing settings");
	CHECK(ged_exec(gedp, 3, slew_bad_coordinate_av) == BRLCAD_ERROR,
		"slew should reject a partially numeric coordinate before changing the view");
	CHECK(ged_exec(gedp, 2, slew_packed_vector_av) == BRLCAD_OK,
		"slew should retain its native packed two-coordinate form");
	CHECK(ged_exec(gedp, 4, sv_vector_av) == BRLCAD_OK,
		"sv should execute the native three-coordinate slew alias form");
	CHECK(ged_exec(gedp, 4, rot_bad_angle_av) == BRLCAD_ERROR,
		"rot should reject a partially numeric angle before changing the view");
	CHECK(ged_exec(gedp, 2, rot_packed_vector_av) == BRLCAD_OK,
		"rot should retain its native packed three-angle form");
	CHECK(ged_exec(gedp, 1, rot_about_query_av) == BRLCAD_OK,
		"rot_about should retain its native no-argument query form");
	CHECK(ged_exec(gedp, 2, rot_about_bad_av) == BRLCAD_ERROR,
		"rot_about should reject an invalid native pivot before changing the view");
	CHECK(ged_exec(gedp, 4, tra_bad_component_av) == BRLCAD_ERROR,
		"tra should reject a partially numeric component before changing the view");
	CHECK(ged_exec(gedp, 3, edmater_missing_combination_av) == BRLCAD_ERROR,
		"edmater should reject a native editor option without creating a temporary file");
	CHECK(ged_exec(gedp, 3, editit_missing_file_av) == BRLCAD_ERROR,
		"editit should reject a native edit-string option without launching an editor");
	CHECK(ged_exec(gedp, 2, editit_help_av) == BRLCAD_OK,
		"editit should retain its native help result without launching an editor");
	CHECK(ged_exec(gedp, 3, red_missing_combination_av) == BRLCAD_ERROR,
		"red should reject a native editor option without creating an edit file");
	CHECK(ged_exec(gedp, 4, move_arb_edge_bad_index_av) == BRLCAD_ERROR,
		"move_arb_edge should reject a native edge index before importing an ARB");
	CHECK(ged_exec(gedp, 3, bev_missing_expression_av) == BRLCAD_ERROR,
		"bev should reject a native output name without evaluating an expression");
	gedp->new_cmd_forms = 1;
	CHECK(ged_exec(gedp, 2, zap_missing_view_av) == BRLCAD_ERROR,
		"new-form zap should reject a native view option without its argument");
	CHECK(ged_exec(gedp, 2, zap_help_av) == GED_HELP,
		"new-form zap should retain its native help result without clearing the display");
	gedp->new_cmd_forms = 0;
	CHECK(ged_exec(gedp, 2, illum_missing_object_av) == BRLCAD_ERROR,
		"illum should reject a native disable flag without an object before display edits");
	CHECK(ged_exec(gedp, 1, labelvert_help_av) == GED_HELP,
		"labelvert should retain its native help result when no object is supplied");
	CHECK(ged_exec(gedp, 1, inside_prompt_av) == GED_MORE,
		"inside should retain its native interactive prompt when no outside primitive is supplied");
	CHECK(ged_exec_pathsum(gedp, 2, pathsum_slash_path_av) == BRLCAD_OK &&
		BU_STR_EQUAL(pathsum_slash_path_av[1], "all.g/tor.r"),
		"pathsum should execute a native slash path without tokenizing caller-owned argv text");
	CHECK(ged_exec_pathsum(gedp, 2, pathsum_missing_path_av) == BRLCAD_ERROR,
		"pathsum should reject a native end-of-options marker without a path");
	CHECK(ged_exec_listeval(gedp, 3, listeval_terse_path_av) == BRLCAD_OK,
		"listeval should execute its native terse option and path operand");
	CHECK(ged_exec_paths(gedp, 2, paths_path_av) == BRLCAD_OK,
		"paths should execute its native path operand through the shared implementation");
	CHECK(ged_exec_erase(gedp, 3, erase_legacy_invalid_av) == BRLCAD_ERROR,
		"legacy erase should reject -o without native attribute selection");
	gedp->new_cmd_forms = 1;
	CHECK(ged_exec_erase(gedp, 4, erase_new_invalid_mode_av) == BRLCAD_ERROR,
		"new-form erase should reject a non-numeric native drawing mode before display edits");
	gedp->new_cmd_forms = 0;
	CHECK(ged_exec_heal(gedp, 3, heal_bad_av) == BRLCAD_ERROR,
		"heal should reject a non-numeric native tolerance before editing a BOT");
	CHECK(ged_exec_ae2dir(gedp, 4, ae2dir_av) == BRLCAD_OK,
		"ae2dir should execute its native inverse flag and number operands");
	CHECK(ged_exec_ae2dir(gedp, 3, ae2dir_bad_av) == BRLCAD_ERROR,
		"ae2dir should reject a non-numeric native angle operand");
	CHECK(ged_exec_dir2ae(gedp, 5, dir2ae_av) == BRLCAD_OK,
		"dir2ae should execute its native inverse flag and number operands");
	CHECK(ged_exec_pset(gedp, 4, pset_bad_av) == BRLCAD_ERROR,
		"pset should reject a non-numeric native value before editing a primitive");
	CHECK(ged_exec_otranslate(gedp, 5, otranslate_bad_av) == BRLCAD_ERROR,
		"otranslate should reject a non-numeric native translation before editing an object");
	CHECK(ged_exec_lt(gedp, 4, lt_av) == BRLCAD_OK,
		"lt should execute its native separator option and object operand");
	CHECK(ged_exec_push(gedp, 4, push_bad_av) == BRLCAD_ERROR,
		"push should reject a non-integer processor option before editing an object tree");
	CHECK(ged_exec_pull(gedp, 3, pull_bad_av) == BRLCAD_ERROR,
		"pull should reject an unknown native option before editing an object tree");
	CHECK(ged_exec_kill(gedp, 3, kill_no_delete_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "{all.g } {}"),
		"kill should execute its native no-delete mode without modifying the database");
	CHECK(ged_exec_keep(gedp, 3, keep_bad_av) == BRLCAD_ERROR,
		"keep should reject an incomplete native operand sequence before creating an output database");
	CHECK(ged_exec_killtree(gedp, 3, killtree_no_delete_av) == BRLCAD_OK,
		"killtree should execute its native no-delete mode without modifying the database");
	CHECK(ged_exec_killtree(gedp, 3, killtree_bad_av) == BRLCAD_ERROR,
		"killtree should reject an unknown native option before tree deletion begins");
	CHECK(ged_exec_edcodes(gedp, 3, edcodes_names_av) == BRLCAD_OK,
		"edcodes should execute its native list-only mode without launching an editor");
	CHECK(ged_exec_edcodes(gedp, 4, edcodes_bad_av) == BRLCAD_ERROR,
		"edcodes should reject mutually exclusive native mode flags");
	CHECK(ged_exec_move_all(gedp, 4, move_all_no_change_av) == BRLCAD_OK,
		"move_all should execute its native no-change form without renaming database objects");
	CHECK(ged_exec_move_all(gedp, 5, move_all_bad_av) == BRLCAD_ERROR,
		"move_all should reject mixed mapping-file and object forms before opening a mapping file");
	CHECK(ged_exec_gdiff(gedp, 4, gdiff_interspersed_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "0"),
		"gdiff should execute an interspersed native option after its first operand");
	CHECK(ged_exec_gdiff(gedp, 5, gdiff_bad_number_av) == BRLCAD_ERROR,
		"gdiff should reject an invalid native numeric option before comparing objects");
	CHECK(ged_exec_concat(gedp, 3, concat_help_interspersed_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: concat") != NULL,
		"concat should execute an interspersed native help option without opening its input database");
	CHECK(ged_exec_dbconcat(gedp, 2, dbconcat_help_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: dbconcat") != NULL,
		"dbconcat should use the matching native command schema for help");
	CHECK(ged_exec_concat(gedp, 4, concat_conflicting_affix_av) == BRLCAD_ERROR &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "-p and -s are mutually exclusive") != NULL,
		"concat should reject documented mutually exclusive affix modes before opening its input database");
	CHECK(ged_exec_summary(gedp, 3, summary_help_interspersed_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: summary") != NULL,
		"summary should execute an interspersed native help option");
	CHECK(ged_exec_summary(gedp, 2, summary_legacy_types_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Summary:") != NULL,
		"summary should retain its documented legacy p/r/g selector");
	CHECK(ged_exec_summary(gedp, 3, summary_object_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Summary (all.g):") != NULL,
		"summary should execute a native database-object option");
	CHECK(ged_exec_summary(gedp, 4, summary_conflicting_object_av) == BRLCAD_ERROR &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "object specified both by --obj") != NULL,
		"summary should reject object option and operand conflicts before lookup");
	CHECK(ged_exec_whichid(gedp, 3, whichid_help_interspersed_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: whichid") != NULL,
		"whichid should execute an interspersed native help option");
	CHECK(ged_exec_whichid(gedp, 2, whichid_range_av) == BRLCAD_OK,
		"whichid should execute a native colon-delimited range");
	CHECK(ged_exec_whichid(gedp, 2, whichid_bad_range_av) == BRLCAD_ERROR,
		"whichid should reject a malformed native range before traversal");
	CHECK(ged_exec_whichair(gedp, 4, whichair_root_av) == BRLCAD_OK,
		"whichair should execute its native root option and range operand");
	CHECK(ged_exec_rm(gedp, 3, rm_help_interspersed_av) == GED_HELP &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: rm") != NULL,
		"rm should execute an interspersed native help option");
	CHECK(ged_exec_rm(gedp, 4, rm_dry_run_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "all.g") != NULL,
		"rm should execute native force dry-run parsing without deleting its operand");
	CHECK(ged_exec_rm(gedp, 3, rm_bad_option_av) == BRLCAD_ERROR,
		"rm should reject unknown native options before resolving operands");
	CHECK(ged_exec_ls(gedp, 3, ls_help_interspersed_av) == BRLCAD_OK &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage:") != NULL,
		"ls should execute an interspersed native help option");
	CHECK(ged_exec_ls(gedp, 3, ls_short_primitives_av) == BRLCAD_OK,
		"ls should execute its documented -s primitives synonym");
	CHECK(ged_exec_ls(gedp, 4, ls_attribute_pairs_av) == BRLCAD_OK,
		"ls should execute complete native attribute name/value pairs");
	CHECK(ged_exec_ls(gedp, 4, ls_attribute_cluster_av) == BRLCAD_OK,
		"ls should recognize attribute mode in a compact native flag cluster");
	CHECK(ged_exec_ls(gedp, 3, ls_bad_attribute_pairs_av) == BRLCAD_ERROR &&
		std::strstr(bu_vls_cstr(gedp->ged_result_str), "attribute value required") != NULL,
		"ls should reject an incomplete native attribute pair before lookup");
    }

    CHECK(ged_cmd_validate(gedp, "tops -ah", std::strlen("tops -ah"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"tops schema should validate a compact short-flag cluster");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "tops -az", std::strlen("tops -az"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tops schema should reject a compact cluster containing an unknown flag");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "cat ", std::strlen("cat "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"cat schema should require at least one database object");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "get tor r_a", std::strlen("get tor r_a"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"get schema should accept object and optional property operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "get tor r_a extra", std::strlen("get tor r_a extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"get schema should reject extra operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "find -a", std::strlen("find -a"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"find schema should require an object after its native flag");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "glob l t*", std::strlen("glob l t*"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"glob schema should reject an unquoted second command-string operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "list -t", std::strlen("list -t"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"list schema should require an object after native flags");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "ls --attributes region", std::strlen("ls --attributes region"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"ls schema should require an attribute value after an attribute name");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "ls --attributes region yes", std::strlen("ls --attributes region yes"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"ls schema should accept complete attribute name/value pairs");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp, "ls all.g", &analysis) == 0,
	"ls object analysis should succeed");
    token = token_matching(&analysis, "ls all.g", "all.g");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND &&
	BU_STR_EQUAL(token->validator, "ged.db_path_or_pattern"),
	"ls object analysis should retain database-path semantics");
    ged_cmd_analysis_clear(&analysis);
    CHECK(ged_cmd_analyze(gedp, "ls --attributes region yes", &analysis) == 0,
	"ls attribute-pair analysis should succeed");
    token = token_matching(&analysis, "ls --attributes region yes", "region");
    CHECK(token && token->role == GED_CMD_TOKEN_OPERAND && token->validator == NULL &&
	token->value_type == BU_CMD_VALUE_STRING,
	"ls attribute names should not be analyzed as database paths");
    ged_cmd_analysis_clear(&analysis);
    CHECK(ged_cmd_validate(gedp, "showmats -a all.g/tor.r", std::strlen("showmats -a all.g/tor.r"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"showmats should validate its native flag and database-path operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "showmats -b all.g/tor.r", std::strlen("showmats -b all.g/tor.r"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"showmats should reject an undocumented native flag");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "wcodes output.txt all.g", std::strlen("wcodes output.txt all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"wcodes should validate native file and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "wcodes output.txt", std::strlen("wcodes output.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"wcodes should require at least one database-object operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "cc new_constraint expression", std::strlen("cc new_constraint expression"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"cc should validate its native new-name and expression operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "cc new_constraint", std::strlen("cc new_constraint"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"cc should require its constraint expression operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "cpi tor.r new.tgc", std::strlen("cpi tor.r new.tgc"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"cpi should distinguish its existing source and new destination operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "cpi tor.r new.tgc extra", std::strlen("cpi tor.r new.tgc extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"cpi should reject an extra native operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "decompose tor.r output", std::strlen("decompose tor.r output"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"decompose should validate its native source and optional prefix operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "fracture tor.r output", std::strlen("fracture tor.r output"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"fracture should validate its native source and optional prefix operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "heal tor.r 0.5", std::strlen("heal tor.r 0.5"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"heal should validate a native numeric zipper tolerance");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "heal tor.r not-a-number", std::strlen("heal tor.r not-a-number"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"heal should reject a non-numeric zipper tolerance");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "heal tor.r 0.5 extra", std::strlen("heal tor.r 0.5 extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"heal should reject an extra native operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "ae2dir -i 0 90", std::strlen("ae2dir -i 0 90"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"ae2dir should validate native flag and numeric angle operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "ae2dir 0 nope", std::strlen("ae2dir 0 nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"ae2dir should reject a non-numeric angle");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dir2ae 1 0 nope", std::strlen("dir2ae 1 0 nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dir2ae should reject a non-numeric direction component");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "shaded_mode 1", std::strlen("shaded_mode 1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"shaded_mode should validate a documented native keyword");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "shaded_mode 3", std::strlen("shaded_mode 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"shaded_mode should reject an unsupported native keyword");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_transparency all.g 0.5", std::strlen("set_transparency all.g 0.5"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"set_transparency should validate native path and numeric operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_transparency all.g nope", std::strlen("set_transparency all.g nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"set_transparency should reject a non-numeric transparency value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pset all.g/tor.r m nope", std::strlen("pset all.g/tor.r m nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"pset should reject a non-numeric native primitive value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "otranslate all.g/tor.r 0 nope 0", std::strlen("otranslate all.g/tor.r 0 nope 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"otranslate should reject a non-numeric translation component");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lt -c , all.g", std::strlen("lt -c , all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"lt should validate its native one-character separator and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lt -c :: all.g", std::strlen("lt -c :: all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lt should reject a multi-character separator");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "tree -vv all.g", std::strlen("tree -vv all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"tree should validate a native repeatable compact flag cluster");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "tree -d -1 all.g", std::strlen("tree -d -1 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"tree should reject a negative depth during native validation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "copy tor new_tor", std::strlen("copy tor new_tor"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"copy should validate its existing source and new-name destination roles");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "copy tor", std::strlen("copy tor"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"copy should require its destination name");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "move tor invalid/destination", std::strlen("move tor invalid/destination"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"move should reject a slash in its native destination-name role");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "group new_group tor", std::strlen("group new_group tor"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"group should validate its new-name and repeated existing-member roles");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "group new_group", std::strlen("group new_group"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"group should require at least one member object");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "push -P 2 all.g", std::strlen("push -P 2 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"push should validate native integer and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "push -P nope all.g", std::strlen("push -P nope all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"push should reject a non-integer processor option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pull -d all.g", std::strlen("pull -d all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"pull should validate native debug and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "keep -R output.g tor.r", std::strlen("keep -R output.g tor.r"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"keep should validate its native flag, output file, and object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "keep -R output.g", std::strlen("keep -R output.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"keep should require at least one object after its output file");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "killtree -an all.g", std::strlen("killtree -an all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"killtree should validate a native compact flag cluster and object operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "killtree -x all.g", std::strlen("killtree -x all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"killtree should reject an unknown native option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "edcodes -i all.g", std::strlen("edcodes -i all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"edcodes should validate a native sort mode and object operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "edcodes -i -r all.g", std::strlen("edcodes -i -r all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"edcodes should reject mutually exclusive native modes during validation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "edcodes -i", std::strlen("edcodes -i"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"edcodes should require an object when not in list-only mode");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "how -b all.g", std::strlen("how -b all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"how should expose its native flag and database-path operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "how all.g -b", std::strlen("how all.g -b"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"how should reject an option after its object operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "get_eyemodel trailing", std::strlen("get_eyemodel trailing"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"get_eyemodel should reject operands through its native schema");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "get_type all.g", std::strlen("get_type all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"get_type should expose its native database-path operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "whatid all.g extra", std::strlen("whatid all.g extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"whatid should enforce its native single-operand rule");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "get_comb all.g", std::strlen("get_comb all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"get_comb should expose its native database-object operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_uplotOutputMode binary", std::strlen("set_uplotOutputMode binary"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"set_uplotOutputMode should validate its native keyword operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_uplotOutputMode invalid", std::strlen("set_uplotOutputMode invalid"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"set_uplotOutputMode should reject an undocumented native keyword");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "echo -literal", std::strlen("echo -literal"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"optionless echo should retain a leading-dash raw operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "title -literal", std::strlen("title -literal"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"optionless title should retain leading-dash title text");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_output_script -literal", std::strlen("set_output_script -literal"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"optionless script setter should retain a leading-dash string operand");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "set_output_script one two", std::strlen("set_output_script one two"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"set_output_script should enforce its native optional single-operand rule");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *echo_av[] = {"echo", "-literal"};
	const char *echo_marker_av[] = {"echo", "--", "-literal"};
	CHECK(ged_exec_echo(gedp, 2, echo_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "-literal\n"),
	    "echo execution should preserve a leading-dash raw operand");
	CHECK(ged_exec_echo(gedp, 3, echo_marker_av) == BRLCAD_OK &&
		BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "-literal\n"),
	    "echo execution should consume the end-of-options marker before raw text");
    }

    CHECK(analyze_operand(gedp, &analysis, "draw all.", GED_CMD_SEMANTIC_INVALID), "draw all. should be invalid");
    CHECK(analyze_operand(gedp, &analysis, "draw all.g", GED_CMD_SEMANTIC_VALID), "draw all.g should be valid");
    CHECK(analyze_operand(gedp, &analysis, "draw all.g/", GED_CMD_SEMANTIC_INVALID), "draw all.g/ should be invalid");
    CHECK(analyze_operand(gedp, &analysis, "draw all.g/e", GED_CMD_SEMANTIC_INVALID), "draw all.g/e should be invalid");
    CHECK(analyze_operand(gedp, &analysis, "draw all.g/t", GED_CMD_SEMANTIC_INVALID), "draw all.g/t should be invalid before completion");

    argv_completion_count = ged_cmd_complete(&completions, &prefix, gedp, "draw all.g/t", std::strlen("draw all.g/t"));
    CHECK(argv_completion_count > 0, "completion should find candidates for draw all.g/t");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&prefix), "t"), "completion prefix should identify the editable path component");
    completion = find_completion(completions, argv_completion_count, "tor.r");
    CHECK(completion != NULL, "completion should offer tor.r");

    completion_count = ged_cmd_complete_result(gedp, "draw all.g/t", std::strlen("draw all.g/t"), &completion_result);
    CHECK(completion_count > 0, "completion result should find candidates for draw all.g/t");
    CHECK(completion_result.prefix && BU_STR_EQUAL(completion_result.prefix, "t"), "completion result prefix should identify path component");
    CHECK(completion_result.replacement_start == std::strlen("draw all.g/"), "completion result replacement start should isolate path component");
    CHECK(completion_result.replacement_end == std::strlen("draw all.g/t"), "completion result replacement end should be the cursor");
    completion = find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r");
    CHECK(completion != NULL, "completion result should offer tor.r");

    completed_input = "draw all.g/t";
    completed_input.replace(completion_result.replacement_start,
	    completion_result.replacement_end - completion_result.replacement_start, completion);
    CHECK(analyze_operand(gedp, &analysis, completed_input.c_str(), GED_CMD_SEMANTIC_VALID),
	    "completed draw path should be semantically valid");
    ged_cmd_completion_result_clear(&completion_result);

    /* tire is the first compact native schema.  Verify that it participates
     * in the same registry, JSON, completion, validation, and highlighting
     * contracts as legacy forms without carrying descriptor rows. */
    CHECK(ged_cmd_schema_exists("tire"), "native tire schema is unavailable");
    schema_json = ged_cmd_schema_json("tire");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"tire\"") &&
	std::strstr(schema_json, "\"long\":\"width\"") &&
	std::strstr(schema_json, "\"type\":\"number\""),
	"native tire schema JSON should publish typed option metadata");
    bu_free(schema_json, "tire schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "tire --wi", std::strlen("tire --wi"), &completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "--width") != NULL,
	"native tire completion should offer --width");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "tire --width nope", std::strlen("tire --width nope"), &validation) == 0,
	"native tire validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"native tire should reject a non-numeric --width argument");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "tire --tread-shape 3", std::strlen("tire --tread-shape 3"), &validation) == 0,
	"native tire constrained-integer validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"native tire should reject an out-of-range --tread-shape value");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "tire --width 215", &analysis) == 0,
	"native tire analysis should succeed");
    {
	const struct ged_cmd_analysis_token *width_arg = token_matching(&analysis, "tire --width 215", "215");
	CHECK(width_arg && width_arg->role == GED_CMD_TOKEN_OPTION_ARG &&
	    width_arg->value_type == BU_CMD_VALUE_NUMBER && width_arg->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native tire width argument should be a valid number");
    }
    ged_cmd_analysis_clear(&analysis);

    schema_json = ged_cmd_schema_json("ps");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"ps\"") &&
	std::strstr(schema_json, "\"argument_shape\":{\"kind\":\"rgb\"") &&
	std::strstr(schema_json, "\"min_tokens\":1") &&
	std::strstr(schema_json, "\"max_tokens\":3"),
	"ps should publish the standard native RGB border-color shape");
    bu_free(schema_json, "ps schema json");
    schema_json = NULL;

    CHECK(ged_cmd_schema_exists("png") && ged_cmd_schema_exists("pngwf"),
	"png and pngwf should publish native schemas");
    schema_json = ged_cmd_schema_json("png");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"png\"") &&
	std::strstr(schema_json, "\"argument_shape\":{\"kind\":\"rgb\"") &&
	std::strstr(schema_json, "\"name\":\"output_file\""),
	"png should publish its RGB background shape and output file role");
    bu_free(schema_json, "png schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "png -c 1;2;3 -s 50 image.png", std::strlen("png -c 1;2;3 -s 50 image.png"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"png should accept a packed RGB background and minimum legal size");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "png -c 1 2 3 image.png", std::strlen("png -c 1 2 3 image.png"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"png should accept three separate RGB background channels");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "png -s 49 image.png", std::strlen("png -s 49 image.png"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"png should reject an undersized output image");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("voxelize"), "voxelize should publish a native schema");
    schema_json = ged_cmd_schema_json("voxelize");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"voxelize\"") &&
	std::strstr(schema_json, "\"argument_shape\":{\"kind\":\"vector3\"") &&
	std::strstr(schema_json, "\"name\":\"output_object\"") &&
	std::strstr(schema_json, "\"type\":\"string\""),
	"voxelize should publish a finite vector shape and new output-name role");
    bu_free(schema_json, "voxelize schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "voxelize -s 1/2/3 -d 2 -t 0.3 vox all.g",
	std::strlen("voxelize -s 1/2/3 -d 2 -t 0.3 vox all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"voxelize should accept a packed positive voxel size and bounded threshold");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "voxelize -s 1 2", std::strlen("voxelize -s 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"voxelize should classify a partial separate voxel size as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "voxelize -s 1/-2/3 vox all.g",
	std::strlen("voxelize -s 1/-2/3 vox all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"voxelize should reject a nonpositive voxel dimension");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "voxelize -d 0 vox all.g", std::strlen("voxelize -d 0 vox all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"voxelize should reject a nonpositive detail level before its asserted implementation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "voxelize -t 1.1 vox all.g", std::strlen("voxelize -t 1.1 vox all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"voxelize should reject an out-of-range fill threshold");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("mirror"), "mirror should publish a native schema");
    schema_json = ged_cmd_schema_json("mirror");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"mirror\"") &&
	std::strstr(schema_json, "\"argument_shape\":{\"kind\":\"vector3\"") &&
	std::strstr(schema_json, "\"name\":\"output_object\"") &&
	std::strstr(schema_json, "\"type\":\"string\""),
	"mirror should publish its finite vector shape and new output-name role");
    bu_free(schema_json, "mirror schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "mirror -p 1/2/3 -d 0,1,0 -o 3 all.g mirrored",
	std::strlen("mirror -p 1/2/3 -d 0,1,0 -o 3 all.g mirrored"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"mirror should accept packed point and nonzero direction vectors");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mirror -d 1 2", std::strlen("mirror -d 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"mirror should classify a partial separate direction as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mirror -d 0/0/0 all.g mirrored",
	std::strlen("mirror -d 0/0/0 all.g mirrored"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"mirror should reject a zero plane normal before normalization");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mirror all.g mirrored Y", std::strlen("mirror all.g mirrored Y"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"mirror should retain its uppercase trailing compatibility axis selector");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mirror -?", std::strlen("mirror -?"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"mirror help should be valid without object operands");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("make"), "make should publish a native schema");
    schema_json = ged_cmd_schema_json("make");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"make\"") &&
	std::strstr(schema_json, "\"argument_shape\":{\"kind\":\"vector3\"") &&
	std::strstr(schema_json, "\"name\":\"primitive_type\"") &&
	std::strstr(schema_json, "\"type\":\"keyword\""),
	"make should publish its finite origin vector and primitive-type role");
    bu_free(schema_json, "make schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "make -o 1/2/3 -s 2 made.s sph",
	std::strlen("make -o 1/2/3 -s 2 made.s sph"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"make should accept a packed finite origin and scalar scale");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "make -o 1 2", std::strlen("make -o 1 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"make should classify a partial separate origin as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "make -s nan made.s sph", std::strlen("make -s nan made.s sph"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"make should reject a nonfinite scale");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "make -t", std::strlen("make -t"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"make type listing should be valid without object operands");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "make -t made.s sph", std::strlen("make -t made.s sph"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"make type listing should reject construction operands");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("man"), "man should publish a native schema");
    schema_json = ged_cmd_schema_json("man");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"man\"") &&
	std::strstr(schema_json, "\"long\":\"language\"") &&
	std::strstr(schema_json, "\"type\":\"vls\"") &&
	std::strstr(schema_json, "\"long\":\"section\"") &&
	std::strstr(schema_json, "\"type\":\"char\""),
	"man should publish native language and manual-section option types");
    bu_free(schema_json, "man schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "man --language en --section n view",
	std::strlen("man --language en --section n view"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"man should accept native ISO language and manual-section options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "man --language EN view",
	std::strlen("man --language EN view"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"man should reject a non-ISO language before launching the viewer");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "man --section 4 view",
	std::strlen("man --section 4 view"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"man should reject an unsupported manual section");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *man_help_argv[] = {"man", "--help"};
	CHECK(ged_exec(gedp, 2, man_help_argv) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: man") != NULL,
	    "man should generate native help without requiring the external viewer");
    }

    CHECK(ged_cmd_schema_exists("opendb") && ged_cmd_schema_exists("reopen"),
	"open command aliases should publish native schemas");
    schema_json = ged_cmd_schema_json("opendb");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"opendb\"") &&
	std::strstr(schema_json, "\"long\":\"open\"") &&
	std::strstr(schema_json, "\"name\":\"filename\"") &&
	std::strstr(schema_json, "\"type\":\"file\""),
	"opendb should publish its native open-only flag and file role");
    bu_free(schema_json, "opendb schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "opendb --open", std::strlen("opendb --open"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"opendb should require a filename after its native existing-file flag");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *open_help_argv[] = {"open", "--help"};
	CHECK(ged_exec(gedp, 2, open_help_argv) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: open") != NULL,
	    "open should generate native help without a filename");
    }
    {
	const char *reopen_argv[] = {"reopen", av[1]};
	CHECK(ged_exec(gedp, 2, reopen_argv) == BRLCAD_OK,
	    "reopen should execute its native existing-file form");
    }

    CHECK(ged_cmd_schema_exists("coil"), "coil should publish a native schema");
    schema_json = ged_cmd_schema_json("coil");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"coil\"") &&
	std::strstr(schema_json, "\"type\":\"number\"") &&
	std::strstr(schema_json, "\"short\":\"S\"") &&
	std::strstr(schema_json, "\"name\":\"output_object\"") &&
	std::strstr(schema_json, "\"type\":\"string\""),
	"coil should publish typed scalar options, its section form, and a new-name role");
    bu_free(schema_json, "coil schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "coil -d -1", std::strlen("coil -d -1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"coil should reject a negative diameter before creating geometry");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "coil -S 10,1000,50,60,800,1 stacked_coil",
	std::strlen("coil -S 10,1000,50,60,800,1 stacked_coil"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"coil should accept its native comma-separated section form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "coil -s 4", std::strlen("coil -s 4"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"coil should reject an unsupported cap type");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *coil_help_argv[] = {"coil", "-h"};
	CHECK(ged_exec(gedp, 2, coil_help_argv) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: coil") != NULL,
	    "coil should generate native help without creating geometry");
    }

    CHECK(ged_cmd_schema_exists("lc"), "lc should publish a native schema");
    schema_json = ged_cmd_schema_json("lc");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"lc\"") &&
	std::strstr(schema_json, "\"short\":\"0\"") &&
	std::strstr(schema_json, "\"short\":\"5\"") &&
	std::strstr(schema_json, "\"name\":\"group\"") &&
	std::strstr(schema_json, "\"type\":\"db_object\""),
	"lc should publish its native sort flags and group role");
    bu_free(schema_json, "lc schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "lc -1 -2 all.g", std::strlen("lc -1 -2 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lc should reject selecting more than one sort column");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lc -f first.out -f second.out all.g",
	std::strlen("lc -f first.out -f second.out all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lc should reject a repeated output-file option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lc -6 all.g", std::strlen("lc -6 all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lc should reject its undocumented and unsupported -6 option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lc -f output.txt all.g", std::strlen("lc -f output.txt all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"lc should accept one native output-file option and group");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *lc_help_argv[] = {"lc"};
	CHECK(ged_exec(gedp, 1, lc_help_argv) == GED_HELP &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "Usage: lc") != NULL,
	    "lc should generate native help without a group");
    }

    CHECK(ged_cmd_schema_exists("lod"), "lod should publish a native schema");
    schema_json = ged_cmd_schema_json("lod");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"lod\"") &&
	std::strstr(schema_json, "\"name\":\"cache\"") &&
	std::strstr(schema_json, "bot_threshold"),
	"lod should publish the same native action grammar as view lod");
    bu_free(schema_json, "lod schema json");
    schema_json = NULL;
    completion_count = ged_cmd_complete_result(gedp, "lod ca", std::strlen("lod ca"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "cache") != NULL,
	"lod should complete its shared native action grammar");
    ged_cmd_completion_result_clear(&completion_result);
    completion_count = ged_cmd_complete_result(gedp, "lod cache ", std::strlen("lod cache "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "clear") != NULL &&
	find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "exists") != NULL,
	"lod cache should complete its shared native cache actions");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "lod point_scale nope", std::strlen("lod point_scale nope"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lod should reject a nonnumeric point scale");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lod bot_threshold -1", std::strlen("lod bot_threshold -1"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lod should reject a negative BoT threshold");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "lod 1 extra", std::strlen("lod 1 extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"lod enable/disable actions should reject extra operands");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("overlay"), "overlay should publish a native schema");
    schema_json = ged_cmd_schema_json("overlay");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"overlay\"") &&
	std::strstr(schema_json, "\"long\":\"format\"") &&
	std::strstr(schema_json, "\"name\":\"input_file\"") &&
	std::strstr(schema_json, "\"type\":\"file\""),
	"overlay should publish native typed options and its input file role");
    bu_free(schema_json, "overlay schema json");
    schema_json = NULL;
    completion_count = ged_cmd_complete_result(gedp, "overlay --fo", std::strlen("overlay --fo"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--format") != NULL,
	"overlay should complete its native MIME-format option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "overlay -F --format pix -w 10 -n 10 image.pix",
	std::strlen("overlay -F --format pix -w 10 -n 10 image.pix"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"overlay should accept its native framebuffer image form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "overlay -F --format unknown image.data",
	std::strlen("overlay -F --format unknown image.data"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"overlay should reject an unknown MIME-format argument");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "overlay --width nope image.pix",
	std::strlen("overlay --width nope image.pix"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"overlay should reject a noninteger image width");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "overlay ", std::strlen("overlay "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"overlay should require an input file");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "overlay --help", std::strlen("overlay --help"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"overlay help should be valid without an input file");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("stat"), "stat should publish a native schema");
    schema_json = ged_cmd_schema_json("stat");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"stat\"") &&
	std::strstr(schema_json, "\"long\":\"verbosity\"") &&
	std::strstr(schema_json, "\"long\":\"output-file\"") &&
	std::strstr(schema_json, "\"name\":\"patterns\""),
	"stat should publish native output, filter, and database-pattern metadata");
    bu_free(schema_json, "stat schema json");
    schema_json = NULL;
    completion_count = ged_cmd_complete_result(gedp, "stat --so", std::strlen("stat --so"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--sort-order") != NULL,
	"stat should complete its native sort-order option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "stat -vv -q -C name,type -S !size all.g",
	std::strlen("stat -vv -q -C name,type -S !size all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"stat should accept its native interspersed option and object-pattern form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "stat --output-file report.txt all.g",
	std::strlen("stat --output-file report.txt all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"stat should expose its output-file option without consuming object patterns");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "stat ", std::strlen("stat "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"stat should require an object pattern outside help mode");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "stat --help", std::strlen("stat --help"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"stat help should be valid without an object pattern");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("garbage_collect"),
	"garbage_collect should publish a native schema");
    schema_json = ged_cmd_schema_json("garbage_collect");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"garbage_collect\"") &&
	std::strstr(schema_json, "\"long\":\"confirm\"") &&
	std::strstr(schema_json, "\"type\":\"flag\""),
	"garbage_collect should publish native confirmation metadata");
    bu_free(schema_json, "garbage_collect schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "garbage_collect ", std::strlen("garbage_collect "), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"garbage_collect should require confirm or help");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "garbage_collect --confirm", std::strlen("garbage_collect --confirm"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"garbage_collect should accept native confirmation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "garbage_collect --confirm extra", std::strlen("garbage_collect --confirm extra"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"garbage_collect should reject unexpected operands");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("3ptarb"), "3ptarb should publish a native schema");
    schema_json = ged_cmd_schema_json("3ptarb");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"3ptarb\"") &&
	std::strstr(schema_json, "\"name\":\"face_points\"") &&
	std::strstr(schema_json, "\"type\":\"number\"") &&
	std::strstr(schema_json, "\"name\":\"solved_coordinate\"") &&
	std::strstr(schema_json, "\"type\":\"string\""),
	"3ptarb should publish typed point and output-name roles");
    bu_free(schema_json, "3ptarb schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "3ptarb new_arb 0 0 0 1 0 0 1 1 0 z 0 1 3",
	std::strlen("3ptarb new_arb 0 0 0 1 0 0 1 1 0 z 0 1 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"3ptarb should accept its complete native positional form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "3ptarb new_arb 0 0 x",
	std::strlen("3ptarb new_arb 0 0 x"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"3ptarb should reject a nonnumeric face coordinate");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "3ptarb new_arb 0 0 0 1 0 0 1 1 0 q 0 1 3",
	std::strlen("3ptarb new_arb 0 0 0 1 0 0 1 1 0 q 0 1 3"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"3ptarb should reject an unsupported solved-coordinate selector");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("idents") && ged_cmd_schema_exists("regions") &&
	ged_cmd_schema_exists("solids"),
	"geometry table commands should publish native schemas");
    schema_json = ged_cmd_schema_json("solids");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"solids\"") &&
	std::strstr(schema_json, "\"name\":\"output_file\"") &&
	std::strstr(schema_json, "\"type\":\"file\"") &&
	std::strstr(schema_json, "\"name\":\"objects\"") &&
	std::strstr(schema_json, "\"type\":\"db_object\""),
	"geometry table schemas should publish output-file and object roles");
    bu_free(schema_json, "tables schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "idents report.txt all.g", std::strlen("idents report.txt all.g"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"idents should accept its native output-file and object form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "regions report.txt", std::strlen("regions report.txt"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"regions should require at least one object after its output file");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("rtcheck"), "rtcheck should publish a native raw-tail schema");
    schema_json = ged_cmd_schema_json("rtcheck");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"rtcheck\"") &&
	std::strstr(schema_json, "\"name\":\"arguments\"") &&
	std::strstr(schema_json, "\"type\":\"raw\""),
	"rtcheck should publish its delegated argument tail without pretending to parse rtcheck options");
    bu_free(schema_json, "rtcheck schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "rtcheck -g10 -G10", std::strlen("rtcheck -g10 -G10"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"rtcheck should preserve delegated options as a valid raw tail");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("solid_report") && ged_cmd_schema_exists("x"),
	"solid-report compatibility commands should publish native schemas");
    schema_json = ged_cmd_schema_json("solid_report");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"solid_report\"") &&
	std::strstr(schema_json, "\"long\":\"view\"") &&
	std::strstr(schema_json, "\"long\":\"mode\"") &&
	std::strstr(schema_json, "\"name\":\"detail_level\""),
	"solid_report should publish the native shared view, mode, and detail roles");
    bu_free(schema_json, "solid report schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "x --mode invalid", std::strlen("x --mode invalid"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"x should reject a noninteger drawing mode before reporting solids");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "x -2", std::strlen("x -2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"x should retain a negative numeric detail level without requiring --");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "solid_report --view main 2",
	std::strlen("solid_report --view main 2"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"solid_report should accept its native interspersed view and detail arguments");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("dsp"), "dsp should publish a native schema");
    schema_json = ged_cmd_schema_json("dsp");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"dsp\"") &&
	std::strstr(schema_json, "\"name\":\"command\"") &&
	std::strstr(schema_json, "\"xy\"") &&
	std::strstr(schema_json, "\"diff\"") &&
	std::strstr(schema_json, "\"name\":\"command_arguments\""),
	"dsp should publish its native command vocabulary and conditional argument tail");
    bu_free(schema_json, "dsp schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "dsp all.g xy 0 0", std::strlen("dsp all.g xy 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"dsp should accept complete nonnegative xy coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dsp all.g xy -1 0", std::strlen("dsp all.g xy -1 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dsp should reject a negative xy coordinate before database access");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dsp all.g diff all.g nan",
	std::strlen("dsp all.g diff all.g nan"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"dsp should reject a nonfinite difference threshold");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "dsp all.g diff", std::strlen("dsp all.g diff"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"dsp should require a comparison object for diff");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *dsp_partial_argv[] = {"dsp", "all.g", "xy", "0"};
	CHECK(ged_exec(gedp, 4, dsp_partial_argv) == BRLCAD_ERROR &&
	    std::strstr(bu_vls_cstr(gedp->ged_result_str), "DSP grid coordinate expected") != NULL,
	    "dsp execution should reject an incomplete xy form before reading argv out of bounds");
    }

    CHECK(ged_cmd_schema_exists("mat_ae") && ged_cmd_schema_exists("mat_mul") &&
	ged_cmd_schema_exists("mat4x3pnt") && ged_cmd_schema_exists("mat_scale_about_pnt"),
	"matrix utility commands should publish native schemas");
    schema_json = ged_cmd_schema_json("mat4x3pnt");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"mat4x3pnt\"") &&
	std::strstr(schema_json, "\"name\":\"matrix\"") &&
	std::strstr(schema_json, "\"type\":\"matrix\"") &&
	std::strstr(schema_json, "\"name\":\"point\"") &&
	std::strstr(schema_json, "\"type\":\"vector\""),
	"mat4x3pnt should publish its typed matrix and point roles");
    bu_free(schema_json, "matrix utility schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "mat_ae 35 20", std::strlen("mat_ae 35 20"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"mat_ae should accept finite azimuth and elevation values");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mat_ae 35 nan", std::strlen("mat_ae 35 nan"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"mat_ae should reject a nonfinite elevation before calculation");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "mat_mul invalid invalid", std::strlen("mat_mul invalid invalid"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"mat_mul should reject malformed packed matrices");
    ged_cmd_validate_result_clear(&validation);
    {
	const char *mat_ae_argv[] = {"mat_ae", "35", "20"};
	CHECK(ged_exec(gedp, 3, mat_ae_argv) == BRLCAD_OK &&
	    bu_vls_strlen(gedp->ged_result_str) > 0,
	    "mat_ae should execute its native finite-number form");
    }

    CHECK(ged_cmd_schema_exists("plot"), "plot should publish a native schema");
    schema_json = ged_cmd_schema_json("plot");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"plot\"") &&
	std::strstr(schema_json, "\"short\":\"f\"") &&
	std::strstr(schema_json, "\"short\":\"2\"") &&
	std::strstr(schema_json, "\"short\":\"3\"") &&
	std::strstr(schema_json, "\"name\":\"output\""),
	"plot should publish native output and rendering-mode options");
    bu_free(schema_json, "plot schema json");
    schema_json = NULL;
    CHECK(ged_cmd_validate(gedp, "plot -f output.plot", std::strlen("plot -f output.plot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"plot should accept native options before its output target");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "plot -2 -3 output.plot", std::strlen("plot -2 -3 output.plot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"plot should reject conflicting 2D and 3D options");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "plot -Z output.plot", std::strlen("plot -Z output.plot"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"plot should retain its uppercase Z-clipping alias");
    ged_cmd_validate_result_clear(&validation);

    /* The framebuffer transfer commands are native schemas too.  Their
     * execution needs a display manager, but syntax and completion must be
     * fully usable before one is available. */
    CHECK(ged_cmd_schema_exists("fb2pix") && ged_cmd_schema_exists("png2fb") &&
	ged_cmd_schema_exists("pix2fb") && ged_cmd_schema_exists("screengrab") &&
	ged_cmd_schema_exists("screen_grab"),
	"framebuffer transfer and capture commands should publish native schemas");
    schema_json = ged_cmd_schema_json("pix2fb");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"pix2fb\"") &&
	std::strstr(schema_json, "\"type\":\"integer\"") &&
	std::strstr(schema_json, "\"name\":\"input_file\""),
	"pix2fb should publish typed native options and its optional input file");
    bu_free(schema_json, "pix2fb schema json");
    schema_json = NULL;

    schema_json = ged_cmd_schema_json("screen_grab");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"screen_grab\"") &&
	std::strstr(schema_json, "\"long\":\"format\"") &&
	std::strstr(schema_json, "\"name\":\"output_file\""),
	"screen_grab should publish native capture options and its output file");
    bu_free(schema_json, "screen_grab schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "fb2pix -", std::strlen("fb2pix -"),
	&completion_result);
    CHECK(completion_count > 0 &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-s") != NULL,
	"native fb2pix completion should offer its square-size option");
    ged_cmd_completion_result_clear(&completion_result);


    completion_count = ged_cmd_complete_result(gedp, "screengrab --f", std::strlen("screengrab --f"),
	&completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "--format") != NULL,
	"screengrab should complete its native image-format option");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "screengrab capture.png -F",
	std::strlen("screengrab capture.png -F"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"screengrab should accept a native interspersed capture option");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "screen_grab --format no_such_image capture.img",
	std::strlen("screen_grab --format no_such_image capture.img"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"screen_grab should reject an invalid native image format before capture");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "fb2pix -s 0", std::strlen("fb2pix -s 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"fb2pix should reject a nonpositive image size");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "png2fb -g 0 input.png", std::strlen("png2fb -g 0 input.png"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"png2fb should reject a nonpositive gamma value");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "png2fb -H input.png", std::strlen("png2fb -H input.png"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"png2fb should accept its header-only mode and optional input file");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pix2fb -p -1 input.pix", std::strlen("pix2fb -p -1 input.pix"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"pix2fb should reject a negative pause duration");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pix2fb -w 64 input.pix", std::strlen("pix2fb -w 64 input.pix"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"pix2fb should accept a typed input width and input file");
    ged_cmd_validate_result_clear(&validation);

    {
	const char *sequence_parse_argv[] = {"--values", "1", "2", "3", "tail"};
	const char *sequence_short_argv[] = {"--values", "1"};
	const char *sequence_bad_argv[] = {"--values", "1", "bad"};
	struct native_sequence_args sequence_args = {{0, 0, 0}};
	struct bu_vls sequence_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result sequence_result = BU_CMD_VALIDATE_RESULT_NULL;
	char *sequence_json = NULL;

	CHECK(bu_cmd_schema_parse(&native_sequence_schema, &sequence_args, &sequence_msg,
		5, sequence_parse_argv) == 4,
	    "native bounded sequence parser should consume its declared three tokens");
	CHECK(sequence_args.values[0] == 1 && sequence_args.values[1] == 2 && sequence_args.values[2] == 3,
	    "native bounded sequence consumer should bind every declared token");
	CHECK(bu_cmd_schema_validate(&native_sequence_schema, 2, sequence_short_argv, 2, &sequence_result) == 0,
	    "native bounded sequence partial validation should run");
	CHECK(sequence_result.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (sequence_result.expected & BU_CMD_EXPECT_OPTION_ARG),
	    "native bounded sequence should report missing required components");
	bu_cmd_validate_result_clear(&sequence_result);
	CHECK(bu_cmd_schema_validate(&native_sequence_schema, 3, sequence_bad_argv, 3, &sequence_result) == 0,
	    "native bounded sequence invalid validation should run");
	CHECK(sequence_result.state == BU_CMD_VALIDATE_INVALID,
	    "native bounded sequence should use its consumer for side-effect-free validation");
	bu_cmd_validate_result_clear(&sequence_result);
	sequence_json = bu_cmd_schema_describe_json(&native_sequence_schema);
	CHECK(sequence_json && std::strstr(sequence_json, "\"kind\":\"token_sequence\"") &&
	    std::strstr(sequence_json, "\"min_tokens\":2") &&
	    std::strstr(sequence_json, "\"max_tokens\":3"),
	    "native schema JSON should publish bounded argument-shape metadata");
	bu_free(sequence_json, "native sequence schema json");
	bu_vls_free(&sequence_msg);
    }

    {
	const char *rgb_packed_argv[] = {"--color", "1;2;3", "tail"};
	const char *rgb_separate_argv[] = {"--color", "1", "2", "3", "tail"};
	const char *rgb_attached_argv[] = {"--color=1,2,3", "tail"};
	const char *rgb_partial_argv[] = {"--color", "1", "2"};
	const char *rgb_bad_argv[] = {"--color", "1/2,3"};
	const char *rgb_optional_packed_argv[] = {"1;2;3"};
	const char *rgb_optional_partial_argv[] = {"1", "2"};
	const char *color_optional_packed_argv[] = {"1/2/3"};
	const char *color_optional_components_argv[] = {"0.1", "0.2", "0.3"};
	const char *color_optional_partial_argv[] = {"1", "2"};
	struct native_rgb_args rgb_args = {};
	struct bu_vls rgb_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result rgb_result = BU_CMD_VALIDATE_RESULT_NULL;
	unsigned char rgb[3] = {0, 0, 0};
	char *rgb_json = NULL;

	CHECK(bu_cmd_schema_parse(&native_rgb_schema, &rgb_args, &rgb_msg,
		3, rgb_packed_argv) == 2,
	    "native RGB parser should leave a tail after a packed color");
	bu_color_to_rgb_chars(&rgb_args.color, rgb);
	CHECK(rgb[RED] == 1 && rgb[GRN] == 2 && rgb[BLU] == 3,
	    "native RGB parser should bind a packed semicolon color");
	CHECK(bu_cmd_schema_parse(&native_rgb_schema, &rgb_args, &rgb_msg,
		5, rgb_separate_argv) == 4,
	    "native RGB parser should consume three separate channels");
	CHECK(bu_cmd_schema_parse(&native_rgb_schema, &rgb_args, &rgb_msg,
		2, rgb_attached_argv) == 1,
	    "native RGB parser should accept an attached packed color");
	CHECK(bu_cmd_schema_validate(&native_rgb_schema, 3, rgb_partial_argv, 3, &rgb_result) == 0,
	    "native RGB partial validation should run");
	CHECK(rgb_result.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (rgb_result.expected & BU_CMD_EXPECT_OPTION_ARG),
	    "native RGB parser should report missing separate channels as incomplete");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_schema_validate(&native_rgb_schema, 2, rgb_bad_argv, 2, &rgb_result) == 0,
	    "native RGB invalid validation should run");
	CHECK(rgb_result.state == BU_CMD_VALIDATE_INVALID,
	    "native RGB parser should reject mixed packed delimiters");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_rgb_optional_validate(1, rgb_optional_packed_argv, 1, &rgb_result) == 0 &&
	    rgb_result.state == BU_CMD_VALIDATE_VALID &&
	    rgb_result.completion_type == BU_CMD_VALUE_COLOR,
	    "native optional RGB validator should accept a packed RGB value");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_rgb_optional_validate(2, rgb_optional_partial_argv, 2, &rgb_result) == 0 &&
	    rgb_result.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "native optional RGB validator should preserve a partial channel sequence");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_color_optional_validate(1, color_optional_packed_argv, 1, &rgb_result) == 0 &&
	    rgb_result.state == BU_CMD_VALIDATE_VALID &&
	    rgb_result.completion_type == BU_CMD_VALUE_COLOR,
	    "native optional color validator should accept a packed compatibility color");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_color_optional_validate(3, color_optional_components_argv, 3, &rgb_result) == 0 &&
	    rgb_result.state == BU_CMD_VALIDATE_VALID,
	    "native optional color validator should accept separate compatibility components");
	bu_cmd_validate_result_clear(&rgb_result);
	CHECK(bu_cmd_color_optional_validate(2, color_optional_partial_argv, 2, &rgb_result) == 0 &&
	    rgb_result.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "native optional color validator should preserve a partial channel sequence");
	bu_cmd_validate_result_clear(&rgb_result);
	rgb_json = bu_cmd_schema_describe_json(&native_rgb_schema);
	CHECK(rgb_json && std::strstr(rgb_json, "\"kind\":\"rgb\"") &&
	    std::strstr(rgb_json, "\"min_tokens\":1") &&
	    std::strstr(rgb_json, "\"max_tokens\":3"),
	    "native RGB JSON should publish the standard RGB shape");
	bu_free(rgb_json, "native RGB schema json");
	bu_vls_free(&rgb_msg);
    }

    {
	const char *vector_packed_argv[] = {"--vector", "1;2.5;-3", "tail"};
	const char *vector_quoted_argv[] = {"--vector", "1 2 3", "tail"};
	const char *vector_separate_argv[] = {"--vector", "1", "2", "3", "tail"};
	const char *vector_attached_argv[] = {"--vector=1,2,3", "tail"};
	const char *vector_partial_argv[] = {"--vector", "1", "2"};
	const char *vector_bad_argv[] = {"--vector", "1", "nan"};
	const char *vector_reader_bad_argv[] = {"1/2/bad"};
	const char *counted_vector_packed_argv[] = {"4", "1/2/3"};
	const char *counted_vector_separate_argv[] = {"4", "1", "2", "3"};
	const char *counted_vector_bad_argv[] = {"four", "1/2/3"};
	const char *vector_optional_packed_argv[] = {"1;2.5;-3"};
	const char *vector_optional_partial_argv[] = {"1", "2"};
	struct native_vector3_args vector_args = {};
	struct bu_vls vector_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result vector_result = BU_CMD_VALIDATE_RESULT_NULL;
	fastf_t xyz[3] = {4.0, 5.0, 6.0};
	vect_t expected_vector = VINIT_ZERO;
	vect_t expected_counted_vector = VINIT_ZERO;
	vect_t unchanged_vector = VINIT_ZERO;
	int vector_count = -1;
	char *vector_json = NULL;

	CHECK(bu_cmd_schema_parse(&native_vector3_schema, &vector_args, &vector_msg,
		3, vector_packed_argv) == 2,
	    "native vector3 parser should leave a tail after a packed vector");
	VSET(expected_vector, 1.0, 2.5, -3.0);
	VSET(expected_counted_vector, 1.0, 2.0, 3.0);
	CHECK(VNEAR_EQUAL(vector_args.vector, expected_vector, SMALL_FASTF),
	    "native vector3 parser should bind packed finite components");
	CHECK(bu_cmd_schema_parse(&native_vector3_schema, &vector_args, &vector_msg,
		3, vector_quoted_argv) == 2,
	    "native vector3 parser should accept a quoted space-separated vector");
	CHECK(bu_cmd_schema_parse(&native_vector3_schema, &vector_args, &vector_msg,
		5, vector_separate_argv) == 4,
	    "native vector3 parser should consume three separate components");
	CHECK(bu_cmd_schema_parse(&native_vector3_schema, &vector_args, &vector_msg,
		2, vector_attached_argv) == 1,
	    "native vector3 parser should accept an attached packed vector");
	CHECK(bu_cmd_schema_validate(&native_vector3_schema, 3, vector_partial_argv, 3, &vector_result) == 0,
	    "native vector3 partial validation should run");
	CHECK(vector_result.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (vector_result.expected & BU_CMD_EXPECT_OPTION_ARG),
	    "native vector3 parser should report missing separate components as incomplete");
	bu_cmd_validate_result_clear(&vector_result);
	CHECK(bu_cmd_schema_validate(&native_vector3_schema, 3, vector_bad_argv, 3, &vector_result) == 0,
	    "native vector3 invalid validation should run");
	CHECK(vector_result.state == BU_CMD_VALIDATE_INVALID,
	    "native vector3 parser should reject a nonfinite partial component");
	bu_cmd_validate_result_clear(&vector_result);
	VSET(unchanged_vector, 4.0, 5.0, 6.0);
	CHECK(!bu_cmd_vector3_from_argv(xyz, 1, vector_reader_bad_argv) &&
	    VNEAR_EQUAL(xyz, unchanged_vector, SMALL_FASTF),
	    "native vector3 reader should not change storage on failure");
	CHECK(bu_cmd_counted_vector3_from_argv(&vector_count, xyz, 2,
	counted_vector_packed_argv) == 2 && vector_count == 4 &&
	VNEAR_EQUAL(xyz, expected_counted_vector, SMALL_FASTF),
	"native counted-vector reader should accept a packed XYZ value");
	CHECK(bu_cmd_counted_vector3_from_argv(&vector_count, xyz, 4,
	counted_vector_separate_argv) == 4 && vector_count == 4 &&
	VNEAR_EQUAL(xyz, expected_counted_vector, SMALL_FASTF),
	"native counted-vector reader should accept three separate components");
	vector_count = 99;
	VSET(xyz, 4.0, 5.0, 6.0);
	CHECK(!bu_cmd_counted_vector3_from_argv(&vector_count, xyz, 2,
	counted_vector_bad_argv) && vector_count == 99 &&
	VNEAR_EQUAL(xyz, unchanged_vector, SMALL_FASTF),
	"native counted-vector reader should not change either output on failure");
	CHECK(bu_cmd_vector3_optional_validate(1, vector_optional_packed_argv, 1, &vector_result) == 0 &&
	    vector_result.state == BU_CMD_VALIDATE_VALID &&
	    vector_result.completion_type == BU_CMD_VALUE_VECTOR,
	    "native optional vector validator should accept a packed XYZ value");
	bu_cmd_validate_result_clear(&vector_result);
	CHECK(bu_cmd_vector3_optional_validate(2, vector_optional_partial_argv, 2, &vector_result) == 0 &&
	    vector_result.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "native optional vector validator should preserve a partial component sequence");
	bu_cmd_validate_result_clear(&vector_result);
	CHECK(bu_cmd_vector3_required_validate(0, NULL, 0, &vector_result) == 0 &&
	    vector_result.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "native required vector validator should require a value");
	bu_cmd_validate_result_clear(&vector_result);
	CHECK(bu_cmd_vector3_required_validate(1, vector_optional_packed_argv, 1, &vector_result) == 0 &&
	    vector_result.state == BU_CMD_VALIDATE_VALID,
	    "native required vector validator should accept a packed vector");
	bu_cmd_validate_result_clear(&vector_result);
	vector_json = bu_cmd_schema_describe_json(&native_vector3_schema);
	CHECK(vector_json && std::strstr(vector_json, "\"kind\":\"vector3\"") &&
	    std::strstr(vector_json, "\"min_tokens\":1") &&
	    std::strstr(vector_json, "\"max_tokens\":3"),
	    "native schema JSON should publish the standard vector3 shape");
	bu_free(vector_json, "native vector3 schema json");
	bu_vls_free(&vector_msg);
    }

    {
	const char *pair_argv[] = {"0x10", "-2"};
	const char *pair_partial_argv[] = {"0x10"};
	const char *pair_bad_argv[] = {"0x10", "bad"};
	int pair[2] = {4, 5};
	struct bu_cmd_validate_result pair_result = BU_CMD_VALIDATE_RESULT_NULL;

	CHECK(bu_cmd_integer_pair_from_argv(pair, 2, pair_argv) == 2 &&
	    pair[0] == 16 && pair[1] == -2,
	    "native integer-pair reader should accept base-zero components");
	CHECK(!bu_cmd_integer_pair_from_argv(pair, 2, pair_bad_argv) &&
	    pair[0] == 16 && pair[1] == -2,
	    "native integer-pair reader should not change storage on failure");
	CHECK(bu_cmd_integer_pair_optional_validate(1, pair_partial_argv, 1, &pair_result) == 0 &&
	    pair_result.state == BU_CMD_VALIDATE_INCOMPLETE,
	    "native integer-pair validator should preserve one component as incomplete");
	bu_cmd_validate_result_clear(&pair_result);
	CHECK(bu_cmd_integer_pair_optional_validate(2, pair_argv, 2, &pair_result) == 0 &&
	    pair_result.state == BU_CMD_VALIDATE_VALID,
	    "native integer-pair validator should accept a complete pair");
	bu_cmd_validate_result_clear(&pair_result);
    }

    {
	const char *color_hex_argv[] = {"--color", "#102030", "tail"};
	const char *color_float_argv[] = {"--color", "0.1", "0.2", "0.3", "tail"};
	const char *color_partial_argv[] = {"--color", "0.1", "0.2"};
	const char *color_reader_bad_argv[] = {"not-a-color"};
	struct native_color_args color_args = {};
	struct bu_vls color_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result color_result = BU_CMD_VALIDATE_RESULT_NULL;
	struct bu_color unchanged_color = BU_COLOR_INIT_ZERO;
	const unsigned char unchanged_rgb_input[3] = {4, 5, 6};
	char *color_json = NULL;

	CHECK(bu_cmd_schema_parse(&native_color_schema, &color_args, &color_msg,
		3, color_hex_argv) == 2,
	    "native compatible color parser should leave a tail after a packed color");
	CHECK(bu_cmd_schema_parse(&native_color_schema, &color_args, &color_msg,
		5, color_float_argv) == 4,
	    "native compatible color parser should consume three floating RGB components");
	CHECK(bu_cmd_schema_validate(&native_color_schema, 3, color_partial_argv, 3, &color_result) == 0,
	    "native compatible color partial validation should run");
	CHECK(color_result.state == BU_CMD_VALIDATE_INCOMPLETE &&
	    (color_result.expected & BU_CMD_EXPECT_OPTION_ARG),
	    "native compatible color parser should report a partial floating triple as incomplete");
	bu_cmd_validate_result_clear(&color_result);
	bu_color_from_rgb_chars(&unchanged_color, unchanged_rgb_input);
	CHECK(!bu_cmd_color_from_argv(&unchanged_color, 1, color_reader_bad_argv),
	    "native compatible color reader should reject an invalid packed color");
	{
	    unsigned char unchanged_rgb[3] = {0, 0, 0};
	    bu_color_to_rgb_chars(&unchanged_color, unchanged_rgb);
	    CHECK(unchanged_rgb[RED] == 4 && unchanged_rgb[GRN] == 5 && unchanged_rgb[BLU] == 6,
		"native compatible color reader should not change storage on failure");
	}
	color_json = bu_cmd_schema_describe_json(&native_color_schema);
	CHECK(color_json && std::strstr(color_json, "\"kind\":\"color\"") &&
	    std::strstr(color_json, "\"min_tokens\":1") &&
	    std::strstr(color_json, "\"max_tokens\":3"),
	    "native schema JSON should publish the compatible color shape");
	bu_free(color_json, "native compatible color schema json");
	bu_vls_free(&color_msg);
    }

    {
	int integer_value = 41;
	unsigned int hex_integer_value = 41;
	long long_value = 41;
	long hex_long_value = 41;
	fastf_t number_value = 0.0;
	double unit_value = 25.4;
	char char_value = 'x';

	CHECK(bu_cmd_integer_from_str(&integer_value, "0x20") && integer_value == 32,
	    "native integer reader should accept the schema's base-zero integer grammar");
	CHECK(!bu_cmd_integer_from_str(&integer_value, "12oops") && integer_value == 32,
	    "native integer reader should reject partial text without changing storage");
	CHECK(bu_cmd_hex_integer_from_str(&hex_integer_value, "ff") && hex_integer_value == 255,
	    "native hexadecimal integer reader should accept hexadecimal input");
	CHECK(!bu_cmd_hex_integer_from_str(&hex_integer_value, "-1") && hex_integer_value == 255,
	    "native hexadecimal integer reader should reject a negative value without changing storage");
	CHECK(bu_cmd_long_from_str(&long_value, "0x20") && long_value == 32,
	    "native long reader should accept the schema's base-zero integer grammar");
	CHECK(!bu_cmd_long_from_str(&long_value, "12oops") && long_value == 32,
	    "native long reader should reject partial text without changing storage");
	CHECK(bu_cmd_hex_long_from_str(&hex_long_value, "ff") && hex_long_value == 255,
	    "native hexadecimal long reader should accept hexadecimal input");
	CHECK(!bu_cmd_hex_long_from_str(&hex_long_value, "0xgg") && hex_long_value == 255,
	    "native hexadecimal long reader should reject malformed text without changing storage");
	CHECK(bu_cmd_number_from_str(&number_value, "2.5") && number_value > 2.49 && number_value < 2.51,
	    "native number reader should accept a finite scalar");
	CHECK(!bu_cmd_number_from_str(&number_value, "nan") && number_value > 2.49 && number_value < 2.51,
	    "native number reader should reject a nonfinite scalar without changing storage");
	CHECK(bu_cmd_units_from_str(&unit_value, "in") && unit_value > 25.39 && unit_value < 25.41,
	    "native units reader should accept a named unit expression");
	CHECK(!bu_cmd_units_from_str(&unit_value, "not-a-unit") && unit_value > 25.39 && unit_value < 25.41,
	    "native units reader should reject an invalid expression without changing storage");
	CHECK(bu_cmd_char_from_str(&char_value, "test") && char_value == 't',
	    "native character reader should retain the first character");
	CHECK(!bu_cmd_char_from_str(&char_value, "") && char_value == 't',
	    "native character reader should reject an empty string without changing storage");
    }

    {
	const char *scalar_argv[] = {"-vv", "--long", "0x20", "--hex", "ff",
	    "--selector", "test", "--word", "first", "--word", "second"};
	struct native_scalar_args scalar_args = {0, 0, 0, '\0', BU_VLS_INIT_ZERO};
	struct bu_vls scalar_msg = BU_VLS_INIT_ZERO;
	char *scalar_json = NULL;

	CHECK(bu_cmd_schema_parse_complete(&native_scalar_schema, &scalar_args, &scalar_msg,
		11, scalar_argv) == 11,
	    "native scalar parser should consume standard scalar options");
	CHECK(bu_cmd_schema_option_span(&native_scalar_schema, 11, scalar_argv) == 1 &&
	    bu_cmd_schema_option_span(&native_scalar_schema, 10, scalar_argv + 1) == 2 &&
	    bu_cmd_schema_option_span(&native_scalar_schema, 1, scalar_argv + 1) < 0,
	    "native option span should follow flag and argument consumption rules");
	CHECK(scalar_args.verbosity == 2 && scalar_args.value == 32 && scalar_args.hex_value == 255 &&
	    scalar_args.selector == 't' && BU_STR_EQUAL(bu_vls_cstr(&scalar_args.words), "first second"),
	    "native scalar parser should bind counters, long values, characters, and VLS text");
	scalar_json = bu_cmd_schema_describe_json(&native_scalar_schema);
	CHECK(scalar_json && std::strstr(scalar_json, "\"type\":\"long\"") &&
	    std::strstr(scalar_json, "\"type\":\"hex_long\"") &&
	    std::strstr(scalar_json, "\"type\":\"char\"") &&
	    std::strstr(scalar_json, "\"type\":\"vls\""),
	    "native schema JSON should identify standard scalar storage types");
	bu_free(scalar_json, "native scalar schema json");
	bu_vls_free(&scalar_msg);
	bu_vls_free(&scalar_args.words);
    }

    {
	const char *vocabulary_argv[] = {"--language", "en", "--section", "n"};
	const char *bad_language_argv[] = {"--language", "EN"};
	struct native_vocabulary_args vocabulary_args = {BU_VLS_INIT_ZERO, '\0'};
	struct bu_vls vocabulary_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result vocabulary_result = BU_CMD_VALIDATE_RESULT_NULL;

	CHECK(bu_cmd_schema_parse_complete(&native_vocabulary_schema, &vocabulary_args,
		&vocabulary_msg, 4, vocabulary_argv) == 4 &&
	    BU_STR_EQUAL(bu_vls_cstr(&vocabulary_args.language), "en") && vocabulary_args.section == 'n',
	    "native vocabulary parser should bind ISO language and manual section values");
	CHECK(bu_cmd_schema_validate(&native_vocabulary_schema, 2, bad_language_argv, 2,
		&vocabulary_result) == 0 && vocabulary_result.state == BU_CMD_VALIDATE_INVALID,
	    "native vocabulary parser should reject an invalid language before VLS mutation");
	bu_cmd_validate_result_clear(&vocabulary_result);
	CHECK(bu_cmd_iso639_1_validate(NULL, "fr") == 0 &&
	    bu_cmd_iso639_1_validate(NULL, "FR") != 0 &&
	    bu_cmd_man_section_validate(NULL, "3") == 0 &&
	    bu_cmd_man_section_validate(NULL, "4") != 0,
	    "native vocabulary validators should preserve ISO and manual-section rules");
	bu_vls_free(&vocabulary_msg);
	bu_vls_free(&vocabulary_args.language);
    }

    {
	const char *cluster_argv[] = {"-ab", "tail"};
	const char *bad_cluster_argv[] = {"-ax", "tail"};
	struct native_cluster_args cluster_args = {0, 0};
	struct bu_vls cluster_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result cluster_result = BU_CMD_VALIDATE_RESULT_NULL;

	CHECK(bu_cmd_schema_parse_complete(&native_cluster_schema, &cluster_args, &cluster_msg,
		2, cluster_argv) == 1 && cluster_args.alpha && cluster_args.beta,
	    "native parser should bind every flag in a compact short-option cluster");
	CHECK(bu_cmd_schema_parse_complete(&native_cluster_schema, &cluster_args, &cluster_msg,
		2, bad_cluster_argv) < 0,
	    "native parser should reject a compact cluster containing an unknown flag");
	CHECK(bu_cmd_schema_validate(&native_cluster_schema, 2, cluster_argv, 0, &cluster_result) == 0 &&
		cluster_result.state == BU_CMD_VALIDATE_VALID,
	    "native validation should accept a compact short-option cluster");
	bu_cmd_validate_result_clear(&cluster_result);
	bu_vls_free(&cluster_msg);
    }

    {
	const char *raw_argv[] = {"-literal"};
	int raw_dummy = 0;
	struct bu_vls raw_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result raw_result = BU_CMD_VALIDATE_RESULT_NULL;

	CHECK(bu_cmd_schema_parse_complete(&native_raw_schema, &raw_dummy, &raw_msg,
		1, raw_argv) == 0,
	    "an optionless native schema should retain a leading-dash raw operand");
	CHECK(bu_cmd_schema_validate(&native_raw_schema, 1, raw_argv, 0, &raw_result) == 0 &&
		raw_result.state == BU_CMD_VALIDATE_VALID,
	    "optionless native validation should classify a leading-dash raw operand");
	bu_cmd_validate_result_clear(&raw_result);
	bu_vls_free(&raw_msg);
    }

    {
	const char *interspersed_argv[] = {"source", "--number", "7", "-a", "destination"};
	const char *interspersed_complete_argv[] = {"source", "--number", "7", "-a", "destination"};
	const char *interspersed_marker_argv[] = {"source", "--", "-destination"};
	struct native_interspersed_args interspersed_args = {0, 0};
	struct native_interspersed_args complete_args = {0, 0};
	struct native_interspersed_args marker_args = {0, 0};
	struct bu_vls interspersed_msg = BU_VLS_INIT_ZERO;

	CHECK(bu_cmd_schema_parse(&native_interspersed_schema, &interspersed_args,
		&interspersed_msg, 5, interspersed_argv) == 3 &&
		interspersed_args.alpha && interspersed_args.number == 7 &&
		BU_STR_EQUAL(interspersed_argv[3], "source") &&
		BU_STR_EQUAL(interspersed_argv[4], "destination"),
	    "native interspersed parsing should compact options and retain operand order");
	CHECK(bu_cmd_schema_parse_complete(&native_interspersed_schema, &complete_args,
		&interspersed_msg, 5, interspersed_complete_argv) == 3 &&
		complete_args.alpha && complete_args.number == 7 &&
		BU_STR_EQUAL(interspersed_complete_argv[3], "source") &&
		BU_STR_EQUAL(interspersed_complete_argv[4], "destination"),
	    "native complete parsing should validate the compacted interspersed form");
	CHECK(bu_cmd_schema_parse_complete(&native_interspersed_schema, &marker_args,
		&interspersed_msg, 3, interspersed_marker_argv) == 1 &&
		BU_STR_EQUAL(interspersed_marker_argv[1], "source") &&
		BU_STR_EQUAL(interspersed_marker_argv[2], "-destination"),
	    "native interspersed parsing should retain an end-marker-protected operand");
	bu_vls_free(&interspersed_msg);
    }

    {
	const char *file_argv[] = {"-f", "mappings.txt"};
	const char *objects_argv[] = {"all.g", "renamed.g"};
	const char *mixed_argv[] = {"-f", "mappings.txt", "all.g"};
	const char *short_argv[] = {"all.g"};
	const char *modes_argv[] = {"-in", "all.g", "renamed.g"};
	struct native_constraint_args constraint_args = {NULL, 0, 0};
	struct bu_vls constraint_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result constraint_result = BU_CMD_VALIDATE_RESULT_NULL;
	char *constraint_json = NULL;

	CHECK(bu_cmd_schema_parse_complete(&native_constraint_schema, &constraint_args,
		&constraint_msg, 2, file_argv) == 2 && BU_STR_EQUAL(constraint_args.file, "mappings.txt"),
	    "native constraints should permit their mapping-file form during execution parsing");
	CHECK(bu_cmd_schema_parse_complete(&native_constraint_schema, &constraint_args,
		&constraint_msg, 2, objects_argv) == 0,
	    "native constraints should permit their source/destination form during execution parsing");
	CHECK(bu_cmd_schema_validate(&native_constraint_schema, 3, mixed_argv, 3, &constraint_result) == 0 &&
		constraint_result.state == BU_CMD_VALIDATE_INVALID,
	    "native constraints should reject mutually exclusive mapping-file and operand forms");
	bu_cmd_validate_result_clear(&constraint_result);
	CHECK(bu_cmd_schema_validate(&native_constraint_schema, 1, short_argv, 1, &constraint_result) == 0 &&
		constraint_result.state == BU_CMD_VALIDATE_INCOMPLETE &&
		constraint_result.completion_type == BU_CMD_VALUE_STRING,
	    "native constraints should preserve the destination role while awaiting a second operand");
	bu_cmd_validate_result_clear(&constraint_result);
	CHECK(bu_cmd_schema_validate(&native_constraint_schema, 3, modes_argv, 3, &constraint_result) == 0 &&
		constraint_result.state == BU_CMD_VALIDATE_INVALID,
	    "native constraints should reject compact clusters containing conflicting modes");
	bu_cmd_validate_result_clear(&constraint_result);
	constraint_json = bu_cmd_schema_describe_json(&native_constraint_schema);
	CHECK(constraint_json && std::strstr(constraint_json, "\"constraints\":[") &&
	    std::strstr(constraint_json, "\"condition\":\"no_option_present\""),
	    "native schema JSON should publish declarative constraint rows");
	bu_free(constraint_json, "native constraint schema json");
	bu_vls_free(&constraint_msg);
    }

    {
	const char *keyword_alias_argv[] = {"--kind", "first"};
	const char *keyword_invalid_argv[] = {"--kind", "third"};
	struct native_keyword_args keyword_args = {NULL};
	struct bu_vls keyword_msg = BU_VLS_INIT_ZERO;
	struct bu_cmd_validate_result keyword_result = BU_CMD_VALIDATE_RESULT_NULL;
	char *keyword_json = NULL;

	CHECK(bu_cmd_schema_parse(&native_keyword_schema, &keyword_args, &keyword_msg,
		2, keyword_alias_argv) == 2 && BU_STR_EQUAL(keyword_args.value, "primary"),
	    "native keyword parsing should normalize an alias to its canonical value");
	CHECK(bu_cmd_schema_parse(&native_keyword_schema, &keyword_args, &keyword_msg,
		2, keyword_invalid_argv) < 0,
	    "native keyword parsing should reject an undocumented value");
	CHECK(bu_cmd_schema_validate(&native_keyword_schema, 2, keyword_alias_argv, 1, &keyword_result) == 0 &&
	    keyword_result.state == BU_CMD_VALIDATE_VALID && keyword_result.completion_count == 1 &&
	    BU_STR_EQUAL(keyword_result.completion_candidates[0], "primary"),
	    "native keyword validation should accept aliases and offer canonical completions");
	bu_cmd_validate_result_clear(&keyword_result);
	keyword_json = bu_cmd_schema_describe_json(&native_keyword_schema);
	CHECK(keyword_json && std::strstr(keyword_json, "\"canonical\":\"primary\"") &&
	    std::strstr(keyword_json, "\"aliases\":[\"first\",\"one\"]") &&
	    std::strstr(keyword_json, "\"description\":\"Primary selection\""),
	    "native keyword JSON should publish canonical aliases and descriptions");
	bu_free(keyword_json, "native keyword schema json");
	bu_vls_free(&keyword_msg);
    }

    {
	const char * const values[] = {"alpha", "alpine", "beta", NULL};
	struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;

	result.state = BU_CMD_VALIDATE_VALID;
	bu_cmd_keyword_candidates(&result, values, "al");
	CHECK(result.state == BU_CMD_VALIDATE_VALID && result.completion_count == 2 &&
		BU_STR_EQUAL(result.completion_candidates[0], "alpha") &&
		BU_STR_EQUAL(result.completion_candidates[1], "alpine") &&
		result.completion_candidates[2] == NULL,
	    "native keyword candidate helper should filter and own canonical matches");
	bu_cmd_keyword_candidates(&result, values, "z");
	CHECK(result.completion_count == 0 && result.completion_candidates == NULL,
	    "native keyword candidate helper should replace prior candidates when no values match");
	bu_cmd_validate_result_clear(&result);
    }

    {
	size_t search_term_count = 0;
	const struct db_search_syntax_term *search_terms = db_search_syntax_terms(&search_term_count);
	const struct db_search_syntax_term *type_term = NULL;
	const struct db_search_syntax_term *exec_term = NULL;
	const struct db_search_syntax_term *bool_term = NULL;
	CHECK(search_terms && search_term_count > 0, "search should publish its parser-owned syntax terms");
	for (size_t i = 0; i < search_term_count; i++) {
	    if (BU_STR_EQUAL(search_terms[i].name, "-type")) type_term = &search_terms[i];
	    if (BU_STR_EQUAL(search_terms[i].name, "-exec")) exec_term = &search_terms[i];
	    if (BU_STR_EQUAL(search_terms[i].name, "-bool")) bool_term = &search_terms[i];
	}
	CHECK(type_term && type_term->argument == DB_SEARCH_SYNTAX_TYPE_ARGUMENT,
	    "published search -type syntax should be typed");
	CHECK(exec_term && exec_term->argument == DB_SEARCH_SYNTAX_EXEC_ARGUMENTS,
	    "published search -exec syntax should be variadic");
	CHECK(bool_term && bool_term->argument == DB_SEARCH_SYNTAX_STRING_ARGUMENT,
	    "published search -bool syntax should be typed");
	CHECK(db_search_syntax_plan_start("-type") && !db_search_syntax_plan_start("/all.g"),
	    "published search plan-start recognition should match the parser");
	CHECK(db_search_syntax_exec_substitution("draw {}") && !db_search_syntax_exec_substitution("draw object"),
	    "published search substitution recognition should match the parser");
    }

    CHECK(ged_cmd_schema_exists("search"), "search schema is unavailable");
    schema_json = ged_cmd_schema_json("search");
    CHECK(schema_json && std::strstr(schema_json, "\"kind\":\"grammar_adapter\"") &&
	std::strstr(schema_json, "\"parse_policy\":\"grammar_adapter\"") &&
	std::strstr(schema_json, "\"options\":[") &&
	std::strstr(schema_json, "\"name\":\"-v\",\"argument\":\"none\",\"repeatable\":true") &&
	std::strstr(schema_json, "\"name\":\"-exec\",\"argument\":\"exec\"") &&
	std::strstr(schema_json, "\"name\":\"-type\",\"argument\":\"type\""),
	"search grammar JSON should publish shared command options and typed parser vocabulary");
    bu_free(schema_json, "search schema json");
    schema_json = NULL;
    CHECK(ged_cmd_schema_lint("search", &lint_msgs) == 0, bu_vls_cstr(&lint_msgs));

    completion_count = ged_cmd_complete_result(gedp, "search -ty", std::strlen("search -ty"), &completion_result);
    CHECK(completion_count > 0, "search -ty should find search expression operators");
    CHECK(completion_result.prefix && BU_STR_EQUAL(completion_result.prefix, "-ty"), "search operator completion should preserve prefix");
    CHECK(completion_result.replacement_start == std::strlen("search "), "search operator replacement start mismatch");
    completion = find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-type");
    CHECK(completion != NULL, "search -ty should offer -type");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search -bool ", std::strlen("search -bool "), &completion_result);
    CHECK(completion_count == 3 && completion_result.completion_type == BU_CMD_VALUE_KEYWORD,
	    "search -bool should offer all three boolean operation spellings");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "u") != NULL &&
	    find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "+") != NULL &&
	    find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-") != NULL,
	    "search -bool completion should offer union, intersection, and subtraction");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search -bool -", std::strlen("search -bool -"), &completion_result);
    CHECK(completion_count == 1 &&
	    find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-") != NULL,
	    "search -bool prefix should retain the subtraction candidate");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search -type regi", std::strlen("search -type regi"), &completion_result);
    CHECK(completion_count > 0, "search -type regi should find type keywords");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD, "search -type completion should report keyword type");
    CHECK(completion_result.prefix && BU_STR_EQUAL(completion_result.prefix, "regi"), "search type completion should preserve prefix");
    completion = find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "region");
    CHECK(completion != NULL, "search -type regi should offer region");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search -type ", std::strlen("search -type "), &completion_result);
    CHECK(completion_count > 0, "search -type should offer user-facing type keywords");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "UNUSED1") == NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "UNUSED2") == NULL &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, ">id_max") == NULL,
	"search -type should hide primitive-table placeholders and its sentinel");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search all.g -Q", std::strlen("search all.g -Q"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-Q") == NULL,
	    "search expression completion after a path should not offer top-level -Q");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_validate(gedp, "search all.g -Q", std::strlen("search all.g -Q"), &validation) == 0,
	    "search validation after path should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID, "search all.g -Q should be an invalid expression operator");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("log"), "native log schema is unavailable");
    schema_json = ged_cmd_schema_json("log");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"log\"") &&
	std::strstr(schema_json, "\"values\":[\"get\",\"start\",\"stop\"]") &&
	std::strstr(schema_json, "\"canonical\":\"start\"") &&
	std::strstr(schema_json, "\"description\":\"Begin collecting bu_log output\""),
	"native keyword schema JSON should publish canonical values and descriptions");
    bu_free(schema_json, "log schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "log st", std::strlen("log st"), &completion_result);
    CHECK(completion_count > 0, "log st should find action keywords");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD, "log action completion should report keyword type");
    CHECK(completion_result.prefix && BU_STR_EQUAL(completion_result.prefix, "st"), "log action completion should preserve prefix");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "start") != NULL,
	    "log st should offer start");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "stop") != NULL,
	"log st should offer stop");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "log pause", std::strlen("log pause"), &validation) == 0,
	"native keyword validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"native keyword validation should reject an undocumented log action");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("units"), "native units schema is unavailable");
    schema_json = ged_cmd_schema_json("units");
    CHECK(schema_json && std::strstr(schema_json, "\"keyword_values\"") &&
	std::strstr(schema_json, "\"canonical\":\"mm\"") &&
	std::strstr(schema_json, "\"aliases\":[\"millimeter\",\"millimeters\"]") &&
	std::strstr(schema_json, "\"description\":\"Millimetre\""),
	"native units schema should publish canonical keyword aliases and descriptions");
    bu_free(schema_json, "units schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "units milli", std::strlen("units milli"), &completion_result);
    CHECK(completion_count > 0 && completion_result.completion_type == BU_CMD_VALUE_KEYWORD &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "mm") != NULL,
	"units alias completion should offer the canonical millimetre spelling");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "units millimeter", std::strlen("units millimeter"), &validation) == 0,
	"units alias validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"units should accept a documented alias");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "units 25cm", std::strlen("units 25cm"), &validation) == 0,
	"units expression validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"units should retain its broader unit-expression grammar");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "units fathom", std::strlen("units fathom"), &validation) == 0,
	"units invalid validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"units should reject an unsupported unit expression");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "units -s mm", std::strlen("units -s mm"), &validation) == 0,
	"units exclusive-form validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"units should reject an operand after its short-name option");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "units millimeter", &analysis) == 0,
	"units alias analysis should run");
    {
	const struct ged_cmd_analysis_token *unit_value = token_matching(&analysis, "units millimeter", "millimeter");
	CHECK(unit_value && unit_value->role == GED_CMD_TOKEN_OPERAND &&
	    unit_value->value_type == BU_CMD_VALUE_KEYWORD && unit_value->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "units aliases should retain valid keyword highlighting");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_schema_exists("make_name"), "native make_name schema is unavailable");
    schema_json = ged_cmd_schema_json("make_name");
    CHECK(schema_json && std::strstr(schema_json, "\"argument_requirement\":\"optional\"") &&
	std::strstr(schema_json, "\"long\":\"\""),
	"native make_name schema should publish its optional -s argument");
    bu_free(schema_json, "make_name schema json");
    schema_json = NULL;

    completion_count = ged_cmd_complete_result(gedp, "make_name ", std::strlen("make_name "), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "-s") != NULL,
	"make_name should complete its reset option");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "make_name ", std::strlen("make_name "), &validation) == 0,
	"native make_name empty validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"make_name should require a template or reset option");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "make_name -s", std::strlen("make_name -s"), &validation) == 0,
	"native make_name optional reset validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"make_name -s should accept its omitted optional integer");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "make_name -s 42", std::strlen("make_name -s 42"), &validation) == 0,
	"native make_name numeric reset validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"make_name -s 42 should accept the optional integer");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "make_name -s nope", std::strlen("make_name -s nope"), &validation) == 0,
	"native make_name invalid reset validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"make_name -s should reject a non-integer optional argument");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "make_name name@", std::strlen("make_name name@"), &validation) == 0,
	"native make_name template validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID,
	"make_name should accept one template alternative");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "make_name -s 42 name@", std::strlen("make_name -s 42 name@"), &validation) == 0,
	"native make_name exclusive alternatives validation should run");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID,
	"make_name should reject a template after its reset alternative");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "make_name -s nope", &analysis) == 0,
	"native make_name invalid optional argument analysis should run");
    {
	const struct ged_cmd_analysis_token *reset_value = token_matching(&analysis, "make_name -s nope", "nope");
	CHECK(reset_value && reset_value->role == GED_CMD_TOKEN_OPTION_ARG &&
	    reset_value->value_type == BU_CMD_VALUE_INTEGER &&
	    reset_value->semantic_state == GED_CMD_SEMANTIC_INVALID,
	    "make_name optional argument should retain integer highlighting");
    }
    ged_cmd_analysis_clear(&analysis);

    {
	const char *make_name_reset[] = {"make_name", "-s"};
	const char *make_name_start[] = {"make_name", "-s", "42"};
	const char *make_name_invalid[] = {"make_name", "-s", "nope"};
	CHECK(ged_exec_make_name(gedp, 2, make_name_reset) == BRLCAD_OK,
	    "make_name execution should accept an omitted optional reset argument");
	CHECK(ged_exec_make_name(gedp, 3, make_name_start) == BRLCAD_OK,
	    "make_name execution should bind its optional integer through the native parser");
	CHECK(ged_exec_make_name(gedp, 3, make_name_invalid) == BRLCAD_ERROR,
	    "make_name execution should reject the same invalid optional integer as analysis");
    }

    completion_count = ged_cmd_complete_result(gedp, "set_uplotOutputMode b", std::strlen("set_uplotOutputMode b"), &completion_result);
    CHECK(completion_count > 0, "set_uplotOutputMode b should find mode keywords");
    CHECK(completion_result.completion_type == BU_CMD_VALUE_KEYWORD, "set_uplotOutputMode completion should report keyword type");
    CHECK(completion_result.prefix && BU_STR_EQUAL(completion_result.prefix, "b"), "set_uplotOutputMode completion should preserve prefix");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "binary") != NULL,
	    "set_uplotOutputMode b should offer binary");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "delay 0 0", std::strlen("delay 0 0"), &validation) == 0,
	    "delay validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID, "delay 0 0 should be valid");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_schema_exists("put_comb"), "native put_comb schema is unavailable");
    schema_json = ged_cmd_schema_json("put_comb");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"put_comb\"") &&
	std::strstr(schema_json, "\"name\":\"boolean_expression\"") &&
	std::strstr(schema_json, "\"name\":\"region_fields\""),
	"native put_comb schema should publish its expression and conditional tail roles");
    bu_free(schema_json, "put_comb schema json");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp,
	"put_comb native_put_comb_check.c 0,220,220 plastic n \"u all.g\" n",
	std::strlen("put_comb native_put_comb_check.c 0,220,220 plastic n \"u all.g\" n"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_VALID,
	"put_comb should validate a non-region native form");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp,
	"put_comb native_put_comb_check.r 0;220;220 plastic n \"u all.g\" y",
	std::strlen("put_comb native_put_comb_check.r 0;220;220 plastic n \"u all.g\" y"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"put_comb should report a true region flag without its required tail as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp,
	"put_comb native_put_comb_check.c 0/220/220 plastic n \"u all.g\" n 1000",
	std::strlen("put_comb native_put_comb_check.c 0/220/220 plastic n \"u all.g\" n 1000"),
	&validation) == 0 && validation.state == BU_CMD_VALIDATE_INVALID,
	"put_comb should reject region fields after a false region flag");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_analyze(gedp,
	"put_comb native_put_comb_check.c 0,220,220 plastic n \"u all.g\" n", &analysis) == 0,
	"native put_comb analysis should succeed");
    {
	const char *input = "put_comb native_put_comb_check.c 0,220,220 plastic n \"u all.g\" n";
	const struct ged_cmd_analysis_token *color = token_matching(&analysis, input, "0,220,220");
	CHECK(color && color->role == GED_CMD_TOKEN_OPERAND &&
	    color->value_type == BU_CMD_VALUE_COLOR && color->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native put_comb analysis should classify its color with the shared color type");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_schema_exists("find_pipe_pnt") &&
	ged_cmd_schema_exists("pipe_move_pnt") &&
	ged_cmd_schema_exists("pipe_append_pnt"),
	"native pipe-point schemas are unavailable");
    schema_json = ged_cmd_schema_json("pipe_move_pnt");
    CHECK(schema_json && std::strstr(schema_json, "\"name\":\"pipe_move_pnt\"") &&
	std::strstr(schema_json, "\"canonical\":\"r\"") &&
	std::strstr(schema_json, "\"name\":\"segment\""),
	"native pipe-point JSON should publish its relative flag and typed operands");
    bu_free(schema_json, "pipe move schema json");
    schema_json = NULL;

    CHECK(ged_cmd_validate(gedp, "find_pipe_pnt all.g 0 0 0",
	std::strlen("find_pipe_pnt all.g 0 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"find_pipe_pnt should accept three finite coordinates");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "find_pipe_pnt all.g 0 0",
	std::strlen("find_pipe_pnt all.g 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INCOMPLETE,
	"find_pipe_pnt should retain a partial three-coordinate point as incomplete");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pipe_append_pnt all.g 0 0 0",
	std::strlen("pipe_append_pnt all.g 0 0 0"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"pipe_append_pnt should require its documented packed point argument");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pipe_move_pnt -r all.g 0 \"0 0 0\"",
	std::strlen("pipe_move_pnt -r all.g 0 \"0 0 0\""), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_VALID,
	"pipe_move_pnt should accept its leading native relative flag and packed point");
    ged_cmd_validate_result_clear(&validation);
    CHECK(ged_cmd_validate(gedp, "pipe_delete_pnt all.g bad",
	std::strlen("pipe_delete_pnt all.g bad"), &validation) == 0 &&
	validation.state == BU_CMD_VALIDATE_INVALID,
	"pipe_delete_pnt should reject a malformed segment index");
    ged_cmd_validate_result_clear(&validation);
    completion_count = ged_cmd_complete_result(gedp, "pipe_move_pnt -",
	std::strlen("pipe_move_pnt -"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates,
	(int)completion_result.completion_count, "-r") != NULL,
	"pipe_move_pnt should complete its relative flag from the native schema");
    ged_cmd_completion_result_clear(&completion_result);
    CHECK(ged_cmd_analyze(gedp, "find_pipe_pnt all.g 0 0", &analysis) == 0,
	"native find_pipe_pnt analysis should succeed");
    {
	const struct ged_cmd_analysis_token *coordinate = analysis.token_count ?
	    &analysis.tokens[analysis.token_count - 1] : NULL;
	CHECK(coordinate && coordinate->role == GED_CMD_TOKEN_OPERAND &&
	    coordinate->value_type == BU_CMD_VALUE_VECTOR &&
	    coordinate->semantic_state == GED_CMD_SEMANTIC_INCOMPLETE,
	    "native find_pipe_pnt analysis should preserve a partial vector role");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_validate(gedp, "delay 0 nope", std::strlen("delay 0 nope"), &validation) == 0,
	"native delay validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID, "delay should reject a non-integer microsecond operand");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "delay -1 0", std::strlen("delay -1 0"), &validation) == 0,
	"native delay range validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID, "delay should reject negative seconds");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_validate(gedp, "delay 0 1000000", std::strlen("delay 0 1000000"), &validation) == 0,
	"native delay microsecond range validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INVALID, "delay should reject out-of-range microseconds");
    ged_cmd_validate_result_clear(&validation);

    {
	const char *delay_valid[] = {"delay", "0", "0"};
	const char *delay_invalid[] = {"delay", "-1", "0"};
	CHECK(ged_exec_delay(gedp, 3, delay_valid) == BRLCAD_OK,
	    "delay execution should accept the schema-valid zero interval");
	CHECK(ged_exec_delay(gedp, 3, delay_invalid) == BRLCAD_ERROR,
	    "delay execution should reject the same negative seconds the schema rejects");
    }

    completion_count = ged_cmd_complete_result(gedp, "hide t", std::strlen("hide t"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT,
	"native hide operand should retain its database-object completion type");
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	"native hide should complete database objects");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_validate(gedp, "hide --", std::strlen("hide --"), &validation) == 0,
	"native end-of-options validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_INCOMPLETE &&
	(validation.expected & BU_CMD_EXPECT_OPERAND),
	"native end-of-options marker should be valid syntax while awaiting its required object");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "hide tor.r", &analysis) == 0, "native hide analysis should succeed");
    {
	const struct ged_cmd_analysis_token *object_arg = token_matching(&analysis, "hide tor.r", "tor.r");
	CHECK(object_arg && object_arg->role == GED_CMD_TOKEN_OPERAND &&
	    object_arg->value_type == BU_CMD_VALUE_DB_OBJECT &&
	    object_arg->validator && BU_STR_EQUAL(object_arg->validator, "ged.db_object") &&
	    object_arg->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native hide operand should retain database-object semantic analysis");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "hide -- tor.r", &analysis) == 0,
	"native end-of-options analysis should succeed");
    {
	const struct ged_cmd_analysis_token *object_arg = token_matching(&analysis, "hide -- tor.r", "tor.r");
	CHECK(object_arg && object_arg->role == GED_CMD_TOKEN_OPERAND &&
	    object_arg->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native end-of-options marker should preserve following object operands");
    }
    ged_cmd_analysis_clear(&analysis);

    completion_count = ged_cmd_complete_result(gedp, "killall -n t", std::strlen("killall -n t"), &completion_result);
    CHECK(completion_result.completion_type == BU_CMD_VALUE_DB_OBJECT &&
	find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "tor.r") != NULL,
	"native options-first flags should preserve following object completion");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_analyze(gedp, "keep -R output.g tor.r", &analysis) == 0,
	"native keep analysis should succeed");
    {
	const struct ged_cmd_analysis_token *output = token_matching(&analysis, "keep -R output.g tor.r", "output.g");
	const struct ged_cmd_analysis_token *object_arg = token_matching(&analysis, "keep -R output.g tor.r", "tor.r");
	CHECK(output && output->role == GED_CMD_TOKEN_OPERAND &&
	    output->value_type == BU_CMD_VALUE_FILE && output->semantic_state == GED_CMD_SEMANTIC_VALID &&
	    object_arg && object_arg->role == GED_CMD_TOKEN_OPERAND &&
	    object_arg->value_type == BU_CMD_VALUE_DB_OBJECT && object_arg->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native keep should distinguish its file and database-object operands");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "remove all.g tor.r", &analysis) == 0,
	"native remove analysis should succeed");
    {
	const struct ged_cmd_analysis_token *combination = token_matching(&analysis, "remove all.g tor.r", "all.g");
	const struct ged_cmd_analysis_token *member = token_matching(&analysis, "remove all.g tor.r", "tor.r");
	CHECK(combination && member && combination->role == GED_CMD_TOKEN_OPERAND &&
	    member->role == GED_CMD_TOKEN_OPERAND &&
	    combination->semantic_state == GED_CMD_SEMANTIC_VALID &&
	    member->semantic_state == GED_CMD_SEMANTIC_VALID,
	    "native remove should validate each sequential database-object role");
    }
    ged_cmd_analysis_clear(&analysis);

    CHECK(ged_cmd_analyze(gedp, "search -type region", &analysis) == 0, "search analysis should succeed");
    {
	const struct ged_cmd_analysis_token *type_arg = token_matching(&analysis, "search -type region", "region");
	CHECK(type_arg != NULL, "search -type region should identify region token");
	CHECK(type_arg->role == GED_CMD_TOKEN_OPTION_ARG, "search region token should be an option argument");
	CHECK(type_arg->value_type == BU_CMD_VALUE_KEYWORD, "search region token should be a keyword");
	CHECK(type_arg->validator && BU_STR_EQUAL(type_arg->validator, "ged.search.type"), "search region token validator mismatch");
	CHECK(type_arg->semantic_state == GED_CMD_SEMANTIC_VALID, "search region token should be semantically valid");
    }

    CHECK(ged_cmd_analyze(gedp, "search -type notatype", &analysis) == 0, "bad search type analysis should succeed");
    {
	const struct ged_cmd_analysis_token *type_arg = token_matching(&analysis, "search -type notatype", "notatype");
	CHECK(type_arg != NULL, "search -type notatype should identify notatype token");
	CHECK(type_arg->semantic_state == GED_CMD_SEMANTIC_INVALID, "notatype should be semantically invalid");
    }

    completion_count = ged_cmd_complete_result(gedp, "search -exec dr", std::strlen("search -exec dr"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "draw") != NULL,
	    "search -exec should complete its embedded GED command");
    ged_cmd_completion_result_clear(&completion_result);

    completion_count = ged_cmd_complete_result(gedp, "search -exec draw a", std::strlen("search -exec draw a"), &completion_result);
    CHECK(find_completion(completion_result.completion_candidates, (int)completion_result.completion_count, "all.g") != NULL,
	    "search -exec should complete arguments using the embedded command schema");
    ged_cmd_completion_result_clear(&completion_result);

    CHECK(ged_cmd_analyze(gedp, "search -exec draw \"{}\" \";\"", &analysis) == 0,
	    "search -exec nested command analysis should succeed");
    CHECK(analysis.token_count == 5, "quoted search -exec syntax should retain command, substitution, and terminator tokens");
    CHECK(analysis.tokens[2].role == GED_CMD_TOKEN_COMMAND && analysis.tokens[2].semantic_state == GED_CMD_SEMANTIC_VALID,
	    "search -exec should recognize draw as a valid embedded command");
    CHECK(analysis.tokens[3].role == GED_CMD_TOKEN_OPERAND && analysis.tokens[3].semantic_state == GED_CMD_SEMANTIC_VALID,
	    "search -exec should accept a quoted result substitution as a draw operand");
    CHECK(analysis.tokens[3].char_end - analysis.tokens[3].char_start == 4,
	    "quoted search -exec substitution highlighting should include both quotes");
    CHECK(analysis.tokens[4].semantic_state == GED_CMD_SEMANTIC_VALID &&
	    analysis.tokens[4].hint && BU_STR_EQUAL(analysis.tokens[4].hint, "search -exec terminator"),
	    "search -exec should recognize a quoted standalone semicolon terminator");

    CHECK(ged_cmd_analyze(gedp, "search -exec draw --not-an-option {} ;", &analysis) == 0,
	    "invalid search -exec nested option analysis should succeed");
    CHECK(analysis.tokens[3].role == GED_CMD_TOKEN_OPTION && analysis.tokens[3].semantic_state == GED_CMD_SEMANTIC_INVALID,
	    "search -exec should reject options invalid for the embedded command");
    CHECK(analysis.tokens[4].semantic_state == GED_CMD_SEMANTIC_VALID,
	    "an exec substitution should remain valid when another embedded argument is invalid");

    CHECK(ged_cmd_analyze(gedp, "search -exec not_a_ged_command {} ;", &analysis) == 0,
	    "unknown search -exec command analysis should succeed");
    CHECK(analysis.tokens[2].role == GED_CMD_TOKEN_COMMAND && analysis.tokens[2].semantic_state == GED_CMD_SEMANTIC_INVALID,
	    "search -exec should reject an unknown embedded GED command");

    CHECK(ged_cmd_analyze(gedp, "search -exec draw {}", &analysis) == 0,
	    "unterminated search -exec analysis should succeed");
    CHECK(analysis.tokens[3].semantic_state == GED_CMD_SEMANTIC_INCOMPLETE,
	    "search -exec should report a missing standalone semicolon terminator");

    CHECK(ged_cmd_analyze(gedp, "search -exec adjust ;", &analysis) == 0,
	    "incomplete embedded command analysis should succeed");
    CHECK(analysis.tokens[2].semantic_state == GED_CMD_SEMANTIC_INCOMPLETE,
	    "search -exec should apply the embedded command's required-argument syntax");

    CHECK(ged_cmd_validate(gedp, "search -exec draw {} ;", std::strlen("search -exec draw {} ;"), &validation) == 0,
	    "terminated search -exec validation should succeed");
    CHECK(validation.state == BU_CMD_VALIDATE_VALID &&
	    validation.hint && BU_STR_EQUAL(validation.hint, "search -exec terminator"),
	    "search -exec should require and recognize a standalone semicolon terminator");
    ged_cmd_validate_result_clear(&validation);

    CHECK(ged_cmd_analyze(gedp, "search -exec draw \"\" \";\"", &analysis) == 0,
	    "empty quoted search -exec argument analysis should succeed");
    CHECK(analysis.token_count == 4,
	    "GSH-compatible parsing should stop at an empty quoted string exactly as command execution does");
    CHECK(analysis.tokens[3].char_end - analysis.tokens[3].char_start == 2 &&
	    analysis.tokens[3].semantic_state == GED_CMD_SEMANTIC_INVALID,
	    "an empty quoted exec argument should be highlighted as an invalid draw path");

    CHECK(ged_cmd_analyze(gedp, "search -exec draw {} ; -and -type region", &analysis) == 0,
	    "search expression analysis should resume after an exec terminator");
    CHECK(analysis.tokens[5].role == GED_CMD_TOKEN_OPTION && analysis.tokens[5].semantic_state == GED_CMD_SEMANTIC_VALID,
	    "search should parse expression operators following -exec");
    CHECK(analysis.tokens[7].role == GED_CMD_TOKEN_OPTION_ARG && analysis.tokens[7].semantic_state == GED_CMD_SEMANTIC_VALID,
	    "search should validate expression arguments following -exec");

    if (ac == 3) {
	const char *expected_m35_order[] = {
	    "r200", "r201", "r203", "r205", "r206",
	    "r207", "r208", "r209", "r210", "r211"
	};

	ged_close(gedp);
	gedp = NULL;

	if (!bu_file_exists(av[2], NULL)) {
	    bu_log("ERROR: [%s] does not exist, expecting optional m35 .g file\n", av[2]);
	    goto cleanup;
	}

	gedp = ged_open("db", av[2], 1);
	CHECK(gedp != NULL, "ged_open failed for optional m35 database");
	{
	    const char *brep_argv[] = {"brep", "s52", "brep", "brep_completion", NULL};
	    CHECK(ged_exec(gedp, 4, brep_argv) == BRLCAD_OK,
		"m35 should provide a primitive that can be converted to BREP for plot completion tests");
	    completion_count = ged_cmd_complete_result(gedp,
		"brep brep_completion plot F ",
		std::strlen("brep brep_completion plot F "), &completion_result);
	    CHECK(completion_count > 0 &&
		find_completion(completion_result.completion_candidates,
		    (int)completion_result.completion_count, "0") != NULL,
		"brep plot should offer indices from the selected BREP face collection");
	    ged_cmd_completion_result_clear(&completion_result);
	    {
		const char *plot_modes[] = {"E", "L", "S", NULL};
		for (const char **mode = plot_modes; *mode; mode++) {
		    std::string plot_input = std::string("brep brep_completion plot ") + *mode + " ";
		    completion_count = ged_cmd_complete_result(gedp, plot_input.c_str(),
			plot_input.size(), &completion_result);
		    CHECK(completion_count > 0 &&
			find_completion(completion_result.completion_candidates,
			    (int)completion_result.completion_count, "0") != NULL,
			"brep plot should map each topological mode to its component collection");
		    ged_cmd_completion_result_clear(&completion_result);
		}
	    }
	    CHECK(ged_cmd_validate(gedp, "brep brep_completion plot F 999999",
		std::strlen("brep brep_completion plot F 999999"), &validation) == 0 &&
		validation.state == BU_CMD_VALIDATE_INVALID,
		"brep plot should reject an out-of-range face index");
	    ged_cmd_validate_result_clear(&validation);
	    CHECK(ged_cmd_validate(gedp, "brep brep_completion plot F 0",
		std::strlen("brep brep_completion plot F 0"), &validation) == 0 &&
		validation.state == BU_CMD_VALIDATE_VALID,
		"brep plot should accept an in-range face index");
	    ged_cmd_validate_result_clear(&validation);
	    {
		const char *bad_plot_argv[] = {"brep", "brep_completion", "plot", "F", "999999", NULL};
		CHECK(ged_exec(gedp, 5, bad_plot_argv) != BRLCAD_OK,
		    "brep plot execution should reject an out-of-range component index");
	    }
	}
	CHECK(check_completion_order(gedp, "draw component/suspension/r2",
		    expected_m35_order, sizeof(expected_m35_order) / sizeof(expected_m35_order[0])),
		"m35 suspension path completions should be alphanum sorted");
    }

    ret = 0;

cleanup:
    if (argv_completion_count > 0 && completions)
	bu_argv_free(argv_completion_count, (char **)completions);
    ged_cmd_validate_result_clear(&validation);
    ged_cmd_completion_result_clear(&completion_result);
    bu_vls_free(&prefix);
    bu_vls_free(&lint_msgs);
    if (schema_json)
	bu_free(schema_json, "schema json");
    if (tree_help)
	bu_free(tree_help, "native tree help");
    ged_cmd_analysis_clear(&analysis);
    if (gedp)
	ged_close(gedp);

    return ret;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
