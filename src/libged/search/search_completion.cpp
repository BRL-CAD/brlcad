/* Command-owned completion and grammar adapter for search. */
#include "common.h"
#include <algorithm>
#include <set>
#include <string>
#include <vector>
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "ged.h"
#include "rt/search.h"
#include "rt/functab.h"
#include "../alphanum.h"
#include "../ged_private.h"
#include "../tab_complete_private.h"

static std::string ged_cursor_seed_local(const struct ged_input_parse &parsed)
{
    if (parsed.cursor_arg < parsed.argc && parsed.argv && parsed.argv[parsed.cursor_arg])
        return std::string(parsed.argv[parsed.cursor_arg]);
    return std::string();
}

static void ged_analysis_set_span_local(struct ged_cmd_analysis_token *token, const struct ged_input_parse &parsed, size_t idx)
{
    token->token_start = idx;
    token->token_end = idx;
    token->char_start = parsed.char_starts[idx];
    token->char_end = parsed.char_ends[idx];
}

static void ged_set_result_chars_local(struct ged_cmd_validate_result *result, const struct ged_input_parse &parsed, size_t token_start, size_t token_end)
{
    if (!result) return;
    if (token_start < parsed.argc) {
        size_t end_index = (token_end < parsed.argc) ? token_end : parsed.argc - 1;
        result->char_start = parsed.char_starts[token_start];
        result->char_end = parsed.char_ends[end_index];
    } else {
        result->char_start = parsed.input_len;
        result->char_end = parsed.input_len;
    }
}

static void ged_set_validate_result_local(struct ged_cmd_validate_result *result, bu_cmd_validate_state_t state, size_t token_start, size_t token_end, unsigned int expected, const char *hint)
{
    ged_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token_start;
    result->token_end = token_end;
    result->expected = expected;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_UNKNOWN;
}

static void ged_fill_command_candidates_local(struct ged_cmd_validate_result *result, const std::string &seed)
{
    const char **completions = NULL;
    int cnt = ged_cmd_completions(&completions, seed.c_str());
    if (cnt > 0) {
        result->completion_candidates = completions;
        result->completion_count = (size_t)cnt;
    }
}

static void ged_fill_geometry_candidates_local(struct ged *gedp, const std::string &seed, struct ged_cmd_validate_result *result)
{
    if (!gedp || !gedp->dbip || !result) return;
    const char **completions = NULL;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    int cnt = ged_geom_completions(&completions, &prefix, gedp->dbip, seed.c_str());
    if (cnt > 0) {
        result->completion_candidates = completions;
        result->completion_count = (size_t)cnt;
        result->completion_type = BU_CMD_VALUE_DB_PATH;
    } else if (completions) {
        bu_argv_free((size_t)cnt, (char **)completions);
    }
    bu_vls_free(&prefix);
}

static int
ged_search_is_plan_start(const char *arg)
{
    return db_search_syntax_plan_start(arg);
}

static int
ged_search_is_top_option(const char *arg)
{
    return ged_search_top_option_parse(arg, NULL, NULL);
}

static const struct db_search_syntax_term *
ged_search_find_plan_op(const char *arg)
{
    size_t count = 0;
    const struct db_search_syntax_term *terms = db_search_syntax_terms(&count);

    if (!arg)
	return NULL;
    for (size_t i = 0; i < count; i++) {
	if (BU_STR_EQUAL(arg, terms[i].name))
	    return &terms[i];
    }
    return NULL;
}

static int
ged_search_plan_op_takes_arg(const struct db_search_syntax_term *term)
{
    return term && term->argument != DB_SEARCH_SYNTAX_NO_ARGUMENT;
}

static bu_cmd_value_t
ged_search_plan_arg_type(const struct db_search_syntax_term *term)
{
    if (!term)
	return BU_CMD_VALUE_UNKNOWN;
    switch (term->argument) {
	case DB_SEARCH_SYNTAX_INTEGER_ARGUMENT: return BU_CMD_VALUE_INTEGER;
	case DB_SEARCH_SYNTAX_TYPE_ARGUMENT: return BU_CMD_VALUE_KEYWORD;
	case DB_SEARCH_SYNTAX_EXEC_ARGUMENTS: return BU_CMD_VALUE_RAW;
	case DB_SEARCH_SYNTAX_STRING_ARGUMENT: return BU_CMD_VALUE_STRING;
	case DB_SEARCH_SYNTAX_NO_ARGUMENT:
	default: return BU_CMD_VALUE_UNKNOWN;
    }
}

