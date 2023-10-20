/*                         W H O . C P P
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

/*
 * List the db objects currently drawn
 *
 * Usage:
 * who
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 */
extern "C" int
ged_who2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    int expand = 0;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[4];
    int mode = -1;
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to work with");
    BU_OPT(vd[1],  "m", "mode",    "#",         &bu_opt_int, &mode,   "only report paths drawn in the specified drawing mode");
    BU_OPT(vd[2],  "E", "expand",  "",          NULL,        &expand, "report individual drawn objects");
    BU_OPT_NULL(vd[3]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    if (opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "who cmd: option parsing error\n");
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }
    struct bview *v = gedp->ged_gvp;
    if (bu_vls_strlen(&cvls)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED, nothing to generate a list of paths from");
	return BRLCAD_ERROR;
    }

    BViewState *bvs = gedp->dbi_state->get_view_state(v);
    std::vector<std::string> paths = bvs->list_drawn_paths(mode, (bool)!expand);
    for (size_t i = 0; i < paths.size(); i++) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", paths[i].c_str());
    }

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

