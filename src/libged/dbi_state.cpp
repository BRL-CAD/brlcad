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

// alphanum sort
static bool alphanum_cmp(const std::string &a, const std::string &b)
{
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

struct walk_data {
    DbiState *dbis = NULL;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};

static void
populate_leaf(void *client_data, const char *name, matp_t c_m, int op)
{
    struct walk_data *d = (struct walk_data *)client_data;
    struct db_i *dbip = d->dbis->dbip;
    RT_CHECK_DBI(dbip);

    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    unsigned long long chash = bu_data_hash(name, strlen(name)*sizeof(char));
    i_count[chash] += 1;
    if (i_count[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, i_count[chash] - 1);
	unsigned long long ihash = bu_data_hash(bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	d->dbis->i_map[ihash] = chash;
	d->dbis->i_str[ihash] = std::string(bu_vls_cstr(&iname));
	d->dbis->p_c[d->phash].insert(ihash);
	d->dbis->p_v[d->phash].push_back(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->invalid_entry_map.erase(ihash);
	}
	bu_vls_free(&iname);

	// For the next stages, if we have an ihash use it
	chash = ihash;

    } else {

	d->dbis->p_v[d->phash].push_back(chash);
	d->dbis->p_c[d->phash].insert(chash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->dbis->invalid_entry_map[chash] = std::string(name);
	} else {
	    // In case this was previously invalid, remove
	    d->dbis->invalid_entry_map.erase(chash);
	}

    }

    // If we have a non-IDN matrix, store it
    if (c_m) {
	for (int i = 0; i < 16; i++)
	    d->dbis->matrices[d->phash][chash].push_back(c_m[i]);
    }

    // If we have a non-UNION op, store it
    d->dbis->i_bool[d->phash][chash] = op;
}

static void
populate_walk_tree(union tree *tp, void *d, int subtract_skip, int p_op,
	void (*leaf_func)(void *, const char *, matp_t, int)
	)
{
    if (!tp)
	return;

    RT_CK_TREE(tp);

    int op = p_op;
    switch (tp->tr_op) {
	case OP_SUBTRACT:
	    op = OP_SUBTRACT;
	    break;
	case OP_INTERSECT:
	    op = OP_INTERSECT;
	    break;
    };


    switch (tp->tr_op) {
	case OP_SUBTRACT:
	    if (subtract_skip)
		return;
	    /* fall through */
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    populate_walk_tree(tp->tr_b.tb_right, d, subtract_skip, op, leaf_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    populate_walk_tree(tp->tr_b.tb_left, d, subtract_skip, OP_UNION, leaf_func);
	    break;
	case OP_DB_LEAF:
	    (*leaf_func)(d, tp->tr_l.tl_name, tp->tr_l.tl_mat, op);
	    break;
	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("draw walk\n");
    }
}


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
	    update_dp(dp, 0);
	}
    }

    elapsed = bu_gettime() - start;
    seconds = elapsed / 1000000.0;

    bu_log("DbiState init: %f sec\n", seconds);


    start = bu_gettime();

    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    update_dp2(dp);
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


void
DbiState::populate_maps(struct directory *dp, unsigned long long phash, int reset)
{
    if (!(dp->d_flags & RT_DIR_COMB))
	return;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
    pc_it = p_c.find(phash);
    pv_it = p_v.find(phash);
    if (pc_it == p_c.end() || pv_it != p_v.end() || reset) {
	if (reset && pc_it != p_c.end()) {
	    pc_it->second.clear();
	}
	if (reset && pv_it != p_v.end()) {
	    pv_it->second.clear();
	}
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, dbip, NULL, res) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	struct walk_data d;
	d.dbis = this;
	d.phash = phash;
	populate_walk_tree(comb->tree, (void *)&d, 0, OP_UNION, populate_leaf);
	rt_db_free_internal(&in);
    }
}

unsigned long long
DbiState::path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    size_t mlen = (max_len) ? max_len : path.size();
    return bu_data_hash(path.data(), mlen * sizeof(unsigned long long));
}

