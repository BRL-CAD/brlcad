/*                         B O T _ S P L I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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

#include "rtgeom.h"

#include "./ged_private.h"


int
ged_bot_split(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal **bots;
    struct rt_bot_internal *bot;
    int bot_count = 256;
    int edge, e, f;
    int * edges;
    int face;
    static const char *usage = "bot_obj";

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

    if (argc != 2) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bots = bu_calloc(sizeof(struct rt_bot_internal), bot_count, "bot internal");

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(&gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    edges = bu_calloc(bot->num_faces, 3, "num_edges");


    for (face=0; face < bot->num_faces; face++) {
	int *faceptr = &bot->faces[face*3];

	for (edge=0; edge < 3; edge++) {

	    for (f=face+1; f < bot->num_faces; f++) {
		int *fptr = &bot->faces[f*3];

		for (e=0; e < 3; e++) {
		    /* does e match edge? */

		    if ( (fptr[e] == faceptr[edge] && fptr[ (e+1) % 3 ] == faceptr[ (edge+1) % 3 ]) ||
			 (fptr[e] == faceptr[ (edge+1) % 3 ] && fptr[ (e+1) % 3 ] == faceptr[edge]) ) {
			/* edge match */
			edges[face*3+edge]++;
			edges[face*3+edge]++;
		    }


		}
	    }
	}
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
