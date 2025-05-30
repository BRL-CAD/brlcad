/*                D B I _ S E L E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @addtogroup ged_db*/
/** @{ */
/** @file libged/dbi_state.cpp
 *
 * Maintain and manage an in-memory representation of the database hierarchy.
 * Main utility of this is to make commonly needed hierarchy/attribute
 * information available to applications without having to crack combs from
 * disk during a standard tree walk.
 */

#include "common.h"

#include <algorithm>
#include <map>
#include <thread>
#include <fstream>
#include <sstream>

extern "C" {
#include "lmdb.h"
}

#include "./alphanum.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bv/lod.h"
#include "raytrace.h"
#include "ged/defines.h"
#include "ged/view.h"
#include "./ged_private.h"


// alphanum sort
static bool alphanum_cmp(const std::string &a, const std::string &b)
{
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

// This version of the cyclic check assumes the path entries other than the
// last one are OK, and checks only against that last entry.
//
// TODO - need to catch instance specifiers here to
// robustly avoid (e.g.) /a/b@1/c/b@2
static bool
path_addition_cyclic(std::vector<unsigned long long> &path)
{
    if (path.size() == 1)
	return false;
    int new_entry = path.size() - 1;
    int i = new_entry - 1;
    while (i >= 0) {
	if (path[new_entry] == path[i])
	    return true;
	i--;
    }
    return false;
}



/* Handle selection status for various instances in the database */

BSelectState::BSelectState(DbiState *s)
{
    dbis = s;
}

bool
BSelectState::select_path(const char *path, bool update)
{
    if (!path)
	return false;

    std::vector<unsigned long long> path_hashes = dbis->digest_path(path);
    if (!path_hashes.size())
	return false;

    bool ret = select_hpath(path_hashes);
    if (update)
	characterize();
    return ret;
}

bool
BSelectState::select_hpath(std::vector<unsigned long long> &hpath)
{
    if (!hpath.size())
	return false;

    // If we're already selected, nothing to do
    unsigned long long shash = dbis->path_hash(hpath, 0);
    if (selected.find(shash) != selected.end())
	return true;

    // Validate that the specified path is current in the database.
    for (size_t i = 1; i < hpath.size(); i++) {
	unsigned long long phash = hpath[i-1];
	unsigned long long chash = hpath[i];
	if (dbis->p_c.find(phash) == dbis->p_c.end())
	    return false;
	if (dbis->p_c[phash].find(chash) == dbis->p_c[phash].end())
	    return false;
    }

    // If we're going to select this path, we need to clear out conflicting
    // paths.  We deliberately don't allow selection of multiple levels of
    // a single path, to avoid unexpected and unintuitive behaviors.  This
    // means we have to clear any selection that is either a superset of this
    // path or a child of it.
    std::vector<unsigned long long> pitems = hpath;
    pitems.pop_back();
    while (pitems.size()) {
	unsigned long long phash = dbis->path_hash(pitems, 0);
	selected.erase(phash);
	active_paths.erase(phash);
	pitems.pop_back();
    }
    // Clear any active children of the selected path
    pitems = hpath;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(pitems[pitems.size() -1]);
    if (pc_it != dbis->p_c.end()) {
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
	    clear_paths(*c_it, pitems);
    }

    // Add to selected set
    selected[shash] = hpath;

    // Note - with this lower level function, it is the caller's responsibility
    // to call characterize to populate the path relationships - we deliberately
    // do not do it here, so an application can do the work once per cycle
    // rather than being forced to do it per path.

    return true;
}

bool
BSelectState::deselect_path(const char *path, bool update)
{
    if (!path)
	return false;

    std::vector<unsigned long long> path_hashes = dbis->digest_path(path);
    if (!path_hashes.size())
	return false;

    bool ret = deselect_hpath(path_hashes);
    if (update)
	characterize();
    return ret;
}

bool
BSelectState::deselect_hpath(std::vector<unsigned long long> &hpath)
{
    if (!hpath.size())
	return false;

    // For higher level paths, need to clear the illuminated solids
    // below this path (if any)
    std::vector<unsigned long long> pitems = hpath;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(pitems[pitems.size() -1]);
    if (pc_it != dbis->p_c.end()) {
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
	    clear_paths(*c_it, pitems);
    }

    // Clear the selection itself
    unsigned long long phash = dbis->path_hash(hpath, 0);
    selected.erase(phash);
    active_paths.erase(phash);
    return true;

    // Note - with this lower level function, it is the caller's responsibility
    // to call characterize to populate the path relationships - we deliberately
    // do not do it here, so an application can do the work once per cycle
    // rather than being forced to do it per path.
}

bool
BSelectState::is_selected(unsigned long long hpath)
{
    if (!hpath)
	return false;

    if (selected.find(hpath) == selected.end())
	return false;

    return true;
}

bool
BSelectState::is_active(unsigned long long phash)
{
    if (!phash)
	return false;

    if (active_paths.find(phash) == active_paths.end())
	return false;

    return true;
}

bool
BSelectState::is_active_parent(unsigned long long phash)
{
    if (!phash)
	return false;

    if (active_parents.find(phash) == active_parents.end())
	return false;

    return true;
}

bool
BSelectState::is_parent_obj(unsigned long long hash)
{
    if (is_immediate_parent_obj(hash) || is_grand_parent_obj(hash))
	return true;

    return false;
}

bool
BSelectState::is_immediate_parent_obj(unsigned long long hash)
{
    if (!hash)
	return false;

    if (immediate_parents.find(hash) == immediate_parents.end())
	return false;

    return true;
}

bool
BSelectState::is_grand_parent_obj(unsigned long long hash)
{

    if (!hash)
	return false;

    if (grand_parents.find(hash) == grand_parents.end())
	return false;

    return true;
}

void
BSelectState::clear()
{
    selected.clear();
    active_paths.clear();
    characterize();
}

std::vector<std::string>
BSelectState::list_selected_paths()
{
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    std::vector<std::string> ret;
    struct bu_vls vpath = BU_VLS_INIT_ZERO;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	dbis->print_path(&vpath, s_it->second);
	ret.push_back(std::string(bu_vls_cstr(&vpath)));
    }
    bu_vls_free(&vpath);
    std::sort(ret.begin(), ret.end(), &alphanum_cmp);
    return ret;
}

