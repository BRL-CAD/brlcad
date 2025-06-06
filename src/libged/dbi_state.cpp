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
    BU_GET(res, struct resource);
    rt_init_resource(res, 0, NULL);
    shared_vs = new BViewState(this);
    d_selection = new BSelectState(this);
    selected_sets[std::string("default")] = d_selection;
    gedp = ged_p;
    if (!gedp || !gedp->dbip)
	return;

    // Set up cache
    dcache = dbi_cache_open(gedp->dbip->dbi_filename);

    int64_t start, elapsed;
    fastf_t seconds;

    start = bu_gettime();

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    update_dp(dp);
	}
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    bu_log("DbiState init: %f sec\n", seconds);

}


DbiState::~DbiState()
{
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	delete ss_it->second;
    }
    delete shared_vs;
    rt_clean_resource_basic(NULL, res);
    BU_PUT(res, struct resource);

    if (dcache)
	dbi_cache_close(dcache);
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
DbiState::GetBViewState(struct bview *v)
{
    if (!v->independent)
	return shared_vs;
    if (view_states.find(v) != view_states.end())
	return view_states[v];

    BViewState *nv = new BViewState(this);
    view_states[v] = nv;
    return nv;
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
DbiState::RemoveSelectionSet(const char *sname)
{
    if (!sname || BU_STR_EQUIV(sname, "default")) {
	d_selection->Clear();
	return;
    }

    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (BU_STR_EQUIV(sname, ss_it->first.c_str())) {
	    delete ss_it->second;
	    selected_sets.erase(ss_it);
	    return;
	}
    }
}

// alphanum sort
static bool alphanum_cmp(const std::string &a, const std::string &b)
{
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

std::vector<std::string>
DbiState::ListSelectionSets()
{
    std::vector<std::string> ret;
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
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

    // If we also want to report combs with cyclic paths, as top level objects
    // use DbiState to try and speed things up.
    // db_ls has that capability, but it has to unpack all the combs walking
    // the tree to find the answer and that results in a slow check for large
    // databases.
    std::unordered_set<unsigned long long> cyclic_paths;
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

unsigned long long
DbiState::update()
{
    unsigned long long ret = 0;

    if (!added.size() && !changed.size() && !removed.size()) {
	return ret;
    }

    // If we got this far, SOMETHING changed
    ret |= GED_DBISTATE_DB_CHANGE;

    std::unordered_set<struct directory *>::iterator g_it;

    if (need_update_nref) {
	db_update_nref(gedp->dbip, res);
	need_update_nref = false;
    }

    // For objects that are removed, delete the GObj and its associated data.
    // Removal of an instance from a comb tree is handled as an object change,
    // and thus is handled internally by the GObj destructor.
    for (g_it = removed.begin(); g_it != removed.end(); g_it++) {
	std::unordered_map<struct directory *, GObj *>::iterator r_it;
	r_it = dp2g.find(*g_it);
	if (r_it == dp2g.end())
	    continue;
	GObj *robj = r_it->second;
	delete robj;
    }

    // We store the changed hashes so view updates can know which
    // objects will need updating.  update_dp will rebuild the GObj
    // and any child CombInsts.
    std::unordered_set<unsigned long long> changed_hashes;
    for (g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	unsigned long long hash = update_dp(dp);
	changed_hashes.insert(hash);
    }

    for (g_it = added.begin(); g_it != added.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("added: %s\n", dp->d_namep);
	update_dp(dp);
    }

    // For all associated view states, execute any necessary changes to
    // view objects and lists
    std::unordered_map<BViewState *, std::unordered_set<struct bview *>> vmap;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	BViewState *bvs = gedp->dbi_state->GetBViewState(v);
	if (!bvs)
	    continue;
	vmap[bvs].insert(v);
    }
    std::unordered_map<BViewState *, std::unordered_set<struct bview *>>::iterator bv_it;
    for (bv_it = vmap.begin(); bv_it != vmap.end(); bv_it++) {
	bv_it->first->Redraw(&bv_it->second, NULL, 1, &changed_hashes);
    }

    // Updates done, clear items stored by callbacks
    added.clear();
    changed.clear();
    removed.clear();

    return ret;
}

void
DbiState::expand_path(std::vector<unsigned long long> *opaths, DbiPath &p)
{
    // Unpack the leaf GObj of p
    GObj *g = p.GetGObj(p.depth());
    if (!g) {
	// If we can't get a valid object, then the path
	// ends here and we just copy p into out_paths
	opaths->push_back(p.hash());
	// If DbiPath isn't already in dbi_paths, copy p and add it
	if (dbi_paths.find(p.hash()) == dbi_paths.end()) {
	    DbiPath *op = new DbiPath(p);
	    dbi_paths[op->hash()] = op;
	}
	return;
    }

    // If g is a solid, this is a leaf path and we're done
    if (!g->cv.size()) {
	opaths->push_back(p.hash());
	// If DbiPath isn't already in dbi_paths, copy p and add it
	if (dbi_paths.find(p.hash()) == dbi_paths.end()) {
	    DbiPath *op = new DbiPath(p);
	    dbi_paths[op->hash()] = op;
	}
	return;
    }

    // If we have a comb, process the instances.
    for (size_t i = 0; i < g->cv.size(); i++) {
	p.push(g->cv[i]->ihash);
	expand_path(opaths, p);
	p.pop(false);
    }
}

std::vector<unsigned long long>
DbiState::ExpandPaths(std::vector<unsigned long long> &paths)
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
	expand_path(&out_paths, wp);
    }

    return out_paths;
}

std::vector<unsigned long long>
DbiState::CollapsePaths(std::vector<unsigned long long> &paths)
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
	size_t depth = op->depth();
	if (!depth) {
	    out_paths.push_back(op->hash());
	    continue;
	}

	// Because some of the paths will be modded during collapse,
	// we need temporary copies rather than working with the
	// ones from dbi_paths.
	DbiPath *np = new DbiPath(*op);
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
		    if (dbi_paths.find(cpaths[i]->hash()) == dbi_paths.end()) {
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
	    if (dbi_paths.find(dpaths[i]->hash()) == dbi_paths.end()) {
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

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
