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
#include <climits>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "./alphanum.h"
#include "./completion_index.h"
#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/datetime.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bu/str.h"
#include "ged.h"
#include "rt/functab.h"
#include "rt/search.h"
#include "./ged_private.h"
#include "./tab_complete_private.h"

static int
ged_search_is_type_keyword(const char *token)
{
    return db_search_type_term_exists(token);
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

static bu_cmd_value_t ged_semantic_provider_type(const char *name);

static bu_cmd_value_t
ged_native_value_type(bu_cmd_value_t value_type, const char *semantic_provider)
{
    bu_cmd_value_t provider_type = ged_semantic_provider_type(semantic_provider);
    /* A provider enriches a schema role whose scalar spelling is otherwise
     * generic.  It must not erase a more precise declared representation
     * (for example, draw's integer-valued mode provider). */
    if (provider_type != BU_CMD_VALUE_UNKNOWN &&
	(value_type == BU_CMD_VALUE_UNKNOWN || value_type == BU_CMD_VALUE_STRING ||
	 value_type == BU_CMD_VALUE_CUSTOM))
	return provider_type;

    switch (value_type) {
	case BU_CMD_VALUE_FLAG:
	case BU_CMD_VALUE_BOOL:
	    return BU_CMD_VALUE_BOOL;
	case BU_CMD_VALUE_INTEGER:
	case BU_CMD_VALUE_HEX_INTEGER:
	case BU_CMD_VALUE_LONG:
	case BU_CMD_VALUE_HEX_LONG:
	    return BU_CMD_VALUE_INTEGER;
	case BU_CMD_VALUE_NUMBER:
	    return BU_CMD_VALUE_NUMBER;
	case BU_CMD_VALUE_CHAR:
	    return BU_CMD_VALUE_CHAR;
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
	case BU_CMD_VALUE_VLS:
	    return BU_CMD_VALUE_VLS;
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

struct ged_semantic_provider_registry {
    std::mutex mutex;
    bool builtins_installed = false;
    std::map<std::string, struct ged_cmd_semantic_provider> providers;
};


static struct ged_semantic_provider_registry &
ged_semantic_providers()
{
    static struct ged_semantic_provider_registry registry;
    return registry;
}

namespace {

class ged_completion_registry_reference {
    public:
	explicit ged_completion_registry_reference(bool mutation = false) : held(
	    (mutation ? _ged_registry_mutation_acquire() :
	     _ged_registry_access_acquire()) == 0) {}
	~ged_completion_registry_reference()
	{
	    if (held)
		_ged_registry_access_release();
	}
	bool acquired() const { return held; }

    private:
	bool held;
};

}

static void ged_register_builtin_semantic_providers(void);

#define GED_VALIDATE_OWNS_HINT 0x01u
#define GED_VALIDATE_OWNS_PROVIDER 0x02u

extern "C" void
ged_cmd_validate_result_init(struct ged_cmd_validate_result *result)
{
    if (!result)
	return;
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
    result->completion_total = 0;
    result->completion_truncated = 0;
    result->completion_common_prefix = NULL;
    result->candidate_validate = NULL;
    result->owned_strings = 0;
}

extern "C" void
ged_cmd_validate_result_clear(struct ged_cmd_validate_result *result)
{
    if (!result)
	return;
    if (result->completion_candidates)
	bu_argv_free(result->completion_count, (char **)result->completion_candidates);
    if (result->completion_common_prefix)
	bu_free(result->completion_common_prefix, "completion common prefix");
    if ((result->owned_strings & GED_VALIDATE_OWNS_HINT) && result->hint)
	bu_free((void *)result->hint, "validation hint");
    if ((result->owned_strings & GED_VALIDATE_OWNS_PROVIDER) &&
	    result->semantic_provider)
	bu_free((void *)result->semantic_provider, "validation provider name");
    ged_cmd_validate_result_init(result);
}


/* Callback-facing results borrow immutable schema strings.  Only the
 * outermost public call publishes them beyond the registry lease; make those
 * strings self-contained while plugin storage is still pinned. */
static void
ged_cmd_validate_result_publish(struct ged_cmd_validate_result *result)
{
    if (!result || _ged_registry_access_depth() != 1)
	return;
    if (result->hint && !(result->owned_strings & GED_VALIDATE_OWNS_HINT)) {
	result->hint = bu_strdup(result->hint);
	result->owned_strings |= GED_VALIDATE_OWNS_HINT;
    }
    if (result->semantic_provider &&
	    !(result->owned_strings & GED_VALIDATE_OWNS_PROVIDER)) {
	result->semantic_provider = bu_strdup(result->semantic_provider);
	result->owned_strings |= GED_VALIDATE_OWNS_PROVIDER;
    }
}

extern "C" int
ged_cmd_semantic_provider_register(const struct ged_cmd_semantic_provider *provider)
{
    if (!provider || BU_STR_EMPTY(provider->name))
	return -1;
    if (provider->value_type < BU_CMD_VALUE_FLAG ||
	provider->value_type > BU_CMD_VALUE_UNKNOWN ||
	(provider->flags & ~GED_CMD_PROVIDER_BOUNDED_QUERY) ||
	!!provider->complete_query != !!(provider->flags & GED_CMD_PROVIDER_BOUNDED_QUERY))
	return -1;

    ged_completion_registry_reference reference(true);
    if (!reference.acquired())
	return -1;

    /* Stable ged.* names are owned by libged.  Install them before making the
     * first-wins decision so registration order cannot replace a built-in. */
    ged_register_builtin_semantic_providers();
    if (!bu_strncmp(provider->name, "ged.", 4))
	return -1;

    struct ged_semantic_provider_registry &registry = ged_semantic_providers();
    std::lock_guard<std::mutex> guard(registry.mutex);
    std::map<std::string, struct ged_cmd_semantic_provider> &providers = registry.providers;
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

    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return 0;

    ged_register_builtin_semantic_providers();
    struct ged_semantic_provider_registry &registry = ged_semantic_providers();
    std::lock_guard<std::mutex> guard(registry.mutex);
    return registry.providers.find(std::string(name)) != registry.providers.end();
}

static int
ged_completion_prefix_match(const std::string &candidate, const std::string &prefix)
{
    return prefix.empty() || (candidate.size() >= prefix.size() &&
	!candidate.compare(0, prefix.size(), prefix));
}


/* Byte offsets are part of the C API, but every command string is UTF-8.  A
 * bytewise comparison is sufficient to find a tentative common prefix; move
 * the boundary back when that comparison stopped inside a code point. */
static size_t
ged_utf8_prefix_boundary(const std::string &value, size_t boundary)
{
    boundary = std::min(boundary, value.size());
    if (boundary == value.size())
	return boundary;
    while (boundary && (((unsigned char)value[boundary] & 0xc0U) == 0x80U))
	boundary--;
    return boundary;
}


static bool
ged_utf8_valid(std::string_view value)
{
    for (size_t i = 0; i < value.size();) {
	unsigned char first = (unsigned char)value[i];
	if (first < 0x80U) {
	    i++;
	    continue;
	}
	size_t count = 0;
	unsigned int codepoint = 0;
	if ((first & 0xe0U) == 0xc0U) { count = 2; codepoint = first & 0x1fU; }
	else if ((first & 0xf0U) == 0xe0U) { count = 3; codepoint = first & 0x0fU; }
	else if ((first & 0xf8U) == 0xf0U) { count = 4; codepoint = first & 0x07U; }
	else return false;
	if (i + count > value.size()) return false;
	for (size_t j = 1; j < count; j++) {
	    unsigned char next = (unsigned char)value[i + j];
	    if ((next & 0xc0U) != 0x80U) return false;
	    codepoint = (codepoint << 6) | (next & 0x3fU);
	}
	if ((count == 2 && codepoint < 0x80U) ||
	    (count == 3 && codepoint < 0x800U) ||
	    (count == 4 && codepoint < 0x10000U) ||
	    codepoint > 0x10ffffU ||
	    (codepoint >= 0xd800U && codepoint <= 0xdfffU)) return false;
	i += count;
    }
    return true;
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


static bool
ged_completion_request_accepts(const struct ged_cmd_completion_request *request,
	const char *candidate)
{
    return !request || !request->candidate_filter ||
	request->candidate_filter(candidate, request->candidate_filter_data) == 0;
}


/* Keep the lexically first requested candidates while still counting every
 * match and deriving the common prefix.  With a finite limit this is O(N log
 * K), rather than sorting and allocating all N database names merely to show
 * K of them in an interactive viewport. */
class ged_candidate_collector {
    public:
	explicit ged_candidate_collector(size_t requested_limit,
		const struct ged_cmd_completion_request *request = NULL) :
	    limit(requested_limit),
	    filter(request ? request->candidate_filter : NULL),
	    filter_data(request ? request->candidate_filter_data : NULL) {}

	void add(const std::string &candidate)
	{
	    if (filter && filter(candidate.c_str(), filter_data) != 0)
		return;
	    if (!total) {
		common = candidate;
	    } else {
		size_t i = 0;
		size_t common_limit = std::min(common.size(), candidate.size());
		    while (i < common_limit && common[i] == candidate[i])
			i++;
		    common.resize(ged_utf8_prefix_boundary(common, i));
	    }
	    total++;
	    if (!limit) {
		values.push_back(candidate);
		return;
	    }
	    if (values.size() < limit) {
		values.push_back(candidate);
		std::push_heap(values.begin(), values.end(), less);
		return;
	    }
	    if (!values.empty() && less(candidate, values.front())) {
		std::pop_heap(values.begin(), values.end(), less);
		values.back() = candidate;
		std::push_heap(values.begin(), values.end(), less);
	    }
	}

	void sort()
	{
	    std::sort(values.begin(), values.end(), less);
	}

	size_t limit = 0;
	size_t total = 0;
	std::string common;
	std::vector<std::string> values;

    private:
	ged_cmd_completion_filter_t filter = NULL;
	const void *filter_data = NULL;

	static bool less(const std::string &a, const std::string &b)
	{
	    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
	}
};


static void
ged_completion_common_add(std::string &common, bool &initialized,
	const char *value)
{
    if (!value)
	return;
    if (!initialized) {
	common = value;
	initialized = true;
	return;
    }
    size_t keep = 0;
    size_t limit = std::min(common.size(), strlen(value));
    while (keep < limit && common[keep] == value[keep])
	keep++;
    common.resize(ged_utf8_prefix_boundary(common, keep));
}


struct ged_completion_merge_filter_context {
    ged_cmd_completion_filter_t filter = NULL;
    const void *filter_data = NULL;
    const std::set<std::string> *excluded = NULL;
};


static int
ged_completion_merge_filter(const char *candidate, const void *data)
{
    const struct ged_completion_merge_filter_context *context =
	(const struct ged_completion_merge_filter_context *)data;

    if (!candidate || !context)
	return 1;
    if (context->filter && context->filter(candidate, context->filter_data) != 0)
	return 1;
    return context->excluded && context->excluded->find(candidate) !=
	context->excluded->end();
}


/* Merge two independently complete result sets without turning their bounded
 * materializations into full-set metadata. */
static void
ged_completion_merge(struct ged_cmd_validate_result *target,
	const struct ged_cmd_validate_result *source,
	const struct ged_cmd_completion_request *request)
{
    if (!target || !source)
	return;

    size_t target_total = target->completion_total ? target->completion_total :
	target->completion_count;
    size_t source_total = source->completion_total ? source->completion_total :
	source->completion_count;
    size_t duplicate_count = 0;
    std::set<std::string> target_values;
    for (size_t i = 0; i < target->completion_count; i++)
	if (target->completion_candidates[i])
	    target_values.insert(target->completion_candidates[i]);
    for (size_t i = 0; i < source->completion_count; i++)
	if (source->completion_candidates[i] &&
		target_values.find(source->completion_candidates[i]) != target_values.end())
	    duplicate_count++;

    ged_candidate_collector combined(request ? request->max_candidates : 0,
	request);
    for (size_t i = 0; i < target->completion_count; i++)
	if (target->completion_candidates[i])
	    combined.add(target->completion_candidates[i]);
    for (size_t i = 0; i < source->completion_count; i++)
	if (source->completion_candidates[i] &&
		target_values.insert(source->completion_candidates[i]).second)
	    combined.add(source->completion_candidates[i]);
    combined.sort();

    std::string common;
    bool common_initialized = false;
    if (target_total) {
	if (target->completion_common_prefix)
	    ged_completion_common_add(common, common_initialized,
		target->completion_common_prefix);
	else if (!target->completion_truncated)
	    for (size_t i = 0; i < target->completion_count; i++)
		ged_completion_common_add(common, common_initialized,
		    target->completion_candidates[i]);
    }
    if (source_total) {
	if (source->completion_common_prefix)
	    ged_completion_common_add(common, common_initialized,
		source->completion_common_prefix);
	else if (!source->completion_truncated)
	    for (size_t i = 0; i < source->completion_count; i++)
		ged_completion_common_add(common, common_initialized,
		    source->completion_candidates[i]);
    }

    if (target->completion_candidates)
	bu_argv_free(target->completion_count, (char **)target->completion_candidates);
    if (target->completion_common_prefix)
	bu_free(target->completion_common_prefix, "merged completion common prefix");
    target->completion_candidates = (const char **)bu_calloc(
	combined.values.size() + 1, sizeof(char *), "merged completion candidates");
    for (size_t i = 0; i < combined.values.size(); i++)
	target->completion_candidates[i] = bu_strdup(combined.values[i].c_str());
    target->completion_count = combined.values.size();
    target->completion_total = target_total + source_total -
	std::min(duplicate_count, std::min(target_total, source_total));
    target->completion_truncated = target->completion_total > target->completion_count ||
	target->completion_truncated || source->completion_truncated;
    target->completion_common_prefix = common_initialized ? bu_strdup(common.c_str()) : NULL;
    if (source->completion_type != BU_CMD_VALUE_UNKNOWN)
	target->completion_type = source->completion_type;
}


struct ged_db_completion_policy {
    unsigned int flags;
};

static const struct ged_db_completion_policy ged_db_geometry_policy = {GED_DB_COMPLETION_GEOMETRY};
static const struct ged_db_completion_policy ged_db_any_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_NON_GEOMETRY};
static const struct ged_db_completion_policy ged_db_geometry_hidden_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_HIDDEN};
static const struct ged_db_completion_policy ged_db_any_hidden_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_NON_GEOMETRY | GED_DB_COMPLETION_HIDDEN};
static const struct ged_db_completion_policy ged_db_hidden_only_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_NON_GEOMETRY | GED_DB_COMPLETION_HIDDEN_ONLY};
static const struct ged_db_completion_policy ged_db_all_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_NON_GEOMETRY | GED_DB_COMPLETION_HIDDEN | GED_DB_COMPLETION_GLOBAL};
static const struct ged_db_completion_policy ged_db_binary_policy = {GED_DB_COMPLETION_BINARY_ONLY};
static const struct ged_db_completion_policy ged_db_primitives_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_PRIMITIVES};
static const struct ged_db_completion_policy ged_db_primitives_hidden_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_PRIMITIVES | GED_DB_COMPLETION_HIDDEN};
static const struct ged_db_completion_policy ged_db_combinations_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_COMBINATIONS};
static const struct ged_db_completion_policy ged_db_combinations_hidden_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_COMBINATIONS | GED_DB_COMPLETION_HIDDEN};
static const struct ged_db_completion_policy ged_db_regions_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_REGIONS};
static const struct ged_db_completion_policy ged_db_regions_hidden_policy = {GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_REGIONS | GED_DB_COMPLETION_HIDDEN};