static const char *
ged_search_plan_validator(const struct db_search_syntax_term *term)
{
    return (term && term->argument == DB_SEARCH_SYNTAX_TYPE_ARGUMENT) ? "ged.search.type" : NULL;
}

static int
ged_search_prefix_match(const std::string &candidate, const std::string &prefix)
{
    if (prefix.empty())
	return 1;
    if (candidate.size() < prefix.size())
	return 0;
    return (candidate.compare(0, prefix.size(), prefix) == 0);
}

static void
ged_search_add_candidate(std::vector<std::string> &candidates, const char *candidate, const std::string &prefix)
{
    if (!candidate)
	return;
    std::string c(candidate);
    if (!ged_search_prefix_match(c, prefix))
	return;
    if (std::find(candidates.begin(), candidates.end(), c) == candidates.end())
	candidates.push_back(c);
}

static void
ged_search_set_candidates(struct ged_cmd_validate_result *result,
			  std::vector<std::string> &candidates,
			  bu_cmd_value_t completion_type)
{
    if (!result || candidates.empty())
	return;

    std::sort(candidates.begin(), candidates.end(), [](const std::string &a, const std::string &b) {
	return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
    });

    result->completion_count = candidates.size();
    result->completion_candidates = (const char **)bu_calloc(result->completion_count + 1, sizeof(char *), "search completion candidates");
    for (size_t i = 0; i < candidates.size(); i++)
	result->completion_candidates[i] = bu_strdup(candidates[i].c_str());
    result->completion_type = completion_type;
}

static void
ged_search_collect_top_options(std::vector<std::string> &candidates, const std::string &prefix)
{
    size_t count = 0;
    const struct ged_search_top_option *options = ged_search_top_options(&count);

    for (size_t i = 0; i < count; i++)
	ged_search_add_candidate(candidates, options[i].name, prefix);
}

static void
ged_search_collect_plan_ops(std::vector<std::string> &candidates, const std::string &prefix)
{
    size_t count = 0;
    const struct db_search_syntax_term *terms = db_search_syntax_terms(&count);

    for (size_t i = 0; i < count; i++)
	ged_search_add_candidate(candidates, terms[i].name, prefix);
}

static std::vector<std::string>
ged_search_completion_type_keywords(int include_aliases)
{
    std::set<std::string> keywords;
    const char *abstract_types[] = {
	"arb4", "arb5", "arb6", "arb7", "arb8", "combination", "plate",
	"region", "shape", "sphere", "volume", NULL
    };
    const char *aliases[] = {"c", "comb", "r", "reg", "sph", NULL};
    for (size_t i = 0; abstract_types[i]; i++) keywords.insert(abstract_types[i]);
    for (int i = 1; i <= ID_MAX_SOLID; i++) {
	if (i != ID_UNUSED1 && i != ID_UNUSED2 && OBJ[i].ft_label[0] &&
	    !BU_STR_EQUAL(OBJ[i].ft_label, "ID_NULL") && OBJ[i].ft_label[0] != '>')
	    keywords.insert(OBJ[i].ft_label);
    }
    if (include_aliases)
	for (size_t i = 0; aliases[i]; i++) keywords.insert(aliases[i]);
    return std::vector<std::string>(keywords.begin(), keywords.end());
}

static int
ged_search_completion_is_type_keyword(const char *token)
{
    if (!token) return 0;
    std::vector<std::string> keywords = ged_search_completion_type_keywords(1);
    for (size_t i = 0; i < keywords.size(); i++)
	if (BU_STR_EQUAL(token, keywords[i].c_str())) return 1;
    return 0;
}

static int
ged_search_completion_type_has_prefix(const char *token)
{
    std::string prefix = token ? token : "";
    if (prefix.empty()) return 1;
    std::vector<std::string> keywords = ged_search_completion_type_keywords(1);
    for (size_t i = 0; i < keywords.size(); i++)
	if (ged_search_prefix_match(keywords[i], prefix)) return 1;
    return 0;
}

static void
ged_search_completion_collect_type_candidates(std::vector<std::string> &candidates, const std::string &prefix)
{
    std::vector<std::string> keywords = ged_search_completion_type_keywords(0);
    for (size_t i = 0; i < keywords.size(); i++)
	ged_search_add_candidate(candidates, keywords[i].c_str(), prefix);
}

