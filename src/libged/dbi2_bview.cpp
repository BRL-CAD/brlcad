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

BViewState::BViewState(DbiState *s, struct bview *ev)
{
    dbis = s;
    v = ev;
    if (!v) {
	BU_GET(v, struct bview);
	bv_init(v);
	local_v = true;
	bu_ptbl_ins(&s->gedp->ged_free_views, (long *)v);

	// Generate a view name that doesn't clash with any other added view
	bool have_name = true;
	int vcnt = 1;
	std::string vname = std::string("V") + std::to_string(vcnt);
	std::unordered_map<struct bview *, BViewState *>::iterator v_it;
	for (v_it = s->view_states.begin(); v_it != s->view_states.end(); ++v_it) {
	    if (vname == std::string(bu_vls_cstr(&v_it->first->gv_name)))
		have_name = false;
	}
	while (!have_name) {
	    vcnt++;
	    vname = std::string("V") + std::to_string(vcnt);
	    have_name = true;
	    for (v_it = s->view_states.begin(); v_it != s->view_states.end(); ++v_it) {
		if (vname == std::string(bu_vls_cstr(&v_it->first->gv_name)))
		    have_name = false;
	    }
	    if (vcnt == INT_MAX) {
		bu_log("Error generating view name\n");
		bu_vls_sprintf(&v->gv_name, "(NULL)");
		return;
	    }
	}
	bu_vls_sprintf(&v->gv_name, "%s", vname.c_str());

    } else {
	local_v = false;
    }

    // Local or not, v is a ged view
    bu_ptbl_ins_unique(&s->gedp->ged_views, (long *)v);

    dbis->view_states[v] = this;
}

BViewState::~BViewState()
{
    if (dbis)
	dbis->view_states.erase(v);
    if (local_v)
	BU_PUT(v, struct bview);

    bu_ptbl_free(&eobjs);
}

void
BViewState::Hash()
{
    view_hash = bv_hash(v);
    dmp_hash = 0;
    if (v->dm_hash)
	dmp_hash = (*v->dm_hash)(v->dmp);

    // Calculate the hash of the the timestamps of the scene objects, whether
    // they are from the dbi paths or view-only objects.  If anything was
    // changed, added or removed it should be reflected in the timestamp hash,
    // provided the object altering code updated it appropriately.
    struct bu_data_hash_state *oc = bu_data_hash_create();
    std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator e_it;
    for (e_it = s_expanded.begin(); e_it != s_expanded.end(); ++e_it) {
	std::unordered_set<unsigned long long>::iterator p_it;
	for (p_it = e_it->second.begin(); p_it != e_it->second.end(); ++p_it) {
	    DbiPath *p = dbis->GetDbiPath(*p_it);
	    if (!p)
		continue;
	    struct bv_scene_obj *so = p->SceneObj(e_it->first, this);
	    if (!so)
		continue;
	    bu_data_hash_update(oc, &so->timestamp, sizeof(unsigned long long));
	}
    }
    std::unordered_set<struct bv_scene_obj *>::iterator u_it;
    for (u_it = scene_objs.begin(); u_it != scene_objs.end(); ++u_it)
	bu_data_hash_update(oc, &(*u_it)->timestamp, sizeof(int64_t));

    if (l_viewobj) {
	for (u_it = l_viewobj->scene_objs.begin(); u_it != l_viewobj->scene_objs.end(); ++u_it)
	    bu_data_hash_update(oc, &(*u_it)->timestamp, sizeof(int64_t));
    }
    objs_hash = bu_data_hash_val(oc);
    bu_data_hash_destroy(oc);
}

bool
BViewState::Dirty()
{
    // If anything has changed according to the hashes, we need a new Render()
    if (view_hash != old_view_hash)
       	return true;
    if (dmp_hash != old_dmp_hash)
       	return true;
    if (objs_hash != old_objs_hash)
	return true;

    return false;
}

