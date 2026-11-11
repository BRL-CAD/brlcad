/*                       D B _ I N D E X . C P P
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 */
/** @file libged/db_index.cpp
 *
 * GED database-index service.
 *
 * The `.g` database is authoritative.  This service owns a derived hierarchy
 * index used by libged, libqtcad, drawing, selection, and editing callers for
 * explicit object, child, and path queries.
 */

#include "common.h"

#include "bu/str.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "bu/hash.h"
#include "ged/event.h"
#include "raytrace.h"
#include "ged/db_index.h"

#include "./ged_private.h"


struct ged_db_index_record_native {
    ged_db_index_id id = 0;
    ged_db_index_id object_id = 0;
    std::string name;
    struct directory *dp = RT_DIR_NULL;
    int valid = 0;
    int is_instance = 0;
    int is_comb = 0;
};


struct ged_db_index_child_native {
    ged_db_index_id id = 0;
    int bool_op = OP_UNION;
    size_t row = 0;
    bool matrix_valid = false;
    mat_t matrix = MAT_INIT_IDN;
};


struct ged_db_index_use_native {
    ged_db_index_id parent_id = 0;
    ged_db_index_id child_id = 0;
    int bool_op = OP_UNION;
    size_t child_row = 0;
};


struct ged_db_index_tree_leaf {
    std::string name;
    int bool_op = OP_UNION;
    bool matrix_valid = false;
    mat_t matrix = MAT_INIT_IDN;
};


struct ged_db_index {
    struct ged *gedp = nullptr;
    struct db_i *dbip = nullptr;
    ged_event_observer_token event_observer = 0;
    uint64_t revision = 0;
    /* Change notification and index dirtiness are deliberately separate.
     * Clients consume pending_flags to update their own derived state, while
     * ordinary queries need only reconcile the database snapshot once. */
    int needs_rebuild = 1;
    unsigned long long pending_flags = 0;
    std::unordered_map<ged_db_index_id, ged_db_index_record_native> records;
    std::unordered_map<ged_db_index_id, std::vector<ged_db_index_child_native>> children;
    std::unordered_map<ged_db_index_id,
	std::unordered_map<std::string, ged_db_index_id>> child_names;
    std::unordered_map<ged_db_index_id, std::vector<ged_db_index_use_native>> parents;
    std::unordered_map<std::string, ged_db_index_id> names;
    std::unordered_map<ged_db_index_id, std::string> removed_names;
    bool tops_cache_valid = false;
    std::vector<ged_db_index_id> standard_tops_cache;
    std::vector<ged_db_index_id> cyclic_tops_cache;
};


static ged_db_index_id
ged_db_index_name_hash(const char *name)
{
    if (!name)
	return 0;
    return bu_data_hash(name, strlen(name) * sizeof(char));
}


static ged_db_index_id
ged_db_index_string_hash(const std::string &name)
{
    if (name.empty())
	return 0;
    return bu_data_hash(name.c_str(), name.size() * sizeof(char));
}


static ged_db_index_id
ged_db_index_path_hash_ids(const ged_db_index_id *path,
			   size_t path_len,
			   size_t max_len)
{
    if (!path || !path_len)
	return 0;

    size_t hlen = max_len ? max_len : path_len;
    if (!hlen || hlen > path_len)
	return 0;

    return bu_data_hash(path, hlen * sizeof(ged_db_index_id));
}


static int
ged_db_index_public_bool_op(int bool_op)
{
    if (bool_op == OP_SUBTRACT)
	return DB_OP_SUBTRACT;
    if (bool_op == OP_INTERSECT)
	return DB_OP_INTERSECT;
    return DB_OP_UNION;
}


static void
ged_db_index_record_clear(struct ged_db_index_record *record)
{
    if (!record)
	return;
    record->id = 0;
    record->object_id = 0;
    record->name = nullptr;
    record->dp = nullptr;
    record->valid = 0;
    record->is_instance = 0;
    record->is_comb = 0;
    record->child_count = 0;
}


static void
ged_db_index_child_clear(struct ged_db_index_child *child)
{
    if (!child)
	return;
    ged_db_index_record_clear(&child->record);
    child->bool_op = DB_OP_UNION;
    child->row = 0;
    child->matrix_valid = 0;
    MAT_IDN(child->matrix);
}


