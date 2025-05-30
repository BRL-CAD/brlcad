/*                   D B I _ B V I E W . C P P
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


BViewState::BViewState(DbiState *s)
{
    dbis = s;
}

// 0 = valid, 3 = need re-eval
int
BViewState::leaf_check(
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    if (!c_hash)
	return 0;

    bool is_removed = (dbis->removed.find(c_hash) != dbis->removed.end());
    if (is_removed)
	return 3;

    bool is_changed = (dbis->changed_hashes.find(c_hash) != dbis->changed_hashes.end());
    if (is_changed)
	return 3;

    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(c_hash);
    path_hashes.push_back(c_hash);

    if (!path_addition_cyclic(path_hashes)) {
	/* Not cyclic - keep going */
	if (pc_it != dbis->p_c.end()) {
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
		if (leaf_check(*c_it, path_hashes))
		    return 3;
	}
    }

    return 0;
}

// 0 = valid, 1 = invalid, 2 = invalid, remain "drawn", 3 == need re-eval
int
BViewState::check_status(
	std::unordered_set<unsigned long long> *invalid_paths,
	std::unordered_set<unsigned long long> *changed_paths,
	unsigned long long path_hash,
       	std::vector<unsigned long long> &cpath,
	bool leaf_expand
	)
{
    // If nothing was removed or changed, there's nothing to tell us anything
    // is invalid - just return
    if (dbis->removed.size() && !dbis->changed_hashes.size())
	return 0;

    bool parent_changed = false;
    for (size_t j = 0; j < cpath.size(); j++) {
	unsigned long long phash = (j > 0) ? cpath[j-1] : 0;
	unsigned long long hash = cpath[j];
	if (phash && parent_changed) {
	    // Need to see if this is still a parent of the new comb. This step
	    // is why the draw update has to come AFTER the above primitive
	    // update passes, so the comb can give us the correct, current
	    // answer.
	    bool is_parent = false;
	    if (dbis->p_c.find(phash) != dbis->p_c.end() && dbis->p_c[phash].find(hash) != dbis->p_c[phash].end())
		is_parent = true;
	    // If not we're done, whether or not the parent dp was
	    // removed from the database.
	    if (!is_parent) {
		if (invalid_paths)
		    (*invalid_paths).insert(path_hash);
		if (changed_paths)
		    (*changed_paths).erase(path_hash);
		return 1;
	    }
	    // If it's still in the comb tree, proceed with the evaluation.
	}

	bool is_removed = (dbis->removed.find(hash) != dbis->removed.end());
	bool is_changed = (dbis->changed_hashes.find(hash) != dbis->changed_hashes.end());
	if (is_removed) {
	    if (is_removed && !j) {
		// Top level removed - everything else is gone
		if (invalid_paths)
		    (*invalid_paths).insert(path_hash);
		if (changed_paths)
		    (*changed_paths).erase(path_hash);
		return 1;
	    }

	    if (is_removed && j != cpath.size()-1) {
		// If removed is first and not a leaf, erase - if we got here
		// the parent comb either wasn't changed at all or this
		// particular instance is still there; either way the state
		// here is not preservable, since the path is trying to refer
		// to a tree path which no longer exists in the hierarchy.
		if (invalid_paths)
		    (*invalid_paths).insert(path_hash);
		if (changed_paths)
		    (*changed_paths).erase(path_hash);
		return 1;
	    }
	    if (is_removed && j == cpath.size()-1) {
		// If removed is a leaf and the comb instance is intact,
		// leave "drawn" as invalid path.
		if (changed_paths)
		    (*changed_paths).insert(path_hash);
		return 2;
	    }
	}
	if (is_changed) {
	    bu_log("changed\n");
	    if (j == cpath.size()-1) {
		// Changed, but a leaf - stays drawn
		if (changed_paths)
		    (*changed_paths).insert(path_hash);
		return 0;
	    }
	    // Not a leaf - check child
	    parent_changed = true;
	    if (changed_paths)
		(*changed_paths).insert(path_hash);
	    continue;
	}

	// If we got here, reset the parent changed flag
	parent_changed = false;
    }

    // If we got through the whole path and leaf check is enabled, check if the
    // leaf of the path is a comb.  If it is, the presumption is that this path
    // is part of the active set because it is an evaluated solid, and we will
    // have to check its tree to see if anything below it changed.
    if (leaf_expand) {
	std::vector<unsigned long long> pitems = cpath;
	unsigned long long lhash = pitems[pitems.size() - 1];
	pitems.pop_back();
	int ret = leaf_check(lhash, pitems);
	if (ret == 3 && changed_paths) {
	    (*changed_paths).insert(path_hash);
	}
    }

    return 0;
}

void
BViewState::add_path(const char *path)
{
    if (!path)
	return;

    std::vector<unsigned long long> path_hashes = dbis->digest_path(path);
    add_hpath(path_hashes);
}

void
BViewState::add_hpath(std::vector<unsigned long long> &path_hashes)
{
    if (!path_hashes.size())
	return;
    staged.push_back(path_hashes);
}

void
BViewState::erase_path(int mode, int argc, const char **argv)
{
    if (!argc || !argv)
	return;

    std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator sm_it;
    for (int i = 0; i < argc; i++) {
	std::vector<unsigned long long> path_hashes = dbis->digest_path(argv[i]);
	if (!path_hashes.size())
	    continue;
	unsigned long long c_hash = path_hashes[path_hashes.size() - 1];
	path_hashes.pop_back();
	erase_hpath(mode, c_hash, path_hashes, false);
    }

    // Update info AFTER all paths are fully drawn
    cache_collapsed();
}

