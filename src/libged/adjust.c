/*                         A D J U S T . C
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
/** @file adjust.c
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
    register struct directory	*dp;
    char			*name;
    struct rt_db_internal	 intern;
    static const char *usage = "object attr value ?attr value?";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);
    gedp->ged_result = GED_RESULT_NULL;
    gedp->ged_result_flags = 0;

    /* must be wanting help */
    if (argc == 1) {
	gedp->ged_result_flags |= GED_RESULT_FLAGS_HELP_BIT;
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_OK;
    }

    if (argc < 4) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = (char *)argv[1];

    /* Verify that this wdb supports lookup operations (non-null dbip) */
    RT_CK_DBI(gedp->ged_wdbp->dbip);

    dp = db_lookup(gedp->ged_wdbp->dbip, name, LOOKUP_QUIET);
    if (dp == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: not found", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "rt_db_get_internal(%s) failure", name);
	return BRLCAD_ERROR;
    }
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    RT_CK_FUNCTAB(intern.idb_meth);

    status = intern.idb_meth->ft_adjust(&gedp->ged_result_str, &intern, argc-2, (char **)argv+2, &rt_uniresource);
    if (status == BRLCAD_OK && wdb_put_internal(gedp->ged_wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "wdb_export(%s) failure", name);
	rt_db_free_internal(&intern, &rt_uniresource);
	return BRLCAD_ERROR;
    }

#if 0
    /* notify observers */
    bu_observer_notify(interp, &wdbp->wdb_observers, bu_vls_addr(&wdbp->wdb_name));
#endif

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
