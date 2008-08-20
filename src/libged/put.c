/*                         P U T . C
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
/** @file put.c
 *
 * The put command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged.h"


int
ged_put(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    register const struct rt_functab *ftp;
    int i;
    char *name;
    char type[16];
    static const char *usage = "object type attrs";

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

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    name = (char *)argv[1];

    /* Verify that this wdb supports lookup operations (non-null dbip).
     * stdout/file wdb objects don't, but can still be written to.
     * If not, just skip the lookup test and write the object
     */
    if (gedp->ged_wdbp->dbip && db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s already exists", argv[1]);
	return BRLCAD_ERROR;
    }

    RT_INIT_DB_INTERNAL(&intern);

    for (i = 0; argv[2][i] != 0 && i < 16; i++) {
	type[i] = isupper(argv[2][i]) ? tolower(argv[2][i]) :
	    argv[2][i];
    }
    type[i] = 0;

    ftp = rt_get_functab_by_label(type);
    if (ftp == NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s is an unknown object type.", type);
	return BRLCAD_ERROR;
    }

    RT_CK_FUNCTAB(ftp);

    if (ftp->ft_make) {
	if (ftp->ft_make == rt_nul_make) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "wdb_put_internal(%s) cannot put a %s",
			  argv[1], type);
	    return BRLCAD_ERROR;
	}
	ftp->ft_make(ftp, &intern, 0.0);
    } else {
	rt_generic_make(ftp, &intern, 0.0);
    }

    if (ftp->ft_adjust(&gedp->ged_result_str, &intern, argc-3, (char **)argv+3, &rt_uniresource) == BRLCAD_ERROR) {
	rt_db_free_internal(&intern, &rt_uniresource);
	return BRLCAD_ERROR;
    }

    if (wdb_put_internal(gedp->ged_wdbp, name, &intern, 1.0) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "wdb_put_internal(%s)", argv[1]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return BRLCAD_ERROR;
    }

    rt_db_free_internal(&intern, &rt_uniresource);

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
