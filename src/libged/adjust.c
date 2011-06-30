/*                         A D J U S T . C
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
/** @file libged/adjust.c
 *
 * The adjust command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged.h"


int
ged_adjust(struct ged *gedp, int argc, const char *argv[])
{
    int status;
    struct directory *dp;
    char *name;
    struct rt_db_internal intern;
    static const char *usage = "object attr value ?attr value?";

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

    name = (char *)argv[1];

    /* Verify that this wdb supports lookup operations (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    GED_DB_LOOKUP(gedp, dp, name, LOOKUP_QUIET, GED_ERROR);

    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    if (!intern.idb_meth->ft_adjust) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) adjust failure", name);
	return GED_ERROR;
    }

    status = intern.idb_meth->ft_adjust(gedp->ged_result_str, &intern, argc-2, argv+2);
    if (status == GED_OK && wdb_put_internal(gedp->ged_wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", name);
	rt_db_free_internal(&intern);
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