static void
ged_db_index_use_clear(struct ged_db_index_use *use)
{
    if (!use)
	return;
    ged_db_index_record_clear(&use->parent);
    ged_db_index_record_clear(&use->child);
    use->bool_op = DB_OP_UNION;
    use->child_row = 0;
}


static void
ged_db_index_record_fill(struct ged_db_index *index,
			 const ged_db_index_record_native &native,
			 struct ged_db_index_record *record)
{
    ged_db_index_record_clear(record);
    record->id = native.id;
    record->object_id = native.object_id;
    record->name = native.name.c_str();
    record->dp = native.dp;
    record->valid = native.valid;
    record->is_instance = native.is_instance;
    record->is_comb = native.is_comb;

    auto child_it = index->children.find(native.object_id);
    if (child_it != index->children.end())
	record->child_count = child_it->second.size();
}


static int
ged_db_index_record_find(struct ged_db_index *index,
			 ged_db_index_id id,
			 struct ged_db_index_record *record)
{
    if (index && id && record) {
	auto record_it = index->records.find(id);
	if (record_it != index->records.end()) {
	    ged_db_index_record_fill(index, record_it->second, record);
	    return record->valid ? 1 : 0;
	}
    }

    ged_db_index_record_clear(record);
    return 0;
}


static void
ged_db_index_record_put(struct ged_db_index *index,
			ged_db_index_id id,
			ged_db_index_id object_id,
			const std::string &name,
			struct directory *dp,
			int is_instance)
{
    if (!index || !id || name.empty())
	return;

    ged_db_index_record_native &record = index->records[id];
    record.id = id;
    record.object_id = object_id ? object_id : id;
    record.name = name;
    record.dp = dp;
    record.valid = 1;
    record.is_instance = is_instance ? 1 : 0;
    record.is_comb = (dp && (dp->d_flags & RT_DIR_COMB)) ? 1 : 0;
    index->names[record.name] = id;
}


static void
ged_db_index_record_remove(struct ged_db_index *index, ged_db_index_id id)
{
    if (!index || !id)
	return;

    auto record_it = index->records.find(id);
    if (record_it != index->records.end() && !record_it->second.name.empty())
	index->names.erase(record_it->second.name);
    index->records.erase(id);
    index->children.erase(id);
    index->child_names.erase(id);
}


static ged_db_index_id
ged_db_index_canonical_object_id(struct ged_db_index *index,
				 ged_db_index_id id)
{
    if (!index || !id)
	return 0;

    auto record_it = index->records.find(id);
    if (record_it == index->records.end())
	return id;

    return record_it->second.object_id;
}


static struct ged_db_index *
ged_db_index_state(struct ged *gedp)
{
    return (gedp && gedp->i) ? gedp->i->ged_db_indexp : nullptr;
}


static void
ged_db_index_collect_leaves(union tree *tp,
			    int bool_op,
			    std::vector<ged_db_index_tree_leaf> &leaves)
{
    if (!tp)
	return;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_DB_LEAF:
	    if (tp->tr_l.tl_name) {
		ged_db_index_tree_leaf leaf;
		leaf.name = tp->tr_l.tl_name;
		leaf.bool_op = bool_op;
		if (tp->tr_l.tl_mat) {
		    leaf.matrix_valid = true;
		    MAT_COPY(leaf.matrix, tp->tr_l.tl_mat);
		}
		leaves.push_back(leaf);
	    }
	    return;
	case OP_UNION:
	    ged_db_index_collect_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    ged_db_index_collect_leaves(tp->tr_b.tb_right, OP_UNION, leaves);
	    return;
	case OP_INTERSECT:
	    ged_db_index_collect_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    ged_db_index_collect_leaves(tp->tr_b.tb_right, OP_INTERSECT, leaves);
	    return;
	case OP_SUBTRACT:
	    ged_db_index_collect_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    ged_db_index_collect_leaves(tp->tr_b.tb_right, OP_SUBTRACT, leaves);
	    return;
	case OP_XOR:
	    ged_db_index_collect_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    ged_db_index_collect_leaves(tp->tr_b.tb_right, bool_op, leaves);
	    return;
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    ged_db_index_collect_leaves(tp->tr_b.tb_left, OP_UNION, leaves);
	    return;
	default:
	    bu_log("ged_db_index: skipping unsupported tree operator %d\n", tp->tr_op);
	    return;
    }
}