static int
ged_db_completion_allowed(const struct directory *dp, const struct ged_db_completion_policy *policy)
{
    if (!policy)
	policy = &ged_db_geometry_policy;
    return _ged_db_completion_allowed(dp, policy->flags) ? 1 : 0;
}


static int
ged_completion_index_query(const char ***completions, struct ged *gedp,
	const std::string &seed, const struct ged_db_completion_policy *policy,
	const struct ged_cmd_completion_request *request, size_t *total,
	std::string *common)
{
    struct ged_db_completion_index_result indexed;
    unsigned int flags = policy ? policy->flags : GED_DB_COMPLETION_GEOMETRY;
    if (!completions || _ged_db_completion_index_query(gedp, seed, flags,
	    request, &indexed))
	return -1;
    if (total)
	*total = indexed.total;
    if (common)
	*common = indexed.common;
    *completions = (const char **)bu_calloc(indexed.values.size() + 1,
	sizeof(char *), "indexed completion candidates");
    for (size_t i = 0; i < indexed.values.size(); i++)
	(*completions)[i] = bu_strdup(indexed.values[i].c_str());
    return (int)indexed.values.size();
}


static int
path_match(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *iseed,
	const struct ged_db_completion_policy *policy,
	const struct ged_cmd_completion_request *request = NULL,
	size_t *total = NULL, std::string *common = NULL)
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
	    return 0;
	cdp = DB_FULL_PATH_CUR_DIR(&parent);
	db_free_full_path(&parent);
	if (!cdp || !(cdp->d_flags & RT_DIR_COMB))
	    return 0;
    }

    size_t limit = request ? request->max_candidates : 0;
    ged_candidate_collector matches(limit, request);
    if (!cdp) {
	/* A root path can begin at any directory entry, not just a top-level
	 * object.  Restricting this to DB_LS_TOPS makes valid paths such as
	 * /component impossible to complete. */
	struct directory *root_dp;
	FOR_ALL_DIRECTORY_START(root_dp, dbip)
	    if (!ged_db_completion_allowed(root_dp, policy))
		continue;
	    std::string candidate = _ged_db_path_component_encode(root_dp->d_namep);
	    if (candidate.compare(0, seed.size(), seed) == 0)
		matches.add(candidate);
	FOR_ALL_DIRECTORY_END;
    } else {
	struct rt_db_internal in;
	std::unordered_set<struct directory *> seen;
	if (rt_db_get_internal(&in, cdp, dbip, NULL) < 0)
	    return 0;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb) {
	    rt_db_free_internal(&in);
	    return 0;
	}
	struct directory **children = NULL;
	int child_cnt = db_comb_children(dbip, comb, &children, NULL, NULL);
	rt_db_free_internal(&in);
	for (int i = 0; i < child_cnt; i++) {
	    if (!seen.insert(children[i]).second)
		continue;
	    if (!ged_db_completion_allowed(children[i], policy))
		continue;
	    std::string candidate = _ged_db_path_component_encode(children[i]->d_namep);
	    if (candidate.compare(0, seed.size(), seed) == 0)
		matches.add(candidate);
	}
	if (children)
	    bu_free(children, "dp array");
    }

    matches.sort();
    if (total)
	*total = matches.total;
    if (common)
	*common = matches.common;

    bu_vls_sprintf(prefix, "%s", seed.c_str());
    *completions = (const char **)bu_calloc(matches.values.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.values.size(); i++)
	(*completions)[i] = bu_strdup(matches.values[i].c_str());
    return (int)matches.values.size();
}

// Because librt doesn't forbid objects with forward slashes in
// their names, and such names have occasionally been observed in
// the wild, we have to treat all seed strings as potential dp names
// first, and only after that fails try them as hierarchy paths.
static int
obj_match(const char ***completions, struct db_i *dbip, const char *seed,
	const struct ged_db_completion_policy *policy,
	const struct ged_cmd_completion_request *request = NULL,
	size_t *total = NULL, std::string *common = NULL)
{
    const size_t seed_len = strlen(seed);
    size_t limit = request ? request->max_candidates : 0;
    ged_candidate_collector matches(limit, request);
    struct directory *dp;
	FOR_ALL_DIRECTORY_START(dp, dbip)
	    if (ged_db_completion_allowed(dp, policy)) {
		std::string candidate = _ged_db_path_component_encode(dp->d_namep);
		if (candidate.compare(0, seed_len, seed) == 0)
		    matches.add(candidate);
	    }
	FOR_ALL_DIRECTORY_END;

    matches.sort();
    if (total)
	*total = matches.total;
    if (common)
	*common = matches.common;

    // Make an argv array for client use
    *completions = (const char **)bu_calloc(matches.values.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.values.size(); i++)
	(*completions)[i] = bu_strdup(matches.values[i].c_str());
    return (int)matches.values.size();
}

static int ged_geom_completions_query(const char ***, struct bu_vls *, struct db_i *,
	const char *, unsigned int, const struct ged_cmd_completion_request *,
	size_t *, std::string *);

static std::string
ged_cursor_seed(const struct ged_input_parse &parsed)
{
    return parsed.cursor_seed;
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
    else if (completions)
	bu_argv_free(0, (char **)completions);
}

static void
ged_fill_geometry_candidates(struct ged *gedp,
			     const std::string &seed,
			     struct ged_cmd_validate_result *result,
			     struct bu_vls *oprefix = NULL,
			     const struct ged_db_completion_policy *policy = &ged_db_geometry_policy,
			     const struct ged_cmd_completion_request *request = NULL)
{
    if (!gedp || !gedp->dbip || !result)
	return;

    struct bu_vls lprefix = BU_VLS_INIT_ZERO;
    struct bu_vls *prefix = oprefix ? oprefix : &lprefix;
    const char **completions = NULL;
    unsigned int filters = policy ? policy->flags : GED_DB_COMPLETION_GEOMETRY;
    size_t total = 0;
    std::string common;
    int cnt = 0;
    size_t separator = ged_last_unescaped_path_separator(seed);
    bool indexed_root = request && (separator == std::string::npos ||
	seed.substr(0, separator).empty() || seed.substr(0, separator) == "/");
    if (indexed_root) {
	std::string component = separator == std::string::npos ? seed :
	    seed.substr(separator + 1);
	cnt = ged_completion_index_query(&completions, gedp, component,
	    policy, request, &total, &common);
	if (cnt >= 0) {
	    bu_vls_sprintf(prefix, "%s", component.c_str());
	} else {
	    cnt = ged_geom_completions_query(&completions, prefix, gedp->dbip,
		seed.c_str(), filters, request, &total, &common);
	}
    } else {
	cnt = ged_geom_completions_query(&completions, prefix, gedp->dbip,
	    seed.c_str(), filters, request, &total, &common);
    }
    if (cnt > 0 && completions) {
	bu_cmd_value_t ctype = result->completion_type;
	if (ctype != BU_CMD_VALUE_DB_OBJECT && ctype != BU_CMD_VALUE_DB_PATH)
	    ctype = (ged_last_unescaped_path_separator(seed) != std::string::npos) ? BU_CMD_VALUE_DB_PATH : BU_CMD_VALUE_DB_OBJECT;
	ged_replace_candidates(result, &completions, cnt,
		ctype);
    } else if (completions) {
	bu_argv_free(0, (char **)completions);
    }
    result->completion_total = total;
    result->completion_truncated = total > result->completion_count;
    /* An empty string is still authoritative full-set metadata when matches
     * share no additional byte.  Bounded-provider normalization distinguishes
     * that valid answer from an omitted common prefix by pointer presence. */
    if (total)
	result->completion_common_prefix = bu_strdup(common.c_str());
    if (!oprefix)
	bu_vls_free(&lprefix);
}

static struct directory *
ged_lookup_exact_quiet(struct db_i *dbip, const char *name)
{
    if (!dbip || BU_STR_EMPTY(name))
	return RT_DIR_NULL;

    /* db_lookup performs an exact hash lookup before its escaped-path
     * fallback.  Repeating that operation as a directory-wide linear scan
     * made slash-bearing validation needlessly expensive.  A multi-element
     * path is not, however, an exact object name. */
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp || BU_STR_EQUAL(dp->d_namep, name))
	return dp;

    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    if (db_full_path_decode(&path, dbip, name) == DB_FULL_PATH_OK) {
	dp = (path.fp_len == 1) ? DB_FULL_PATH_CUR_DIR(&path) : RT_DIR_NULL;
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

    /* Completion preserves a leading slash as the database-root spelling
     * while replacing only the first path component.  db_full_path_decode
     * consumes hierarchy components and does not accept that presentation
     * marker, so validate the path behind it. */
    if (token[0] == '/' && token[1] != '\0')
	token++;

    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    int ret = db_full_path_decode(&path, gedp->dbip, token);
    if (ret == DB_FULL_PATH_OK)
	db_free_full_path(&path);
    return (ret == DB_FULL_PATH_OK) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_db_object_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, const void *data)
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp || !gedp->dbip)
	return GED_CMD_SEMANTIC_UNKNOWN;
    struct directory *dp = ged_lookup_exact_quiet(gedp->dbip, token);
    return ged_db_completion_allowed(dp, (const struct ged_db_completion_policy *)data) ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}