void
BViewState::erase_hpath(int mode, unsigned long long c_hash, std::vector<unsigned long long> &path_hashes, bool cache_collapse)
{
    std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator sm_it;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator m_it;
    pc_it = dbis->p_c.find(c_hash);

    path_hashes.push_back(c_hash);

    /* Cyclic - wasn't anything to draw */
    if (path_addition_cyclic(path_hashes))
	return;

    /* For some modes it's possible for combs to have evaluated objects, so
     * check regardless of whether c_hash is a solid */
    if (mode == 3 || mode == 5 || pc_it == dbis->p_c.end()) {
	unsigned long long phash = dbis->path_hash(path_hashes, 0);
	sm_it = s_map.find(phash);
	if (sm_it != s_map.end()) {
	    std::unordered_map<int, struct bv_scene_obj *>::iterator s_it;
	    if (mode < 0) {
		for (s_it = sm_it->second.begin(); s_it != sm_it->second.end(); s_it++)
		    bv_obj_put(s_it->second);
		for (m_it = drawn_paths.begin(); m_it != drawn_paths.end(); m_it++)
		    m_it->second.erase(phash);
		s_map.erase(phash);
	    } else {
		s_it = sm_it->second.find(mode);
		if (s_it != sm_it->second.end()) {
		    bv_obj_put(s_it->second);
		    sm_it->second.erase(s_it);
		    drawn_paths[mode].erase(phash);
		    s_map[phash].erase(mode);
		}
	    }

	    // In case phash is now gone from s_map check again
	    sm_it = s_map.find(phash);

	    // IFF we have removed all of the drawn elements for this path,
	    // clear it from the active sets
	    if (sm_it == s_map.end() || !sm_it->second.size()) {
		s_keys.erase(phash);
		all_drawn_paths.erase(phash);
	    }
	}
    }

    /* If we do have a comb, keep going */
    if (pc_it != dbis->p_c.end()) {
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
	    erase_hpath(mode, *c_it, path_hashes, false);
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();

    // Update info on drawn paths
    if (cache_collapse)
	cache_collapsed();
}

unsigned long long
BViewState::path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    return dbis->path_hash(path, max_len);
}

void
BViewState::depth_group_collapse(
	std::vector<std::vector<unsigned long long>> &collapsed,
	std::unordered_set<unsigned long long> &d_paths,
	std::unordered_set<unsigned long long> &p_d_paths,
	std::map<size_t, std::unordered_set<unsigned long long>> &depth_groups
	)
{
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
	std::unordered_set<unsigned long long>::iterator s_it;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> &pc_path = s_keys[*s_it];
	    unsigned long long ppathhash = dbis->path_hash(pc_path, plen - 1);
	    grouped_pckeys[ppathhash].insert(*s_it);
	    pcomb[ppathhash] = pc_path[plen-2];
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pg_it;
	for (pg_it = grouped_pckeys.begin(); pg_it != grouped_pckeys.end(); pg_it++) {

	    unsigned long long cpkey = *pg_it->second.begin();
	    std::vector<unsigned long long> check_path = s_keys[cpkey];
	    check_path.pop_back();

	    // As above, use the full path from the s_keys, but this time
	    // we're collecting the children.  This is the set we need to compare
	    // against the .g ground truth to determine fully or partially drawn.
	    std::unordered_set<unsigned long long> g_children;
	    std::unordered_set<unsigned long long> &g_pckeys = pg_it->second;
	    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		std::vector<unsigned long long> &pc_path = s_keys[*s_it];
		g_children.insert(pc_path[plen-1]);
	    }

	    // Do the check against the .g comb children info - the "ground truth"
	    // that defines what must be present for a fully drawn comb
	    bool is_fully_drawn = true;
	    std::unordered_set<unsigned long long> &ground_truth = dbis->p_c[pcomb[pg_it->first]];
	    for (s_it = ground_truth.begin(); s_it != ground_truth.end(); s_it++) {
	    }
	    for (s_it = ground_truth.begin(); s_it != ground_truth.end(); s_it++) {
		if (g_children.find(*s_it) == g_children.end()) {
		    is_fully_drawn = false;
		    break;
		}
	    }

	    // All the sub-paths in this grouping are fully drawn, whether
	    // or not they define a fully drawn parent, so stash their
	    // hashes
	    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		std::vector<unsigned long long> &path_hashes = s_keys[*s_it];
		unsigned long long thash = dbis->path_hash(path_hashes, plen);
		d_paths.insert(thash);
	    }

	    if (is_fully_drawn) {
		// If fully drawn, depth_groups[plen-1] gets the first path in
		// g_pckeys.  The path is longer than that depth, but contains
		// all the necessary information and using that approach avoids
		// the need to duplicate paths.
		depth_groups[plen - 1].insert(*g_pckeys.begin());
	    } else {
		// No further collapsing - add to final.  We must make trimmed
		// versions of the paths in case this depth holds promoted
		// paths from deeper levels, since we are duplicating the full
		// path contents.
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> trimmed = s_keys[*s_it];
		    trimmed.resize(plen);
		    collapsed.push_back(trimmed);

		    // Because we're not collapsing further, any paths above this
		    // path can be considered partially drawn.
		    while (trimmed.size() - 1) {
			trimmed.pop_back();
			unsigned long long thash = dbis->path_hash(trimmed, 0);
			p_d_paths.insert(thash);
		    }
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
	std::unordered_set<unsigned long long> &pckeys = depth_groups[1];
	std::unordered_set<unsigned long long>::iterator s_it;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> trimmed = s_keys[*s_it];
	    trimmed.resize(1);
	    collapsed.push_back(trimmed);
	    unsigned long long thash = dbis->path_hash(trimmed, 0);
	    d_paths.insert(thash);
	}
    }
}