bool
BViewState::Empty()
{
    view_empty = true;

    if (drawn_paths.size())
	view_empty = false;

    if (scene_objs.size())
	view_empty = false;

    if (l_dbipath)
	if (l_dbipath->drawn_paths.size())
	    view_empty = false;

    if (l_viewobj)
	if (l_viewobj->scene_objs.size())
	    view_empty = false;

    return view_empty;
}

void
BViewState::Autoview(fastf_t factor, bool dbipaths, bool view_only)
{
    const struct bu_ptbl *sobjs = FindSceneObjsPtbl(NULL, dbipaths, view_only);
    bv_autoview(v, sobjs, factor);
}

const struct bu_ptbl *
BViewState::FindSceneObjsPtbl(const char *pattern, bool dbipaths, bool view_only)
{
    // NOTE - paths must be baked for this to provide the output
    // expected by callers...
    bu_ptbl_reset(&eobjs);
    std::vector<struct bv_scene_obj *> objs = FindSceneObjs(pattern, dbipaths, view_only);
    for (size_t i = 0; i < objs.size(); i++)
	bu_ptbl_ins(&eobjs, (long *)objs[i]);

    return (const struct bu_ptbl *)&eobjs;
}


std::vector<struct bv_scene_obj *>
BViewState::FindSceneObjs(const char *pattern, bool dbipaths, bool view_only)
{
    std::vector<struct bv_scene_obj *> objs;
    std::unordered_set<struct bv_scene_obj *>::iterator o_it;
    for (o_it = scene_objs.begin(); o_it != scene_objs.end(); ++o_it) {
	if (!pattern) {
	    objs.push_back(*o_it);
	} else {
	    if (!bu_path_match(pattern, bu_vls_cstr(&((*o_it)->s_name)), 0))
		objs.push_back(*o_it);
	}
    }

    if (dbipaths) {
	std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator e_it;
	for (e_it = s_expanded.begin(); e_it != s_expanded.end(); ++e_it) {
	    std::unordered_set<unsigned long long>::iterator h_it;
	    for (h_it = e_it->second.begin(); h_it != e_it->second.end(); ++h_it) {
		DbiPath *p = dbis->GetDbiPath(*h_it);
		struct bv_scene_obj *sobj = p->SceneObj(e_it->first, this);
		if (!sobj)
		    continue;
		if (!pattern) {
		    objs.push_back(sobj);
		} else {
		    if (!bu_path_match(pattern, bu_vls_cstr(&(sobj->s_name)), 0))
			objs.push_back(sobj);
		}
	    }
	}
    }

    if (view_only) {
	std::unordered_set<struct bv_scene_obj *>::iterator so_it;
	for (so_it = scene_objs.begin(); so_it != scene_objs.end(); ++so_it) {
	    struct bv_scene_obj *sobj = *so_it;
	    if (!pattern) {
		objs.push_back(sobj);
	    } else {
		if (!bu_path_match(pattern, bu_vls_cstr(&(sobj->s_name)), 0))
		    objs.push_back(sobj);
	    }
	}
    }

    return objs;
}


std::string
BViewState::Name()
{
    if (!v)
	return std::string("(NULL)");
    return std::string(bu_vls_cstr(&v->gv_name));
}

bool
BViewState::Link(BViewState *ls, bool view_objs)
{
    // No linking the default view to anything else
    if (UNLIKELY(this == dbis->default_view))
	return false;

    if (!ls)
	return false;

    if (!view_objs) {
	l_dbipath = ls;
    } else {
	l_viewobj = ls;
    }

    return true;
}

bool
BViewState::UnLink(BViewState *ls, bool view_objs)
{
    // No unlinking the default view from itself
    if (UNLIKELY(this == dbis->default_view))
	return false;

    if (!ls) {
	if (!view_objs) {
	    l_dbipath = NULL;
	} else {
	    l_viewobj = NULL;
	}
    } else {
	if (!view_objs) {
	    if (l_dbipath != ls)
		return false;
	    l_dbipath = NULL;
	} else {
	    if (l_viewobj != ls)
		return false;
	    l_viewobj = NULL;
	}
    }

    return true;
}

