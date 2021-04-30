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
	return (bu_strcmp(fp->fp_names[fp->fp_len-1]->d_namep, o->fp_names[fp->fp_len-1]->d_namep) > 0);
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
    struct bu_vls pvls = BU_VLS_INIT_ZERO;
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

    // Seed the sets with what we know to be drawn.  At this stage,
    // we don't yet know of anything that is not drawn.  We put the
    // known drawn objects into the drawn set, and queue their parents
    // for checking.
    std::set<std::string> not_drawn;
    std::set<std::string> drawn;
    std::set<std::string> finalized;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_scene_objs); i++) {
	struct bview_scene_obj *sobj = (struct bview_scene_obj *)BU_PTBL_GET(gedp->ged_gvp->gv_scene_objs, i);
	if (sobj->s_type_flags & BVIEW_DBOBJ_BASED) {
	    struct draw_update_data_t *ud = (struct draw_update_data_t *)sobj->s_i_data;

	    // This path is drawn
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, &ud->fp);
	    drawn.insert(std::string(bu_vls_cstr(&pvls)));

	    // The parent needs to be characterized, if there is one.
	    if (ud->fp.fp_len == 1) {
		// If there isn't anything above the current dp (such as a
		// solid with no hierarchy is drawn) it's a finalized object
		finalized.insert(std::string(bu_vls_cstr(&pvls)));
	    } else {
		// Have path depth - get the parent and queue it up
		struct db_full_path *fp;
		BU_GET(fp, struct db_full_path);
		db_full_path_init(fp);
		db_dup_full_path(fp, &ud->fp);
		DB_FULL_PATH_POP(fp);
		if (s->find(fp) == s->end()) {
		    s->insert(fp);
		} else {
		    db_free_full_path(fp);
		    BU_PUT(fp, struct db_full_path);
		}
	    }
	}
    }

    // Debugging - print out the set to confirm the ordering
    std::set<struct db_full_path *, dfp_cmp>::iterator s_it;
    for (s_it = s->begin(); s_it != s->end(); s_it++) {
	struct db_full_path *fp = *s_it;
	bu_vls_trunc(&pvls, 0);
	db_path_to_vls(&pvls, fp);
	bu_log("%zd %s\n", fp->fp_len, bu_vls_cstr(&pvls));
    }

    // Main characterization loop
    size_t longest = (s->size()) ? (*s->begin())->fp_len : 0;
    while (s->size()) {
	struct db_full_path *pp = *s->begin();
	s->erase(s->begin());

	// If we already know it's not drawn, don't process again
	bu_vls_trunc(&pvls, 0);
	db_path_to_vls(&pvls, pp);
	if (not_drawn.find(std::string(bu_vls_cstr(&pvls))) != not_drawn.end()) {
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}

	// If pp is shorter than the longest paths of this cycle, defer it
	// until the next round.  Longer paths need to be fully processed
	// before we can correctly characterize shorter ones, and it may happen
	// that a shorter path queues a parent that's also a parent in a longer
	// path path before we're ready for it.
	if (pp->fp_len < longest) {
	    snext->insert(pp);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}

	// Get the list of immediate children from pp:
	struct directory **children = NULL;
	struct rt_db_internal in;
	struct rt_comb_internal *comb;
	if (rt_db_get_internal(&in, DB_FULL_PATH_CUR_DIR(pp), gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
	    not_drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}
	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_comb_children(gedp->ged_wdbp->dbip, comb, &children, NULL, NULL);

	// If we have a comb with no children, we know the answer - it cannot
	// be drawn because there is nothing to draw.
	if (children[0] == RT_DIR_NULL) {
	    not_drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    bu_free(children, "free children struct directory ptr array");
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}

	int i = 0;
	int not_found = 0;
	struct directory *dp =  children[0];
	while (dp != RT_DIR_NULL) {
	    db_add_node_to_full_path(pp, dp);
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    if (drawn.find(std::string(bu_vls_cstr(&pvls))) == drawn.end()) {
		// This path is not drawn.  That means the parent comb is also
		// not (fully) drawn.
		not_found = 1;
	    }
	    i++;
	    dp = children[i];
	    DB_FULL_PATH_POP(pp);
	}

	// If everything is good, this is a drawn path.  Flag it and insert its
	// parent into the next queue.
	if (!not_found) {
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    if (pp->fp_len == 1) {
		// Fully drawn, no parent - finalize
		bu_vls_trunc(&pvls, 0);
		db_path_to_vls(&pvls, pp);
		finalized.insert(std::string(bu_vls_cstr(&pvls)));
	    } else {
		DB_FULL_PATH_POP(pp);
		bu_vls_trunc(&pvls, 0);
		db_path_to_vls(&pvls, pp);
		if (pp->fp_len && snext->find(pp) == snext->end()) {
		    struct db_full_path *npp;
		    BU_GET(npp, struct db_full_path);
		    db_full_path_init(npp);
		    db_dup_full_path(npp, pp);
		    snext->insert(npp);
		}
	    }
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	} else {
	    // If we had a not_found path, we need to finalize any child paths
	    // that are drawn - that is the terminating condition for walking
	    // back up paths.
	    i = 0;
	    dp = children[0];
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);

	    while (dp != RT_DIR_NULL) {
		db_add_node_to_full_path(pp, dp);
		bu_vls_trunc(&pvls, 0);
		db_path_to_vls(&pvls, pp);
		if (drawn.find(std::string(bu_vls_cstr(&pvls))) != drawn.end()) {
		    // This path is drawn - finalize
		    finalized.insert(std::string(bu_vls_cstr(&pvls)));
		}
		DB_FULL_PATH_POP(pp);
		i++;
		dp = children[i];
	    }
	    // Handled the children - the parent classifies as not_drawn
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    not_drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	}

	bu_free(children, "free children struct directory ptr array");


	// If s is not empty after this process, we have more paths from other
	// sources to process, and we continue in a similar fashion.  Otherwise,
	// swap snext in for s to see if we have new parents to check.
	if (!s->size()) {
	    std::set<struct db_full_path *, dfp_cmp> *tmp = s;
	    s = snext;
	    snext = tmp;
	    longest = (s->size()) ? (*s->begin())->fp_len : 0;
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
