/*                         M O V E . C
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
/** @file libged/move.c
 *
 * The move command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_move(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_display_list *gdlp;
    struct directory *dp;
    struct rt_db_internal intern;
    static const char *usage = "from to";

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

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (db_lookup(gedp->ged_wdbp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists", argv[2]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    /* Change object name in the in-memory directory. */
    if (db_rename(gedp->ged_wdbp->dbip, dp, argv[2]) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "error in db_rename to %s, aborting", argv[2]);
	return GED_ERROR;
    }

    /* Re-write to the database.  New name is applied on the way out. */
    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return GED_ERROR;
    }

    /* Change object name if it matches the first element in the display list path. */
    for (BU_LIST_FOR(gdlp, ged_display_list, &gedp->ged_gdp->gd_headDisplay)) {
	int first = 1;
	int found = 0;
	struct bu_vls new_path;
	char *dupstr = strdup(bu_vls_addr(&gdlp->gdl_path));
	char *tok = strtok(dupstr, "/");

	bu_vls_init(&new_path);

	while (tok) {
	    if (first) {
		first = 0;

		if (BU_STR_EQUAL(tok, argv[1])) {
		    found = 1;
		    bu_vls_printf(&new_path, "%s", argv[2]);
		} else
		    break; /* no need to go further */
	    } else
		bu_vls_printf(&new_path, "/%s", tok);

	    tok = strtok((char *)NULL, "/");
	}

	if (found) {
	    bu_vls_free(&gdlp->gdl_path);
	    bu_vls_printf(&gdlp->gdl_path, "%V", &new_path);
	}

	free((void *)dupstr);
	bu_vls_free(&new_path);
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
