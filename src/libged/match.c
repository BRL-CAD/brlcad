/*                         M A T C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/match.c
 *
 * The match command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "./ged_private.h"


int
ged_match(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "expression";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (++argv; *argv != NULL; ++argv) {
	register int i, num;
	register struct directory *dp;
	for (i = num = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (bu_fnmatch(*argv, dp->d_namep, 0) != 0)
		    continue;
		if (num == 0)
		    bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
		else {
		    bu_vls_strcat(gedp->ged_result_str, " ");
		    bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
		}
		++num;
	    }
	}

	if (num > 0)
	    bu_vls_strcat(gedp->ged_result_str, " ");
    }
    bu_vls_trimspace(gedp->ged_result_str);

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