static int
ged_search_is_bool_keyword(const char *token)
{
    return token && (BU_STR_EQUAL(token, "u") || BU_STR_EQUAL(token, "U") ||
	BU_STR_EQUAL(token, "+") || BU_STR_EQUAL(token, "-"));
}

static void
ged_search_collect_bool_candidates(std::vector<std::string> &candidates, const std::string &prefix)
{
    /* Keep the canonical spellings used by the search parser.  Uppercase U
     * is accepted as an input alias, but completion should converge on u. */
    const char *bool_keywords[] = {"u", "+", "-", NULL};
    for (size_t i = 0; bool_keywords[i]; i++)
	ged_search_add_candidate(candidates, bool_keywords[i], prefix);
}

static ged_cmd_semantic_state_t
ged_search_path_state(struct ged *gedp, const char *token)
{
    if (!token || BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (BU_STR_EQUAL(token, "/") || BU_STR_EQUAL(token, ".") || BU_STR_EQUAL(token, "|"))
	return GED_CMD_SEMANTIC_VALID;
    if (token[0] == '|') {
	if (!token[1])
	    return GED_CMD_SEMANTIC_VALID;
	token++;
    }
    if (!gedp || !gedp->dbip)
	return GED_CMD_SEMANTIC_INVALID;
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    int ret = db_full_path_decode(&path, gedp->dbip, token);
    db_free_full_path(&path);
    return ret == DB_FULL_PATH_OK ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

struct ged_search_parse_state {
    int in_plan = 0;
    int path_seen = 0;
    int in_exec = 0;
    size_t exec_start = (size_t)-1;
    const struct db_search_syntax_term *pending_arg = NULL;
};

static void
ged_search_state_consume(struct ged_search_parse_state *state, const char *arg, size_t arg_index)
{
    const struct db_search_syntax_term *op = NULL;

    if (!state || !arg)
	return;

    if (state->in_exec) {
	if (BU_STR_EQUAL(arg, ";")) {
	    state->in_exec = 0;
	    state->exec_start = (size_t)-1;
	}
	return;
    }

    if (state->pending_arg) {
	if (BU_STR_EQUAL(state->pending_arg->name, "-exec") && !BU_STR_EQUAL(arg, ";")) {
	    state->in_exec = 1;
	    state->exec_start = arg_index;
	}
	state->pending_arg = NULL;
	return;
    }

    if (!state->in_plan) {
	if (!state->path_seen && ged_search_is_top_option(arg))
	    return;
	if (!ged_search_is_plan_start(arg)) {
	    state->path_seen = 1;
	    return;
	}
	state->in_plan = 1;
    }

    op = ged_search_find_plan_op(arg);
	if (ged_search_plan_op_takes_arg(op))
	state->pending_arg = op;
}

static std::string
ged_argv_range_line_local(const struct ged_input_parse &parsed, size_t start, size_t end, int trailing_space = 0)
{
    std::string line;

    if (end > parsed.argc)
	end = parsed.argc;
    for (size_t i = start; i < end; i++) {
	if (!line.empty())
	    line.push_back(' ');
	line.push_back('"');
	for (const char *cp = parsed.argv[i]; cp && *cp; cp++) {
	    if (*cp == '\\' || *cp == '"')
		line.push_back('\\');
	    line.push_back(*cp);
	}
	line.push_back('"');
    }
    if (trailing_space && !line.empty())
	line.push_back(' ');
    return line;
}

static int
ged_search_validate(struct ged *gedp, const struct ged_input_parse &parsed, const std::string &seed, struct ged_cmd_validate_result *result)
{
    struct ged_search_parse_state state;
    const struct db_search_syntax_term *current_op = NULL;
    std::vector<std::string> candidates;
    size_t token_index = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg : parsed.argc;
    const char *arg = (parsed.cursor_arg < parsed.argc) ? parsed.argv[parsed.cursor_arg] : NULL;

    for (size_t i = 1; i < parsed.cursor_arg && i < parsed.argc; i++)
	ged_search_state_consume(&state, parsed.argv[i], i);

    if (state.pending_arg) {
	ged_set_validate_result_local(result, BU_CMD_VALIDATE_INCOMPLETE, token_index, token_index,
		BU_CMD_EXPECT_OPTION_ARG, "search expression argument expected");
	result->completion_type = ged_search_plan_arg_type(state.pending_arg);
	if (BU_STR_EQUAL(state.pending_arg->name, "-exec")) {
	    ged_fill_command_candidates_local(result, seed);
	    result->expected = BU_CMD_EXPECT_SUBCOMMAND;
	    result->hint = "search -exec GED command expected";
	    if (arg && ged_cmd_exists(arg))
		result->state = BU_CMD_VALIDATE_VALID;
	    else if (arg && !result->completion_count)
		result->state = BU_CMD_VALIDATE_INVALID;
	    ged_set_result_chars_local(result, parsed, token_index, token_index);
	    return 1;
	}
	if (ged_search_plan_validator(state.pending_arg))
	    result->hint = ged_search_plan_validator(state.pending_arg);
	if (BU_STR_EQUAL(state.pending_arg->name, "-type")) {
	    ged_search_completion_collect_type_candidates(candidates, seed);
	    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_KEYWORD);
	    if (arg && ged_search_completion_is_type_keyword(arg))
		result->state = BU_CMD_VALIDATE_VALID;
	    else if (arg && !ged_search_completion_type_has_prefix(arg))
		result->state = BU_CMD_VALIDATE_INVALID;
	} else if (BU_STR_EQUAL(state.pending_arg->name, "-bool")) {
	    ged_search_collect_bool_candidates(candidates, seed);
	    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_KEYWORD);
	    result->completion_type = BU_CMD_VALUE_KEYWORD;
	    if (arg && ged_search_is_bool_keyword(arg))
		result->state = BU_CMD_VALIDATE_VALID;
	    else if (arg && candidates.empty())
		result->state = BU_CMD_VALIDATE_INVALID;
	}
	ged_set_result_chars_local(result, parsed, token_index, token_index);
	return 1;
    }

    if (state.in_exec) {
	if (arg && BU_STR_EQUAL(arg, ";")) {
	    ged_set_validate_result_local(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		    BU_CMD_EXPECT_OPTION, "search -exec terminator");
	    result->completion_type = BU_CMD_VALUE_RAW;
	    ged_set_result_chars_local(result, parsed, token_index, token_index);
	    return 1;
	}

	struct ged_cmd_validate_result nested = GED_CMD_VALIDATE_RESULT_NULL;
	size_t nested_end = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg + 1 : parsed.argc;
	int trailing_space = (parsed.cursor_arg >= parsed.argc && seed.empty()) ? 1 : 0;
	std::string nested_line = ged_argv_range_line_local(parsed, state.exec_start, nested_end, trailing_space);
	(void)ged_cmd_validate(gedp, nested_line.c_str(), nested_line.size(), &nested);

	ged_set_validate_result_local(result, nested.state, token_index, token_index,
		nested.expected, nested.hint ? nested.hint : "search -exec command argument");
	result->completion_type = nested.completion_type;
	result->completion_count = nested.completion_count;
	result->completion_candidates = nested.completion_candidates;
	nested.completion_count = 0;
	nested.completion_candidates = NULL;
	if (arg && db_search_syntax_exec_substitution(arg) &&
		(result->completion_type == BU_CMD_VALUE_DB_OBJECT || result->completion_type == BU_CMD_VALUE_DB_PATH))
	    result->state = BU_CMD_VALIDATE_VALID;
	if (trailing_space && result->state == BU_CMD_VALIDATE_VALID) {
	    result->state = BU_CMD_VALIDATE_INCOMPLETE;
	    result->expected = BU_CMD_EXPECT_OPERAND;
	    result->hint = "search -exec requires a standalone ; terminator";
	}
	ged_set_result_chars_local(result, parsed, token_index, token_index);
	ged_cmd_validate_result_clear(&nested);
	return 1;
    }

    if (!state.in_plan) {
	if (seed.empty() || ged_search_is_plan_start(seed.c_str())) {
	    ged_set_validate_result_local(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		    BU_CMD_EXPECT_OPTION | BU_CMD_EXPECT_OPERAND, "search path or expression expected");
	    if (!state.path_seen)
		ged_search_collect_top_options(candidates, seed);
	    ged_search_collect_plan_ops(candidates, seed);
	    if (!seed.empty() && candidates.empty()) {
		result->state = BU_CMD_VALIDATE_INVALID;
		result->hint = "unknown search expression operator";
	    }
	    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
	    ged_set_result_chars_local(result, parsed, token_index, token_index);
	    return 1;
	}

	ged_set_validate_result_local(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		BU_CMD_EXPECT_OPERAND, "search path expected");
	result->completion_type = BU_CMD_VALUE_DB_PATH;
	ged_fill_geometry_candidates_local(gedp, seed, result);
	if (arg && ged_search_path_state(gedp, arg) == GED_CMD_SEMANTIC_INVALID)
	    result->state = BU_CMD_VALIDATE_INVALID;
	ged_set_result_chars_local(result, parsed, token_index, token_index);
	return 1;
    }

    current_op = arg ? ged_search_find_plan_op(arg) : NULL;
    if (arg && ged_search_is_plan_start(arg) && !current_op && !candidates.size()) {
	ged_set_validate_result_local(result, BU_CMD_VALIDATE_INVALID, token_index, token_index,
		BU_CMD_EXPECT_OPTION, "unknown search expression operator");
	ged_search_collect_plan_ops(candidates, seed);
	ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
	ged_set_result_chars_local(result, parsed, token_index, token_index);
	return 1;
    }

    ged_set_validate_result_local(result, current_op ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE,
	    token_index, token_index, BU_CMD_EXPECT_OPTION, "search expression operator expected");
    ged_search_collect_plan_ops(candidates, seed);
    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
    ged_set_result_chars_local(result, parsed, token_index, token_index);
    return 1;
}

