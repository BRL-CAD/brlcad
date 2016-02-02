/*                         G L O B . C
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
/** @file libged/glob.c
 *
 * The glob command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/vls.h"
#include "raytrace.h"
#include "ged.h"

int
ged_glob(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "expression";
    int flags = 0;
    flags |= DB_GLOB_HIDDEN;
    flags |= DB_GLOB_NON_GEOM;
    flags |= DB_GLOB_SKIP_FIRST;

    /* Silently return */
    if (gedp == GED_NULL)
	return GED_ERROR;

    /* No database to match against, so return. */
    if (gedp->ged_wdbp == RT_WDB_NULL || gedp->ged_wdbp->dbip == DBI_NULL)
	return GED_OK;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    (void)db_expand_str_glob(gedp->ged_result_str, argv[1], gedp->ged_wdbp->dbip, flags);


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
