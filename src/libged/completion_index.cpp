/*             C O M P L E T I O N _ I N D E X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */

#include "common.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "./alphanum.h"
#include "./completion_index.h"
#include "./ged_private.h"
#include "ged.h"


struct ged_completion_index_entry {
    std::string name;
    struct directory *dp = RT_DIR_NULL;
};


struct ged_completion_index {
    struct policy_view {
	std::vector<uint32_t> lexical;
	std::vector<uint32_t> natural;
	size_t rank_tree_base = 0;
	std::vector<uint32_t> rank_tree;
	uint64_t last_used = 0;
    };
    std::mutex mutex;
    struct db_i *dbip = DBI_NULL;
    bool dirty = true;
    std::vector<ged_completion_index_entry> entries;
    std::vector<uint32_t> natural;
    std::vector<uint32_t> natural_rank;
    std::unordered_map<unsigned int, policy_view> policies;
    uint64_t policy_clock = 0;
};


static constexpr unsigned int GED_COMPLETION_POLICY_MASK =
    GED_DB_COMPLETION_GEOMETRY | GED_DB_COMPLETION_NON_GEOMETRY |
    GED_DB_COMPLETION_HIDDEN | GED_DB_COMPLETION_GLOBAL |
    GED_DB_COMPLETION_BINARY_ONLY | GED_DB_COMPLETION_PRIMITIVES |
    GED_DB_COMPLETION_COMBINATIONS | GED_DB_COMPLETION_REGIONS |
    GED_DB_COMPLETION_HIDDEN_ONLY;


std::string
_ged_db_path_component_encode(const char *name)
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


bool
_ged_db_completion_allowed(const struct directory *dp, unsigned int flags)
{
    if (!dp)
	return false;
    if (BU_STR_EQUAL(dp->d_namep, DB5_GLOBAL_OBJECT_NAME))
	return (flags & GED_DB_COMPLETION_GLOBAL) != 0;
    if ((flags & GED_DB_COMPLETION_HIDDEN_ONLY) &&
	    !(dp->d_flags & RT_DIR_HIDDEN))
	return false;
    if ((dp->d_flags & RT_DIR_HIDDEN) &&
	    !(flags & (GED_DB_COMPLETION_HIDDEN | GED_DB_COMPLETION_HIDDEN_ONLY)))
	return false;
    if (flags & GED_DB_COMPLETION_BINARY_ONLY)
	return dp->d_major_type == DB5_MAJORTYPE_BINARY_UNIF;
    if (dp->d_flags & RT_DIR_NON_GEOM)
	return (flags & GED_DB_COMPLETION_NON_GEOMETRY) != 0;
    if (!(flags & GED_DB_COMPLETION_GEOMETRY))
	return false;
    unsigned int subtypes = flags & (GED_DB_COMPLETION_PRIMITIVES |
	GED_DB_COMPLETION_COMBINATIONS | GED_DB_COMPLETION_REGIONS);
    if (!subtypes)
	return true;
    return ((subtypes & GED_DB_COMPLETION_PRIMITIVES) && (dp->d_flags & RT_DIR_SOLID)) ||
	((subtypes & GED_DB_COMPLETION_COMBINATIONS) && (dp->d_flags & RT_DIR_COMB)) ||
	((subtypes & GED_DB_COMPLETION_REGIONS) && (dp->d_flags & RT_DIR_REGION));
}


static size_t
ged_index_utf8_boundary(const std::string &value, size_t boundary)
{
    boundary = std::min(boundary, value.size());
    while (boundary && boundary < value.size() &&
	    (((unsigned char)value[boundary] & 0xc0) == 0x80))
	boundary--;
    return boundary;
}


class ged_index_collector {
    public:
	explicit ged_index_collector(size_t requested_limit,
		const struct ged_cmd_completion_request *request) :
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
		size_t keep = 0;
		size_t common_limit = std::min(common.size(), candidate.size());
		while (keep < common_limit && common[keep] == candidate[keep])
		    keep++;
		common.resize(ged_index_utf8_boundary(common, keep));
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

	void finish()
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

	static bool less(const std::string &left, const std::string &right)
	{
	    int ordered = alphanum_impl(left.c_str(), right.c_str(), NULL);
	    return ordered < 0 || (!ordered && left < right);
	}
};


static void
ged_index_changed(struct db_i *UNUSED(dbip), struct directory *UNUSED(dp),
	int UNUSED(change_type), void *data)
{
    struct ged_completion_index *index = (struct ged_completion_index *)data;
    if (!index)
	return;
    std::lock_guard<std::mutex> guard(index->mutex);
    index->dirty = true;
}


extern "C" void
_ged_cmd_completion_cache_clear(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->i || !gedp->i->i->cmd_completion_cache)
	return;
    struct ged_completion_index *index =
	(struct ged_completion_index *)gedp->i->i->cmd_completion_cache;
    if (index->dbip)
	(void)db_rm_changed_clbk(index->dbip, ged_index_changed, index);
    delete index;
    gedp->i->i->cmd_completion_cache = NULL;
}