static int
ged_search_analyze(struct ged *gedp, const struct ged_input_parse &parsed, struct ged_cmd_analysis *analysis)
{
    struct ged_search_parse_state state;

    if (!analysis || !analysis->tokens)
	return 0;

    for (size_t i = 1; i < parsed.argc; i++) {
	const char *arg = parsed.argv[i];
	struct ged_cmd_analysis_token *token = &analysis->tokens[i];

	if (state.pending_arg) {
	    if (BU_STR_EQUAL(state.pending_arg->name, "-exec")) {
		size_t exec_end = i;
		while (exec_end < parsed.argc && !BU_STR_EQUAL(parsed.argv[exec_end], ";"))
		    exec_end++;

		if (exec_end > i) {
		    struct ged_cmd_analysis nested = {0, NULL};
		    std::string nested_line = ged_argv_range_line_local(parsed, i, exec_end);
		    (void)ged_cmd_analyze(gedp, nested_line.c_str(), &nested);
		    size_t nested_count = std::min(nested.token_count, exec_end - i);
		    for (size_t ni = 0; ni < nested_count; ni++) {
			struct ged_cmd_analysis_token *outer = &analysis->tokens[i + ni];
			outer->role = nested.tokens[ni].role;
			outer->value_type = nested.tokens[ni].value_type;
			outer->semantic_state = nested.tokens[ni].semantic_state;
			outer->validator = nested.tokens[ni].validator;
			outer->hint = nested.tokens[ni].hint;
			if (db_search_syntax_exec_substitution(parsed.argv[i + ni]) &&
				(outer->role == GED_CMD_TOKEN_OPERAND || outer->role == GED_CMD_TOKEN_OPTION_ARG)) {
			    outer->semantic_state = GED_CMD_SEMANTIC_VALID;
			    outer->hint = "search -exec substitution argument";
			}
		    }
		    if (exec_end < parsed.argc && nested_count > 0) {
			struct ged_cmd_validate_result nested_validation = GED_CMD_VALIDATE_RESULT_NULL;
			std::string completed_line = nested_line + " ";
			(void)ged_cmd_validate(gedp, completed_line.c_str(), completed_line.size(), &nested_validation);
			if (nested_validation.state == BU_CMD_VALIDATE_INCOMPLETE) {
			    struct ged_cmd_analysis_token *last = &analysis->tokens[exec_end - 1];
			    if (last->semantic_state != GED_CMD_SEMANTIC_INVALID) {
				last->semantic_state = GED_CMD_SEMANTIC_INCOMPLETE;
				last->hint = "incomplete search -exec GED command";
			    }
			}
			ged_cmd_validate_result_clear(&nested_validation);
		    }
		    ged_cmd_analysis_clear(&nested);
		    if (exec_end == parsed.argc &&
			    analysis->tokens[exec_end - 1].semantic_state != GED_CMD_SEMANTIC_INVALID) {
			analysis->tokens[exec_end - 1].semantic_state = GED_CMD_SEMANTIC_INCOMPLETE;
			analysis->tokens[exec_end - 1].hint = "search -exec requires a standalone ; terminator";
		    }
		}

		if (exec_end < parsed.argc) {
		    struct ged_cmd_analysis_token *terminator = &analysis->tokens[exec_end];
		    terminator->role = GED_CMD_TOKEN_OPERAND;
		    terminator->value_type = BU_CMD_VALUE_RAW;
		    terminator->semantic_state = (exec_end == i) ? GED_CMD_SEMANTIC_INCOMPLETE : GED_CMD_SEMANTIC_VALID;
		    terminator->hint = (exec_end == i) ? "search -exec GED command expected" : "search -exec terminator";
		}
		state.pending_arg = NULL;
		i = exec_end;
		continue;
	    }

	    token->role = GED_CMD_TOKEN_OPTION_ARG;
	    token->value_type = ged_search_plan_arg_type(state.pending_arg);
	    token->validator = ged_search_plan_validator(state.pending_arg);
	    token->hint = "search expression argument";
	    if (BU_STR_EQUAL(state.pending_arg->name, "-type")) {
		token->semantic_state = ged_search_completion_is_type_keyword(arg) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
	    } else {
		token->semantic_state = GED_CMD_SEMANTIC_UNKNOWN;
	    }
	    state.pending_arg = NULL;
	    continue;
	}

	if (!state.in_plan) {
	    if (!state.path_seen && ged_search_is_top_option(arg)) {
		token->role = GED_CMD_TOKEN_OPTION;
		token->value_type = BU_CMD_VALUE_BOOL;
		token->semantic_state = GED_CMD_SEMANTIC_VALID;
		token->hint = "search option";
		continue;
	    }
	    if (!ged_search_is_plan_start(arg)) {
		token->role = GED_CMD_TOKEN_OPERAND;
		token->value_type = BU_CMD_VALUE_DB_PATH;
		token->validator = "ged.search.path";
		token->semantic_state = ged_search_path_state(gedp, arg);
		token->hint = "search path";
		state.path_seen = 1;
		continue;
	    }
	    state.in_plan = 1;
	}

	const struct db_search_syntax_term *op = ged_search_find_plan_op(arg);
	token->role = GED_CMD_TOKEN_OPTION;
	token->value_type = BU_CMD_VALUE_UNKNOWN;
	token->semantic_state = op ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
	token->hint = op ? "search expression operator" : "unknown search expression operator";
	if (ged_search_plan_op_takes_arg(op))
	    state.pending_arg = op;
    }

    return 1;
}

