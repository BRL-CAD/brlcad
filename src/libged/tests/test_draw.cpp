/*                    T E S T _ D R A W . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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
/** @file test_draw.cpp
 *
 * Experiment with approaches for managing drawing and selecting
 *
 */

#include "common.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"
}

#include <stdio.h>
#include <bu.h>
#include <ged.h>

struct draw_ctx {
    // This map is the ".g ground truth" of the comb structures - the set associated
    // with each has contains all the child hashes from the comb definition in the
    // database.  It can be used to tell if a comb is fully drawn.
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> p_c;

    // Translate individual object hashes to their directory names.  This map must
    // be updated any time a database object changes to remain valid.
    std::unordered_map<unsigned long long, struct directory *> d_map;

    // For invalid comb entry strings, we can't point to a directory pointer.  This
    // map must also be updated after every db change - if a directory pointer hash
    // maps to an entry in this map it needs to be removed, and newly invalid entries
    // need to be added.
    std::unordered_map<unsigned long long, std::string> invalid_entry_map;

    // This is a map of non-uniquely named child instances (i.e. instances that must be
    // numbered) to the .g database name associated with those instances.  Allows for
    // one unique entry in p_c rather than requiring per-instance duplication
    std::unordered_map<unsigned long long, unsigned long long> i_map;
    std::unordered_map<unsigned long long, std::string> i_str;
    // TODO - the opposite mapping of i_map - allow a comb edit to also get all
    // instance using paths
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> i_sets;


    // The above containers are universal to the .g - the following will need
    // both shared and view specific instances

    // Sets defining all drawn solid paths (including invalid paths).  The
    // s_keys holds the ordered individual keys of each drawn solid path - it
    // is the latter that allows for the collapse operation to populate
    // drawn_paths.  s_map uses the same key as s_keys to map instances to
    // actual scene objects.
    std::unordered_map<unsigned long long, struct bv_scene_obj *> s_map;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>> s_keys;

    // Set of hashes of all drawn paths and subpaths, constructed during the collapse
    // operation from the set of drawn solid paths.  This allows calling codes to
    // spot check any path to see if it is active, without having to interrogate
    // other data structures or walk down the tree.
    std::unordered_set<unsigned long long> drawn_paths;
};

