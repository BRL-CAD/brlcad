/*              S E L E C T I O N _ S T A T E . C P P
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
/** @file libged/selection_state.cpp
 *
 * GED selection-state service facade.
 */

#include "common.h"

#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bsg/interaction.h"
#include "bsg/selection.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "ged/bsg_ged_draw.h"
#include "ged/selection_state.h"

#include "./bsg_ged_draw_private.h"
#include "./ged_private.h"


struct ged_native_selection_set {
    std::set<std::string> selected_paths;
    mutable int index_current = 0;
    mutable std::set<ged_db_index_id> selected_path_hashes;
    mutable std::set<ged_db_index_id> active_path_hashes;
    mutable std::set<ged_db_index_id> active_parent_path_hashes;
    mutable std::set<ged_db_index_id> immediate_parent_object_ids;
    mutable std::set<ged_db_index_id> grand_parent_object_ids;
};

struct ged_selection_state {
    struct ged *gedp;
    uint64_t revision;
    std::map<std::string, ged_native_selection_set> native_sets;
};


static void
ged_selection_revision_bump(struct ged *gedp)
{
    if (gedp && gedp->i && gedp->i->ged_selection_statep)
	gedp->i->ged_selection_statep->revision++;
}


static struct ged_selection_state *
ged_selection_native_state(struct ged *gedp)
{
    return (gedp && gedp->i) ? gedp->i->ged_selection_statep : nullptr;
}


static std::string
ged_selection_set_key(const char *set_name)
{
    if (!set_name || !set_name[0] || BU_STR_EQUIV(set_name, "default"))
	return std::string();
    return std::string(set_name);
}


static int
ged_selection_set_pattern_is_glob(const char *set_pattern)
{
    return (set_pattern && strchr(set_pattern, '*')) ? 1 : 0;
}


static std::vector<std::string>
ged_selection_native_set_keys(struct ged_selection_state *state,
			      const char *set_pattern,
			      int create_exact)
{
    std::vector<std::string> keys;
    if (!state)
	return keys;

    if (!set_pattern || !set_pattern[0] || BU_STR_EQUIV(set_pattern, "default")) {
	keys.push_back(std::string());
	if (create_exact)
	    (void)state->native_sets[std::string()];
	return keys;
    }

    if (ged_selection_set_pattern_is_glob(set_pattern)) {
	for (auto &entry : state->native_sets) {
	    if (entry.first.empty())
		continue;
	    if (!bu_path_match(set_pattern, entry.first.c_str(), 0))
		keys.push_back(entry.first);
	}
	return keys;
    }

    std::string key = ged_selection_set_key(set_pattern);
    auto it = state->native_sets.find(key);
    if (it != state->native_sets.end()) {
	keys.push_back(key);
    } else if (create_exact) {
	state->native_sets[key] = ged_native_selection_set();
	keys.push_back(key);
    }
    return keys;
}


static ged_native_selection_set *
ged_selection_native_set(struct ged_selection_state *state,
			 const char *set_name,
			 int create)
{
    if (!state)
	return nullptr;

    std::string key = ged_selection_set_key(set_name);
    auto it = state->native_sets.find(key);
    if (it != state->native_sets.end())
	return &it->second;
    if (!create)
	return nullptr;
    return &state->native_sets[key];
}


static std::string
ged_selection_canonical_path(const char *path)
{
    if (!path)
	return std::string();

    while (*path == '/')
	path++;

    std::string ret(path);
    while (!ret.empty() && ret.back() == '/')
	ret.pop_back();
    return ret;
}


static void
ged_selection_path_split(std::vector<std::string> &objs, const char *str)
{
    if (!str)
	return;

    std::string s(str);
    while (s.length() && s.c_str()[0] == '/')
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
	    if (nstr.length())
		objs.push_back(nstr);
	    nstr.clear();
	    continue;
	}
	nstr.push_back(s[i]);
	escaped = false;
    }
    if (nstr.length())
	objs.push_back(nstr);
}


static std::string
ged_selection_name_deescape(const std::string &name)
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


static unsigned long long
ged_selection_name_hash(const char *name)
{
    if (!name)
	return 0;
    return bu_data_hash(name, strlen(name) * sizeof(char));
}


static unsigned long long
ged_selection_path_hash_ids(const std::vector<unsigned long long> &path,
			    size_t max_len)
{
    if (path.empty())
	return 0;

    size_t hlen = max_len ? max_len : path.size();
    if (!hlen || hlen > path.size())
	return 0;

    return bu_data_hash(path.data(), hlen * sizeof(unsigned long long));
}


