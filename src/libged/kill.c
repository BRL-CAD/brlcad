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
    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* invalid command name */
    if (argc < 1) {
	bu_vls_printf(&gedp->ged_result_str, "Error: command name not provided");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (MAXARGS < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* skip past "-f" */
    if (argc > 1 && strcmp(argv[1], "-f") == 0) {
	verbose = LOOKUP_QUIET;
	argc--;
	argv++;
    }

    for (i = 1; i < argc; i++) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], verbose)) != DIR_NULL) {
	    struct directory *dpp[2];

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
