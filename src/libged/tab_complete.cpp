/*                  T A B _ C O M P L E T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022-2026 United States Government as represented by
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
/** @file tab_complete.cpp
 *
 * Facilities for constructing automatic completions of partial command and/or
 * object name inputs supplied by applications.
 */

#include "common.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "./alphanum.h"
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bu/str.h"
#include "ged.h"
#include "rt/functab.h"
#include "rt/search.h"

static int ged_search_is_type_keyword(const char *token);

static const char *
ged_native_semantic_provider(bu_cmd_value_t value_type, const char *semantic_provider)
{
    if (!BU_STR_EMPTY(semantic_provider))
	return semantic_provider;

    switch (value_type) {
	case BU_CMD_VALUE_DB_OBJECT: return "ged.db_object";
	case BU_CMD_VALUE_DB_PATH: return "ged.db_path";
	case BU_CMD_VALUE_FILE: return "ged.file_path";
	case BU_CMD_VALUE_COLOR: return "ged.color";
	case BU_CMD_VALUE_MATRIX: return "ged.matrix";
	case BU_CMD_VALUE_VECTOR: return "ged.vector";
	default: break;
    }
    return NULL;
}

static bu_cmd_value_t
ged_native_value_type(bu_cmd_value_t value_type, const char *semantic_provider)
{
    if (semantic_provider) {
	if (BU_STR_EQUAL(semantic_provider, "ged.db_object")) return BU_CMD_VALUE_DB_OBJECT;
	if (BU_STR_EQUAL(semantic_provider, "ged.db_path") ||
	    BU_STR_EQUAL(semantic_provider, "ged.search.path") ||
	    BU_STR_EQUAL(semantic_provider, "ged.db_path_or_pattern")) return BU_CMD_VALUE_DB_PATH;
	if (BU_STR_EQUAL(semantic_provider, "ged.file_path")) return BU_CMD_VALUE_FILE;
	if (BU_STR_EQUAL(semantic_provider, "ged.primitive_type") ||
	    BU_STR_EQUAL(semantic_provider, "ged.display_mode") ||
	    BU_STR_EQUAL(semantic_provider, "ged.unit")) return BU_CMD_VALUE_KEYWORD;
    }
    switch (value_type) {
	case BU_CMD_VALUE_FLAG:
	case BU_CMD_VALUE_BOOL:
	    return BU_CMD_VALUE_BOOL;
	case BU_CMD_VALUE_INTEGER:
	    return BU_CMD_VALUE_INTEGER;
	case BU_CMD_VALUE_NUMBER:
	    return BU_CMD_VALUE_NUMBER;
	case BU_CMD_VALUE_VECTOR:
	    return BU_CMD_VALUE_VECTOR;
	case BU_CMD_VALUE_MATRIX:
	    return BU_CMD_VALUE_MATRIX;
	case BU_CMD_VALUE_COLOR:
	    return BU_CMD_VALUE_COLOR;
	case BU_CMD_VALUE_KEYWORD:
	    return BU_CMD_VALUE_KEYWORD;
	case BU_CMD_VALUE_DB_OBJECT:
	    return BU_CMD_VALUE_DB_OBJECT;
	case BU_CMD_VALUE_DB_PATH:
	    return BU_CMD_VALUE_DB_PATH;
	case BU_CMD_VALUE_FILE:
	    return BU_CMD_VALUE_FILE;
	case BU_CMD_VALUE_RAW:
	    return BU_CMD_VALUE_RAW;
	case BU_CMD_VALUE_CUSTOM:
	case BU_CMD_VALUE_STRING:
	default:
	    return BU_CMD_VALUE_STRING;
    }
}

static bu_cmd_validate_state_t
ged_native_state(bu_cmd_validate_state_t state)
{
    switch (state) {
	case BU_CMD_VALIDATE_VALID: return BU_CMD_VALIDATE_VALID;
	case BU_CMD_VALIDATE_INCOMPLETE: return BU_CMD_VALIDATE_INCOMPLETE;
	case BU_CMD_VALIDATE_INVALID: return BU_CMD_VALIDATE_INVALID;
	case BU_CMD_VALIDATE_UNKNOWN:
	default: return BU_CMD_VALIDATE_UNKNOWN;
    }
}

static unsigned int
ged_native_expected(unsigned int expected)
{
    unsigned int mapped = BU_CMD_EXPECT_NONE;
    if (expected & BU_CMD_EXPECT_OPTION) mapped |= BU_CMD_EXPECT_OPTION;
    if (expected & BU_CMD_EXPECT_OPTION_ARG) mapped |= BU_CMD_EXPECT_OPTION_ARG;
    if (expected & BU_CMD_EXPECT_OPERAND) mapped |= BU_CMD_EXPECT_OPERAND;
    if (expected & BU_CMD_EXPECT_SUBCOMMAND) mapped |= BU_CMD_EXPECT_SUBCOMMAND;
    return mapped;
}

static std::map<std::string, struct ged_cmd_semantic_provider> &ged_semantic_provider_map()
{
    static std::map<std::string, struct ged_cmd_semantic_provider> providers;
    return providers;
}

static void ged_register_builtin_semantic_providers(void);

extern "C" void
ged_cmd_validate_result_clear(struct ged_cmd_validate_result *result)
{
    if (!result)
	return;
    if (result->completion_candidates)
	bu_argv_free(result->completion_count, (char **)result->completion_candidates);
    result->state = BU_CMD_VALIDATE_UNKNOWN;
    result->token_start = 0;
    result->token_end = 0;
    result->expected = BU_CMD_EXPECT_NONE;
    result->hint = NULL;
    result->completion_count = 0;
    result->completion_candidates = NULL;
    result->completion_type = BU_CMD_VALUE_UNKNOWN;
    result->semantic_provider = NULL;
    result->char_start = 0;
    result->char_end = 0;
}

extern "C" int
ged_cmd_semantic_provider_register(const struct ged_cmd_semantic_provider *provider)
{
    if (!provider || BU_STR_EMPTY(provider->name))
	return -1;

    std::map<std::string, struct ged_cmd_semantic_provider> &providers = ged_semantic_provider_map();
    std::string key(provider->name);
    if (providers.find(key) != providers.end())
	return 1;

    std::pair<std::map<std::string, struct ged_cmd_semantic_provider>::iterator, bool> ret =
	providers.insert(std::make_pair(key, *provider));
    ret.first->second.name = ret.first->first.c_str();
    return 0;
}

extern "C" int
ged_cmd_semantic_provider_exists(const char *name)
{
    if (BU_STR_EMPTY(name))
	return 0;

    ged_register_builtin_semantic_providers();
    return ged_semantic_provider_map().find(std::string(name)) != ged_semantic_provider_map().end();
}

static int
alphanum_cmp(const void *a, const void *b, void *UNUSED(data)) {
    struct directory *ga = *(struct directory **)a;
    struct directory *gb = *(struct directory **)b;
    return alphanum_impl(ga->d_namep, gb->d_namep, NULL);
}

static int
path_match(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *iseed)
{
    // If we've gotten this far, we either have a hierarchy or an error
    std::string lstr(iseed);
    if (lstr.find_first_of("/", 0) == std::string::npos)
	return 0;

    // Split the seed into substrings.  We can't use the db fullpath
    // routines for this, because the final obj name is likely incomplete
    // and therefore invalid.
    std::vector<std::string> objs;
    size_t pos = 0;
    while ((pos = lstr.find_first_of("/", 0)) != std::string::npos) {
	std::string ss = lstr.substr(0, pos);
	objs.push_back(ss);
	lstr.erase(0, pos + 1);
    }
    objs.push_back(lstr);
    if (objs.size() < 2)
	return 0;

    // If the last char in lstr is a slash, then we are looking to do
    // completions in the comb tree of the parent (if a comb or if we are
    // starting with a slash - i.e. the implicit tops comb) or there is no
    // completion to make (if not a comb).  If the last string is a partial,
    // then we're looking in the parent comb's tree for something that matches
    // the partial.
    std::string seed;
    std::string context;
    struct directory *dp = db_lookup(dbip, objs[objs.size()-1].c_str(), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL && (!lstr.length() || lstr[lstr.length() - 1] != '/')) {
	seed = objs[objs.size() - 1];
	context = objs[objs.size() - 2];
    } else {
	if (lstr.length() && lstr[lstr.length() - 1] != '/') {
	    seed = objs[objs.size() - 1];
	    context = objs[objs.size() - 2];
	} else {
	    context = objs[objs.size() - 1];
	}
    }

    if (!context.length()) {
	if (!seed.length()) {
	    bu_vls_trunc(prefix, 0);
	} else {
	    bu_vls_sprintf(prefix, "%s", seed.c_str());
	}
	// Empty context - we need the tops list
	db_update_nref(dbip);
	struct directory **all_paths;
	int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &all_paths);
	bu_sort(all_paths, tops_cnt, sizeof(struct directory *), alphanum_cmp, NULL);
	std::vector<struct directory *> matches;
	for (int i = 0; i < tops_cnt; i++) {
	    if (seed.empty() || (strlen(all_paths[i]->d_namep) >= seed.length() &&
		    !bu_strncmp(seed.c_str(), all_paths[i]->d_namep, seed.length())))
		matches.push_back(all_paths[i]);
	}
	*completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
	for (size_t i = 0; i < matches.size(); i++)
	    (*completions)[i] = bu_strdup(matches[i]->d_namep);
	bu_free(all_paths, "free db_ls output");
	return (int)matches.size();
    }

    struct directory *cdp = db_lookup(dbip, context.c_str(), LOOKUP_QUIET);
    if (cdp == RT_DIR_NULL || !(cdp->d_flags & RT_DIR_COMB))
	return BRLCAD_ERROR;

    struct rt_db_internal in;
    if (rt_db_get_internal(&in, cdp, dbip, NULL) < 0)
	return BRLCAD_ERROR;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    if (!comb) {
	rt_db_free_internal(&in);
	return BRLCAD_ERROR;
    }
    struct directory **children = NULL;
    int child_cnt = db_comb_children(dbip, comb, &children, NULL, NULL);
    rt_db_free_internal(&in);
    if (child_cnt > 1)
	bu_sort(children, child_cnt, sizeof(struct directory *), alphanum_cmp, NULL);

    // If we don't have a seed or a prev entry, grab all the children
    if (!seed.length()) {
	bu_vls_trunc(prefix, 0);
	*completions = (const char **)bu_calloc(child_cnt + 1, sizeof(const char *), "av array");
	for (int i = 0; i < child_cnt; i++)
	    (*completions)[i] = bu_strdup(children[i]->d_namep);
	bu_free(children, "dp array");
	return child_cnt;
    }

    // Have seed - grab the matches
    bu_vls_sprintf(prefix, "%s", seed.c_str());
    std::vector<struct directory *> matches;
    for (int i = 0; i < child_cnt; i++) {
	if (strlen(children[i]->d_namep) < seed.length())
	    continue;
	if (!bu_strncmp(seed.c_str(), children[i]->d_namep, seed.length()))
	    matches.push_back(children[i]);
    }
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]->d_namep);
    bu_free(children, "dp array");
    return (int)matches.size();
}

// Because librt doesn't forbid objects with forward slashes in
// their names, and such names have occasionally been observed in
// the wild, we have to treat all seed strings as potential dp names
// first, and only after that fails try them as hierarchy paths.
static int
obj_match(const char ***completions, struct db_i *dbip, const char *seed)
{
    // Prepare the dp list in the order we want - first tops entries, then
    // all objects.
    std::vector<struct directory *> dps;

    // all active directory pointers
    struct bu_ptbl fdps = BU_PTBL_INIT_ZERO;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip)
	bu_ptbl_ins(&fdps, (long *)dp);
    FOR_ALL_DIRECTORY_END;
    bu_sort(BU_PTBL_BASEADDR(&fdps), BU_PTBL_LEN(&fdps), sizeof(struct directory *), alphanum_cmp, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&fdps); i++) {
	struct directory *fdp = (struct directory *)BU_PTBL_GET(&fdps, i);
	dps.push_back(fdp);
    }
    bu_ptbl_free(&fdps);

    // Have the possibilities organized - find seed matches
    std::vector<struct directory *> matches;
    for (size_t i = 0; i < dps.size(); i++) {
	if (strlen(dps[i]->d_namep) < strlen(seed))
	    continue;
	if (!bu_strncmp(seed, dps[i]->d_namep, strlen(seed))) {
	    matches.push_back(dps[i]);
	}
    }

    // Make an argv array for client use
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]->d_namep);
    return (int)matches.size();
}