static std::vector<unsigned long long>
ged_selection_path_digest_string(const char *path)
{
    std::vector<unsigned long long> ret;
    if (!path || !path[0])
	return ret;

    std::vector<std::string> elements;
    ged_selection_path_split(elements, path);
    for (const std::string &element : elements) {
	std::string deescaped = ged_selection_name_deescape(element);
	ret.push_back(ged_selection_name_hash(deescaped.c_str()));
    }

    return ret;
}


static int
ged_selection_path_ids_to_string(struct ged *gedp,
				 const ged_db_index_id *path,
				 size_t path_len,
				 std::string &out)
{
    if (!gedp || !path || !path_len)
	return 0;

    struct bu_vls vpath = BU_VLS_INIT_ZERO;
    int valid = ged_db_index_path_print(gedp, &vpath, path, path_len, 0, 0);
    if (!valid || !bu_vls_strlen(&vpath)) {
	bu_vls_free(&vpath);
	return 0;
    }

    out = bu_vls_cstr(&vpath);
    bu_vls_free(&vpath);
    return 1;
}


static int
ged_selection_native_path_op(struct ged *gedp,
			     ged_native_selection_set *set,
			     const char *path,
			     int select_path);


static int
ged_selection_native_path_ids_op(struct ged *gedp,
				 const char *set_name,
				 const ged_db_index_id *path,
				 size_t path_len,
				 int select_path)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 1);
    if (!native_set)
	return 0;

    std::string path_str;
    if (!ged_selection_path_ids_to_string(gedp, path, path_len, path_str))
	return 0;

    return ged_selection_native_path_op(gedp, native_set, path_str.c_str(),
	select_path);
}


static int
ged_selection_path_ids_resolve(struct ged *gedp,
			       const std::string &path,
			       std::vector<ged_db_index_id> &ids)
{
    ids.clear();
    if (!gedp || path.empty())
	return 0;

    size_t len = ged_db_index_path_resolve(gedp, path.c_str(), NULL, 0);
    if (!len)
	return 0;

    ids.resize(len);
    size_t got = ged_db_index_path_resolve(gedp, path.c_str(), ids.data(), ids.size());
    if (got != len) {
	ids.clear();
	return 0;
    }
    return 1;
}


static ged_db_index_id
ged_selection_path_hash(struct ged *gedp,
			const std::string &path,
			size_t max_len = 0)
{
    std::vector<ged_db_index_id> ids;
    if (ged_selection_path_ids_resolve(gedp, path, ids))
	return ged_db_index_path_hash(gedp, ids.data(), ids.size(), max_len);

    std::vector<unsigned long long> digest =
	ged_selection_path_digest_string(path.c_str());
    return ged_selection_path_hash_ids(digest, max_len);
}


static int
ged_selection_path_prefix(const std::string &prefix, const std::string &path)
{
    if (prefix.empty() || path.empty())
	return 0;
    if (prefix == path)
	return 1;
    return path.size() > prefix.size() &&
	path.compare(0, prefix.size(), prefix) == 0 &&
	path[prefix.size()] == '/';
}


static int
ged_selection_expand_ids(struct ged *gedp,
			 const std::vector<ged_db_index_id> &path,
			 std::set<std::string> &out,
			 std::set<ged_db_index_id> &seen)
{
    if (!gedp || path.empty())
	return 0;

    ged_db_index_id leaf = path.back();
    if (seen.find(leaf) != seen.end()) {
	std::string path_str;
	if (ged_selection_path_ids_to_string(gedp, path.data(), path.size(),
		path_str))
	    out.insert(path_str);
	return 1;
    }

    struct ged_db_index_record rec;
    if (!ged_db_index_record_get(gedp, leaf, &rec) || !rec.child_count) {
	std::string path_str;
	if (ged_selection_path_ids_to_string(gedp, path.data(), path.size(),
		path_str))
	    out.insert(path_str);
	return 1;
    }

    seen.insert(leaf);
    for (size_t i = 0; i < rec.child_count; i++) {
	struct ged_db_index_child child;
	if (!ged_db_index_child_at(gedp, leaf, i, &child))
	    continue;
	std::vector<ged_db_index_id> cpath = path;
	cpath.push_back(child.record.id);
	ged_selection_expand_ids(gedp, cpath, out, seen);
    }
    seen.erase(leaf);
    return 1;
}


