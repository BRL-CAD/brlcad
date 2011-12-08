/*                         B O T _ M E R G E . C
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
/** @file libged/bot_merge.c
 *
 * The bot_merge command.
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
ged_bot_merge(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp, *new_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal **bots;
    int i, idx;
    int avail_vert, avail_face;
    size_t face;
    static const char *usage = "bot_dest bot1_src [botn_src]";

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

    bots = bu_calloc(sizeof(struct rt_bot_internal), argc, "bot internal");

    /* create a new bot */
    BU_GET(bots[0], struct rt_bot_internal);
    bots[0]->mode = 0;
    bots[0]->orientation = RT_BOT_UNORIENTED;
    bots[0]->bot_flags = 0;
    bots[0]->num_vertices = 0;
    bots[0]->num_faces = 0;
    bots[0]->faces = (int *)0;
    bots[0]->vertices = (fastf_t *)0;
    bots[0]->thickness = (fastf_t *)0;
    bots[0]->face_mode = (struct bu_bitv*)0;
    bots[0]->num_normals = 0;
    bots[0]->normals = (fastf_t *)0;
    bots[0]->num_face_normals = 0;
    bots[0]->face_normals = 0;
    bots[0]->magic = RT_BOT_INTERNAL_MAGIC;


    /* read in all the bots */
    for (idx=1, i=2; i < argc; i++) {
	if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	GED_DB_GET_INTERNAL(gedp, &intern, dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!  Skipping.\n", argv[0], argv[i]);
	    continue;
	}

	bots[idx] = (struct rt_bot_internal *)intern.idb_ptr;

	intern.idb_ptr = (genptr_t)0;

	RT_BOT_CK_MAGIC(bots[idx]);

	bots[0]->num_vertices += bots[idx]->num_vertices;
	bots[0]->num_faces += bots[idx]->num_faces;

	idx++;
    }

    if (idx == 1) return GED_ERROR;


    for (i=1; i < idx; i++) {
	/* check for surface normals */
	if (bots[0]->mode) {
	    if (bots[0]->mode != bots[i]->mode) {
		bu_vls_printf(gedp->ged_result_str, "%s: Warning: not all bots share same mode\n", argv[0]);
	    }
	} else {
	    bots[0]->mode = bots[i]->mode;
	}


	if (bots[i]->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) bots[0]->bot_flags |= RT_BOT_HAS_SURFACE_NORMALS;
	if (bots[i]->bot_flags & RT_BOT_USE_NORMALS) bots[0]->bot_flags |= RT_BOT_USE_NORMALS;

	if (bots[0]->orientation) {
	    if (bots[i]->orientation == RT_BOT_UNORIENTED) {
		bots[0]->orientation = RT_BOT_UNORIENTED;
	    } else {
		bots[i]->magic = 1; /* set flag to reverse order of faces */
	    }
	} else {
	    bots[0]->orientation = bots[i]->orientation;
	}
    }


    bots[0]->vertices = bu_calloc(bots[0]->num_vertices*3, sizeof(fastf_t), "verts");
    bots[0]->faces = bu_calloc(bots[0]->num_faces*3, sizeof(int), "verts");

    avail_vert = 0;
    avail_face = 0;


    for (i=1; i < idx; i++) {
	/* copy the vertices */
	memcpy(&bots[0]->vertices[3*avail_vert], bots[i]->vertices, bots[i]->num_vertices*3*sizeof(fastf_t));

	/* copy/convert the faces, potentially maintaining a common orientation */
	if (bots[0]->orientation != RT_BOT_UNORIENTED && bots[i]->magic != RT_BOT_INTERNAL_MAGIC) {
	    /* copy and reverse */
	    for (face=0; face < bots[i]->num_faces; face++) {
		/* copy the 3 verts of this face and convert to new index */
		bots[0]->faces[avail_face*3+face*3+2] = bots[i]->faces[face*3  ] + avail_vert;
		bots[0]->faces[avail_face*3+face*3+1] = bots[i]->faces[face*3+1] + avail_vert;
		bots[0]->faces[avail_face*3+face*3  ] = bots[i]->faces[face*3+2] + avail_vert;
	    }
	} else {
	    /* just copy */
	    for (face=0; face < bots[i]->num_faces; face++) {
		/* copy the 3 verts of this face and convert to new index */
		bots[0]->faces[avail_face*3+face*3  ] = bots[i]->faces[face*3  ] + avail_vert;
		bots[0]->faces[avail_face*3+face*3+1] = bots[i]->faces[face*3+1] + avail_vert;
		bots[0]->faces[avail_face*3+face*3+2] = bots[i]->faces[face*3+2] + avail_vert;
	    }
	}

	/* copy surface normals */
	if (bots[0]->bot_flags == RT_BOT_HAS_SURFACE_NORMALS) {
	    bu_log("not yet copying surface normals\n");
	    if (bots[i]->bot_flags == RT_BOT_HAS_SURFACE_NORMALS) {
	    } else {
	    }
	}


	avail_vert += bots[i]->num_vertices;
	avail_face += bots[i]->num_faces;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_type = ID_BOT;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_BOT;
    intern.idb_meth = &rt_functab[ID_BOT];
    intern.idb_ptr = (genptr_t)bots[0];

    GED_DB_DIRADD(gedp, new_dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, new_dp, &intern, &rt_uniresource, GED_ERROR);

    bu_free(bots, "bots");

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