struct ged_input_parse {
    char *copy = NULL;
    char **argv = NULL;
    size_t argc = 0;
    size_t cursor_arg = 0;
    size_t input_len = 0;
    std::vector<size_t> char_starts;
    std::vector<size_t> char_ends;
};

static void
ged_input_parse_free(struct ged_input_parse *parsed)
{
    if (!parsed)
	return;

    if (parsed->argv)
	bu_free(parsed->argv, "argv array");
    if (parsed->copy)
	bu_free(parsed->copy, "input copy");

    parsed->copy = NULL;
    parsed->argv = NULL;
    parsed->argc = 0;
    parsed->cursor_arg = 0;
    parsed->input_len = 0;
    parsed->char_starts.clear();
    parsed->char_ends.clear();
}

static int
ged_input_parse_line(struct ged_input_parse *parsed, const char *input, size_t cursor_pos)
{
    std::vector<size_t> raw_starts;
    std::vector<size_t> raw_ends;
    bool in_token = false;
    bool in_quote = false;
    bool escaped = false;

    if (!parsed || !input)
	return -1;

    parsed->input_len = strlen(input);
    parsed->copy = bu_strdup(input);

    /* Record spans from the unmodified input.  bu_argv_from_string removes
     * quotes and escapes in place, so pointers into its result cannot be used
     * to highlight the original command reliably. */
    for (size_t i = 0; i < parsed->input_len; i++) {
	unsigned char c = (unsigned char)input[i];
	if (!in_token) {
	    if (isspace(c))
		continue;
	    raw_starts.push_back(i);
	    in_token = true;
	}
	if (escaped) {
	    escaped = false;
	    continue;
	}
	if (c == '\\') {
	    escaped = true;
	    continue;
	}
	if (c == '"') {
	    in_quote = !in_quote;
	    continue;
	}
	if (!in_quote && isspace(c)) {
	    raw_ends.push_back(i);
	    in_token = false;
	}
    }
    if (in_token)
	raw_ends.push_back(parsed->input_len);

    {
	size_t len = parsed->input_len;
	while (len > 0 && isspace((unsigned char)parsed->copy[len - 1]))
	    parsed->copy[--len] = '\0';
    }

    parsed->argv = (char **)bu_calloc(parsed->input_len + 1, sizeof(char *), "argv array");
    parsed->argc = bu_argv_from_string(parsed->argv, parsed->input_len, parsed->copy);
    parsed->char_starts.resize(parsed->argc);
    parsed->char_ends.resize(parsed->argc);

    size_t raw_index = 0;
    for (size_t i = 0; i < parsed->argc; i++) {
	int span_found = 0;
	while (raw_index < raw_starts.size() && raw_index < raw_ends.size()) {
	    std::string raw(input + raw_starts[raw_index], raw_ends[raw_index] - raw_starts[raw_index]);
	    std::vector<char> raw_copy(raw.begin(), raw.end());
	    char *raw_argv[2] = {NULL, NULL};
	    raw_copy.push_back('\0');
	    size_t raw_argc = bu_argv_from_string(raw_argv, 1, raw_copy.data());
	    if (raw_argc == 1 && BU_STR_EQUAL(raw_argv[0], parsed->argv[i])) {
		parsed->char_starts[i] = raw_starts[raw_index];
		parsed->char_ends[i] = raw_ends[raw_index];
		raw_index++;
		span_found = 1;
		break;
	    }
	    raw_index++;
	}
	if (!span_found) {
	    parsed->char_starts[i] = (size_t)(parsed->argv[i] - parsed->copy);
	    parsed->char_ends[i] = parsed->char_starts[i] + strlen(parsed->argv[i]);
	}
    }

    parsed->cursor_arg = parsed->argc;
    for (size_t i = 0; i < parsed->argc; i++) {
	if (cursor_pos <= parsed->char_ends[i]) {
	    parsed->cursor_arg = i;
	    break;
	}
    }

    return 0;
}

static std::string
ged_cursor_seed(const struct ged_input_parse &parsed)
{
    if (parsed.cursor_arg < parsed.argc && parsed.argv && parsed.argv[parsed.cursor_arg])
	return std::string(parsed.argv[parsed.cursor_arg]);

    return std::string();
}

static void
ged_set_validate_result(struct ged_cmd_validate_result *result,
			bu_cmd_validate_state_t state,
			size_t token_start,
			size_t token_end,
			unsigned int expected,
			const char *hint)
{
    ged_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token_start;
    result->token_end = token_end;
    result->expected = expected;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_UNKNOWN;
}

static void
ged_set_result_chars(struct ged_cmd_validate_result *result,
		     const struct ged_input_parse &parsed,
		     size_t token_start,
		     size_t token_end)
{
    if (!result)
	return;

    if (token_start < parsed.argc) {
	size_t end_index = (token_end < parsed.argc) ? token_end : parsed.argc - 1;
	result->char_start = parsed.char_starts[token_start];
	result->char_end = parsed.char_ends[end_index];
    } else {
	result->char_start = parsed.input_len;
	result->char_end = parsed.input_len;
    }
}

static void
ged_replace_candidates(struct ged_cmd_validate_result *result,
		       const char ***completions,
		       int cnt,
		       bu_cmd_value_t ctype)
{
    if (!result)
	return;

    if (result->completion_candidates)
	bu_argv_free(result->completion_count, (char **)result->completion_candidates);

    result->completion_candidates = *completions;
    result->completion_count = (cnt > 0) ? (size_t)cnt : 0;
    result->completion_type = ctype;
}

static void
ged_fill_command_candidates(struct ged_cmd_validate_result *result, const std::string &seed)
{
    const char **completions = NULL;
    int cnt = ged_cmd_completions(&completions, seed.c_str());
    if (cnt > 0)
	ged_replace_candidates(result, &completions, cnt, BU_CMD_VALUE_UNKNOWN);
}

static void
ged_fill_geometry_candidates(struct ged *gedp,
			     const std::string &seed,
			     struct ged_cmd_validate_result *result,
			     struct bu_vls *oprefix = NULL)
{
    if (!gedp || !gedp->dbip || !result)
	return;

    struct bu_vls lprefix = BU_VLS_INIT_ZERO;
    struct bu_vls *prefix = oprefix ? oprefix : &lprefix;
    const char **completions = NULL;
    int cnt = ged_geom_completions(&completions, prefix, gedp->dbip, seed.c_str());
    if (cnt > 0) {
	bu_cmd_value_t ctype = result->completion_type;
	if (ctype != BU_CMD_VALUE_DB_OBJECT && ctype != BU_CMD_VALUE_DB_PATH)
	    ctype = (seed.find('/') != std::string::npos) ? BU_CMD_VALUE_DB_PATH : BU_CMD_VALUE_DB_OBJECT;
	ged_replace_candidates(result, &completions, cnt,
		ctype);
    }
    if (!oprefix)
	bu_vls_free(&lprefix);
}

static struct directory *
ged_lookup_exact_quiet(struct db_i *dbip, const char *name)
{
    if (!dbip || BU_STR_EMPTY(name))
	return RT_DIR_NULL;

    if (!strchr(name, '/'))
	return db_lookup(dbip, name, LOOKUP_QUIET);

    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip)
	if (BU_STR_EQUAL(dp->d_namep, name))
	    return dp;
    FOR_ALL_DIRECTORY_END;

    return RT_DIR_NULL;
}

static ged_cmd_semantic_state_t
ged_quiet_db_path_validate(struct ged *gedp, const char *token)
{
    if (!gedp || !gedp->dbip || BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INVALID;

    size_t tlen = strlen(token);
    if (token[tlen - 1] == '/')
	return GED_CMD_SEMANTIC_INVALID;

    if (ged_lookup_exact_quiet(gedp->dbip, token) != RT_DIR_NULL)
	return GED_CMD_SEMANTIC_VALID;

    std::string path(token);
    while (!path.empty() && path[0] == '/')
	path.erase(0, 1);
    if (path.empty())
	return GED_CMD_SEMANTIC_VALID;

    std::vector<std::string> elems;
    size_t start = 0;
    while (start <= path.size()) {
	size_t slash = path.find('/', start);
	size_t end = (slash == std::string::npos) ? path.size() : slash;
	if (end == start)
	    return GED_CMD_SEMANTIC_INVALID;
	elems.push_back(path.substr(start, end - start));
	if (slash == std::string::npos)
	    break;
	start = slash + 1;
    }

    if (elems.empty())
	return GED_CMD_SEMANTIC_INVALID;

    struct directory *parent = db_lookup(gedp->dbip, elems[0].c_str(), LOOKUP_QUIET);
    if (parent == RT_DIR_NULL)
	return GED_CMD_SEMANTIC_INVALID;

    for (size_t i = 1; i < elems.size(); i++) {
	if (!(parent->d_flags & RT_DIR_COMB))
	    return GED_CMD_SEMANTIC_INVALID;

	struct rt_db_internal in;
	if (rt_db_get_internal(&in, parent, gedp->dbip, NULL) < 0)
	    return GED_CMD_SEMANTIC_INVALID;

	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb) {
	    rt_db_free_internal(&in);
	    return GED_CMD_SEMANTIC_INVALID;
	}

	struct directory **children = NULL;
	int child_cnt = db_comb_children(gedp->dbip, comb, &children, NULL, NULL);
	rt_db_free_internal(&in);

	struct directory *child = RT_DIR_NULL;
	for (int j = 0; j < child_cnt; j++) {
	    if (BU_STR_EQUAL(children[j]->d_namep, elems[i].c_str())) {
		child = children[j];
		break;
	    }
	}

	if (children)
	    bu_free(children, "dp array");

	if (child == RT_DIR_NULL)
	    return GED_CMD_SEMANTIC_INVALID;

	parent = child;
    }

    return GED_CMD_SEMANTIC_VALID;
}

static ged_cmd_semantic_state_t
ged_builtin_db_object_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp || !gedp->dbip)
	return GED_CMD_SEMANTIC_UNKNOWN;
    return (ged_lookup_exact_quiet(gedp->dbip, token) != RT_DIR_NULL) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}


static ged_cmd_semantic_state_t
ged_builtin_summary_object_or_legacy_type_validate(struct ged *gedp,
	bu_cmd_value_t type, const char *token, void *data)
{
    const char *p = token;

    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    while (*p) {
	if (*p != 'p' && *p != 'r' && *p != 'g')
	    return ged_builtin_db_object_validate(gedp, type, token, data);
	p++;
    }
    return GED_CMD_SEMANTIC_VALID;
}

static ged_cmd_semantic_state_t
ged_builtin_db_path_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp || !gedp->dbip)
	return GED_CMD_SEMANTIC_UNKNOWN;
    if (type == BU_CMD_VALUE_DB_OBJECT && ged_lookup_exact_quiet(gedp->dbip, token) != RT_DIR_NULL)
	return GED_CMD_SEMANTIC_VALID;
    return ged_quiet_db_path_validate(gedp, token);
}