void
BViewState::AddPath(const char *path, unsigned int mode, struct bv_obj_settings *s_obj)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!path))
	return;

    // String to DbiPath.
    DbiPath p(dbis, path);
    if (!p.valid())
	return;

    AddPath(&p, mode, s_obj);
}

void
BViewState::AddPath(DbiPath *p, unsigned int mode, struct bv_obj_settings *s_obj)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!p))
	return;

    // Any explicitly added path needs a registered DbiPath.  If we don't
    // already have one, create it (we can't assume p is suitable for
    // registering, so we make our own.)
    std::unordered_map<unsigned long long, DbiPath *>::iterator dp_it;
    dp_it = dbis->dbi_paths.find(p->hash());
    DbiPath *np = NULL;
    if (dp_it == dbis->dbi_paths.end()) {
	np = dbis->GetDbiPath();
	*np = *p;
	dbis->dbi_paths[p->hash()] = np;
    } else {
	np = dp_it->second;
    }

    AddPath(np->hash(), mode, s_obj);
}

void
BViewState::AddPath(unsigned long long hash, unsigned int mode, struct bv_obj_settings *s_obj)
{
    std::unordered_map<unsigned long long, DbiPath *>::iterator dp_it;

    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!hash))
	return;

    // First, check if hash is a child of any currently drawn path in the
    // specified mode.  If so, it is a redundant specifier and we don't need to
    // proceed further.
    std::unordered_set<unsigned long long>::iterator s_it;
    int mkey = (int)mode;
    s_it = fully_drawn_paths[mkey].find(hash);
    if (s_it != fully_drawn_paths[mkey].end()) {
	// If we have specified settings, apply them
	if (s_obj) {
	    dp_it = dbis->dbi_paths.find(hash);
	    if (UNLIKELY(dp_it == dbis->dbi_paths.end()))
		return;
	    DbiPath *p = dp_it->second;
	    p->Read(s_obj, mkey);
	}
	return;
    }

    // To add a path, we need a DbiPath container registered.
    // Otherwise, the hash is just some random number.
    dp_it = dbis->dbi_paths.find(hash);
    if (dp_it == dbis->dbi_paths.end())
	return;
    DbiPath *p = dp_it->second;

    // If we have specified settings, assign them
    if (s_obj)
	p->Read(s_obj, mode);

    // Next, we need to see if this is a parent path for any of the paths
    // already in drawn_paths.  If so, it will be replacing them.  There is no
    // point in clearing them from the fully drawn paths set, since anything
    // already in that set will remain fully drawn.
    std::vector<unsigned long long> to_clear;
    for (s_it = drawn_paths[mode].begin(); s_it != drawn_paths[mode].end(); ++s_it) {
	DbiPath *cp = dbis->dbi_paths[*s_it];
	if (p->parent(*cp)) {
	    to_clear.push_back(*s_it);
	    // Any parents of this path DO need to come out of the
	    // partially_drawn_paths sets, since the overriding parent path may
	    // be much higher in the hierarchy.
	    for (s_it = cp->parent_path_hashes.begin(); s_it != cp->parent_path_hashes.end(); ++s_it) {
		partially_drawn_paths[-1].erase(*s_it);
		partially_drawn_paths[mode].erase(*s_it);
	    }
	}
    }

    // Done iterating, clear anything subsumed from drawn_paths.
    for (size_t i = 0; i < to_clear.size(); i++) {
	drawn_paths[mode].erase(to_clear[i]);
    }

    // From here on out, hash is now a fully drawn path.
    partially_drawn_paths[-1].erase(hash);
    partially_drawn_paths[mkey].erase(hash);
    fully_drawn_paths[-1].insert(hash);
    fully_drawn_paths[mkey].insert(hash);

    // All child paths are also considered fully drawn.  Even an evaluated mode
    // drawing is considered to be drawing the subtree (just not necessarily as
    // individual scene objects) so we update the lists for both all and the
    // individual mode in all cases.
    dbis->AddPathsBelow(&(fully_drawn_paths[-1]), hash);
    dbis->AddPathsBelow(&(fully_drawn_paths[mkey]), hash);

    // The parents (if any) are now partially drawn paths, since the
    // fully_drawn_paths check at the beginning of AddPath determined this path
    // wasn't a child of an already drawn path.
    for (s_it = p->parent_path_hashes.begin(); s_it != p->parent_path_hashes.end(); ++s_it) {
	partially_drawn_paths[-1].insert(*s_it);
	partially_drawn_paths[mkey].insert(*s_it);
    }

    // That takes care of staging the paths.  We do not process any scene
    // objects at this time.  Doing so requires a full ExpandPaths to populate
    // the leaf DbiPath instances and it is always possible there are more
    // add/remove paths operations to come before a graphical scene update is
    // needed.  (Some cases of RemovePaths are actually a case in point.)  We
    // don't want to do unnecessary work, so we defer those updates.
}