static void
fp_path_split(std::vector<std::string> &objs, const char *str)
{
    std::string s(str);
    while (s.length() && s.c_str()[0] == '/')
	s.erase(0, 1);  //Remove leading slashes

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
name_deescape(std::string &name)
{
    std::string s(name);
    std::string nstr;

    for (size_t i = 0; i < s.length(); i++) {
	if (s[i] == '\\') {
	    if ((i+1) < s.length())
		nstr.push_back(s[i+1]);
	    i++;
	} else {
	    nstr.push_back(s[i]);
	}
    }

    return nstr;
}

// This is a full (and more expensive) check to ensure
// a path has no cycles anywhere in it.
bool
DbiState::path_cyclic(std::vector<unsigned long long> &path)
{
    unsigned long long chash, phash;
    std::unordered_map<unsigned long long, GObj *>::iterator r;
    std::unordered_map<unsigned long long, CombInst *>::iterator c, p;

    // A single GObj toplevel path can't be cyclic
    if (path.size() == 1)
	return false;

    // Make sure we have a valid root GObj
    r =  gobjs.find(path[0]);
    if (r == gobjs.end()) {
	// If we have an invalid GObj hash, we can't properly test - return
	// the worst-case assumption, which is true (path is cyclic)
	return true;
    }

    // Start with the last entry in the path
    int i = path.size() - 1;

    while (i > 0) {

	c =  combinsts.find(path[i]);
	if (c == combinsts.end()) {
	    // If we have an invalid Comb hash, we can't properly test - return
	    // the worst-case assumption, which is true (path is cyclic)
	    return true;
	}

	chash = c->second->ohash;
	int j = i - 1;

	while (j >= 0) {

	    if (j > 0) {

		p = combinsts.find(path[j]);

		if (p == combinsts.end()) {
		    // If we have an invalid Comb hash, we can't properly test - return
		    // the worst-case assumption, which is true (path is cyclic)
		    return true;
		}

		phash = p->second->ohash;

	    } else {

		phash = r->second->hash;

	    }

	    if (chash == phash)
		return true;

	    j--;
	}

	i--;
    }
    return false;
}

// This version of the cyclic check assumes the path entries other than the
// last one are OK, and checks only against that last entry.
bool
DbiState::path_addition_cyclic(std::vector<unsigned long long> &path)
{
    unsigned long long chash, phash;
    std::unordered_map<unsigned long long, GObj *>::iterator r;
    std::unordered_map<unsigned long long, CombInst *>::iterator c, p;

    // A single GObj toplevel path can't be cyclic
    if (path.size() == 1)
	return false;

    // Make sure we have a valid root GObj
    r =  gobjs.find(path[0]);
    if (r == gobjs.end()) {
	// If we have an invalid GObj hash, we can't properly test - return
	// the worst-case assumption, which is true (path is cyclic)
	return true;
    }

    // Get the last entry in the path
    c =  combinsts.find(path[path.size() - 1]);
    if (c == combinsts.end()) {
	// If we have an invalid Comb hash, we can't properly test - return
	// the worst-case assumption, which is true (path is cyclic)
	return true;
    }

    chash = c->second->ohash;

    int j = path.size() - 2;

    while (j >= 0) {

	if (j > 0) {

	    p = combinsts.find(path[j]);

	    if (p == combinsts.end()) {
		// If we have an invalid Comb hash, we can't properly test - return
		// the worst-case assumption, which is true (path is cyclic)
		return true;
	    }

	    phash = p->second->ohash;

	} else {

	    phash = r->second->hash;

	}

	if (chash == phash)
	    return true;

	j--;
    }

    return false;
}

std::vector<unsigned long long>
DbiState::digest_path(const char *path)
{
    // If no path, nothing to process
    if (!path)
	return std::vector<unsigned long long>();

    // Digest the string into individual path elements
    std::vector<std::string> elements, substrs;
    fp_path_split(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	elements.push_back(cleared);
    }

    // Convert the string elements into hash elements.
    // The first element is handled as a gobj - beyond that,
    // the hash is performed on the parent/child combo to get
    // the CombInst hash
    std::vector<unsigned long long> phe;
    phe.push_back(bu_data_hash(elements[0].c_str(), strlen(elements[0].c_str())*sizeof(char)));
    for (size_t i = 1; i < elements.size(); i++) {
	unsigned long long lhash = bu_data_hash(elements[i].c_str(), strlen(elements[0].c_str())*sizeof(char));
	std::vector<unsigned long long> lvec;
	lvec.push_back(phe[0]);
	lvec.push_back(lhash);
	phe.push_back(bu_data_hash(lvec.data(), lvec.size() * sizeof(unsigned long long)));
    }

    // If we're cyclic, path is invalid
    if (path_cyclic(phe))
	return std::vector<unsigned long long>();

    // If we're not valid, it's no dice
    if (!valid_hash_path(phe)) {
	// If we don't have a valid hash path, try to tell the user more about
	// *why* it is invalid.
	unsigned long long phash = phe[0];
	if (gobjs.find(phash) == gobjs.end()) {
	    bu_log("Invalid first path element (must be GObj): %s\n", elements[0].c_str());
	    return std::vector<unsigned long long>();
	}
	for (size_t i = 1; i < phe.size(); i++) {
	    if (combinsts.find(phe[i]) == combinsts.end()) {
		bu_log("CombInst not found for path element # %zd %s/%s\n", i, elements[i-1].c_str(), elements[i].c_str());
		return std::vector<unsigned long long>();
	    }

	    CombInst *ci = combinsts[phe[i]];

	    // Verify that ci's chash matches phash
	    if (ci->chash != phash) {
		bu_log("Invalid comb instance - chash mismatch %s/%s\n", elements[i-1].c_str(), elements[i].c_str());
		return std::vector<unsigned long long>();
	    }

	    // The ohash of this comb instance now becomes
	    // the parent for the next check
	    phash = ci->ohash;
	}

	bu_log("Unknown digest_path validation failure\n");
	return std::vector<unsigned long long>();
    }

    return phe;
}

bool
DbiState::valid_hash(unsigned long long phash)
{
    if (!phash)
	return false;

    // There are two possibilities for a hash to be valid - it can
    // be either a GObj hash or a CombInst hash.  Most of the time
    // it will be a comb instance, so try that first

    if (combinsts.find(phash) != combinsts.end())
	return true;

    if (gobjs.find(phash) != gobjs.end())
	return true;

    return false;
}

bool
DbiState::valid_hash_path(std::vector<unsigned long long> &phashes)
{
    unsigned long long phash = phashes[0];
    if (gobjs.find(phash) == gobjs.end())
	return false;
    for (size_t i = 1; i < phashes.size(); i++) {
	if (combinsts.find(phashes[i]) == combinsts.end())
	    return false;
	CombInst *ci = combinsts[phashes[i]];
	if (ci->chash != phash)
	    return false;
	phash = ci->ohash;
    }
    return true;
}

bool
DbiState::print_hash(struct bu_vls *opath, unsigned long long phash)
{
    if (!phash)
	return false;

    if (combinsts.find(phash) != combinsts.end()) {
	CombInst *c = combinsts[phash];
	bu_vls_printf(opath, "%s", (c->iname.length()) ? c->iname.c_str() : c->oname.c_str());
	return true;
    }

    if (gobjs.find(phash) != gobjs.end()) {
	GObj *g = gobjs[phash];
	bu_vls_printf(opath, "%s", g->name.c_str());
	return true;
    }

    bu_exit(EXIT_FAILURE, "DbiState::print_hash failure, dbi_state.cpp::%d - a hash not known to the database's DbiState was passed in.  This can happen when the dbip contents change and dbi_state->update() isn't called in the parent application after doing so.\n", __LINE__);
    bu_vls_printf(opath, "\nERROR!!!\n");
    return false;
}

void
DbiState::print_path(struct bu_vls *opath, std::vector<unsigned long long> &path, size_t pmax, int verbose)
{
    if (!opath || !path.size())
	return;

    bu_vls_trunc(opath, 0);
    for (size_t i = 0; i < path.size(); i++) {
	if (pmax && i == pmax)
	    break;
	if (i > 0 && verbose) {
	    if (combinsts.find(path[i]) != combinsts.end()) {
		if (combinsts[path[i]]->non_default_matrix)
		    bu_vls_printf(opath, "[M]");
	    }
	}
	if (!print_hash(opath, path[i]))
	    continue;
	if (i < path.size() - 1 && (!pmax || i < pmax - 1))
	    bu_vls_printf(opath, "/");
    }
}

const char *
DbiState::pathstr(std::vector<unsigned long long> &path, size_t pmax)
{
    bu_vls_trunc(&path_string, 0);
    print_path(&path_string, path, pmax);
    return bu_vls_cstr(&path_string);
}


const char *
DbiState::hashstr(unsigned long long hash)
{
    bu_vls_trunc(&hash_string, 0);
    print_hash(&hash_string, hash);
    return bu_vls_cstr(&hash_string);
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

    bboxes.erase(hash);
    region_id.erase(hash);
    c_inherit.erase(hash);
    rgb.erase(hash);
}

unsigned long long
DbiState::update_dp(struct directory *dp, int reset)
{
    if (dp->d_flags & DB_LS_HIDDEN)
	return 0;

    // Set up to go from hash back to name
    unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
    d_map[hash] = dp;

    // Clear any (possibly) state bbox.  bbox calculation
    // can be expensive, so defer it until it's needed
    bboxes.erase(hash);

    // Encode hierarchy info if this is a comb
    if (dp->d_flags & RT_DIR_COMB)
	populate_maps(dp, hash, reset);

    // Check for various drawing related attributes
    // Ideally, if we have enough info, we'd like to avoid loading
    // the avs.  See if we can get away with it using dcache
    struct bu_attribute_value_set c_avs = BU_AVS_INIT_ZERO;
    bool loaded_avs = false;
    region_id.erase(hash);
    c_inherit.erase(hash);
    rgb.erase(hash);

    // First, check the dcache for all remaining needed values
    const char *b = NULL;
    size_t bsize = 0;

    bool need_region_id_avs = true;
    bool need_region_flag_avs = true;
    bool need_color_inherit_avs = true;
    //bool need_cval_avs = true;

    int region_flag = 0;
    int attr_region_id = -1;
    int color_inherit = 0;
    //unsigned int cval = INT_MAX;

    bsize = cache_get(dcache, (void **)&b, hash, CACHE_REGION_ID);
    if (bsize == sizeof(attr_region_id)) {
	memcpy(&attr_region_id, b, sizeof(attr_region_id));
	need_region_id_avs = false;
    }
    cache_done(dcache);

    bsize = cache_get(dcache, (void **)&b, hash, CACHE_REGION_FLAG);
    if (bsize == sizeof(region_flag)) {
	memcpy(&region_flag, b, sizeof(region_flag));
	need_region_flag_avs = false;
    }
    cache_done(dcache);

    bsize = cache_get(dcache, (void **)&b, hash, CACHE_INHERIT_FLAG);
    if (bsize == sizeof(color_inherit)) {
	memcpy(&color_inherit, b, sizeof(color_inherit));
	need_color_inherit_avs = false;
    }
    cache_done(dcache);

#if 0
    bsize = cache_get(dcache, (void **)&b, hash, CACHE_COLOR);
    if (bsize == sizeof(cval)) {
	memcpy(&cval, b, sizeof(cval));
	need_cval_avs = false;
    }
    cache_done(dcache);
#endif

    if (need_region_flag_avs) {
	if (!loaded_avs) {
	    db5_get_attributes(dbip, &c_avs, dp);
	    loaded_avs = true;
	}
	// Check for region flag.
	const char *region_flag_str = bu_avs_get(&c_avs, "region");
	if (region_flag_str && (BU_STR_EQUAL(region_flag_str, "R") || BU_STR_EQUAL(region_flag_str, "1"))) {
	    region_flag = 1;
	}

	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&region_flag), sizeof(region_flag));
	cache_write(dcache, hash, CACHE_REGION_FLAG, s);
    }


    if (need_region_id_avs) {
	if (!loaded_avs) {
	    db5_get_attributes(dbip, &c_avs, dp);
	    loaded_avs = true;
	}
	// Check for region id.  For drawing purposes this needs to be a number.
	const char *region_id_val = bu_avs_get(&c_avs, "region_id");
	if (region_id_val)
	    bu_opt_int(NULL, 1, &region_id_val, (void *)&attr_region_id);

	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&attr_region_id), sizeof(attr_region_id));
	cache_write(dcache, hash, CACHE_REGION_ID, s);
    }

    if (need_color_inherit_avs) {
	if (!loaded_avs) {
	    db5_get_attributes(dbip, &c_avs, dp);
	    loaded_avs = true;
	}
	color_inherit = (BU_STR_EQUAL(bu_avs_get(&c_avs, "inherit"), "1")) ? 1 : 0;

	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&color_inherit), sizeof(color_inherit));
	cache_write(dcache, hash, CACHE_INHERIT_FLAG, s);
    }

