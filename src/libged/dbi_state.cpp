/*                D B I _ S T A T E . C P P
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

DbiState::DbiState(struct ged *ged_p)
{
    int64_t start, elapsed;
    fastf_t seconds;
    start = bu_gettime();

    // Create and initialize our local resource.  We'll generally
    // use this in the libged C++ code instead of rt_uniresource.
    BU_GET(res, struct resource);
    rt_init_resource(res, 0, NULL);

    // Set DbiState know what gedp we're working with, and check if it is valid
    // - if not, there's no point in continuing.
    gedp = ged_p;
    if (!gedp)
	return;

    // The default view is handled a bit specially - it is the "anchor" used
    // for modes like quad view, so it is generally the "target" view others will
    // link to and shouldn't be linked to another view.
    default_view = new BViewState(this, ged_p->ged_gvp);
    view_states[ged_p->ged_gvp] = default_view;
    // "Link" the default view to itself - this lets us detect that this view
    // is neither an independent view (l == NULL) nor a candidate for linking.
    default_view->l_dbipath = default_view;
    default_view->l_viewobj = default_view;

    // Set up view states for any other pre-existing views as well
    struct bu_ptbl *vset = &ged_p->ged_views;
    for (size_t i = 0; i < BU_PTBL_LEN(vset); i++) {
	struct bview *vsv = (struct bview *)BU_PTBL_GET(vset, i);
	if (view_states.find(vsv) != view_states.end())
	    continue;
        view_states[vsv] = new BViewState(this, vsv);
    }

    // Set up the default selection state
    d_selection = new BSelectState(this);
    selected_sets[std::string("default")] = d_selection;

    if (gedp->dbip) {
	// Set up cache
	dcache = dbi_cache_open(gedp->dbip->dbi_filename);

	// Do the initial population
	Sync(true);
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    bu_log("DbiState init: %f sec\n", seconds);

}


DbiState::~DbiState()
{
    std::vector<BSelectState *> to_delete_s;
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ++ss_it)
	to_delete_s.push_back(ss_it->second);
    for (size_t i = 0; i < to_delete_s.size(); i++)
	delete to_delete_s[i];

    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    std::vector<BViewState *> to_delete_v;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it)
	to_delete_v.push_back(v_it->second);
    for (size_t i = 0; i < to_delete_v.size(); i++)
	delete to_delete_v[i];

    // TODO - clean up GObjs, DbiPaths, the DbiPath queue...

    rt_clean_resource_basic(NULL, res);
    BU_PUT(res, struct resource);

    if (dcache)
	bu_cache_close(dcache);
}

bool
DbiState::valid_hash(unsigned long long phash)
{
    // Zero is never valid
    if (!phash)
	return false;

    // Most of the time phash will be a comb instance, so try that first
    if (combinsts.find(phash) != combinsts.end())
	return true;

    if (gobjs.find(phash) != gobjs.end())
	return true;

    return false;
}

std::string
DbiState::hash_str(unsigned long long phash)
{
    if (combinsts.find(phash) != combinsts.end()) {
	CombInst *c = combinsts[phash];
	std::string hash_str = c->cname + std::string("/") + (c->iname.length() ? c->iname : c->oname);
	return hash_str;
    }

    if (gobjs.find(phash) != gobjs.end()) {
	GObj *g = gobjs[phash];
	return g->name;
    }

    return std::string("Unknown hash: ") + std::to_string(phash);
}

GObj *
DbiState::GetGObj(struct directory *dp)
{
    if (UNLIKELY(!dp))
	return NULL;

    std::unordered_map<struct directory *, GObj *>::iterator dp_it;
    dp_it = dp2g.find(dp);
    if (dp_it == dp2g.end())
	return NULL;

    return dp_it->second;
}

GObj *
DbiState::GetGObj(unsigned long long hash)
{
    if (UNLIKELY(!hash))
	return NULL;

    std::unordered_map<unsigned long long, GObj *>::iterator dp_it;
    dp_it = gobjs.find(hash);
    if (dp_it == gobjs.end())
	return NULL;

    return dp_it->second;
}

CombInst *
DbiState::GetCombInst(unsigned long long hash)
{
    if (UNLIKELY(!hash))
	return NULL;

    std::unordered_map<unsigned long long, CombInst *>::iterator c_it;
    c_it = combinsts.find(hash);
    if (c_it == combinsts.end())
	return NULL;

    return c_it->second;
}

DbiPath *
DbiState::GetDbiPath()
{
    if (dbiq.empty())
	return new DbiPath(this);

    DbiPath *cp = dbiq.front();
    dbiq.pop();
    return cp;
}


DbiPath *
DbiState::GetDbiPath(const char *path)
{
    if (!path)
	return NULL;

    DbiPath tmpp(this, path);
    if (!tmpp.valid())
	return NULL;

    unsigned long long thash = tmpp.hash();
    DbiPath *np = GetDbiPath(thash);
    if (!np) {
	np = new DbiPath(tmpp);
	dbi_paths[thash] = np;
    }

    return np;
}

DbiPath *
DbiState::GetDbiPath(unsigned long long hash)
{
    if (UNLIKELY(!hash))
	return NULL;

    std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
    p_it = dbi_paths.find(hash);
    if (p_it != dbi_paths.end())
	return p_it->second;

    return NULL;
}

void
DbiState::PutDbiPath(DbiPath *p)
{
    if (UNLIKELY(!p))
	return;

    p->Reset();
    dbiq.push(p);
}

void
DbiState::BakePaths()
{
    // Make sure we have DbiPaths for every leaf of every valid drawn path in
    // any currently active view - those are the paths we will need for
    // property setting
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    std::unordered_set<unsigned long long> uniq_paths;
    std::unordered_set<BViewState *> processed_vstates;
    for (v_it = view_states.begin(); v_it != view_states.end(); v_it++) {
	BViewState *vs = v_it->second;
	if (processed_vstates.find(vs) != processed_vstates.end())
	    continue;
	processed_vstates.insert(vs);
	std::unordered_map<size_t, std::unordered_set<unsigned long long>>::iterator d_it;
	for (d_it = vs->drawn_paths.begin(); d_it != vs->drawn_paths.end(); ++d_it) {
	    for (s_it = d_it->second.begin(); s_it != d_it->second.end(); ++s_it) {
		if (dbi_paths.find(*s_it) != dbi_paths.end())
		    uniq_paths.insert(*s_it);
	    }
	}
    }
    std::vector<unsigned long long> to_expand;
    to_expand.reserve(uniq_paths.size());
    for (s_it = uniq_paths.begin(); s_it != uniq_paths.end(); ++s_it)
	to_expand.push_back(*s_it);
    ExpandPaths(to_expand, true);
    uniq_paths.clear();
    to_expand.clear();

    // We now have expanded DbiPaths, but their color and matrix info is not
    // yet set on their scene objects.  We also need to make sure any needed
    // child GObj objects have been added.
    std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
    for (p_it = dbi_paths.begin(); p_it != dbi_paths.end(); ++p_it) {
	DbiPath *p = p_it->second;
	p->BakeSceneObjs();
    }
}

void
DbiState::clear_cache(struct directory *dp)
{
    if (!dp || !dcache)
	return;

    unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));

    cache_del(dcache, hash, CACHE_OBJ_BOUNDS);
    cache_del(dcache, hash, CACHE_REGION_ID);
    cache_del(dcache, hash, CACHE_REGION_FLAG);
    cache_del(dcache, hash, CACHE_INHERIT_FLAG);
    cache_del(dcache, hash, CACHE_COLOR);
}

unsigned long long
DbiState::update_dp(struct directory *dp)
{
    if (dp->d_flags & DB_LS_HIDDEN)
	return 0;

    unsigned long long dphash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    if (gobjs.find(dphash) != gobjs.end()) {
	// Existing GObj found, remove for regeneration
	GObj *old_gobj = gobjs[dphash];
	delete old_gobj;
    }

    GObj *g = new GObj(this, dp);
    return g->hash;
}

std::vector<GObj *>
DbiState::get_gobjs(std::vector<unsigned long long> &path)
{
    std::vector<GObj *> cgs;
    // The first element in the path is a GObj
    cgs.push_back(gobjs[path[0]]);
    // Any remaining path are CombInst.  We want the GObj associated
    // with their oname
    for (size_t i = 1; i < path.size(); i++) {
	// TODO  - if we hit something invalid should we error out?
	if (combinsts.find(path[i]) == combinsts.end())
	    continue;
	if (gobjs.find(combinsts[path[i]]->ohash) == gobjs.end())
	    continue;
	cgs.push_back(gobjs[combinsts[path[i]]->ohash]);
    }

    return cgs;
}

std::vector<CombInst *>
DbiState::get_combinsts(std::vector<unsigned long long> &path)
{
    std::vector<CombInst *> cis;
    // The first element in the path is a GObj Any remaining path are CombInst.
    for (size_t i = 1; i < path.size(); i++) {
	// TODO  - if we hit something invalid should we error out?
	if (combinsts.find(path[i]) == combinsts.end())
	    continue;
	cis.push_back(combinsts[path[i]]);
    }
    return cis;
}

BViewState *
DbiState::AddBViewState(struct bview *v)
{
    BViewState *nv;
    if (v) {
	nv = GetBViewState(v);
	if (nv)
	    return nv;
    }
    nv = new BViewState(this, v);
    return nv;
}

BViewState *
DbiState::GetBViewState(struct bview *v)
{
    if (UNLIKELY(!v))
	return NULL;

    std::unordered_map<struct bview *, BViewState *>::iterator v_it = view_states.find(v);
    if (v_it != view_states.end())
	return v_it->second;

    return NULL;
}

std::vector<BViewState *>
DbiState::FindBViewState(const char *pattern)
{
    std::vector<BViewState *> vs;
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it) {
	if (!pattern || !strlen(pattern) || !bu_path_match(pattern, bu_vls_cstr(&(v_it->first->gv_name)), 0))
	    vs.push_back(v_it->second);
    }
    return vs;
}

void
DbiState::FlagEmpty()
{
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it)
	v_it->second->Empty();
}

void
DbiState::Redraw(BViewState *v, bool autoview)
{
    BViewState *wv = (v) ? v : default_view;

    std::vector<BViewState *> aviews;
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it) {
	if (v_it->second == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
	if (v_it->second->l_viewobj == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
	if (v_it->second->l_dbipath == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
    }

    for (size_t i = 0; i < aviews.size(); i++) {
	aviews[i]->Redraw(autoview);
    }

}


void
DbiState::Render(BViewState *v)
{
    BViewState *wv = (v) ? v : default_view;

    std::vector<BViewState *> aviews;
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it) {
	if (v_it->second == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
	if (v_it->second == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
	if (v_it->second->l_viewobj == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
	if (v_it->second->l_dbipath == wv) {
	    aviews.push_back(v_it->second);
	    continue;
	}
    }

    for (size_t i = 0; i < aviews.size(); i++) {
	aviews[i]->Render();
    }
}

BSelectState *
DbiState::GetSelectionSet(const char *sname)
{
    if (!sname || BU_STR_EQUIV(sname, "default"))
	return d_selection;

    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++)
	if (BU_STR_EQUIV(sname, ss_it->first.c_str()))
	    return ss_it->second;

    return NULL;
}

BSelectState *
DbiState::AddSelectionSet(const char *sname)
{
    if (!sname || BU_STR_EQUIV(sname, "default"))
	return d_selection;

    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++)
	if (BU_STR_EQUIV(sname, ss_it->first.c_str()))
	    return ss_it->second;

    BSelectState *s = new BSelectState(this);
    selected_sets[sname] = s;
    return s;
}


void
DbiState::RemoveSelectionSet(BSelectState *s)
{
    if (!s)
	return;	

    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (s == ss_it->second) {
	    delete ss_it->second;
	    selected_sets.erase(ss_it);
	    return;
	}
    }
}


void
DbiState::RemoveSelectionSets(const char *pattern)
{
    if (BU_STR_EQUIV(pattern, "default")) {
	d_selection->Clear();
	return;
    }

    std::vector<std::string> to_erase;
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (!pattern || !strlen(pattern) || !bu_path_match(pattern, ss_it->first.c_str(), 0)) {
	    ss_it->second->Clear();
	    if (!BU_STR_EQUIV(ss_it->first.c_str(), "default")) {
		delete ss_it->second;
		to_erase.push_back(ss_it->first);
	    }
	}
    }
    for (size_t i = 0; i < to_erase.size(); i++)
	selected_sets.erase(to_erase[i]);
}

// alphanum sort
static bool alphanum_cmp(const std::string &a, const std::string &b)
{
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

std::vector<std::string>
DbiState::FindSelectionSets(const char *pattern)
{
    std::vector<std::string> ret;
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (!pattern || !strlen(pattern) || !bu_path_match(pattern, ss_it->first.c_str(), 0))
	    ret.push_back(ss_it->first);
    }
    std::sort(ret.begin(), ret.end(), &alphanum_cmp);
    return ret;
}


// TODO test
void
DbiState::gather_cyclic(
	std::unordered_set<unsigned long long> &cyclic,
	unsigned long long ihash, DbiPath &p
	)
{
    p.push(ihash);
    if (!p.cyclic()) {
	/* Not cyclic - keep going */
	CombInst *c = combinsts[ihash];
	GObj *g = gobjs[c->ohash];
	for (size_t i = 0; i < g->cv.size(); i++) {
	    gather_cyclic(cyclic, g->cv[i]->ihash, p);
	}
    } else {
	CombInst *c = combinsts[ihash];
	cyclic.insert(c->ohash);
    }

    /* Done with branch - restore path.  Disable validation since we are doing
     * a controlled walk and know we're not cyclic after the pop. */
    p.pop(false);
}

