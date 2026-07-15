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
#include "./tab_complete_private.h"

static std::vector<std::string>
ged_search_type_keywords(int include_aliases)
{
    std::set<std::string> keywords;
    const char *abstract_types[] = {
	"arb4", "arb5", "arb6", "arb7", "arb8",
	"combination", "plate", "region", "shape", "sphere", "volume",
	NULL
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
ged_search_is_type_keyword(const char *token)
{
    if (!token) return 0;
    std::vector<std::string> keywords = ged_search_type_keywords(1);
    for (size_t i = 0; i < keywords.size(); i++)
	if (BU_STR_EQUAL(token, keywords[i].c_str())) return 1;
    return 0;
}

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
ged_completion_prefix_match(const std::string &candidate, const std::string &prefix)
{
    return prefix.empty() || (candidate.size() >= prefix.size() &&
	!candidate.compare(0, prefix.size(), prefix));
}

static void
ged_completion_add_candidate(std::vector<std::string> &candidates, const char *candidate,
	const std::string &prefix)
{
    if (!candidate) return;
    std::string value(candidate);
    if (ged_completion_prefix_match(value, prefix) &&
	std::find(candidates.begin(), candidates.end(), value) == candidates.end())
	candidates.push_back(value);
}

static void
ged_completion_set_candidates(struct ged_cmd_validate_result *result,
	std::vector<std::string> &candidates, bu_cmd_value_t type)
{
    if (!result || candidates.empty()) return;
    std::sort(candidates.begin(), candidates.end(), [](const std::string &a, const std::string &b) {
	return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
    });
    result->completion_count = candidates.size();
    result->completion_candidates = (const char **)bu_calloc(candidates.size() + 1,
	    sizeof(char *), "completion candidates");
    for (size_t i = 0; i < candidates.size(); i++)
	result->completion_candidates[i] = bu_strdup(candidates[i].c_str());
    result->completion_type = type;
}


/* Encode a single directory-entry name for the database-path grammar. */
static std::string
ged_db_path_component_encode(const char *name)
{
    std::string encoded;
    if (!name)
	return encoded;
    for (const char *c = name; *c; c++) {
	if (*c == '\\' || *c == '/' || *c == '@')
	    encoded.push_back('\\');
	encoded.push_back(*c);
    }
    return encoded;
}


static size_t
ged_last_unescaped_path_separator(const std::string &path)
{
    size_t separator = std::string::npos;
    bool escaped = false;
    for (size_t i = 0; i < path.size(); i++) {
	if (escaped) {
	    escaped = false;
	    continue;
	}
	if (path[i] == '\\') {
	    escaped = true;
	    continue;
	}
	if (path[i] == '/')
	    separator = i;
    }
    return separator;
}


static int
path_match(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *iseed)
{
    std::string path(iseed ? iseed : "");
    size_t separator = ged_last_unescaped_path_separator(path);
    if (separator == std::string::npos)
	return 0;

    /* The final component may be incomplete, but its parent must be a real,
     * fully escaped path. */
    std::string seed = path.substr(separator + 1);
    std::string parent_text = path.substr(0, separator);
    struct directory *cdp = RT_DIR_NULL;
    if (!parent_text.empty() && parent_text != "/") {
	struct db_full_path parent = DB_FULL_PATH_INIT_ZERO;
	if (db_full_path_decode(&parent, dbip, parent_text.c_str()) != DB_FULL_PATH_OK)
	    return BRLCAD_ERROR;
	cdp = DB_FULL_PATH_CUR_DIR(&parent);
	db_free_full_path(&parent);
	if (!cdp || !(cdp->d_flags & RT_DIR_COMB))
	    return BRLCAD_ERROR;
    }

    std::vector<struct directory *> matches;
    if (!cdp) {
	/* No parent denotes the implicit tops collection. */
	db_update_nref(dbip);
	struct directory **all_paths = NULL;
	int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &all_paths);
	if (tops_cnt > 1)
	    bu_sort(all_paths, tops_cnt, sizeof(struct directory *), alphanum_cmp, NULL);
	for (int i = 0; i < tops_cnt; i++) {
	    std::string candidate = ged_db_path_component_encode(all_paths[i]->d_namep);
	    if (candidate.compare(0, seed.size(), seed) == 0)
		matches.push_back(all_paths[i]);
	}
	if (all_paths)
	    bu_free(all_paths, "free db_ls output");
    } else {
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
	for (int i = 0; i < child_cnt; i++) {
	    std::string candidate = ged_db_path_component_encode(children[i]->d_namep);
	    if (candidate.compare(0, seed.size(), seed) == 0)
		matches.push_back(children[i]);
	}
	if (children)
	    bu_free(children, "dp array");
    }

    bu_vls_sprintf(prefix, "%s", seed.c_str());
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(ged_db_path_component_encode(matches[i]->d_namep).c_str());
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
	std::string candidate = ged_db_path_component_encode(dps[i]->d_namep);
	if (candidate.compare(0, strlen(seed), seed) == 0) {
	    matches.push_back(dps[i]);
	}
    }

    // Make an argv array for client use
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(ged_db_path_component_encode(matches[i]->d_namep).c_str());
    return (int)matches.size();
}

void
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

int
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
	    ctype = (ged_last_unescaped_path_separator(seed) != std::string::npos) ? BU_CMD_VALUE_DB_PATH : BU_CMD_VALUE_DB_OBJECT;
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

    if (!strchr(name, '/') && !strchr(name, '\\'))
	return db_lookup(dbip, name, LOOKUP_QUIET);

    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, dbip)
	if (BU_STR_EQUAL(dp->d_namep, name))
	    return dp;
    FOR_ALL_DIRECTORY_END;

    /* The escaped full-path spelling also names a root-level object. */
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    if (db_full_path_decode(&path, dbip, name) == DB_FULL_PATH_OK) {
	if (path.fp_len == 1)
	    dp = DB_FULL_PATH_CUR_DIR(&path);
	else
	    dp = RT_DIR_NULL;
	db_free_full_path(&path);
	return dp;
    }

    return RT_DIR_NULL;
}

static ged_cmd_semantic_state_t
ged_quiet_db_path_validate(struct ged *gedp, const char *token)
{
    if (!gedp || !gedp->dbip || BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INVALID;

    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    int ret = db_full_path_decode(&path, gedp->dbip, token);
    if (ret == DB_FULL_PATH_OK)
	db_free_full_path(&path);
    return (ret == DB_FULL_PATH_OK) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
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

	for (size_t i = 0; tree && tree->subcommands && tree->subcommands[i].schema; i++) {
	ged_completion_add_candidate(candidates,
	    tree->subcommands[i].schema->name, seed);
	}
	ged_completion_set_candidates(result, candidates, BU_CMD_VALUE_KEYWORD);
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
	    (parsed.cursor_arg == 0 || parsed.argv[parsed.cursor_arg - 1][0] != '-') &&
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

    /* Only an unescaped slash denotes an interactive hierarchy separator.
     * Literal slash-containing names remain root-level candidates and are
     * represented with '\\/' by the database-path grammar. */
    if (ged_last_unescaped_path_separator(std::string(seed)) != std::string::npos) {
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
