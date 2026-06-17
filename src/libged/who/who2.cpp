/*                         W H O . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include "ged.h"
#include "ged/bsg_ged_draw.h"

#include "../ged_private.h"

extern "C" int ged_who_solids_core(struct ged *gedp, int argc, const char *argv[]);

/*
 * List the db objects currently drawn
 *
 * Usage:
 * who [options]
 * who solids [level]
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets - if we're adaptive plotting, for example.
 */
extern "C" int
ged_who2_core(struct ged *gedp, int argc, const char *argv[])
{
    if (argc > 1 && (BU_STR_EQUAL(argv[1], "solids") || BU_STR_EQUAL(argv[1], "report")))
	return ged_who_solids_core(gedp, argc, argv);

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* skip command name argv[0] */
    if (argc > 0) {
	argc--;
	argv++;
    }

    int expand = 0;
    int print_help = 0;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    static const char *usage =
	"Usage:\n"
	"  who [options]\n"
	"  who [real|phony|both]\n"
	"  who solids [-V view] [-m #] [level]\n";
    struct bu_opt_desc vd[6];
    int mode = -1;
    BU_OPT(vd[0],  "h", "help",    "",          NULL,        &print_help, "Print help and exit");
    BU_OPT(vd[1],  "?", "",        "",          NULL,        &print_help, "");
    BU_OPT(vd[2],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to work with");
    BU_OPT(vd[3],  "m", "mode",    "#",         &bu_opt_int, &mode,   "only report paths drawn in the specified drawing mode");
    BU_OPT(vd[4],  "E", "expand",  "",          NULL,        &expand, "report individual drawn objects");
    BU_OPT_NULL(vd[5]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    if (opt_ret < 0) {
	_ged_cmd_help(gedp, usage, vd);
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    if (print_help) {
	_ged_cmd_help(gedp, usage, vd);
	bu_vls_free(&cvls);
	return BRLCAD_OK;
    }

    argc = opt_ret;
    if (argc) {
	_ged_cmd_help(gedp, usage, vd);
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    struct bsg_view *v = gedp->ged_gvp;
    if (bu_vls_strlen(&cvls)) {
	v = bsg_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
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

    ged_draw_list_paths(gedp, v, mode, expand, gedp->ged_result_str);

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