static ged_cmd_semantic_state_t
ged_builtin_summary_object_or_legacy_type_validate(struct ged *gedp,
	bu_cmd_value_t type, const char *token, const void *data)
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
ged_builtin_db_path_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, const void *data)
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp || !gedp->dbip)
	return GED_CMD_SEMANTIC_UNKNOWN;

    if (token[0] == '/' && token[1] != '\0')
	token++;

    if (type == BU_CMD_VALUE_DB_OBJECT) {
	struct directory *dp = ged_lookup_exact_quiet(gedp->dbip, token);
	if (dp)
	    return ged_db_completion_allowed(dp, (const struct ged_db_completion_policy *)data) ?
		GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
    }
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    int ret = db_full_path_decode(&path, gedp->dbip, token);
    if (ret != DB_FULL_PATH_OK)
	return GED_CMD_SEMANTIC_INVALID;
    struct directory *dp = DB_FULL_PATH_CUR_DIR(&path);
    int allowed = ged_db_completion_allowed(dp, (const struct ged_db_completion_policy *)data);
    db_free_full_path(&path);
    return allowed ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_db_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, const void *data)
{
    ged_fill_geometry_candidates(gedp, seed ? std::string(seed) : std::string(), result, NULL,
	(const struct ged_db_completion_policy *)data);
    return result ? (int)result->completion_count : 0;
}

static int
ged_builtin_db_complete_query(struct ged *gedp, const char *seed,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result, const void *data)
{
    ged_fill_geometry_candidates(gedp, seed ? std::string(seed) : std::string(), result, NULL,
	(const struct ged_db_completion_policy *)data, request);
    return result ? (int)result->completion_count : 0;
}


static ged_cmd_semantic_state_t
ged_builtin_db_components_validate(struct ged *UNUSED(gedp),
	bu_cmd_value_t UNUSED(type), const char *UNUSED(token),
	const void *UNUSED(data))
{
    /* The command-level context validator owns the combined path. */
    return GED_CMD_SEMANTIC_UNKNOWN;
}


static int
ged_builtin_db_components_complete_query(struct ged *gedp, const char *seed,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result, const void *data)
{
    size_t first_operand = 1;
    std::string path;

    if (!request || !request->argv || !request->argc ||
	request->cursor_arg > request->argc)
	return ged_builtin_db_complete_query(gedp, seed, request, result, data);

    if (request->argv[0] && BU_STR_EQUAL(request->argv[0], "listeval")) {
	while (first_operand < request->cursor_arg) {
	    const char *arg = request->argv[first_operand];
	    if (!arg)
		return 0;
	    if (BU_STR_EQUAL(arg, "-t") || BU_STR_EQUAL(arg, "--")) {
		first_operand++;
		continue;
	    }
	    break;
	}
    }
    if (request->cursor_arg <= first_operand)
	return ged_builtin_db_complete_query(gedp, seed, request, result, data);

    for (size_t i = first_operand; i < request->cursor_arg; i++) {
	const char *component = request->argv[i];
	if (BU_STR_EMPTY(component) || strchr(component, '/'))
	    return 0;
	if (!path.empty())
	    path.push_back('/');
	path.append(_ged_db_path_component_encode(component));
    }
    if (!path.empty())
	path.push_back('/');
    path.append(seed ? seed : "");
    ged_fill_geometry_candidates(gedp, path, result, NULL,
	(const struct ged_db_completion_policy *)data, request);
    return result ? (int)result->completion_count : 0;
}


static int
ged_builtin_db_components_complete(struct ged *gedp, const char *seed,
	struct ged_cmd_validate_result *result, const void *data)
{
    return ged_builtin_db_complete(gedp, seed, result, data);
}


static ged_cmd_semantic_state_t
ged_builtin_view_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (!gedp)
	return GED_CMD_SEMANTIC_UNKNOWN;
    return bv_set_find_view(&gedp->ged_views, token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_view_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, const void *UNUSED(data))
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
ged_builtin_command_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    return ged_cmd_exists(token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_command_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, const void *UNUSED(data))
{
    const char **candidates = NULL;
    int cnt = 0;
    if (!result)
	return 0;
    cnt = ged_cmd_completions(&candidates, seed ? seed : "");
    if (cnt <= 0) {
	if (candidates)
	    bu_argv_free(0, (char **)candidates);
	return 0;
    }
    result->completion_count = (size_t)cnt;
    result->completion_candidates = candidates;
    return cnt;
}

static ged_cmd_semantic_state_t
ged_builtin_primitive_type_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    std::string label(token);
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return rt_get_functab_by_label(label.c_str()) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_primitive_type_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, const void *UNUSED(data))
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
ged_builtin_geometry_or_primitive_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, const void *data)
{
    ged_cmd_semantic_state_t pstate = ged_builtin_primitive_type_validate(gedp, type, token, data);
    if (pstate == GED_CMD_SEMANTIC_VALID)
	return pstate;
    return ged_builtin_db_object_validate(gedp, type, token, data);
}

static int
ged_builtin_geometry_or_primitive_complete_query(struct ged *gedp, const char *seed,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result, const void *UNUSED(data))
{
    if (!result)
	return 0;
    std::string prefix = seed ? seed : "";
    size_t limit = request ? request->max_candidates : 0;
    ged_candidate_collector matches(limit, request);
    size_t primitive_count = 0;
    size_t duplicate_count = 0;
    size_t object_total = 0;
    std::string object_common;
    std::set<std::string> materialized;

    if (gedp && gedp->dbip) {
	const char **objects = NULL;
	int cnt = ged_completion_index_query(&objects, gedp, prefix,
	    &ged_db_geometry_policy, request, &object_total, &object_common);
	if (cnt < 0) {
	    struct bu_vls oprefix = BU_VLS_INIT_ZERO;
	    cnt = ged_geom_completions_query(&objects, &oprefix, gedp->dbip,
		prefix.c_str(), GED_DB_COMPLETION_GEOMETRY, request,
		&object_total, &object_common);
	    bu_vls_free(&oprefix);
	}
	for (int i = 0; i < cnt; i++)
	    if (objects[i] && materialized.insert(objects[i]).second)
		matches.add(objects[i]);
	if (objects)
	    bu_argv_free((size_t)std::max(0, cnt), (char **)objects);
    }

    std::vector<std::string> primitive_values;
    for (int i = 1; i <= ID_MAX_SOLID; i++) {
	if (!OBJ[i].ft_label[0] || BU_STR_EQUAL(OBJ[i].ft_label, "ID_NULL"))
	    continue;
	std::string label(OBJ[i].ft_label);
	if (!prefix.empty() && label.compare(0, prefix.size(), prefix) != 0)
	    continue;
	if (!ged_completion_request_accepts(request, label.c_str()))
	    continue;
	primitive_values.push_back(label);
	primitive_count++;
	if (gedp && gedp->dbip) {
	    struct directory *dp = ged_lookup_exact_quiet(gedp->dbip, label.c_str());
	    if (ged_db_completion_allowed(dp, &ged_db_geometry_policy))
		duplicate_count++;
	}
	if (materialized.insert(label).second)
	    matches.add(label);
    }

    if (!object_total && !primitive_count)
	return 0;

    matches.sort();
    result->completion_count = matches.values.size();
    result->completion_candidates = (const char **)bu_calloc(matches.values.size() + 1,
	sizeof(char *), "geometry or primitive candidates");
    size_t oi = 0;

    for (const std::string &match : matches.values)
	result->completion_candidates[oi++] = bu_strdup(match.c_str());
    result->completion_type = BU_CMD_VALUE_RAW;
    result->completion_total = object_total + primitive_count - duplicate_count;
    result->completion_truncated = result->completion_total > result->completion_count;

    bool have_common = false;
    std::string common;
    auto add_common = [&common, &have_common](const std::string &value) {
	if (!have_common) {
	    common = value;
	    have_common = true;
	    return;
	}
	size_t i = 0;
	size_t common_limit = std::min(common.size(), value.size());
	while (i < common_limit && common[i] == value[i])
	    i++;
	common.resize(ged_utf8_prefix_boundary(common, i));
    };
    if (object_total)
	add_common(object_common);
    for (const std::string &primitive : primitive_values)
	add_common(primitive);
    result->completion_common_prefix = bu_strdup(common.c_str());
    return (int)result->completion_count;
}

static int
ged_builtin_geometry_or_primitive_complete(struct ged *gedp, const char *seed,
	struct ged_cmd_validate_result *result, const void *data)
{
    return ged_builtin_geometry_or_primitive_complete_query(gedp, seed, NULL,
	result, data);
}

static ged_cmd_semantic_state_t
ged_builtin_search_path_validate(struct ged *gedp, bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
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
ged_builtin_search_type_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    return ged_search_is_type_keyword(token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_path_or_pattern_validate(struct ged *gedp, bu_cmd_value_t type, const char *token, const void *data)
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    if (strpbrk(token, "*?[]"))
	return GED_CMD_SEMANTIC_UNKNOWN;
    return ged_builtin_db_path_validate(gedp, type, token, data);
}

static int
ged_builtin_path_complete(struct ged *gedp, const char *seed, struct ged_cmd_validate_result *result, const void *data)
{
    ged_fill_geometry_candidates(gedp, seed ? std::string(seed) : std::string(), result, NULL,
	(const struct ged_db_completion_policy *)data);
    return result ? (int)result->completion_count : 0;
}

static ged_cmd_semantic_state_t
ged_builtin_nonempty_unknown(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    return BU_STR_EMPTY(token) ? GED_CMD_SEMANTIC_INCOMPLETE : GED_CMD_SEMANTIC_UNKNOWN;
}

static ged_cmd_semantic_state_t
ged_builtin_nonnegative_integer(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    char *end = NULL;
    long val = strtol(token, &end, 0);
    return (end && *end == '\0' && val >= 0) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_display_mode_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    static const char * const modes[] = {"0", "1", "2", "3", "wireframe", "shaded", "shaded_all", "evaluated", NULL};
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    for (size_t i = 0; modes[i]; i++) if (BU_STR_EQUAL(token, modes[i])) return GED_CMD_SEMANTIC_VALID;
    return GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_draw_mode_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token))
	return GED_CMD_SEMANTIC_INCOMPLETE;
    return (token[1] == '\0' && token[0] >= '0' && token[0] <= '5') ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_unit_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    return (bu_mm_value(token) >= 0.0) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_color_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    struct bu_color color = BU_COLOR_INIT_ZERO;
    return bu_color_from_str(&color, token) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_matrix_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    mat_t matrix;
    return (bn_decode_mat(matrix, token) == 16) ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static ged_cmd_semantic_state_t
ged_builtin_vector_validate(struct ged *UNUSED(gedp), bu_cmd_value_t UNUSED(type), const char *token, const void *UNUSED(data))
{
    if (BU_STR_EMPTY(token)) return GED_CMD_SEMANTIC_INCOMPLETE;
    vect_t vector = VINIT_ZERO;
    const char *argv[1] = {token};
    return bu_cmd_vector3_from_argv(vector, 1, argv) == 1 ?
	GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID;
}

static int
ged_builtin_keyword_complete(struct ged *UNUSED(gedp), const char *seed, struct ged_cmd_validate_result *result, const void *data)
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
ged_builtin_file_complete(struct ged *UNUSED(gedp), const char *seed,
	struct ged_cmd_validate_result *result, const void *data)
{
    if (!result)
	return 0;
    char **files = NULL;
    const char * const *extensions = (const char * const *)data;
    size_t cnt = bu_file_complete(seed ? seed : "", BU_FILE_COMPLETE_APPEND_SLASH,
	extensions, &files);
    result->completion_candidates = (const char **)files;
    result->completion_count = cnt;
    result->completion_type = BU_CMD_VALUE_FILE;
    return (int)cnt;
}

static int
ged_builtin_file_complete_query(struct ged *UNUSED(gedp), const char *seed,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result, const void *data)
{
    if (!result)
	return 0;
    char **files = NULL;
    size_t total = 0;
    struct bu_vls common = BU_VLS_INIT_ZERO;
    const char * const *extensions = (const char * const *)data;
    size_t cnt = bu_file_complete_query_filtered(seed ? seed : "",
	BU_FILE_COMPLETE_APPEND_SLASH, extensions,
	request ? request->max_candidates : 0,
	request ? request->candidate_filter : NULL,
	request ? request->candidate_filter_data : NULL,
	&files, &total, &common);
    result->completion_candidates = (const char **)files;
    result->completion_count = cnt;
    result->completion_total = total;
    result->completion_truncated = total > cnt;
    if (total)
	result->completion_common_prefix = bu_vls_strdup(&common);
    result->completion_type = BU_CMD_VALUE_FILE;
    bu_vls_free(&common);
    return (int)cnt;
}