#if 0
    if (need_cval_avs) {
	if (!loaded_avs) {
	    db5_get_attributes(dbip, &c_avs, dp);
	    loaded_avs = true;
	}
	// Color (note that the rt_material_head colors and a region_id may
	// override this, as might a parent comb with color and the inherit
	// flag both set.
	rgb.erase(hash);
	struct bu_color c = BU_COLOR_INIT_ZERO;
	const char *color_val = bu_avs_get(&c_avs, "color");
	if (!color_val)
	    color_val = bu_avs_get(&c_avs, "rgb");
	if (color_val){
	    bu_opt_color(NULL, 1, &color_val, (void *)&c);
	    cval = color_int(&c);
	    bu_log("have color: %u\n", cval);
	}

	std::stringstream s;
	s.write(reinterpret_cast<const char *>(&cval), sizeof(cval));
	cache_write(dcache, hash, CACHE_COLOR, s);
    }
#endif

    // If a region flag is set but a region_id is not, there is an implicit
    // assumption that the region_id is to be regarded as 0.  Not sure this
    // will always be true, but right now region table based coloring works
    // that way in existing BRL-CAD code (see the example m35.g model's
    // all.g/component/power.train/r75 for an instance of this)
    if (region_flag && attr_region_id == -1)
	attr_region_id = 0;


    if (attr_region_id != -1)
	region_id[hash] = attr_region_id;
    if (color_inherit)
	c_inherit[hash] = color_inherit;
    //if (cval != INT_MAX)
	//rgb[hash] = cval;

    // Done with attributes
    if (loaded_avs) {
	bu_log("Had to load avs\n");
	bu_avs_free(&c_avs);
    }
    return hash;
}