static ged_cmd_semantic_state_t
ged_builtin_view_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp)
	return GED_CMD_SEMANTIC_UNKNOWN;
    return bv_set_find_view(&gedp->ged_views, token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_view_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    std::vector<std::string> matches;
    const char *prefix = seed ? seed : "";

    if (!gedp || !result)
	return 0;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    if (!views)
	return 0;
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	const char *name = v ? bu_vls_cstr(&v->gv_name) : NULL;
	if (name && bu_strncmp(name, prefix, strlen(prefix)) == 0)
	    matches.push_back(std::string(name));
    }
    std::sort(matches.begin(), matches.end(), [](const std::string &a, const std::string &b) {
	return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
    });
    if (matches.empty())
	return 0;
    result->completion_count = matches.size();
    result->completion_candidates = (const char **)bu_calloc(matches.size() + 1, sizeof(char *), "view completion candidates");
    for (size_t i = 0; i < matches.size(); i++)
	result->completion_candidates[i] = bu_strdup(matches[i].c_str());
    return (int)matches.size();
}

static ged_cmd_semantic_state_t
ged_builtin_command_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    return ged_cmd_exists(token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_command_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    const char **candidates = NULL;
    int cnt = 0;
    if (!result)
	return 0;
    cnt = ged_cmd_completions(&candidates, seed ? seed : "");
    if (cnt <= 0)
	return 0;
    result->completion_count = (size_t)cnt;
    result->completion_candidates = candidates;
    return cnt;
}

static ged_cmd_semantic_state_t
ged_builtin_primitive_type_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    std::string label(token);
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return rt_get_functab_by_label(label.c_str()) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_primitive_type_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    if (!result)
	return 0;
    std::set<std::string> matches;
    std::string prefix = seed ? seed : "";
    for (int i = 1; i <= ID_MAX_SOLID; i++) {
	if (!OBJ[i].ft_label[0] || BU_STR_EQUAL(OBJ[i].ft_label, "ID_NULL"))
	    continue;
	std::string label(OBJ[i].ft_label);
	if (prefix.empty() || label.compare(0, prefix.size(), prefix) == 0)
	    matches.insert(label);
    }
    if (matches.empty())
	return 0;
    result->completion_count = matches.size();
    result->completion_candidates = (const char **)bu_calloc(matches.size() + 1, sizeof(char *), "primitive type candidates");
    size_t oi = 0;
    for (const std::string &match : matches)
	result->completion_candidates[oi++] = bu_strdup(match.c_str());
    result->completion_type = BU_CMD_VALUE_KEYWORD;
    return (int)matches.size();
}

static ged_cmd_semantic_state_t
ged_builtin_geometry_or_primitive_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, void *data)
{
    ged_cmd_semantic_state_t pstate = ged_builtin_primitive_type_validate(gedp, type, token, data);
    if (pstate == GED_CMD_SEMANTIC_VALID)
	return pstate;
    return ged_builtin_db_object_validate(gedp, type, token, data);
}

static int
ged_builtin_geometry_or_primitive_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    if (!result)
	return 0;
    std::set<std::string> matches;
    std::string prefix = seed ? seed : "";
    for (int i = 1; i <= ID_MAX_SOLID; i++) {
	if (!OBJ[i].ft_label[0] || BU_STR_EQUAL(OBJ[i].ft_label, "ID_NULL"))
	    continue;
	std::string label(OBJ[i].ft_label);
	if (prefix.empty() || label.compare(0, prefix.size(), prefix) == 0)
	    matches.insert(label);
    }
    if (gedp && gedp->dbip) {
	const char **objects = NULL;
	struct bu_vls oprefix = BU_VLS_INIT_ZERO;
	int cnt = ged_geom_completions(&objects, &oprefix, gedp->dbip, prefix.c_str());
	for (int i = 0; i < cnt; i++)
	    if (objects[i]) matches.insert(objects[i]);
	if (objects) bu_argv_free((size_t)cnt, (char **)objects);
	bu_vls_free(&oprefix);
    }
    if (matches.empty())
	return 0;
    std::vector<std::string> ordered(matches.begin(), matches.end());
    std::sort(ordered.begin(), ordered.end(), [](const std::string &a, const std::string &b) {
	return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
    });
    result->completion_count = ordered.size();
    result->completion_candidates = (const char **)bu_calloc(ordered.size() + 1, sizeof(char *), "geometry or primitive candidates");
    size_t oi = 0;
    for (const std::string &match : ordered)
	result->completion_candidates[oi++] = bu_strdup(match.c_str());
    result->completion_type = BU_CMD_VALUE_RAW;
    return (int)matches.size();
}

static ged_cmd_semantic_state_t
ged_builtin_search_path_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (!token || BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (BU_STR_EQUAL(token, "/") || BU_STR_EQUAL(token, ".") || BU_STR_EQUAL(token, "|"))
	return GED_CMD_SEMANTIC_VALID;
    if (token[0] == '|') {
	if (!token[1])
	    return GED_CMD_SEMANTIC_VALID;
	return ged_quiet_db_path_validate(gedp, token + 1);
    }
    return ged_quiet_db_path_validate(gedp, token);
}

static ged_cmd_semantic_state_t
ged_builtin_search_type_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    return ged_search_is_type_keyword(token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_path_or_pattern_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, void *data)
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (strpbrk(token, "*?[]"))
	return GED_CMD_SEMANTIC_UNKNOWN;
    return ged_builtin_db_path_validate(gedp, type, token, data);
}

static int
ged_builtin_path_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    ged_fill_geometry_candidates(gedp, seed ? std::string(seed) : std::string(), result);
    return result ? (int)result->completion_count : 0;
}

static ged_cmd_semantic_state_t
ged_builtin_nonempty_unknown(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    return BU_STR_EMPTY(token) ? GED_CMD_SEMANTIC_INCOMPLETE : GED_CMD_SEMANTIC_UNKNOWN;
}

static ged_cmd_semantic_state_t
ged_builtin_nonnegative_integer(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    char *end = NULL;
    long val = strtol(token, &end, 0);
    return (end && *end == '\0' && val >= 0) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_display_mode_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    static const char * const modes[] = {"0", "1", "2", "3", "wireframe", "shaded", "shaded_all", "evaluated", NULL};
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    for (size_t i = 0; modes[i]; i++) if (BU_STR_EQUAL(token, modes[i])) return GED_CMD_SEMANTIC_VALID;
    return GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_draw_mode_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    return (token[1] == '\0' && token[0] >= '0' && token[0] <= '5') ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_unit_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    return (bu_mm_value(token) >= 0.0) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_color_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    return bu_color_from_str(&color, token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_matrix_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    mat_t matrix;
    return (bn_decode_mat(matrix, token) == 16) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_vector_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    vect_t vector = VINIT_ZERO;
    const char *argv[1] = {token};
    return bu_cmd_vector3_from_argv(vector, 1, argv) == 1 ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_keyword_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, void *data)
{
    if (!result || !data) return 0;
    const char * const *keywords = (const char * const *)data;
    const char *prefix = seed ? seed : "";
    std::vector<std::string> matches;
    for (size_t i = 0; keywords[i]; i++)
	if (!bu_strncmp(keywords[i], prefix, strlen(prefix))) matches.push_back(keywords[i]);
    if (matches.empty()) return 0;
    result->completion_count = matches.size();
    result->completion_candidates = (const char **)bu_calloc(matches.size() + 1, sizeof(char *), "semantic keyword candidates");
    for (size_t i = 0; i < matches.size(); i++) result->completion_candidates[i] = bu_strdup(matches[i].c_str());
    return (int)matches.size();
}

static int
ged_builtin_file_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, void *UNUSED(data))
{
    if (!result) return 0;
    char **files = NULL;
    size_t cnt = bu_file_complete(seed ? seed : "", BU_FILE_COMPLETE_APPEND_SLASH, NULL, &files);
    result->completion_candidates = (const char **)files;
    result->completion_count = cnt;
    result->completion_type = BU_CMD_VALUE_FILE;
    return (int)cnt;
}