void
print_path(struct bu_vls *opath, std::vector<unsigned long long> &path, struct draw_ctx *ctx)
{
    if (!opath || !path.size() || !ctx)
	return;
    bu_vls_trunc(opath, 0);
    for (size_t i = 0; i < path.size(); i++) {
	// First, see if the hash is an instance string
	if (ctx->i_str.find(path[i]) != ctx->i_str.end()) {
	    bu_vls_printf(opath, "%s", ctx->i_str[path[i]].c_str());
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	// If not, try the directory pointer
	if (ctx->d_map.find(path[i]) != ctx->d_map.end()) {
	    bu_vls_printf(opath, "%s", ctx->d_map[path[i]]->d_namep);
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	// Last option - invalid string
	if (ctx->invalid_entry_map.find(path[i]) != ctx->invalid_entry_map.end()) {
	    bu_vls_printf(opath, "%s", ctx->invalid_entry_map[path[i]].c_str());
	    if (i < path.size() - 1)
		bu_vls_printf(opath, "/");
	    continue;
	}
	bu_vls_printf(opath, "ERROR!!!");
    }
}


void
print_ctx(struct draw_ctx *ctx)
{
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator cs_it;
    for (pc_it = ctx->p_c.begin(); pc_it != ctx->p_c.end(); pc_it++) {
	bool found_entry = false;
	std::unordered_map<unsigned long long, struct directory *>::iterator dpn = ctx->d_map.find(pc_it->first);
	if (dpn != ctx->d_map.end()) {
	    bu_log("%s	(%llu):\n", dpn->second->d_namep, pc_it->first);
	    found_entry = true;
	}
	if (!found_entry) {
	    unsigned long long chash = ctx->i_map[pc_it->first];
	    dpn = ctx->d_map.find(chash);
	    if (dpn != ctx->d_map.end()) {
		bu_log("%s	(%llu->%llu):\n", dpn->second->d_namep, pc_it->first, chash);
		found_entry = true;
	    }
	}
	if (!found_entry) {
	    std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(pc_it->first);
	    if (en != ctx->invalid_entry_map.end()) {
		bu_log("%s[I]	(%llu)\n", en->second.c_str(), pc_it->first);
		found_entry = true;
	    } else {
		bu_log("P ERROR: %llu\n", pc_it->first);
	    }
	}
	if (!found_entry)
	    continue;

	for (cs_it = pc_it->second.begin(); cs_it != pc_it->second.end(); cs_it++) {
	    found_entry = false;
	    dpn = ctx->d_map.find(*cs_it);
	    if (dpn != ctx->d_map.end()) {
		bu_log("	%s	(%llu)\n", dpn->second->d_namep, *cs_it);
		found_entry = true;
	    }
	    if (!found_entry) {
		unsigned long long chash = ctx->i_map[*cs_it];
		dpn = ctx->d_map.find(chash);
		if (dpn != ctx->d_map.end()) {
		    bu_log("	%s	(%llu->%llu)\n", dpn->second->d_namep, *cs_it, chash);
		    found_entry = true;
		}
	    }
	    if (!found_entry) {
		std::unordered_map<unsigned long long, std::string>::iterator en = ctx->invalid_entry_map.find(*cs_it);
		if (en != ctx->invalid_entry_map.end()) {
		    bu_log("	%s[I] (%llu)\n", en->second.c_str(), *cs_it);
		    found_entry = true;
		} else {
		    bu_log("P ERROR: %llu:\n", pc_it->first);
		}
	    }
	}
    }
    bu_log("\n");
}

unsigned long long
path_hash(std::vector<unsigned long long> &path, size_t max_len)
{
    size_t mlen = (max_len) ? max_len : path.size();
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, path.data(), mlen * sizeof(unsigned long long));
    return (unsigned long long)XXH64_digest(&h_state);
}



static void
draw_walk_tree(union tree *tp, void *d, int subtract_skip,
	void (*leaf_func)(void *, const char *, matp_t, int)
	)
{
    if (!tp)
	return;

    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_SUBTRACT:
	    if (subtract_skip)
		return;
	    /* fall through */
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    draw_walk_tree(tp->tr_b.tb_right, d, subtract_skip, leaf_func);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    draw_walk_tree(tp->tr_b.tb_left, d, subtract_skip, leaf_func);
	    break;
	case OP_DB_LEAF:
	    (*leaf_func)(d, tp->tr_l.tl_name, tp->tr_l.tl_mat, tp->tr_op);
	    break;
	default:
	    bu_log("unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("draw walk\n");
    }
}

struct lc_data {
    std::unordered_set<unsigned long long> chashes;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
};


static void
get_child_hashes_leaf(void *client_data, const char *name, matp_t UNUSED(c_m), int UNUSED(op))
{
    struct lc_data *d = (struct lc_data *)client_data;
    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    XXH64_state_t h_state;
    unsigned long long chash;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    chash = (unsigned long long)XXH64_digest(&h_state);
    i_count[chash] += 1;
    if (i_count[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, i_count[chash]);
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	bu_vls_free(&iname);
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	d->chashes.insert(ihash);
    } else {
	d->chashes.insert(chash);
    }
}

struct walk_data {
    struct draw_ctx *ctx;
    struct ged *gedp;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};


static void
populate_leaf(void *client_data, const char *name, matp_t UNUSED(c_m), int UNUSED(op))
{
    struct walk_data *d = (struct walk_data *)client_data;
    struct db_i *dbip = d->gedp->dbip;
    RT_CHECK_DBI(dbip);

    std::unordered_map<unsigned long long, unsigned long long> &i_count = d->i_count;
    XXH64_state_t h_state;
    unsigned long long chash;
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    chash = (unsigned long long)XXH64_digest(&h_state);
    i_count[chash] += 1;
    if (i_count[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, i_count[chash]);
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	d->ctx->i_map[ihash] = chash;
	d->ctx->i_str[ihash] = std::string(bu_vls_cstr(&iname));
	d->ctx->p_c[d->phash].insert(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->ctx->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->ctx->invalid_entry_map.erase(ihash);
	}
	bu_vls_free(&iname);
    } else {
	d->ctx->p_c[d->phash].insert(chash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->ctx->invalid_entry_map[chash] = std::string(name);
	} else {
	    // In case this was previously invalid, remove
	    d->ctx->invalid_entry_map.erase(chash);
	}
    }
}

static void
populate_maps(struct ged *gedp, struct draw_ctx *ctx, struct directory *dp, int reset)
{


    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;

    // First, get parent name hash
    XXH64_reset(&h_state, 0);
    bu_vls_sprintf(&hname, "%s", dp->d_namep);
    XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
    unsigned long long phash = (unsigned long long)XXH64_digest(&h_state);

    // Set up to go from hash back to name
    ctx->d_map[phash] = dp;

    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    pc_it = ctx->p_c.find(phash);
    if (pc_it == ctx->p_c.end() || reset) {
	if (reset)
	    pc_it->second.clear();
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	    return;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree)
	    return;

	std::unordered_map<unsigned long long, unsigned long long> i_count;
	struct walk_data d;
	d.ctx = ctx;
	d.gedp = gedp;
	d.phash = phash;
	draw_walk_tree(comb->tree, (void *)&d, 0, populate_leaf);
	rt_db_free_internal(&in);
    }

}

