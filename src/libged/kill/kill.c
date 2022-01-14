/*                         K I L L . C
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
/** @file libged/kill.c
 *
 * The kill command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"

#include "../ged_private.h"


int
ged_kill_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int c;
    int is_phony;
    int verbose = LOOKUP_NOISY;
    int force = 0;
    int nflag = 0;
    static const char *usage = "[-f|-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "fnq")) != -1) {
	switch (c) {
	    case 'f':
		force = 1;
		break;
	    case 'n':
		nflag = 1;
		break;
	    case 'q':
		verbose = LOOKUP_QUIET;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	}
    }

    if ((force + nflag) > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (nflag) {
	bu_vls_printf(gedp->ged_result_str, "{");
	for (i = 1; i < argc; i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", argv[i]);
	bu_vls_printf(gedp->ged_result_str, "} {}");

	return GED_OK;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(gedp->dbip,  argv[i], verbose)) != RT_DIR_NULL) {
	    if (!force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
		bu_vls_printf(gedp->ged_result_str, "You attempted to delete the _GLOBAL object.\n");
		bu_vls_printf(gedp->ged_result_str, "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n");
		bu_vls_printf(gedp->ged_result_str, "\tsuch as your preferred units and the title of the database.\n");
		bu_vls_printf(gedp->ged_result_str, "\tUse the \"-f\" option, if you really want to do this.\n");
		continue;
	    }

	    is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

	    /* don't worry about phony objects */
	    if (is_phony)
		continue;

	    _dl_eraseAllNamesFromDisplay(gedp, argv[i], 0);

	    if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
		/* Abort kill processing on first error */
		bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[i]);
		return GED_ERROR;
	    }
	}
    }

    /* Update references. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl kill_cmd_impl = {
    "kill",
    ged_kill_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd kill_cmd = { &kill_cmd_impl };
const struct ged_cmd *kill_cmds[] = { &kill_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  kill_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
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
