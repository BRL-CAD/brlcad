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

HIDDEN void
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

struct walk_data {
    struct draw_ctx *ctx;
    struct ged *gedp;
    std::unordered_map<unsigned long long, unsigned long long> i_count;
    mat_t *curr_mat = NULL;
    unsigned long long phash = 0;
};

static void
list_children_leaf(void *client_data, const char *name, matp_t UNUSED(c_m), int UNUSED(op))
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
	d->ctx->p_c[d->phash].insert(ihash);
	if (dp == RT_DIR_NULL) {
	    // Invalid comb reference - goes into map
	    d->ctx->invalid_entry_map[ihash] = std::string(bu_vls_cstr(&iname));
	} else {
	    // In case this was previously invalid, remove
	    d->ctx->invalid_entry_map.erase(ihash);
	}
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
	draw_walk_tree(comb->tree, (void *)&d, 0, list_children_leaf);
	rt_db_free_internal(&in);
    }

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

#if 0
static void
draw_gather_paths(std::vector<unsigned long long> *path, struct directory *dp, mat_t *curr_mat, void *client_data)
{
    struct walk_data *d = (struct walk_data *)client_data;
}

static void
draw(struct ged *gedp, struct draw_ctx *ctx, const char *path)
{
    std::vector<std::string> pe;
    path_elements(pe, path);
    // TODO: path color
    // TODO: path matrix

}
#endif


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
#if 1
    std::unordered_map<unsigned long long, std::unordered_set<unsigned long long>>::iterator pc_it;
    std::unordered_set<unsigned long long>::iterator cs_it;
    for (pc_it = ctx.p_c.begin(); pc_it != ctx.p_c.end(); pc_it++) {
	bool found_entry = false;
	std::unordered_map<unsigned long long, struct directory *>::iterator dpn = ctx.d_map.find(pc_it->first);
	if (dpn != ctx.d_map.end()) {
	    bu_log("%s	(%llu):\n", dpn->second->d_namep, pc_it->first);
	    found_entry = true;
	}
	if (!found_entry) {
	    unsigned long long chash = ctx.i_map[pc_it->first];
	    dpn = ctx.d_map.find(chash);
	    if (dpn != ctx.d_map.end()) {
		bu_log("%s	(%llu->%llu):\n", dpn->second->d_namep, pc_it->first, chash);
		found_entry = true;
	    }
	}
	if (!found_entry) {
	    std::unordered_map<unsigned long long, std::string>::iterator en = ctx.invalid_entry_map.find(pc_it->first);
	    if (en != ctx.invalid_entry_map.end()) {
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
	    dpn = ctx.d_map.find(*cs_it);
	    if (dpn != ctx.d_map.end()) {
		bu_log("	%s	(%llu)\n", dpn->second->d_namep, *cs_it);
		found_entry = true;
	    }
	    if (!found_entry) {
		unsigned long long chash = ctx.i_map[*cs_it];
		dpn = ctx.d_map.find(chash);
		if (dpn != ctx.d_map.end()) {
		    bu_log("	%s	(%llu->%llu)\n", dpn->second->d_namep, *cs_it, chash);
		    found_entry = true;
		}
	    }
	    if (!found_entry) {
		std::unordered_map<unsigned long long, std::string>::iterator en = ctx.invalid_entry_map.find(*cs_it);
		if (en != ctx.invalid_entry_map.end()) {
		    bu_log("	%s[I] (%llu)\n", en->second.c_str(), *cs_it);
		    found_entry = true;
		} else {
		    bu_log("P ERROR: %llu:\n", pc_it->first);
		}
	    }
	}
    }
    bu_log("\n");
#endif

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
