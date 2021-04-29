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


struct dfp_cmp {
    bool operator() (struct db_full_path *fp, struct db_full_path *o) const {
	// First, check length
	if (fp->fp_len != o->fp_len) {
	    return (fp->fp_len < o->fp_len);
	}

	// If length is the same, check the dp contents
	for (size_t i = 0; i < fp->fp_len; i++) {
	    if (fp->fp_names[i] != o->fp_names[i]) {
		return (fp->fp_names[i] < o->fp_names[i]);
	    }
	}

	// If we have boolean flags, try those...
	if ((fp->fp_bool && !o->fp_bool) || (!fp->fp_bool && o->fp_bool)) {
	    return (fp->fp_bool < o->fp_bool);
	}
	if (fp->fp_bool) {
	    for (size_t i = 0; i < fp->fp_len; i++) {
		if (fp->fp_bool[i] != o->fp_bool[i]) {
		    return (fp->fp_bool[i] < o->fp_bool[i]);
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
 */

// TODO - analyze DB scene objects - they will each have full paths
// corresponding to a solid.  We need to figure out which parent combs (if any)
// are fully realized by having all their child solid objects drawn, and return
// those shorter paths rather than listing out all the solids - that's the most
// useful output for the user, although it's a lot harder.  (The current logic
// doesn't do it after a full-path erase has been applied, as near as I can
// tell...)
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

    // TODO - need to process deepest to shallowest, so we don't get bit by
    // starting at a high level path - we need to check the deepest parent, and
    // build up from the bottom.  Make sure the dbfp class's sorting results in
    // the longest path always returning from begin()
    while (s->size()) {
	struct db_full_path *fp = *s->begin();
	s->erase(s->begin());

	// if fp_len == 1, this is a top level object and its finalized -
	// add it to the final set.

	// If we have a deeper path: pop the last path off of fp, and get
	// the list of immediate children from the new current dir comb.
	// For each of those children, construct the full path and see if
	// it is present in s.  If all child paths are present in s, the
	// parent is fully realized in the scene and all child paths may
	// be deleted.  The shorter path is then added to snext.  If all
	// child paths are NOT present in s, they are all finalized since
	// their immediate parent is not fully realized in the scene and
	// they themselves are the drawn object instances.
	//
	// If s is not empty after this process, we have more paths from
	// other sources to process, and we continue in a similar fashion.
	DB_FULL_PATH_POP(fp);

	if (fp->fp_len) {
	    if (snext->find(fp) == snext->end()) {
		snext->insert(fp);
	    } else {
		db_free_full_path(fp);
		BU_PUT(fp, struct db_full_path);
	    }
	}

	if (!s->size()) {
	    std::set<struct db_full_path *, dfp_cmp> *tmp = s;
	    s = snext;
	    snext = tmp;
	}
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
