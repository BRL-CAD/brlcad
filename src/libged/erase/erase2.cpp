/*                         E R A S E . C P P
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
/** @file libged/erase.cpp
 *
 * The erase command.
 *
 */

#include "common.h"

#include <set>

#include <stdlib.h>
#include <string.h>


#include "../ged_private.h"

/*
 * Erase objects from the display.
 *
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[object(s)]";
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No current view defined in GED, nothing to erase from");
	return GED_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    argc--; argv++;

    /* Build the processing sets */
    std::set<struct bview_scene_obj *> to_clear;
    std::set<struct bview_scene_obj *> all_objs;
    for (size_t i = 0; i < BU_PTBL_LEN(gedp->ged_gvp->gv_scene_objs); i++) {
	struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(v->gv_scene_objs, i);
	if (s->s_type_flags & BVIEW_DBOBJ_BASED) {
	    if (!s->s_u_data)
		continue;
	    all_objs.insert(s);
	}
    }

    /* For each supplied path, check the scene objects against it for matches */
    std::set<struct bview_scene_obj *>::iterator s_it;
    for (size_t i = 0; i < (size_t)argc; ++i) {
	struct db_full_path fp;
	db_full_path_init(&fp);
	int ret = db_string_to_path(&fp, dbip, argv[i]);
	if (ret < 0) {
	    // Validate path.  We postpone actual scene object removal until all
	    // paths supplied have successfully validated, so we don't end up
	    // with partial removals
	    db_free_full_path(&fp);
	    bu_vls_printf(gedp->ged_result_str, "Invalid path: %s\n", argv[i]);
	    return GED_ERROR;
	}
	for (s_it = all_objs.begin(); s_it != all_objs.end(); s_it++) {
	    struct bview_scene_obj *s = *s_it;
	    struct ged_bview_data *bdata = (struct ged_bview_data *)s->s_u_data;
	    // Anything below the supplied path in the hierarchy is removed
	    if (db_full_path_match_top(&fp, &bdata->s_fullpath)) {
		to_clear.insert(s);
	    }
	}
	// Anything already flagged for removal doesn't need to be checked against
	// other paths
	for (s_it = to_clear.begin(); s_it != to_clear.end(); s_it++) {
	    struct bview_scene_obj *s = *s_it;
	    all_objs.erase(s);
	}
	db_free_full_path(&fp);
    }

    // Built up the removal set - now clear from table
    for (s_it = to_clear.begin(); s_it != to_clear.end(); s_it++) {
	struct bview_scene_obj *s = *s_it;
	bu_ptbl_rm(gedp->ged_gvp->gv_scene_objs, (long *)s);
	bview_scene_obj_free(s);
	BU_PUT(s, struct bview_scene_obj);
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