static bool
validate_entry(struct ged *gedp, struct draw_ctx *ctx, const char *name)
{
    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    unsigned long long hash = (unsigned long long)XXH64_digest(&h_state);

    // A supplied name can be a number of things.  First, check if it is an
    // instance.  If it is, the thing to check is whether the comb map
    // has a key that matches the i_map value.  (From a higher level .g
    // perspective, we must first validate parent combs before validating
    // children, to make sure p_c is current before we check instances.)
    if (ctx->i_map.find(hash) != ctx->i_map.end()) {
	unsigned long long mhash = ctx->i_map[hash];
	if (ctx->p_c.find(mhash) == ctx->p_c.end())
	    return false;
	return true;
    }

    // For the rest, we need to know the status in the .g file
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);

    // Name isn't an i_map entry.  See if it refers to an invalid map entry.
    // If so, we need to make sure it is still invalid.  NOTE:  We don't verify
    // here that this entry is used in at least one comb...  that operation -
    // clearing stale invalid entries - needs to be part of a higher level,
    // whole .g check.
    if (ctx->invalid_entry_map.find(hash) != ctx->invalid_entry_map.end()) {
	if (dp != RT_DIR_NULL)
	    return false;
	return true;
    }

    // If it's not an invalid entry, make sure d_map is current
    std::unordered_map<unsigned long long, struct directory *>::iterator d_it = ctx->d_map.find(hash);
    if (d_it == ctx->d_map.end() && !dp)
	return false;
    if (d_it != ctx->d_map.end() && dp != d_it->second)
	return false;

    // If we got this far and we don't have a dp, whatever we have isn't valid
    if (!dp)
	return false;

    // If we have a comb, we need to check that the children are current
    if (dp->d_flags & RT_DIR_COMB) {

	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
	pc_it = ctx->p_c.find(hash);

	// Have a child hash set - need to validate it against the on-disk version
	struct rt_db_internal in;
	if (rt_db_get_internal(&in, dp, gedp->dbip, NULL, &rt_uniresource) < 0)
	    return false;
	struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
	if (!comb->tree) {
	    if (pc_it == ctx->p_c.end() || !pc_it->second.size())
		return true;
	    return false;
	}

	// Have a tree - get the hashes
	struct lc_data ldata;
	draw_walk_tree(comb->tree, (void *)&ldata, 0, get_child_hashes_leaf);

	// Check that every entry in the current pc_it is also on disk
	std::unordered_set<unsigned long long>::iterator c_it;
	for (c_it = pc_it->second.begin(); c_it != pc_it->second.end(); c_it++) {
	    if (ldata.chashes.find(*c_it) == ldata.chashes.end()) {
		return false;
	    }
	    ldata.chashes.erase(*c_it);
	}
	if (ldata.chashes.size() > 0) {
	    // If we have entries in the on-disk set that aren't in the working
	    // set, we're also invalid
	    return false;
	}
    }

    // All checks passed
    return true;
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