void
BViewState::RemovePath(const char *path, int mode, bool rebuild)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!path))
	return;

    // String to DbiPath.
    DbiPath p(dbis, path);
    if (!p.valid())
	return;

    RemovePath(&p, mode, rebuild);
}

void
BViewState::RemovePath(DbiPath *p, int mode, bool rebuild)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!p))
	return;

    RemovePath(p->hash(), mode, rebuild);
}

void
BViewState::RemovePath(unsigned long long hash, int mode, bool rebuild)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return;

    if (UNLIKELY(!hash))
	return;

    // For this to work correctly, we will need a DbiPath container
    // registered for hash
    std::unordered_map<unsigned long long, DbiPath *>::iterator dp_it;
    dp_it = dbis->dbi_paths.find(hash);
    if (dp_it == dbis->dbi_paths.end())
	return;
    DbiPath *p = dp_it->second;

    // The removal case is somewhat trickier than the add case, in that
    // both parent and child relationships with existing drawn_paths have
    // work to do.  If hash is a parent of an existing s_path it needs
    // to be removed - and there may be multiple such.  If hash is a child
    // of an existing path, then it must be split and the surviving un-erased
    // children added back into the drawn set.
    //
    // The default behavior for an erase command is to erase the supplied path
    // for ALL active modes, so unless we've been instructed to focus on a
    // single mode we have to do this for every mode separately.  Each mode may
    // have completely disjoint active elements (perhaps not a common use case
    // but by no means impossible) so each must be handled on its own.
    std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator ss_it;
    for (ss_it = drawn_paths.begin(); ss_it != drawn_paths.end(); ++ss_it) {
	if (mode != (int)(ss_it->first) && mode != -1)
	    continue;
	std::unordered_set<unsigned long long>::iterator s_it;
	std::vector<unsigned long long> to_clear;  // hash is parent path
	std::vector<unsigned long long> to_split;  // hash is child path
	for (s_it = ss_it->second.begin(); s_it != ss_it->second.end(); ++s_it) {
	    DbiPath *cp = dbis->dbi_paths[*s_it];
	    if (*cp == *p || p->parent(*cp))
		to_clear.push_back(*s_it);
	    if (p->child(*cp))
		to_split.push_back(*s_it);
	}

	// Scrub fully cleared paths
	for (size_t i = 0; i < to_clear.size(); i++) {
	    DbiPath *cp = dbis->dbi_paths[to_clear[i]];
	    ss_it->second.erase(to_clear[i]);

	    // Get this path out of the support containers
	    fully_drawn_paths[ss_it->first].erase(to_clear[i]);
	    dbis->RemovePathsBelow(&(fully_drawn_paths[ss_it->first]), to_clear[i]);

	    // Any parents of the erased path need to come out of the
	    // partially_drawn_paths sets as well, since the source path for
	    // them is being *fully* erased.
	    for (s_it = cp->parent_path_hashes.begin(); s_it != cp->parent_path_hashes.end(); ++s_it)
		partially_drawn_paths[ss_it->first].erase(*s_it);
	}

	// Splitting is the hard case.  We will remove our current path but add all
	// of the subpaths that don't match the one we were told to remove.  To
	// handle this, we expand the existing parent path to its leaves, remove
	// any that have hash as a parent, collapse the remainder, and add them.
	//
	// There is no point in removing anything from partially_drawn_paths, since
	// everything that was partially drawn before the split will still be partially
	// drawn - MORE things will be partially drawn (and different things fully drawn)
	// but that will be handled by the AddPath calls.
	for (size_t i = 0; i < to_split.size(); i++) {
	    DbiPath *cp = dbis->dbi_paths[to_clear[i]];

	    // The split path is no longer fully drawn
	    ss_it->second.erase(to_split[i]);
	    fully_drawn_paths[ss_it->first].erase(to_split[i]);
	    dbis->RemovePathsBelow(&(fully_drawn_paths[ss_it->first]), to_split[i]);

	    // Find the remaining children
	    std::vector<unsigned long long> seed;
	    seed.push_back(to_clear[i]);
	    std::vector<unsigned long long> exp_paths = dbis->ExpandPaths(seed);
	    std::vector<unsigned long long> to_collapse;
	    for (size_t j = 0; j < exp_paths.size(); j++) {
		// We needed ExpandPaths to create DbiPaths so we could do this check
		DbiPath *ep = dbis->dbi_paths[exp_paths[j]];
		if (!cp->parent(*ep))
		    to_collapse.push_back(exp_paths[j]);
	    }
	    // We also needed CollapsePaths to create DbiPaths, since we will be calling
	    // AddPath on the results.
	    std::vector<unsigned long long> collapsed_paths = dbis->CollapsePaths(to_collapse);
	    for (size_t j = 0; j < collapsed_paths.size(); j++)
		AddPath(collapsed_paths[j], ss_it->first);
	}
    }

    // Need to fully rebuild fully_drawn_paths and partially_drawn_paths, since
    // we don't know if a removal of a single mode from a path was enough to
    // completely remove it from all drawing.  Just iterate over the modes and
    // insert everything, no need to rewalk the trees.
    if (rebuild) {
	std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator iss_it;
	fully_drawn_paths[-1].clear();
	for (iss_it = fully_drawn_paths.begin(); iss_it != fully_drawn_paths.end(); ++iss_it) {
	    if (iss_it->first == -1)
		continue;
	    std::unordered_set<unsigned long long>::iterator s_it;
	    for (s_it = iss_it->second.begin(); s_it != iss_it->second.end(); ++s_it)
		fully_drawn_paths[-1].insert(*s_it);
	}
	partially_drawn_paths[-1].clear();
	for (iss_it = partially_drawn_paths.begin(); iss_it != partially_drawn_paths.end(); ++iss_it) {
	    if (iss_it->first == -1)
		continue;
	    std::unordered_set<unsigned long long>::iterator s_it;
	    for (s_it = iss_it->second.begin(); s_it != iss_it->second.end(); ++s_it)
		partially_drawn_paths[-1].insert(*s_it);
	}
    }
}

