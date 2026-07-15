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

#include <set>

#include "bu/cmdschema.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "ged.h"

#include "../dbi.h"
#include "../ged_private.h"

extern "C" int ged_who_solids_core(struct ged *gedp, int argc, const char *argv[]);


struct who2_args {
    int print_help;
    struct bu_vls view;
    int mode;
    int expand;
};


static const struct bu_cmd_option who2_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct who2_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_VLS_APPEND("V", "view", struct who2_args, view, "name",
	"Specify the view"),
    BU_CMD_INTEGER("m", "mode", struct who2_args, mode, "number",
	"Restrict the drawing mode"),
    BU_CMD_FLAG("E", "expand", struct who2_args, expand,
	"Report individual drawn objects"),
    BU_CMD_OPTION_NULL
};
extern "C" const struct bu_cmd_schema ged_who_new_schema = {
    "who", "List individually drawn objects", who2_schema_options,
    NULL, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static void
who2_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&ged_who_new_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage:\n"
	"  who [options]\n"
	"  who solids [-V view] [-m #] [level]\n");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "who native option help");
    }
}

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

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct who2_args args = {0, BU_VLS_INIT_ZERO, -1, 0};
    int opt_ret = bu_cmd_schema_parse_complete(&ged_who_new_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (opt_ret < 0 || opt_ret != argc - 1) {
	who2_usage(gedp);
	bu_vls_free(&args.view);
	return BRLCAD_ERROR;
    }

    if (args.print_help) {
	who2_usage(gedp);
	bu_vls_free(&args.view);
	return BRLCAD_OK;
    }

    if (!ged_dl(gedp)) {
	bu_vls_free(&args.view);
	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "A drawable does not exist.");
	return BRLCAD_ERROR;
    }

    struct bview *v = gedp->ged_gvp;
	if (bu_vls_strlen(&args.view)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&args.view));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&args.view));
	    bu_vls_free(&args.view);
	    return BRLCAD_ERROR;
	}
    }
	bu_vls_free(&args.view);

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED, nothing to generate a list of paths from");
	return BRLCAD_ERROR;
    }

    DbiState *dbis = (DbiState *)gedp->dbi_state;
    BViewState *bvs = dbis->get_view_state(v);
    std::vector<std::string> paths = bvs->list_drawn_paths(args.mode, (bool)!args.expand);
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