static void
ged_db_index_add_object(struct ged_db_index *index, struct directory *dp)
{
    if (!index || !dp || !dp->d_namep)
	return;
    if (dp->d_flags & DB_LS_HIDDEN)
	return;

    ged_db_index_id id = ged_db_index_name_hash(dp->d_namep);
    ged_db_index_record_put(index, id, id, std::string(dp->d_namep), dp, 0);
}


static void
ged_db_index_add_child(struct ged_db_index *index,
		       ged_db_index_id parent_id,
		       const std::string &child_name,
		       int bool_op,
		       unsigned long long instance_count,
		       bool matrix_valid,
		       const mat_t matrix,
		       struct directory *known_child = nullptr)
{
    if (!index || !index->gedp || !index->gedp->dbip || !parent_id ||
	child_name.empty())
	return;

    ged_db_index_id object_id = ged_db_index_string_hash(child_name);
    if (!object_id)
	return;

    std::string instance_name = child_name;
    int is_instance = 0;
    if (instance_count > 1) {
	instance_name = child_name + "@" + std::to_string(instance_count - 1);
	is_instance = 1;
    }

    ged_db_index_id child_id = is_instance ?
			       ged_db_index_string_hash(instance_name) : object_id;
    /*
     * Rebuild's first pass has already indexed every visible directory by
     * its exact name.  Resolving a child through db_lookup() here needlessly
     * walks librt's directory hash for every combination leaf (and accounted
     * for a measurable part of large-scene GUI startup).  Use the exact-name
     * index to avoid both that repeated lookup and any ambiguity from an ID
     * hash collision.  A missing or hidden reference remains a valid index
     * record with no live directory, matching the service's visibility
     * contract.
     */
    struct directory *child_dp = known_child;
    if (!child_dp) {
	auto name_it = index->names.find(child_name);
	if (name_it != index->names.end()) {
	    auto record_it = index->records.find(name_it->second);
	    if (record_it != index->records.end() &&
		record_it->second.name == child_name)
		child_dp = record_it->second.dp;
	}
    }

    if (is_instance || index->records.find(child_id) == index->records.end())
	ged_db_index_record_put(index, child_id, object_id, instance_name,
				child_dp, is_instance);
    else if (!child_dp) {
	ged_db_index_record_native &record = index->records[child_id];
	if (!record.dp)
	    record.name = instance_name;
    }

    std::vector<ged_db_index_child_native> &children = index->children[parent_id];
    ged_db_index_child_native child;
    child.id = child_id;
    child.bool_op = bool_op;
    child.row = children.size();
    child.matrix_valid = matrix_valid;
    if (matrix_valid && matrix)
	MAT_COPY(child.matrix, matrix);
    children.push_back(child);
    index->child_names[parent_id][instance_name] = child_id;

    ged_db_index_use_native use;
    use.parent_id = parent_id;
    use.child_id = child_id;
    use.bool_op = bool_op;
    use.child_row = child.row;
    index->parents[object_id].push_back(use);
}


static void
ged_db_index_add_comb_children(struct ged_db_index *index, struct directory *dp)
{
    if (!index || !index->gedp || !index->gedp->dbip || !dp || !dp->d_namep)
	return;
    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, index->gedp->dbip, NULL) < 0)
	return;

    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (!comb || !comb->tree) {
	rt_db_free_internal(&intern);
	return;
    }

    std::vector<ged_db_index_tree_leaf> leaves;
    ged_db_index_collect_leaves(comb->tree, OP_UNION, leaves);

    ged_db_index_id parent_id = ged_db_index_name_hash(dp->d_namep);
    std::unordered_map<ged_db_index_id, unsigned long long> instance_counts;
    for (const ged_db_index_tree_leaf &leaf : leaves) {
	ged_db_index_id child_object_id = ged_db_index_string_hash(leaf.name);
	instance_counts[child_object_id]++;
	ged_db_index_add_child(index, parent_id, leaf.name, leaf.bool_op,
			       instance_counts[child_object_id],
			       leaf.matrix_valid, leaf.matrix);
    }

    rt_db_free_internal(&intern);
}


static void
ged_db_index_add_removed_names(struct ged_db_index *index)
{
    if (!index)
	return;

    for (const auto &entry : index->removed_names) {
	if (index->records.find(entry.first) != index->records.end())
	    continue;
	ged_db_index_record_put(index, entry.first, entry.first, entry.second,
				nullptr, 0);
    }
}


