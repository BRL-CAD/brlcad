/*                         K I L L R E F S . C
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
/** @file libged/killrefs.c
 *
 * The killrefs command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"

struct killrefs_args {
    int print;
};

#define KILLREFS_OPTIONS(args) \
    BU_OPT_FLAG(args, "n", NULL, print, \
	"Report references without removing them"),

BU_OPT_DESC_BUILDER(killrefs_options, struct killrefs_args, KILLREFS_OPTIONS);
static const ged_opt_spec killrefs_opt_spec =
    GED_OPT("killrefs", "Remove references to database objects",
	killrefs_options, "options-first objects:object+");


int
ged_killrefs_core(struct ged *gedp, int argc, const char *argv[])
{
    int k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct killrefs_args args = {0};
    int object_count;
    int ret;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (!gedp->ged_internal_call) {
	/* initialize result */
	bu_vls_trunc(gedp->ged_result_str, 0);
    }

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    object_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, killrefs_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (object_count < 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }


    if (!args.print && !gedp->ged_internal_call) {
	for (k = 0; k < object_count; k++)
	    _dl_eraseAllNamesFromDisplay(gedp, argv[k], 1);
    }

    ret = BRLCAD_OK;

    FOR_ALL_DIRECTORY_START(dp, gedp->dbip) {
	if (!(dp->d_flags & RT_DIR_COMB))
	    continue;

	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
	    ret = BRLCAD_ERROR;
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	for (k = 0; k < object_count; k++) {
	    int code;

	    code = db_tree_rm_dbleaf(&(comb->tree), argv[k], args.print);
	    if (code == -1)
		continue;	/* not found */
	    if (code == -2)
		continue;	/* empty tree */
	    if (code < 0) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, argv[k]);
		ret = BRLCAD_ERROR;
	    } else {
		if (args.print)
		    bu_vls_printf(gedp->ged_result_str, "%s ", dp->d_namep);
		else
		    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, argv[k]);
	    }
	}

	if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Unable to write new combination into database.\n");
	    ret = BRLCAD_ERROR;
	    continue;
	}
    } FOR_ALL_DIRECTORY_END;

    /* Update references. */
    db_update_nref(gedp->dbip);

    return ret;
}

#include "../include/plugin.h"

#define GED_KILLREFS_COMMANDS(X, XID) \
    X(killrefs, ged_killrefs_core, GED_CMD_DEFAULT, &killrefs_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_KILLREFS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_killrefs", 1, GED_KILLREFS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