void
BViewState::cache_collapsed()
{
    // Group drawn paths by drawing mode type
    std::unordered_map<int, std::unordered_set<unsigned long long>> mode_map;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    for (sk_it = s_keys.begin(); sk_it != s_keys.end(); sk_it++) {
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator s_it;
	s_it = s_map.find(sk_it->first);
	if (s_it == s_map.end())
	    continue;
	std::unordered_map<int, struct bv_scene_obj *>::iterator sm_it;
	for (sm_it = s_it->second.begin(); sm_it != s_it->second.end(); sm_it++)
	    mode_map[sm_it->first].insert(sk_it->first);
    }

    // Collapse each drawing mode until the leaf is a changed dp or not fully
    // drawn.  We must do this before the comb p_c relationships are updated in
    // the context, since we want the answers for this collapse to be from the
    // prior state, not the current state.

    // Reset containers
    mode_collapsed.clear();
    drawn_paths.clear();
    partially_drawn_paths.clear();

    // Each mode is initially processed separately, so we maintain
    // awareness of the drawing state in various modes
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator mm_it;
    for (mm_it = mode_map.begin(); mm_it != mode_map.end(); mm_it++) {

	std::map<size_t, std::unordered_set<unsigned long long>> depth_groups;
	std::unordered_set<unsigned long long> &mode_keys = mm_it->second;;
	std::unordered_set<unsigned long long>::iterator ms_it;

	// Group paths of the same depth.  Depth == 1 paths are already
	// top level objects and need no further processing.
	for (ms_it = mode_keys.begin(); ms_it != mode_keys.end(); ms_it++) {
	    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
	    k_it = s_keys.find(*ms_it);
	    if (k_it->second.size() == 1) {
		mode_collapsed[mm_it->first].push_back(k_it->second);
	        unsigned long long dhash = dbis->path_hash(k_it->second, 0);
		drawn_paths[mm_it->first].insert(dhash);
	    } else {
		depth_groups[k_it->second.size()].insert(k_it->first);
	    }
	}

	depth_group_collapse(mode_collapsed[mm_it->first], drawn_paths[mm_it->first], partially_drawn_paths[mm_it->first], depth_groups);
    }


    // Having processed all the modes, we now do the same thing without regards to
    // drawing mode, to provide "who" with a mode-agnostic list of drawn paths.
    all_collapsed.clear();
    std::map<size_t, std::unordered_set<unsigned long long>> all_depth_groups;
    for (mm_it = mode_map.begin(); mm_it != mode_map.end(); mm_it++) {
	// Group paths of the same depth.  Depth == 1 paths are already
	// top level objects and need no further processing.
	std::unordered_set<unsigned long long> &mode_keys = mm_it->second;;
	std::unordered_set<unsigned long long>::iterator ms_it;
	for (ms_it = mode_keys.begin(); ms_it != mode_keys.end(); ms_it++) {
	    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
	    k_it = s_keys.find(*ms_it);
	    if (k_it->second.size() == 1) {
		all_collapsed.push_back(k_it->second);
	        unsigned long long dhash = dbis->path_hash(k_it->second, 0);
		all_drawn_paths.insert(dhash);
	    } else {
		// Populate mode-agnostic container
		all_depth_groups[k_it->second.size()].insert(k_it->first);
	    }
	}
    }

    all_drawn_paths.clear();
    all_partially_drawn_paths.clear();
    depth_group_collapse(all_collapsed, all_drawn_paths, all_partially_drawn_paths, all_depth_groups);
}