unsigned long long
DbiState::update_dp2(struct directory *dp)
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
	// TODO  - if we hit a CombInt without the associated GObj, should we
	// error out?
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
	// TODO  - if we hit a CombInt without the associated GObj, should we
	// error out?
	if (combinsts.find(path[i]) == combinsts.end())
	    continue;
	cis.push_back(combinsts[path[i]]);
    }
    return cis;
}

bool
DbiState::path_color(struct bu_color *c, std::vector<unsigned long long> &elements)
{
    const struct mater *mp;

    std::vector<GObj *> cgs = get_gobjs(elements);

    // This may not be how we'll always want to do this, but at least for the
    // moment (to duplicate observed MGED behavior) the first region_id seen
    // along the path with an active color in rt_material_head trumps all other
    // color values set by any other means.
    if (rt_material_head() != MATER_NULL) {
	for (size_t i = 0; i < cgs.size(); i++) {
	    if (cgs[i]->region_id >= 0) {
		for (mp = rt_material_head(); mp != MATER_NULL; mp = mp->mt_forw) {
		    if (cgs[i]->region_id > mp->mt_high || cgs[i]->region_id < mp->mt_low)
			continue;
		    unsigned char mt[3];
		    mt[0] = mp->mt_r;
		    mt[1] = mp->mt_g;
		    mt[2] = mp->mt_b;
		    bu_color_from_rgb_chars(c, mt);
		    return true;
		}
	    }
	}
    }

    // Next, check for an inherited color.  If we have one (the behavior seen in MGED
    // appears to require a comb with both inherit and a color value set to override
    // lower colors) then we are done.
    for (size_t i = 0; i < cgs.size(); i++) {
	if (!cgs[i]->c_inherit)
	    continue;
	if (!cgs[i]->color_set)
	    continue;
	BU_COLOR_CPY(c, &cgs[i]->color);
	return true;
    }

    // If we don't have an inherited color, it works the other way around - the
    // lowest set color wins.  Note that a region flag doesn't automatically
    // override a lower color level - i.e. there is no implicit inherit flag
    // in a region being set on a comb.
    for (long long i = cgs.size()-1; i >= 0; i--) {
	if (!cgs[i]->color_set)
	    continue;
	BU_COLOR_CPY(c, &cgs[i]->color);
	return true;
    }

    // If we don't have anything else, default to red
    unsigned char mt[3];
    mt[0] = 255;
    mt[1] = 0;
    mt[2] = 0;
    bu_color_from_rgb_chars(c, mt);
    return false;
}

