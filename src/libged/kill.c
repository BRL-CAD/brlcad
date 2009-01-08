/*                         K I L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file kill.c
 *
 * The kill command.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"
#include "cmd.h"
#include "ged_private.h"

int
ged_kill(struct ged *gedp, int argc, const char *argv[])
{
    register struct directory *dp;
    register int i;
    int	is_phony;
    int	verbose = LOOKUP_NOISY;
    int force = 0;
    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (MAXARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* skip past "-f" */
    if (argc > 1 && strcmp(argv[1], "-f") == 0) {
	verbose = LOOKUP_QUIET;
	force = 1;
	argc--;
	argv++;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], verbose)) != DIR_NULL) {
	    struct directory *dpp[2];

	    if ( !force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0 ) {
		bu_vls_printf(&gedp->ged_result_str, "You attempted to delete the _GLOBAL object.\n");
		bu_vls_printf(&gedp->ged_result_str, "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n" );
		bu_vls_printf(&gedp->ged_result_str, "\tsuch as your preferred units and the title of the database.\n" );
		bu_vls_printf(&gedp->ged_result_str, "\tUse the \"-f\" option, if you really want to do this.\n" );
		continue;
	    }

	    is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

	    /* don't worry about phony objects */
	    if (is_phony)
		continue;

	    dpp[0] = dp;
	    dpp [1] = DIR_NULL;
	    ged_eraseobjall(gedp, dpp);

	    if (db_delete(gedp->ged_wdbp->dbip, dp) < 0 ||
		db_dirdelete(gedp->ged_wdbp->dbip, dp) < 0) {
		/* Abort kill processing on first error */
		bu_vls_printf(&gedp->ged_result_str, "an error occurred while deleting %s", argv[i]);
		return BRLCAD_ERROR;
	    }
	}
    }

    return BRLCAD_OK;
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
