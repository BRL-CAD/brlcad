/*                         L I S T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/list.c
 *
 * The l command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/units.h"

#include "../ged_private.h"

int
ged_list_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int arg;
    int id;
    int c;
    int recurse = 0;
    int verbose = 99;
    char *terse_parm = "-t";
    char *listeval = "listeval";
    struct rt_db_internal intern;
    static const char *usage = "[-r] [-t] [-v] <objects>";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;      /* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "rtv")) != -1) {
	switch (c) {
	    case 'r':
		recurse = 1;
		break;
	    case 't': /* terse */
		verbose = 0;
		break;
	    case 'v': /* verbose */
		verbose++;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return BRLCAD_ERROR;
	}
    }

    /* skip options processed plus command name */
    argc -= bu_optind;
    argv += bu_optind;

    for (arg = 0; arg < argc; arg++) {
	if (recurse) {
	    char *tmp_argv[3] = {NULL, NULL, NULL};

	    tmp_argv[0] = listeval;
	    if (verbose) {
		tmp_argv[1] = (char *)argv[arg];
		ged_exec(gedp, 2, (const char **)tmp_argv);
	    } else {
		tmp_argv[1] = terse_parm;
		tmp_argv[2] = (char *)argv[arg];
		ged_exec(gedp, 3, (const char **)tmp_argv);
	    }
	} else if (strchr(argv[arg], '/')) {
	    if ((dp = db_lookup(gedp->dbip, argv[arg], LOOKUP_QUIET)) == RT_DIR_NULL) {
		continue;
	    }

	    /* dp should have resolved to a shape. A slash still in d_namep likely means the 
	     string is a name with a slash, not a path. 
	     NOTE: this only works if the user is requesting a top-level name with a slash. 
	     A slashed name anywhere else in the hierarchy will fail the db_lookup */
	    if (strchr(dp->d_namep, '/')) {
		_ged_do_list(gedp, dp, verbose);	/* very verbose */
		continue;
	    }

	    struct db_tree_state ts;
	    struct db_full_path path;

	    db_full_path_init(&path);
	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    ts = wdbp->wdb_initial_tree_state;     /* struct copy */
	    ts.ts_dbip = gedp->dbip;
	    ts.ts_resp = &rt_uniresource;
	    MAT_IDN(ts.ts_mat);

	    if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
		continue;

	    if ((id = rt_db_get_internal(&intern, dp, gedp->dbip, ts.ts_mat, &rt_uniresource)) < 0) {
		bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
		continue;
	    }

	    db_free_full_path(&path);

	    bu_vls_printf(gedp->ged_result_str, "%s:  ", argv[arg]);

	    if (!OBJ[id].ft_describe
		|| OBJ[id].ft_describe(gedp->ged_result_str, &intern, verbose, gedp->dbip->dbi_base2local) < 0)
	    {
		bu_vls_printf(gedp->ged_result_str, "%s: describe error", dp->d_namep);
	    }

	    rt_db_free_internal(&intern);
	} else {
	    if ((dp = db_lookup(gedp->dbip, argv[arg], LOOKUP_NOISY)) == RT_DIR_NULL)
		continue;

	    _ged_do_list(gedp, dp, verbose);	/* very verbose */
	}
    }

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl list_cmd_impl = {"list", ged_list_core, GED_CMD_DEFAULT};
const struct ged_cmd list_cmd = { &list_cmd_impl };

struct ged_cmd_impl l_cmd_impl = {"l", ged_list_core, GED_CMD_DEFAULT};
const struct ged_cmd l_cmd = { &l_cmd_impl };

const struct ged_cmd *list_cmds[] = { &list_cmd, &l_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  list_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
