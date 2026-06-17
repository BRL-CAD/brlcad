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

#include <map>
#include <string>
#include <vector>

#include "./alphanum.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bu/str.h"
#include "ged.h"

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
	*completions = (const char **)bu_calloc(tops_cnt + 1, sizeof(const char *), "av array");
	for (int i = 0; i < tops_cnt; i++) {
	    (*completions)[i] = bu_strdup(all_paths[i]->d_namep);
	}
	bu_free(all_paths, "free db_ls output");
	return tops_cnt;
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
    size_t p = 0;
    int in_token = 0;

    if (!parsed || !input)
	return -1;

    parsed->input_len = strlen(input);
    parsed->copy = bu_strdup(input);

    {
	size_t len = parsed->input_len;
	while (len > 0 && isspace((unsigned char)parsed->copy[len - 1]))
	    parsed->copy[--len] = '\0';
    }

    parsed->argv = (char **)bu_calloc(parsed->input_len + 1, sizeof(char *), "argv array");
    parsed->argc = bu_argv_from_string(parsed->argv, parsed->input_len, parsed->copy);
    parsed->char_starts.resize(parsed->argc);
    parsed->char_ends.resize(parsed->argc);

    for (size_t i = 0; i < parsed->argc; i++) {
	parsed->char_starts[i] = (size_t)(parsed->argv[i] - parsed->copy);
	parsed->char_ends[i] = parsed->char_starts[i] + strlen(parsed->argv[i]);
    }

    for (p = 0; p < cursor_pos && input[p]; p++) {
	if (isspace((unsigned char)input[p])) {
	    if (in_token) {
		parsed->cursor_arg++;
		in_token = 0;
	    }
	} else {
	    in_token = 1;
	}
    }
    if (parsed->cursor_arg > parsed->argc)
	parsed->cursor_arg = parsed->argc;

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
ged_set_validate_result(struct bu_opt_validate_result *result,
			bu_opt_validate_state_t state,
			size_t token_start,
			size_t token_end,
			unsigned int expected,
			const char *hint)
{
    bu_opt_validate_result_clear(result);
    result->state = state;
    result->token_start = token_start;
    result->token_end = token_end;
    result->expected = expected;
    result->hint = hint;
    result->completion_type = BU_OPT_VAL_UNKNOWN;
}

