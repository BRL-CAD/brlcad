/*                         H O W . C
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
/** @file libged/how.c
 *
 * The how command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/opt.h"
#include "bu/str.h"
#include "dm.h"
#include "../ged_private.h"

struct how_args {
    int both;
};

#define HOW_OPTIONS(args) \
    BU_OPT_FLAG(args, "b", NULL, both, \
	"Report both display mode and transparency"),

BU_OPT_DESC_BUILDER(how_options, struct how_args, HOW_OPTIONS);

static const ged_opt_spec how_opt_spec =
    GED_OPT("how", "Report how a database object is displayed",
	how_options, "options-first object:path");

static int
dl_how(struct bu_list *hdlp, struct bu_vls *vls, struct directory **dpp, int both)
{
    size_t i;
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    struct directory **tmp_dpp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    for (i = 0, tmp_dpp = dpp;
		 i < bdata->s_fullpath.fp_len && *tmp_dpp != RT_DIR_NULL;
		 ++i, ++tmp_dpp) {
		if (bdata->s_fullpath.fp_names[i] != *tmp_dpp)
		    break;
	    }

	    if (*tmp_dpp != RT_DIR_NULL)
		continue;


	    /* found a match */
	    if (sp->s_os->s_dmode == 4) {
		if (both)
		    bu_vls_printf(vls, "%d 1", _GED_HIDDEN_LINE);
		else
		    bu_vls_printf(vls, "%d", _GED_HIDDEN_LINE);
	    } else {
		if (both)
		    bu_vls_printf(vls, "%d %g", sp->s_os->s_dmode, sp->s_os->transparency);
		else
		    bu_vls_printf(vls, "%d", sp->s_os->s_dmode);
	    }

	    return 1;
	}

	gdlp = next_gdlp;
    }

    return 0;
}


/*
 * Returns "how" an object is being displayed.
 *
 * Usage:
 * how [-b] object
 *
 */
int
ged_how_core(struct ged *gedp, int argc, const char *argv[])
{
    int good;
    struct directory **dpp = NULL;
    struct how_args args = {0};
    int operand_count = 0;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, how_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count != 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }

    if ((dpp = _ged_build_dpp(gedp, argv[0])) == NULL)
	goto good_label;

    good = dl_how(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_result_str, dpp, args.both);

    /* match NOT found */
    if (!good) bu_vls_printf(gedp->ged_result_str, "-1");

good_label:
    if (dpp != (struct directory **)NULL)
	bu_free((void *)dpp, "ged_how_core: directory pointers");

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_HOW_COMMANDS(X, XID) \
    X(how, ged_how_core, GED_CMD_DEFAULT, &how_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_HOW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_how", 1, GED_HOW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
