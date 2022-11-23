/*                       A U T O V I E W . C P P
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
/** @file libged/autoview.cpp
 *
 * The autoview command.
 *
 */

#include "common.h"
#include "bu/opt.h"
#include "dm.h"
#include "../ged_private.h"

/*
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 * autoview
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 *
 */
extern "C" int
ged_autoview2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[options] [scale]";
    struct bu_vls cvls = BU_VLS_INIT_ZERO;

    /* default, 0.5 model scale == 2.0 view factor */
    fastf_t factor = BV_AUTOVIEW_SCALE_DEFAULT;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    int all_view_objs = 0;
    int print_help = 0;
    struct bview *v = gedp->ged_gvp;

    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",      "",        NULL,     &print_help, "Print help and exit");
    BU_OPT(d[1], "",   "all-objs", "",        NULL,  &all_view_objs, "Bound all non-faceplate view objects");
    BU_OPT(d[2], "V", "view",  "name", &bu_opt_vls,           &cvls, "Specify view to adjust");
    BU_OPT_NULL(d[3]);

    argc-=(argc>0); argv+=(argc>0); /* skip command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    argc = opt_ret;

    if (argc && argc != 1) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_ERROR;
    }

    if (bu_vls_strlen(&cvls)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);


    /* parse the optional scale argument */
    if (argc) {
	double scale = 0.0;
	int ret = sscanf(argv[0], "%lf", &scale);
	if (ret != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Expecting floating point scale value\n");
	    return BRLCAD_ERROR;
	}
	if (scale > 0.0) {
	    factor = 1.0 / scale;
	}
    }

    // libbv has the nuts and bolts
    bv_autoview(v, factor, all_view_objs);

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