static void
ged_db_index_clear_snapshot(struct ged_db_index *index)
{
    index->records.clear();
    index->children.clear();
    index->child_names.clear();
    index->parents.clear();
    index->names.clear();
    index->tops_cache_valid = false;
    index->standard_tops_cache.clear();
    index->cyclic_tops_cache.clear();
}


void
ged_db_index_database_set_private(struct ged *gedp, struct db_i *dbip)
{
    struct ged_db_index *index = ged_db_index_state(gedp);
    if (!index || index->dbip == dbip)
	return;

    ged_db_index_clear_snapshot(index);
    index->removed_names.clear();
    index->dbip = dbip;
    index->needs_rebuild = 1;
    index->pending_flags = 0;
}


static int
ged_db_index_rebuild(struct ged_db_index *index)
{
    if (!index || !index->gedp || !index->gedp->dbip)
	return 0;

    ged_db_index_clear_snapshot(index);

    struct directory *dp = RT_DIR_NULL;
    FOR_ALL_DIRECTORY_START(dp, index->gedp->dbip) {
	ged_db_index_add_object(index, dp);
    }
    FOR_ALL_DIRECTORY_END;

    FOR_ALL_DIRECTORY_START(dp, index->gedp->dbip) {
	if (dp && !(dp->d_flags & DB_LS_HIDDEN))
	    ged_db_index_add_comb_children(index, dp);
    }
    FOR_ALL_DIRECTORY_END;

    ged_db_index_add_removed_names(index);
    index->revision++;
    index->needs_rebuild = 0;
    return 1;
}


static struct ged_db_index *
ged_db_index_ready(struct ged *gedp)
{
    struct ged_db_index *index = ged_db_index_state(gedp);
    if (!index || !gedp->dbip)
	return nullptr;

    ged_db_index_database_set_private(gedp, gedp->dbip);
    if (index->needs_rebuild && !ged_db_index_rebuild(index))
	return nullptr;
    return index;
}


static std::vector<std::vector<ged_db_index_id>>
ged_db_index_affected_paths(struct ged_db_index *index,
			    ged_db_index_id object_id,
			    size_t max_depth);

static void
ged_db_index_cycle_dfs(struct ged_db_index *index,
		       ged_db_index_id id,
		       std::unordered_map<ged_db_index_id, int> &color,
		       std::vector<ged_db_index_id> &stack,
		       std::unordered_set<ged_db_index_id> &cyclic)
{
    color[id] = 1;
    stack.push_back(id);

    auto child_it = index->children.find(id);
    if (child_it != index->children.end()) {
	for (const ged_db_index_child_native &child : child_it->second) {
	    auto record_it = index->records.find(child.id);
	    if (record_it == index->records.end() || !record_it->second.dp)
		continue;
	    ged_db_index_id child_object_id = record_it->second.object_id;
	    if (index->children.find(child_object_id) == index->children.end())
		continue;

	    int child_color = color[child_object_id];
	    if (child_color == 0) {
		ged_db_index_cycle_dfs(index, child_object_id, color, stack,
				       cyclic);
	    } else if (child_color == 1) {
		auto cycle_start =
		    std::find(stack.begin(), stack.end(), child_object_id);
		for (auto it = cycle_start; it != stack.end(); ++it)
		    cyclic.insert(*it);
	    }
	}
    }

    stack.pop_back();
    color[id] = 2;
}


static std::unordered_set<ged_db_index_id>
ged_db_index_cyclic_objects(struct ged_db_index *index)
{
    std::unordered_set<ged_db_index_id> cyclic;
    if (!index)
	return cyclic;

    std::unordered_map<ged_db_index_id, int> color;
    std::vector<ged_db_index_id> stack;
    for (const auto &entry : index->children) {
	if (color[entry.first] == 0)
	    ged_db_index_cycle_dfs(index, entry.first, color, stack, cyclic);
    }

    return cyclic;
}


