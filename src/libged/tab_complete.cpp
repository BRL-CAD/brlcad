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

#include "bu/time.h"
#include "bu/path.h"
#include "bu/vls.h"
#include "ged.h"

static int
cmd_match(struct bu_vls *s, const char *seed, const char *prev)
{
    const char * const *cl = NULL;
    size_t cmd_cnt = ged_cmd_list(&cl);

    if (!prev || !strlen(prev)) {

	// No context - start at the beginning
	if (!seed) {
	    bu_vls_sprintf(s, "%s\n", cl[0]);
	    return BRLCAD_OK;
	}

	// We have a seed - find the first match and return it
	for (size_t i = 0; i < cmd_cnt; i++) {
	    if (strlen(cl[i]) < strlen(seed))
		continue;
	    if (!strncmp(seed, cl[i], strlen(seed))) {
		bu_vls_sprintf(s, "%s\n", cl[i]);
		return BRLCAD_OK;
	    }
	}

	// If nothing matched the seed, we can't complete - the input is
	// invalid
	return BRLCAD_ERROR;

    }

    int found_prev = 0;
    for (size_t i = 0; i < cmd_cnt; i++) {
	if (found_prev) {
	    if (seed) {
		if (strlen(cl[i]) < strlen(seed))
		    continue;
		if (!strncmp(seed, cl[i], strlen(seed))) {
		    bu_vls_sprintf(s, "%s\n", cl[i]);
		    return BRLCAD_OK;
		}
	    } else {
		// No seed, so the next entry after prev
		// is the return
		bu_vls_sprintf(s, "%s\n", cl[i]);
		return BRLCAD_OK;
	    }
	}
	if (BU_STR_EQUAL(prev, cl[i]))
	    found_prev = 1;
    }

    // Got through the list - do we need to loop around to the beginning?
    if (found_prev) {
	if (seed) {
	    if (strlen(cl[0]) < strlen(seed))
		return BRLCAD_ERROR;
	    if (!strncmp(seed, cl[0], strlen(seed))) {
		bu_vls_sprintf(s, "%s\n", cl[0]);
		return BRLCAD_OK;
	    }
	} else {
	    bu_vls_sprintf(s, "%s\n", cl[0]);
	    return BRLCAD_OK;
	}
    }

    return BRLCAD_ERROR;
}

static int
path_match(struct bu_vls *s, struct db_i *dbip, const char *seedp, const char *prevp)
{
    // If we've gotten this far, we either have a hierarchy or an error
    std::string lstr(seedp);
    if (lstr.find_first_of("/", 0) == std::string::npos)
	return BRLCAD_ERROR;

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
	return BRLCAD_ERROR;

    // If the last string from lstr is a fully qualified object name, then we
    // are looking to do completions in its comb tree (if a comb) or there is
    // no completion to make (if not a comb).  If the last string is a partial,
    // then we're looking in the parent comb's tree for something that matches
    // the partial
    std::string seed;
    std::string context;
    struct directory *dp = db_lookup(dbip, objs[objs.size()-1].c_str(), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	seed = objs[objs.size() - 1];
	context = objs[objs.size() - 2];
    } else {
	context = objs[objs.size() - 1];
    }
    struct directory *cdp = db_lookup(dbip, context.c_str(), LOOKUP_QUIET);
    if (cdp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB))
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

    // If we don't have a seed or a prev entry, grab the first entry
    if (!seed.length() && !prevp) {
	bu_vls_printf(s, "%s", children[0]->d_namep);
	bu_free(children, "dp array");
	return BRLCAD_OK;
    }

    // Seed but no previous entry - grab the first match
    if (!prevp) {
	for (int i = 0; i < child_cnt; i++) {
	    if (strlen(children[i]->d_namep) < seed.length())
		continue;
	    if (!strncmp(seed.c_str(), children[i]->d_namep, seed.length())) {
		bu_vls_sprintf(s, "%s\n", children[i]->d_namep);
		bu_free(children, "dp array");
		return BRLCAD_OK;
	    }
	}
	// No matches - error
	bu_free(children, "dp array");
	return BRLCAD_ERROR;
    }

    // We have a prev - split it to get the last object
    std::vector<std::string> pobjs;
    std::string plstr(prevp);
    while ((pos = plstr.find_first_of("/", 0)) != std::string::npos) {
	std::string ss = plstr.substr(0, pos);
	objs.push_back(ss);
	plstr.erase(0, pos + 1);
    }
    pobjs.push_back(lstr);
    if (pobjs.size() < 2) {
	bu_free(children, "dp array");
	return BRLCAD_ERROR;
    }

    int found_prev = 0;
    for (int i = 0; i < child_cnt; i++) {
	if (found_prev) {
	    if (!seed.length()) {
		bu_vls_sprintf(s, "%s\n", children[i]->d_namep);
		bu_free(children, "dp array");
		return BRLCAD_OK;
	    } else {
		if (strlen(children[i]->d_namep) < seed.length())
		    continue;
		if (!strncmp(seed.c_str(), children[i]->d_namep, seed.length())) {
		    bu_vls_sprintf(s, "%s\n", children[i]->d_namep);
		    bu_free(children, "dp array");
		    return BRLCAD_OK;
		}
	    }
	    if (BU_STR_EQUAL(pobjs[pobjs.size()-1].c_str(), children[i]->d_namep)) {
		found_prev = 1;
		// If we need to, wrap around to the beginning
		if (i == child_cnt - 1)
		    i = 0;
	    }
	}
    }

    bu_free(children, "dp array");
    return BRLCAD_ERROR;
}

