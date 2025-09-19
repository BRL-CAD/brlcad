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

/* Handle selection status for various instances in the database */

BSelectState::BSelectState(DbiState *s)
{
    dbis = s;
}

bool
BSelectState::Select(const char *path, bool update)
{
    if (!path)
	return false;

    DbiPath *p = dbis->GetDbiPath(path);
    if (!p)
	return false;

    return Select(p->hash(), update);
}

bool
BSelectState::Select(unsigned long long hash, bool update)
{
    if (!hash)
	return false;

    // To select a path by hash, we need the Dbi state to
    // be able to decode it for us.
    DbiPath *p = dbis->GetDbiPath(hash);
    if (!p)
	return false;

    // If we're going to select this path, we need to clear out conflicting
    // paths.  We deliberately don't allow selection of multiple levels of a
    // single path, to avoid unexpected and unintuitive behaviors.  This means
    // we have to clear any selection that is either a superset of this path or
    // a child of it.
    dbis->RemovePathsBelow(&selected, hash);
    std::unordered_set<unsigned long long>::iterator p_it;
    for (p_it = p->parent_path_hashes.begin(); p_it != p->parent_path_hashes.end(); ++p_it) {
	selected.erase(*p_it);
    }

    // Add to selected set
    selected.insert(hash);

    // If we're not updating the other sets, we're done
    if (!update)
	return true;

    // We are updating - process the sets
    characterize();

    return true;
}

bool
BSelectState::DeSelect(const char *path, bool update)
{
    if (!path)
	return false;

    DbiPath *p = dbis->GetDbiPath(path);
    if (!p)
	return false;

    return DeSelect(p->hash(), update);
}

bool
BSelectState::DeSelect(unsigned long long hash, bool update)
{
    // Don't bother with any checking - valid or not, hash is out of the set
    selected.erase(hash);

    // If we're not updating, that's all
    if (!update)
	return true;

    // We are updating - process the sets
    characterize();

    return true;
}

bool
BSelectState::IsSelected(unsigned long long hash)
{
    if (!hash)
	return false;

    if (selected.find(hash) != selected.end())
	return true;

    return false;
}

bool
BSelectState::IsActivated(unsigned long long hash)
{
    if (!hash)
	return false;

    if (children.find(hash) != children.end())
	return true;

    return false;
}

bool
BSelectState::IsActiveParent(unsigned long long hash)
{
    if (!hash)
	return false;

    if (immediate_parents.find(hash) != immediate_parents.end())
	return true;

    return false;
}


bool
BSelectState::IsActiveAncestor(unsigned long long hash)
{
    if (!hash)
	return false;

    if (ancestors.find(hash) != ancestors.end())
	return true;

    return false;
}

void
BSelectState::Clear()
{
    selected.clear();
    children.clear();
    immediate_parents.clear();
    ancestors.clear();
}

std::vector<std::string>
BSelectState::FindSelectedPaths(const char *pattern)
{
    std::vector<std::string> ret;
    std::unordered_set<unsigned long long>::iterator s_it;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	DbiPath *p = dbis->GetDbiPath(*s_it);
	if (!p)
	    continue;
	std::string pstr = p->str();
	if (!pattern || !strlen(pattern) || !bu_path_match(pattern, pstr.c_str(), 0))
	    ret.push_back(p->str());
    }
    std::sort(ret.begin(), ret.end(), &alphanum_cmp);
    return ret;
}

void
BSelectState::characterize()
{
    children.clear();
    immediate_parents.clear();
    ancestors.clear();

    std::unordered_set<unsigned long long>::iterator s_it;
    for (s_it = selected.begin(); s_it != selected.end(); ++s_it) {
	DbiPath *p = dbis->GetDbiPath(*s_it);
	if (!p)
	    continue;
	dbis->AddPathsBelow(&children, *s_it);
	immediate_parents.insert(p->parent_hash);
	std::unordered_set<unsigned long long>::iterator pp_it;
	for (pp_it = p->parent_path_hashes.begin(); pp_it != p->parent_path_hashes.end(); ++pp_it) {
	    if (*pp_it == p->parent_hash)
		continue;
	    ancestors.insert(*pp_it);
	}
    }
}



void
BSelectState::Sync()
{
#if 0
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
#endif
}

unsigned long long
BSelectState::state_hash()
{
    std::unordered_set<unsigned long long>::iterator s_it;
    struct bu_data_hash_state *s = bu_data_hash_create();
    if (!s)
	return 0;
    for (s_it = selected.begin(); s_it != selected.end(); s_it++) {
	unsigned long long phash = *s_it;
	bu_data_hash_update(s, &phash, sizeof(unsigned long long));
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