static void
ged_register_builtin_semantic_providers(void)
{
    static int registered = 0;
    if (registered)
	return;
    registered = 1;

    const struct ged_cmd_semantic_provider db_object_provider = {"ged.db_object", ged_builtin_db_object_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider summary_object_or_legacy_type_provider = {
	"ged.summary_object_or_legacy_type",
	ged_builtin_summary_object_or_legacy_type_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider db_path_provider = {"ged.db_path", ged_builtin_db_path_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider search_path_provider = {"ged.search.path", ged_builtin_search_path_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider search_type_provider = {"ged.search.type", ged_builtin_search_type_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider search_parser_provider = {"ged.search", NULL, NULL, NULL};
    const struct ged_cmd_semantic_provider view_provider = {"ged.view", ged_builtin_view_validate, ged_builtin_view_complete, NULL};
    const struct ged_cmd_semantic_provider command_provider = {"ged.command_name", ged_builtin_command_validate, ged_builtin_command_complete, NULL};
    const struct ged_cmd_semantic_provider primitive_type_provider = {"ged.primitive_type", ged_builtin_primitive_type_validate, ged_builtin_primitive_type_complete, NULL};
    const struct ged_cmd_semantic_provider geometry_or_primitive_provider = {"ged.geometry_or_primitive", ged_builtin_geometry_or_primitive_validate, ged_builtin_geometry_or_primitive_complete, NULL};
    static const char * const attribute_names[] = {"aircode", "color", "inherit", "los", "material_id", "material_name", "region", "region_id", "shader", NULL};
    static const char * const display_modes[] = {"0", "1", "2", "3", "wireframe", "shaded", "shaded_all", "evaluated", NULL};
    static const char * const draw_modes[] = {"0", "1", "2", "3", "4", "5", NULL};
    static const char * const unit_names[] = {"um", "mm", "cm", "m", "km", "in", "ft", "yd", "mi", NULL};
    const struct ged_cmd_semantic_provider path_pattern_provider = {"ged.db_path_or_pattern", ged_builtin_path_or_pattern_validate, ged_builtin_path_complete, NULL};
    const struct ged_cmd_semantic_provider attribute_provider = {"ged.db_attribute", ged_builtin_nonempty_unknown, ged_builtin_keyword_complete, (void *)attribute_names};
    const struct ged_cmd_semantic_provider region_id_provider = {"ged.db_region_id", ged_builtin_nonnegative_integer, NULL, NULL};
    const struct ged_cmd_semantic_provider display_mode_provider = {"ged.display_mode", ged_builtin_display_mode_validate, ged_builtin_keyword_complete, (void *)display_modes};
    const struct ged_cmd_semantic_provider draw_mode_provider = {"ged.draw_mode", ged_builtin_draw_mode_validate, ged_builtin_keyword_complete, (void *)draw_modes};
    const struct ged_cmd_semantic_provider file_path_provider = {"ged.file_path", ged_builtin_nonempty_unknown, ged_builtin_file_complete, NULL};
    const struct ged_cmd_semantic_provider unit_provider = {"ged.unit", ged_builtin_unit_validate, ged_builtin_keyword_complete, (void *)unit_names};
    const struct ged_cmd_semantic_provider color_provider = {"ged.color", ged_builtin_color_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider matrix_provider = {"ged.matrix", ged_builtin_matrix_validate, NULL, NULL};
    const struct ged_cmd_semantic_provider vector_provider = {"ged.vector", ged_builtin_vector_validate, NULL, NULL};
    /* A schema-owned multi-token vector validator has already checked the
     * complete group.  Do not revalidate one component as a standalone packed
     * vector while completion is focused on that component. */
    const struct ged_cmd_semantic_provider vector_group_provider = {"ged.vector_group", NULL, NULL, NULL};

    (void)ged_cmd_semantic_provider_register(&db_object_provider);
    (void)ged_cmd_semantic_provider_register(&summary_object_or_legacy_type_provider);
    (void)ged_cmd_semantic_provider_register(&db_path_provider);
    (void)ged_cmd_semantic_provider_register(&search_path_provider);
    (void)ged_cmd_semantic_provider_register(&search_type_provider);
    (void)ged_cmd_semantic_provider_register(&search_parser_provider);
    (void)ged_cmd_semantic_provider_register(&view_provider);
    (void)ged_cmd_semantic_provider_register(&command_provider);
    (void)ged_cmd_semantic_provider_register(&primitive_type_provider);
    (void)ged_cmd_semantic_provider_register(&geometry_or_primitive_provider);
    (void)ged_cmd_semantic_provider_register(&path_pattern_provider);
    (void)ged_cmd_semantic_provider_register(&attribute_provider);
    (void)ged_cmd_semantic_provider_register(&region_id_provider);
    (void)ged_cmd_semantic_provider_register(&display_mode_provider);
    (void)ged_cmd_semantic_provider_register(&draw_mode_provider);
    (void)ged_cmd_semantic_provider_register(&file_path_provider);
    (void)ged_cmd_semantic_provider_register(&unit_provider);
    (void)ged_cmd_semantic_provider_register(&color_provider);
    (void)ged_cmd_semantic_provider_register(&matrix_provider);
    (void)ged_cmd_semantic_provider_register(&vector_provider);
    (void)ged_cmd_semantic_provider_register(&vector_group_provider);
}

static ged_cmd_semantic_state_t
ged_semantic_validate(struct ged *gedp, bu_cmd_value_t type, const char *validator, const char *token)
{
    if (!validator || !validator[0])
	return GED_CMD_SEMANTIC_UNKNOWN;

    ged_register_builtin_semantic_providers();
    std::map<std::string, struct ged_cmd_semantic_provider> &providers = ged_semantic_provider_map();
    std::map<std::string, struct ged_cmd_semantic_provider>::iterator it = providers.find(std::string(validator));
    if (it != providers.end() && it->second.validate) {
	return (*it->second.validate)(gedp, type, token, it->second.data);
    }

    return GED_CMD_SEMANTIC_UNKNOWN;
}

static int
ged_semantic_complete(struct ged *gedp, const char *provider, const char *seed, struct ged_cmd_validate_result *result)
{
    if (BU_STR_EMPTY(provider) || !result)
	return 0;
    ged_register_builtin_semantic_providers();
    std::map<std::string, struct ged_cmd_semantic_provider> &providers = ged_semantic_provider_map();
    std::map<std::string, struct ged_cmd_semantic_provider>::iterator it = providers.find(std::string(provider));
    if (it == providers.end() || !it->second.complete)
	return 0;
    return (*it->second.complete)(gedp, seed ? seed : "", result, it->second.data);
}

static void
ged_analysis_set_span(struct ged_cmd_analysis_token *token, const struct ged_input_parse &parsed, size_t idx)
{
    token->token_start = idx;
    token->token_end = idx;
    token->char_start = parsed.char_starts[idx];
    token->char_end = parsed.char_ends[idx];
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
ged_search_type_keywords(int include_aliases)
{
    std::set<std::string> keywords;
    const char *abstract_types[] = {
	"arb4", "arb5", "arb6", "arb7", "arb8",
	"combination", "plate", "region", "shape", "sphere", "volume",
	NULL
    };
    const char *aliases[] = {
	"c", "comb", "r", "reg", "sph",
	NULL
    };

    for (size_t i = 0; abstract_types[i]; i++)
	keywords.insert(abstract_types[i]);

    for (int i = 1; i <= ID_MAX_SOLID; i++) {
	if (OBJ[i].ft_label[0] && !BU_STR_EQUAL(OBJ[i].ft_label, "ID_NULL"))
	    keywords.insert(OBJ[i].ft_label);
    }

    if (include_aliases) {
	for (size_t i = 0; aliases[i]; i++)
	    keywords.insert(aliases[i]);
    }

    return std::vector<std::string>(keywords.begin(), keywords.end());
}

static int
ged_search_is_type_keyword(const char *token)
{
    std::vector<std::string> keywords = ged_search_type_keywords(1);

    if (!token)
	return 0;
    for (size_t i = 0; i < keywords.size(); i++) {
	if (BU_STR_EQUAL(token, keywords[i].c_str()))
	    return 1;
    }
    return 0;
}

static int
ged_search_type_has_prefix(const char *token)
{
    std::vector<std::string> keywords = ged_search_type_keywords(1);
    std::string prefix = token ? token : "";

    if (prefix.empty())
	return 1;
    for (size_t i = 0; i < keywords.size(); i++) {
	if (ged_search_prefix_match(keywords[i], prefix))
	    return 1;
    }
    return 0;
}

static void
ged_search_collect_type_candidates(std::vector<std::string> &candidates, const std::string &prefix)
{
    std::vector<std::string> keywords = ged_search_type_keywords(0);

    for (size_t i = 0; i < keywords.size(); i++)
	ged_search_add_candidate(candidates, keywords[i].c_str(), prefix);
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
	return ged_quiet_db_path_validate(gedp, token + 1);
    }
    return ged_quiet_db_path_validate(gedp, token);
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
ged_argv_range_line(const struct ged_input_parse &parsed, size_t start, size_t end, int trailing_space = 0)
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
	ged_set_validate_result(result, BU_CMD_VALIDATE_INCOMPLETE, token_index, token_index,
		BU_CMD_EXPECT_OPTION_ARG, "search expression argument expected");
	result->completion_type = ged_search_plan_arg_type(state.pending_arg);
	if (BU_STR_EQUAL(state.pending_arg->name, "-exec")) {
	    ged_fill_command_candidates(result, seed);
	    result->expected = BU_CMD_EXPECT_SUBCOMMAND;
	    result->hint = "search -exec GED command expected";
	    if (arg && ged_cmd_exists(arg))
		result->state = BU_CMD_VALIDATE_VALID;
	    else if (arg && !result->completion_count)
		result->state = BU_CMD_VALIDATE_INVALID;
	    ged_set_result_chars(result, parsed, token_index, token_index);
	    return 1;
	}
	if (ged_search_plan_validator(state.pending_arg))
	    result->hint = ged_search_plan_validator(state.pending_arg);
	if (BU_STR_EQUAL(state.pending_arg->name, "-type")) {
	    ged_search_collect_type_candidates(candidates, seed);
	    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_KEYWORD);
	    if (arg && ged_search_is_type_keyword(arg))
		result->state = BU_CMD_VALIDATE_VALID;
	    else if (arg && !ged_search_type_has_prefix(arg))
		result->state = BU_CMD_VALIDATE_INVALID;
	}
	ged_set_result_chars(result, parsed, token_index, token_index);
	return 1;
    }

    if (state.in_exec) {
	if (arg && BU_STR_EQUAL(arg, ";")) {
	    ged_set_validate_result(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		    BU_CMD_EXPECT_OPTION, "search -exec terminator");
	    result->completion_type = BU_CMD_VALUE_RAW;
	    ged_set_result_chars(result, parsed, token_index, token_index);
	    return 1;
	}

	struct ged_cmd_validate_result nested = GED_CMD_VALIDATE_RESULT_NULL;
	size_t nested_end = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg + 1 : parsed.argc;
	int trailing_space = (parsed.cursor_arg >= parsed.argc && seed.empty()) ? 1 : 0;
	std::string nested_line = ged_argv_range_line(parsed, state.exec_start, nested_end, trailing_space);
	(void)ged_cmd_validate(gedp, nested_line.c_str(), nested_line.size(), &nested);

	ged_set_validate_result(result, nested.state, token_index, token_index,
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
	ged_set_result_chars(result, parsed, token_index, token_index);
	ged_cmd_validate_result_clear(&nested);
	return 1;
    }

    if (!state.in_plan) {
	if (seed.empty() || ged_search_is_plan_start(seed.c_str())) {
	    ged_set_validate_result(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		    BU_CMD_EXPECT_OPTION | BU_CMD_EXPECT_OPERAND, "search path or expression expected");
	    if (!state.path_seen)
		ged_search_collect_top_options(candidates, seed);
	    ged_search_collect_plan_ops(candidates, seed);
	    if (!seed.empty() && candidates.empty()) {
		result->state = BU_CMD_VALIDATE_INVALID;
		result->hint = "unknown search expression operator";
	    }
	    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
	    ged_set_result_chars(result, parsed, token_index, token_index);
	    return 1;
	}

	ged_set_validate_result(result, BU_CMD_VALIDATE_VALID, token_index, token_index,
		BU_CMD_EXPECT_OPERAND, "search path expected");
	result->completion_type = BU_CMD_VALUE_DB_PATH;
	ged_fill_geometry_candidates(gedp, seed, result);
	if (arg && ged_search_path_state(gedp, arg) == GED_CMD_SEMANTIC_INVALID)
	    result->state = BU_CMD_VALIDATE_INVALID;
	ged_set_result_chars(result, parsed, token_index, token_index);
	return 1;
    }

    current_op = arg ? ged_search_find_plan_op(arg) : NULL;
    if (arg && ged_search_is_plan_start(arg) && !current_op && !candidates.size()) {
	ged_set_validate_result(result, BU_CMD_VALIDATE_INVALID, token_index, token_index,
		BU_CMD_EXPECT_OPTION, "unknown search expression operator");
	ged_search_collect_plan_ops(candidates, seed);
	ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
	ged_set_result_chars(result, parsed, token_index, token_index);
	return 1;
    }

    ged_set_validate_result(result, current_op ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE,
	    token_index, token_index, BU_CMD_EXPECT_OPTION, "search expression operator expected");
    ged_search_collect_plan_ops(candidates, seed);
    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_UNKNOWN);
    ged_set_result_chars(result, parsed, token_index, token_index);
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
		    std::string nested_line = ged_argv_range_line(parsed, i, exec_end);
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
		token->semantic_state = ged_search_is_type_keyword(arg) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
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
    seed = ged_cursor_seed(parsed);
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
	ged_analysis_set_span(&analysis->tokens[i], parsed, i);
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

int
ged_cmd_completions(const char ***completions, const char *seed)
{
    int ret = 0;

    if (!completions || !seed)
	return 0;

    //Build a set of matches
    const char * const *cl = NULL;
    size_t cmd_cnt = ged_cmd_list(&cl);

    std::vector<const char *> matches;
    for (size_t i = 0; i < cmd_cnt; i++) {
	if (strlen(cl[i]) < strlen(seed))
	    continue;
	if (!bu_strncmp(seed, cl[i], strlen(seed)))
	    matches.push_back(cl[i]);
    }

    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]);
    ret = (int)matches.size();

    return ret;
}

extern "C" void
ged_cmd_analysis_clear(struct ged_cmd_analysis *analysis)
{
    if (!analysis)
	return;
    if (analysis->tokens)
	bu_free(analysis->tokens, "ged command analysis tokens");
    analysis->token_count = 0;
    analysis->tokens = NULL;
}

static ged_cmd_semantic_state_t
ged_native_semantic_state(bu_cmd_validate_state_t state)
{
    switch (state) {
	case BU_CMD_VALIDATE_VALID: return GED_CMD_SEMANTIC_VALID;
	case BU_CMD_VALIDATE_INCOMPLETE: return GED_CMD_SEMANTIC_INCOMPLETE;
	case BU_CMD_VALIDATE_INVALID: return GED_CMD_SEMANTIC_INVALID;
	case BU_CMD_VALIDATE_UNKNOWN:
	default: return GED_CMD_SEMANTIC_UNKNOWN;
    }
}

static const struct bu_cmd_option *
ged_native_find_option(const struct bu_cmd_schema *schema, const char *arg)
{
    int longopt = 0;
    const char *name = NULL;
    std::string name_storage;

    if (!schema || !schema->options || !arg || arg[0] != '-' || !arg[1])
	return NULL;
    longopt = arg[1] == '-';
    name = arg + (longopt ? 2 : 1);
    name_storage.assign(name);
    size_t equal = name_storage.find('=');
    if (equal != std::string::npos)
	name_storage.resize(equal);
    name = name_storage.c_str();
    for (size_t i = 0; schema->options[i].canonical; i++) {
	const struct bu_cmd_option *option = &schema->options[i];
	const char *spelling = longopt ? option->longopt : option->shortopt;
	size_t spelling_len = spelling ? strlen(spelling) : 0;
	if (!spelling || strncmp(name, spelling, spelling_len) ||
		(name[spelling_len] && name[spelling_len] != '='))
	    continue;
	if (!option->alias_of)
	    return option;
	for (size_t ci = 0; schema->options[ci].canonical; ci++) {
	    const struct bu_cmd_option *canonical = &schema->options[ci];
	    if (!canonical->alias_of && BU_STR_EQUAL(canonical->canonical, option->alias_of))
		return canonical;
	}
	return NULL;
    }
    return NULL;
}

static const struct bu_cmd_operand *
ged_native_operand_at(const struct bu_cmd_schema *schema, size_t operand_index)
{
    size_t index = 0;

    if (!schema || !schema->operands)
	return NULL;
    for (size_t i = 0; schema->operands[i].name; i++) {
	const struct bu_cmd_operand *operand = &schema->operands[i];
	if (operand->max_count == BU_CMD_COUNT_UNLIMITED ||
	    (operand_index >= index && operand_index - index < operand->max_count))
	    return operand;
	index += operand->max_count;
    }
    return NULL;
}

static void
ged_native_analyze(struct ged *gedp, const struct bu_cmd_schema *schema, const struct ged_input_parse &parsed,
	struct ged_cmd_analysis *analysis)
{
    int options_allowed = 1;
    const struct bu_cmd_option *pending = NULL;
    size_t pending_count = 0;
    size_t operand_count = 0;

    for (size_t i = 1; i < parsed.argc; i++) {
	const char *arg = parsed.argv[i];
	struct ged_cmd_analysis_token *token = &analysis->tokens[i];
	if (pending) {
	    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
	    const char *provider = ged_native_semantic_provider(pending->value_type, pending->semantic_provider);
	    (void)bu_cmd_schema_validate(schema, i, (const char **)parsed.argv + 1, i - 1, &result);
	    const char *dynamic_provider = ged_native_semantic_provider(result.completion_type,
		result.semantic_provider);
	    if (dynamic_provider)
		provider = dynamic_provider;
	    token->role = GED_CMD_TOKEN_OPTION_ARG;
	    token->value_type = ged_native_value_type(result.completion_type, provider);
	    token->semantic_state = ged_native_semantic_state(result.state);
	    token->validator = provider;
	    if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
		ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, arg);
		if (state != GED_CMD_SEMANTIC_UNKNOWN)
		    token->semantic_state = state;
	    }
	    token->hint = result.hint ? result.hint : "option argument";
	    bu_cmd_validate_result_clear(&result);
	if (--pending_count == 0)
	    pending = NULL;
	continue;
	}
	if (options_allowed && BU_STR_EQUAL(arg, "--")) {
	    options_allowed = 0;
	    token->role = GED_CMD_TOKEN_OPTION;
	    token->semantic_state = GED_CMD_SEMANTIC_VALID;
	    token->hint = "end of options";
	    continue;
	}
	if (options_allowed && arg[0] == '-' && arg[1]) {
	    const struct bu_cmd_option *option = ged_native_find_option(schema, arg);
	    const char *eq = strchr(arg, '=');
	    token->role = GED_CMD_TOKEN_OPTION;
	    token->semantic_state = option ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
	    token->hint = option ? "option" : "unknown option";
	    if (!option)
		continue;
	if (eq && option->value_type != BU_CMD_VALUE_FLAG) {
		struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
		const char *provider = ged_native_semantic_provider(option->value_type, option->semantic_provider);
		(void)bu_cmd_schema_validate(schema, i, (const char **)parsed.argv + 1, i - 1, &result);
		const char *dynamic_provider = ged_native_semantic_provider(result.completion_type,
		    result.semantic_provider);
		if (dynamic_provider)
		    provider = dynamic_provider;
		token->role = GED_CMD_TOKEN_OPTION_ARG;
		token->value_type = ged_native_value_type(result.completion_type, provider);
		token->semantic_state = ged_native_semantic_state(result.state);
		token->validator = provider;
		if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
		    ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, eq + 1);
		    if (state != GED_CMD_SEMANTIC_UNKNOWN)
			token->semantic_state = state;
		}
		token->hint = result.hint ? result.hint : "option with argument";
		bu_cmd_validate_result_clear(&result);
	    } else if (eq) {
		token->semantic_state = GED_CMD_SEMANTIC_INVALID;
		token->hint = "option does not take an argument";
	    } else if (option->value_type != BU_CMD_VALUE_FLAG) {
		int option_span = bu_cmd_schema_option_span(schema, parsed.argc - i,
		    (const char **)parsed.argv + i);
		if (option_span > 1) {
		    pending = option;
		    pending_count = (size_t)option_span - 1;
		} else if (option_span < 0 && i + 1 < parsed.argc) {
		    /* Preserve a precise option-argument classification when the
		     * shared parser detects a malformed or incomplete value. */
		    pending = option;
		    pending_count = 1;
		}
	    }
	    continue;
	}
	const struct bu_cmd_operand *operand = ged_native_operand_at(schema, operand_count);
	const char *provider = operand ? ged_native_semantic_provider(operand->value_type, operand->semantic_provider) : NULL;
	struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
	(void)bu_cmd_schema_validate(schema, i, (const char **)parsed.argv + 1, i - 1, &result);
	const char *dynamic_provider = ged_native_semantic_provider(result.completion_type,
	    result.semantic_provider);
	if (dynamic_provider)
	    provider = dynamic_provider;
	/* A custom native schema may own a child command phase after one or
	 * more operands (for example, view obj <name> axes).  Its validator is
	 * authoritative about that phase, so preserve the subcommand role rather
	 * than flattening every non-root word into an operand. */
	token->role = (result.expected & BU_CMD_EXPECT_SUBCOMMAND) ?
	    GED_CMD_TOKEN_SUBCOMMAND : GED_CMD_TOKEN_OPERAND;
	token->value_type = ged_native_value_type(result.completion_type, provider);
	token->semantic_state = ged_native_semantic_state(result.state);
	token->validator = provider;
	if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
	    ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, arg);
	    if (state != GED_CMD_SEMANTIC_UNKNOWN)
		token->semantic_state = state;
	}
	token->hint = result.hint ? result.hint :
	    (operand ? operand->name : "unexpected operand");
	bu_cmd_validate_result_clear(&result);
	operand_count++;
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
    }
}