static int
alphanum_sort(const void *a, const void *b, void *UNUSED(data)) {
    struct directory *ga = *(struct directory **)a;
    struct directory *gb = *(struct directory **)b;
    return alphanum_impl(ga->d_namep, gb->d_namep, NULL);
}

std::vector<unsigned long long>
DbiState::tops(bool show_cyclic)
{
    std::vector<unsigned long long> ret;
    // First, get the standard tops results
    struct directory **all_paths = NULL;
    db_update_nref(gedp->dbip, &rt_uniresource);
    int tops_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &all_paths);
    if (all_paths) {
	bu_sort(all_paths, tops_cnt, sizeof(struct directory *), alphanum_sort, NULL);
	for (int i = 0; i < tops_cnt; i++) {
	    unsigned long long hash = bu_data_hash(all_paths[i]->d_namep, strlen(all_paths[i]->d_namep)*sizeof(char));
	    ret.push_back(hash);
	}
	bu_free(all_paths, "free db_ls output");
    }

    if (!show_cyclic)
	return ret;

    // If we also want to report combs with cyclic paths as top level objects
    // use DbiState to try and speed things up.
    //
    // db_ls has that capability, but it has to unpack all the combs walking
    // the tree to find the answer and that results in a slow check for large
    // databases.
    std::unordered_set<unsigned long long> cyclic_paths;

    // Out of the gate, the only non-solids we can be sure aren't cyclic
    // are the top level returns.  Using them as seeds, walk the hierarchies
    // looking for any cycles.
    for (size_t i = 0; i < ret.size(); i++) {
	GObj *g = gobjs[ret[i]];
	for (size_t j = 0; j < g->cv.size(); j++) {
	    DbiPath p(this);
	    p.push(g->hash);
	    gather_cyclic(cyclic_paths, g->cv[j]->ihash, p);
	}
    }
    std::unordered_set<unsigned long long>::iterator c_it;
    for (c_it = cyclic_paths.begin(); c_it != cyclic_paths.end(); c_it++) {
	ret.push_back(*c_it);
    }

    return ret;
}