static std::vector<ged_db_index_id>
ged_db_index_standard_tops(struct ged_db_index *index)
{
    std::vector<ged_db_index_id> tops;
    if (!index || !index->gedp || !index->gedp->dbip)
	return tops;

    // Native reference counts include sketch and data dependencies that
    // do not appear as combination children.  Cache tops for this revision.
    db_update_nref(index->gedp->dbip);
    std::vector<std::pair<std::string, ged_db_index_id>> named_tops;
    named_tops.reserve(index->records.size());
    for (const auto &entry : index->records) {
	const ged_db_index_record_native &record = entry.second;
	if (!record.valid || !record.dp || record.is_instance ||
	    record.id != record.object_id || record.dp->d_nref)
	    continue;
	named_tops.emplace_back(record.name, record.id);
    }
    std::sort(named_tops.begin(), named_tops.end(),
	[](const auto &left, const auto &right) {
	    return left.first < right.first;
	});
    tops.reserve(named_tops.size());
    for (const auto &entry : named_tops)
	tops.push_back(entry.second);

    return tops;
}


static void
ged_db_index_path_split(std::vector<std::string> &objs, const char *str)
{
    if (!str)
	return;

    std::string s(str);
    while (!s.empty() && s[0] == '/')
	s.erase(0, 1);

    std::string nstr;
    bool escaped = false;
    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if (escaped) {
		nstr.push_back(s[i]);
		escaped = false;
		continue;
	    }
	    escaped = true;
	    continue;
	}
	if (s[i] == '/' && !escaped) {
	    if (!nstr.empty())
		objs.push_back(nstr);
	    nstr.clear();
	    continue;
	}
	nstr.push_back(s[i]);
	escaped = false;
    }
    if (!nstr.empty())
	objs.push_back(nstr);
}


static std::string
ged_db_index_name_deescape(const std::string &name)
{
    std::string nstr;
    for (size_t i = 0; i < name.length(); i++) {
	if (name[i] == '\\') {
	    if ((i + 1) < name.length())
		nstr.push_back(name[i + 1]);
	    i++;
	} else {
	    nstr.push_back(name[i]);
	}
    }

    return nstr;
}


static size_t
ged_db_index_path_elements(std::vector<std::string> &elements, const char *path)
{
    std::vector<std::string> substrs;
    ged_db_index_path_split(substrs, path);
    for (const std::string &substr : substrs)
	elements.push_back(ged_db_index_name_deescape(substr));
    return elements.size();
}


static bool
ged_db_index_path_cyclic(const std::vector<ged_db_index_id> &path)
{
    std::unordered_set<ged_db_index_id> seen;
    for (ged_db_index_id id : path) {
	if (!seen.insert(id).second)
	    return true;
    }
    return false;
}


static size_t
ged_db_index_path_resolve_native(struct ged_db_index *index,
				 const char *path,
				 ged_db_index_id *ids,
				 size_t capacity);


static void
ged_db_index_collect_affected_paths(struct ged_db_index *index,
				    ged_db_index_id object_id,
				    size_t max_depth,
				    std::vector<ged_db_index_id> &stack,
				    std::vector<std::vector<ged_db_index_id>> &paths)
{
    if (!index || !object_id)
	return;

    object_id = ged_db_index_canonical_object_id(index, object_id);
    if (!object_id)
	return;

    if (std::find(stack.begin(), stack.end(), object_id) != stack.end())
	return;

    auto use_it = index->parents.find(object_id);
    if (use_it == index->parents.end() || use_it->second.empty()) {
	if ((!max_depth || max_depth >= 1) &&
	    index->records.find(object_id) != index->records.end()) {
	    paths.push_back({object_id});
	}
	return;
    }

    stack.push_back(object_id);
    for (const ged_db_index_use_native &use : use_it->second) {
	std::vector<std::vector<ged_db_index_id>> parent_paths;
	ged_db_index_collect_affected_paths(index, use.parent_id, max_depth,
					    stack, parent_paths);
	for (std::vector<ged_db_index_id> &path : parent_paths) {
	    if (max_depth && path.size() + 1 > max_depth)
		continue;
	    path.push_back(use.child_id);
	    paths.push_back(path);
	}
    }
    stack.pop_back();
}


static std::vector<std::vector<ged_db_index_id>>
ged_db_index_affected_paths(struct ged_db_index *index,
			    ged_db_index_id object_id,
			    size_t max_depth)
{
    std::vector<std::vector<ged_db_index_id>> paths;
    std::vector<ged_db_index_id> stack;
    ged_db_index_collect_affected_paths(index, object_id, max_depth, stack,
					paths);
    return paths;
}