static int
ged_builtin_force_all_path_complete_query(struct ged *gedp, const char *seed,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_validate_result *result, const void *UNUSED(data))
{
    const struct ged_db_completion_policy *policy = &ged_db_geometry_policy;
    int options_allowed = 1;

    for (size_t i = 1; request && request->argv && i < request->argc; i++) {
	const char *arg = request->argv[i];
	if (!arg)
	    continue;
	if (options_allowed && BU_STR_EQUAL(arg, "--")) {
	    options_allowed = 0;
	    continue;
	}
	if (!options_allowed)
	    continue;
	if (BU_STR_EQUAL(arg, "--force") || BU_STR_EQUAL(arg, "-f") ||
	    BU_STR_EQUAL(arg, "-F") ||
	    (arg[0] == '-' && arg[1] != '-' &&
	     (strchr(arg + 1, 'f') || strchr(arg + 1, 'F')))) {
	    policy = &ged_db_all_policy;
	    break;
	}
    }
    return ged_builtin_db_complete_query(gedp, seed, request, result, policy);
}


#define GED_BUILTIN_PROVIDER(_name, _type, _validate, _complete, _data, _query) \
    {_name, _validate, _complete, _data, _query, _type, \
	(_query) ? GED_CMD_PROVIDER_BOUNDED_QUERY : 0}