void
DbiState::Sync(bool force)
{
    if (!added.size() && !changed.size() && !removed.size()) {

	// We don't know anything about any changes - unless we're
	// being forced to do a full sync, assume we're good.
	if (!force)
	    return;

	// We've got to redo everything if we don't know what happened.
	dp2g.clear();

	// The GObj deletes will also delete the CombInst instances
	combinsts.clear();

	// Clear all GObj instances
	std::unordered_map<unsigned long long, GObj *>::iterator go_it;
	for (go_it = gobjs.begin(); go_it != gobjs.end(); ++go_it) {
	    GObj *go = go_it->second;
	    delete go;
	}

	// (Re)create all GObj instances
	if (gedp->dbip) {
	    db_update_nref(gedp->dbip, res);
	    for (int i = 0; i < RT_DBNHASH; i++) {
		struct directory *dp;
		for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		    update_dp(dp);
		}
	    }
	}
	return;
    }

    // Make sure our references are current
    db_update_nref(gedp->dbip, res);

    // For objects that are removed, delete the GObj and its associated data.
    // Removal of any instances from a comb tree object is treated as part of
    // the object removal, and thus is handled internally by the GObj
    // destructor - we don't need to do it here.
    std::unordered_set<struct directory *>::iterator g_it;
    for (g_it = removed.begin(); g_it != removed.end(); g_it++) {
	std::unordered_map<struct directory *, GObj *>::iterator r_it;
	r_it = dp2g.find(*g_it);
	if (r_it == dp2g.end())
	    continue;
	GObj *robj = r_it->second;
	delete robj;
    }

    // For changed and added objects, call update_dp
    for (g_it = added.begin(); g_it != added.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("added: %s\n", dp->d_namep);
	update_dp(dp);
    }

    for (g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("changed : %s\n", dp->d_namep);
	update_dp(dp);
    }

    // Now that GObj and CombInst modifications are complete, we need to audit
    // any DbiPaths registered with dbi_paths.  We deliberately don't do so
    // until all changes are in, since a path that was invalid after a remove
    // op might be valid again if the added or changed deltas recreated the
    // removed components.
    std::unordered_map<unsigned long long, DbiPath *>::iterator dbip_it;
    std::vector<unsigned long long> invalid_dbipaths;
    for (dbip_it = dbi_paths.begin(); dbip_it != dbi_paths.end(); ++dbip_it) {
	DbiPath *p = dbip_it->second;
	if (!p->valid()) {
	    invalid_dbipaths.push_back(dbip_it->first);
	    PutDbiPath(p);
	}
    }
    for (size_t i = 0; i < invalid_dbipaths.size(); i++) {
	dbi_paths.erase(invalid_dbipaths[i]);
    }

    // DbiPath states may be out of date, so we need to re-bake them
    BakePaths();

    // For all associated view states, execute any necessary changes to the
    // drawn object lists and scene objects
    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it)
	v_it->second->Redraw();

    // Sync all selection states
    if (d_selection)
	d_selection->Sync();
    std::unordered_map<std::string, BSelectState *>::iterator st_it;
    for (st_it = selected_sets.begin(); st_it != selected_sets.end(); ++st_it) {
	if (st_it->second == d_selection)
	    continue;
	st_it->second->Sync();
    }

    // Make sure the renders are current
    for (v_it = view_states.begin(); v_it != view_states.end(); ++v_it)
	v_it->second->Render();

    // Updates done, clear items stored by callbacks
    added.clear();
    changed.clear();
    removed.clear();
}