static int
ged_selection_expand_path(struct ged *gedp,
			  const std::string &path,
			  std::set<std::string> &out)
{
    std::vector<ged_db_index_id> ids;
    if (!ged_selection_path_ids_resolve(gedp, path, ids)) {
	out.insert(path);
	return 0;
    }

    std::set<ged_db_index_id> seen;
    return ged_selection_expand_ids(gedp, ids, out, seen);
}


static void
ged_selection_native_index_clear(const ged_native_selection_set *set)
{
    if (!set)
	return;
    set->selected_path_hashes.clear();
    set->active_path_hashes.clear();
    set->active_parent_path_hashes.clear();
    set->immediate_parent_object_ids.clear();
    set->grand_parent_object_ids.clear();
}


static void
ged_selection_native_index_invalidate(ged_native_selection_set *set)
{
    if (!set)
	return;
    set->index_current = 0;
}


static void
ged_selection_native_index_ensure(struct ged *gedp,
				  const ged_native_selection_set *set)
{
    if (!set || set->index_current)
	return;

    ged_selection_native_index_clear(set);
    for (const std::string &path : set->selected_paths) {
	ged_db_index_id selected_hash = ged_selection_path_hash(gedp, path);
	if (selected_hash) {
	    set->selected_path_hashes.insert(selected_hash);
	    set->active_path_hashes.insert(selected_hash);
	}

	std::set<std::string> active_paths;
	ged_selection_expand_path(gedp, path, active_paths);
	active_paths.insert(path);
	for (const std::string &active_path : active_paths) {
	    ged_db_index_id active_hash =
		ged_selection_path_hash(gedp, active_path);
	    if (active_hash)
		set->active_path_hashes.insert(active_hash);
	}

	std::vector<ged_db_index_id> ids;
	if (ged_selection_path_ids_resolve(gedp, path, ids)) {
	    for (size_t len = ids.size(); len > 1; len--) {
		ged_db_index_id parent_hash =
		    ged_db_index_path_hash(gedp, ids.data(), ids.size(),
			    len - 1);
		if (parent_hash)
		    set->active_parent_path_hashes.insert(parent_hash);
	    }
	}

	std::vector<unsigned long long> digest =
	    ged_selection_path_digest_string(path.c_str());
	if (digest.size() > 1)
	    set->immediate_parent_object_ids.insert(digest[digest.size() - 2]);
	for (size_t i = 0; i + 2 < digest.size(); i++)
	    set->grand_parent_object_ids.insert(digest[i]);
    }

    set->index_current = 1;
}


static void
ged_selection_native_expand(struct ged *gedp, ged_native_selection_set *set)
{
    if (!gedp || !set)
	return;

    std::set<std::string> expanded;
    for (const std::string &path : set->selected_paths)
	ged_selection_expand_path(gedp, path, expanded);
    set->selected_paths.swap(expanded);
    ged_selection_native_index_invalidate(set);
}


static int
ged_selection_ids_equal_prefix(const std::vector<ged_db_index_id> &a,
			       const std::vector<ged_db_index_id> &b,
			       size_t len)
{
    if (a.size() < len || b.size() < len)
	return 0;
    for (size_t i = 0; i < len; i++) {
	if (a[i] != b[i])
	    return 0;
    }
    return 1;
}


static int
ged_selection_parent_complete(struct ged *gedp,
			      const std::vector<ged_db_index_id> &parent,
			      const std::vector<std::vector<ged_db_index_id>> &paths)
{
    if (!gedp || parent.empty())
	return 0;

    size_t child_count = ged_db_index_child_count(gedp, parent.back());
    if (!child_count)
	return 0;

    std::set<ged_db_index_id> selected_children;
    for (const std::vector<ged_db_index_id> &path : paths) {
	if (path.size() != parent.size() + 1)
	    continue;
	if (ged_selection_ids_equal_prefix(path, parent, parent.size()))
	    selected_children.insert(path.back());
    }

    if (selected_children.size() != child_count)
	return 0;

    for (size_t i = 0; i < child_count; i++) {
	struct ged_db_index_child child;
	if (!ged_db_index_child_at(gedp, parent.back(), i, &child))
	    return 0;
	if (selected_children.find(child.record.id) == selected_children.end())
	    return 0;
    }

    return 1;
}