struct bv_scene_obj *
BViewState::scene_obj(
	std::unordered_set<struct bv_scene_obj *> &objs,
	int curr_mode,
	struct bv_obj_settings *vs,
	matp_t m,
       	std::vector<unsigned long long> &path_hashes,
	std::unordered_set<struct bview *> &views,
	struct bview *v
	)
{
    // Solid - scene object time
    unsigned long long phash = dbis->path_hash(path_hashes, 0);
    std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator sm_it;
    sm_it = s_map.find(phash);
    struct bv_scene_obj *sp = NULL;
    if (sm_it != s_map.end()) {

	// If we have user supplied settings, we need to do some checking
	if (vs && !vs->mixed_modes) {
	    // If we're not allowed to mix modes, we need to erase any modes
	    // that don't match the current mode
	    std::vector<unsigned long long> phashes = path_hashes;
	    if (phashes.size()) {
		unsigned long long c_hash = phashes[phashes.size() - 1];
		phashes.pop_back();
		std::unordered_set<int> erase_modes;
		std::unordered_map<int, struct bv_scene_obj *>::iterator s_it;
		for (s_it = sm_it->second.begin(); s_it != sm_it->second.end(); s_it++) {
		    if (s_it->first == curr_mode)
			continue;
		    erase_modes.insert(s_it->first);
		}
		std::unordered_set<int>::iterator e_it;
		for (e_it = erase_modes.begin(); e_it != erase_modes.end(); e_it++) {
		    erase_hpath(*e_it, c_hash, phashes, false);
		}
	    }
	}

	if (s_map[phash].find(curr_mode) != s_map[phash].end()) {
	    // Already have scene object - check it against vs
	    // settings to see if we need to update
	    sp = s_map[phash][curr_mode];
	    if (vs && vs->s_dmode == curr_mode) {
		if (sp->s_soldash && vs->draw_non_subtract_only) {
		    if (sp->s_flag != DOWN)
			sp->s_flag = DOWN;
		} else {
		    if (sp->s_flag != UP)
			sp->s_flag = UP;
		}
		if (bv_obj_settings_sync(sp->s_os, vs))
		    objs.insert(sp);
	    }

	    // Most view setting changes won't alter geometry, and adaptive
	    // drawing updating is handled via callbacks.  However, adaptive
	    // plotting enablement/disablement changes which type of objects
	    // we need.  Make sure we're synced.
	    std::unordered_set<struct bview *>::iterator v_it;
	    if (sp->csg_obj) {
		for (v_it = views.begin(); v_it != views.end(); v_it++) {
		    int have_adaptive = bv_obj_have_vo(sp, *v_it);
		    if ((*v_it)->gv_s->adaptive_plot_csg && !have_adaptive) {
			bv_obj_stale(sp);
			sp->curve_scale = -1; // Make sure a rework is triggered
			objs.insert(sp);
		    }
		    if (!(*v_it)->gv_s->adaptive_plot_csg && have_adaptive && bv_clear_view_obj(sp, *v_it)) {
			bv_obj_stale(sp);
			objs.insert(sp);
		    }
		}
	    }
	    if (sp->mesh_obj) {
		for (v_it = views.begin(); v_it != views.end(); v_it++) {
		    int have_adaptive = bv_obj_have_vo(sp, *v_it);
		    if ((*v_it)->gv_s->adaptive_plot_mesh && !have_adaptive) {
			bv_obj_stale(sp);
			objs.insert(sp);
		    }
		    if (!(*v_it)->gv_s->adaptive_plot_mesh && have_adaptive && bv_clear_view_obj(sp, *v_it)) {
			bv_obj_stale(sp);
			objs.insert(sp);
		    }
		}
	    }

	    return NULL;
	}
    }

    // No pre-existing object - make a new one
    sp = bv_obj_get(v, BV_DB_OBJS);

    // Find the leaf directory pointer
    struct directory *dp = dbis->get_hdp(path_hashes[path_hashes.size()-1]);
    if (!dp) {
	bu_log("dbi_state.cpp:%d - dp lookup failed!\n", __LINE__);
	return NULL;
    }

    // Prepare draw data
    struct rt_wdb *wdbp = wdb_dbopen(dbis->gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct draw_update_data_t *ud;
    BU_GET(ud, struct draw_update_data_t);
    ud->dbip = dbis->gedp->dbip;
    ud->tol = &wdbp->wdb_tol;
    ud->ttol = &wdbp->wdb_ttol;
    ud->res = &rt_uniresource; // TODO - at some point this may be from the app or view... local_res is temporary, don't use it here
    ud->mesh_c = dbis->gedp->ged_lod;
    sp->dp = dp;
    sp->s_i_data = (void *)ud;

    // Get color from path, unless we're overridden
    struct bu_color c;
    dbis->path_color(&c, path_hashes);
    bu_color_to_rgb_chars(&c, sp->s_color);
    if (vs && vs->color_override) {
	// TODO - shouldn't be using s_color for the override...
	sp->s_color[0] = vs->color[0];
	sp->s_color[1] = vs->color[1];
	sp->s_color[2] = vs->color[2];
    }

    // Set drawing mode
    sp->s_os->s_dmode = curr_mode;

    // Tell scene object what the current matrix is
    if (m) {
	MAT_COPY(sp->s_mat, m);
    } else {
	dbis->get_path_matrix(sp->s_mat, path_hashes);
    }

    // Assign the bounding box (needed for pre-adaptive-plot
    // autoview)
    dbis->get_path_bbox(&sp->bmin, &sp->bmax, path_hashes);

    // Adaptive also needs s_size and s_center to be set
    sp->s_center[X] = (sp->bmin[X] + sp->bmax[X]) * 0.5;
    sp->s_center[Y] = (sp->bmin[Y] + sp->bmax[Y]) * 0.5;
    sp->s_center[Z] = (sp->bmin[Z] + sp->bmax[Z]) * 0.5;
    sp->s_size = sp->bmax[X] - sp->bmin[X];
    V_MAX(sp->s_size, sp->bmax[Y] - sp->bmin[Y]);
    V_MAX(sp->s_size, sp->bmax[Z] - sp->bmin[Z]);
    sp->have_bbox = 1;

    // If we're drawing a subtraction and we're not overridden, set the
    // appropriate flag for dashed line drawing
    if (vs && !vs->draw_solid_lines_only) {
	bool is_subtract = dbis->path_is_subtraction(path_hashes);
	sp->s_soldash = (is_subtract) ? 1 : 0;
    }

    // Set line width, if the user specified a non-default value
    if (vs && vs->s_line_width)
	sp->s_os->s_line_width = vs->s_line_width;

    // Set transparency
    if (vs)
	sp->s_os->transparency = vs->transparency;

    dbis->print_path(&sp->s_name, path_hashes);
    s_map[phash][sp->s_os->s_dmode] = sp;
    s_keys[phash] = path_hashes;

    // Final geometry generation is deferred - see draw_scene
    objs.insert(sp);

    return sp;
}


void
BViewState::walk_tree(
	std::unordered_set<struct bv_scene_obj *> &objs,
	unsigned long long chash,
	int curr_mode,
	struct bview *v,
	struct bv_obj_settings *vs,
	matp_t m,
       	std::vector<unsigned long long> &path_hashes,
	std::unordered_set<struct bview *> &views,
	unsigned long long *ret
	)
{
    size_t op = OP_UNION;
    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>>::iterator b_it;
    b_it = dbis->i_bool.find(path_hashes[path_hashes.size() - 1]);
    if (b_it != dbis->i_bool.end()) {
	std::unordered_map<unsigned long long, size_t>::iterator bb_it;
	bb_it = b_it->second.find(chash);
	if (bb_it != b_it->second.end()) {
	    op = bb_it->second;
	}
    }

    if (op == OP_SUBTRACT && vs && vs->draw_solid_lines_only)
	return;

    mat_t lm;
    MAT_IDN(lm);
    //unsigned long long phash = path_hashes[path_hashes.size() - 1];
    //dbis->get_matrix(lm, phash, chash);

    gather_paths(objs, chash, curr_mode, v, vs, m, lm, path_hashes, views, ret);
}

// Note - by the time we are using gather_paths, any existing objects
// already defined are assumed to be updated/valid - only create
// missing objects.
void
BViewState::gather_paths(
	std::unordered_set<struct bv_scene_obj *> &objs,
	unsigned long long c_hash,
	int curr_mode,
	struct bview *v,
	struct bv_obj_settings *vs,
	matp_t m,
       	matp_t lm,
	std::vector<unsigned long long> &path_hashes,
	std::unordered_set<struct bview *> &views,
	unsigned long long *ret
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = dbis->p_c.find(c_hash);

    struct directory *dp = dbis->get_hdp(c_hash);
    path_hashes.push_back(c_hash);

    mat_t om, nm;
    /* Update current matrix state to reflect the new branch of
     * the tree. Either we have a local matrix, or we have an
     * implicit IDN matrix. */
    MAT_COPY(om, m);
    if (lm) {
	MAT_COPY(nm, lm);
    } else {
	MAT_IDN(nm);
    }
    bn_mat_mul(m, om, nm);

    if (pc_it != dbis->p_c.end()) {
	// Two things may prevent further processing of a comb - a hidden dp, or
	// a cyclic path.
	if (dp && !(dp->d_flags & RT_DIR_HIDDEN) && pc_it->second.size() && !path_addition_cyclic(path_hashes)) {
	    /* Keep going */
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++) {
		walk_tree(objs, *c_it, curr_mode, v, vs, m, path_hashes, views, ret);
	    }
	} else {
	    // Comb without children - (empty) scene object time
	    scene_obj(objs, curr_mode, vs, m, path_hashes, views, v);
	}
    } else {
	// Solid - scene object time
	struct bv_scene_obj *nobj = scene_obj(objs, curr_mode, vs, m, path_hashes, views, v);
	if (nobj && ret)
	    (*ret) |= GED_DBISTATE_VIEW_CHANGE;
    }
    /* Done with branch - restore path, put back the old matrix state,
     * and restore previous color settings */
    path_hashes.pop_back();
    MAT_COPY(m, om);
}