static int
ged_search_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    std::string seed;
    int ret = 0;

    if (!input || !result)
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    seed = ged_cursor_seed_local(parsed);
    ret = ged_search_validate(gedp, parsed, seed, result);
    ged_input_parse_free(&parsed);
    return ret < 0 ? ret : 0;
}

static int
ged_search_grammar_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;
    int ret = 0;

    if (!input || !analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }

    analysis->tokens = (struct ged_cmd_analysis_token *)bu_calloc(parsed.argc,
	sizeof(struct ged_cmd_analysis_token), "search grammar analysis tokens");
    analysis->token_count = parsed.argc;
    for (size_t i = 0; i < parsed.argc; i++) {
	ged_analysis_set_span_local(&analysis->tokens[i], parsed, i);
	analysis->tokens[i].role = GED_CMD_TOKEN_UNKNOWN;
	analysis->tokens[i].value_type = BU_CMD_VALUE_UNKNOWN;
	analysis->tokens[i].semantic_state = GED_CMD_SEMANTIC_UNKNOWN;
	analysis->tokens[i].validator = NULL;
	analysis->tokens[i].hint = NULL;
    }
    analysis->tokens[0].role = GED_CMD_TOKEN_COMMAND;
    analysis->tokens[0].semantic_state = ged_cmd_exists(parsed.argv[0]) ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
    analysis->tokens[0].hint = analysis->tokens[0].semantic_state == GED_CMD_SEMANTIC_VALID ?
	"valid command" : "unknown command";

    ret = ged_search_analyze(gedp, parsed, analysis);
    ged_input_parse_free(&parsed);
    return ret < 0 ? ret : 0;
}