static void
ged_selection_native_collapse(struct ged *gedp, ged_native_selection_set *set)
{
    if (!gedp || !set)
	return;

    std::set<std::string> expanded;
    for (const std::string &path : set->selected_paths)
	ged_selection_expand_path(gedp, path, expanded);

    std::vector<std::vector<ged_db_index_id>> paths;
    std::set<std::string> unresolved;
    for (const std::string &path : expanded) {
	std::vector<ged_db_index_id> ids;
	if (ged_selection_path_ids_resolve(gedp, path, ids))
	    paths.push_back(ids);
	else
	    unresolved.insert(path);
    }

    int changed = 1;
    while (changed) {
	changed = 0;
	std::map<std::vector<ged_db_index_id>, int> parents;
	for (const std::vector<ged_db_index_id> &path : paths) {
	    if (path.size() <= 1)
		continue;
	    std::vector<ged_db_index_id> parent(path.begin(), path.end() - 1);
	    parents[parent] = 1;
	}

	for (const auto &entry : parents) {
	    const std::vector<ged_db_index_id> &parent = entry.first;
	    if (!ged_selection_parent_complete(gedp, parent, paths))
		continue;

	    std::vector<std::vector<ged_db_index_id>> next_paths;
	    int parent_added = 0;
	    for (const std::vector<ged_db_index_id> &path : paths) {
		if (path.size() == parent.size() + 1 &&
			ged_selection_ids_equal_prefix(path, parent, parent.size())) {
		    if (!parent_added) {
			next_paths.push_back(parent);
			parent_added = 1;
		    }
		    changed = 1;
		} else {
		    next_paths.push_back(path);
		}
	    }
	    paths.swap(next_paths);
	    break;
	}
    }

    std::set<std::string> collapsed = unresolved;
    for (const std::vector<ged_db_index_id> &path : paths) {
	std::string path_str;
	if (ged_selection_path_ids_to_string(gedp, path.data(), path.size(),
		path_str))
	    collapsed.insert(path_str);
    }

    set->selected_paths.swap(collapsed);
    ged_selection_native_index_invalidate(set);
}


static int
ged_selection_native_is_path_selected(struct ged *gedp,
				      const ged_native_selection_set *set,
				      ged_db_index_id path_hash)
{
    if (!set || !path_hash)
	return 0;

    ged_selection_native_index_ensure(gedp, set);
    return set->selected_path_hashes.find(path_hash) !=
	set->selected_path_hashes.end();
}


static int
ged_selection_native_is_path_active(struct ged *gedp,
				    const ged_native_selection_set *set,
				    ged_db_index_id path_hash)
{
    if (!set || !path_hash)
	return 0;

    ged_selection_native_index_ensure(gedp, set);
    return set->active_path_hashes.find(path_hash) !=
	set->active_path_hashes.end();
}


static int
ged_selection_native_is_path_active_parent(struct ged *gedp,
					   const ged_native_selection_set *set,
					   ged_db_index_id path_hash)
{
    if (!set || !path_hash)
	return 0;

    ged_selection_native_index_ensure(gedp, set);
    return set->active_parent_path_hashes.find(path_hash) !=
	set->active_parent_path_hashes.end();
}


static int
ged_selection_native_is_object_immediate_parent(
	struct ged *gedp,
	const ged_native_selection_set *set,
	ged_db_index_id object_id)
{
    if (!set || !object_id)
	return 0;

    ged_selection_native_index_ensure(gedp, set);
    return set->immediate_parent_object_ids.find(object_id) !=
	set->immediate_parent_object_ids.end();
}


static int
ged_selection_native_is_object_grand_parent(struct ged *gedp,
					    const ged_native_selection_set *set,
					    ged_db_index_id object_id)
{
    if (!set || !object_id)
	return 0;

    ged_selection_native_index_ensure(gedp, set);
    return set->grand_parent_object_ids.find(object_id) !=
	set->grand_parent_object_ids.end();
}


static int
ged_selection_path_is_valid_or_drawn(struct ged *gedp,
				     const std::string &path);


static int
ged_selection_native_path_op(struct ged *gedp,
			     ged_native_selection_set *set,
			     const char *path,
			     int select_path)
{
    if (!gedp || !set || !path || !path[0])
	return 0;

    std::string cpath = ged_selection_canonical_path(path);
    if (cpath.empty() || !ged_selection_path_is_valid_or_drawn(gedp, cpath))
	return 0;

    int changed = 0;
    if (select_path) {
	std::vector<std::string> to_erase;
	for (const std::string &selected : set->selected_paths) {
	    if (selected == cpath)
		continue;
	    if (ged_selection_path_prefix(selected, cpath) ||
		    ged_selection_path_prefix(cpath, selected))
		to_erase.push_back(selected);
	}
	for (const std::string &selected : to_erase)
	    set->selected_paths.erase(selected);
	changed = set->selected_paths.insert(cpath).second || !to_erase.empty();
    } else {
	std::vector<std::string> to_erase;
	for (const std::string &selected : set->selected_paths) {
	    if (selected == cpath || ged_selection_path_prefix(cpath, selected))
		to_erase.push_back(selected);
	}
	for (const std::string &selected : to_erase)
	    set->selected_paths.erase(selected);
	changed = !to_erase.empty();
    }

    if (changed)
	ged_selection_revision_bump(gedp);
    if (changed)
	ged_selection_native_index_invalidate(set);
    return 1;
}