bool
BViewState::AddObj(struct bv_scene_obj *s)
{
    if (UNLIKELY(!s))
	return false;
    scene_objs.insert(s);
    return true;
}

bool
BViewState::RemoveObjs(const char *pattern, bool free_objs)
{
    if (UNLIKELY(!pattern))
	return false;

    std::vector<struct bv_scene_obj *> rmobjs;
    std::unordered_set<struct bv_scene_obj *>::iterator o_it;
    for (o_it = scene_objs.begin(); o_it != scene_objs.end(); ++o_it) {
	if (!bu_path_match(pattern, bu_vls_cstr(&((*o_it)->s_name)), 0))
	    rmobjs.push_back(*o_it);
    }
    for (size_t i = 0; i < rmobjs.size(); i++)
	RemoveObj(rmobjs[i], free_objs);

    return true;
}

bool
BViewState::RemoveObj(struct bv_scene_obj *s, bool free_obj)
{
    if (UNLIKELY(!s))
	return false;
    scene_objs.erase(s);
    if (free_obj)
	bv_obj_put(s);
    return true;
}


std::vector<std::string>
BViewState::DrawnPaths(int mode)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return std::vector<std::string>();

    std::vector<std::string> ret;

    std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator d_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    if (mode == -1) {
	std::set<unsigned long long> uniq_paths;
	for (d_it = drawn_paths.begin(); d_it != drawn_paths.end(); ++d_it) {
	    for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it)
		uniq_paths.insert(*s_it);
	}
	std::set<unsigned long long>::iterator ss_it;
	for (ss_it = uniq_paths.begin(); ss_it != uniq_paths.end(); ++ss_it) {
	    DbiPath *p = dbis->GetDbiPath(*s_it);
	    if (!p)
		continue;
	    ret.push_back(p->str());
	}

	std::sort(ret.begin(), ret.end(), &alphanum_cmp);
    } else {
	d_it = drawn_paths.find(mode);
	if (d_it == drawn_paths.end())
	    return std::vector<std::string>();

	for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it) {
	    DbiPath *p = dbis->GetDbiPath(*s_it);
	    if (!p)
		continue;
	    ret.push_back(p->str());
	}

	std::sort(ret.begin(), ret.end(), &alphanum_cmp);
    }

    return ret;
}