extern "C" int
ged_cmd_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

    if (!input || !analysis)
	return -1;

    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;

    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }

    analysis->tokens = (struct ged_cmd_analysis_token *)bu_calloc(parsed.argc, sizeof(struct ged_cmd_analysis_token), "ged command analysis tokens");
    analysis->token_count = parsed.argc;

    for (size_t i = 0; i < parsed.argc; i++) {
	ged_analysis_set_span(&analysis->tokens[i], parsed, i);
	analysis->tokens[i].role = GED_CMD_TOKEN_UNKNOWN;
	analysis->tokens[i].value_type = BU_CMD_VALUE_UNKNOWN;
	analysis->tokens[i].semantic_state = GED_CMD_SEMANTIC_UNKNOWN;
	analysis->tokens[i].validator = NULL;
	analysis->tokens[i].hint = NULL;
    }

    analysis->tokens[0].role = GED_CMD_TOKEN_COMMAND;
    analysis->tokens[0].semantic_state = ged_cmd_exists(parsed.argv[0]) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
    analysis->tokens[0].hint = ged_cmd_exists(parsed.argv[0]) ? "valid command" : "unknown command";

    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(parsed.argv[0]);
    if (grammar && grammar->analyze) {
	int ret = grammar->analyze(gedp, input, analysis);
	ged_input_parse_free(&parsed);
	return ret;
    }

    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(parsed.argv[0]);
    if (native_schema) {
	ged_native_analyze(gedp, native_schema, parsed, analysis);
	ged_input_parse_free(&parsed);
	return 0;
    }

    ged_input_parse_free(&parsed);
    return 0;
}

/* Translate a compact native schema through GED's semantic providers.  Grammar
 * adapters with context-selected flat forms use this same path, rather than
 * reimplementing completion or token-state rules. */
static int
ged_native_validate(struct ged *gedp, const struct bu_cmd_schema *native_schema,
	const struct ged_input_parse &parsed, const char *input, size_t cursor_pos,
	const std::string &seed, struct ged_cmd_validate_result *result)
{
    struct bu_cmd_validate_result native_result = BU_CMD_VALIDATE_RESULT_NULL;
    size_t native_argc = (parsed.cursor_arg < parsed.argc) ?
	parsed.cursor_arg : parsed.argc - 1;
    size_t native_cursor = (parsed.cursor_arg < parsed.argc) ?
	parsed.cursor_arg - 1 : native_argc;
    int ret = bu_cmd_schema_validate_ctx(native_schema, native_argc,
	(const char **)parsed.argv + 1, native_cursor, gedp, &native_result);

    if (ret == 0) {
	const char *provider = ged_native_semantic_provider(native_result.completion_type,
	    native_result.semantic_provider);
	ged_cmd_validate_result_clear(result);
	result->state = ged_native_state(native_result.state);
	result->token_start = native_result.token_start + 1;
	result->token_end = native_result.token_end + 1;
	result->expected = ged_native_expected(native_result.expected);
	result->hint = native_result.hint;
	result->completion_type = ged_native_value_type(native_result.completion_type, provider);
	result->semantic_provider = provider;
	result->completion_count = native_result.completion_count;
	result->completion_candidates = native_result.completion_candidates;
	native_result.completion_count = 0;
	native_result.completion_candidates = NULL;
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	/* The cursor-local result describes the token being edited.  At end of
	 * input, retain that role but report a missing later operand or violated
	 * relationship from the fully accepted prefix. */
	if (result->state == BU_CMD_VALIDATE_VALID && cursor_pos == strlen(input) &&
	    parsed.cursor_arg < parsed.argc && native_cursor < native_argc) {
	    struct bu_cmd_validate_result full_result = BU_CMD_VALIDATE_RESULT_NULL;
	if (bu_cmd_schema_validate_ctx(native_schema, native_argc,
		(const char **)parsed.argv + 1, native_argc, gedp, &full_result) == 0 &&
		full_result.state != BU_CMD_VALIDATE_VALID &&
		full_result.state != BU_CMD_VALIDATE_UNKNOWN) {
		result->state = ged_native_state(full_result.state);
	    }
	    bu_cmd_validate_result_clear(&full_result);
	}
	if (result->state == BU_CMD_VALIDATE_VALID && provider &&
	    parsed.cursor_arg < parsed.argc && parsed.argv[parsed.cursor_arg]) {
	    ged_cmd_semantic_state_t semantic_state = ged_semantic_validate(gedp,
		result->completion_type, provider, parsed.argv[parsed.cursor_arg]);
	    if (semantic_state == GED_CMD_SEMANTIC_INVALID)
		result->state = BU_CMD_VALIDATE_INVALID;
	    else if (semantic_state == GED_CMD_SEMANTIC_INCOMPLETE)
		result->state = BU_CMD_VALIDATE_INCOMPLETE;
	}
	/* A custom schema validator may reject a geometry-shaped token even
	 * though its generic semantic provider can enumerate object paths.  Do
	 * not offer those candidates: accepting one would leave the command in
	 * the same invalid syntactic form (for example, an incomplete a/b arc). */
	if (native_result.state != BU_CMD_VALIDATE_INVALID) {
	    if (!result->completion_count && provider)
		(void)ged_semantic_complete(gedp, provider, seed.c_str(), result);
	    if (!result->completion_count &&
		(result->completion_type == BU_CMD_VALUE_DB_OBJECT || result->completion_type == BU_CMD_VALUE_DB_PATH)) {
		ged_fill_geometry_candidates(gedp, seed, result);
	    }
	    if (!result->completion_count && result->completion_type == BU_CMD_VALUE_FILE) {
		char **files = NULL;
		size_t file_cnt = bu_file_complete(seed.c_str(), BU_FILE_COMPLETE_APPEND_SLASH, NULL, &files);
		result->completion_candidates = (const char **)files;
		result->completion_count = file_cnt;
	    }
	}
    }
    bu_cmd_validate_result_clear(&native_result);
    return ret;
}


