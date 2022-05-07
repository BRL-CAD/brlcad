/*                  T A B _ C O M P L E T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file tab_complete.cpp
 *
 * Facilities for constructing automatic completions of partial command and/or
 * object name inputs supplied by applications.
 */

#include "common.h"

#include <map>
#include <string>

#define ALPHANUM_IMPL
#include "./alphanum.h"

#include "bu/path.h"
#include "bu/sort.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bu/str.h"
#include "ged.h"

static int
alphanum_cmp(const void *a, const void *b, void *UNUSED(data)) {
    struct directory *ga = *(struct directory **)a;
    struct directory *gb = *(struct directory **)b;
    return alphanum_impl(ga->d_namep, gb->d_namep, NULL);
}

static int
path_match(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *iseed)
{
    // If we've gotten this far, we either have a hierarchy or an error
    std::string lstr(iseed);
    if (lstr.find_first_of("/", 0) == std::string::npos)
	return 0;

    // Split the seed into substrings.  We can't use the db fullpath
    // routines for this, because the final obj name is likely incomplete
    // and therefore invalid.
    std::vector<std::string> objs;
    size_t pos = 0;
    while ((pos = lstr.find_first_of("/", 0)) != std::string::npos) {
	std::string ss = lstr.substr(0, pos);
	objs.push_back(ss);
	lstr.erase(0, pos + 1);
    }
    objs.push_back(lstr);
    if (objs.size() < 2)
	return 0;

    // If the last char in lstr is a slash, then we are looking to do
    // completions in the comb tree of the parent (if a comb or if we are
    // starting with a slash - i.e. the implicit tops comb) or there is no
    // completion to make (if not a comb).  If the last string is a partial,
    // then we're looking in the parent comb's tree for something that matches
    // the partial.
    std::string seed;
    std::string context;
    struct directory *dp = db_lookup(dbip, objs[objs.size()-1].c_str(), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL && lstr[lstr.length() - 1] != '/') {
	seed = objs[objs.size() - 1];
	context = objs[objs.size() - 2];
    } else {
	if (lstr[lstr.length() - 1] != '/') {
	    seed = objs[objs.size() - 1];
	    context = objs[objs.size() - 2];
	} else {
	    context = objs[objs.size() - 1];
	}
    }

    if (!context.length()) {
	if (!seed.length()) {
	    bu_vls_trunc(prefix, 0);
	} else {
	    bu_vls_sprintf(prefix, "%s", seed.c_str());
	}
	// Empty context - we need the tops list
	db_update_nref(dbip, &rt_uniresource);
	struct directory **all_paths;
	int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &all_paths);
	bu_sort(all_paths, tops_cnt, sizeof(struct directory *), alphanum_cmp, NULL);
	*completions = (const char **)bu_calloc(tops_cnt + 1, sizeof(const char *), "av array");
	for (int i = 0; i < tops_cnt; i++) {
	    (*completions)[i] = bu_strdup(all_paths[i]->d_namep);
	}
	bu_free(all_paths, "free db_ls output");
	return tops_cnt;
    }

    struct directory *cdp = db_lookup(dbip, context.c_str(), LOOKUP_QUIET);
    if (cdp == RT_DIR_NULL || !(cdp->d_flags & RT_DIR_COMB))
	return BRLCAD_ERROR;

    struct rt_db_internal in;
    if (rt_db_get_internal(&in, cdp, dbip, NULL, &rt_uniresource) < 0)
	return BRLCAD_ERROR;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)in.idb_ptr;
    if (!comb) {
	rt_db_free_internal(&in);
	return BRLCAD_ERROR;
    }
    struct directory **children = NULL;
    int child_cnt = db_comb_children(dbip, comb, &children, NULL, NULL);
    rt_db_free_internal(&in);

    // If we don't have a seed or a prev entry, grab all the children
    if (!seed.length()) {
	bu_vls_trunc(prefix, 0);
	*completions = (const char **)bu_calloc(child_cnt + 1, sizeof(const char *), "av array");
	for (int i = 0; i < child_cnt; i++)
	    (*completions)[i] = bu_strdup(children[i]->d_namep);
	bu_free(children, "dp array");
	return child_cnt;
    }

    // Have seed - grab the matches
    bu_vls_sprintf(prefix, "%s", seed.c_str());
    std::vector<struct directory *> matches;
    for (int i = 0; i < child_cnt; i++) {
	if (strlen(children[i]->d_namep) < seed.length())
	    continue;
	if (!bu_strncmp(seed.c_str(), children[i]->d_namep, seed.length()))
	    matches.push_back(children[i]);
    }
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]->d_namep);
    bu_free(children, "dp array");
    return (int)matches.size();
}

// Because librt doesn't forbid objects with forward slashes in
// their names, and such names have occasionally been observed in
// the wild, we have to treat all seed strings as potential dp names
// first, and only after that fails try them as hierarchy paths.
static int
obj_match(const char ***completions, struct db_i *dbip, const char *seed)
{
    // Prepare the dp list in the order we want - first tops entries, then
    // all objects.
    std::vector<struct directory *> dps;

    // all active directory pointers
    struct bu_ptbl fdps = BU_PTBL_INIT_ZERO;
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    bu_ptbl_ins(&fdps, (long *)dp);
	}
    }
    bu_sort(BU_PTBL_BASEADDR(&fdps), BU_PTBL_LEN(&fdps), sizeof(struct directory *), alphanum_cmp, NULL);
    for (size_t i = 0; i < BU_PTBL_LEN(&fdps); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(&fdps, i);
	dps.push_back(dp);
    }
    bu_ptbl_free(&fdps);

    // Have the possibilities organized - find seed matches
    std::vector<struct directory *> matches;
    for (size_t i = 0; i < dps.size(); i++) {
	if (strlen(dps[i]->d_namep) < strlen(seed))
	    continue;
	if (!bu_strncmp(seed, dps[i]->d_namep, strlen(seed))) {
	    matches.push_back(dps[i]);
	}
    }

    // Make an argv array for client use
    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]->d_namep);
    return (int)matches.size();
}

int
ged_cmd_completions(const char ***completions, const char *seed)
{
    int ret = 0;

    if (!completions || !seed)
	return 0;

    //Build a set of matches
    const char * const *cl = NULL;
    size_t cmd_cnt = ged_cmd_list(&cl);

    std::vector<const char *> matches;
    for (size_t i = 0; i < cmd_cnt; i++) {
	if (strlen(cl[i]) < strlen(seed))
	    continue;
	if (!bu_strncmp(seed, cl[i], strlen(seed)))
	    matches.push_back(cl[i]);
    }

    *completions = (const char **)bu_calloc(matches.size() + 1, sizeof(const char *), "av array");
    for (size_t i = 0; i < matches.size(); i++)
	(*completions)[i] = bu_strdup(matches[i]);
    ret = (int)matches.size();

    return ret;
}

int
ged_geom_completions(const char ***completions, struct bu_vls *prefix, struct db_i *dbip, const char *seed)
{
    int ret = 0;

    if (!dbip || !prefix || !completions || !seed)
	return 0;

    ret = obj_match(completions, dbip, seed);

    // If the object name match didn't work, see if we have a hierarchy specification
    if (!ret) {
	ret = path_match(completions, prefix, dbip, seed);
    } else {
	// If the match is from object names, the prefix is just the seed
	bu_vls_sprintf(prefix, "%s", seed);
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