// Because librt doesn't forbid objects with forward slashes in
// their names, and such names have occasionally been observed in
// the wild, we have to treat all seed strings as potential dp names
// first, and only after that fails try them as hierarchy paths.
static int
obj_match(struct bu_vls *s, struct db_i *dbip, const char *seed, const char *prev)
{
    int ret = BRLCAD_OK;
    int found_prev = 0;

    // We may need the tops list, so set it up
    db_update_nref(dbip, &rt_uniresource);
    struct directory **all_paths;
    int tops_cnt = db_ls(dbip, DB_LS_TOPS, NULL, &all_paths);

    // Make a single iterative vector of all active directory pointers
    std::vector<struct directory *> dps;
    for (int i = 0; i < RT_DBNHASH; i++) {
	struct directory *dp;
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    dps.push_back(dp);
	}
    }

    if (!prev || !strlen(prev)) {

	// No context, seed or prev - start at the beginning
	if (!seed) {
	    bu_vls_sprintf(s, "%s\n", all_paths[0]->d_namep);
	    goto obj_cleanup;
	}

	// We have a seed - find the first match and return it
	for (int i = 0; i < tops_cnt; i++) {
	    if (strlen(all_paths[i]->d_namep) < strlen(seed))
		continue;
	    if (!strncmp(seed, all_paths[i]->d_namep, strlen(seed))) {
		bu_vls_sprintf(s, "%s\n", all_paths[i]->d_namep);
		goto obj_cleanup;
	    }
	}
	// Nothing in the tops list - try all objects
	for (size_t i = 0; i < dps.size(); i++) {
	    struct directory *dp = dps[i];
	    if (strlen(dp->d_namep) < strlen(seed))
		continue;
	    if (!strncmp(seed, dp->d_namep, strlen(seed))) {
		bu_vls_sprintf(s, "%s\n", dp->d_namep);
		goto obj_cleanup;
	    }
	}

	// Individual objects didn't work - see if we might have a partial
	// path


	// If nothing matched the seed, we can't complete - the input is
	// invalid
	ret = BRLCAD_ERROR;
	goto obj_cleanup;
    }

    // We do have a previous entry to iterate on from - find the next entry
    for (int i = 0; i < tops_cnt; i++) {
	if (found_prev) {
	    if (!seed) {
		bu_vls_sprintf(s, "%s\n", all_paths[i]->d_namep);
		goto obj_cleanup;
	    } else {
		if (strlen(all_paths[i]->d_namep) < strlen(seed))
		    continue;
		if (!strncmp(seed, all_paths[i]->d_namep, strlen(seed))) {
		    bu_vls_sprintf(s, "%s\n", all_paths[i]->d_namep);
		    goto obj_cleanup;
		}
	    }
	    if (BU_STR_EQUAL(prev, all_paths[i]->d_namep)) {
		found_prev = 1;
		// If we need to, wrap around to the beginning
		if (i == tops_cnt - 1)
		    i = 0;
	    }
	}
    }

    // Nothing in the tops list - try all objects
    for (size_t i = 0; i < dps.size(); i++) {
	struct directory *dp = dps[i];
	if (found_prev) {
	    if (!seed) {
		bu_vls_sprintf(s, "%s\n", dp->d_namep);
		goto obj_cleanup;
	    } else {
		if (strlen(dp->d_namep) < strlen(seed))
		    continue;
		if (!strncmp(seed, dp->d_namep, strlen(seed))) {
		    bu_vls_sprintf(s, "%s\n", dp->d_namep);
		    goto obj_cleanup;
		}
	    }
	    if (BU_STR_EQUAL(prev, dp->d_namep)) {
		found_prev = 1;
		// If we need to, wrap around to the beginning
		if (i == dps.size() - 1)
		    i = 0;
	    }
	}
    }


    // Individual objects didn't work - see if we might have a partial
    // path


    // If nothing matched the seed, we can't complete - the input is
    // invalid
    ret = BRLCAD_ERROR;
    goto obj_cleanup;


    return BRLCAD_ERROR;
