/*                         B O T _ F A C E _ S O R T . C
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
/** @file bot_face_sort.c
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

    if (argc < 3) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    tris_per_piece = atoi(argv[1]);
    if (tris_per_piece < 1) {
	bu_vls_printf(&gedp->ged_result_str,
		      "Illegal value for triangle per piece (%s)\n",
		      argv[1]);
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    for (i=2; i<argc; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int id;

	if ((dp=db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL) {
	    continue;
	}

	if ((id=rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, bn_mat_identity, gedp->ged_wdbp->wdb_resp)) < 0) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Failed to get internal form of %s, not sorting this one\n",
			  dp->d_namep);
	    continue;
	}

	if (id != ID_BOT) {
	    rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	    bu_vls_printf(&gedp->ged_result_str,
			  "%s is not a BOT primitive, skipped\n",
			  dp->d_namep);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);

	bu_log("processing %s (%d triangles)\n", dp->d_namep, bot->num_faces);

	if (rt_bot_sort_faces(bot, tris_per_piece)) {
	    rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	    bu_vls_printf(&gedp->ged_result_str,
			  "Face sort failed for %s, this BOT not sorted\n",
			  dp->d_namep);
	    continue;
	}

	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, gedp->ged_wdbp->wdb_resp)) {
	    bu_vls_printf(&gedp->ged_result_str,
			  "Failed to write sorted BOT (%s) to database!!!",
			  dp->d_namep);
	    rt_db_free_internal(&intern, gedp->ged_wdbp->wdb_resp);
	    return BRLCAD_ERROR;
	}
    }

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
