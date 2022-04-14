/*                         Z A P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
    struct bview *v = gedp->ged_gvp;

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[7];
    BU_OPT(d[0],  "h", "help",          "",        NULL,       &print_help, "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",        NULL,       &print_help, "");
    BU_OPT(d[2],  "V", "view",     "name", &bu_opt_vls,             &cvls, "specify view to draw on");
    BU_OPT(d[3],  "v", "view-objs",    "",        NULL,  &clear_view_objs, "clear non-solid based view objects");
    BU_OPT(d[4],  "g", "solid-objs",   "",        NULL, &clear_solid_objs, "clear solid based view objects");
    BU_OPT(d[5],  "",  "all",          "",        NULL, &clear_all_views, "clear shared and independent views");
    BU_OPT_NULL(d[6]);

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    argc = opt_ret;

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_HELP;
    }

    if (argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (!clear_all_views && bu_vls_strlen(&cvls)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!v->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support clearing db objects in only this view.  To change the view's status, the command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    if (!clear_solid_objs && !clear_view_objs) {
	clear_solid_objs = 1;
	clear_view_objs = 1;
    }

    // If we're clearing just one view, handle it
    if (v && v->independent && !clear_all_views) {
	int flags = 0;
	if (clear_solid_objs)
	    flags |= BV_DB_OBJS;
	if (clear_view_objs)
	    flags |= BV_VIEW_OBJS;
	if (!bv_clear(v, flags))
	    v->gv_s->gv_cleared = 1;
	return BRLCAD_OK;
    }

    // Clear everything
    int ret = BRLCAD_OK;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	v = (struct bview *)BU_PTBL_GET(views, i);
	if (v->independent && !clear_all_views)
	    continue;
	int flags = BV_SHARED_OBJS;
	if (clear_solid_objs)
	    flags |= BV_DB_OBJS;
	if (clear_view_objs)
	    flags |= BV_VIEW_OBJS;
	int nret = bv_clear(v, flags);
	if (!nret)
	    v->gv_s->gv_cleared = 1;
	ret = BRLCAD_OK;
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
