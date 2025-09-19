/*                         Z A P . C P P
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
/** @file libged/zap.cpp
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>


#include "../ged_private.h"
#include "../dbi.h"

/*
 * Erase all currently displayed geometry
 *
 * Usage:
 * zap
 *
 */
extern "C" int
ged_zap2_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    int clear_view_objs = 0;
    int clear_solid_objs = 0;
    int clear_all_views = 0;
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    const char *usage = "zap [options]\n";

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[8];
    BU_OPT(d[0],  "h", "help",         "",        NULL,       &print_help, "Print help and exit");
    BU_OPT(d[1],  "?", "",             "",        NULL,       &print_help, "");
    BU_OPT(d[2],  "V", "view",     "name", &bu_opt_vls,             &cvls, "clear data in the specified view (will used default view if none specified)");
    BU_OPT(d[4],  "v", "view-objs",    "",        NULL,  &clear_view_objs, "clear non-solid based view objects (on by default unless -g specified)");
    BU_OPT(d[5],  "g", "solid-objs",   "",        NULL, &clear_solid_objs, "clear solid based view objects (on by default unless -v specified)");
    BU_OPT(d[6],  "",  "all",          "",        NULL,  &clear_all_views, "clear data in ALL views");
    BU_OPT_NULL(d[7]);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    argc = opt_ret;

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    if (argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (!clear_solid_objs && !clear_view_objs) {
	clear_solid_objs = 1;
	clear_view_objs = 1;
    }

    // If we're clearing everything, get down to it
    if (clear_all_views) {
	std::vector<BViewState *> views = gedp->dbi_state->FindBViewState();
	for (size_t i = 0; i < views.size(); i++) {
	    BViewState *vs = views[i];
	    if (clear_solid_objs)
		vs->Erase();
	    if (clear_view_objs)
		vs->Erase(-1, true);
	}
    }

    BViewState *vs = NULL;
    if (bu_vls_strlen(&cvls)) {
	std::vector<BViewState *> vstates = gedp->dbi_state->FindBViewState(bu_vls_cstr(&cvls));
	if (vstates.size() > 1) {
	    bu_vls_printf(gedp->ged_result_str, "Specifier %s matches more than one active view.\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	if (!vstates.size()) {
	    bu_vls_printf(gedp->ged_result_str, "Specifier %s does not match any active view.\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	vs = vstates[0];
    }
    bu_vls_free(&cvls);
    vs = (!vs) ? gedp->dbi_state->GetBViewState() : vs;

    if (!vs) {
	bu_vls_printf(gedp->ged_result_str, "Unable to find a BViewState to act on.\n");
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    if (clear_solid_objs)
	vs->Erase();
    if (clear_view_objs)
	vs->Erase(-1, true);

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
