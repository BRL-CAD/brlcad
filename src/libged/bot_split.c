/*                         B O T _ S P L I T . C
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
/** @file bot_split.c
 *
 * The bot_split command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "rtgeom.h"	/* for rt_bot_split (in raytrace.h) */
#include "./ged_private.h"


int
ged_bot_split(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    static const char *usage = "bot [bot2 bot3 ...]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    for (i=1; i < argc; ++i) {
	struct rt_bot_list *headRblp;

	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET)) == DIR_NULL) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: db_lookup(%s) error\n", argv[0], argv[i]);
	    continue;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(&gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], argv[i]);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	headRblp = rt_bot_split(bot);

	{
	    int ac = 2;
	    const char *av[3];
	    struct rt_db_internal bot_intern;
	    struct rt_bot_list *rblp;

	    av[0] = "make_name";
	    av[2] = (char *)0;

	    for (BU_LIST_FOR(rblp, rt_bot_list, &headRblp->l)) {
		/* Get a unique name based on the original name */
		av[1] = argv[i];
		ged_make_name(gedp, ac, av);

		/* Create the bot */
		RT_INIT_DB_INTERNAL(&bot_intern);
		bot_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		bot_intern.idb_type = ID_BOT;
		bot_intern.idb_meth = &rt_functab[ID_BOT];
		bot_intern.idb_ptr = (genptr_t)rblp->bot;

		dp = db_diradd(gedp->ged_wdbp->dbip, bu_vls_addr(&gedp->ged_result_str), RT_DIR_PHONY_ADDR, 0, DIR_SOLID, (genptr_t)&bot_intern.idb_type);
		if (dp == DIR_NULL) {
		    bu_vls_printf(&gedp->ged_result_str, " failed to be added to the database.");
		    rt_bot_list_free(headRblp, 0);
		    rt_db_free_internal(&intern);
		}

		if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &bot_intern, &rt_uniresource) < 0) {
		    bu_vls_printf(&gedp->ged_result_str, " failed to be added to the database.");
		    rt_bot_list_free(headRblp, 0);
		    rt_db_free_internal(&intern);
		}
	    }

	    bu_vls_trunc(&gedp->ged_result_str, 0);
	}

	rt_bot_list_free(headRblp, 0);
	rt_db_free_internal(&intern);
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
