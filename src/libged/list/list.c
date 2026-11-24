/*                         L I S T . C
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
/** @file libged/list.c
 *
 * The l command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/arg.h"
#include "bu/units.h"

#include "../ged_private.h"


struct list_args {
    int recurse;
    long verbosity;
};

static int
list_terse_parse(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage)
	*((long *)storage) = 0;
    return 0;
}

static const struct bu_arg_option list_options[] = {
    BU_ARG_FLAG("r", NULL, struct list_args, recurse,
	"Recursively list evaluated contents"),
    BU_ARG_CUSTOM_FLAG("t", NULL, "t", struct list_args, verbosity,
	list_terse_parse, "Use terse output"),
    BU_ARG_COUNTING_FLAG("v", NULL, struct list_args, verbosity,
	BU_ARG_VALUE_LONG, "Increase output verbosity"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand list_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_PATH, 1,
	BU_ARG_COUNT_UNLIMITED, "Objects to describe", "ged.db_path"),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema list_cmd_schema =
    BU_ARG_SCHEMA_BOUND("list", "Describe database objects", list_options,
	list_operands, BU_ARG_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);
static const struct bu_arg_schema l_cmd_schema =
    BU_ARG_SCHEMA_BOUND("l", "Describe database objects", list_options,
	list_operands, BU_ARG_PARSE_OPTIONS_FIRST, NULL, NULL, NULL, NULL);


static void
list_show_help(struct ged *gedp, const char *command)
{
    char *help = ged_cmd_help(command, command);

    if (help) {
	bu_vls_sprintf(gedp->ged_result_str, "%s", help);
	bu_free(help, "list native schema help");
    }
}

int
ged_list_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int arg;
    int id;
    struct list_args args = {0, 99};
    int operand_index;
    const char *command = argv[0];
    char *terse_parm = "-t";
    struct rt_db_internal intern;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	list_show_help(gedp, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(
	BU_STR_EQUAL(command, "l") ? &l_cmd_schema : &list_cmd_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	list_show_help(gedp, command);
	return BRLCAD_ERROR;
    }
    argc -= operand_index;
    argv += operand_index;

    for (arg = 0; arg < argc; arg++) {
	if (args.recurse) {
	    char *tmp_argv[3] = {"listeval", NULL, NULL};
	    if (args.verbosity) {
		tmp_argv[1] = (char *)argv[arg];
		ged_exec_listeval(gedp, 2, (const char **)tmp_argv);
	    } else {
		tmp_argv[1] = terse_parm;
		tmp_argv[2] = (char *)argv[arg];
		ged_exec_listeval(gedp, 3, (const char **)tmp_argv);
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
		_ged_do_list(gedp, dp, (int)args.verbosity);	/* very verbose */
		continue;
	    }

	    struct db_tree_state ts;
	    struct db_full_path path;

	    db_full_path_init(&path);
	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    ts = wdbp->wdb_initial_tree_state;     /* struct copy */
	    ts.ts_dbip = gedp->dbip;
	    MAT_IDN(ts.ts_mat);

	    if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
		continue;

	    if ((id = rt_db_get_internal(&intern, dp, gedp->dbip, ts.ts_mat)) < 0) {
		bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
		continue;
	    }

	    db_free_full_path(&path);

	    bu_vls_printf(gedp->ged_result_str, "%s:  ", argv[arg]);

	    if (!OBJ[id].ft_describe
		|| OBJ[id].ft_describe(gedp->ged_result_str, &intern, args.verbosity, gedp->dbip->dbi_base2local) < 0)
	    {
		bu_vls_printf(gedp->ged_result_str, "%s: describe error", dp->d_namep);
	    }

	    rt_db_free_internal(&intern);
	} else {
	    if ((dp = db_lookup(gedp->dbip, argv[arg], LOOKUP_NOISY)) == RT_DIR_NULL)
		continue;

	    _ged_do_list(gedp, dp, (int)args.verbosity);	/* very verbose */
	}
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_LIST_COMMANDS(X, XID) \
    X(list, ged_list_core, GED_CMD_DEFAULT, &list_cmd_schema) \
    X(l, ged_list_core, GED_CMD_DEFAULT, &l_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LIST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_list", 1, GED_LIST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