static struct ged_completion_index *
ged_index_get(struct ged *gedp)
{
    if (!gedp || !gedp->dbip || !gedp->i || !gedp->i->i)
	return NULL;
    struct ged_completion_index *index =
	(struct ged_completion_index *)gedp->i->i->cmd_completion_cache;
    if (!index) {
	index = new ged_completion_index;
	index->dbip = gedp->dbip;
	(void)db_add_changed_clbk(index->dbip, ged_index_changed, index);
	gedp->i->i->cmd_completion_cache = index;
    }
    return index->dbip == gedp->dbip ? index : NULL;
}


static struct ged_completion_index::policy_view &
ged_index_policy(struct ged_completion_index *index, unsigned int flags)
{
    flags &= GED_COMPLETION_POLICY_MASK;
    auto found = index->policies.find(flags);

    index->policy_clock++;
    if (!index->policy_clock) {
	/* Preserve a meaningful LRU order after the theoretical wrap. */
	for (auto &entry : index->policies)
	    entry.second.last_used = 0;
	index->policy_clock = 1;
    }
    if (found != index->policies.end()) {
	found->second.last_used = index->policy_clock;
	return found->second;
    }
    if (index->policies.size() >= GED_DB_COMPLETION_POLICY_CACHE_MAX) {
	auto victim = std::min_element(index->policies.begin(),
	    index->policies.end(), [](const auto &left, const auto &right) {
		return left.second.last_used < right.second.last_used;
	    });
	if (victim != index->policies.end())
	    index->policies.erase(victim);
    }
    auto &view = index->policies[flags];
    view.last_used = index->policy_clock;
    view.lexical.reserve(index->entries.size());
    for (size_t i = 0; i < index->entries.size(); i++)
	if (_ged_db_completion_allowed(index->entries[i].dp, flags))
	    view.lexical.push_back((uint32_t)i);
    return view;
}


size_t
_ged_db_completion_index_policy_count(struct ged *gedp)
{
    struct ged_completion_index *index = ged_index_get(gedp);
    if (!index)
	return 0;
    std::lock_guard<std::mutex> guard(index->mutex);
    return index->policies.size();
}


extern "C" size_t
_ged_cmd_completion_cache_policy_count(struct ged *gedp)
{
    return _ged_db_completion_index_policy_count(gedp);
}


static void
ged_index_natural_policy(struct ged_completion_index *index,
	unsigned int flags, struct ged_completion_index::policy_view &view)
{
    if (!view.natural.empty() || view.lexical.empty())
	return;
    view.natural.reserve(view.lexical.size());
    for (uint32_t i : index->natural)
	if (_ged_db_completion_allowed(index->entries[i].dp, flags))
	    view.natural.push_back(i);
}


/* Map a lexical policy range to natural-order results without scanning the
 * range.  Each segment-tree node stores the smallest global natural rank in
 * its lexical interval.  A best-first traversal of the canonical range nodes
 * therefore emits exactly the same order as a full alphanumeric sort in
 * O(log N + K log N) work for K displayed candidates. */
static void
ged_index_rank_tree(struct ged_completion_index *index,
	struct ged_completion_index::policy_view &view)
{
    if (view.rank_tree_base || view.lexical.empty())
	return;
    size_t base = 1;
    while (base < view.lexical.size())
	base <<= 1;
    view.rank_tree_base = base;
    view.rank_tree.assign(base * 2, UINT32_MAX);
    for (size_t i = 0; i < view.lexical.size(); i++)
	view.rank_tree[base + i] = index->natural_rank[view.lexical[i]];
    for (size_t i = base; i-- > 1;)
	view.rank_tree[i] = std::min(view.rank_tree[i * 2],
	    view.rank_tree[i * 2 + 1]);
}


static void
ged_index_natural_range(struct ged_completion_index *index,
	struct ged_completion_index::policy_view &view,
	size_t first, size_t last, size_t limit, std::vector<std::string> &values)
{
    ged_index_rank_tree(index, view);
    if (!view.rank_tree_base || first >= last || !limit)
	return;

    using ranked_node = std::pair<uint32_t, size_t>;
    std::priority_queue<ranked_node, std::vector<ranked_node>,
	std::greater<ranked_node>> pending;
    size_t left = first + view.rank_tree_base;
    size_t right = last + view.rank_tree_base;
    while (left < right) {
	if (left & 1) {
	    pending.emplace(view.rank_tree[left], left);
	    left++;
	}
	if (right & 1) {
	    right--;
	    pending.emplace(view.rank_tree[right], right);
	}
	left >>= 1;
	right >>= 1;
    }

    values.reserve(std::min(limit, last - first));
    while (!pending.empty() && values.size() < limit) {
	const ranked_node current = pending.top();
	pending.pop();
	size_t node = current.second;
	if (node >= view.rank_tree_base) {
	    size_t position = node - view.rank_tree_base;
	    if (position < view.lexical.size())
		values.push_back(index->entries[view.lexical[position]].name);
	    continue;
	}
	for (size_t child = node * 2; child <= node * 2 + 1; child++)
	    if (view.rank_tree[child] != UINT32_MAX)
		pending.emplace(view.rank_tree[child], child);
    }
}