static void
ged_register_builtin_semantic_providers(void)
{
    struct ged_semantic_provider_registry &registry = ged_semantic_providers();
    std::lock_guard<std::mutex> guard(registry.mutex);
    if (registry.builtins_installed)
	return;

    auto install = [&registry](const struct ged_cmd_semantic_provider &provider) {
	std::string key(provider.name);
	auto inserted = registry.providers.emplace(key, provider);
	inserted.first->second.name = inserted.first->first.c_str();
    };

    const struct ged_cmd_semantic_provider db_object_provider = GED_BUILTIN_PROVIDER(
	"ged.db_object", BU_CMD_VALUE_DB_OBJECT, ged_builtin_db_object_validate,
	ged_builtin_db_complete, &ged_db_geometry_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_object_any_provider = GED_BUILTIN_PROVIDER(
	"ged.db_object_any", BU_CMD_VALUE_DB_OBJECT, ged_builtin_db_object_validate,
	ged_builtin_db_complete, &ged_db_any_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_object_hidden_provider = GED_BUILTIN_PROVIDER(
	"ged.db_object_hidden", BU_CMD_VALUE_DB_OBJECT, ged_builtin_db_object_validate,
	ged_builtin_db_complete, &ged_db_hidden_only_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_object_all_provider = GED_BUILTIN_PROVIDER(
	"ged.db_object_all", BU_CMD_VALUE_DB_OBJECT, ged_builtin_db_object_validate,
	ged_builtin_db_complete, &ged_db_all_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_object_binary_provider = GED_BUILTIN_PROVIDER(
	"ged.db_object_binary", BU_CMD_VALUE_DB_OBJECT, ged_builtin_db_object_validate,
	ged_builtin_db_complete, &ged_db_binary_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider summary_object_or_legacy_type_provider = GED_BUILTIN_PROVIDER(
	"ged.summary_object_or_legacy_type", BU_CMD_VALUE_UNKNOWN,
	ged_builtin_summary_object_or_legacy_type_validate, NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider db_path_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path", BU_CMD_VALUE_DB_PATH, ged_builtin_db_path_validate,
	ged_builtin_db_complete, &ged_db_geometry_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_path_components_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_components", BU_CMD_VALUE_DB_PATH, ged_builtin_db_components_validate,
	ged_builtin_db_components_complete, &ged_db_geometry_policy,
	ged_builtin_db_components_complete_query);
    const struct ged_cmd_semantic_provider db_path_any_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_any", BU_CMD_VALUE_DB_PATH, ged_builtin_db_path_validate,
	ged_builtin_db_complete, &ged_db_any_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_path_all_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_all", BU_CMD_VALUE_DB_PATH, ged_builtin_db_path_validate,
	ged_builtin_db_complete, &ged_db_all_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_path_geometry_hidden_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_geometry_hidden", BU_CMD_VALUE_DB_PATH, ged_builtin_db_path_validate,
	ged_builtin_db_complete, &ged_db_geometry_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider db_path_any_hidden_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_any_hidden", BU_CMD_VALUE_DB_PATH, ged_builtin_db_path_validate,
	ged_builtin_db_complete, &ged_db_any_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider search_path_provider = GED_BUILTIN_PROVIDER(
	"ged.search.path", BU_CMD_VALUE_DB_PATH, ged_builtin_search_path_validate,
	NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider search_type_provider = GED_BUILTIN_PROVIDER(
	"ged.search.type", BU_CMD_VALUE_KEYWORD, ged_builtin_search_type_validate,
	NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider search_parser_provider = GED_BUILTIN_PROVIDER(
	"ged.search", BU_CMD_VALUE_UNKNOWN, NULL, NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider view_provider = GED_BUILTIN_PROVIDER(
	"ged.view", BU_CMD_VALUE_KEYWORD, ged_builtin_view_validate,
	ged_builtin_view_complete, NULL, NULL);
    const struct ged_cmd_semantic_provider command_provider = GED_BUILTIN_PROVIDER(
	"ged.command_name", BU_CMD_VALUE_KEYWORD, ged_builtin_command_validate,
	ged_builtin_command_complete, NULL, NULL);
    const struct ged_cmd_semantic_provider primitive_type_provider = GED_BUILTIN_PROVIDER(
	"ged.primitive_type", BU_CMD_VALUE_KEYWORD, ged_builtin_primitive_type_validate,
	ged_builtin_primitive_type_complete, NULL, NULL);
    const struct ged_cmd_semantic_provider geometry_or_primitive_provider = GED_BUILTIN_PROVIDER(
	"ged.geometry_or_primitive", BU_CMD_VALUE_UNKNOWN,
	ged_builtin_geometry_or_primitive_validate,
	ged_builtin_geometry_or_primitive_complete, NULL,
	ged_builtin_geometry_or_primitive_complete_query);
    static const char * const attribute_names[] = {"aircode", "color", "inherit", "los", "material_id", "material_name", "region", "region_id", "shader", NULL};
    static const char * const display_modes[] = {"0", "1", "2", "3", "wireframe", "shaded", "shaded_all", "evaluated", NULL};
    static const char * const draw_modes[] = {"0", "1", "2", "3", "4", "5", NULL};
    static const char * const unit_names[] = {"um", "mm", "cm", "m", "km", "in", "ft", "yd", "mi", NULL};
    static const char * const svg_extensions[] = {"svg", NULL};
    const struct ged_cmd_semantic_provider path_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_or_pattern", BU_CMD_VALUE_DB_PATH, ged_builtin_path_or_pattern_validate,
	ged_builtin_path_complete, &ged_db_geometry_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_all_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_all_or_pattern", BU_CMD_VALUE_DB_PATH, ged_builtin_path_or_pattern_validate,
	ged_builtin_path_complete, &ged_db_all_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider force_all_path_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_or_pattern.force_all", BU_CMD_VALUE_DB_PATH, ged_builtin_nonempty_unknown,
	ged_builtin_path_complete, &ged_db_geometry_policy,
	ged_builtin_force_all_path_complete_query);
    const struct ged_cmd_semantic_provider path_any_hidden_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_any_hidden_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_any_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_primitives_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_primitives_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_primitives_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_primitives_hidden_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_primitives_hidden_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_primitives_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_combinations_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_combinations_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_combinations_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_combinations_hidden_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_combinations_hidden_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_combinations_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_regions_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_regions_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_regions_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider path_regions_hidden_pattern_provider = GED_BUILTIN_PROVIDER(
	"ged.db_path_regions_hidden_or_pattern", BU_CMD_VALUE_DB_PATH,
	ged_builtin_path_or_pattern_validate, ged_builtin_path_complete,
	&ged_db_regions_hidden_policy, ged_builtin_db_complete_query);
    const struct ged_cmd_semantic_provider attribute_provider = GED_BUILTIN_PROVIDER(
	"ged.db_attribute", BU_CMD_VALUE_KEYWORD, ged_builtin_nonempty_unknown,
	ged_builtin_keyword_complete, attribute_names, NULL);
    const struct ged_cmd_semantic_provider region_id_provider = GED_BUILTIN_PROVIDER(
	"ged.db_region_id", BU_CMD_VALUE_INTEGER, ged_builtin_nonnegative_integer,
	NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider display_mode_provider = GED_BUILTIN_PROVIDER(
	"ged.display_mode", BU_CMD_VALUE_KEYWORD, ged_builtin_display_mode_validate,
	ged_builtin_keyword_complete, display_modes, NULL);
    const struct ged_cmd_semantic_provider draw_mode_provider = GED_BUILTIN_PROVIDER(
	"ged.draw_mode", BU_CMD_VALUE_KEYWORD, ged_builtin_draw_mode_validate,
	ged_builtin_keyword_complete, draw_modes, NULL);
    const struct ged_cmd_semantic_provider file_path_provider = GED_BUILTIN_PROVIDER(
	"ged.file_path", BU_CMD_VALUE_FILE, ged_builtin_nonempty_unknown,
	ged_builtin_file_complete, NULL, ged_builtin_file_complete_query);
    const struct ged_cmd_semantic_provider svg_file_path_provider = GED_BUILTIN_PROVIDER(
	"ged.file_path.svg", BU_CMD_VALUE_FILE, ged_builtin_nonempty_unknown,
	ged_builtin_file_complete, svg_extensions, ged_builtin_file_complete_query);
    const struct ged_cmd_semantic_provider unit_provider = GED_BUILTIN_PROVIDER(
	"ged.unit", BU_CMD_VALUE_KEYWORD, ged_builtin_unit_validate,
	ged_builtin_keyword_complete, unit_names, NULL);
    const struct ged_cmd_semantic_provider color_provider = GED_BUILTIN_PROVIDER(
	"ged.color", BU_CMD_VALUE_COLOR, ged_builtin_color_validate, NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider matrix_provider = GED_BUILTIN_PROVIDER(
	"ged.matrix", BU_CMD_VALUE_MATRIX, ged_builtin_matrix_validate, NULL, NULL, NULL);
    const struct ged_cmd_semantic_provider vector_provider = GED_BUILTIN_PROVIDER(
	"ged.vector", BU_CMD_VALUE_VECTOR, ged_builtin_vector_validate, NULL, NULL, NULL);
    /* A schema-owned multi-token vector validator has already checked the
     * complete group.  Do not revalidate one component as a standalone packed
     * vector while completion is focused on that component. */
    const struct ged_cmd_semantic_provider vector_group_provider = GED_BUILTIN_PROVIDER(
	"ged.vector_group", BU_CMD_VALUE_VECTOR, NULL, NULL, NULL, NULL);

    install(db_object_provider);
    install(db_object_any_provider);
    install(db_object_hidden_provider);
    install(db_object_all_provider);
    install(db_object_binary_provider);
    install(summary_object_or_legacy_type_provider);
    install(db_path_provider);
    install(db_path_components_provider);
    install(db_path_any_provider);
    install(db_path_all_provider);
    install(db_path_geometry_hidden_provider);
    install(db_path_any_hidden_provider);
    install(search_path_provider);
    install(search_type_provider);
    install(search_parser_provider);
    install(view_provider);
    install(command_provider);
    install(primitive_type_provider);
    install(geometry_or_primitive_provider);
    install(path_pattern_provider);
    install(path_all_pattern_provider);
    install(force_all_path_pattern_provider);
    install(path_any_hidden_pattern_provider);
    install(path_primitives_pattern_provider);
    install(path_primitives_hidden_pattern_provider);
    install(path_combinations_pattern_provider);
    install(path_combinations_hidden_pattern_provider);
    install(path_regions_pattern_provider);
    install(path_regions_hidden_pattern_provider);
    install(attribute_provider);
    install(region_id_provider);
    install(display_mode_provider);
    install(draw_mode_provider);
    install(file_path_provider);
    install(svg_file_path_provider);
    install(unit_provider);
    install(color_provider);
    install(matrix_provider);
    install(vector_provider);
    install(vector_group_provider);
    registry.builtins_installed = true;
}

#undef GED_BUILTIN_PROVIDER


static bu_cmd_value_t
ged_semantic_provider_type(const char *name)
{
    if (BU_STR_EMPTY(name))
	return BU_CMD_VALUE_UNKNOWN;

    ged_register_builtin_semantic_providers();
    struct ged_semantic_provider_registry &registry = ged_semantic_providers();
    std::lock_guard<std::mutex> guard(registry.mutex);
    auto it = registry.providers.find(std::string(name));
    return (it == registry.providers.end()) ? BU_CMD_VALUE_UNKNOWN : it->second.value_type;
}


extern "C" void
_ged_cmd_semantic_provider_registry_reset(void)
{
    struct ged_semantic_provider_registry &registry = ged_semantic_providers();
    std::lock_guard<std::mutex> guard(registry.mutex);
    registry.providers.clear();
    registry.builtins_installed = false;
}

static ged_cmd_semantic_state_t
ged_semantic_validate(struct ged *gedp, bu_cmd_value_t type, const char *validator, const char *token)
{
    if (!validator || !validator[0])
	return GED_CMD_SEMANTIC_UNKNOWN;

    ged_register_builtin_semantic_providers();
    struct ged_cmd_semantic_provider provider = GED_CMD_SEMANTIC_PROVIDER_NULL;
    {
	struct ged_semantic_provider_registry &registry = ged_semantic_providers();
	std::lock_guard<std::mutex> guard(registry.mutex);
	auto it = registry.providers.find(std::string(validator));
	if (it != registry.providers.end())
	    provider = it->second;
    }
    if (provider.validate)
	return (*provider.validate)(gedp, type, token, provider.data);

    return GED_CMD_SEMANTIC_UNKNOWN;
}

static int
ged_semantic_result_normalize(const char *seed,
	const struct ged_cmd_completion_request *request,
	bool bounded_query,
	struct ged_cmd_validate_result *result)
{
    if (!result)
	return -1;
    /* Public completion functions return an int, but an unbounded query may
     * otherwise legitimately contain every object in a very large database.
     * Do not impose an unrelated interactive viewport limit here. */
    if (result->completion_count > (size_t)INT_MAX ||
	    (result->completion_count && !result->completion_candidates)) {
	ged_cmd_validate_result_clear(result);
	return -1;
    }

    const size_t original_count = result->completion_count;
    if ((result->completion_truncated &&
	    (!result->completion_total ||
	     result->completion_total <= original_count)) ||
	(bounded_query && request && request->max_candidates &&
	 result->completion_count > request->max_candidates)) {
	ged_cmd_validate_result_clear(result);
	return -1;
    }
    size_t reported_total = result->completion_total ?
	result->completion_total : original_count;
    bool reported_truncated = result->completion_truncated ||
	reported_total > original_count;
    if (reported_total < original_count)
	reported_total = original_count;

    std::string prefix = seed ? seed : "";
    std::set<std::string> unique;
    std::vector<std::string> accepted;
    accepted.reserve(original_count);
    for (size_t i = 0; i < original_count; i++) {
	const char *candidate = result->completion_candidates[i];
	if (BU_STR_EMPTY(candidate))
	    continue;
	std::string value(candidate);
	if (!ged_utf8_valid(value) || !ged_completion_prefix_match(value, prefix) ||
		!ged_completion_request_accepts(request, candidate) ||
		!unique.insert(value).second)
	    continue;
	accepted.push_back(std::move(value));
    }
    std::sort(accepted.begin(), accepted.end(),
	[](const std::string &a, const std::string &b) {
	    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
	});

    /* Filtering or deduplicating a truncated materialization would leave no
     * way to repair its exact full-set total.  Complete results can be
     * normalized and recounted, but bounded providers must honor their
     * declared prefix/filter/uniqueness contract exactly. */
    if (reported_truncated && accepted.size() != original_count) {
	ged_cmd_validate_result_clear(result);
	return -1;
    }

    if (result->completion_candidates)
	bu_argv_free(original_count, (char **)result->completion_candidates);
    result->completion_candidates = NULL;
    result->completion_count = 0;

    size_t materialized = accepted.size();
    if (request && request->max_candidates)
	materialized = std::min(materialized, request->max_candidates);
    result->completion_candidates = (const char **)bu_calloc(materialized + 1,
	sizeof(char *), "normalized semantic candidates");
    for (size_t i = 0; i < materialized; i++)
	result->completion_candidates[i] = bu_strdup(accepted[i].c_str());
    result->completion_count = materialized;

    if (!reported_truncated)
	reported_total = accepted.size();
    else
	reported_total = std::max(reported_total, accepted.size());
    result->completion_total = reported_total;
    result->completion_truncated = reported_total > materialized;

    bool common_supplied = result->completion_common_prefix != NULL;
    bool common_valid = common_supplied;
    std::string common = common_valid ? result->completion_common_prefix : "";
    if (result->completion_common_prefix) {
	bu_free(result->completion_common_prefix, "provider common prefix");
	result->completion_common_prefix = NULL;
    }
    if (!reported_truncated) {
	common.clear();
	bool initialized = false;
	for (const std::string &candidate : accepted)
	    ged_completion_common_add(common, initialized, candidate.c_str());
	common_valid = initialized;
    } else if (common_valid) {
	if (!ged_utf8_valid(common))
	    common_valid = false;
	size_t boundary = ged_utf8_prefix_boundary(common, common.size());
	common.resize(boundary);
	if (!ged_completion_prefix_match(common, prefix))
	    common_valid = false;
	for (const std::string &candidate : accepted)
	    if (candidate.compare(0, common.size(), common)) {
		common_valid = false;
		break;
	    }
    }
    /* A bounded provider may materialize only part of its result.  In that
     * case libged cannot reconstruct the full-set common prefix, so accepting
     * an absent or inconsistent one would make Tab insertion depend on the
     * arbitrary materialized subset. */
    if (reported_truncated && (!request || !request->max_candidates ||
	    !common_supplied || !common_valid)) {
	ged_cmd_validate_result_clear(result);
	return -1;
    }
    if (common_valid)
	result->completion_common_prefix = bu_strdup(common.c_str());
    return 0;
}


static int
ged_semantic_complete(struct ged *gedp, const char *provider, const char *seed,
		struct ged_cmd_validate_result *result,
		const struct ged_cmd_completion_request *request = NULL)
{
    if (BU_STR_EMPTY(provider) || !result)
	return 0;
    ged_register_builtin_semantic_providers();
    struct ged_cmd_semantic_provider selected = GED_CMD_SEMANTIC_PROVIDER_NULL;
    {
	struct ged_semantic_provider_registry &registry = ged_semantic_providers();
	std::lock_guard<std::mutex> guard(registry.mutex);
	auto it = registry.providers.find(std::string(provider));
	if (it != registry.providers.end())
	    selected = it->second;
    }
    int ret = 0;
    if (request && selected.complete_query)
	ret = (*selected.complete_query)(gedp, seed ? seed : "", request, result, selected.data);
    else if (selected.complete)
	ret = (*selected.complete)(gedp, seed ? seed : "", result, selected.data);
    else
	return 0;
    std::string candidate_seed = seed ? seed : "";
    if (selected.value_type == BU_CMD_VALUE_DB_PATH) {
	size_t separator = ged_last_unescaped_path_separator(candidate_seed);
	if (separator != std::string::npos)
	    candidate_seed.erase(0, separator + 1);
    }
    if (ret < 0 || ged_semantic_result_normalize(candidate_seed.c_str(),
	    request, request && selected.complete_query, result)) {
	ged_cmd_validate_result_clear(result);
	return -1;
    }
    return (int)result->completion_count;
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
    if (cl)
	bu_argv_free(cmd_cnt, (char **)cl);

    return ret;
}

extern "C" void
ged_cmd_analysis_init(struct ged_cmd_analysis *analysis)
{
    if (!analysis)
	return;
    analysis->token_count = 0;
    analysis->tokens = NULL;
    analysis->owned_storage = NULL;
}


extern "C" void
ged_cmd_analysis_clear(struct ged_cmd_analysis *analysis)
{
    if (!analysis)
	return;
    if (analysis->owned_storage)
	bu_free(analysis->owned_storage, "analysis string storage");
    if (analysis->tokens)
	bu_free(analysis->tokens, "ged command analysis tokens");
    ged_cmd_analysis_init(analysis);
}


static void
ged_cmd_analysis_publish(struct ged_cmd_analysis *analysis)
{
    if (!analysis || analysis->owned_storage ||
	    _ged_registry_access_depth() != 1)
	return;

    std::unordered_map<std::string, size_t> offsets;
    size_t storage_size = 0;
    auto count_string = [&offsets, &storage_size](const char *value) {
	if (!value)
	    return;
	auto inserted = offsets.emplace(value, storage_size);
	if (inserted.second)
	    storage_size += inserted.first->first.size() + 1;
    };

    for (size_t i = 0; i < analysis->token_count; i++) {
	count_string(analysis->tokens[i].validator);
	count_string(analysis->tokens[i].hint);
    }
    if (!storage_size)
	return;

    char *storage = (char *)bu_malloc(storage_size,
	"analysis string storage");
    for (const auto &entry : offsets)
	bu_strlcpy(storage + entry.second, entry.first.c_str(),
	    storage_size - entry.second);
    auto stored_string = [&offsets, storage](const char *value) -> const char * {
	if (!value)
	    return NULL;
	auto entry = offsets.find(value);
	return (entry == offsets.end()) ? NULL : storage + entry->second;
    };
    for (size_t i = 0; i < analysis->token_count; i++) {
	analysis->tokens[i].validator =
	    stored_string(analysis->tokens[i].validator);
	analysis->tokens[i].hint = stored_string(analysis->tokens[i].hint);
    }
    analysis->owned_storage = storage;
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
    for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	const struct bu_cmd_option *option = &schema->options[i];
	const char *spelling = longopt ? option->longopt : option->shortopt;
	size_t spelling_len = spelling ? strlen(spelling) : 0;
	if (!spelling || bu_strncmp(name, spelling, spelling_len) ||
		(name[spelling_len] && name[spelling_len] != '='))
	    continue;
	if (!option->alias_of)
	    return option;
	for (size_t ci = 0; bu_cmd_option_is_valid(&schema->options[ci]); ci++) {
	    const struct bu_cmd_option *canonical = &schema->options[ci];
	    if (!canonical->alias_of && BU_STR_EQUAL(bu_cmd_option_canonical(canonical), option->alias_of))
		return canonical;
	}
	return NULL;
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
    int pending_valid = 1;
    size_t operand_count = 0;
    void *analysis_plan = schema->validation.custom_validate ?
	_ged_schema_analysis_plan_create(schema, parsed.argc - 1,
	    (const char **)parsed.argv + 1) : NULL;
    bool custom_token_validate = schema->validation.custom_validate &&
	!analysis_plan;

    for (size_t i = 1; i < parsed.argc; i++) {
	const char *arg = parsed.argv[i];
	struct ged_cmd_analysis_token *token = &analysis->tokens[i];
	if (pending) {
	    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
	    const char *provider = ged_native_semantic_provider(pending->value_type, pending->semantic_provider);
	    if (custom_token_validate) {
		(void)bu_cmd_schema_validate(schema, i, (const char **)parsed.argv + 1,
		    i - 1, &result);
		const char *dynamic_provider = ged_native_semantic_provider(
		    result.completion_type, result.semantic_provider);
		if (dynamic_provider)
		    provider = dynamic_provider;
	    }
	    token->role = GED_CMD_TOKEN_OPTION_ARG;
	    token->value_type = ged_native_value_type(
		custom_token_validate ? result.completion_type :
		pending->value_type, provider);
	    token->semantic_state = custom_token_validate ?
		ged_native_semantic_state(result.state) :
		(pending_valid ? GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID);
	    token->validator = provider;
	    if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
		ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, arg);
		if (state != GED_CMD_SEMANTIC_UNKNOWN)
		    token->semantic_state = state;
	    }
	    token->hint = (custom_token_validate && result.hint) ?
		result.hint : "option argument";
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
		if (custom_token_validate) {
		    (void)bu_cmd_schema_validate(schema, i,
			(const char **)parsed.argv + 1, i - 1, &result);
		    const char *dynamic_provider = ged_native_semantic_provider(
			result.completion_type, result.semantic_provider);
		    if (dynamic_provider)
			provider = dynamic_provider;
		}
		token->role = GED_CMD_TOKEN_OPTION_ARG;
		token->value_type = ged_native_value_type(
		    custom_token_validate ? result.completion_type :
		    option->value_type, provider);
		token->semantic_state = custom_token_validate ?
		    ged_native_semantic_state(result.state) :
		    (bu_cmd_schema_option_span(schema, 1, &arg) > 0 ? GED_CMD_SEMANTIC_VALID :
			GED_CMD_SEMANTIC_INVALID);
		token->validator = provider;
		if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
		    ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, eq + 1);
		    if (state != GED_CMD_SEMANTIC_UNKNOWN)
			token->semantic_state = state;
		}
		token->hint = (custom_token_validate && result.hint) ?
		    result.hint : "option with argument";
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
		    pending_valid = 1;
		} else if (option_span < 0 && i + 1 < parsed.argc) {
		    /* Preserve a precise option-argument classification when the
		     * shared parser detects a malformed or incomplete value. */
		    pending = option;
		    pending_count = 1;
		    pending_valid = 0;
		}
	    }
	    continue;
	}
	const struct bu_cmd_operand *operand = bu_cmd_schema_active_operand(schema,
	    parsed.argc - 1, (const char **)parsed.argv + 1, operand_count);
	const char *provider = operand ? ged_native_semantic_provider(operand->value_type, operand->semantic_provider) : NULL;
	struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
	bool dynamic_role = analysis_plan &&
	    _ged_schema_analysis_plan_role(analysis_plan, operand, &result);
	if (dynamic_role)
	    provider = ged_native_semantic_provider(result.completion_type,
		result.semantic_provider);
	/* The scanner already knows the declarative operand role.  Revalidating
	 * every growing argv prefix here made highlighting quadratic in command
	 * length.  Only a schema-owned callback can override that role, so invoke
	 * validation solely for those uncommon dynamic schemas. */
	if (custom_token_validate) {
	    (void)bu_cmd_schema_validate(schema, i, (const char **)parsed.argv + 1,
		i - 1, &result);
	    const char *dynamic_provider = ged_native_semantic_provider(
		result.completion_type, result.semantic_provider);
	    if (dynamic_provider || schema->validation.cases)
		provider = dynamic_provider;
	}
	/* A custom native schema may own a child command phase after one or
	 * more operands (for example, view obj <name> axes).  Its validator is
	 * authoritative about that phase, so preserve the subcommand role rather
	 * than flattening every non-root word into an operand. */
	token->role = (custom_token_validate &&
	    (result.expected & BU_CMD_EXPECT_SUBCOMMAND)) ?
	    GED_CMD_TOKEN_SUBCOMMAND : GED_CMD_TOKEN_OPERAND;
	token->value_type = ged_native_value_type(
	    custom_token_validate || dynamic_role ? result.completion_type :
	    (operand ? operand->value_type : BU_CMD_VALUE_UNKNOWN), provider);
	token->semantic_state = custom_token_validate ?
	    ged_native_semantic_state(result.state) :
	    (operand && bu_cmd_operand_validate(operand, arg) ?
		GED_CMD_SEMANTIC_VALID : GED_CMD_SEMANTIC_INVALID);
	token->validator = provider;
	if (token->semantic_state == GED_CMD_SEMANTIC_VALID && token->validator) {
	    ged_cmd_semantic_state_t state = ged_semantic_validate(gedp, token->value_type, token->validator, arg);
	    if (state != GED_CMD_SEMANTIC_UNKNOWN)
		token->semantic_state = state;
	}
	token->hint = ((custom_token_validate || dynamic_role) && result.hint) ? result.hint :
	    (operand ? operand->name : "unexpected operand");
	bu_cmd_validate_result_clear(&result);
	operand_count++;
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
    }
    _ged_schema_analysis_plan_destroy(analysis_plan);
}

extern "C" int
ged_cmd_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

    if (!analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (!input)
	return -1;

    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;

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
	if (!ret)
	    ged_cmd_analysis_publish(analysis);
	else
	    ged_cmd_analysis_clear(analysis);
	return ret;
    }

    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(parsed.argv[0]);
    if (native_schema) {
	ged_native_analyze(gedp, native_schema, parsed, analysis);
	ged_input_parse_free(&parsed);
	ged_cmd_analysis_publish(analysis);
	return 0;
    }

    ged_input_parse_free(&parsed);
    ged_cmd_analysis_publish(analysis);
    return 0;
}

/* Translate a compact native schema through GED's semantic providers.  Grammar
 * adapters with context-selected flat forms use this same path, rather than
 * reimplementing completion or token-state rules. */
static int
ged_native_validate(struct ged *gedp, const struct bu_cmd_schema *native_schema,
	const struct ged_input_parse &parsed, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
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
	result->candidate_validate = native_result.candidate_validate;
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
	    const char *semantic_arg = parsed.argv[parsed.cursor_arg];
	    if (result->expected & BU_CMD_EXPECT_OPTION_ARG) {
		const char *equal = strrchr(semantic_arg, '=');
		if (equal)
		    semantic_arg = equal + 1;
	    }
	    ged_cmd_semantic_state_t semantic_state = ged_semantic_validate(gedp,
		result->completion_type, provider, semantic_arg);
	    if (semantic_state == GED_CMD_SEMANTIC_INVALID)
		result->state = BU_CMD_VALIDATE_INVALID;
	    else if (semantic_state == GED_CMD_SEMANTIC_INCOMPLETE)
		result->state = BU_CMD_VALIDATE_INCOMPLETE;
	}
    }
    bu_cmd_validate_result_clear(&native_result);
    return ret;
}