size_t
BViewState::DrawnPathCount(bool explicitly, int mode)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return 0;

    size_t ocnt = 0;
    std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator d_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    if (mode == -1) {
	if (explicitly) {
	    // Just count the specified drawn paths
	    std::set<unsigned long long> uniq_paths;
	    for (d_it = drawn_paths.begin(); d_it != drawn_paths.end(); ++d_it) {
		for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it)
		    uniq_paths.insert(*s_it);
	    }
	    ocnt = uniq_paths.size();
	} else {
	    // Count ALL leaves - the count returned in this mode is all active
	    // scene objects derived from the geometry info in the dbip.
	    for (d_it = s_expanded.begin(); d_it != s_expanded.end(); ++d_it)
		ocnt += s_expanded.size();
	}
    } else {
	if (explicitly) {
	    d_it = drawn_paths.find(mode);
	    if (d_it == drawn_paths.end())
		return 0;
	} else {
	    // Count ALL leaves - the count returned in this mode is all active
	    // scene objects derived from the geometry info in the dbip.
	    d_it = s_expanded.find(mode);
	    if (d_it == drawn_paths.end())
		return 0;
	}
	ocnt = d_it->second.size();
    }
    return ocnt;
}

int
BViewState::IsDrawn(DbiPath *p, int mode)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return 0;
    if (UNLIKELY(!p))
	return 0;
    return IsDrawn(p->hash(), mode);
}

int
BViewState::IsDrawn(unsigned long long hash, int mode)
{
    // Sanity
    if (UNLIKELY(!dbis))
	return 0;
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator m_it;
    if (mode == -1) {
	for (m_it = fully_drawn_paths.begin(); m_it != fully_drawn_paths.end(); ++m_it) {
	    if (m_it->second.find(hash) != m_it->second.end())
		return 1;
	}
	for (m_it = partially_drawn_paths.begin(); m_it != partially_drawn_paths.end(); ++m_it) {
	    if (m_it->second.find(hash) != m_it->second.end())
		return 2;
	}
    } else {
	m_it = fully_drawn_paths.find(mode);
	if (m_it != fully_drawn_paths.end()) {
	    if (m_it->second.find(hash) != m_it->second.end())
		return 1;
	}
	m_it = partially_drawn_paths.find(mode);
	if (m_it != partially_drawn_paths.end()) {
	    if (m_it->second.find(hash) != m_it->second.end())
		return 2;
	}
    }

    return 0;
}

// TODO - Clear drawn paths.  Probably need to garbage collect DbiPaths when doing this.
void
BViewState::Erase(int mode, bool process_scene_objs, int argc, const char **av)
{
   if (process_scene_objs) {
       // TODO - check for scene objs matching av array and take them out
       return;
   }

   // If we got here, we're after DbiPath entities.  Process everything but the
   // last path without doing the full rebuild.
   for (int pcnt = 0; pcnt < argc - 1; pcnt++)
       RemovePath(av[pcnt], mode, false);

   // When we do the last one, trigger the rebuild of fully_drawn_paths and
   // partially_drawn_paths.
   RemovePath(av[argc - 1], mode, true);
}


