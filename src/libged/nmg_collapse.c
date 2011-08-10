/*                         N M G _ C O L L A P S E . C
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
/** @file libged/nmg_collapse.c
 *
 * The nmg_collapse command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_nmg_collapse(struct ged *gedp, int argc, const char *argv[])
{
    char *new_name;
    struct model *m;
    struct rt_db_internal intern;
    struct directory *dp;
    struct bu_ptbl faces;
    struct face *fp;
    size_t count;
    fastf_t tol_coll;
    fastf_t min_angle;
    static const char *usage = "nmg_prim new_prim max_err_dist [min_angle]";

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

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (strchr(argv[2], '/')) {
	bu_vls_printf(gedp->ged_result_str, "Do not use '/' in solid names: %s\n", argv[2]);
	return GED_ERROR;
    }

    new_name = (char *)argv[2];

    if (db_lookup(gedp->ged_wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists\n", new_name);
	return GED_ERROR;
    }

    if ((dp=db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (dp->d_flags & RT_DIR_COMB) {
	bu_vls_printf(gedp->ged_result_str, "%s is a combination, only NMG primitives are allowed here\n", argv[1]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Failed to get internal form of %s!!!!\n", argv[1]);
	return GED_ERROR;
    }

    if (intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid!!!!\n", argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    tol_coll = atof(argv[3]) * gedp->ged_wdbp->dbip->dbi_local2base;
    if (tol_coll <= 0.0) {
	bu_vls_printf(gedp->ged_result_str, "tolerance distance too small\n");
	return GED_ERROR;
    }

    if (argc == 5) {
	min_angle = atof(argv[4]);
	if (min_angle < 0.0) {
	    bu_vls_printf(gedp->ged_result_str, "Minimum angle cannot be less than zero\n");
	    return GED_ERROR;
	}
    } else
	min_angle = 0.0;

    m = (struct model *)intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* check that all faces are planar */
    nmg_face_tabulate(&faces, &m->magic);
    for (BU_PTBL_FOR(fp, (struct face *), &faces)) {
	if (fp->g.magic_p != NULL && *(fp->g.magic_p) != NMG_FACE_G_PLANE_MAGIC) {
	    bu_log("\tnot planar\n");
	    bu_ptbl_free(&faces);
	    bu_vls_printf(gedp->ged_result_str,
			  "nmg_collapse can only be applied to NMG primitives with planar faces\n");
	    return GED_ERROR;
	}
    }
    bu_ptbl_free(&faces);

    /* triangulate model */
    nmg_triangulate_model(m, &gedp->ged_wdbp->wdb_tol);

    count = (size_t)nmg_edge_collapse(m, &gedp->ged_wdbp->wdb_tol, tol_coll, min_angle);

    dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", new_name);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting.\n");
	return GED_ERROR;
    }

    rt_db_free_internal(&intern);

    bu_vls_printf(gedp->ged_result_str, "%zu edges collapsed\n", count);

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
