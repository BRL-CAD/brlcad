/*                         T O P S . C
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
/** @file libged/tops.c
 *
 * The tops command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"

struct tops_args {
    int all;
    int hidden;
    int no_decorate;
    int phony;
};

#define TOPS_OPTIONS(args) \
    BU_OPT_FLAG(args, "a", NULL, all, "Include all top-level objects"), \
    BU_OPT_FLAG(args, "h", NULL, hidden, "Include hidden top-level objects"), \
    BU_OPT_FLAG(args, "n", NULL, no_decorate, "Do not decorate object names"), \
    BU_OPT_FLAG(args, "p", NULL, phony, "Include phony top-level objects"),

BU_OPT_DESC_BUILDER(tops_options, struct tops_args, TOPS_OPTIONS);
static const ged_opt_spec tops_opt_spec =
    GED_OPT("tops", "List top-level database objects", tops_options,
	"options-first");


int
ged_tops_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    struct tops_args args = {0, 0, 0, 0};
    int operand_count = 0;

    /* static const char *usage = "[-a|-h|-n|-p]"; */

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc - 1, argv + 1, tops_options, &args,
	BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count != 0) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return BRLCAD_ERROR;
    }

    /* Can this be executed only sometimes?
       Perhaps a "dirty bit" on the database? */
    db_update_nref(gedp->dbip);

    /*
     * Find number of possible entries and allocate memory
     */
    dirp = _ged_dir_getspace(gedp->dbip, 0);
    dirp0 = dirp;

    if (db_version(gedp->dbip) < 5) {
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	    if (dp->d_nref == 0)
		*dirp++ = dp;
	FOR_ALL_DIRECTORY_END;
    } else {
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip)

		if (dp->d_nref != 0) {
		    continue;
		}

		if ((args.all) ||
		    (args.hidden && (dp->d_flags & RT_DIR_HIDDEN)) ||
		    (args.phony && dp->d_addr == RT_DIR_PHONY_ADDR)) {

		    /* add object because it matches an option */
		    *dirp++ = dp;

		} else if (!args.all && !args.hidden && !args.phony &&
			   !(dp->d_flags & RT_DIR_HIDDEN) &&
			   (dp->d_addr != RT_DIR_PHONY_ADDR)) {

		    /* add non-hidden real object */
		    *dirp++ = dp;

		}
	    FOR_ALL_DIRECTORY_END;
    }

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), args.no_decorate, 0);
    bu_free((void *)dirp0, "wdb_tops_cmd: wdb_dir_getspace");

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_TOPS_COMMANDS(X, XID) \
    X(tops, ged_tops_core, GED_CMD_DEFAULT, &tops_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_TOPS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_tops", 1, GED_TOPS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