static const char *
ged_search_syntax_argument_name(db_search_syntax_arg_t argument)
{
    switch (argument) {
	case DB_SEARCH_SYNTAX_STRING_ARGUMENT: return "string";
	case DB_SEARCH_SYNTAX_INTEGER_ARGUMENT: return "integer";
	case DB_SEARCH_SYNTAX_TYPE_ARGUMENT: return "type";
	case DB_SEARCH_SYNTAX_EXEC_ARGUMENTS: return "exec";
	case DB_SEARCH_SYNTAX_NO_ARGUMENT:
	default: return "none";
    }
}

static char *
ged_search_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    size_t count = 0;
    size_t option_count = 0;
    const struct db_search_syntax_term *terms = db_search_syntax_terms(&count);
    const struct ged_search_top_option *options = ged_search_top_options(&option_count);

    bu_vls_strcat(&out, "{\"name\":\"search\",\"kind\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"parse_policy\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"help\":\"Search database objects\",\"options\":[");
    for (size_t i = 0; i < option_count; i++) {
	if (i)
	    bu_vls_putc(&out, ',');
	bu_vls_printf(&out, "{\"name\":\"%s\",\"argument\":\"none\",\"repeatable\":%s}",
		options[i].name, options[i].kind == GED_SEARCH_TOP_OPTION_VERBOSE ? "true" : "false");
    }
    bu_vls_strcat(&out, "],\"terms\":[");
    for (size_t i = 0; i < count; i++) {
	if (i)
	    bu_vls_putc(&out, ',');
	bu_vls_printf(&out, "{\"name\":\"%s\",\"argument\":\"%s\"}",
		terms[i].name, ged_search_syntax_argument_name(terms[i].argument));
    }
    bu_vls_strcat(&out, "]}");
    return bu_vls_strdup(&out);
}