static int
ged_cmd_validate_parsed(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_input_parse &parsed, const std::string &seed,
	struct ged_cmd_validate_result *result)
{
    const char *cmd = NULL;
    int cmd_exists = 0;

    if (!input || !result)
	return -1;

    cmd = (parsed.argc > 0) ? parsed.argv[0] : NULL;
    if (parsed.cursor_arg == 0 && !seed.empty())
	cmd = seed.c_str();
    cmd_exists = (cmd) ? ged_cmd_exists(cmd) : 0;

    if (!parsed.argc || parsed.cursor_arg == 0 || !cmd_exists) {
	bu_cmd_validate_state_t state = (!parsed.argc) ? BU_CMD_VALIDATE_INCOMPLETE :
	    (cmd_exists ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_UNKNOWN);
	const char *hint = (!parsed.argc) ? "command expected" :
	    (cmd_exists ? "valid command" : "unknown command");
	ged_set_validate_result(result, state, 0, 0, BU_CMD_EXPECT_SUBCOMMAND, hint);
	ged_set_result_chars(result, parsed, 0, 0);
	ged_fill_command_candidates(result, seed);
	return 0;
    }

    /* Native validators inspect argv, so present the cursor-local prefix in
     * place of the full token.  Text following the cursor remains available
     * through the source spans and is not allowed to narrow completion. */
    if (parsed.cursor_arg < parsed.argc)
	parsed.argv[parsed.cursor_arg] = (char *)seed.c_str();

    const struct ged_cmd_grammar *grammar = _ged_cmd_grammar(cmd);
    if (grammar && grammar->validate)
	return grammar->validate(gedp, input, cursor_pos, result);

    const struct bu_cmd_schema *native_schema = _ged_cmd_native_schema(cmd);
    if (native_schema)
	return ged_native_validate(gedp, native_schema, parsed, input, cursor_pos,
	    result);


    size_t token_index = (parsed.cursor_arg < parsed.argc) ? parsed.cursor_arg : parsed.argc;
    ged_set_validate_result(result, BU_CMD_VALIDATE_UNKNOWN, token_index, token_index,
	BU_CMD_EXPECT_NONE, "syntax metadata unavailable");
    ged_set_result_chars(result, parsed, token_index, token_index);
    if (parsed.cursor_arg > 0 && gedp && gedp->dbip) {
	result->completion_type = (seed.find('/') != std::string::npos) ?
	    BU_CMD_VALUE_DB_PATH : BU_CMD_VALUE_DB_OBJECT;
    }
    return 0;
}


static int
ged_cmd_validate_internal(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;

    if (!input || !result)
	return -1;
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    std::string seed = ged_cursor_seed(parsed);
    int ret = ged_cmd_validate_parsed(gedp, input, cursor_pos, parsed, seed,
	result);
    ged_input_parse_free(&parsed);
    return ret;
}


extern "C" int
ged_cmd_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    if (!result)
	return -1;
    ged_cmd_validate_result_clear(result);
    if (!input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    int ret = ged_cmd_validate_internal(gedp, input, cursor_pos, result);
    if (!ret)
	ged_cmd_validate_result_publish(result);
    else
	ged_cmd_validate_result_clear(result);
    return ret;
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
    child.cursor_seed = parsed.cursor_seed;
    child.cursor_path_seed = parsed.cursor_path_seed;
    child.cursor_content_start = parsed.cursor_content_start;
    child.cursor_component_start = parsed.cursor_component_start;
    child.cursor_value_start = parsed.cursor_value_start;
    child.cursor_replace_end = parsed.cursor_replace_end;
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
	    cursor_pos, result);
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
	    cursor_pos, result);
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
	result);
	result->token_start += token_offset + subcommand_index;
	result->token_end += token_offset + subcommand_index;
    return ret;
}


static void
ged_native_tree_apply_full_state(const struct bu_cmd_tree *tree,
	const struct ged_input_parse &parsed, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct bu_cmd_validate_result full_result = BU_CMD_VALIDATE_RESULT_NULL;

    if (!tree || !input || !result || result->state != BU_CMD_VALIDATE_VALID ||
	cursor_pos != strlen(input) || parsed.cursor_arg >= parsed.argc ||
	parsed.argc < 1)
	return;
    if (bu_cmd_tree_validate_argv(tree, parsed.argc - 1,
	(const char **)parsed.argv + 1, parsed.argc - 1, &full_result) == 0 &&
	full_result.state != BU_CMD_VALIDATE_VALID &&
	full_result.state != BU_CMD_VALIDATE_UNKNOWN)
	result->state = ged_native_state(full_result.state);
    bu_cmd_validate_result_clear(&full_result);
}