static int
ged_selection_record_matches_path(const struct ged_draw_shape_record *rec,
				  const std::string &path)
{
    if (!rec || path.empty())
	return 0;

    auto path_matches_record_path = [](const std::string &selected,
				       const char *record_path) -> int {
	if (!record_path)
	    return 0;
	std::string rpath = ged_selection_canonical_path(record_path);
	if (rpath.empty())
	    return 0;
	if (rpath == selected)
	    return 1;
	if (rpath.size() > selected.size() &&
		rpath.compare(0, selected.size(), selected) == 0 &&
		rpath[selected.size()] == '/')
	    return 1;
	return 0;
    };

    if (path_matches_record_path(path, rec->display_name))
	return 1;

    if (rec->fullpath) {
	char *fp_path = db_path_to_string(rec->fullpath);
	if (fp_path) {
	    int matched = path_matches_record_path(path, fp_path);
	    bu_free(fp_path, "selection record fullpath string");
	    if (matched)
		return 1;
	}
    }

    if (rec->leaf_name && path.find('/') == std::string::npos &&
	    BU_STR_EQUAL(rec->leaf_name, path.c_str()))
	return 1;

    return 0;
}


struct ged_selection_path_drawn_ctx {
    std::string path;
    int found;
};


static int
ged_selection_path_drawn_cb(const struct ged_draw_shape_record *rec, void *ud)
{
    struct ged_selection_path_drawn_ctx *ctx =
	(struct ged_selection_path_drawn_ctx *)ud;
    if (!ctx || !rec)
	return 1;
    if (ged_selection_record_matches_path(rec, ctx->path)) {
	ctx->found = 1;
	return 0;
    }
    return 1;
}


static int
ged_selection_path_is_valid_or_drawn(struct ged *gedp,
				     const std::string &path)
{
    if (!gedp || path.empty())
	return 0;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    int valid = (db_string_to_path(&dfp, gedp->dbip, path.c_str()) == 0);
    db_free_full_path(&dfp);
    if (valid)
	return 1;

    struct ged_selection_path_drawn_ctx ctx;
    ctx.path = path;
    ctx.found = 0;
    ged_draw_foreach_shape_record(gedp, ged_selection_path_drawn_cb, &ctx);
    return ctx.found;
}


static unsigned long long
ged_selection_native_hash(const ged_native_selection_set *set)
{
    if (!set || set->selected_paths.empty())
	return 0;

    struct bu_data_hash_state *s = bu_data_hash_create();
    if (!s)
	return 0;

    for (const std::string &path : set->selected_paths) {
	bu_data_hash_update(s, path.c_str(), path.size());
	const char sep = '\n';
	bu_data_hash_update(s, &sep, sizeof(sep));
    }

    unsigned long long hval = bu_data_hash_val(s);
    bu_data_hash_destroy(s);
    return hval;
}


static void
ged_selection_native_view_snapshot(struct ged *gedp,
				   std::map<struct bsg_view *, std::set<std::string>> &snapshot)
{
    if (!gedp)
	return;

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    for (size_t i = 0; views && i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	struct bsg_selection *selection = bsg_view_selection(v);
	if (!selection)
	    continue;
	for (size_t ri = 0; ri < bsg_selection_count(selection); ri++) {
	    const struct bsg_interaction_record *record =
		bsg_selection_record(selection, ri);
	    const char *path = bsg_interaction_record_path(record);
	    if (path && path[0])
		snapshot[v].insert(ged_selection_canonical_path(path));
	}
    }
}


struct ged_selection_native_draw_sync_ctx {
    struct ged *gedp;
    const ged_native_selection_set *set;
    std::map<struct bsg_view *, std::set<std::string>> *new_selection;
    const std::string *selected_path;
};