bool
DbiState::path_is_subtraction(std::vector<unsigned long long> &elements)
{
    if (elements.size() < 2)
	return false;

    unsigned long long phash = elements[0];
    for (size_t i = 1; i < elements.size(); i++) {
	unsigned long long chash = elements[i];
	std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, size_t>>::iterator i_it;
	i_it = i_bool.find(phash);
	if (i_it == i_bool.end())
	    return false;
	std::unordered_map<unsigned long long, size_t>::iterator ib_it;
	ib_it = i_it->second.find(chash);
	if (ib_it == i_it->second.end())
	    return false;

	if (ib_it->second == OP_SUBTRACT)
	    return true;

	phash = chash;
    }

    return false;
}

db_op_t
DbiState::bool_op(unsigned long long phash, unsigned long long chash)
{
    if (!phash)
	return DB_OP_UNION;
    size_t op = i_bool[phash][chash];
    if (op == OP_SUBTRACT) {
	return DB_OP_SUBTRACT;
    }
    if (op == OP_INTERSECT)
	return DB_OP_INTERSECT;
    return DB_OP_UNION;
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

bool
DbiState::get_matrix(matp_t m, unsigned long long p_key, unsigned long long i_key)
{
    if (UNLIKELY(!m || p_key == 0 || i_key == 0))
	return false;

    std::unordered_map<unsigned long long, std::unordered_map<unsigned long long, std::vector<fastf_t>>>::iterator m_it;
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator mv_it;
    m_it = matrices.find(p_key);
    if (m_it == matrices.end())
	return false;
    mv_it = m_it->second.find(i_key);
    if (mv_it == m_it->second.end())
	return false;

    // If we got this far, we have an index into the matrices vector.  Assign
    // the result to m
    std::vector<fastf_t> &mv = mv_it->second;
    for (size_t i = 0; i < 16; i++)
	m[i] = mv[i];

    return true;
}

bool
DbiState::get_path_matrix(matp_t m, std::vector<unsigned long long> &elements)
{
    bool have_mat = false;
    if (UNLIKELY(!m))
	return false;

    MAT_IDN(m);
    if (elements.size() < 2)
	return false;

    std::vector<CombInst *> cis = get_combinsts(elements);
    for (size_t i = 1; i < cis.size(); i++) {
	if (cis[i]->non_default_matrix) {
	    mat_t cmat;
	    bn_mat_mul(cmat, m, cis[i]->m);
	    MAT_COPY(m, cmat);
	    have_mat = true;
	}
    }

    return have_mat;
}

// TODO - this should be pushed to the GObj and CombInst classes
bool
DbiState::get_bbox(point_t *bbmin, point_t *bbmax, matp_t curr_mat, unsigned long long hash)
{

    if (UNLIKELY(!bbmin || !bbmax || hash == 0))
	return false;

    bool ret = false;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator s_it;
    unsigned long long key = hash;
    // First, see if this is an instance we need to translate to its canonical
    // .g database name
    if (i_map.find(hash) != i_map.end())
	key = i_map[hash];

    // See if we have a direct bbox lookup available
    std::unordered_map<unsigned long long, std::vector<fastf_t>>::iterator b_it;
    b_it = bboxes.find(key);
    if (b_it != bboxes.end()) {
	point_t lbmin, lbmax;
	lbmin[X] = b_it->second[0];
	lbmin[Y] = b_it->second[1];
	lbmin[Z] = b_it->second[2];
	lbmax[X] = b_it->second[3];
	lbmax[Y] = b_it->second[4];
	lbmax[Z] = b_it->second[5];

	if (curr_mat) {
	    point_t tbmin, tbmax;
	    MAT4X3PNT(tbmin, curr_mat, lbmin);
	    VMOVE(lbmin, tbmin);
	    MAT4X3PNT(tbmax, curr_mat, lbmax);
	    VMOVE(lbmax, tbmax);
	}

	VMINMAX(*bbmin, *bbmax, lbmin);
	VMINMAX(*bbmin, *bbmax, lbmax);
	return true;
    }

    // We might have a comb.  If that's the case, we need to work
    // through the hierarchy to get the bboxes of the children.
    pc_it = p_c.find(key);
    if (pc_it != p_c.end()) {
	// Have comb children - incorporate each one
	for (s_it = pc_it->second.begin(); s_it != pc_it->second.end(); s_it++) {
	    unsigned long long child_hash = *s_it;
	    // See if we have a matrix for this case - if so, we need to
	    // incorporate it
	    mat_t nm;
	    MAT_IDN(nm);
	    bool have_mat = get_matrix(nm, key, child_hash);
	    if (have_mat) {
		// Construct new "current" matrix
		if (curr_mat) {
		    // If we already have a non-IDN matrix from parent
		    // path elements, we need to multiply the matrices
		    // to accumulate the position changes
		    mat_t om;
		    MAT_COPY(om, curr_mat);
		    bn_mat_mul(curr_mat, om, nm);
		    if (get_bbox(bbmin, bbmax, curr_mat, child_hash))
			ret = true;
		    MAT_COPY(curr_mat, om);
		} else {
		    // If this is the first non-IDN matrix, we don't
		    // need to combine it with parent matrices
		    if (get_bbox(bbmin, bbmax, nm, child_hash))
			ret = true;
		}
	    } else {
		if (get_bbox(bbmin, bbmax, curr_mat, child_hash))
		    ret = true;
	    }
	}
    }

    // When we have an object that is not a comb, look up its pre-calculated
    // box and incorporate it into bmin/bmax.
    point_t bmin, bmax;
    bool have_bbox = false;

    // First, check the dcache
    const char *b = NULL;
    size_t bsize = cache_get(dcache, (void **)&b, hash, CACHE_OBJ_BOUNDS);
    if (bsize) {
	if (bsize != (sizeof(bmin) + sizeof(bmax))) {
	    bu_log("Incorrect data size found loading cached bounds data\n");
	} else {
	    memcpy(&bmin, b, sizeof(bmin));
	    b += sizeof(bmin);
	    memcpy(&bmax, b, sizeof(bmax));
	    //bu_log("cached: bmin: %f %f %f bbmax: %f %f %f\n", V3ARGS(bmin), V3ARGS(bmax));
	    have_bbox = true;
	}
    }
    cache_done(dcache);


    // This calculation can be expensive.  If we've already
    // got it stashed as part of LoD processing, use that
    // version.
    struct directory *dp = get_hdp(hash);
    if (!dp)
	return false;
    if (!have_bbox) {
	if (dp->d_minor_type == DB5_MINORTYPE_BRLCAD_BOT && gedp->ged_lod) {
	    key = bv_mesh_lod_key_get(gedp->ged_lod, dp->d_namep);
	    if (key) {
		struct bv_mesh_lod *lod = bv_mesh_lod_create(gedp->ged_lod, key);
		if (lod) {
		    VMOVE(bmin, lod->bmin);
		    VMOVE(bmax, lod->bmax);
		    have_bbox = true;

		    std::stringstream s;
		    s.write(reinterpret_cast<const char *>(&bmin), sizeof(bmin));
		    s.write(reinterpret_cast<const char *>(&bmax), sizeof(bmax));
		    cache_write(dcache, hash, CACHE_OBJ_BOUNDS, s);
		}
	    }
	}
    }

    // No LoD - ask librt
    if (!have_bbox) {
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	mat_t m;
	MAT_IDN(m);
	int bret = rt_bound_instance(&bmin, &bmax, dp, dbip, &ttol, &tol, &m, res);
	if (bret != -1) {
	    have_bbox = true;

	    std::stringstream s;
	    s.write(reinterpret_cast<const char *>(&bmin), sizeof(bmin));
	    s.write(reinterpret_cast<const char *>(&bmax), sizeof(bmax));
	    cache_write(dcache, hash, CACHE_OBJ_BOUNDS, s);
	}
    }

    if (have_bbox) {
	for (size_t j = 0; j < 3; j++)
	    bboxes[hash].push_back(bmin[j]);
	for (size_t j = 0; j < 3; j++)
	    bboxes[hash].push_back(bmax[j]);

	VMINMAX(*bbmin, *bbmax, bmin);
	VMINMAX(*bbmin, *bbmax, bmax);
	ret = true;
    }

    return ret;
}

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

void
DbiState::gather_cyclic(
	std::unordered_set<unsigned long long> &cyclic,
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = p_c.find(c_hash);

    path_hashes.push_back(c_hash);

    if (!path_addition_cyclic(path_hashes)) {
	/* Not cyclic - keep going */
	if (pc_it != p_c.end()) {
	    std::unordered_set<unsigned long long>::iterator c_it;
	    for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
		gather_cyclic(cyclic, *c_it, path_hashes);
	}
    } else {
	cyclic.insert(c_hash);
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();
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
    std::vector<unsigned long long> path_hashes;
    for (size_t i = 0; i < ret.size(); i++) {
	path_hashes.clear();
	gather_cyclic(cyclic_paths, ret[i], path_hashes);
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
	changed_hashes.clear();
	old_names.clear();
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

    // dps -> hashes
    changed_hashes.clear();
    for(g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	unsigned long long hash = bu_data_hash(dp->d_namep, strlen(dp->d_namep)*sizeof(char));
	changed_hashes.insert(hash);
    }

    // Update the primary data structures
    for(s_it = removed.begin(); s_it != removed.end(); s_it++) {
	bu_log("removed: %llu\n", *s_it);

	// Combs with this key in their child set need to be updated to refer
	// to it as an invalid entry.
	std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator pv_it;
	for (pv_it = p_v.begin(); pv_it != p_v.end(); pv_it++) {
	    for (size_t i = 0; i < pv_it->second.size(); i++) {
		if (i_map.find(pv_it->second[i]) != i_map.end()) {
		    invalid_entry_map[i_map[pv_it->second[i]]] = i_str[i_map[pv_it->second[i]]];
		} else {
		    invalid_entry_map[pv_it->second[i]] = old_names[*s_it];
		}
	    }
	}

	d_map.erase(*s_it);
	bboxes.erase(*s_it);
	c_inherit.erase(*s_it);
	rgb.erase(*s_it);
	region_id.erase(*s_it);
	matrices.erase(*s_it);
	i_bool.erase(*s_it);

	// We do not clear the instance maps (i_map and i_str) since those containers do not
	// guarantee uniqueness to one child object.  To remove entries no longer
	// used anywhere in the database, we have to confirm they are no longer needed on a global
	// basis in a subsequent garbage-collect operation.

	// Entries with this hash as their key are erased.
	p_c.erase(*s_it);
	p_v.erase(*s_it);
    }

    for(g_it = added.begin(); g_it != added.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("added: %s\n", dp->d_namep);
	unsigned long long hash = update_dp(dp, 0);

	// If this name was previously the source of an invalid reference,
	// it is no longer.
	invalid_entry_map.erase(hash);
    }

    for(g_it = changed.begin(); g_it != changed.end(); g_it++) {
	struct directory *dp = *g_it;
	bu_log("changed: %s\n", dp->d_namep);
	// Properties need to be updated - comb children, colors, matrices,
	// bounding box for solids, etc.
	update_dp(dp, 1);
    }

    // Garbage collect i_map and i_str
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator sk_it;
    std::unordered_set<unsigned long long> used;
    for (sk_it = p_v.begin(); sk_it != p_v.end(); sk_it++) {
	used.insert(sk_it->second.begin(), sk_it->second.end());
    }
    std::vector<unsigned long long> unused;
    std::unordered_map<unsigned long long, unsigned long long>::iterator im_it;
    for (im_it = i_map.begin(); im_it != i_map.end(); im_it++) {
	if (used.find(im_it->first) != used.end())
	    unused.push_back(im_it->first);
    }
    for (size_t i = 0; i < unused.size(); i++) {
	i_map.erase(unused[i]);
	i_str.erase(unused[i]);
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
	bv_it->first->redraw(NULL, bv_it->second, 1);
    }

    // Updates done, clear items stored by callbacks
    added.clear();
    changed.clear();
    changed_hashes.clear();
    removed.clear();
    old_names.clear();

    return ret;
}

void
DbiState::print_leaves(
	std::set<std::string> &leaves,
	unsigned long long c_hash,
	std::vector<unsigned long long> &path_hashes
	)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    path_hashes.push_back(c_hash);

    bool leaf = path_addition_cyclic(path_hashes);

    if (!leaf) {
	/* Not cyclic - keep going */
	pc_it = p_c.find(c_hash);
	if (pc_it == p_c.end()) {
	    leaf = true;
	}
    }

    if (!leaf) {
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++)
	    print_leaves(leaves, *c_it, path_hashes);
    }

    // Print leaf
    if (leaf) {
	struct bu_vls p = BU_VLS_INIT_ZERO;
	print_path(&p, path_hashes, 0, 1);
	leaves.insert(std::string(bu_vls_cstr(&p)));
	bu_vls_free(&p);
    }

    /* Done with branch - restore path */
    path_hashes.pop_back();
}

void
DbiState::print_dbi_state(struct bu_vls *outvls, bool report_view_states)
{
    struct bu_vls *o = outvls;
    if (!o) {
	BU_GET(o, struct bu_vls);
	bu_vls_init(o);
    }

    std::vector<unsigned long long> top_objs = tops(true);
    std::set<std::string> leaves;
    // Report each path to its leaves (or to cyclic termination)
    std::vector<unsigned long long> path_hashes;
    for (size_t i = 0; i < top_objs.size(); i++) {
	path_hashes.clear();
	print_leaves(leaves, top_objs[i], path_hashes);
    }

    std::set<std::string>::iterator l_it;
    for (l_it = leaves.begin(); l_it != leaves.end(); l_it++)
	bu_vls_printf(o, "%s\n", l_it->c_str());

    if (report_view_states) {
	if (gedp->ged_gvp) {
	    BViewState *vs = get_view_state(gedp->ged_gvp);
	    bu_vls_printf(o, "\nDefault:\n");
	    vs->print_view_state(o);
	}
	if (view_states.size()) {
	    std::unordered_map<struct bview *, BViewState *>::iterator v_it;
	    std::map<std::string, std::set<BViewState *>> oviews;
	    for (v_it = view_states.begin(); v_it != view_states.end(); v_it++) {
		if (v_it->first == gedp->ged_gvp)
		    continue;
		std::string vname(bu_vls_cstr(&v_it->first->gv_name));
		oviews[vname].insert(v_it->second);
	    }
	    if (oviews.size()) {
		bu_vls_printf(o, "\nViews:\n");
		std::map<std::string, std::set<BViewState *>>::iterator o_it;
		for (o_it = oviews.begin(); o_it != oviews.end(); o_it++) {
		    std::set<BViewState *> &vset = o_it->second;
		    if (vset.size() > 1) {
			std::cout << "Warning:  " << vset.size() << " views with name " << o_it->first << "\n";
		    }
		    std::set<BViewState *>::iterator vs_it;
		    for (vs_it = vset.begin(); vs_it != vset.end(); vs_it++) {
			bu_vls_printf(o, "\n%s:\n", o_it->first.c_str());
			(*vs_it)->print_view_state(o);
		    }
		}
	    }
	}
    }

    if (!outvls)
	std::cout << bu_vls_cstr(o) << "\n";

    if (o != outvls) {
	bu_vls_free(o);
	BU_PUT(o, struct bu_vls);
    }
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