extern "C" int
ged_cmd_validate(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    std::string seed;
    const char *cmd = NULL;
    int cmd_exists = 0;

    if (!input || !result)
	return -1;

    (void)ged_input_parse_line(&parsed, input, cursor_pos);
    seed = ged_cursor_seed(parsed);
    cmd = (parsed.argc > 0) ? parsed.argv[0] : NULL;
    cmd_exists = (cmd) ? ged_cmd_exists(cmd) : 0;

    if (!parsed.argc || parsed.cursor_arg == 0 || !cmd_exists) {
	bu_cmd_validate_state_t state = (!parsed.argc) ? BU_CMD_VALIDATE_INCOMPLETE :
	    (cmd_exists ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_UNKNOWN);
	const char *hint = (!parsed.argc) ? "command expected" :
	    (cmd_exists ? "valid command" : "unknown command");
	ged_set_validate_result(result, state, 0, 0, BU_CMD_EXPECT_SUBCOMMAND, hint);
	ged_set_result_chars(result, parsed, 0, 0);
	ged_fill_command_candidates(result, seed);
	ged_input_parse_free(&parsed);
	return 0;
    }

    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->validate) {
	int ret = grammar->validate(gedp, input, cursor_pos, result);
	ged_input_parse_free(&parsed);
	return ret;
    }

    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
    if (native_schema) {
	int ret = ged_native_validate(gedp, native_schema, parsed, input, cursor_pos, seed, result);
	ged_input_parse_free(&parsed);
	return ret;
    }


    size_t token_index = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg : parsed.argc;
    ged_set_validate_result(result, BU_CMD_VALIDATE_UNKNOWN, token_index, token_index,
	BU_CMD_EXPECT_NONE, "syntax metadata unavailable");
    ged_set_result_chars(result, parsed, token_index, token_index);
    if (parsed.cursor_arg > 0 && gedp && gedp->dbip) {
	result->completion_type = (seed.find('/') != std::string::npos) ?
	    BU_CMD_VALUE_DB_PATH : BU_CMD_VALUE_DB_OBJECT;
	ged_fill_geometry_candidates(gedp, seed, result);
    }
    ged_input_parse_free(&parsed);
    return 0;
}


/* Erase selects a deliberately different native form according to the GED
 * command-form setting.  Registering it as a grammar adapter lets the editor
 * make that context-sensitive selection without advertising one form's
 * options in the other form. */
static const struct bu_cmd_schema *
ged_erase_active_schema(const struct ged *gedp)
{
    return gedp && gedp->new_cmd_forms ? &ged_erase_new_schema :
	&ged_erase_legacy_schema;
}


static int
ged_erase_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
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
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    seed = ged_cursor_seed(parsed);
    ret = ged_native_validate(gedp, ged_erase_active_schema(gedp), parsed,
	input, cursor_pos, seed, result);
    ged_input_parse_free(&parsed);
    return ret;
}


static int
ged_erase_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

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
	sizeof(struct ged_cmd_analysis_token), "erase grammar analysis tokens");
    analysis->token_count = parsed.argc;
    for (size_t i = 0; i < parsed.argc; i++) {
	ged_analysis_set_span(&analysis->tokens[i], parsed, i);
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
    ged_native_analyze(gedp, ged_erase_active_schema(gedp), parsed, analysis);
    ged_input_parse_free(&parsed);
    return 0;
}


static char *
ged_erase_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *legacy = bu_cmd_schema_describe_json(&ged_erase_legacy_schema);
    char *new_form = bu_cmd_schema_describe_json(&ged_erase_new_schema);

    bu_vls_strcat(&out, "{\"name\":\"erase\",\"kind\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"parse_policy\":\"context_selected_form\",");
    bu_vls_strcat(&out, "\"help\":\"Erase database paths from the display\",");
    bu_vls_printf(&out, "\"forms\":{\"legacy\":%s,\"new\":%s}}",
	legacy ? legacy : "{}", new_form ? new_form : "{}");
    if (legacy)
	bu_free(legacy, "erase legacy schema JSON");
    if (new_form)
	bu_free(new_form, "erase new schema JSON");
    return bu_vls_strdup(&out);
}


static int
ged_erase_grammar_lint(struct bu_vls *msgs)
{
    int failures = 0;

    if (!ged_erase_legacy_schema.options || !ged_erase_legacy_schema.operands ||
	!ged_erase_legacy_schema.validation.custom_validate) {
	if (msgs)
	    bu_vls_strcat(msgs, "erase: legacy native form is incomplete\n");
	failures++;
    }
    if (!ged_erase_new_schema.options || !ged_erase_new_schema.operands ||
	ged_erase_new_schema.parse_policy != BU_CMD_PARSE_INTERSPERSED) {
	if (msgs)
	    bu_vls_strcat(msgs, "erase: new native form is incomplete\n");
	failures++;
    }
    return failures;
}


extern "C" GED_EXPORT const struct ged_cmd_grammar ged_erase_grammar = {
    "erase", "Erase database paths from the display", ged_erase_grammar_validate,
    ged_erase_grammar_analyze, ged_erase_grammar_json, ged_erase_grammar_lint
};


/* A native tree has a flat root option phase and parser-owned subcommands.
 * Each node publishes an executable schema, allowing an adapter to delegate
 * cursor validation and analysis without rebuilding a command-specific table. */
static size_t
ged_native_tree_subcommand_index(const struct bu_cmd_tree *tree,
	const struct ged_input_parse &parsed)
{
    size_t required_operands = 0;
    size_t operand_count = 0;

    if (!tree || !tree->root_schema)
	return parsed.argc;
    if (tree->child_phase == BU_CMD_TREE_CHILD_FIRST)
	return parsed.argc > 1 ? 1 : parsed.argc;
    if (tree->child_phase == BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS) {
	if (!tree->root_schema->operands)
	    return parsed.argc;
	for (size_t oi = 0; tree->root_schema->operands[oi].name; oi++) {
	    const struct bu_cmd_operand *operand = &tree->root_schema->operands[oi];
	    if (operand->min_count != operand->max_count ||
		operand->max_count == BU_CMD_COUNT_UNLIMITED ||
		(operand->shape && (operand->shape->min_tokens != 1 ||
		    operand->shape->max_tokens != 1 || operand->shape->token_count)))
		return parsed.argc;
	    required_operands += operand->min_count;
	}
	if (!required_operands)
	    return parsed.argc;
	for (size_t i = 1; i < parsed.argc; i++) {
	    const char *arg = parsed.argv[i];
	    int option_span;
	    if (operand_count >= required_operands)
		return i;
	    option_span = bu_cmd_schema_option_span(tree->root_schema,
		parsed.argc - i, (const char **)parsed.argv + i);
	    if (option_span > 0) {
		i += (size_t)option_span - 1;
		continue;
	    }
	    if (option_span < 0 || (arg[0] == '-' && arg[1]))
		return parsed.argc;
	    operand_count++;
	}
	return parsed.argc;
    }
    for (size_t i = 1; i < parsed.argc; i++) {
	const char *arg = parsed.argv[i];
	int option_span = bu_cmd_schema_option_span(tree->root_schema,
	    parsed.argc - i, (const char **)parsed.argv + i);
	if (option_span > 0) {
	    i += (size_t)option_span - 1;
	    continue;
	}
	/* A malformed option remains in the root phase so the schema can report
	 * its precise error.  A non-option word starts the subcommand phase,
	 * including an unknown partial word that should receive child candidates. */
	if (option_span < 0 || (arg[0] == '-' && arg[1]))
	    return parsed.argc;
	return i;
    }
    return parsed.argc;
}


static void
ged_native_tree_subcommand_candidates(const struct bu_cmd_tree *tree,
	struct ged_cmd_validate_result *result, const std::string &seed)
{
	std::vector<std::string> candidates;

    for (size_t i = 0; tree && tree->subcommands && tree->subcommands[i].schema; i++)
	ged_search_add_candidate(candidates,
	    tree->subcommands[i].schema->name, seed);
    ged_search_set_candidates(result, candidates, BU_CMD_VALUE_KEYWORD);
}


/* This is a non-owning, child-relative view.  It deliberately has no copy of
 * argv or input text; its vectors only preserve the absolute source spans
 * needed by the native validator to report a replacement range. */
static struct ged_input_parse
ged_native_tree_subcommand_parse(const struct ged_input_parse &parsed,
	size_t subcommand_index)
{
    struct ged_input_parse child;

    child.argv = parsed.argv + subcommand_index;
    child.argc = parsed.argc - subcommand_index;
    child.cursor_arg = parsed.cursor_arg >= subcommand_index ?
	parsed.cursor_arg - subcommand_index : 0;
    child.input_len = parsed.input_len;
    child.char_starts.assign(parsed.char_starts.begin() + subcommand_index,
	parsed.char_starts.end());
    child.char_ends.assign(parsed.char_ends.begin() + subcommand_index,
	parsed.char_ends.end());
    return child;
}


static int
ged_native_tree_validate_level(struct ged *gedp, const struct bu_cmd_tree *tree,
	const struct ged_input_parse &parsed, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result, size_t token_offset)
{
    struct ged_input_parse root;
    struct bu_cmd_validate_result root_result = BU_CMD_VALIDATE_RESULT_NULL;
    std::string seed;
    size_t subcommand_index;
    size_t root_argc;
    int ret = 0;

    if (!tree || !tree->root_schema || !input || !result || !parsed.argc)
	return -1;
    seed = ged_cursor_seed(parsed);
    subcommand_index = ged_native_tree_subcommand_index(tree, parsed);
    root_argc = subcommand_index < parsed.argc ? subcommand_index : parsed.argc;

    /* A token in the root phase is delegated unchanged to the native flat
     * validator.  Truncating the view at the selected subcommand prevents it
     * from mistaking that command word for an invalid operand. */
    if (parsed.cursor_arg < root_argc) {
	root = parsed;
	root.argc = root_argc;
	ret = ged_native_validate(gedp, tree->root_schema, root, input,
	    cursor_pos, seed, result);
	result->token_start += token_offset;
	result->token_end += token_offset;
	return ret;
    }

    if (bu_cmd_schema_validate(tree->root_schema, root_argc - 1,
	(const char **)parsed.argv + 1, root_argc - 1, &root_result) != 0 ||
	(root_result.state == BU_CMD_VALIDATE_INVALID ||
	 root_result.state == BU_CMD_VALIDATE_INCOMPLETE)) {
	root = parsed;
	root.argc = root_argc;
	ret = ged_native_validate(gedp, tree->root_schema, root, input,
	    cursor_pos, seed, result);
	bu_cmd_validate_result_clear(&root_result);
	result->token_start += token_offset;
	result->token_end += token_offset;
	return ret;
    }
    bu_cmd_validate_result_clear(&root_result);

    if (subcommand_index >= parsed.argc) {
	ged_set_validate_result(result, BU_CMD_VALIDATE_INCOMPLETE,
	    parsed.cursor_arg, parsed.cursor_arg, BU_CMD_EXPECT_SUBCOMMAND,
	    "subcommand expected");
	result->completion_type = BU_CMD_VALUE_KEYWORD;
	ged_native_tree_subcommand_candidates(tree, result, seed);
	if (!seed.empty() && !result->completion_count) {
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->hint = "unknown subcommand";
	}
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	result->token_start += token_offset;
	result->token_end += token_offset;
	return 0;
    }

    if (parsed.cursor_arg == subcommand_index) {
	int exact = bu_cmd_tree_find_subcommand(tree,
	    parsed.argv[subcommand_index]) != NULL;
	ged_set_validate_result(result, exact ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE,
	    subcommand_index, subcommand_index, BU_CMD_EXPECT_SUBCOMMAND,
	    exact ? "subcommand" : "subcommand expected");
	result->completion_type = BU_CMD_VALUE_KEYWORD;
	ged_native_tree_subcommand_candidates(tree, result, seed);
	if (!exact && !result->completion_count) {
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->hint = "unknown subcommand";
	}
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	result->token_start += token_offset;
	result->token_end += token_offset;
	return 0;
    }

    const struct bu_cmd_tree_node *child_node = bu_cmd_tree_find_subcommand(
	tree, parsed.argv[subcommand_index]);
    const struct bu_cmd_schema *child_schema = child_node ? child_node->schema : NULL;
    if (!child_schema) {
	ged_set_validate_result(result, BU_CMD_VALIDATE_INVALID, subcommand_index,
	    subcommand_index, BU_CMD_EXPECT_SUBCOMMAND, "unknown subcommand");
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	result->token_start += token_offset;
	result->token_end += token_offset;
	return 0;
    }
    struct ged_input_parse child = ged_native_tree_subcommand_parse(parsed, subcommand_index);
	if (child_node->subcommands) {
	    const struct bu_cmd_tree child_tree = {
		child_schema, child_node->subcommands, child_node->child_phase
	    };
	    return ged_native_tree_validate_level(gedp, &child_tree, child, input,
		cursor_pos, result, token_offset + subcommand_index);
	}
    ret = ged_native_validate(gedp, child_schema, child, input, cursor_pos,
	ged_cursor_seed(child), result);
	result->token_start += token_offset + subcommand_index;
	result->token_end += token_offset + subcommand_index;
    return ret;
}