void
DbiState::expand_path(std::vector<unsigned long long> *opaths, DbiPath &p, bool create_paths)
{
    // Unpack the leaf GObj of p
    GObj *g = p.GetGObj(p.Depth());
    // If we can't keep walking, then the path ends here and we just copy p
    // into out_paths
    if (!g || !g->cv.size()) {
	opaths->push_back(p.hash());
	// If DbiPath isn't already in dbi_paths, copy p and add it
	if (create_paths && dbi_paths.find(p.hash()) == dbi_paths.end()) {
	    DbiPath *op = GetDbiPath();
	    *op = p;
	    dbi_paths[op->hash()] = op;
	}
	return;
    }

    // If we have a comb, process the instances.
    for (size_t i = 0; i < g->cv.size(); i++) {
	p.push(g->cv[i]->ihash);
	expand_path(opaths, p, create_paths);
	p.pop(false);
    }
}

std::vector<unsigned long long>
DbiState::ExpandPaths(std::vector<unsigned long long> &paths, bool create_paths)
{
    std::vector<unsigned long long> out_paths;
    std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
    for (size_t i = 0; i < paths.size(); i++) {
	p_it = dbi_paths.find(paths[i]);
	// If the hash doesn't correspond to a current path,
	// we can't expand it
	if (p_it == dbi_paths.end())
	    continue;

	// If a path isn't valid (i.e. one or more of its components
	// are no longer in gobjs or combinsts) we can't expand it.
	DbiPath *p = p_it->second;
	if (!p->valid())
	    continue;

	// We need to push and pop a path while we walk - create a
	// working path copy for that purpose
	DbiPath wp(*p);
	expand_path(&out_paths, wp, create_paths);
    }

    return out_paths;
}