void
BSelectState::add_paths(
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(c_hash);

    path_hashes.push_back(c_hash);
    unsigned long long phash = dbis->path_hash(path_hashes, 0);
    active_paths.insert(phash);

    if (!path_addition_cyclic(path_hashes)) {
	/* Not cyclic - keep going */
	if (pc_it != dbis->p_c.end()) {
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
		add_paths(*c_it, path_hashes);
	}
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();
}

void
BSelectState::clear_paths(
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(c_hash);
    path_hashes.push_back(c_hash);

    unsigned long long phash = dbis->path_hash(path_hashes, 0);
    selected.erase(phash);
    active_paths.erase(phash);

    if (!path_addition_cyclic(path_hashes)) {
	/* Not cyclic - keep going */
	if (pc_it != dbis->p_c.end()) {
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
		clear_paths(*c_it, path_hashes);
	}
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();
}

void
BSelectState::expand_paths(
	std::vector<std::vector<unsigned long long>> &out_paths,
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(c_hash);

    path_hashes.push_back(c_hash);

    if (!path_addition_cyclic(path_hashes)) {
	/* Not cyclic - keep going */
	if (pc_it != dbis->p_c.end()) {
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
		expand_paths(out_paths, *c_it, path_hashes);
	} else {
	    out_paths.push_back(path_hashes);
	}
    } else {
	out_paths.push_back(path_hashes);
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();
}

void
BSelectState::expand()
{
    // Given the current selection set, expand all the paths to
    // their leaf solids and report those paths
    std::vector<std::vector<unsigned long long>> out_paths;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	std::vector<unsigned long long> seed_hashes = s_it->second;
	unsigned long long shash = seed_hashes[seed_hashes.size() - 1];
	seed_hashes.pop_back();
	expand_paths(out_paths, shash, seed_hashes);
    }

    // Update selected.
    selected.clear();
    for (size_t i = 0; i < out_paths.size(); i++) {
	unsigned long long phash = dbis->path_hash(out_paths[i], 0);
	selected[phash] = out_paths[i];
    }

    characterize();
}

void
BSelectState::collapse()
{
    std::vector<std::vector<unsigned long long>> collapsed;
    std::map<size_t, std::unordered_set<unsigned long long>> depth_groups;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    std::unordered_set<unsigned long long>::iterator u_it;

    // Group paths of the same depth.  Depth == 1 paths are already
    // top level objects and need no further processing.
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	if (s_it->second.size() == 1) {
	    collapsed.push_back(s_it->second);
	} else {
	    depth_groups[s_it->second.size()].insert(s_it->first);
	}
    }

    // Whittle down the mode depth groups until we find not-fully-drawn
    // parents - when we find that, the children constitute non-collapsible
    // paths based on what's drawn in this mode
    while (depth_groups.size()) {
	size_t plen = depth_groups.rbegin()->first;
	if (plen == 1)
	    break;
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;

	// For a given depth, group the paths by parent path.  This results
	// in path sub-groups which will define for us how "fully drawn"
	// that particular parent comb instance is.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> grouped_pckeys;
	std::unordered_map<unsigned long long, unsigned long long> pcomb;
	for (u_it = pckeys.begin(); u_it != pckeys.end(); u_it++) {
	    std::vector<unsigned long long> &pc_path = selected[*u_it];
	    unsigned long long ppathhash = dbis->path_hash(pc_path, plen - 1);
	    grouped_pckeys[ppathhash].insert(*u_it);
	    pcomb[ppathhash] = pc_path[plen-2];
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pg_it;
	for (pg_it = grouped_pckeys.begin(); pg_it != grouped_pckeys.end(); pg_it++) {

	    // As above, use the full path from selected, but this time
	    // we're collecting the children.  This is the set we need to compare
	    // against the .g ground truth to determine fully or partially drawn.
	    std::unordered_set<unsigned long long> g_children;
	    std::unordered_set<unsigned long long> &g_pckeys = pg_it->second;
	    for (u_it = g_pckeys.begin(); u_it != g_pckeys.end(); u_it++) {
		std::vector<unsigned long long> &pc_path = selected[*u_it];
		g_children.insert(pc_path[plen-1]);
	    }

	    // Do the check against the .g comb children info - the "ground truth"
	    // that defines what must be present for a fully drawn comb
	    bool is_fully_selected = true;
	    std::unordered_set<unsigned long long> &ground_truth = dbis->p_c[pcomb[pg_it->first]];
	    for (u_it = ground_truth.begin(); u_it != ground_truth.end(); u_it++) {
		if (g_children.find(*u_it) == g_children.end()) {
		    is_fully_selected = false;
		    break;
		}
	    }

	    if (is_fully_selected) {
		// If fully selected, depth_groups[plen-1] gets the first path in
		// g_pckeys.  The path is longer than that depth, but contains
		// all the necessary information and using that approach avoids
		// the need to duplicate paths.
		depth_groups[plen - 1].insert(*g_pckeys.begin());
	    } else {
		// No further collapsing - add to final.  We must make trimmed
		// versions of the paths in case this depth holds promoted
		// paths from deeper levels, since we are duplicating the full
		// path contents.
		for (u_it = g_pckeys.begin(); u_it != g_pckeys.end(); u_it++) {
		    std::vector<unsigned long long> trimmed = selected[*u_it];
		    trimmed.resize(plen);
		    collapsed.push_back(trimmed);
		}
	    }
	}

	// Done with this depth
	depth_groups.erase(plen);
    }

    // If we collapsed all the way to top level objects, make sure to add them
    // if they are still valid entries.  If a toplevel entry is invalid, there
    // is no parent comb to refer to it as an "invalid" object and it can no
    // longer be drawn.
    if (depth_groups.find(1) != depth_groups.end()) {
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;
	for (u_it = pckeys.begin(); u_it != pckeys.end(); u_it++) {
	    std::vector<unsigned long long> trimmed = selected[*u_it];
	    trimmed.resize(1);
	    collapsed.push_back(trimmed);
	}
    }

    selected.clear();
    for (size_t i = 0; i < collapsed.size(); i++) {
	unsigned long long phash = dbis->path_hash(collapsed[i], 0);
	selected[phash] = collapsed[i];
    }

    characterize();
}

void
BSelectState::characterize()
{
    //bu_log("BSelectState::characterize\n");
    active_parents.clear();
    immediate_parents.clear();
    grand_parents.clear();

    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	std::vector<unsigned long long> seed_hashes = s_it->second;
	unsigned long long shash = seed_hashes[seed_hashes.size() - 1];
	seed_hashes.pop_back();
	add_paths(shash, seed_hashes);

	// Stash the parent paths above this specific selection
	std::vector<unsigned long long> pitems = s_it->second;
	size_t c = s_it->second.size() - 1;
	while (c > 0) {
	    pitems.pop_back();
	    unsigned long long pphash = dbis->path_hash(s_it->second, c);
	    active_parents.insert(pphash);
	    c--;
	}
    }

    // Now, characterizing related objects.  This is not just the immediate
    // path parents - anything above the selected object is impacted.

    // Because we don't want to keep iterating over p_c, make a reverse map of children
    // to parents
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> reverse_map;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    for (pc_it = dbis->p_c.begin(); pc_it != dbis->p_c.end(); pc_it++) {
	std::unordered_set<unsigned long long>::iterator sc_it;
	for (sc_it = pc_it->second.begin(); sc_it != pc_it->second.end(); sc_it++) {
	    reverse_map[*sc_it].insert(pc_it->first);
	}
    }

    // Find the leaf children - they're the seeds
    std::unordered_set<unsigned long long> active_children;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	active_children.insert(s_it->second[s_it->second.size()-1]);
    }

    // Find the immediate parents - they can be highlighted differently
    std::unordered_set<unsigned long long>::iterator c_it, p_it;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator r_it;
    for (c_it = active_children.begin(); c_it != active_children.end(); c_it++) {
	r_it = reverse_map.find(*c_it);
	if (r_it == reverse_map.end())
	    continue;
	for (p_it = r_it->second.begin(); p_it != r_it->second.end(); p_it++)
	    immediate_parents.insert(*p_it);
    }

    // Work our way up from the immediate parents - we want the higher levels to
    // be known as active so they may indicate that active selections can be found
    // below
    std::queue<unsigned long long> gqueue;
    for (p_it = immediate_parents.begin(); p_it != immediate_parents.end(); p_it++) {
	gqueue.push(*p_it);
    }
    while (!gqueue.empty()) {
	unsigned long long obj = gqueue.front();
	gqueue.pop();
	r_it = reverse_map.find(obj);
	if (r_it == reverse_map.end())
	    continue;
	for (p_it = r_it->second.begin(); p_it != r_it->second.end(); p_it++) {
	    gqueue.push(*p_it);
	    grand_parents.insert(*p_it);
	}
    }
}