#if 0
static void
name_deinstance(struct bu_vls *nstr, const char *i_name)
{
    if (!nstr || !i_name)
	return;

    for (size_t i = 0; i < strlen(i_name); i++) {
	if (i_name[i] == '@')
	    return;
	bu_vls_putc(nstr, i_name[i]);
    }
}
#endif


static size_t
path_elements(std::vector<std::string> &elements, const char *path)
{
    std::vector<std::string> substrs;
    fp_path_split(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	std::string cleared = name_deescape(substrs[i]);
	elements.push_back(cleared);
    }
    return elements.size();
}

// This is a full (and more expensive) check to ensure
// a path has no cycles anywhere in it.
bool
path_cyclic(std::vector<unsigned long long> &path)
{
    if (path.size() == 1)
	return false;
    int i = path.size() - 1;
    while (i > 0) {
	int j = i - 1;
	while (j >= 0) {
	    if (path[i] == path[j])
		return true;
	    j--;
	}
	i--;
    }
    return false;
}

// This version assumes the path entries other than the last
// one are OK, and checks only against that last entry.
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

struct dd_t {
    struct ged *gedp;
    struct draw_ctx *ctx;
    matp_t m;
    int subtract_skip;
    std::vector<unsigned long long> path_hashes;
    std::unordered_map<unsigned long long, unsigned long long> *i_count;
};

void
draw_gather_paths(void *d, const char *name, matp_t m, int UNUSED(op))
{
    struct dd_t *dd = (struct dd_t *)d;
    struct directory *dp = db_lookup(dd->gedp->dbip, name, LOOKUP_QUIET);
    if (!dp)
	return;

    XXH64_state_t h_state;
    XXH64_reset(&h_state, 0);
    XXH64_update(&h_state, name, strlen(name)*sizeof(char));
    unsigned long long chash = (unsigned long long)XXH64_digest(&h_state);
    (*dd->i_count)[chash] += 1;
    if ((*dd->i_count)[chash] > 1) {
	// If we've got multiple instances of the same object in the tree,
	// hash the string labeling the instance and map it to the correct
	// parent comb so we can associate it with the tree contents
	struct bu_vls iname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&iname, "%s@%llu", name, (*dd->i_count)[chash]);
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, bu_vls_cstr(&iname), bu_vls_strlen(&iname)*sizeof(char));
	unsigned long long ihash = (unsigned long long)XXH64_digest(&h_state);
	dd->path_hashes.push_back(ihash);
    } else {	
	dd->path_hashes.push_back(chash);
    }

    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    print_path(&path_str, dd->path_hashes, dd->ctx);
    bu_log("%s\n", bu_vls_cstr(&path_str));
    bu_vls_free(&path_str);

    mat_t om, nm;
    /* Update current matrix state to reflect the new branch of
     * the tree. Either we have a local matrix, or we have an
     * implicit IDN matrix. */
    MAT_COPY(om, dd->m);
    if (m) {
	MAT_COPY(nm, m);
    } else {
	MAT_IDN(nm);
    }
    bn_mat_mul(dd->m, om, nm);

#if 0
    // Stash current color settings and see if we're getting new ones
    struct bu_color oc;
    int inherit_old = dd->color_inherit;
    HSET(oc.buc_rgb, dd->c.buc_rgb[0], dd->c.buc_rgb[1], dd->c.buc_rgb[2], dd->c.buc_rgb[3]);
    if (!dd->bound_only) {
	tree_color(dp, dd);
    }
