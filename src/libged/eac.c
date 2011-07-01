/*                         E A C . C
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
/** @file libged/eac.c
 *
 * The eac command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_eac(struct ged *gedp, int argc, const char *argv[])
{
    int j;
    int item;
    struct directory *dp;
    struct bu_vls v;
    int new_argc;
    int lim;
    static const char *usage = "air_code(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_init(&v);

    bu_vls_strcat(&v, "draw");
    lim = 1;

    for (j=1; j<argc; j++) {
	item = atoi(argv[j]);
	if (item < 1)
	    continue;

	FOR_ALL_DIRECTORY_START(dp, gedp->ged_wdbp->dbip) {
	    struct rt_db_internal intern;
	    struct rt_comb_internal *comb;

	    if (!(dp->d_flags & RT_DIR_REGION))
		continue;

	    bu_vls_printf(gedp->ged_result_str, "%s: looking at %s\n", argv[0], dp->d_namep);

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "%s: Database read error, aborting\n", argv[0]);
		return GED_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    if (comb->region_id != 0
		|| comb->aircode != item)
	    {
		intern.idb_meth->ft_ifree(&intern);
		continue;
	    }
	    intern.idb_meth->ft_ifree(&intern);

	    bu_vls_strcat(&v, " ");
	    bu_vls_strcat(&v, dp->d_namep);
	    lim++;
	} FOR_ALL_DIRECTORY_END;
    }

    if (lim > 1) {
	int retval;
	char **new_argv;

	new_argv = (char **)bu_calloc(lim+1, sizeof(char *), "ged_eac: new_argv");
	new_argc = bu_argv_from_string(new_argv, lim, bu_vls_addr(&v));
	retval = ged_draw(gedp, new_argc, (const char **)new_argv);
	bu_free((genptr_t)new_argv, "ged_eac: new_argv");
	bu_vls_free(&v);
	return retval;
    }

    bu_vls_free(&v);
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