void
BViewState::clear()
{
    s_map.clear();
    s_keys.clear();
    staged.clear();
    drawn_paths.clear();
    all_drawn_paths.clear();
    partially_drawn_paths.clear();
    mode_collapsed.clear();
    all_collapsed.clear();
}

std::vector<std::string>
BViewState::list_drawn_paths(int mode, bool list_collapsed)
{
    std::unordered_map<int, std::vector<std::vector<unsigned long long>>>::iterator m_it;
    std::vector<std::string> ret;
    if (mode == -1 && list_collapsed) {
	struct bu_vls vpath = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < all_collapsed.size(); i++) {
	    dbis->print_path(&vpath, all_collapsed[i]);
	    ret.push_back(std::string(bu_vls_cstr(&vpath)));
	}
    }
    if (mode != -1 && list_collapsed) {
	m_it = mode_collapsed.find(mode);
	if (m_it == mode_collapsed.end())
	    return ret;
    	struct bu_vls vpath = BU_VLS_INIT_ZERO;
	for (size_t i = 0; i < m_it->second.size(); i++) {
	    dbis->print_path(&vpath, m_it->second[i]);
	    ret.push_back(std::string(bu_vls_cstr(&vpath)));
	}
    }
    if (mode == -1 && !list_collapsed) {
	struct bu_vls vpath = BU_VLS_INIT_ZERO;
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
	for (k_it = s_keys.begin(); k_it != s_keys.end(); k_it++) {
	    dbis->print_path(&vpath, k_it->second);
	    ret.push_back(std::string(bu_vls_cstr(&vpath)));
	}
    }
    if (mode != -1 && !list_collapsed) {
	struct bu_vls vpath = BU_VLS_INIT_ZERO;
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator sm_it;
	for (sm_it = s_map.begin(); sm_it != s_map.end(); sm_it++) {
	    if (sm_it->second.find(mode) == sm_it->second.end())
		continue;
	    dbis->print_path(&vpath, s_keys[sm_it->first]);
	    ret.push_back(std::string(bu_vls_cstr(&vpath)));
	}
    }

    std::sort(ret.begin(), ret.end(), &alphanum_cmp);

    return ret;
}

size_t
BViewState::count_drawn_paths(int mode, bool list_collapsed)
{
    std::unordered_map<int, std::vector<std::vector<unsigned long long>>>::iterator m_it;
    std::vector<std::string> ret;
    if (mode == -1 && list_collapsed)
	return all_collapsed.size();

    if (mode != -1 && list_collapsed) {
	m_it = mode_collapsed.find(mode);
	if (m_it == mode_collapsed.end())
	    return m_it->second.size();
	return 0;
    }

    if (mode == -1 && !list_collapsed)
	return s_keys.size();

    if (mode != -1 && !list_collapsed) {
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator sm_it;
	sm_it = s_map.find(mode);
	if (sm_it != s_map.end())
	    return sm_it->second.size();
	return 0;
    }

    return 0;
}

int
BViewState::is_hdrawn(int mode, unsigned long long phash)
{
    if (mode == -1) {
	if (all_drawn_paths.find(phash) != all_drawn_paths.end())
	    return 1;
	if (all_partially_drawn_paths.find(phash) != all_partially_drawn_paths.end())
	    return 2;
	return 0;
    }

    if (drawn_paths.find(mode) == drawn_paths.end())
	return 0;

    if (drawn_paths[mode].find(phash) != drawn_paths[mode].end())
	return 1;
    if (partially_drawn_paths[mode].find(phash) != partially_drawn_paths[mode].end())
	return 2;
    return 0;
}