#endif

    if (dp->d_flags & RT_DIR_COMB) {
	// Two things may prevent further processing of a comb - a hidden dp, or
	// a cyclic path.
	if (!(dp->d_flags & RT_DIR_HIDDEN) && !path_addition_cyclic(dd->path_hashes)) {
	    /* Keep going */
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal(&in, dp, dd->gedp->dbip, NULL, &rt_uniresource) < 0)
		return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    std::unordered_map<unsigned long long, unsigned long long> i_count;
	    std::unordered_map<unsigned long long, unsigned long long> *tmp = dd->i_count;
	    dd->i_count = &i_count;
	    draw_walk_tree(comb->tree, d, dd->subtract_skip, draw_gather_paths);
	    dd->i_count = tmp;
	    rt_db_free_internal(&in);
	}
    } else {
	// Solid - scene object time
	bu_log("make solid\n");
	unsigned long long phash = path_hash(dd->path_hashes, 0);
	dd->ctx->s_keys[phash] = dd->path_hashes;
    }

    /* Done with branch - restore path, put back the old matrix state,
     * and restore previous color settings */
    dd->path_hashes.pop_back();
    MAT_COPY(dd->m, om);
#if 0
    if (!dd->bound_only) {
	dd->color_inherit = inherit_old;
	HSET(dd->c.buc_rgb, oc.buc_rgb[0], oc.buc_rgb[1], oc.buc_rgb[2], oc.buc_rgb[3]);
    }
#endif
}

static void
draw(struct ged *gedp, struct draw_ctx *ctx, const char *path)
{
    mat_t m;
    MAT_IDN(m);
    int op = OP_UNION;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    std::vector<std::string> pe;
    path_elements(pe, path);
    // TODO: path color
    // TODO: path matrix
    // TODO: op check (specific instances specified may contain subtractions...)
    struct dd_t d;
    d.gedp = gedp;
    d.ctx = ctx;
    d.m = m;
    d.subtract_skip = 0;
    d.i_count = &i_count;
    path_elements(pe, path);
    draw_gather_paths((void *)&d, pe[pe.size()-1].c_str(), m, op);
}


static void
erase(struct draw_ctx *ctx, const char *path)
{
    std::vector<std::string> pe;
    path_elements(pe, path);
    std::vector<unsigned long long> root;
    for (size_t i = 0; i < pe.size(); i++) {
	XXH64_state_t h_state;
	unsigned long long chash;
	XXH64_reset(&h_state, 0);
	XXH64_update(&h_state, pe[i].c_str(), pe[i].length()*sizeof(char));
	chash = (unsigned long long)XXH64_digest(&h_state);
	root.push_back(chash);
    }

    std::vector<unsigned long long> bad_paths;
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = ctx->s_keys.begin(); k_it != ctx->s_keys.end(); k_it++) {
	if (k_it->second.size() < root.size())
	    continue;
	bool match = std::equal(root.begin(), root.end(), k_it->second.begin());
	if (match) {
	    bad_paths.push_back(k_it->first);
	}
    }
    bu_log("Starting size: %zd\n", ctx->s_keys.size());
    bu_log("To erase: %zd\n", bad_paths.size());
    for (size_t i = 0; i < bad_paths.size(); i++) {
	ctx->s_keys.erase(bad_paths[i]);
    }
    bu_log("Ending size: %zd\n", ctx->s_keys.size());
}