void
DbiState::collect_paths(std::unordered_set<unsigned long long> *s, DbiPath &p, bool create_paths)
{
    // Save current path hash
    s->insert(p.hash());

    // If DbiPath isn't already in dbi_paths, copy p and add it
    if (create_paths && dbi_paths.find(p.hash()) == dbi_paths.end()) {
	DbiPath *op = GetDbiPath();
	*op = p;
	dbi_paths[op->hash()] = op;
    }

    // Unpack the leaf GObj of p.  If we can't, or if there's nothing there,
    // we're done.
    GObj *g = p.GetGObj(p.Depth());
    if (!g || !g->cv.size())
	return;

    // If we have a comb, process the instances.
    for (size_t i = 0; i < g->cv.size(); i++) {
	p.push(g->cv[i]->ihash);
	collect_paths(s, p, create_paths);
	p.pop(false);
    }
}

void
DbiState::AddPathsBelow(std::unordered_set<unsigned long long> *s, unsigned long long phash, bool create_paths)
{
    if (UNLIKELY(!s))
	return;

    std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
    p_it = dbi_paths.find(phash);
    // If the hash doesn't correspond to a current path,
    // we can't expand it
    if (p_it == dbi_paths.end())
	return;

    // If a path isn't valid (i.e. one or more of its components
    // are no longer in gobjs or combinsts) we can't expand it.
    DbiPath *p = p_it->second;
    if (!p->valid())
	return;

    // We need to push and pop a path while we walk - create a
    // working path copy for that purpose
    DbiPath wp(*p);
    collect_paths(s, wp, create_paths);
}


