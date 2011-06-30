/*                   B O T _ F A C E _ S O R T . C
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
/** @file libged/bot_face_sort.c
 *
 * The bot_face_sort command.
 *
 */

#include "common.h"
#include "bio.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rtgeom.h"
#include "ged.h"


int
ged_bot_face_sort(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int tris_per_piece=0;
    static const char *usage = "triangles_per_piece bot_solid1 [bot_solid2 bot_solid3 ...]";

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

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    tris_per_piece = atoi(argv[1]);
    if (tris_per_piece < 1) {
	bu_vls_printf(gedp->ged_result_str,
		      "Illegal value for triangle per piece (%s)\n",
		      argv[1]);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    for (i=2; i<argc; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;

	if ((dp=db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, gedp->ged_wdbp->wdb_resp, GED_ERROR);

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(gedp->ged_result_str,
			  "%s is not a BOT primitive, skipped\n",
			  dp->d_namep);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);

	bu_log("processing %s (%zu triangles)\n", dp->d_namep, bot->num_faces);

	if (rt_bot_sort_faces(bot, tris_per_piece)) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(gedp->ged_result_str,
			  "Face sort failed for %s, this BOT not sorted\n",
			  dp->d_namep);
	    continue;
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, gedp->ged_wdbp->wdb_resp, GED_ERROR);
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