extern "C" GED_EXPORT int
ged_cmd_tree_validate(struct ged *gedp, const struct bu_cmd_tree *tree,
	const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    int ret = 0;

    if (!tree || !tree->root_schema || !input || !result)
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    ret = ged_native_tree_validate_level(gedp, tree, parsed, input,
	cursor_pos, result, 0);
    ged_input_parse_free(&parsed);
    return ret;
}


static int
ged_select_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    std::string seed;
    int ret;

    if (gedp && gedp->new_cmd_forms)
	return ged_cmd_tree_validate(gedp, &ged_select_new_tree, input, cursor_pos, result);
    if (!input || !result)
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    seed = ged_cursor_seed(parsed);
    ret = ged_native_validate(gedp, &ged_select_legacy_schema, parsed, input,
	cursor_pos, seed, result);
    ged_input_parse_free(&parsed);
    return ret;
}


static void
ged_native_tree_analysis_initialize(struct ged_cmd_analysis *analysis,
	const struct ged_input_parse &parsed)
{
    analysis->tokens = (struct ged_cmd_analysis_token *)bu_calloc(parsed.argc,
	sizeof(struct ged_cmd_analysis_token), "native tree grammar analysis tokens");
    analysis->token_count = parsed.argc;
    for (size_t i = 0; i < parsed.argc; i++) {
	ged_analysis_set_span(&analysis->tokens[i], parsed, i);
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
}


static int
ged_native_tree_analyze_level(struct ged *gedp, const struct bu_cmd_tree *tree,
	const struct ged_input_parse &parsed, struct ged_cmd_analysis *analysis)
{
    if (!tree || !tree->root_schema || !analysis || !parsed.argc)
	return -1;

    size_t subcommand_index = ged_native_tree_subcommand_index(tree, parsed);
    struct ged_input_parse root = parsed;
    root.argc = subcommand_index < parsed.argc ? subcommand_index : parsed.argc;
    ged_native_analyze(gedp, tree->root_schema, root, analysis);
    if (subcommand_index < parsed.argc) {
	struct ged_cmd_analysis_token *subcommand = &analysis->tokens[subcommand_index];
	const struct bu_cmd_tree_node *child_node = bu_cmd_tree_find_subcommand(
	    tree, parsed.argv[subcommand_index]);
	const struct bu_cmd_schema *child_schema = child_node ? child_node->schema : NULL;
	subcommand->role = GED_CMD_TOKEN_SUBCOMMAND;
	subcommand->value_type = BU_CMD_VALUE_KEYWORD;
	subcommand->semantic_state = child_schema ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
	subcommand->hint = child_schema ? "subcommand" : "unknown subcommand";
	if (child_schema) {
	    struct ged_input_parse child = ged_native_tree_subcommand_parse(parsed, subcommand_index);
	    struct ged_cmd_analysis child_analysis = *analysis;
	    child_analysis.tokens = analysis->tokens + subcommand_index;
	    child_analysis.token_count = child.argc;
	    if (child_node->subcommands) {
		const struct bu_cmd_tree child_tree = {
		    child_schema, child_node->subcommands, child_node->child_phase
		};
		(void)ged_native_tree_analyze_level(gedp, &child_tree, child,
		    &child_analysis);
	    } else {
		ged_native_analyze(gedp, child_schema, child, &child_analysis);
	    }
	}
    }
    return 0;
}


extern "C" GED_EXPORT int
ged_cmd_tree_analyze(struct ged *gedp, const struct bu_cmd_tree *tree,
	const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;
    int ret = 0;

    if (!tree || !tree->root_schema || !input || !analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    ret = ged_native_tree_analyze_level(gedp, tree, parsed, analysis);
    ged_input_parse_free(&parsed);
    return ret;
}


static const struct ged_cmd_native_form *
ged_native_forms_selected(const struct ged_cmd_native_form *forms,
	ged_cmd_native_form_select_t select, const struct ged *gedp,
	size_t argc, const char * const *argv)
{
    const struct ged_cmd_native_form *selected = select ? select(gedp, argc, argv) : NULL;

    if (!forms || !selected)
	return NULL;
    for (size_t i = 0; forms[i].name; i++) {
	if (&forms[i] == selected && ((forms[i].schema && !forms[i].tree) ||
		(forms[i].tree && !forms[i].schema)))
	    return selected;
    }
    return NULL;
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_validate(struct ged *gedp, const struct ged_cmd_native_form *forms,
	ged_cmd_native_form_select_t select, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    const struct ged_cmd_native_form *form;
    int ret = 0;

    if (!forms || !select || !input || !result)
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    form = ged_native_forms_selected(forms, select, gedp, parsed.argc,
	(const char * const *)parsed.argv);
    if (!form) {
	ged_set_validate_result(result, BU_CMD_VALIDATE_INVALID, parsed.cursor_arg,
	    parsed.cursor_arg, BU_CMD_EXPECT_NONE, "no matching native command form");
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	ged_input_parse_free(&parsed);
	return 0;
    }
    if (form->tree) {
	ret = ged_native_tree_validate_level(gedp, form->tree, parsed, input,
	    cursor_pos, result, 0);
    } else {
	ret = ged_native_validate(gedp, form->schema, parsed, input, cursor_pos,
	    ged_cursor_seed(parsed), result);
    }
    ged_input_parse_free(&parsed);
    return ret;
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_analyze(struct ged *gedp, const struct ged_cmd_native_form *forms,
	ged_cmd_native_form_select_t select, const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;
    const struct ged_cmd_native_form *form;
    int ret = 0;

    if (!forms || !select || !input || !analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    form = ged_native_forms_selected(forms, select, gedp, parsed.argc,
	(const char * const *)parsed.argv);
    if (!form) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    if (form->tree)
	ret = ged_native_tree_analyze_level(gedp, form->tree, parsed, analysis);
    else
	ged_native_analyze(gedp, form->schema, parsed, analysis);
    ged_input_parse_free(&parsed);
    return ret;
}


extern "C" GED_EXPORT char *
ged_cmd_native_forms_describe_json(const char *command, const char *help,
	const struct ged_cmd_native_form *forms)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;

    if (!command || !forms)
	return NULL;
    bu_vls_strcat(&out, "{\"name\":");
    bu_vls_printf(&out, "\"%s\"", command);
    bu_vls_strcat(&out, ",\"kind\":\"native_forms\",\"help\":");
    bu_vls_printf(&out, "\"%s\"", help ? help : "");
    bu_vls_strcat(&out, ",\"forms\":{");
    for (size_t i = 0; forms[i].name; i++) {
	char *description = forms[i].tree ? bu_cmd_tree_describe_json(forms[i].tree) :
	    bu_cmd_schema_describe_json(forms[i].schema);
	if (i)
	    bu_vls_putc(&out, ',');
	bu_vls_printf(&out, "\"%s\":%s", forms[i].name,
	    description ? description : "{}");
	if (description)
	    bu_free(description, "native command form JSON");
    }
    bu_vls_strcat(&out, "}}");
    return bu_vls_strdup(&out);
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_lint(const char *command, const struct ged_cmd_native_form *forms,
	struct bu_vls *msgs)
{
    int failures = 0;

    if (!command || !forms) {
	if (msgs)
	    bu_vls_strcat(msgs, "native command forms are missing\n");
	return 1;
    }
    for (size_t i = 0; forms[i].name; i++) {
	const struct ged_cmd_native_form *form = &forms[i];
	if (!form->name[0] || (form->schema && form->tree) || (!form->schema && !form->tree)) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: malformed native form\n", command);
	    failures++;
	    continue;
	}
	for (size_t j = 0; j < i; j++) {
	    if (BU_STR_EQUAL(forms[j].name, form->name)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: duplicate native form %s\n", command, form->name);
		failures++;
		break;
	    }
	}
	if (form->tree)
	    failures += bu_cmd_tree_lint(form->tree, msgs);
    }
    return failures;
}


static int
ged_select_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

    if (gedp && gedp->new_cmd_forms)
	return ged_cmd_tree_analyze(gedp, &ged_select_new_tree, input, analysis);
    if (!input || !analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    ged_native_analyze(gedp, &ged_select_legacy_schema, parsed, analysis);
    ged_input_parse_free(&parsed);
    return 0;
}


static char *
ged_select_grammar_json(void)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *legacy = bu_cmd_schema_describe_json(&ged_select_legacy_schema);
    char *new_form = bu_cmd_tree_describe_json(&ged_select_new_tree);

    bu_vls_strcat(&out, "{\"name\":\"select\",\"kind\":\"grammar_adapter\",");
    bu_vls_strcat(&out, "\"parse_policy\":\"context_selected_form\",");
    bu_vls_strcat(&out, "\"help\":\"Manage display selection and selection sets\",");
    bu_vls_printf(&out, "\"forms\":{\"legacy\":%s,\"new\":%s}",
	legacy ? legacy : "{}", new_form ? new_form : "{}");
    bu_vls_putc(&out, '}');
    if (legacy)
	bu_free(legacy, "select legacy schema JSON");
    if (new_form)
	bu_free(new_form, "select new schema JSON");
    return bu_vls_strdup(&out);
}


static int
ged_select_grammar_lint(struct bu_vls *msgs)
{
    int failures = 0;

    if (!ged_select_legacy_schema.options || !ged_select_legacy_schema.operands ||
	!ged_select_new_schema.options) {
	if (msgs)
	    bu_vls_strcat(msgs, "select: native command forms are incomplete\n");
	failures++;
    }
    failures += bu_cmd_tree_lint(&ged_select_new_tree, msgs);
    return failures;
}


static int
ged_view_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_view_tree, input, cursor_pos, result);
}


static int
ged_view2_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &ged_view2_tree, input, cursor_pos, result);
}


static int
ged_view_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_view_tree, input, analysis);
}


static int
ged_view2_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &ged_view2_tree, input, analysis);
}


static char *
ged_view_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_view_tree);
}


static char *
ged_view2_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&ged_view2_tree);
}


static int
ged_view_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_view_tree, msgs);
}


static int
ged_view2_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&ged_view2_tree, msgs);
}


static int
ged_rselect_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    std::string seed;
    int ret;

    if (!input || !result)
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    seed = ged_cursor_seed(parsed);
    ret = ged_native_validate(gedp, &ged_rselect_legacy_schema, parsed, input,
	cursor_pos, seed, result);
    ged_input_parse_free(&parsed);
    return ret;
}


static int
ged_rselect_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

    if (!input || !analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    ged_native_analyze(gedp, &ged_rselect_legacy_schema, parsed, analysis);
    ged_input_parse_free(&parsed);
    return 0;
}