static void
collapse(struct draw_ctx *ctx)
{
    std::unordered_set<unsigned long long>::iterator s_it;
    std::map<size_t, std::unordered_set<unsigned long long>> depth_groups;

    std::vector<std::vector<unsigned long long>> drawn_paths;
    ctx->drawn_paths.clear();

    // Group paths of the same depth.  Depth == 1 paths are already
    // top level objects and need no further processing
    std::unordered_map<unsigned long long, std::vector<unsigned long long>>::iterator k_it;
    for (k_it = ctx->s_keys.begin(); k_it != ctx->s_keys.end(); k_it++) {
	if (k_it->second.size() == 1) {
	    drawn_paths.push_back(k_it->second);
	    ctx->drawn_paths.insert(k_it->first);
	} else {
	    depth_groups[k_it->second.size()].insert(k_it->first);
	}
    }
    bu_log("depth groups: %zd\n", depth_groups.size());


    // Whittle down the depth groups until we find not-fully-drawn parents - when
    // we find that, the children constitute non-collapsible paths
    while (depth_groups.size()) {
	size_t plen = depth_groups.rbegin()->first;
	if (plen == 1)
	    break;
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;
	bu_log("depth_groups(%zd) key count: %zd\n", plen, pckeys.size());

	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>> grouped_pckeys;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> &pc_path = ctx->s_keys[*s_it];
	    grouped_pckeys[pc_path[plen-2]].insert(*s_it);
	}

	// For each parent/child grouping, compare it against the .g ground
	// truth set.  If they match, fully drawn and we promote the path to
	// the parent depth.  If not, the paths do not collapse further and are
	// added to drawn paths.
	std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pg_it;
	for (pg_it = grouped_pckeys.begin(); pg_it != grouped_pckeys.end(); pg_it++) {

	    // Because we store keys to full paths, we construct the set of the children
	    // needed at this specific depth from the full paths for comparison purposes
	    std::unordered_set<unsigned long long> g_children;
	    std::unordered_set<unsigned long long> &g_pckeys = pg_it->second;
	    for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		std::vector<unsigned long long> &pc_path = ctx->s_keys[*s_it];
		g_children.insert(pc_path[plen-1]);
	    }

	    // Do the check against the .g comb children info
	    bool fully_drawn = true;
	    std::unordered_set<unsigned long long> &ground_truth = ctx->p_c[pg_it->first];
	    for (s_it = ground_truth.begin(); s_it != ground_truth.end(); s_it++) {
		if (g_children.find(*s_it) == g_children.end()) {
		    fully_drawn = false;
		    break;
		}
	    }

	    if (fully_drawn) {
		// If fully drawn, depth_groups[plen-1] gets the first path in g_pckeys.  The path is longer
		// than that depth, but contains all the necessary information and using that approach avoids
		// the need to duplicate paths.
		depth_groups[plen - 1].insert(*g_pckeys.begin());
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> &pc_path = ctx->s_keys[*s_it];
		    ctx->drawn_paths.insert(path_hash(pc_path, plen));
		}
	    } else {
		// No further collapsing - add to final.  We must make trimmed
		// versions of the paths in case this depth holds promoted paths
		// from deeper levels.
		for (s_it = g_pckeys.begin(); s_it != g_pckeys.end(); s_it++) {
		    std::vector<unsigned long long> trimmed = ctx->s_keys[*s_it];
		    trimmed.resize(plen);
		    drawn_paths.push_back(trimmed);
		    ctx->drawn_paths.insert(path_hash(trimmed, 0));
		}
	    }
	}

	// Done with this depth
	depth_groups.erase(plen);
    }

    // If we collapsed all the way to top level objects, make sure to add them
    if (depth_groups.find(1) != depth_groups.end()) {
	std::unordered_set<unsigned long long> &pckeys = depth_groups.rbegin()->second;
	for (s_it = pckeys.begin(); s_it != pckeys.end(); s_it++) {
	    std::vector<unsigned long long> trimmed = ctx->s_keys[*s_it];
	    trimmed.resize(1);
	    drawn_paths.push_back(trimmed);
	}
    }

    // DEBUG - print results
    struct bu_vls path_str = BU_VLS_INIT_ZERO;
    for (size_t i = 0; i < drawn_paths.size(); i++) {
	print_path(&path_str, drawn_paths[i], ctx);
	bu_log("%s\n", bu_vls_cstr(&path_str));
    }
    bu_vls_free(&path_str);
}

static void
split_test(const char *path)
{
    std::vector<std::string> substrs;
    path_elements(substrs, path);
    for (size_t i = 0; i < substrs.size(); i++) {
	bu_log("%s\n", substrs[i].c_str());
    }
    bu_log("\n");
}


