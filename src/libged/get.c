/*                         G E T . C
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
/** @file libged/get.c
 *
 * The get command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged.h"


int
ged_get(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    struct rt_db_internal intern;
    static const char *usage = "object ?attr?";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    if (gedp->ged_wdbp->dbip == 0) {
	bu_vls_printf(gedp->ged_result_str, "dbip does not support lookup operations");
	return GED_ERROR;
    }

    if (wdb_import_from_path(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp) == GED_ERROR)
	return GED_ERROR;

    if (!intern.idb_meth->ft_get) {
	return GED_ERROR;
    }

    status = intern.idb_meth->ft_get(gedp->ged_result_str, &intern, argv[2]);
    rt_db_free_internal(&intern);

    return status;
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