static char *
ged_rselect_grammar_json(void)
{
    return bu_cmd_schema_describe_json(&ged_rselect_legacy_schema);
}


static int
ged_rselect_grammar_lint(struct bu_vls *msgs)
{
    if (ged_rselect_legacy_schema.options)
	return 0;
    if (msgs)
	bu_vls_strcat(msgs, "rselect: native schema is incomplete\n");
    return 1;
}


extern "C" GED_EXPORT const struct ged_cmd_grammar ged_select_grammar = {
    "select", "Manage display selection and selection sets", ged_select_grammar_validate,
    ged_select_grammar_analyze, ged_select_grammar_json, ged_select_grammar_lint
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_view_grammar = {
    "view", "Inspect and manipulate named views", ged_view_grammar_validate,
    ged_view_grammar_analyze, ged_view_grammar_json, ged_view_grammar_lint
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_view2_grammar = {
    "view2", "Inspect and manipulate named views", ged_view2_grammar_validate,
    ged_view2_grammar_analyze, ged_view2_grammar_json, ged_view2_grammar_lint
};

extern "C" GED_EXPORT const struct ged_cmd_grammar ged_rselect_grammar = {
    "rselect", "Select using the current rubber-band region", ged_rselect_grammar_validate,
    ged_rselect_grammar_analyze, ged_rselect_grammar_json, ged_rselect_grammar_lint
};


extern "C" int
ged_cmd_complete_result(struct ged *gedp, const char *input, size_t cursor_pos, struct ged_cmd_completion_result *result)
{
    struct ged_input_parse parsed;
    struct ged_cmd_validate_result vr = GED_CMD_VALIDATE_RESULT_NULL;
    std::string seed;
    size_t input_len = 0;
    size_t prefix_len = 0;

    if (!input || !result)
	return 0;

    ged_cmd_completion_result_clear(result);
    input_len = strlen(input);
    if (cursor_pos > input_len)
	cursor_pos = input_len;

    (void)ged_input_parse_line(&parsed, input, cursor_pos);
    seed = ged_cursor_seed(parsed);
    (void)ged_cmd_validate(gedp, input, cursor_pos, &vr);

    /* Custom validators may expose a common option table while allowing only
     * a frontend-specific subset.  Revalidate each offered option spelling in
     * the current context so completion never suggests syntax the command's
     * own validator rejects. */
    if (!seed.empty() && seed[0] == '-' && vr.completion_candidates &&
	    (vr.expected & BU_CMD_EXPECT_OPTION) && cursor_pos >= seed.size()) {
	size_t replace_start = cursor_pos - seed.size();
	size_t write_idx = 0;
	for (size_t i = 0; i < vr.completion_count; i++) {
	    const char *candidate = vr.completion_candidates[i];
	    bool keep = candidate != NULL;
	    if (keep && candidate[0] == '-') {
		std::string probe(input);
		probe.replace(replace_start, cursor_pos - replace_start, candidate);
		struct ged_cmd_validate_result candidate_vr = GED_CMD_VALIDATE_RESULT_NULL;
		(void)ged_cmd_validate(gedp, probe.c_str(), replace_start + strlen(candidate), &candidate_vr);
		keep = candidate_vr.state != BU_CMD_VALIDATE_INVALID;
		ged_cmd_validate_result_clear(&candidate_vr);
	    }
	    if (keep) {
		vr.completion_candidates[write_idx++] = candidate;
	    } else if (candidate) {
		bu_free((void *)candidate, "invalid contextual option completion");
	    }
	}
	vr.completion_count = write_idx;
	vr.completion_candidates[write_idx] = NULL;
    }

    /* A complete option that requires a separate argument is still the token
     * under the cursor.  Do not replace it with candidates for that later
     * argument until the user supplies a separator.  Besides preserving the
     * option spelling, this keeps a second Tab on "-t" from turning it into
     * a format keyword; after "-t " the argument token has its own span and
     * normal keyword cycling applies. */
    if (parsed.cursor_arg < parsed.argc && cursor_pos == parsed.char_ends[parsed.cursor_arg] &&
	    cursor_pos > parsed.char_starts[parsed.cursor_arg] && !seed.empty() &&
	    seed[0] == '-' && (vr.expected & BU_CMD_EXPECT_OPTION_ARG) &&
	    vr.char_start == parsed.char_starts[parsed.cursor_arg] &&
	    vr.char_end == parsed.char_ends[parsed.cursor_arg]) {
	ged_cmd_validate_result_clear(&vr);
	ged_input_parse_free(&parsed);
	return 0;
    }

    /* Do not expose candidates for the next syntactic phase until the user
     * has typed a separator.  Otherwise a completed option or subcommand can
     * be replaced by unrelated candidates belonging after it. */
    if (parsed.cursor_arg < parsed.argc && cursor_pos == parsed.char_ends[parsed.cursor_arg] &&
	    cursor_pos > parsed.char_starts[parsed.cursor_arg] &&
	    vr.char_start == cursor_pos && vr.char_end == cursor_pos && !seed.empty()) {
	bool current_seed_match = false;
	for (size_t i = 0; i < vr.completion_count; i++) {
	    if (vr.completion_candidates[i] &&
		    strncmp(vr.completion_candidates[i], seed.c_str(), seed.size()) == 0) {
		current_seed_match = true;
		break;
	    }
	}
	if (!current_seed_match) {
	    ged_cmd_validate_result_clear(&vr);
	    ged_input_parse_free(&parsed);
	    return 0;
	}
    }

    result->replacement_start = (vr.char_start <= cursor_pos) ? vr.char_start : cursor_pos;
    result->replacement_end = cursor_pos;
    result->completion_type = vr.completion_type;
    result->expected = vr.expected;
    result->hint = vr.hint;
    result->options_legal = (vr.expected & BU_CMD_EXPECT_OPTION) ? 1 : 0;
    result->starts_new_phase = (vr.expected & BU_CMD_EXPECT_SUBCOMMAND) ? 1 : 0;
    result->active_role = bu_strdup(vr.hint ? vr.hint : "");

    struct ged_cmd_analysis active_analysis = {0, NULL};
    if (ged_cmd_analyze(gedp, input, &active_analysis) == 0) {
	struct bu_vls active_path = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < active_analysis.token_count; i++) {
	    const struct ged_cmd_analysis_token *token = &active_analysis.tokens[i];
	    if (token->role != GED_CMD_TOKEN_COMMAND && token->role != GED_CMD_TOKEN_SUBCOMMAND)
		continue;
	    if (token->char_start >= cursor_pos || token->char_end > input_len || token->char_end < token->char_start)
		continue;
	    if (bu_vls_strlen(&active_path)) bu_vls_putc(&active_path, ' ');
	    bu_vls_strncat(&active_path, input + token->char_start, token->char_end - token->char_start);
	}
	result->active_command_path = bu_vls_strdup(&active_path);
	bu_vls_free(&active_path);
    }

    bool schema_option_candidates = (vr.completion_count && !seed.empty() && seed[0] == '-' &&
	(vr.expected & BU_CMD_EXPECT_OPTION));
    /* Do not synthesize generic geometry candidates after a schema validator
     * rejects a command-specific form (such as a required parent/member arc).
     * Candidates already produced by native validation are retained below. */
    /* A semantically invalid path may still carry candidates gathered during
     * native validation; recompute those through this path-aware API so the
     * replacement range is narrowed to its final component. */
    if ((vr.state != BU_CMD_VALIDATE_INVALID || vr.completion_count) && !schema_option_candidates &&
	(vr.completion_type == BU_CMD_VALUE_DB_OBJECT || vr.completion_type == BU_CMD_VALUE_DB_PATH) &&
	gedp && gedp->dbip) {
	struct bu_vls gprefix = BU_VLS_INIT_ZERO;
	struct ged_cmd_validate_result gvr = GED_CMD_VALIDATE_RESULT_NULL;
	gvr.completion_type = vr.completion_type;
	ged_fill_geometry_candidates(gedp, seed, &gvr, &gprefix);
	result->completion_candidates = gvr.completion_candidates;
	result->completion_count = gvr.completion_count;
	result->completion_type = gvr.completion_type;
	gvr.completion_candidates = NULL;
	gvr.completion_count = 0;
	result->prefix = bu_strdup(bu_vls_cstr(&gprefix));
	ged_cmd_validate_result_clear(&gvr);
	bu_vls_free(&gprefix);
    } else {
	result->completion_candidates = vr.completion_candidates;
	result->completion_count = vr.completion_count;
	vr.completion_candidates = NULL;
	vr.completion_count = 0;
	result->prefix = bu_strdup(seed.c_str());
    }

    prefix_len = result->prefix ? strlen(result->prefix) : 0;
    if (cursor_pos >= prefix_len) {
	size_t rstart = cursor_pos - prefix_len;
	/* The validator may report an insertion point after a fully recognized
	 * option or subcommand, even though completion is still describing the
	 * token under the cursor.  The returned prefix is the text the candidate
	 * replaces (and, for paths, intentionally only the final component), so
	 * it is authoritative for the interactive replacement start. */
	result->replacement_start = rstart;
    }

    ged_cmd_validate_result_clear(&vr);
    ged_input_parse_free(&parsed);

    return (int)result->completion_count;
}

extern "C" void
ged_cmd_completion_result_clear(struct ged_cmd_completion_result *result)
{
    if (!result)
	return;

    if (result->completion_candidates)
	bu_argv_free(result->completion_count, (char **)result->completion_candidates);
    if (result->prefix)
	bu_free(result->prefix, "completion prefix");
    if (result->active_command_path)
	bu_free(result->active_command_path, "active command path");
    if (result->active_role)
	bu_free(result->active_role, "active command role");

    result->completion_count = 0;
    result->completion_candidates = NULL;
    result->replacement_start = 0;
    result->replacement_end = 0;
    result->prefix = NULL;
    result->completion_type = BU_CMD_VALUE_UNKNOWN;
    result->expected = BU_CMD_EXPECT_NONE;
    result->hint = NULL;
    result->active_command_path = NULL;
    result->active_role = NULL;
    result->options_legal = 0;
    result->starts_new_phase = 0;
}

extern "C" int
ged_cmd_complete(const char ***completions, struct bu_vls *prefix, struct ged *gedp, const char *input, size_t cursor_pos)
{
    struct ged_cmd_completion_result result = GED_CMD_COMPLETION_RESULT_NULL;
    int ret = 0;

    if (!completions || !input)
	return 0;

    *completions = NULL;
    if (prefix)
	bu_vls_trunc(prefix, 0);

    ret = ged_cmd_complete_result(gedp, input, cursor_pos, &result);
    if (prefix && result.prefix)
	bu_vls_sprintf(prefix, "%s", result.prefix);
    *completions = result.completion_candidates;
    result.completion_candidates = NULL;
    result.completion_count = 0;
    ged_cmd_completion_result_clear(&result);

    return ret;
}

int
ged_geom_completions(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *seed)
{
    int ret = 0;

    if (!dbip || !prefix || !completions || !seed)
	return 0;

    /* A slash denotes an interactive hierarchy path unless that path cannot
     * be resolved.  Checking literal directory names first loses the prefix
     * before the final slash, so a completed path cannot be extended with
     * another component.  Retain literal slash-containing object names as a
     * fallback for databases that use them. */
    if (strchr(seed, '/')) {
	ret = path_match(completions, prefix, dbip, seed);
	if (ret > 0)
	    return ret;
	if (*completions) {
	    bu_free((void *)*completions, "empty path completion array");
	    *completions = NULL;
	}
	ret = obj_match(completions, dbip, seed);
	if (ret > 0)
	    bu_vls_sprintf(prefix, "%s", seed);
	return ret;
    }

    ret = obj_match(completions, dbip, seed);
    if (!ret) {
	ret = path_match(completions, prefix, dbip, seed);
    } else {
	/* If the match is from object names, the prefix is just the seed. */
	bu_vls_sprintf(prefix, "%s", seed);
    }

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
