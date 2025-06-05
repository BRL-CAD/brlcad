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
    bu_vls_init(&path_string);
    bu_vls_init(&hash_string);
    BU_GET(res, struct resource);
    rt_init_resource(res, 0, NULL);
    shared_vs = new BViewState(this);
    default_selected = new BSelectState(this);
    selected_sets[std::string("default")] = default_selected;
    gedp = ged_p;
    if (!gedp)
	return;
    dbip = gedp->dbip;
    if (!dbip)
	return;

    // Set up cache
    dcache = dbi_cache_open(dbip->dbi_filename);

    int64_t start, elapsed;
    fastf_t seconds;

    start = bu_gettime();

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    update_dp(dp);
	}
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    bu_log("DbiState init: %f sec\n", seconds);

}


DbiState::~DbiState()
{
    bu_vls_free(&path_string);
    bu_vls_free(&hash_string);
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



struct directory *
DbiState::get_hdp(unsigned long long phash)
{
    // For a comb instance hash, we could return either the parent comb or the
    // directory pointer of oname - since there isn't an unambiguous return,
    // just return NULL.  It is the caller's responsibility to select what they
    // want if the CombInst struct is in play and submit the GObj hash.

    if (gobjs.find(phash) == gobjs.end())
	return NULL;

    return gobjs[phash]->dp;
}

#if 0
bool
DbiState::get_path_bbox(point_t *bbmin, point_t *bbmax, std::vector<unsigned long long> &elements)
{
    if (UNLIKELY(!bbmin || !bbmax || !elements.size()))
	return false;

    // Everything but the last element should be a comb - we only need to
    // assemble a matrix from the path (if there are any non-identity matrices)
    // and call get_bbox on the last element.
    bool have_mat = false;
    mat_t start_mat;
    MAT_IDN(start_mat);
    for (size_t i = 0; i < elements.size() - 1; i++) {
	mat_t nm;
	MAT_IDN(nm);
	bool got_mat = get_matrix(nm, elements[i], elements[i+1]);
	if (got_mat) {
	    mat_t cmat;
	    bn_mat_mul(cmat, start_mat, nm);
	    MAT_COPY(start_mat, cmat);
	    have_mat = true;
	}
    }
    if (have_mat) {
	return get_bbox(bbmin, bbmax, start_mat, elements[elements.size() - 1]);
    }

    return get_bbox(bbmin, bbmax, NULL, elements[elements.size() - 1]);
}
#endif

BViewState *
DbiState::get_view_state(struct bview *v)
{
    if (!v->independent)
	return shared_vs;
    if (view_states.find(v) != view_states.end())
	return view_states[v];

    BViewState *nv = new BViewState(this);
    view_states[v] = nv;
    return nv;
}

std::vector<BSelectState *>
DbiState::get_selected_states(const char *sname)
{
    std::vector<BSelectState *> ret;
    std::unordered_map<std::string, BSelectState *>::iterator ss_it;

    if (!sname || BU_STR_EQUIV(sname, "default")) {
	ret.push_back(default_selected);
	return ret;
    }

    std::string sn(sname);
    if (sn.find('*') != std::string::npos) {
	for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	    if (bu_path_match(sname, ss_it->first.c_str(), 0)) {
		ret.push_back(ss_it->second);
	    }
	}
	return ret;
    }

    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (BU_STR_EQUIV(sname, ss_it->first.c_str())) {
	    ret.push_back(ss_it->second);
	}
    }
    if (ret.size())
	return ret;

    BSelectState *ns = new BSelectState(this);
    selected_sets[sn] = ns;
    ret.push_back(ns);
    return ret;
}

BSelectState *
DbiState::find_selected_state(const char *sname)
{
    if (!sname || BU_STR_EQUIV(sname, "default")) {
	return default_selected;
    }

    std::unordered_map<std::string, BSelectState *>::iterator ss_it;
    for (ss_it = selected_sets.begin(); ss_it != selected_sets.end(); ss_it++) {
	if (BU_STR_EQUIV(sname, ss_it->first.c_str())) {
	    return ss_it->second;
	}
    }

    return NULL;
}