obj_cleanup:
    bu_free(all_paths, "free db_ls output");
    return ret;
}



int
ged_cmd_suggest(struct bu_vls *s, struct ged *gedp, const char *iseed, struct bu_vls *iprev, int UNUSED(list_all))
{
    int ret = BRLCAD_OK;

    if (!gedp || !s)
	return BRLCAD_ERROR;

    struct bu_vls suggest = BU_VLS_INIT_ZERO;

    // If we have a seed, break it down into an argc/argv array, so we can
    // examine the components
    int ac = 0;
    char **av = NULL;
    char *seed = (iseed) ? bu_strdup(iseed) : NULL;
    if (seed) {
	av = (char **)bu_calloc(strlen(seed) + 1, sizeof(char *), "argv array");
	ac = bu_argv_from_string(av, strlen(seed), seed);
    }

    // If there's a prev input, break it down as well
    int pac = 0;
    char **pav = NULL;
    char *prev = (iprev && bu_vls_strlen(iprev)) ? bu_strdup(bu_vls_cstr(iprev)) : NULL;
    if (prev) {
	pav = (char **)bu_calloc(strlen(prev) + 1, sizeof(char *), "argv array");
	pac = bu_argv_from_string(av, strlen(prev), prev);
    }

    // If we have a fully defined command, go with it.  Otherwise, we need to
    // see about a completion
    int cmd_valid = (av && av[0]) ? !ged_cmd_valid(av[0], NULL) : 0;
    if (cmd_valid) {
	bu_vls_printf(&suggest, "%s ", av[0]);
    } else {
	ret = cmd_match(&suggest, (av) ? av[0] : NULL, (pav) ? pav[0] : NULL);
    }

    if (!cmd_valid) {
	// If we didn't come in with a valid command, that's what the
	// completion has to be focused on - we're done.
	goto suggest_cleanup;
    }

    // Valid command.  TODO - see if it implements a specialized completion
    // specifier


    // No specialized completion, so we're being asked to do a db lookup
    // completion on the last element, if we have it.  First try a straight
    // up object name match
    ret = obj_match(&suggest, gedp->dbip, (av && ac > 1) ? av[ac-1] : NULL, (pav && pac > 1) ? pav[pac - 1] : NULL);

    // If the object match didn't work, see if we have a hierarchy specification
    if (ret != BRLCAD_OK) {
	ret = path_match(&suggest, gedp->dbip, (av && ac > 1) ? av[ac-1] : NULL, (pav && pac > 1) ? pav[pac - 1] : NULL);
    }
suggest_cleanup:
    if (seed)
	bu_free(seed, "seed cpy");
    if (prev)
	bu_free(prev, "prev cpy");
    if (av)
	bu_free(av, "input argv");
    if (pav)
	bu_free(pav, "input pargv");

    if (ret == BRLCAD_OK)
	bu_vls_sprintf(s, "%s", bu_vls_cstr(&suggest));
    bu_vls_free(&suggest);

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
