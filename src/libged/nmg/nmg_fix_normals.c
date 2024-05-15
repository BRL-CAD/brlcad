/*                  N M G _ F I X _ N O R M A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2024 United States Government as represented by
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

/** @file libged/nmg_fix_normals.c
 *
 * The nmg_fix_normals command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "../ged_private.h"


int
ged_nmg_fix_normals_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal nmg_intern;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    const char *nmg_name;
    struct bn_tol tol;

    static const char *usage = "nmg_prim";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* in theory, we should probably allow the user to override this. */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.01;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 0.001;
    tol.para = 0.999;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* attempt to resolve and verify before we jump in */
    nmg_name = argv[1];

    if ((dp=db_lookup(gedp->dbip, nmg_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", nmg_name);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&nmg_intern, dp, gedp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return BRLCAD_ERROR;
    }

    if (nmg_intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", nmg_name);
	rt_db_free_internal(&nmg_intern);
	return BRLCAD_ERROR;
    }

    m = (struct model *)nmg_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* hum, here we go */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
	for (BU_LIST_FOR(s, shell, &r->s_hd))
	    nmg_fix_normals(s, &RTG.rtg_vlfree, &tol);

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