static int
ged_selection_native_draw_sync_shape_ref(
	struct ged_selection_native_draw_sync_ctx *ctx,
	ged_draw_shape_ref ref)
{
    if (!ctx || !ctx->gedp || !ctx->set || ged_draw_shape_ref_is_null(ref))
	return 1;

    struct bsg_view *v = ged_draw_shape_ref_view(ctx->gedp, ref);
    if (!v || !bsg_view_selection(v))
	v = ctx->gedp->ged_gvp;
    if (!v || !bsg_view_selection(v))
	return 1;

    struct bsg_interaction_record *record =
	ged_draw_shape_ref_selection_record(ctx->gedp, ref, v);
    if (!record)
	return 1;

    const char *path = bsg_interaction_record_path(record);
    if (path && path[0])
	(*ctx->new_selection)[v].insert(ged_selection_canonical_path(path));
    if (!bsg_selection_contains_record(bsg_view_selection(v), record))
	bsg_selection_add_record(bsg_view_selection(v), record);
    bsg_interaction_record_free(record);
    return 1;
}


static int
ged_selection_native_draw_sync_cb(const struct ged_draw_shape_record *rec,
				  void *ud)
{
    struct ged_selection_native_draw_sync_ctx *ctx =
	(struct ged_selection_native_draw_sync_ctx *)ud;
    if (!ctx || !ctx->gedp || !ctx->set || !rec)
	return 1;

    int selected = 0;
    for (const std::string &path : ctx->set->selected_paths) {
	if (ged_selection_record_matches_path(rec, path)) {
	    selected = 1;
	    break;
	}
    }
    if (!selected)
	return 1;

    return ged_selection_native_draw_sync_shape_ref(ctx, rec->ref);
}


static int
ged_selection_native_draw_sync_index_cb(bsg_scene_ref shape_ref, void *ud)
{
    struct ged_selection_native_draw_sync_ctx *ctx =
	(struct ged_selection_native_draw_sync_ctx *)ud;
    if (!ctx || !ctx->gedp || bsg_scene_ref_is_null(shape_ref))
	return 1;

    ged_draw_shape_ref ref =
	ged_draw_shape_ref_from_scene_ref(ctx->gedp, shape_ref);
    if (ctx->selected_path) {
	struct ged_draw_shape_record rec;
	memset(&rec, 0, sizeof(rec));
	if (!ged_draw_shape_record_get(ctx->gedp, ref, &rec) ||
		!ged_selection_record_matches_path(&rec, *ctx->selected_path))
	    return 1;
    }

    return ged_selection_native_draw_sync_shape_ref(ctx, ref);
}


struct ged_selection_draw_sync_query {
    std::string selected_path;
    int component;
    unsigned long long path_hash;
};


static int
ged_selection_native_draw_sync_queries(
	struct ged *gedp,
	const ged_native_selection_set *set,
	std::vector<ged_selection_draw_sync_query> &queries)
{
    if (!gedp || !set)
	return 0;

    queries.clear();
    for (const std::string &selected_path : set->selected_paths) {
	if (selected_path.find('/') == std::string::npos) {
	    ged_selection_draw_sync_query query;
	    query.selected_path = selected_path;
	    query.component = 1;
	    query.path_hash = 0;
	    queries.push_back(query);
	    continue;
	}

	std::set<std::string> active_paths;
	if (!ged_selection_expand_path(gedp, selected_path, active_paths))
	    return 0;

	for (const std::string &active_path : active_paths) {
	    unsigned long long path_hash =
		ged_selection_path_hash(gedp, active_path);
	    if (!path_hash)
		return 0;

	    ged_selection_draw_sync_query query;
	    query.selected_path = selected_path;
	    query.component = 0;
	    query.path_hash = path_hash;
	    queries.push_back(query);
	}
    }

    return 1;
}


static void
ged_selection_draw_sync_note_shape_scan(struct ged *gedp)
{
    if (gedp && gedp->i && gedp->i->ged_gdp)
	gedp->i->ged_gdp->gd_draw_index_fallback_shape_scans++;
}