static bool
check_elements(struct ged *gedp, struct draw_ctx *ctx, std::vector<std::string> &elements)
{
    if (!gedp || !elements.size())
	return false;

    // If we have only a single entry, make sure it's a valid db entry and return
    if (elements.size() == 1) {
	struct directory *dp = db_lookup(gedp->dbip, elements[0].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    return false;
	populate_maps(gedp, ctx, dp, 1);
	return true;
    }

    // Deeper paths take more work.  NOTE - if the last element isn't a valid
    // database entry, we have an invalid comb entry specified.  We are more
    // tolerant of such paths and let them be "drawn" to allow views to add
    // wireframes to scenes when objects are renamed to make an existing
    // invalid instance "good".  However, any comb entries above the final
    // entry *must* be valid objects in the database.
    for (size_t i = 0; i < elements.size(); i++) {
	struct directory *dp = db_lookup(gedp->dbip, elements[i].c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL && i < elements.size() - 1) {
	    bu_log("invalid path: %s\n", elements[i].c_str());
	    return false;
	}
	populate_maps(gedp, ctx, dp, 1);
    }


    // Check that elements were written correctly
    for (size_t i = 0; i < elements.size(); i++) {
	bool status = validate_entry(gedp, ctx, elements[i].c_str());
	bu_log("%s: %s\n", elements[i].c_str(), (status) ? "true" : "false");
    }

    // parent/child relationship validate
    struct bu_vls hname = BU_VLS_INIT_ZERO;
    XXH64_state_t h_state;
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    unsigned long long phash = 0;
    unsigned long long chash = 0;
    XXH64_reset(&h_state, 0);
    bu_vls_sprintf(&hname, "%s", elements[0].c_str());
    bu_log("parent: %s\n", elements[0].c_str());
    XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
    phash = (unsigned long long)XXH64_digest(&h_state);
    for (size_t i = 1; i < elements.size(); i++) {
	pc_it = ctx->p_c.find(phash);
	// The parent comb structure is stored only under its original name's hash - if
	// we have a numbered instance from a comb tree as a parent, we may be able to
	// map it to the correct entry with i_map.  If not, we have an invalid path.
	if (pc_it == ctx->p_c.end()) {
	    std::unordered_map<unsigned long long, unsigned long long>::iterator i_it = ctx->i_map.find(phash);
	    if (i_it == ctx->i_map.end()) {
		return false;
	    }
	    phash = i_it->second;
	}
	XXH64_reset(&h_state, 0);
	bu_vls_sprintf(&hname, "%s", elements[i].c_str());
	XXH64_update(&h_state, bu_vls_cstr(&hname), bu_vls_strlen(&hname)*sizeof(char));
	chash = (unsigned long long)XXH64_digest(&h_state);
	bu_log("child: %s\n\n", elements[i].c_str());

	if (pc_it->second.find(chash) == pc_it->second.end()) {
	    bu_log("Invalid element path: %s\n", elements[i].c_str());
	    return false;
	}
	phash = chash;
	bu_log("parent: %s\n", elements[i].c_str());
    }

    return true;
}

int
main(int ac, char *av[]) {
    struct ged *gedp;

    bu_setprogname(av[0]);

    if (ac != 2) {
	printf("Usage: %s file.g\n", av[0]);
	return 1;
    }
    if (!bu_file_exists(av[1], NULL)) {
	printf("ERROR: [%s] does not exist, expecting .g file\n", av[1]);
	return 2;
    }

    bu_setenv("GED_TEST_NEW_CMD_FORMS", "1", 1);

    gedp = ged_open("db", av[1], 1);

    struct draw_ctx ctx;
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_flags & DB_LS_HIDDEN)
		continue;
	    populate_maps(gedp, &ctx, dp, 0);
	}
    }

    print_ctx(&ctx);

    split_test("all.g/cone.r/cone.s");
    split_test("all.g/cone.r/cone.s/");
    split_test("all.g/cone.r/cone.s//");
    split_test("all.g/cone.r/cone.s\\//");
    split_test("all.g\\/cone.r\\//cone.s");
    split_test("all.g\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\\\\\\\/cone.r\\//cone.s");
    split_test("all.g\\cone.r\\//cone.s");
    split_test("all.g\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\cone.r\\//cone.s");
    split_test("all.g\\\\\\\\cone.r\\//cone.s");

    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "all.g/cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r/cone.s");
	check_elements(gedp, &ctx, pe);
    }
    {
	std::vector<std::string> pe;
	path_elements(pe, "cone2.r\\//cone.s");
	check_elements(gedp, &ctx, pe);
    }

    draw(gedp, &ctx, "all.g");

    collapse(&ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());

    erase(&ctx, "all.g/havoc/havoc_middle");

    collapse(&ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());

    erase(&ctx, "all.g");

    collapse(&ctx);
    bu_log("drawn path cnt: %zd\n", ctx.drawn_paths.size());

    ged_close(gedp);

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
