/*                         R E G I O N . C
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
/** @file libged/region.c
 *
 * The region command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "wdb.h"

#include "./ged_private.h"


int
ged_region(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int ident, air;
    char oper;
    static const char *usage = "reg_name <op obj ...>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    ident = gedp->ged_wdbp->wdb_item_default;
    air = gedp->ged_wdbp->wdb_air_default;

    /* Check for even number of arguments */
    if (argc & 01) {
	bu_vls_printf(gedp->ged_result_str, "error in number of args!");
	return GED_ERROR;
    }

    if (db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* will attempt to create the region */
	if (gedp->ged_wdbp->wdb_item_default) {
	    gedp->ged_wdbp->wdb_item_default++;
	    bu_vls_printf(gedp->ged_result_str, "Defaulting item number to %d\n",
			  gedp->ged_wdbp->wdb_item_default);
	}
    }

    /* Get operation and solid name for each solid */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][1] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "bad operation: %s skip member: %s\n",  argv[i], argv[i+1]);
	    continue;
	}
	oper = argv[i][0];
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i+1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "skipping %s\n", argv[i+1]);
	    continue;
	}

	if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
	    bu_vls_printf(gedp->ged_result_str, "bad operation: %c skip member: %s\n",
			  oper, dp->d_namep);
	    continue;
	}

	/* Adding region to region */
	if (dp->d_flags & RT_DIR_REGION) {
	    bu_vls_printf(gedp->ged_result_str, "Note: %s is a region\n", dp->d_namep);
	}

	if (_ged_combadd(gedp, dp, (char *)argv[1], 1, oper, ident, air) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "error in combadd");
	    return GED_ERROR;
	}
    }

    if (db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET) == RT_DIR_NULL) {
	/* failed to create region */
	if (gedp->ged_wdbp->wdb_item_default > 1)
	    gedp->ged_wdbp->wdb_item_default--;
	return GED_ERROR;
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