// This will move much of the dm_draw_objs and draw_scene_obj logic from libdm
// to this method and the DbiPath Draw method.  That should be a good thing,
// since the libraries and applications may ultimately want or need more
// control over how the drawing is done, but it will mean a bit of a rethink on
// how to trigger the libdm low-level operations from here.  Ideally, the libdm
// role will reduce to more or less the dm_draw_obj call - e.g.,
// (*v->dm_draw_sobj)(v->dmp, s) after s has been prepared with DbiPath->Draw().
//
// However, for that to work without a libdm type coupling, we will need to add
// a callback-specific signature function to libdm (dm_draw_sobj instead of
// dm_draw_obj) that has a function signature of int (*)(void *, struct
// bv_scene_obj *) and a function pointer slot to bview (the v->dm_draw_sobj
// above) so we can register an object rendering method with a bview the same
// way we register a dmp pointer.  If we keep the convention of one BViewState
// per dm, then between dmp and v->dm_draw_sobj the view will know what it needs
// to deal with scene objects.
//
// Also need to be able to trigger faceplate drawing, which is not composed of
// scene objects, so we have a second callback to register for that.
void
BViewState::Render()
{
    // If we don't have an assigned display manager, nothing to do.
    if (UNLIKELY(!v->dmp || !v->dm_draw_sobj || !v->dm_draw_view))
	return;

    // First, draw database geometry path-based scene objects active in this view
    if (LIKELY(dbis != NULL)) {
	std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator e_it;
	std::unordered_set<unsigned long long>::iterator s_it;

	// Get linked paths, if any
	if (l_dbipath && l_dbipath != this) {
	    for (e_it = l_dbipath->s_expanded.begin(); e_it != l_dbipath->s_expanded.end(); ++e_it) {
		for (s_it = e_it->second.begin(); s_it != e_it->second.end(); ++s_it) {
		    DbiPath *p = dbis->GetDbiPath(*s_it);
		    if (!p)
			continue;
		    // Check if this leaf is part of an edit - if so, we defer drawing

		    // Check if this leaf is part of a selection - if so, we need to override
		    // its color

		    // Standard leaf - have the dm do its standard operation on the object
		    p->Draw(this);
		}
	    }
	}

	// Get local-only paths
	for (e_it = s_expanded.begin(); e_it != s_expanded.end(); ++e_it) {
	    for (s_it = e_it->second.begin(); s_it != e_it->second.end(); ++s_it) {
		DbiPath *p = dbis->GetDbiPath(*s_it);
		if (!p)
		    continue;
		// Check if this leaf is part of an edit - if so, we defer drawing

		// Check if this leaf is part of a selection - if so, we need to override
		// its color

		// Standard leaf - have the dm do its standard operation on the object
		p->Draw(this);
	    }
	}
    }

    // Then draw user-provided scene objects
    //
    // First, those from the linked view (if any)
    std::unordered_set<struct bv_scene_obj *>::iterator o_it;
    if (l_viewobj && l_viewobj != this) {
	for (o_it = l_viewobj->scene_objs.begin(); o_it != l_viewobj->scene_objs.end(); ++o_it)
	    (*v->dm_draw_sobj)(v->dmp, *o_it);
    }

    // Next, those specific to this view
    for (o_it = scene_objs.begin(); o_it != scene_objs.end(); ++o_it)
	(*v->dm_draw_sobj)(v->dmp, *o_it);


    // Lastly, draw any faceplate elements active in the view.
    (*v->dm_draw_view)(v->dmp, v);

    // We're current now, so update the hash views
    old_view_hash = view_hash;
    old_dmp_hash = dmp_hash;
    old_objs_hash = objs_hash;
}