static int
ged_search_grammar_lint(struct bu_vls *msgs)
{
    int failures = 0;
    int has_exec = 0;
    int has_type = 0;
    size_t count = 0;
    size_t option_count = 0;
    const struct db_search_syntax_term *terms = db_search_syntax_terms(&count);
    const struct ged_search_top_option *options = ged_search_top_options(&option_count);
    std::set<std::string> names;

    if (!terms || !count) {
	if (msgs)
	    bu_vls_strcat(msgs, "search: parser-owned syntax vocabulary is empty\n");
	return 1;
    }
    for (size_t i = 0; i < count; i++) {
	if (BU_STR_EMPTY(terms[i].name) || !names.insert(std::string(terms[i].name)).second) {
	    if (msgs)
		bu_vls_printf(msgs, "search: empty or duplicate parser-owned term \"%s\"\n",
		    terms[i].name ? terms[i].name : "");
	    failures++;
	}
	if (BU_STR_EQUAL(terms[i].name, "-exec"))
	    has_exec = terms[i].argument == DB_SEARCH_SYNTAX_EXEC_ARGUMENTS;
	if (BU_STR_EQUAL(terms[i].name, "-type"))
	    has_type = terms[i].argument == DB_SEARCH_SYNTAX_TYPE_ARGUMENT;
    }
    if (!has_exec || !has_type) {
	if (msgs)
	    bu_vls_strcat(msgs, "search: parser-owned syntax vocabulary lacks typed -exec or -type\n");
	failures++;
    }

    names.clear();
    if (!options || !option_count) {
	if (msgs)
	    bu_vls_strcat(msgs, "search: command-owned prefix vocabulary is empty\n");
	return failures + 1;
    }
    for (size_t i = 0; i < option_count; i++) {
	enum ged_search_top_option_kind kind = GED_SEARCH_TOP_OPTION_UNKNOWN;
	size_t occurrences = 0;
	if (BU_STR_EMPTY(options[i].name) || !names.insert(std::string(options[i].name)).second ||
		!ged_search_top_option_parse(options[i].name, &kind, &occurrences) ||
		kind != options[i].kind || occurrences != 1) {
	    if (msgs)
		bu_vls_printf(msgs, "search: malformed or duplicate command-owned option \"%s\"\n",
			options[i].name ? options[i].name : "");
	    failures++;
	}
    }
    return failures;
}

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_search_grammar = {
    "search", "Search database objects", ged_search_grammar_validate,
    ged_search_grammar_analyze, ged_search_grammar_json, ged_search_grammar_lint
};