unsigned long long
BViewState::refresh(struct bview *v, int argc, const char **argv)
{
    if (!v)
	return 0;

    bv_log(1, "BViewState::refresh");
    // We (well, callers) need to be able to tell if the redraw pass actually
    // changed anything.
    unsigned long long ret = 0;

    // Make sure the view knows how to update the oriented bounding box
    v->gv_bounds_update = &bv_view_bounds;

    // If we have specific paths specified, the leaves of those paths
    // denote which paths need refreshing.  We need to process them
    // and turn them to hashes, so we can check the s_keys hash vectors
    // for the presence of "hashes of interest".
    //
    // Note - this is too aggressive, in that it will result in refreshing
    // of objects that have the leaf in their paths but don't match the
    // parent full path.  However, checking the full parent path is more
    // complicated without an n^2 order performance problem, so for the
    // moment we punt and use the more aggressive redraw solution.
    std::unordered_set<unsigned long long> active_hashes;
    for (int i = 0; i < argc; i++) {
	std::vector<unsigned long long> phashes = dbis->digest_path(argv[i]);
	active_hashes.insert(phashes[phashes.size() - 1]);
    }

    // Objects may be "drawn" in different ways - wireframes, shaded,
    // evaluated.  How they must be redrawn is mode dependent.
    std::unordered_map<int, std::unordered_set<unsigned long long>> mode_map;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    for (sk_it = s_keys.begin(); sk_it != s_keys.end(); sk_it++) {
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator s_it;
	s_it = s_map.find(sk_it->first);
	if (s_it == s_map.end())
	    continue;

	// If we have specified objects, we only refresh if the path
	// involves a hash of interest
	if (active_hashes.size()) {
	    int active = 0;
	    for (size_t i = 0; i < sk_it->second.size(); i++) {
		if (active_hashes.find(sk_it->second[i]) != active_hashes.end()) {
		    active = 1;
		    break;
		}
	    }
	    if (!active)
		continue;
	}

	std::unordered_map<int, struct bv_scene_obj *>::iterator sm_it;
	for (sm_it = s_it->second.begin(); sm_it != s_it->second.end(); sm_it++) {
	    mode_map[sm_it->first].insert(sk_it->first);
	}
    }

    // Redo drawing based on current db info - color, matrix, and geometry
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator mm_it;
    for (mm_it = mode_map.begin(); mm_it != mode_map.end(); mm_it++) {
	std::unordered_set<unsigned long long> &mkeys = mm_it->second;
	std::unordered_set<unsigned long long>::iterator k_it;
	for (k_it = mkeys.begin(); k_it != mkeys.end(); k_it++) {
	    std::vector<unsigned long long> &cp = s_keys[*k_it];
	    struct bv_scene_obj *s = NULL;
	    if (s_map.find(*k_it) != s_map.end()) {
		if (s_map[*k_it].find(mm_it->first) != s_map[*k_it].end())
		    s = s_map[*k_it][mm_it->first];
	    }
	    if (!s)
		continue;
	    struct bv_scene_obj *nso = bv_obj_get(v, BV_DB_OBJS);
	    bv_obj_sync(nso, s);
	    nso->s_i_data = s->s_i_data;
	    s->s_i_data = NULL;
	    s_map[*k_it].erase(mm_it->first);
	    ret = GED_DBISTATE_VIEW_CHANGE;

	    // print path name, set view - otherwise empty
	    dbis->print_path(&nso->s_name, cp);
	    nso->s_v = v;
	    nso->dp = s->dp;
	    s_map[*k_it][mm_it->first] = nso;

	    //bv_log(3, "refresh %s[%s]", bu_vls_cstr(&(nso->s_name)), bu_vls_cstr(&(v->gv_name)));
	    bu_log("refresh %s[%s]\n", bu_vls_cstr(&(nso->s_name)), bu_vls_cstr(&(v->gv_name)));
	    draw_scene(nso, v);
	    bv_obj_put(s);
	}
    }

    // Do selection sync
    BSelectState *ss = dbis->find_selected_state(NULL);
    if (ss) {
	ss->draw_sync();
	ret = GED_DBISTATE_VIEW_CHANGE;
    }

    return ret;
}

