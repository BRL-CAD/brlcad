/*                         E R A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
#include "ged/database.h"
#include "ged/view.h"

#include "../dbi.h"

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

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* skip past cmd */
    argc--; argv++;

    /* Erase may operate on a specific user specified view. */
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[5];
    int mode = -1;
    int scene_objs = 0;
    BU_OPT(vd[0],  "V", "view",       "name",      &bu_opt_vls, &cvls,       "Specify view to draw on.  Note that if a view is linked, objects originating from the linked view will not be cleared.");
    BU_OPT(vd[1],  "m", "mode",       "#",         &bu_opt_int, &mode,       "Erase only one specific drawing mode for specified objects.");
    BU_OPT(vd[2],  "S", "scene-objs", "",          NULL,        &scene_objs, "Specifiers are for view scene objects rather than database paths.");
    BU_OPT_NULL(vd[2]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;

    std::vector<BViewState *> vsv;
    if (bu_vls_strlen(&cvls)) {
	std::string vname(bu_vls_cstr(&cvls));
	vsv = gedp->dbi_state->FindBViewState(vname.c_str());
	if (!vsv.size()) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view(s) %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    } else {
	BViewState *dv = gedp->dbi_state->GetBViewState(NULL);
	vsv.push_back(dv);
    }
    bu_vls_free(&cvls);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    for (size_t i = 0; i < vsv.size(); i++)
	vsv[i]->Erase(mode, scene_objs, argc, argv);

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