static void
ged_db_index_event_callback(struct ged *gedp,
	const struct ged_event *events, size_t event_count,
	const struct ged_event_result *UNUSED(result), void *client_data)
{
    struct ged_db_index *index = static_cast<struct ged_db_index *>(client_data);
    for (size_t i = 0; i < event_count; ++i) {
	const struct ged_event &event = events[i];
	switch (event.kind) {
	    case GED_EVENT_OBJECT_REMOVED:
		(void)ged_db_index_note_object_name_change(gedp, event.name,
		    GED_DB_INDEX_OBJECT_REMOVED);
		break;
	    case GED_EVENT_OBJECT_RENAMED:
		(void)ged_db_index_note_object_name_change(gedp, event.name,
		    GED_DB_INDEX_OBJECT_REMOVED);
		(void)ged_db_index_note_object_name_change(gedp, event.new_name,
		    GED_DB_INDEX_OBJECT_ADDED);
		break;
	    case GED_EVENT_OBJECT_ADDED:
		(void)ged_db_index_note_object_name_change(gedp, event.name,
		    GED_DB_INDEX_OBJECT_ADDED);
		break;
	    case GED_EVENT_COMB_INSTANCE_REMOVED:
	    case GED_EVENT_OBJECT_REFERENCES_REMOVED:
		(void)ged_db_index_note_object_name_change(gedp,
		    event.parent_name, GED_DB_INDEX_OBJECT_CHANGED);
		break;
	    case GED_EVENT_NONE:
		break;
	    case GED_EVENT_BATCH_REBUILD:
	    case GED_EVENT_DATABASE_METADATA_CHANGED:
		index->needs_rebuild = 1;
		index->pending_flags |= GED_DB_INDEX_REFRESH_DB_CHANGE;
		break;
	    default:
		if (!ged_db_index_note_object_name_change(gedp, event.name,
		    GED_DB_INDEX_OBJECT_CHANGED)) {
		    index->needs_rebuild = 1;
		    index->pending_flags |= GED_DB_INDEX_REFRESH_DB_CHANGE;
		}
		break;
	}
    }
}


struct ged_db_index *
ged_db_index_create(struct ged *gedp)
{
    struct ged_db_index *index = new ged_db_index;
    index->gedp = gedp;
    index->event_observer = ged_event_observer_add(gedp,
	GED_EVENT_OBSERVER_INTERNAL, ged_db_index_event_callback, index);
    return index;
}


void
ged_db_index_destroy(struct ged_db_index *index)
{
    if (index && index->gedp && index->event_observer)
	ged_event_observer_remove(index->gedp, index->event_observer);
    delete index;
}


int
ged_db_index_available(struct ged *gedp)
{
    return (gedp && gedp->i && gedp->i->ged_db_indexp && gedp->dbip) ? 1 : 0;
}


unsigned long long
ged_db_index_refresh(struct ged *gedp)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index)
	return 0;

    index->pending_flags = 0;
    return index->revision;
}


unsigned long long
ged_db_index_refresh_flags(struct ged *gedp)
{
    struct ged_db_index *index = ged_db_index_state(gedp);
    if (!index || !index->pending_flags || !ged_db_index_ready(gedp))
	return 0;

    const unsigned long long flags = index->pending_flags;
    index->pending_flags = 0;
    return flags;
}


int
ged_db_index_note_object_change(struct ged *gedp,
				struct directory *dp,
				int change_kind)
{
    if (!gedp || !dp || !dp->d_namep)
	return 0;

    if (change_kind != GED_DB_INDEX_OBJECT_CHANGED &&
	change_kind != GED_DB_INDEX_OBJECT_ADDED &&
	change_kind != GED_DB_INDEX_OBJECT_REMOVED)
	return 0;

    struct ged_db_index *index = ged_db_index_state(gedp);
    if (!index)
	return 0;

    index->needs_rebuild = 1;
    index->pending_flags |= GED_DB_INDEX_REFRESH_DB_CHANGE;

    if (change_kind == GED_DB_INDEX_OBJECT_REMOVED) {
	ged_db_index_id id = ged_db_index_name_hash(dp->d_namep);
	if (!id)
	    return 0;
	index->removed_names[id] = std::string(dp->d_namep);
	ged_db_index_record_remove(index, id);
    }

    return 1;
}