void
BSelectState::refresh()
{
    // If the database may have changed, we need to revalidate selected
    // paths are still current, and regenerate the active_paths set.
    active_paths.clear();

    // Unlike drawing, nothing fancy here - if a selected path is invalid,
    // it's gone.
    std::vector<unsigned long long> to_clear;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	std::vector<unsigned long long> &cpath = s_it->second;
	for (size_t i = 1; i < cpath.size(); i++) {
	    unsigned long long phash = cpath[i-1];
	    unsigned long long chash = cpath[i];
	    if (dbis->p_c.find(phash) == dbis->p_c.end()) {
		to_clear.push_back(s_it->first);
		continue;
	    }
	    if (dbis->p_c[phash].find(chash) == dbis->p_c[phash].end()) {
		to_clear.push_back(s_it->first);
		continue;
	    }
	}
    }

    // Erase invalid paths
    for (size_t i = 0; i < to_clear.size(); i++) {
	selected.erase(to_clear[i]);
    }

    // For all surviving selections, generate paths
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	std::vector<unsigned long long> seed_hashes = s_it->second;
	unsigned long long shash = seed_hashes[seed_hashes.size() - 1];
	seed_hashes.pop_back();
	add_paths(shash, seed_hashes);
    }
}

bool
BSelectState::draw_sync()
{
    bool changed = false;
    std::unordered_set<BViewState *> vstates;

    struct bu_ptbl *views = bv_set_views(&dbis->gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	BViewState *vs = dbis->get_view_state(v);
	vstates.insert(vs);
    }

    std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator so_it;
    std::unordered_map<int, struct bv_scene_obj *>::iterator m_it;
    std::unordered_set<BViewState *>::iterator vs_it;
    for (vs_it = vstates.begin(); vs_it != vstates.end(); vs_it++) {
	for (so_it = (*vs_it)->s_map.begin(); so_it != (*vs_it)->s_map.end(); so_it++) {
	    char ill_state = is_active(so_it->first) ? UP : DOWN;
	    //bu_log("select ill_state: %s\n", (ill_state == UP) ? "up" : "down");
	    for (m_it = so_it->second.begin(); m_it != so_it->second.end(); m_it++) {
		struct bv_scene_obj *so = m_it->second;
		int ill_changed = bv_illum_obj(so, ill_state);
		if (ill_changed)
		    changed = true;
	    }
	}
    }

    return changed;
}

unsigned long long
BSelectState::state_hash()
{
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator s_it;
    struct bu_data_hash_state *s = bu_data_hash_create();
    if (!s)
	return 0;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	bu_data_hash_update(s, &s_it->first, sizeof(unsigned long long));
    }
    unsigned long long hval = bu_data_hash_val(s);
    bu_data_hash_destroy(s);
    return hval;
}

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