void
DbiState::clear_paths(std::unordered_set<unsigned long long> *s, DbiPath &p)
{
    // Save current path hash
    s->erase(p.hash());

    // Unpack the leaf GObj of p.  If we can't, or if there's nothing there,
    // we're done.
    GObj *g = p.GetGObj(p.Depth());
    if (!g || !g->cv.size())
	return;

    // If we have a comb, process the instances.
    for (size_t i = 0; i < g->cv.size(); i++) {
	p.push(g->cv[i]->ihash);
	clear_paths(s, p);
	p.pop(false);
    }
}

void
DbiState::RemovePathsBelow(std::unordered_set<unsigned long long> *s, unsigned long long phash)
{
    if (UNLIKELY(!s))
	return;

    std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
    p_it = dbi_paths.find(phash);
    // If the hash doesn't correspond to a current path,
    // we can't expand it
    if (p_it == dbi_paths.end())
	return;

    // If a path isn't valid (i.e. one or more of its components
    // are no longer in gobjs or combinsts) we can't expand it.
    DbiPath *p = p_it->second;
    if (!p->valid())
	return;

    // We need to push and pop a path while we walk - create a
    // working path copy for that purpose
    DbiPath wp(*p);
    clear_paths(s, wp);
}

std::vector<unsigned long long>
DbiState::CollapsePaths(std::vector<unsigned long long> &paths, bool create_paths)
{
    std::vector<unsigned long long> out_paths;
    std::unordered_set<DbiPath *> cleanup;
    std::unordered_set<DbiPath *>::iterator c_it;

    // Group paths of the same depth.
    std::map<size_t, std::vector<DbiPath *>> depth_groups;
    for (size_t i = 0; i < paths.size(); i++) {
	std::unordered_map<unsigned long long, DbiPath *>::iterator p_it;
	p_it = dbi_paths.find(paths[i]);
	// If the hash doesn't correspond to a current path,
	// we can't expand it
	if (p_it == dbi_paths.end())
	    continue;

	// We have a DbiPath - we're in business
	DbiPath *op = p_it->second;

	// Depth == 0 paths are top level and need no collapsing
	size_t depth = op->Depth();
	if (!depth) {
	    out_paths.push_back(op->hash());
	    continue;
	}

	// Because some of the paths will be modded during collapse,
	// we need temporary copies rather than working with the
	// ones from dbi_paths.
	DbiPath *np = new DbiPath();
	*np = *op;
	depth_groups[i].push_back(np);
	cleanup.insert(np);
    }

    // Whittle down the depth groups until we find not-fully-listed parents -
    // when we find that, the children constitute non-collapsible paths based
    // on what's included in the input set
    while (depth_groups.size()) {
	size_t plen = depth_groups.rbegin()->first;
	if (!plen)
	    break;

	// Give ourselves an easy handle for the current depth group
	std::vector<DbiPath *> &dpaths = depth_groups.rbegin()->second;

	// For a given depth, group the paths by parent path.  This results
	// in path sub-groups which will define for us how "fully drawn"
	// that particular parent comb instance is.
	std::unordered_map<unsigned long long, std::vector<DbiPath *>> grouped_paths;
	for (size_t i = 0; i < dpaths.size(); i++){
	    unsigned long long ppathhash = dpaths[i]->hash(plen - 1);
	    grouped_paths[ppathhash].push_back(dpaths[i]);
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::vector<DbiPath *>>::iterator pg_it;
	for (pg_it = grouped_paths.begin(); pg_it != grouped_paths.end(); ++pg_it) {

	    // Collect the set of active child hashes
	    std::vector<DbiPath *> &cpaths = pg_it->second;
	    std::unordered_set<unsigned long long> leaf_hashes;
	    for (size_t i = 0; i < cpaths.size(); i++) {
		DbiPath *lp = cpaths[i];
		CombInst *c = lp->GetCombInst();
		if (UNLIKELY(!c)) {
		    for (c_it = cleanup.begin(); c_it != cleanup.end(); ++c_it) {
			DbiPath *p = *c_it;
			delete p;
		    }
		    return std::vector<unsigned long long> ();
		}
		leaf_hashes.insert(lp->GetCombInst()->ihash);
	    }

	    // We need the full list of children from the CombInst.  Since the
	    // grouped paths all have the same parent - just use the first path
	    // to get the leaf CombInst, which will tell us where the GObj
	    // containing the full list of child CombInsts can be found.
	    DbiPath *p = cpaths[0];
	    CombInst *c = p->GetCombInst();
	    if (UNLIKELY(!c)) {
		for (c_it = cleanup.begin(); c_it != cleanup.end(); ++c_it) {
		    DbiPath *dp = *c_it;
		    delete dp;
		}
		return std::vector<unsigned long long> ();
	    }
	    GObj *pg = gobjs[c->chash];
	    if (UNLIKELY(!pg)) {
		for (c_it = cleanup.begin(); c_it != cleanup.end(); ++c_it) {
		    DbiPath *dp = *c_it;
		    delete dp;
		}
		return std::vector<unsigned long long> ();
	    }

	    bool complete = true;
	    for (size_t i = 0; i < pg->cv.size(); i++) {
		if (leaf_hashes.find(pg->cv[i]->ihash) == leaf_hashes.end()) {
		    complete = false;
		    break;
		}
	    }

	    if (complete) {
		// If fully drawn, depth_groups[plen-1] gets the first path in
		// cpaths with the leaf popped off.
		DbiPath *cp = cpaths[0];
		cp->pop(false);
		depth_groups[plen - 1].push_back(cp);
	    } else {
		// No further collapsing - add to final.
		for (size_t i = 0; i < cpaths.size(); i++) {
		    out_paths.push_back(cpaths[i]->hash());
		    // Save the DbiPath instances in dbi_paths if they don't
		    // already exist there.
		    if (create_paths && dbi_paths.find(cpaths[i]->hash()) == dbi_paths.end()) {
			dbi_paths[cpaths[i]->hash()] = cpaths[i];
			cleanup.erase(cpaths[i]);
		    }
		}
	    }
	}

	// Done with this depth
	depth_groups.erase(plen);
    }

    // If we collapsed all the way to top level objects, make sure to add them
    if (depth_groups.find(0) != depth_groups.end()) {
	std::vector<DbiPath *> &dpaths = depth_groups.rbegin()->second;
	for (size_t i = 0; i < dpaths.size(); i++) {
	    out_paths.push_back(dpaths[i]->hash());
	    // Save the DbiPath instances in dbi_paths if they don't already
	    // exist there.
	    if (create_paths && dbi_paths.find(dpaths[i]->hash()) == dbi_paths.end()) {
		dbi_paths[dpaths[i]->hash()] = dpaths[i];
		cleanup.erase(dpaths[i]);
	    }
	}
    }

    // Clean up any temporary paths that didn't end up getting saved
    for (c_it = cleanup.begin(); c_it != cleanup.end(); ++c_it) {
	DbiPath *p = *c_it;
	delete p;
    }

    // Return hashes
    return out_paths;
}


// C exposure of certain DbiState/BViewState information and capabilities


const struct bu_ptbl *
ged_find_scene_objs(struct ged *gedp, struct bview *v, const char *pattern, int dbipaths, int view_only)
{
    if (!gedp)
	return NULL;

    BViewState *bvs = gedp->dbi_state->GetBViewState(v);
    if (!bvs)
	return NULL;

    return bvs->FindSceneObjsPtbl(pattern, dbipaths, view_only);
}

struct bv_scene_obj *
ged_vlblock_scene_obj(struct ged *gedp, struct bview *v, const char *name, struct bv_vlblock *vbp)
{
    if (!gedp || !name || !vbp)
	return NULL;

    BViewState *bvs = gedp->dbi_state->GetBViewState(v);
    if (!bvs)
	return NULL;

    bvs->RemoveObjs(name);

    struct bv_scene_obj *s = bv_obj_get(gedp->free_scene_objs);
    if (!s)
	return NULL;

    bv_vlblock_obj(s, vbp);
    bu_vls_sprintf(&s->s_name, "%s", name);

    bvs->AddObj(s);

    return s;
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