unsigned long long
BViewState::redraw(struct bv_obj_settings *vs, std::unordered_set<struct bview *> &views, int no_autoview)
{
    bv_log(1, "BViewState::redraw");
    // We (well, callers) need to be able to tell if the redraw pass actually
    // changed anything.
    unsigned long long ret = 0;

    if (!views.size())
	return 0;

    // Make sure the views know how to update the oriented bounding box
    std::unordered_set<struct bview *>::iterator v_it;
    for (v_it = views.begin(); v_it != views.end(); v_it++) {
	struct bview *v = *v_it;
	v->gv_bounds_update = &bv_view_bounds;
    }

    // For most operations on objects, we need only the current view (for
    // independent views) or a single instance of any representative view (for
    // shared state views).
    struct bview *v = NULL;
    if (views.size() == 1)
	v = (*(views.begin()));
    if (!v && views.size() > 1) {
	// If we have multiple views, we want a non-independent view
	for (v_it = views.begin(); v_it != views.end(); v_it++) {
	    struct bview *nv = *v_it;
	    if (nv->independent)
		continue;
	    v = nv;
	    break;
	}
    }

    // The principle for redrawing will be that anything that was previously
    // fully drawn should stay fully drawn, even if its tree structure has
    // changed.
    //
    // In order to accommodate autoview requirements, final geometry
    // drawing has to be delayed until after the initial scene objects are
    // created.  Make a set to track which objects we need to draw in the
    // finalization stage.
    std::unordered_set<struct bv_scene_obj *> objs;


    // First order of business is to go through already drawn solids, if any,
    // and remove no-longer-valid paths. Keep still-valid paths to avoid the
    // work of re-generating the scene objects.
    std::unordered_set<unsigned long long> invalid_paths;
    std::unordered_set<unsigned long long> changed_paths;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    for (sk_it = s_keys.begin(); sk_it != s_keys.end(); sk_it++) {
	// Work down from the root of each path looking for the first changed or
	// removed entry.
	std::vector<unsigned long long> &cpath = sk_it->second;
	check_status(&invalid_paths, &changed_paths, sk_it->first, cpath, true);
    }

    // Invalid path objects we remove completely
    std::unordered_set<unsigned long long>::iterator iv_it;
    for (iv_it = invalid_paths.begin(); iv_it != invalid_paths.end(); iv_it++) {
	std::vector<unsigned long long> &phashes = s_keys[*iv_it];
	if (!phashes.size())
	    continue;
	unsigned long long c_hash = phashes[phashes.size() - 1];
	phashes.pop_back();
	erase_hpath(-1, c_hash, phashes, false);
    }

    // Objects may be "drawn" in different ways - wireframes, shaded, evaluated.
    // How they must be redrawn in the event of a database change is mode dependent,
    // so after removing the invalid paths we categorize active paths according to
    // which modes they are being visualized with.
    std::unordered_map<int, std::unordered_set<unsigned long long>> mode_map;
    for (sk_it = s_keys.begin(); sk_it != s_keys.end(); sk_it++) {
	std::unordered_map<unsigned long long, std::unordered_map<int, struct bv_scene_obj *>>::iterator s_it;
	s_it = s_map.find(sk_it->first);
	if (s_it == s_map.end())
	    continue;
	std::unordered_map<int, struct bv_scene_obj *>::iterator sm_it;
	for (sm_it = s_it->second.begin(); sm_it != s_it->second.end(); sm_it++) {
	    mode_map[sm_it->first].insert(sk_it->first);
	}
    }

    // Changed paths we redo based on current db info - color, matrix, and
    // geometry if the entry isn't invalid.  This is the step that ensures any
    // surviving solid objects in the drawing state are current for subsequent
    // operations (and thus valid to reuse)
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator mm_it;
    for (mm_it = mode_map.begin(); mm_it != mode_map.end(); mm_it++) {
	for (iv_it = changed_paths.begin(); iv_it != changed_paths.end(); iv_it++) {
	    if (mm_it->second.find(*iv_it) == mm_it->second.end())
		continue;
	    std::vector<unsigned long long> &cp = s_keys[*iv_it];
	    struct bv_scene_obj *s = NULL;
	    if (s_map.find(*iv_it) != s_map.end()) {
		if (s_map[*iv_it].find(mm_it->first) != s_map[*iv_it].end()) {
		    ret = GED_DBISTATE_VIEW_CHANGE;
		    s = s_map[*iv_it][mm_it->first];
		}
	    }
	    if (dbis->invalid_entry_map.find(cp[cp.size() - 1]) != dbis->invalid_entry_map.end()) {
		if (s) {
		    // Invalid - remove any scene object geometry
		    ret = GED_DBISTATE_VIEW_CHANGE;
		    bv_obj_reset(s);
		    s->s_v = v;
		} else {
		    s = bv_obj_get(v, BV_DB_OBJS);
		    // print path name, set view - otherwise empty
		    dbis->print_path(&s->s_name, cp);
		    s->s_v = v;
		    s_map[*iv_it][mm_it->first] = s;
		}
		continue;
	    }
	    if (s) {
		// Geometry is suspect - clear to prepare for regeneration
		bv_obj_put(s);
		s_map[*iv_it].erase(mm_it->first);
		ret = GED_DBISTATE_VIEW_CHANGE;
	    }
	}
    }

    // Evaluate prior collapsed paths according to the same validity criteria,
    // then re-expand them
    std::unordered_map<int, std::vector<std::vector<unsigned long long>>>::iterator ms_it;
    for (ms_it = mode_collapsed.begin(); ms_it != mode_collapsed.end(); ms_it++) {
	std::unordered_set<size_t> active_collapsed;
	std::unordered_set<size_t> draw_invalid_collapsed;
	for (size_t i = 0; i < ms_it->second.size(); i++) {
	    std::vector<unsigned long long> &cpath = ms_it->second[i];
	    int sret = check_status(NULL, NULL, 0, cpath, true);
	    if (sret == 2)
		draw_invalid_collapsed.insert(i);
	    if (sret == 0)
		active_collapsed.insert(i);
	}

	// Expand active collapsed paths to solids, creating any missing path objects
	//
	// NOTE:  We deliberately do NOT pass the supplied vs (if any) to these scene_obj/
	// gather_paths calls - user specified override settings should be applied only to
	// staged paths specified by the user.  Pre-existing geometry NOT specified by
	// those commands does not get those settings applied during redraw.
	std::unordered_set<size_t>::iterator sz_it;
	for (sz_it = active_collapsed.begin(); sz_it != active_collapsed.end(); sz_it++) {
	    std::vector<unsigned long long> cpath = ms_it->second[*sz_it];
	    mat_t m;
	    MAT_IDN(m);
	    dbis->get_path_matrix(m, cpath);
	    if (ms_it->first == 3 || ms_it->first == 5) {
		dbis->get_path_matrix(m, cpath);
		scene_obj(objs, ms_it->first, NULL, m, cpath, views, v);
		continue;
	    }
	    unsigned long long ihash = cpath[cpath.size() - 1];
	    cpath.pop_back();
	    gather_paths(objs, ihash, ms_it->first, v, NULL, m, NULL, cpath, views, &ret);
	}
	for (sz_it = draw_invalid_collapsed.begin(); sz_it != draw_invalid_collapsed.end(); sz_it++) {
	    std::vector<unsigned long long> cpath = ms_it->second[*sz_it];
	    struct bv_scene_obj *s = bv_obj_get(v, BV_DB_OBJS);
	    // print path name, set view - otherwise empty
	    dbis->print_path(&s->s_name, cpath);
	    s->s_v = v;
	    s_map[ms_it->first][*iv_it] = s;

	    // NOTE: Because there is no geometry to update, these scene objs
	    // are not added to objs
	    bu_log("invalid expand\n");
	}
    }

    // Expand (or queue, depending on settings) any staged paths.
    if (vs) {
	for (size_t i = 0; i < staged.size(); i++) {
	    std::vector<unsigned long long> cpath = staged[i];
	    // Validate this path - if the user has specified an invalid
	    // path, there's nothing else to do
	    if (!dbis->valid_hash_path(cpath))
		continue;
	    unsigned long long phash = dbis->path_hash(cpath, 0);
	    if (check_status(NULL, NULL, phash, cpath, false))
		continue;
	    mat_t m;
	    MAT_IDN(m);
	    dbis->get_path_matrix(m, cpath);
	    if ((vs->s_dmode == 3 || vs->s_dmode == 5)) {
		dbis->get_path_matrix(m, cpath);
		scene_obj(objs, vs->s_dmode, vs, m, cpath, views, v);
		continue;
	    }
	    unsigned long long ihash = cpath[cpath.size() - 1];
	    cpath.pop_back();
	    gather_paths(objs, ihash, vs->s_dmode, v, vs, m, NULL, cpath, views, &ret);
	}
    }
    // Staged paths are now added (as long as settings were supplied) - clear the queue
    staged.clear();

    // Do a preliminary autoview, unless suppressed, so any adaptive plotting
    // routines have a rough idea of the correct dimensions to use
    if (!no_autoview) {
	for (v_it = views.begin(); v_it != views.end(); v_it++) {
	    bv_autoview(*v_it, BV_AUTOVIEW_SCALE_DEFAULT, 0);
	}
    }

    // Update geometry.  draw_scene will avoid repeat creation of geometry
    // when s is not adaptive, but if s IS adaptive we need unique geometry
    // for each view even though the BViewState is shared - camera settings,
    // which are unique to each bview, may differ and adaptive geometry must
    // reflect that.
    //
    // Note that this is the ONLY situation where we must care about each
    // view individually for shared state - the above uses of the first view
    // work for the "top level" object used for adaptive cases, since shared
    // views will be using a shared object pool for anything other than their
    // view specific geometry sub-objects.
    for (v_it = views.begin(); v_it != views.end(); v_it++) {
	std::unordered_set<struct bv_scene_obj *>::iterator o_it;
	for (o_it = objs.begin(); o_it != objs.end(); o_it++) {
	    bv_log(3, "redraw %s[%s]", bu_vls_cstr(&((*(*o_it)).s_name)), bu_vls_cstr(&((*(*v_it)).gv_name)));
	    draw_scene(*o_it, *v_it);
	}
    }

    // We need to check if any drawn solids are selected.  If so, we need
    // to illuminate them.  This is what ensures that newly drawn solids
    // respect a previously selected set from the command line
    BSelectState *ss = dbis->find_selected_state(NULL);
    if (ss) {
	if (invalid_paths.size() || changed_paths.size()) {
	    ss->refresh();
	    ss->collapse();
	}
	ss->draw_sync();
	ret = GED_DBISTATE_VIEW_CHANGE;
    }
    // Now that we have the finalized geometry, do a finishing autoview,
    // unless suppressed
    if (!no_autoview) {
	for (v_it = views.begin(); v_it != views.end(); v_it++) {
	    bv_autoview(*v_it, BV_AUTOVIEW_SCALE_DEFAULT, 0);
	}
    }

    // Now that all path manipulations are finalized, update the
    // sets of drawn paths
    cache_collapsed();

    return ret;
}

