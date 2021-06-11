/*                       V I E W . C P P
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
/** @file libged/view.cpp
 *
 * This file contains view related utility functions.
 *
 */

#include "common.h"

#include <set>

#include "bu/ptbl.h"
#include "ged.h"

// For this routine to work correctly, we are relying on the children of the
// group being full paths to solid objects.  Because they are, checking the
// dp's edit_flag for each object in each path means we will see if any combs
// anywhere along the paths have changed.  We only need to look until we either
// find something that prompts a draw event for the group, or until we have
// confirmed that we have no need to redraw.
extern "C" int
ged_view_update(struct ged *gedp)
{
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct bview *v = gedp->ged_gvp;
    struct bu_ptbl *sg = v->gv_db_grps;
    std::set<struct bv_scene_group *> regen;
    std::set<struct bv_scene_group *> erase;
    std::set<struct bv_scene_group *>::iterator r_it;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	// When checking a scene group, there are two things to confirm:
	// 1.  All dp pointers in all paths are valid
	// 2.  No edit flags have been set.
	// If either of these things is not true, the group must be
	// regenerated.
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	int invalid = 0;
	for (size_t j = 0; j < BU_PTBL_LEN(&cg->g->children); j++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->g->children, j);
	    struct draw_update_data_t *ud = (struct draw_update_data_t *)s->s_i_data;

	    // First, check the root - if it is no longer present, we're
	    // erasing rather than redrawing.
	    struct directory *dp = db_lookup(dbip, ud->fp.fp_names[0]->d_namep, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		erase.insert(cg);
		break;
	    }

	    for (size_t fp_i = 0; fp_i < ud->fp.fp_len; fp_i++) {
		dp = db_lookup(dbip, ud->fp.fp_names[fp_i]->d_namep, LOOKUP_QUIET);
		if (dp == RT_DIR_NULL || dp != ud->fp.fp_names[fp_i]|| dp->edit_flag) {
		    invalid = 1;
		    break;
		}
	    }
	    if (invalid) {
		regen.insert(cg);
		break;
	    }
	}
    }

    // Edit flags have done their job - reset
    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    dp->edit_flag = 0;
	}
    }

    for (r_it = erase.begin(); r_it != erase.end(); r_it++) {
	struct bv_scene_group *cg = *r_it;
	struct bu_vls opath = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&opath, "%s", bu_vls_cstr(&cg->g->s_name));
	const char *av[3];
	av[0] = "erase";
	av[1] = bu_vls_cstr(&opath);
	av[2] = NULL;
	ged_exec(gedp, 2, av);
	bu_vls_free(&opath);
    }
    for (r_it = regen.begin(); r_it != regen.end(); r_it++) {
	struct bv_scene_group *cg = *r_it;
	struct bu_vls opath = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&opath, "%s", bu_vls_cstr(&cg->g->s_name));
	const char *av[4];
	av[0] = "draw";
	av[1] = "-R";
	av[2] = bu_vls_cstr(&opath);
	av[3] = NULL;
	ged_exec(gedp, 3, av);
	bu_vls_free(&opath);
    }

    return (int)(regen.size() + erase.size());
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

