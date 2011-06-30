/*                         K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_kill(struct ged *gedp, int argc, const char *argv[])
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
    while ((c = bu_getopt(argc, (char * const *)argv, "fn")) != -1) {
	switch(c) {
	    case 'f':
		force = 1;
		break;
	    case 'n':
		nflag = 1;
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
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], verbose)) != RT_DIR_NULL) {
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

	    _ged_eraseAllNamesFromDisplay(gedp, argv[i], 0);

	    if (db_delete(gedp->ged_wdbp->dbip, dp) != 0 || db_dirdelete(gedp->ged_wdbp->dbip, dp) != 0) {
		/* Abort kill processing on first error */
		bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", argv[i]);
		return GED_ERROR;
	    }
	}
    }

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