int
ged_db_index_note_object_name_change(struct ged *gedp,
				     const char *name,
				     int change_kind)
{
    if (!gedp || !name || !name[0])
	return 0;

    if (change_kind != GED_DB_INDEX_OBJECT_CHANGED &&
	change_kind != GED_DB_INDEX_OBJECT_ADDED &&
	change_kind != GED_DB_INDEX_OBJECT_REMOVED)
	return 0;

    struct ged_db_index *index = ged_db_index_state(gedp);
    if (!index)
	return 0;

    if (change_kind == GED_DB_INDEX_OBJECT_REMOVED) {
	ged_db_index_id id = ged_db_index_name_hash(name);
	if (!id)
	    return 0;
	index->removed_names[id] = std::string(name);
	index->needs_rebuild = 1;
	index->pending_flags |= GED_DB_INDEX_REFRESH_DB_CHANGE;
	ged_db_index_record_remove(index, id);
	return 1;
    }

    if (!gedp->dbip)
	return 0;

    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return 0;

    return ged_db_index_note_object_change(gedp, dp, change_kind);
}


int
ged_db_index_record_get(struct ged *gedp, ged_db_index_id id,
			struct ged_db_index_record *record)
{
    return ged_db_index_record_find(ged_db_index_ready(gedp), id, record);
}


size_t
ged_db_index_tops(struct ged *gedp,
		  int include_cyclic,
		  ged_db_index_id *ids,
		  size_t capacity)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index)
	return 0;

    if (!index->tops_cache_valid) {
	index->standard_tops_cache = ged_db_index_standard_tops(index);
	index->cyclic_tops_cache = index->standard_tops_cache;
	std::unordered_set<ged_db_index_id> top_set(
	    index->standard_tops_cache.begin(),
	    index->standard_tops_cache.end());
	std::unordered_set<ged_db_index_id> cyclic =
	    ged_db_index_cyclic_objects(index);
	for (ged_db_index_id id : cyclic) {
	    if (top_set.insert(id).second)
		index->cyclic_tops_cache.push_back(id);
	}
	index->tops_cache_valid = true;
    }

    const std::vector<ged_db_index_id> &tops = include_cyclic ?
	index->cyclic_tops_cache : index->standard_tops_cache;

    size_t ncopy = std::min(capacity, tops.size());
    if (ids && ncopy)
	std::copy(tops.begin(), tops.begin() + ncopy, ids);
    return tops.size();
}


size_t
ged_db_index_child_count(struct ged *gedp, ged_db_index_id parent_id)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !parent_id)
	return 0;

    parent_id = ged_db_index_canonical_object_id(index, parent_id);
    auto child_it = index->children.find(parent_id);
    return (child_it != index->children.end()) ? child_it->second.size() : 0;
}


int
ged_db_index_child_at(struct ged *gedp,
		      ged_db_index_id parent_id,
		      size_t row,
		      struct ged_db_index_child *child)
{
    ged_db_index_child_clear(child);
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !parent_id || !child)
	return 0;

    parent_id = ged_db_index_canonical_object_id(index, parent_id);
    auto child_it = index->children.find(parent_id);
    if (child_it == index->children.end() || row >= child_it->second.size())
	return 0;

    const ged_db_index_child_native &native_child = child_it->second[row];
    child->row = row;
    child->bool_op = ged_db_index_public_bool_op(native_child.bool_op);
    child->matrix_valid = native_child.matrix_valid ? 1 : 0;
    MAT_COPY(child->matrix, native_child.matrix);
    return ged_db_index_record_find(index, native_child.id, &child->record);
}


size_t
ged_db_index_object_use_count(struct ged *gedp, ged_db_index_id object_id)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !object_id)
	return 0;

    object_id = ged_db_index_canonical_object_id(index, object_id);
    auto use_it = index->parents.find(object_id);
    return (use_it != index->parents.end()) ? use_it->second.size() : 0;
}


int
ged_db_index_object_use_at(struct ged *gedp,
			   ged_db_index_id object_id,
			   size_t row,
			   struct ged_db_index_use *use)
{
    ged_db_index_use_clear(use);
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !object_id || !use)
	return 0;

    object_id = ged_db_index_canonical_object_id(index, object_id);
    auto use_it = index->parents.find(object_id);
    if (use_it == index->parents.end() || row >= use_it->second.size())
	return 0;

    const ged_db_index_use_native &native_use = use_it->second[row];
    if (!ged_db_index_record_find(index, native_use.parent_id, &use->parent))
	return 0;
    if (!ged_db_index_record_find(index, native_use.child_id, &use->child)) {
	ged_db_index_use_clear(use);
	return 0;
    }
    use->bool_op = ged_db_index_public_bool_op(native_use.bool_op);
    use->child_row = native_use.child_row;
    return 1;
}