void
BViewState::print_view_state(struct bu_vls *outvls)
{
    struct bu_vls *o = outvls;
    if (!o) {
	BU_GET(o, struct bu_vls);
	bu_vls_init(o);
    }

    bu_vls_printf(o, "Object count: %zd\n", s_keys.size());
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = s_keys.begin(); k_it != s_keys.end(); k_it++) {
	std::vector<unsigned long long> &path = k_it->second;
	struct bu_vls pstr = BU_VLS_INIT_ZERO;
	dbis->print_path(&pstr, path, 0, 1);
	bu_vls_printf(o, "%s\n", bu_vls_cstr(&pstr));
	bu_vls_free(&pstr);
    }

    if (o != outvls) {
	bu_vls_free(o);
	BU_PUT(o, struct bu_vls);
    }
}

// Added dps present their own challenge, in terms of whether or not to
// automatically draw them.  (I think this decision comes after the existing
// draw paths' removed/changed processing and the main .g reflecting maps are
// updated.)  The cases:
//
// 1.  Already part of a drawn invalid path - draw and expand, as path was
// drawn but is no longer invalid
//
// 2.  Already part of a non-drawn invalid path - do not draw (? - could see
// a case for either behavior here, if the user wants to see the instances
// of the newly enabled part... this may have to be a user option)
//
// 2.  Not part of any path, pre or post removed/changed draw states (i.e.
// a tops object) - draw


/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
