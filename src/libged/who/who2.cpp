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
	    if (fp->fp_names[i] != o->fp_names[i]) {
		return (fp->fp_names[i] > o->fp_names[i]);
	    }
	}

	// If we have boolean flags, try those...
	if ((fp->fp_bool && !o->fp_bool) || (!fp->fp_bool && o->fp_bool)) {
	    return (fp->fp_bool > o->fp_bool);
	}
	if (fp->fp_bool) {
	    for (size_t i = 0; i < fp->fp_len; i++) {
		if (fp->fp_bool[i] != o->fp_bool[i]) {
		    return (fp->fp_bool[i] > o->fp_bool[i]);
		}
	    }
	}

	// All checks done - they look the same
	return false;
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

    std::set<struct db_full_path *, dfp_cmp>::iterator s_it;
    for (s_it = s->begin(); s_it != s->end(); s_it++) {
	char *fp = db_path_to_string(*s_it);
	bu_log("%zd %s\n", (*s_it)->fp_len, fp);
	bu_free(fp, "path string");
    }

    std::set<struct db_full_path *, dfp_cmp> finalized;
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

	// if pp_len == 0, fp is a top level object and its finalized -
	// add it to the final set.
	if (!pp->fp_len) {
	    s->erase(fp);
	    finalized.insert(fp);
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	} else {
	    // If we have a deeper path:
	    // 1. get the list of immediate children from pp;
	    struct directory **children = NULL;
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;
	    if (rt_db_get_internal(&in, DB_FULL_PATH_CUR_DIR(pp), gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
		finalized.insert(fp);
		db_free_full_path(pp);
		BU_PUT(pp, struct db_full_path);
	    }
	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    db_comb_children(gedp->ged_wdbp->dbip, comb, &children, NULL, NULL);

	    // For all children, see if the path is in s
	    int i = 0;
	    int not_found = 0;
	    struct directory *dp =  children[0];
	    while (dp != RT_DIR_NULL) {
		db_add_node_to_full_path(pp, dp);
		char *str = db_path_to_string(pp);
		if (BU_STR_EQUAL(str, "/all.g/tor.r")) {
		}
		if (s->find(pp) == s->end()) {
		    not_found = 1;
		    bu_log("%s not found\n", str);
		    DB_FULL_PATH_POP(pp);
		    break;
		}
		DB_FULL_PATH_POP(pp);
		i++;
		dp = children[i];
	    }
	    i = 0;
	    dp =  children[0];

	    // If all child paths are present in s, the parent is fully
	    // realized in the scene and all child paths may be deleted.  The
	    // shorter path is then added to snext.  If all child paths are NOT
	    // present in s, they are all finalized since their immediate
	    // parent is not fully realized in the scene and they themselves
	    // are the drawn object instances.
	    if (not_found) {
		while (dp != RT_DIR_NULL) {
		    db_add_node_to_full_path(pp, dp);
		    s_it = s->find(pp);
		    if (s_it != s->end()) {
			finalized.insert(*s_it);
			s->erase(s_it);
		    }
		    DB_FULL_PATH_POP(pp);
		    i++;
		    dp = children[i];
		}
		db_free_full_path(pp);
		BU_PUT(pp, struct db_full_path);
	    } else {
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
		snext->insert(pp);
	    }
	    bu_free(children, "free children struct directory ptr array");
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
	struct db_full_path *fp = *finalized.begin();
	finalized.erase(finalized.begin());
	char *sfp = db_path_to_string(fp);
	bu_vls_printf(gedp->ged_result_str, "%s\n", sfp);
	bu_free(sfp, "path string");
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
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
