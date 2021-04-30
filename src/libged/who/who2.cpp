/*                         W H O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/who.cpp
 *
 * The who command.
 *
 */

#include "common.h"

#include <set>

#include "bu/malloc.h"
#include "bu/str.h"
#include "ged.h"

// Need to process deepest to shallowest, so we don't get bit by starting at a
// high level path - we need to check the deepest parent, and build up from the
// bottom.  Below sorting criteria need to be defined to sort such that we
// return the deepest paths as the begin() of the set.
struct dfp_cmp {
    bool operator() (struct db_full_path *fp, struct db_full_path *o) const {
	// First, check length
	if (fp->fp_len != o->fp_len) {
	    return (fp->fp_len > o->fp_len);
	}

	// If length is the same, check the dp contents
	for (size_t i = 0; i < fp->fp_len; i++) {
	    if (!BU_STR_EQUAL(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep)) {
		return (bu_strcmp(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep) > 0);
	    }
	}
	return (strcmp(fp->fp_names[fp->fp_len-1]->d_namep, o->fp_names[fp->fp_len-1]->d_namep) > 0);
    }
};

/*
 * List the db objects currently drawn
 *
 * Usage:
 * who
 *
 * We need to figure out which parent combs (if any) are fully realized by
 * having all their child solid objects drawn, and return those shorter paths
 * rather than listing out all the solids - that's the most useful output for
 * the user, although it's a lot harder.
 */
extern "C" int
ged_who2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    std::set<struct db_full_path *, dfp_cmp> s1, s2;
    std::set<struct db_full_path *, dfp_cmp> *s, *snext;
    s = &s1;
    snext = &s2;

    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_scene_objs); i++) {
	struct bview_scene_obj *sobj = (struct bview_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_scene_objs, i);
	if (sobj->s_type_flags & BVIEW_DBOBJ_BASED) {
	    struct draw_update_data_t *ud = (struct draw_update_data_t *)sobj->s_i_data;
	    struct db_full_path *fp;
	    if (s->find(&ud->fp) == s->end()) {
		BU_GET(fp, struct db_full_path);
		db_full_path_init(fp);
		db_dup_full_path(fp, &ud->fp);
		s->insert(fp);
	    }
	}
    }

    std::set<std::string> drawn;
    std::set<struct db_full_path *, dfp_cmp>::iterator s_it;
    for (s_it = s->begin(); s_it != s->end(); s_it++) {
	struct db_full_path *fp = *s_it;
	char *sfp = db_path_to_string(fp);
	bu_log("%zd %s\n", (*s_it)->fp_len, sfp);
	if (!(DB_FULL_PATH_CUR_DIR(fp)->d_flags & RT_DIR_COMB)) {
	    // By definition, solids are fully drawn
	    drawn.insert(std::string(sfp));
	}
	bu_free(sfp, "path string");
    }

    std::set<std::string> finalized;
    while (s->size()) {
	struct db_full_path *fp = *s->begin();

	// We'll be checking the parent of fp, but we want
	// fp to be part of the comparison set.  Make a copy
	// to serve as the parent path.
	struct db_full_path *pp;
	BU_GET(pp, struct db_full_path);
	db_full_path_init(pp);
	db_dup_full_path(pp, fp);
	DB_FULL_PATH_POP(pp);

	char *ppstr = db_path_to_string(pp);
	bu_log("Checking children of %s\n", ppstr);
	bu_free(ppstr, "ppstr");

	// if pp_len == 0, fp is a top level object and its finalized -
	// add it to the final set.
	if (!pp->fp_len) {
	    s->erase(fp);
	    char *str = db_path_to_string(fp);
	    finalized.insert(std::string(str));
	    bu_free(str, "str");
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	} else {
	    // If we have a deeper path get the list of immediate children from pp:
	    struct directory **children = NULL;
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;
	    if (rt_db_get_internal(&in, DB_FULL_PATH_CUR_DIR(pp), gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
		char *str = db_path_to_string(fp);
		finalized.insert(std::string(str));
		bu_free(str, "str");
		db_free_full_path(pp);
		BU_PUT(pp, struct db_full_path);
	    }
	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    db_comb_children(gedp->ged_wdbp->dbip, comb, &children, NULL, NULL);

	    // For all children, see if the path is in s, snext or finalized. If it is
	    // present in any of those it is drawn.
	    int i = 0;
	    int not_found = 0;
	    struct directory *dp =  children[0];
	    while (dp != RT_DIR_NULL) {
		db_add_node_to_full_path(pp, dp);
		char *str = db_path_to_string(pp);
		if (drawn.find(std::string(str)) == drawn.end()) {
		    not_found = 1;
		    bu_log("%s not found\n", str);
		    bu_free(str, "str");
		    DB_FULL_PATH_POP(pp);
		    break;
		}
		bu_free(str, "str");
		DB_FULL_PATH_POP(pp);
		i++;
		dp = children[i];
	    }
	    i = 0;
	    dp =  children[0];

	    // If all child paths are present in s or snext, the parent is
	    // fully realized in the scene.  If all child paths are NOT present
	    // in s, those that are present are all finalized since their
	    // immediate parent is not fully realized in the scene and they
	    // themselves are the drawn object instances.
	    if (not_found) {
		// Not fully realized
		while (dp != RT_DIR_NULL) {
		    db_add_node_to_full_path(pp, dp);
		    char *str = db_path_to_string(pp);
		    if (drawn.find(std::string(str)) != drawn.end()) {
			// We have a path that is drawn, in a comb not fully
			// drawn.  Finalize it and remove it from processing
			bu_log("finalizing partial path: %s\n", str);
			finalized.insert(std::string(str));
			s->erase(pp);
			snext->erase(pp);
		    }
		    bu_free(str, "str");
		    DB_FULL_PATH_POP(pp);
		    i++;
		    dp = children[i];
		}
		// Children handled - we're done processing this branch of the tree
		s->erase(pp);
		snext->erase(pp);
	    } else {
		// Fully realized
		while (dp != RT_DIR_NULL) {
		    db_add_node_to_full_path(pp, dp);
		    s_it = s->find(pp);
		    // Children are not required to be unique in a comb, so we
		    // need to make sure we've not already deleted this
		    // particular child path from the set.
		    if (s_it != s->end()) {
			struct db_full_path *op = *s_it;
			s->erase(s_it);
			db_free_full_path(op);
			BU_PUT(op, struct db_full_path);
		    }
		    DB_FULL_PATH_POP(pp);
		    i++;
		    dp = children[i];
		}
		if (BU_STR_EQUAL(db_path_to_string(pp), "/component/bed/r1064")) {
		    bu_log("about to get weird\n");
		}
		char *fstr = db_path_to_string(pp);
		drawn.insert(std::string(fstr));
		bu_free(fstr, "fstr");
		bu_log("inserting fully realized comb: %s\n", db_path_to_string(pp));
		struct db_full_path *npp;
		BU_GET(npp, struct db_full_path);
		db_full_path_init(npp);
		db_dup_full_path(npp, pp);
		snext->insert(npp);
	    }
	    bu_free(children, "free children struct directory ptr array");

	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	}

	// If s is not empty after this process, we have more paths from other
	// sources to process, and we continue in a similar fashion.  Otherwise,
	// swap snext in for s to see if we have new parents to check.
	if (!s->size()) {
	    std::set<struct db_full_path *, dfp_cmp> *tmp = s;
	    s = snext;
	    snext = tmp;
	}
    }

    // Whatever ended up in finalized is what we report
    while (finalized.size()) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", finalized.begin()->c_str());
	finalized.erase(finalized.begin());
    }

    return GED_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