void
DbiState::put_selected_state(const char *sname)
{
    if (!sname || BU_STR_EQUIV(sname, "default")) {
	default_selected->clear();
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
DbiState::list_selection_sets()
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
	unsigned long long c_hash, DbiPath &p
	)
{
    GObj *g = gobjs[c_hash];
    p.add(g->hash);
    if (!p.cyclic()) {
	/* Not cyclic - keep going */
	for (size_t i = 0; i < g->cv.size(); i++) {
	    CombInst *c = g->cv[i];
	    gather_cyclic(cyclic, c->ohash, p);
	}
    } else {
	cyclic.insert(c_hash);
    }

    /* Done with branch - restore path.  Disable validation since we are doing
     * a controlled walk and know we're not cyclic after the pop. */
    p.pop(true);
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

    // If we also want cyclic paths, use DbiState to try and speed things up.
    // db_ls has that capability, but it has to unpack all the combs walking
    // the tree to find the answer and that results in a slow check for large
    // databases.
    std::unordered_set<unsigned long long> cyclic_paths;
    for (size_t i = 0; i < ret.size(); i++) {
	DbiPath p(this);
	gather_cyclic(cyclic_paths, ret[i], p);
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

    std::unordered_set<unsigned long long>::iterator s_it;
    std::unordered_set<struct directory *>::iterator g_it;

    if (need_update_nref) {
	db_update_nref(dbip, res);
	need_update_nref = false;
    }

    // For objects that are removed, delete the GObj and its associated data.
    // Removal of an instance from a comb tree is handled as an object change,
    // and thus is not handled here.
    for (s_it = removed.begin(); s_it != removed.end(); s_it++) {
	bu_log("removed: %llu\n", *s_it);
	if (gobjs.find(*s_it) == gobjs.end())
	    continue;
	GObj *robj = gobjs[*s_it];
	delete robj;
	gobjs.erase(*s_it);
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
	BViewState *bvs = gedp->dbi_state->get_view_state(v);
	if (!bvs)
	    continue;
	vmap[bvs].insert(v);
    }
    std::unordered_map<BViewState *, std::unordered_set<struct bview *>>::iterator bv_it;
    for (bv_it = vmap.begin(); bv_it != vmap.end(); bv_it++) {
	bv_it->first->redraw(NULL, bv_it->second, 1, changed_hashes);
    }

    // Updates done, clear items stored by callbacks
    added.clear();
    changed.clear();
    removed.clear();

    return ret;
}

void
DbiState::expand_path(std::vector<DbiPath *> *opaths, DbiPath &p)
{
    // Unpack the leaf GObj of p
    GObj *g = NULL;
    if (p.depth()) {
	// Leaf is CombInst - get oname GObj
	CombInst *c = combinsts[p.elements[p.elements.size() - 1]];
	std::unordered_map<unsigned long long, GObj *>::iterator g_it = gobjs.find(c->ohash);
	if (g_it == gobjs.end()) {
	    // If CombInst oname isn't a valid object, then the path
	    // ends here and we just copy p into out_paths
	    DbiPath *op = new DbiPath(this);
	    op->copy(p);
	    opaths->push_back(op);
	    return;
	}
	g = g_it->second;
    } else {
	// Top level path - just get GObj
	g = gobjs[p.elements[0]];
    }

    // If g is a solid, this is a leaf path and we're done
    if (!g->cv.size()) {
	DbiPath *op = new DbiPath(this);
	op->copy(p);
	opaths->push_back(op);
	return;
    }

    // If we have a comb, process the instances.
    for (size_t i = 0; i < g->cv.size(); i++) {
	p.add(g->cv[i]->ihash);
	expand_path(opaths, p);
	p.pop();
    }
}

std::vector<DbiPath *>
DbiState::expand(std::vector<DbiPath *> paths)
{
    std::vector<DbiPath *> out_paths;
    for (size_t i = 0; i < paths.size(); i++) {
	DbiPath *p = paths[i];
	if (!p->valid())
	    continue;

	DbiPath wp(this);
	wp.copy(*p);
	expand_path(&out_paths, wp);
    }

    return out_paths;
}

std::vector<DbiPath *>
DbiState::collapse(std::vector<DbiPath *> paths)
{
    std::vector<DbiPath *> out_paths;
    std::map<size_t, std::vector<DbiPath *>> depth_groups;

    // Group paths of the same depth.  Depth == 0 paths are top level
    // and need no collapsing
    for (size_t i = 0; i < paths.size(); i++) {
	DbiPath *op = new DbiPath(this);
	op->copy(*paths[i]);
	size_t depth = paths[i]->depth();
	if (!depth) {
	    out_paths.push_back(op);
	    continue;
	}
	depth_groups[i].push_back(op);
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
		leaf_hashes.insert(lp->elements[lp->elements.size() - 1]);
	    }

	    // We need the full list of children from the CombInst.  Since the
	    // grouped paths all have the same parent - just use the first path
	    // to get the leaf CombInst, which will tell us where the GObj
	    // containing the full list of child CombInsts can be found.
	    DbiPath *p = cpaths[0];
	    CombInst *c = combinsts[p->elements[p->elements.size() - 1]];
	    GObj *pg = gobjs[c->chash];
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
		cp->pop();
		depth_groups[plen - 1].push_back(cp);
		// Remainder of paths can be freed
		for (size_t i = 1; i < cpaths.size(); i++)
		    delete cpaths[i];
	    } else {
		// No further collapsing - add to final.
		for (size_t i = 0; i < cpaths.size(); i++)
		    out_paths.push_back(cpaths[i]);
	    }
	}

	// Done with this depth
	depth_groups.erase(plen);
    }

    // If we collapsed all the way to top level objects, make sure to add them
    if (depth_groups.find(0) != depth_groups.end()) {
	std::vector<DbiPath *> &dpaths = depth_groups.rbegin()->second;
	for (size_t i = 0; i < dpaths.size(); i++) {
	    out_paths.push_back(dpaths[i]);
	}
    }

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
