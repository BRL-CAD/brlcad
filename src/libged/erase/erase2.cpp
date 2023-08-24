/*                         E R A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
#include <unordered_map>

#include <stdlib.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/sort.h"
#include "bv/view_sets.h"
#include "ged/database.h"
#include "ged/view.h"
#include "ged/view/state.h"

/*
 * Erase objects from the scene.  In many ways this is the most complex
 * operation we need to perform on a displayed set of objects.  When subsets of
 * objects are erased, the top level "who" objects need to be split into new
 * objects, with each new "who" object being the highest level child instance
 * available in the hierarchy that is still fully drawn.
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[object(s)]";
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Erase may operate on a specific user specified view.  If it does so,
     * we want the default settings to reflect those set in that particular
     * view.  In order to set up the correct default views, we need to know
     * if a specific view has in fact been specified.  We do a preliminary
     * option check to figure this out */
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[3];
    int mode = -1;
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT(vd[1],  "m", "mode",    "#",         &bu_opt_int, &mode,   "erase objects drawn in the specified drawing mode");
    BU_OPT_NULL(vd[2]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!v->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, the command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);


    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED, nothing to erase from");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    argc--; argv++;

    if (!gedp->dbi_state)
	return BRLCAD_OK;

    BViewState *bvs = gedp->dbi_state->get_view_state(v);
    bvs->erase_path(mode, argc, argv);

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