// In a Redraw, we are assuming that the DbiState's GObj and CombInst objects
// are all valid - if the database has changed, a Sync() should be performed
// on the DbiState before calling this method.
//
// GObj scene objects will have been updated by update_dp, and the DbiPath
// instances should be current after a BakePaths() call - what we are doing
// here is updating the view specific state containers needed to support
// things like hierarchy highlighting.
//
// If a DbiPath is straight up invalid, it is simply removed from the drawn
// sets.
void
BViewState::Redraw(bool UNUSED(autoview))
{
    // If we have a linked view, it is there responsibility of the app
    // to call Redraw on that view rather than this one.  We will process
    // only the info unique to this view.

    // Clear fully drawn and partially drawn path sets - they are derived from
    // drawn_paths and may have changed.
    std::unordered_map<int, std::unordered_set<unsigned long long>>::iterator i_it;
    for (i_it = fully_drawn_paths.begin(); i_it != fully_drawn_paths.end(); ++i_it)
	i_it->second.clear();
    for (i_it = partially_drawn_paths.begin(); i_it != partially_drawn_paths.end(); ++i_it)
	i_it->second.clear();

    std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator d_it;
    for (d_it = drawn_paths.begin(); d_it != drawn_paths.end(); ++d_it) {

	// Anything in the drawn_paths list that is invalid is gone.
	std::unordered_set<unsigned long long>::iterator s_it;
	std::vector<unsigned long long> to_erase;
	for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it) {
	    if (dbis->dbi_paths.find(*s_it) == dbis->dbi_paths.end()) {
		to_erase.push_back(*s_it);
	    }
	    for (size_t i = 0; i < to_erase.size(); i++)
		d_it->second.erase(to_erase.size());

	    // Update fully and partially drawn from new drawn_paths state
	    for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it) {
		DbiPath *p = dbis->GetDbiPath(*s_it);
		if (!p)
		    continue;

		// All child paths are also considered fully drawn.  Even an evaluated mode
		// drawing is considered to be drawing the subtree (just not necessarily as
		// individual scene objects) so we update the lists for both all and the
		// individual mode in all cases.
		dbis->AddPathsBelow(&(fully_drawn_paths[-1]), *s_it);
		dbis->AddPathsBelow(&(fully_drawn_paths[d_it->first]), *s_it);

		// The parents (if any) are now partially drawn paths, since the
		// fully_drawn_paths check at the beginning of AddPath determined this path
		// wasn't a child of an already drawn path.
		std::unordered_set<unsigned long long>::iterator sp_it;
		for (sp_it = p->parent_path_hashes.begin(); sp_it != p->parent_path_hashes.end(); ++sp_it) {
		    partially_drawn_paths[-1].insert(*sp_it);
		    partially_drawn_paths[d_it->first].insert(*sp_it);
		}
	    }

	    // Anything still valid needs to be re-expanded.  First, clear the old
	    // s_expanded hashes for this mode
	    s_expanded[d_it->first].clear();

	    // If we're mode 3 or 5 (the evaluated modes) we don't need the leaves expanding to leaves
	    if (d_it->first == 3 || d_it->first == 5)
		continue;

	    // Collect all the active drawn paths from this mode
	    std::vector<unsigned long long> to_expand;
	    to_expand.reserve(d_it->second.size());
	    for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it)
		to_expand.push_back(*s_it);

	    // Generate the new s_expanded hashes (which may be unchanged, but
	    // there's no way to know without doing the work.)  BakePaths should
	    // already have established prepared DbiPaths for this - what we are
	    // doing here is preparing the set of DbiPaths Render will actually
	    // iterate over to draw this specific view.
	    std::vector<unsigned long long> leaves = dbis->ExpandPaths(to_expand, false);
	    for (size_t i = 0; i < leaves.size(); i++)
		s_expanded[d_it->first].insert(leaves[i]);
	}
    }

    // Having done the redraw work, recalculate the hashes
    Hash();
}

#if 0
unsigned long long
BViewState::redraw(struct bv_obj_settings *vs, std::unordered_set<struct bview *> &views, int no_autoview, std::unordered_set<unsigned long long> &changed_hashes)
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
#endif

#if 0
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
#endif

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