static void
ged_index_refresh(struct ged_completion_index *index)
{
    if (!index || !index->dirty || !index->dbip)
	return;
    index->entries.clear();
    index->natural.clear();
    index->natural_rank.clear();
    index->policies.clear();
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, index->dbip) {
	ged_completion_index_entry entry;
	entry.name = _ged_db_path_component_encode(dp->d_namep);
	entry.dp = dp;
	index->entries.push_back(std::move(entry));
    } FOR_ALL_DIRECTORY_END;
    std::sort(index->entries.begin(), index->entries.end(),
	[](const auto &left, const auto &right) { return left.name < right.name; });
    if (index->entries.size() > UINT32_MAX) {
	index->entries.clear();
	index->natural_rank.clear();
	index->dirty = false;
	return;
    }
    index->natural.resize(index->entries.size());
    for (size_t i = 0; i < index->entries.size(); i++)
	index->natural[i] = (uint32_t)i;
    std::sort(index->natural.begin(), index->natural.end(),
	[index](uint32_t left, uint32_t right) {
	    int ordered = alphanum_impl(index->entries[left].name.c_str(),
		index->entries[right].name.c_str(), NULL);
	    return ordered < 0 || (!ordered &&
		index->entries[left].name < index->entries[right].name);
	});
    index->natural_rank.resize(index->entries.size());
    for (size_t rank = 0; rank < index->natural.size(); rank++)
	index->natural_rank[index->natural[rank]] = (uint32_t)rank;
    /* Policy membership, natural subsets, and range indexes are all lazy.
     * Building every supported view here multiplied cold-query work and
     * retained memory by the number of completion policies even when a
     * session used only ordinary geometry completion. */
    index->dirty = false;
}


static std::string
ged_index_prefix_upper(std::string prefix)
{
    while (!prefix.empty() && (unsigned char)prefix.back() == 0xff)
	prefix.pop_back();
    if (!prefix.empty())
	prefix.back() = (char)((unsigned char)prefix.back() + 1);
    return prefix;
}


int
_ged_db_completion_index_query(struct ged *gedp, const std::string &seed,
	unsigned int policy_flags,
	const struct ged_cmd_completion_request *request,
	struct ged_db_completion_index_result *result)
{
    struct ged_completion_index *index = ged_index_get(gedp);
    if (!index || !result)
	return -1;
    policy_flags &= GED_COMPLETION_POLICY_MASK;
    std::lock_guard<std::mutex> guard(index->mutex);
    ged_index_refresh(index);
    if (index->entries.empty() && db_directory_size(index->dbip) > 0)
	return -1;
    auto &view = ged_index_policy(index, policy_flags);
    auto lexical_less = [index](uint32_t entry, const std::string &value) {
	return index->entries[entry].name < value;
    };
    auto first = seed.empty() ? view.lexical.begin() :
	std::lower_bound(view.lexical.begin(), view.lexical.end(), seed, lexical_less);
    auto last = view.lexical.end();
    if (!seed.empty()) {
	std::string upper = ged_index_prefix_upper(seed);
	if (!upper.empty())
	    last = std::lower_bound(first, view.lexical.end(), upper, lexical_less);
    }

    *result = ged_db_completion_index_result();
    size_t limit = request ? request->max_candidates : 0;
    if ((!request || !request->candidate_filter) && seed.empty()) {
	ged_index_natural_policy(index, policy_flags, view);
	result->total = view.natural.size();
	if (!view.lexical.empty()) {
	    result->common = index->entries[view.lexical.front()].name;
	    const std::string &final = index->entries[view.lexical.back()].name;
	    size_t keep = 0;
	    size_t common_limit = std::min(result->common.size(), final.size());
	    while (keep < common_limit && result->common[keep] == final[keep])
		keep++;
	    result->common.resize(ged_index_utf8_boundary(result->common, keep));
	}
	size_t count = limit ? std::min(limit, view.natural.size()) : view.natural.size();
	result->values.reserve(count);
	for (size_t i = 0; i < count; i++)
	    result->values.push_back(index->entries[view.natural[i]].name);
	return 0;
    }

    size_t range_count = (size_t)std::distance(first, last);
    if (request && !request->candidate_filter && !seed.empty() && limit &&
	    range_count) {
	result->total = range_count;
	result->common = index->entries[*first].name;
	const std::string &final = index->entries[*(last - 1)].name;
	size_t keep = 0;
	size_t common_limit = std::min(result->common.size(), final.size());
	while (keep < common_limit && result->common[keep] == final[keep])
	    keep++;
	result->common.resize(ged_index_utf8_boundary(result->common, keep));
	ged_index_natural_range(index, view,
	    (size_t)std::distance(view.lexical.begin(), first),
	    (size_t)std::distance(view.lexical.begin(), last), limit,
	    result->values);
	return 0;
    }

    ged_index_collector matches(limit, request);
    for (auto entry = first; entry != last; ++entry)
	matches.add(index->entries[*entry].name);
    matches.finish();
    result->total = matches.total;
    result->common = std::move(matches.common);
    result->values = std::move(matches.values);
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