size_t
ged_db_index_affected_path_count(struct ged *gedp,
				 ged_db_index_id object_id,
				 size_t max_depth)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !object_id)
	return 0;

    return ged_db_index_affected_paths(index, object_id, max_depth).size();
}


size_t
ged_db_index_affected_path_at(struct ged *gedp,
			      ged_db_index_id object_id,
			      size_t row,
			      ged_db_index_id *ids,
			      size_t capacity,
			      size_t max_depth)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !object_id)
	return 0;

    std::vector<std::vector<ged_db_index_id>> paths =
	ged_db_index_affected_paths(index, object_id, max_depth);
    if (row >= paths.size())
	return 0;

    const std::vector<ged_db_index_id> &path = paths[row];
    size_t ncopy = std::min(capacity, path.size());
    if (ids && ncopy)
	std::copy(path.begin(), path.begin() + ncopy, ids);
    return path.size();
}


int
ged_db_index_valid_id(struct ged *gedp, ged_db_index_id id)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !id)
	return 0;
    return index->records.find(id) != index->records.end();
}


int
ged_db_index_path_exists(struct ged *gedp, const char *path)
{
    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index || !path || !path[0])
	return 0;

    return ged_db_index_path_resolve_native(index, path, NULL, 0) > 0;
}


static size_t
ged_db_index_path_resolve_native(struct ged_db_index *index,
				 const char *path,
				 ged_db_index_id *ids,
				 size_t capacity)
{
    if (!path || !path[0])
	return 0;
    if (!index)
	return 0;

    std::vector<std::string> elements;
    if (!ged_db_index_path_elements(elements, path))
	return 0;

    std::vector<ged_db_index_id> path_vec;
    auto root_it = index->names.find(elements[0]);
    if (root_it == index->names.end())
	return 0;
    path_vec.push_back(root_it->second);

    for (size_t i = 1; i < elements.size(); i++) {
	struct ged_db_index_record parent;
	if (!ged_db_index_record_find(index, path_vec.back(), &parent))
	    return 0;

	auto child_names_it = index->child_names.find(parent.object_id);
	if (child_names_it == index->child_names.end())
	    return 0;

	auto child_it = child_names_it->second.find(elements[i]);
	if (child_it == child_names_it->second.end())
	    return 0;
	path_vec.push_back(child_it->second);
    }

    if (ged_db_index_path_cyclic(path_vec))
	return 0;

    size_t ncopy = std::min(capacity, path_vec.size());
    if (ids && ncopy)
	std::copy(path_vec.begin(), path_vec.begin() + ncopy, ids);

    return path_vec.size();
}


size_t
ged_db_index_path_resolve(struct ged *gedp,
			  const char *path,
			  ged_db_index_id *ids,
			  size_t capacity)
{
    return ged_db_index_path_resolve_native(ged_db_index_ready(gedp),
	path, ids, capacity);
}


int
ged_db_index_path_print(struct ged *gedp,
			struct bu_vls *out,
			const ged_db_index_id *path,
			size_t path_len,
			size_t max_len,
			int verbose)
{
    if (!out || !path || !path_len)
	return 0;

    struct ged_db_index *index = ged_db_index_ready(gedp);
    if (!index)
	return 0;

    size_t plen = max_len ? max_len : path_len;
    if (!plen)
	return 0;
    if (plen > path_len)
	plen = path_len;

    bu_vls_trunc(out, 0);
    for (size_t i = 0; i < plen; i++) {
	struct ged_db_index_record record;
	if (!ged_db_index_record_find(index, path[i], &record))
	    return 0;
	if (i > 0)
	    bu_vls_putc(out, '/');
	if (verbose)
	    (void)verbose;
	bu_vls_strcat(out, record.name ? record.name : "");
    }

    return 1;
}


ged_db_index_id
ged_db_index_path_hash(struct ged *UNUSED(gedp),
		       const ged_db_index_id *path,
		       size_t path_len,
		       size_t max_len)
{
    return ged_db_index_path_hash_ids(path, path_len, max_len);
}