extern "C" GED_EXPORT int
ged_cmd_native_validate(struct ged *gedp, const struct bu_cmd_schema *schema,
	const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    int ret = 0;

    if (!result)
	return -1;
    ged_cmd_validate_result_clear(result);
    if (!schema || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    if (parsed.cursor_arg < parsed.argc)
	parsed.argv[parsed.cursor_arg] = (char *)parsed.cursor_seed.c_str();
    ret = ged_native_validate(gedp, schema, parsed, input, cursor_pos, result);
    ged_input_parse_free(&parsed);
    if (!ret)
	ged_cmd_validate_result_publish(result);
    else
	ged_cmd_validate_result_clear(result);
    return ret;
}


extern "C" GED_EXPORT int
ged_cmd_tree_validate(struct ged *gedp, const struct bu_cmd_tree *tree,
	const char *input, size_t cursor_pos, struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    int ret = 0;

    if (!result)
	return -1;
    ged_cmd_validate_result_clear(result);
    if (!tree || !tree->root_schema || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    if (parsed.cursor_arg < parsed.argc)
	parsed.argv[parsed.cursor_arg] = (char *)parsed.cursor_seed.c_str();
    ret = ged_native_tree_validate_level(gedp, tree, parsed, input,
	cursor_pos, result, 0);
    if (!ret)
	ged_native_tree_apply_full_state(tree, parsed, input, cursor_pos, result);
    ged_input_parse_free(&parsed);
    if (!ret)
	ged_cmd_validate_result_publish(result);
    else
	ged_cmd_validate_result_clear(result);
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


extern "C" GED_EXPORT int
ged_cmd_native_analyze(struct ged *gedp, const struct bu_cmd_schema *schema,
	const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;

    if (!analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (!schema || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    ged_native_analyze(gedp, schema, parsed, analysis);
    ged_input_parse_free(&parsed);
    ged_cmd_analysis_publish(analysis);
    return 0;
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

    if (!analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (!tree || !tree->root_schema || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    ret = ged_native_tree_analyze_level(gedp, tree, parsed, analysis);
    ged_input_parse_free(&parsed);
    if (!ret)
	ged_cmd_analysis_publish(analysis);
    else
	ged_cmd_analysis_clear(analysis);
    return ret;
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_validate(struct ged *gedp, const struct bu_cmd_forms *forms,
	const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    struct ged_input_parse parsed;
    const struct bu_cmd_form *form;
    int ret = 0;

    if (!result)
	return -1;
    ged_cmd_validate_result_clear(result);
    if (!forms || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (cursor_pos > strlen(input))
	cursor_pos = strlen(input);
    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return -1;
    }
    if (parsed.cursor_arg < parsed.argc)
	parsed.argv[parsed.cursor_arg] = (char *)parsed.cursor_seed.c_str();
    form = bu_cmd_forms_select(forms, parsed.argc,
	(const char * const *)parsed.argv, gedp);
    if (!form) {
	ged_set_validate_result(result, BU_CMD_VALIDATE_INVALID, parsed.cursor_arg,
	    parsed.cursor_arg, BU_CMD_EXPECT_NONE, "no matching native command form");
	ged_set_result_chars(result, parsed, result->token_start, result->token_end);
	ged_input_parse_free(&parsed);
	ged_cmd_validate_result_publish(result);
	return 0;
    }
    if (form->tree) {
	ret = ged_native_tree_validate_level(gedp, form->tree, parsed, input,
	    cursor_pos, result, 0);
	if (!ret)
	    ged_native_tree_apply_full_state(form->tree, parsed, input, cursor_pos,
		result);
    } else {
	ret = ged_native_validate(gedp, form->schema, parsed, input, cursor_pos,
	    result);
    }
    ged_input_parse_free(&parsed);
    if (!ret)
	ged_cmd_validate_result_publish(result);
    else
	ged_cmd_validate_result_clear(result);
    return ret;
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_analyze(struct ged *gedp, const struct bu_cmd_forms *forms,
	const char *input, struct ged_cmd_analysis *analysis)
{
    struct ged_input_parse parsed;
    const struct bu_cmd_form *form;
    int ret = 0;

    if (!analysis)
	return -1;
    ged_cmd_analysis_clear(analysis);
    if (!forms || !input)
	return -1;
    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;
    if (ged_input_parse_line(&parsed, input, strlen(input)) != 0)
	return -1;
    if (!parsed.argc) {
	ged_input_parse_free(&parsed);
	return 0;
    }
    ged_native_tree_analysis_initialize(analysis, parsed);
    form = bu_cmd_forms_select(forms, parsed.argc,
	(const char * const *)parsed.argv, gedp);
    if (!form) {
	ged_input_parse_free(&parsed);
	ged_cmd_analysis_clear(analysis);
	return -1;
    }
    if (form->tree)
	ret = ged_native_tree_analyze_level(gedp, form->tree, parsed, analysis);
    else
	ged_native_analyze(gedp, form->schema, parsed, analysis);
    ged_input_parse_free(&parsed);
    if (!ret)
	ged_cmd_analysis_publish(analysis);
    else
	ged_cmd_analysis_clear(analysis);
    return ret;
}


extern "C" GED_EXPORT char *
ged_cmd_native_forms_describe_json(const struct bu_cmd_forms *forms)
{
    return bu_cmd_forms_describe_json(forms);
}


extern "C" GED_EXPORT char *
ged_cmd_native_forms_help(const struct bu_cmd_forms *forms,
	const char *invocation)
{
    return bu_cmd_forms_help(forms, invocation);
}


extern "C" GED_EXPORT int
ged_cmd_native_forms_lint(const struct bu_cmd_forms *forms,
	struct bu_vls *msgs)
{
    return bu_cmd_forms_lint(forms, msgs);
}


struct ged_completion_filter_context {
    bu_cmd_value_validate_t validate = NULL;
    std::string cursor_token;
    std::string seed;
    bu_cmd_value_t type = BU_CMD_VALUE_UNKNOWN;
    unsigned int expected = BU_CMD_EXPECT_NONE;
};


static int
ged_completion_candidate_filter(const char *candidate, const void *data)
{
    const struct ged_completion_filter_context *context =
	(const struct ged_completion_filter_context *)data;
    if (!context || !context->validate || !candidate)
	return -1;

    if (context->type == BU_CMD_VALUE_FILE && candidate[0]) {
	size_t length = strlen(candidate);
	if (candidate[length - 1] == '/' || candidate[length - 1] == '\\')
	    return 0;
    }

    std::string proposed = context->cursor_token;
    size_t decoded_start = 0;
    size_t decoded_length = context->seed.size();
    if (context->type == BU_CMD_VALUE_DB_OBJECT ||
	context->type == BU_CMD_VALUE_DB_PATH) {
	size_t separator = context->seed.rfind('/');
	if (separator != std::string::npos)
	    decoded_start = separator + 1;
    } else if (context->expected & BU_CMD_EXPECT_OPTION_ARG) {
	size_t equal = context->seed.rfind('=');
	if (equal != std::string::npos)
	    decoded_start = equal + 1;
    }
    decoded_length -= std::min(decoded_start, decoded_length);
    if (decoded_start <= proposed.size())
	proposed.replace(decoded_start, decoded_length, candidate);
    else
	proposed = candidate;
    if (context->expected & BU_CMD_EXPECT_OPTION_ARG) {
	size_t equal = proposed.rfind('=');
	if (equal != std::string::npos)
	    proposed.erase(0, equal + 1);
    }
    return context->validate(NULL, proposed.c_str());
}


static int
ged_cmd_complete_result_internal(struct ged *gedp, const char *input, size_t cursor_pos,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_completion_result *result)
{
    struct ged_input_parse parsed;
    struct ged_cmd_validate_result vr = GED_CMD_VALIDATE_RESULT_NULL;
    std::string seed;
    std::string path_seed;
    std::string cursor_token;
    std::string completion_prefix;
    size_t input_len = 0;

    if (!result)
	return 0;
    ged_cmd_completion_result_clear(result);
    if (!input)
	return 0;

    ged_ensure_initialized();
    ged_completion_registry_reference reference;
    if (!reference.acquired())
	return -1;

    input_len = strlen(input);
    if (cursor_pos > input_len)
	cursor_pos = input_len;

    if (ged_input_parse_line(&parsed, input, cursor_pos) != 0)
	return -1;
    seed = ged_cursor_seed(parsed);
    path_seed = parsed.cursor_path_seed;
    if (parsed.cursor_arg < parsed.argc && parsed.argv[parsed.cursor_arg])
	cursor_token = parsed.argv[parsed.cursor_arg];
    /* Validation identifies the active role and semantic provider.  Candidate
     * enumeration is a completion operation and is performed exactly once
     * below.  Keeping these phases separate is essential for large databases:
     * a validation probe must never rescan and sort the object directory. */
    int validation_status = ged_cmd_validate_parsed(gedp, input, cursor_pos,
	parsed, seed, &vr);
    if (validation_status < 0) {
	ged_cmd_validate_result_clear(&vr);
	ged_input_parse_free(&parsed);
	return -1;
    }
    const char *active_cmd = parsed.argc ? parsed.argv[0] : NULL;
    const struct ged_cmd_grammar *active_grammar = active_cmd ?
	_ged_cmd_grammar(active_cmd) : NULL;
    if (validation_status == 0 && active_grammar && active_grammar->complete) {
	struct ged_cmd_completion_request unbounded = GED_CMD_COMPLETION_REQUEST_NULL;
	unbounded.cursor_pos = cursor_pos;
	unbounded.flags = GED_CMD_COMPLETION_WANT_CONTEXT;
	const struct ged_cmd_completion_request *grammar_request = request ?
	    request : &unbounded;
	if (active_grammar->complete(gedp, input, cursor_pos, grammar_request,
		&vr) < 0) {
	    ged_cmd_validate_result_clear(&vr);
	    ged_input_parse_free(&parsed);
	    return -1;
	}
	std::string grammar_seed = seed;
	if (vr.completion_type == BU_CMD_VALUE_DB_PATH) {
	    size_t separator = ged_last_unescaped_path_separator(grammar_seed);
	    if (separator != std::string::npos)
		grammar_seed.erase(0, separator + 1);
	} else if ((vr.expected & BU_CMD_EXPECT_OPTION_ARG) &&
		parsed.cursor_value_start > parsed.cursor_content_start) {
	    size_t equal = grammar_seed.rfind('=');
	    if (equal != std::string::npos)
		grammar_seed.erase(0, equal + 1);
	}
	if ((vr.completion_candidates || vr.completion_count ||
		vr.completion_total || vr.completion_truncated) &&
	    ged_semantic_result_normalize(grammar_seed.c_str(), grammar_request,
		true, &vr)) {
	    ged_cmd_validate_result_clear(&vr);
	    ged_input_parse_free(&parsed);
	    return -1;
	}
    }

    /* At an empty operand position, interspersed schemas report that both
     * options and operands are legal.  Their native validator can only attach
     * one candidate list, so it supplies option spellings and the semantic
     * provider adds operands below.  Shell completion convention is to offer
     * options only after a '-' seed; do not mix those spellings into a large
     * object listing or context-validate every object because options happened
     * to be legal at the same position. */
    if (seed.empty() && (vr.expected & BU_CMD_EXPECT_OPERAND) &&
	(vr.completion_type == BU_CMD_VALUE_DB_OBJECT ||
	 vr.completion_type == BU_CMD_VALUE_DB_PATH) &&
	vr.completion_candidates && vr.completion_count) {
	size_t kept = 0;
	for (size_t i = 0; i < vr.completion_count; i++) {
	    const char *candidate = vr.completion_candidates[i];
	    if (candidate && candidate[0] != '-') {
		vr.completion_candidates[kept++] = candidate;
	    } else if (candidate) {
		bu_free((void *)candidate, "option completion at empty operand");
	    }
	}
	vr.completion_count = kept;
	vr.completion_candidates[kept] = NULL;
    }

    /* A custom validator may publish a shared option table while admitting
     * only a subset in the current form.  Option sets are small and bounded,
     * so validating those spellings is inexpensive.  Do not apply this loop
     * to semantic candidates: database-sized candidate sets made the former
     * generic version of this check quadratic. */
    if (!seed.empty() && seed[0] == '-' && vr.completion_candidates &&
	    (vr.expected & BU_CMD_EXPECT_OPTION) &&
	    parsed.cursor_arg < parsed.argc && cursor_pos == parsed.char_ends[parsed.cursor_arg]) {
	size_t write_idx = 0;
	for (size_t i = 0; i < vr.completion_count; i++) {
	    const char *candidate = vr.completion_candidates[i];
	    bool keep = candidate != NULL;
	    if (keep && candidate[0] == '-') {
		std::string probe(input);
		probe.replace(parsed.cursor_content_start,
		    parsed.cursor_replace_end - parsed.cursor_content_start, candidate);
		struct ged_cmd_validate_result candidate_vr = GED_CMD_VALIDATE_RESULT_NULL;
		(void)ged_cmd_validate_internal(gedp, probe.c_str(),
		    parsed.cursor_content_start + strlen(candidate), &candidate_vr);
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
	    seed[0] == '-' && seed.find('=') == std::string::npos &&
	    (vr.expected & BU_CMD_EXPECT_OPTION_ARG) &&
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
		    bu_strncmp(vr.completion_candidates[i], seed.c_str(), seed.size()) == 0) {
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

    result->replacement_start = parsed.cursor_content_start;
    result->replacement_end = parsed.cursor_replace_end;
    result->completion_type = vr.completion_type;
    result->expected = vr.expected;
    result->hint = vr.hint ? bu_strdup(vr.hint) : NULL;

    struct ged_cmd_analysis active_analysis = GED_CMD_ANALYSIS_NULL;
    if ((!request || (request->flags & GED_CMD_COMPLETION_WANT_CONTEXT)) &&
	    ged_cmd_analyze(gedp, input, &active_analysis) == 0) {
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
    ged_cmd_analysis_clear(&active_analysis);

    bool schema_option_candidates = (vr.completion_count && !seed.empty() && seed[0] == '-' &&
	(vr.expected & BU_CMD_EXPECT_OPTION));
    if (!schema_option_candidates && vr.semantic_provider &&
	    (vr.expected & (BU_CMD_EXPECT_OPERAND | BU_CMD_EXPECT_OPTION_ARG))) {
	struct ged_cmd_validate_result semantic = GED_CMD_VALIDATE_RESULT_NULL;
	struct ged_cmd_completion_request provider_request =
	    GED_CMD_COMPLETION_REQUEST_NULL;
	struct ged_completion_filter_context filter_context;
	struct ged_completion_merge_filter_context merge_filter_context;
	std::set<std::string> static_candidates;
	std::string semantic_seed =
	    (vr.completion_type == BU_CMD_VALUE_DB_OBJECT ||
	     vr.completion_type == BU_CMD_VALUE_DB_PATH) ? path_seed : seed;
	if (request)
	    provider_request = *request;
	provider_request.cursor_pos = cursor_pos;
	provider_request.argc = parsed.argc;
	provider_request.argv = (const char * const *)parsed.argv;
	provider_request.cursor_arg = parsed.cursor_arg;
	if (vr.candidate_validate) {
	    filter_context.validate = vr.candidate_validate;
	    filter_context.cursor_token = cursor_token;
	    filter_context.seed = seed;
	    filter_context.type = vr.completion_type;
	    filter_context.expected = vr.expected;
	    provider_request.candidate_filter = ged_completion_candidate_filter;
	    provider_request.candidate_filter_data = &filter_context;
	}
	/* Static schema candidates are already part of the result.  Excluding
	 * them inside bounded query providers keeps union totals and truncation
	 * exact even when an overlapping semantic set is not fully materialized. */
	if (vr.completion_count) {
	    for (size_t i = 0; i < vr.completion_count; i++)
		if (vr.completion_candidates[i])
		    static_candidates.insert(vr.completion_candidates[i]);
	    merge_filter_context.filter = provider_request.candidate_filter;
	    merge_filter_context.filter_data = provider_request.candidate_filter_data;
	    merge_filter_context.excluded = &static_candidates;
	    provider_request.candidate_filter = ged_completion_merge_filter;
	    provider_request.candidate_filter_data = &merge_filter_context;
	}
	if ((vr.expected & BU_CMD_EXPECT_OPTION_ARG) &&
		parsed.cursor_value_start > parsed.cursor_content_start) {
	    size_t equal = semantic_seed.rfind('=');
	    if (equal != std::string::npos)
		semantic_seed = semantic_seed.substr(equal + 1);
	}
	semantic.completion_type = vr.completion_type;
	if (ged_semantic_complete(gedp, vr.semantic_provider,
		semantic_seed.c_str(), &semantic, &provider_request) < 0) {
	    ged_cmd_validate_result_clear(&semantic);
	    ged_cmd_validate_result_clear(&vr);
	    ged_input_parse_free(&parsed);
	    ged_cmd_completion_result_clear(result);
	    return -1;
	}
	if (semantic.completion_count && !vr.completion_count) {
	    vr.completion_count = semantic.completion_count;
	    vr.completion_candidates = semantic.completion_candidates;
	    vr.completion_type = semantic.completion_type;
	    semantic.completion_count = 0;
	    semantic.completion_candidates = NULL;
	    vr.completion_total = semantic.completion_total;
	    vr.completion_truncated = semantic.completion_truncated;
	    vr.completion_common_prefix = semantic.completion_common_prefix;
	    semantic.completion_common_prefix = NULL;
	} else if (semantic.completion_count) {
	    ged_completion_merge(&vr, &semantic, request);
	}
	ged_cmd_validate_result_clear(&semantic);
    }
    /* Preserve provider-selected candidates.  Replacing them with generic
     * geometry here would discard command-specific filters (for example,
     * binary-only or hidden-object modes).  Only synthesize candidates when
     * a grammar supplied a geometry role without a provider or candidates. */
    if (!vr.completion_count && vr.state != BU_CMD_VALIDATE_INVALID && !vr.semantic_provider &&
	!schema_option_candidates &&
	(vr.completion_type == BU_CMD_VALUE_DB_OBJECT || vr.completion_type == BU_CMD_VALUE_DB_PATH) &&
	gedp && gedp->dbip) {
	struct bu_vls gprefix = BU_VLS_INIT_ZERO;
	struct ged_cmd_validate_result gvr = GED_CMD_VALIDATE_RESULT_NULL;
	gvr.completion_type = vr.completion_type;
	ged_fill_geometry_candidates(gedp, path_seed, &gvr, &gprefix,
	    &ged_db_geometry_policy, request);
	result->completion_candidates = gvr.completion_candidates;
	result->completion_count = gvr.completion_count;
	result->completion_type = gvr.completion_type;
	result->total_count = gvr.completion_total;
	result->truncated = gvr.completion_truncated;
	result->common_prefix = gvr.completion_common_prefix;
	gvr.completion_common_prefix = NULL;
	gvr.completion_candidates = NULL;
	gvr.completion_count = 0;
	result->prefix = bu_strdup(bu_vls_cstr(&gprefix));
	ged_cmd_validate_result_clear(&gvr);
	bu_vls_free(&gprefix);
    } else {
	result->completion_candidates = vr.completion_candidates;
	result->completion_count = vr.completion_count;
	result->total_count = vr.completion_total;
	result->truncated = vr.completion_truncated;
	result->common_prefix = vr.completion_common_prefix;
	vr.completion_common_prefix = NULL;
	vr.completion_candidates = NULL;
	vr.completion_count = 0;
	completion_prefix = (result->completion_type == BU_CMD_VALUE_DB_OBJECT ||
	    result->completion_type == BU_CMD_VALUE_DB_PATH) ? path_seed : seed;
	size_t separator = ged_last_unescaped_path_separator(completion_prefix);
	if ((result->completion_type == BU_CMD_VALUE_DB_OBJECT ||
		result->completion_type == BU_CMD_VALUE_DB_PATH) &&
		separator != std::string::npos) {
	    completion_prefix = completion_prefix.substr(separator + 1);
	} else if ((result->expected & BU_CMD_EXPECT_OPTION_ARG) &&
		parsed.cursor_value_start > parsed.cursor_content_start) {
	    size_t equal = completion_prefix.rfind('=');
	    if (equal != std::string::npos) {
		completion_prefix = completion_prefix.substr(equal + 1);
	    }
	}
	result->prefix = bu_strdup(completion_prefix.c_str());
    }

    /* Source ranges are raw input offsets, not decoded-token lengths.  Keep
     * the suffix after the cursor intact and replace only the path component
     * or attached option value represented by a completion candidate. */
    size_t separator = ged_last_unescaped_path_separator(path_seed);
    if ((result->completion_type == BU_CMD_VALUE_DB_OBJECT ||
	    result->completion_type == BU_CMD_VALUE_DB_PATH) &&
	    separator != std::string::npos) {
	result->replacement_start = parsed.cursor_component_start;
    } else if ((result->expected & BU_CMD_EXPECT_OPTION_ARG) &&
	    parsed.cursor_value_start > parsed.cursor_content_start &&
	    seed.rfind('=') != std::string::npos) {
	result->replacement_start = parsed.cursor_value_start;
    }

    /* Query providers apply scalar predicates before their materialization
     * limit.  Retain a final pass for legacy providers without a query hook;
     * they enumerate an unbounded set, so this pass is still exact. */
    if (vr.candidate_validate && result->completion_candidates &&
	    (result->expected & (BU_CMD_EXPECT_OPERAND | BU_CMD_EXPECT_OPTION_ARG))) {
	size_t kept = 0;
	size_t original_count = result->completion_count;
	struct ged_completion_filter_context final_context;
	final_context.validate = vr.candidate_validate;
	final_context.cursor_token = cursor_token;
	final_context.seed = seed;
	final_context.type = result->completion_type;
	final_context.expected = result->expected;
	for (size_t i = 0; i < result->completion_count; i++) {
	    const char *candidate = result->completion_candidates[i];
	    if (candidate && ged_completion_candidate_filter(candidate,
		    &final_context) == 0) {
		result->completion_candidates[kept++] = candidate;
	    } else if (candidate) {
		bu_free((void *)candidate, "candidate rejected by value predicate");
	    }
	}
	result->completion_count = kept;
	result->completion_candidates[kept] = NULL;
	if (kept != original_count && !result->truncated)
	    result->total_count = kept;
	if (kept != original_count && result->common_prefix) {
	    bu_free(result->common_prefix, "unfiltered completion common prefix");
	    result->common_prefix = NULL;
	}
    }

    if (!result->total_count)
	result->total_count = result->completion_count;
    /* Any source which materializes only part of its set must supply the
     * full-set common prefix.  Reconstructing it from the displayed subset
     * can insert bytes that are not shared by undisplayed candidates. */
    if ((result->truncated || result->total_count > result->completion_count) &&
	result->total_count && !result->common_prefix) {
	ged_cmd_validate_result_clear(&vr);
	ged_input_parse_free(&parsed);
	ged_cmd_completion_result_clear(result);
	return -1;
    }
    if (!result->common_prefix && result->completion_count &&
	result->completion_candidates) {
	std::string common(result->completion_candidates[0]);
	for (size_t i = 1; i < result->completion_count && !common.empty(); i++) {
	    const char *candidate = result->completion_candidates[i];
	    if (!candidate) {
		common.clear();
		break;
	    }
	    size_t j = 0;
	    size_t common_limit = std::min(common.size(), strlen(candidate));
	    while (j < common_limit && common[j] == candidate[j])
		j++;
	    common.resize(ged_utf8_prefix_boundary(common, j));
	}
	result->common_prefix = bu_strdup(common.c_str());
    }
    if (request && request->max_candidates &&
	    result->completion_count > request->max_candidates) {
	for (size_t i = request->max_candidates; i < result->completion_count; i++)
	    bu_free((void *)result->completion_candidates[i], "bounded completion candidate");
	result->completion_count = request->max_candidates;
	result->completion_candidates[result->completion_count] = NULL;
	result->truncated = 1;
    }
    if (result->total_count > result->completion_count)
	result->truncated = 1;

    ged_cmd_validate_result_clear(&vr);
    ged_input_parse_free(&parsed);

    return (int)result->completion_count;
}

extern "C" int
ged_cmd_complete_result(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_completion_result *result)
{
    return ged_cmd_complete_result_internal(gedp, input, cursor_pos, NULL, result);
}

extern "C" int
ged_cmd_complete_query(struct ged *gedp, const char *input,
	const struct ged_cmd_completion_request *request,
	struct ged_cmd_completion_result *result)
{
    if (!result)
	return -1;
    if (!input || !request ||
	    (request->flags & ~GED_CMD_COMPLETION_WANT_CONTEXT)) {
	ged_cmd_completion_result_clear(result);
	return -1;
    }
    int ret = ged_cmd_complete_result_internal(gedp, input, request->cursor_pos,
	request, result);
    return ret < 0 ? -1 : 0;
}

extern "C" void
ged_cmd_completion_result_init(struct ged_cmd_completion_result *result)
{
    if (!result)
	return;
    result->completion_count = 0;
    result->completion_candidates = NULL;
    result->replacement_start = 0;
    result->replacement_end = 0;
    result->prefix = NULL;
    result->completion_type = BU_CMD_VALUE_UNKNOWN;
    result->expected = BU_CMD_EXPECT_NONE;
    result->hint = NULL;
    result->active_command_path = NULL;
    result->total_count = 0;
    result->truncated = 0;
    result->common_prefix = NULL;
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
    if (result->common_prefix)
	bu_free(result->common_prefix, "completion common prefix");
    if (result->hint)
	bu_free(result->hint, "completion hint");

    ged_cmd_completion_result_init(result);
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
    return ged_geom_completions_filtered(completions, prefix, dbip, seed,
	GED_DB_COMPLETION_GEOMETRY);
}

static int
ged_geom_completions_query(const char ***completions, struct bu_vls *prefix,
	struct db_i *dbip, const char *seed, unsigned int filters,
	const struct ged_cmd_completion_request *request, size_t *total,
	std::string *common)
{
    int ret = 0;
    struct ged_db_completion_policy policy = {filters};

    if (!dbip || !prefix || !completions || !seed)
	return 0;
    *completions = NULL;
    if (total)
	*total = 0;
    if (common)
	common->clear();

    /* Only an unescaped slash denotes an interactive hierarchy separator.
     * Literal slash-containing names remain root-level candidates and are
     * represented with '\\/' by the database-path grammar. */
    if (ged_last_unescaped_path_separator(std::string(seed)) != std::string::npos) {
	ret = path_match(completions, prefix, dbip, seed, &policy, request, total, common);
	return ret;
    }

    ret = obj_match(completions, dbip, seed, &policy, request, total, common);
    /* If the match is from object names, the prefix is just the seed. */
    bu_vls_sprintf(prefix, "%s", seed);

    return ret;
}

int
ged_geom_completions_filtered(const char ***completions, struct bu_vls *prefix,
	struct db_i *dbip, const char *seed, unsigned int filters)
{
    return ged_geom_completions_query(completions, prefix, dbip, seed, filters,
	NULL, NULL, NULL);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