static void
ged_set_result_chars(struct bu_opt_validate_result *result,
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
ged_replace_candidates(struct bu_opt_validate_result *result,
		       const char ***completions,
		       int cnt,
		       bu_opt_value_type_t ctype)
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
ged_fill_command_candidates(struct bu_opt_validate_result *result, const std::string &seed)
{
    const char **completions = NULL;
    int cnt = ged_cmd_completions(&completions, seed.c_str());
    if (cnt > 0)
	ged_replace_candidates(result, &completions, cnt, BU_OPT_VAL_UNKNOWN);
}

static void
ged_fill_geometry_candidates(struct ged *gedp,
			     const std::string &seed,
			     struct bu_opt_validate_result *result)
{
    if (!gedp || !gedp->dbip || !result)
	return;

    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    const char **completions = NULL;
    int cnt = ged_geom_completions(&completions, &prefix, gedp->dbip, seed.c_str());
    if (cnt > 0) {
	ged_replace_candidates(result, &completions, cnt,
		(seed.find('/') != std::string::npos) ? BU_OPT_VAL_DB_PATH : BU_OPT_VAL_DB_OBJECT);
    }
    bu_vls_free(&prefix);
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

extern "C" int
ged_cmd_validate(struct ged *gedp, const char *input, size_t cursor_pos, struct bu_opt_validate_result *result)
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
	bu_opt_validate_state_t state = (!parsed.argc) ? BU_OPT_VALIDATE_INCOMPLETE :
	    (cmd_exists ? BU_OPT_VALIDATE_VALID : BU_OPT_VALIDATE_UNKNOWN);
	const char *hint = (!parsed.argc) ? "command expected" :
	    (cmd_exists ? "valid command" : "unknown command");
	ged_set_validate_result(result, state, 0, 0, BU_OPT_EXPECT_SUBCOMMAND, hint);
	ged_set_result_chars(result, parsed, 0, 0);
	ged_fill_command_candidates(result, seed);
	ged_input_parse_free(&parsed);
	return 0;
    }

    const struct bu_opt_cmd_desc *schema = _ged_cmd_schema(cmd);
    if (!schema) {
	size_t token_index = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg : parsed.argc;
	ged_set_validate_result(result, BU_OPT_VALIDATE_UNKNOWN, token_index, token_index,
		BU_OPT_EXPECT_NONE, "syntax metadata unavailable");
	ged_set_result_chars(result, parsed, token_index, token_index);
	if (parsed.cursor_arg > 0 && gedp && gedp->dbip) {
	    result->completion_type = (seed.find('/') != std::string::npos) ? BU_OPT_VAL_DB_PATH : BU_OPT_VAL_DB_OBJECT;
	    ged_fill_geometry_candidates(gedp, seed, result);
	}
	ged_input_parse_free(&parsed);
	return 0;
    }

    size_t local_cursor = parsed.cursor_arg - 1;
    int ret = bu_opt_validate_argv(schema, parsed.argc - 1, (const char **)parsed.argv + 1, local_cursor, result);
    result->token_start += 1;
    result->token_end += 1;
    ged_set_result_chars(result, parsed, result->token_start, result->token_end);
    if (!result->completion_count &&
	(result->completion_type == BU_OPT_VAL_DB_OBJECT || result->completion_type == BU_OPT_VAL_DB_PATH)) {
	ged_fill_geometry_candidates(gedp, seed, result);
    }

    ged_input_parse_free(&parsed);
    return ret;
}

extern "C" int
ged_cmd_complete(const char ***completions, struct bu_vls *prefix, struct ged *gedp, const char *input, size_t cursor_pos)
{
    struct ged_input_parse parsed;
    struct bu_opt_validate_result result = BU_OPT_VALIDATE_RESULT_NULL;
    std::string seed;
    int ret = 0;

    if (!completions || !input)
	return 0;

    *completions = NULL;
    if (prefix)
	bu_vls_trunc(prefix, 0);

    (void)ged_input_parse_line(&parsed, input, cursor_pos);
    seed = ged_cursor_seed(parsed);
    (void)ged_cmd_validate(gedp, input, cursor_pos, &result);

    if ((result.completion_type == BU_OPT_VAL_DB_OBJECT || result.completion_type == BU_OPT_VAL_DB_PATH) &&
	gedp && gedp->dbip) {
	struct bu_vls lprefix = BU_VLS_INIT_ZERO;
	struct bu_vls *pfx = prefix ? prefix : &lprefix;
	ret = ged_geom_completions(completions, pfx, gedp->dbip, seed.c_str());
	if (!prefix)
	    bu_vls_free(&lprefix);
	bu_opt_validate_result_clear(&result);
	ged_input_parse_free(&parsed);
	return ret;
    }

    if (prefix)
	bu_vls_sprintf(prefix, "%s", seed.c_str());

    *completions = result.completion_candidates;
    ret = (int)result.completion_count;
    result.completion_candidates = NULL;
    result.completion_count = 0;
    bu_opt_validate_result_clear(&result);
    ged_input_parse_free(&parsed);

    return ret;
}

int
ged_geom_completions(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *seed)
{
    int ret = 0;

    if (!dbip || !prefix || !completions || !seed)
	return 0;

    ret = obj_match(completions, dbip, seed);

    // If the object name match didn't work, see if we have a hierarchy specification
    if (!ret) {
	ret = path_match(completions, prefix, dbip, seed);
    } else {
	// If the match is from object names, the prefix is just the seed
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
