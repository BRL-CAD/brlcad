/*                         B O T _ F U S E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @file libged/bot_fuse.c
 *
 * The bot_fuse command.
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
ged_bot_fuse(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal intern, intern2;
    struct rt_bot_internal *bot;
    int count=0;
    static const char *usage = "new_bot old_bot";

    struct model *m;
    struct nmgregion *r;
    int ret;
    struct bn_tol *tol = &gedp->ged_wdbp->wdb_tol;
    int total = 0;

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

    bu_log("%s: start\n", argv[0]);

    GED_DB_LOOKUP(gedp, old_dp, argv[2], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, old_dp, bn_mat_identity, &rt_uniresource, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], argv[2]);
	return GED_ERROR;
    }

    /* create nmg model structure */
    m = nmg_mm();

    /* place bot in nmg structure */
    bu_log("%s: running rt_bot_tess\n", argv[0]);
    ret = rt_bot_tess(&r, m, &intern, &gedp->ged_wdbp->wdb_ttol, tol);

    /* free internal representation of original bot */
    rt_db_free_internal(&intern);

    if (ret != 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (1).\n", argv[0], argv[2]);
	nmg_km(m);
	return GED_ERROR;
    }

    total = 0;

    /* Step 1 -- the vertices. */
    bu_log("%s: running nmg_vertex_fuse\n", argv[0]);
    count = nmg_vertex_fuse(&m->magic, tol);
    total += count;
    bu_log("%s: %s, %d vertex fused\n", argv[0], argv[2], count);

    /* Step 1.5 -- break edges on vertices, before fusing edges */
    bu_log("%s: running nmg_model_break_e_on_v\n", argv[0]);
    count = nmg_model_break_e_on_v(m, tol);
    total += count;
    bu_log("%s: %s, %d broke 'e' on 'v'\n", argv[0], argv[2], count);

    if (total) {
        struct nmgregion *r2;
        struct shell *s;

	bu_log("%s: running nmg_make_faces_within_tol\n", argv[0]);

        /* vertices and/or edges have been moved,
         * may have created out-of-tolerance faces
         */

        for (BU_LIST_FOR(r2, nmgregion, &m->r_hd)) {
            for (BU_LIST_FOR(s, shell, &r2->s_hd))
                nmg_make_faces_within_tol(s, tol);
        }
    }

    /* Step 2 -- the face geometry */
    bu_log("%s: running nmg_model_face_fuse\n", argv[0]);
    count = nmg_model_face_fuse(m, tol);
    total += count;
    bu_log("%s: %s, %d faces fused\n", argv[0], argv[2], count);

    /* Step 3 -- edges */
    bu_log("%s: running nmg_edge_fuse\n", argv[0]);
    count = nmg_edge_fuse(&m->magic, tol);
    total += count;

    bu_log("%s: %s, %d edges fused\n", argv[0], argv[2], count);

    bu_log("%s: %s, %d total fused\n", argv[0], argv[2], total);

    if (!BU_SETJUMP) {
	/* try */

	/* convert the nmg model back into a bot */
	bot = nmg_bot(BU_LIST_FIRST(shell, &r->s_hd), tol);

	/* free the nmg model structure */
	nmg_km(m);
    } else {
	/* catch */
            BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (2).\n", argv[0], argv[2]);
	    return GED_ERROR;
    }

    RT_DB_INTERNAL_INIT(&intern2);
    intern2.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern2.idb_type = ID_BOT;
    intern2.idb_meth = &rt_functab[ID_BOT];
    intern2.idb_ptr = (genptr_t)bot;

    GED_DB_DIRADD(gedp, new_dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern2.idb_type, GED_ERROR);
    GED_DB_PUT_INTERNAL(gedp, new_dp, &intern2, &rt_uniresource, GED_ERROR);

    bu_vls_printf(gedp->ged_result_str, "%s: %s fuse done.\n", argv[0], argv[2]);

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

