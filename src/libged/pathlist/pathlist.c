/*                         P A T H L I S T . C
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
/** @file libged/pathlist.c
 *
 * The pathlist command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"


struct pathlist_args {
    int no_leaf;
};

struct pathlist_walk_state {
    struct ged *gedp;
    int no_leaf;
};


static union tree *
pathlist_leaf_func(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data)
{
    struct pathlist_walk_state *state = (struct pathlist_walk_state *)client_data;
    struct ged *gedp = state->gedp;
    char *str;

    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);

    if (state->no_leaf) {
	struct db_full_path pp;
	db_full_path_init(&pp);
	db_dup_full_path(&pp, pathp);
	--pp.fp_len;
	str = db_path_to_string(&pp);
	bu_vls_printf(gedp->ged_result_str, " %s", str);
	db_free_full_path(&pp);
    } else {
	str = db_path_to_string(pathp);
	bu_vls_printf(gedp->ged_result_str, " %s", str);
    }

    bu_free((void *)str, "path string");
    return TREE_NULL;
}


#define PATHLIST_OPTIONS(args) \
    BU_OPT_FLAG(args, "n", "noleaf", no_leaf, \
	"Omit leaf names from reported paths"),

BU_OPT_DESC_BUILDER(pathlist_options, struct pathlist_args, PATHLIST_OPTIONS);
static const ged_opt_spec pathlist_opt_spec =
    GED_OPT("pathlist", "List leaf paths below a database object",
	pathlist_options, "options-first object:object");


int
ged_pathlist_core(struct ged *gedp, int argc, const char *argv[])
{
    int operand_count;
    struct pathlist_args args = {0};
    struct pathlist_walk_state state = {gedp, 0};
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
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
	argc, argv, pathlist_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count != 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    argc = operand_count;
    state.no_leaf = args.no_leaf;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (db_walk_tree(gedp->dbip, argc, (const char **)argv, 1,
		     &wdbp->wdb_initial_tree_state,
		     0, 0, pathlist_leaf_func, (void *)&state) < 0) {
	bu_vls_printf(gedp->ged_result_str, "ged_pathlist_core: db_walk_tree() error");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_PATHLIST_COMMANDS(X, XID) \
    X(pathlist, ged_pathlist_core, GED_CMD_DEFAULT, &pathlist_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_PATHLIST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_pathlist", 1, GED_PATHLIST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