static int
ged_selection_native_draw_sync(struct ged *gedp,
			       const ged_native_selection_set *set)
{
    if (!gedp || !set)
	return 0;

    std::map<struct bsg_view *, std::set<std::string>> old_selection;
    std::map<struct bsg_view *, std::set<std::string>> new_selection;
    ged_selection_native_view_snapshot(gedp, old_selection);

    struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
    for (size_t i = 0; views && i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	struct bsg_selection *selection = bsg_view_selection(v);
	if (selection)
	    bsg_selection_clear(selection);
    }

    struct ged_selection_native_draw_sync_ctx ctx;
    ctx.gedp = gedp;
    ctx.set = set;
    ctx.new_selection = &new_selection;
    ctx.selected_path = nullptr;

    std::vector<ged_selection_draw_sync_query> queries;
    int indexed = ged_selection_native_draw_sync_queries(gedp, set, queries);
    if (indexed) {
	for (const ged_selection_draw_sync_query &query : queries) {
	    ctx.selected_path = &query.selected_path;
	    int count = query.component ?
		ged_draw_shape_index_for_component(gedp,
			query.selected_path.c_str(),
			ged_selection_native_draw_sync_index_cb, &ctx) :
		ged_draw_shape_index_for_path_hash(gedp, query.path_hash,
			ged_selection_native_draw_sync_index_cb, &ctx);
	    if (count < 0) {
		indexed = 0;
		break;
	    }
	}
	ctx.selected_path = nullptr;
    }

    if (!indexed) {
	new_selection.clear();
	for (size_t i = 0; views && i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	    struct bsg_selection *selection = bsg_view_selection(v);
	    if (selection)
		bsg_selection_clear(selection);
	}
	ged_selection_draw_sync_note_shape_scan(gedp);
	ged_draw_foreach_shape_record(gedp, ged_selection_native_draw_sync_cb,
		&ctx);
    }

    return old_selection != new_selection;
}


struct ged_selection_state *
ged_selection_state_create(struct ged *gedp)
{
    struct ged_selection_state *state = new ged_selection_state;
    state->gedp = gedp;
    state->revision = 1;
    return state;
}


void
ged_selection_state_destroy(struct ged_selection_state *state)
{
    if (!state)
	return;
    delete state;
}


int
ged_selection_state_available(struct ged *gedp)
{
    return (gedp && gedp->i && gedp->i->ged_selection_statep) ? 1 : 0;
}


size_t
ged_selection_count(struct ged *gedp, const char *set_name)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return native_set ? native_set->selected_paths.size() : 0;
}


int
ged_selection_list_set_names(struct ged *gedp, struct bu_vls *out)
{
    if (!out)
	return 0;

    struct ged_selection_state *state = ged_selection_native_state(gedp);
    if (!state)
	return 0;

    int count = 0;
    for (auto &entry : state->native_sets) {
	if (entry.first.empty())
	    continue;
	bu_vls_printf(out, "%s\n", entry.first.c_str());
	count++;
    }
    return count;
}


int
ged_selection_list_paths(struct ged *gedp,
			 const char *set_pattern,
			 struct bu_vls *out)
{
    if (!out)
	return 0;

    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 1);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	if (!native_set)
	    continue;
	for (const std::string &path : native_set->selected_paths)
	    bu_vls_printf(out, "%s\n", path.c_str());
    }
    return (int)keys.size();
}


int
ged_selection_set_match_count(struct ged *gedp, const char *set_pattern)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 1);
    return (int)keys.size();
}


unsigned long long
ged_selection_state_hash(struct ged *gedp, const char *set_name)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_hash(native_set);
}


int
ged_selection_clear(struct ged *gedp, const char *set_name)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 1);
    if (!native_set)
	return 0;
    native_set->selected_paths.clear();
    ged_selection_native_index_invalidate(native_set);
    ged_selection_revision_bump(gedp);
    return 1;
}


int
ged_selection_clear_matching(struct ged *gedp, const char *set_pattern)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 1);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	if (native_set) {
	    native_set->selected_paths.clear();
	    ged_selection_native_index_invalidate(native_set);
	}
    }
    if (keys.size())
	ged_selection_revision_bump(gedp);
    return (int)keys.size();
}


static int
ged_selection_path_op(struct ged *gedp,
		      const char *set_name,
		      const char *path,
		      int UNUSED(recompute_hierarchy),
		      int select_path)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 1);
    return ged_selection_native_path_op(gedp, native_set, path, select_path);
}


static int
ged_selection_path_matching_op(struct ged *gedp,
			       const char *set_pattern,
			       const char *path,
			       int UNUSED(recompute_hierarchy),
			       int select_path)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 1);
    if (keys.size() != 1)
	return 0;

    ged_native_selection_set *native_set =
	ged_selection_native_set(state, keys[0].c_str(), 1);
    return ged_selection_native_path_op(gedp, native_set, path, select_path);
}


int
ged_selection_select_path(struct ged *gedp,
			  const char *set_name,
			  const char *path,
			  int recompute_hierarchy)
{
    return ged_selection_path_op(gedp, set_name, path, recompute_hierarchy, 1);
}


