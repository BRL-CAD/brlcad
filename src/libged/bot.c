/*                         B O T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2016 United States Government as represented by
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
/** @file libged/bot.c
 *
 * Bot command for simple bot primitive operations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"
#include "./ged_private.h"


int
ged_bot(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *bot_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    const char *cmd = argv[0];
    const char *sub = NULL;
    const char *arg = NULL;
    const char *primitive = NULL;
    size_t len;
    fastf_t tmp;
    fastf_t propVal;
    static const char *usage = "get (faces|minEdge|maxEdge|orientation|type|vertices) bot\nchull bot_in bot_out\n";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    /* determine subcommand */
    sub = argv[1];
    len = strlen(sub);
    if (bu_strncmp(sub, "get", len) == 0) {
	primitive = argv[argc - 1];
    }
    if (bu_strncmp(sub, "chull", len) == 0) {
	primitive = argv[2];
    }
    if (bu_strncmp(sub, "solid", len) == 0) {
	primitive = argv[2];
    }
    if (bu_strncmp(sub, "solid_vis", len) == 0) {
	primitive = argv[2];
    }
    if (primitive == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a known subcommand!", cmd, sub);
	return GED_ERROR;
    }

    /* get bot */
    GED_DB_LOOKUP(gedp, bot_dp, primitive, LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!", cmd, primitive);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    if (bu_strncmp(sub, "get", len) == 0) {

	arg = argv[2];
	propVal = rt_bot_propget(bot, arg);

	/* print result string */
	if (!EQUAL(propVal, -1.0)) {

	    tmp = (int) propVal;

	    /* int result */
	    if (EQUAL(propVal, tmp)) {
		bu_vls_printf(gedp->ged_result_str, "%d", (int) propVal);
	    }

	    /* float result */
	    else {
		bu_vls_printf(gedp->ged_result_str, "%f", propVal);
	    }
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a valid argument!", sub, arg);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }
    if (bu_strncmp(sub, "chull", len) == 0) {
	int retval = 0;
	int fc = 0;
	int vc = 0;
	point_t *vert_array;
	int *faces;
	unsigned char err = 0;

	/* must be wanting help */
	if (argc < 4) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)bot->vertices, (int)bot->num_vertices);

	if (retval != 3) {
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	retval = mk_bot(gedp->ged_wdbp, argv[3], RT_BOT_SOLID, RT_BOT_CCW, err, vc, fc, (fastf_t *)vert_array, faces, NULL, NULL);

	if (retval) {
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }
    if (bu_strncmp(sub, "solid", len) == 0) {
	int is_solid = 0;

	/* must be wanting help */
	if (argc != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	is_solid = !bg_trimesh_solid(bot->num_vertices, bot->num_faces, bot->vertices, bot->faces, NULL);
	bu_vls_printf(gedp->ged_result_str, "%d", is_solid);

	rt_db_free_internal(&intern);

	return GED_OK;
    }

    if (bu_strncmp(sub, "solid_vis", len) == 0) {
	int not_solid = 0;
	int *edges = NULL;

	/* must be wanting help */
	if (argc != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}

	not_solid = bg_trimesh_solid(bot->num_vertices, bot->num_faces, bot->vertices, bot->faces, &edges);
	if (!not_solid) {
	    bu_vls_printf(gedp->ged_result_str, "1");
	} else {
	    bu_vls_printf(gedp->ged_result_str, "0");
	}

	if (not_solid && edges) {
	    int curr_edge = 0;
	    struct bu_list *vhead;
	    point_t a, b;
	    struct bn_vlblock *vbp;
	    struct bu_list local_vlist;
	    BU_LIST_INIT(&local_vlist);
	    vbp = bn_vlblock_init(&local_vlist, 32);

	    /* Clear any previous visual */
	    if (db_lookup(gedp->ged_wdbp->dbip, "badedge_visualff", LOOKUP_QUIET) != RT_DIR_NULL)
		dl_erasePathFromDisplay(gedp->ged_gdp->gd_headDisplay, gedp->ged_wdbp->dbip, gedp->ged_free_vlist_callback, "badedge_visualff", 1, gedp->freesolid);

	    for (curr_edge = 0; curr_edge < not_solid; curr_edge++) {
		int p1 = edges[curr_edge*2];
		int p2 = edges[curr_edge*2+1];
		VSET(a, bot->vertices[p1*3], bot->vertices[p1*3+1], bot->vertices[p1*3+2]);
		VSET(b, bot->vertices[p2*3], bot->vertices[p2*3+1], bot->vertices[p2*3+2]);
		vhead = bn_vlblock_find(vbp, 0, 0, 255); /* should be blue */
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BN_VLIST_LINE_DRAW);
	    }

	    _ged_cvt_vlblock_to_solids(gedp, vbp, "badedge_visualff", 0);
	    bn_vlist_cleanup(&local_vlist);
	    bn_vlblock_free(vbp);
	}
	bu_free(edges, "free edge info array");
	rt_db_free_internal(&intern);

	return GED_OK;
    }

    rt_db_free_internal(&intern);
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