int
ged_selection_select_path_matching(struct ged *gedp,
				   const char *set_pattern,
				   const char *path,
				   int recompute_hierarchy)
{
    return ged_selection_path_matching_op(gedp, set_pattern, path,
	recompute_hierarchy, 1);
}


int
ged_selection_deselect_path(struct ged *gedp,
			    const char *set_name,
			    const char *path,
			    int recompute_hierarchy)
{
    return ged_selection_path_op(gedp, set_name, path, recompute_hierarchy, 0);
}


int
ged_selection_deselect_path_matching(struct ged *gedp,
				     const char *set_pattern,
				     const char *path,
				     int recompute_hierarchy)
{
    return ged_selection_path_matching_op(gedp, set_pattern, path,
	recompute_hierarchy, 0);
}


int
ged_selection_select_path_ids(struct ged *gedp,
			      const char *set_name,
			      const ged_db_index_id *path,
			      size_t path_len,
			      int recompute_hierarchy)
{
    (void)recompute_hierarchy;
    if (!path || !path_len)
	return 0;

    return ged_selection_native_path_ids_op(gedp, set_name, path, path_len,
	1);
}


int
ged_selection_deselect_path_ids(struct ged *gedp,
				const char *set_name,
				const ged_db_index_id *path,
				size_t path_len,
				int recompute_hierarchy)
{
    (void)recompute_hierarchy;
    if (!path || !path_len)
	return 0;

    return ged_selection_native_path_ids_op(gedp, set_name, path, path_len,
	0);
}


int
ged_selection_recompute(struct ged *gedp, const char *set_name)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    if (!native_set)
	return 0;
    ged_selection_native_index_invalidate(native_set);
    return 1;
}


int
ged_selection_recompute_matching(struct ged *gedp, const char *set_pattern)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 0);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	ged_selection_native_index_invalidate(native_set);
    }
    return (int)keys.size();
}


int
ged_selection_expand_matching(struct ged *gedp, const char *set_pattern)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 0);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	ged_selection_native_expand(gedp, native_set);
    }
    if (keys.size())
	ged_selection_revision_bump(gedp);
    return (int)keys.size();
}


int
ged_selection_collapse_matching(struct ged *gedp, const char *set_pattern)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 0);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	ged_selection_native_collapse(gedp, native_set);
    }
    if (keys.size())
	ged_selection_revision_bump(gedp);
    return (int)keys.size();
}


int
ged_selection_draw_sync(struct ged *gedp, const char *set_name)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_draw_sync(gedp, native_set);
}


int
ged_selection_draw_sync_matching(struct ged *gedp, const char *set_pattern)
{
    int changed = 0;
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    std::vector<std::string> keys =
	ged_selection_native_set_keys(state, set_pattern, 0);
    for (const std::string &key : keys) {
	ged_native_selection_set *native_set =
	    ged_selection_native_set(state, key.c_str(), 0);
	if (ged_selection_native_draw_sync(gedp, native_set))
	    changed = 1;
    }
    return changed;
}


int
ged_selection_is_path_selected(struct ged *gedp,
			       const char *set_name,
			       ged_db_index_id path_hash)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_path_selected(gedp, native_set, path_hash);
}


int
ged_selection_is_path_active(struct ged *gedp,
			     const char *set_name,
			     ged_db_index_id path_hash)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_path_active(gedp, native_set, path_hash);
}


int
ged_selection_is_path_active_parent(struct ged *gedp,
				    const char *set_name,
				    ged_db_index_id path_hash)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_path_active_parent(gedp, native_set, path_hash);
}


int
ged_selection_is_object_parent(struct ged *gedp,
			       const char *set_name,
			       ged_db_index_id object_id)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_object_immediate_parent(gedp, native_set,
	object_id) ||
	ged_selection_native_is_object_grand_parent(gedp, native_set,
	    object_id);
}


int
ged_selection_is_object_immediate_parent(struct ged *gedp,
					 const char *set_name,
					 ged_db_index_id object_id)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_object_immediate_parent(gedp, native_set,
	object_id);
}


int
ged_selection_is_object_grand_parent(struct ged *gedp,
				     const char *set_name,
				     ged_db_index_id object_id)
{
    struct ged_selection_state *state = ged_selection_native_state(gedp);
    ged_native_selection_set *native_set =
	ged_selection_native_set(state, set_name, 0);
    return ged_selection_native_is_object_grand_parent(gedp, native_set,
	object_id);
}
