/*                         N M G _ M A K E _ V. C
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file libged/nmg_make_v.c
 *
 * The make V subcommand for nmg top-level command.
 *
 */

#include "common.h"
#include "nmg.h"

#include <signal.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "../ged_private.h"

struct tmp_v {
    point_t pt;
    struct vertex *v;
};

int
ged_nmg_make_v_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    struct nmgregion* r;
    struct shell* s;
    struct tmp_v* verts;
    struct bn_tol tol;
    int idx;
    int num_verts;
    static const char *usage = "make V x0 y0 z0 ... xn yn zn";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    num_verts = (argc - 3) / 3;

    /* check for less than three vertices or incomplete vertex coordinates */
    if (argc < ELEMENTS_PER_POINT + 3 || (argc - 3) % 3 != 0) {
       bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
       return GED_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[0];

    dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
       bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", name);
       return GED_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, gedp->dbip,
       bn_mat_identity, &rt_uniresource) < 0) {
       bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
       return GED_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
       bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
       rt_db_free_internal(&internal);
       return GED_ERROR;
    }

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    if (BU_LIST_IS_EMPTY(&m->r_hd)) {
	r = nmg_mrsv(m);
	s = BU_LIST_FIRST(shell, &r->s_hd);
    } else {
	r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	s = BU_LIST_FIRST(shell, &r->s_hd);
    }

    NMG_CK_REGION(r);
    NMG_CK_SHELL(s);

    verts = (struct tmp_v *)NULL;
    verts = (struct tmp_v *)bu_calloc(num_verts, sizeof(struct tmp_v), "verts");

    for (idx=0; idx < num_verts; idx++){
	struct shell* ns = nmg_msv(r);
	NMG_CK_SHELL(ns);

	verts[idx].pt[0] = (fastf_t)atof(argv[idx*3+3]);
	verts[idx].pt[1] = (fastf_t)atof(argv[idx*3+4]);
	verts[idx].pt[2] = (fastf_t)atof(argv[idx*3+5]);

	nmg_vertex_gv( ns->vu_p->v_p, verts[idx].pt);
    }

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    nmg_rebound(m, &tol);

    if ( wdb_put_internal(gedp->ged_wdbp, name, &internal, 1.0) < 0 ) {
	bu_vls_printf(gedp->ged_result_str, "wdb_put_internal(%s)", argv[1]);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    rt_db_free_internal(&internal);
    bu_free(verts, "new verts");

    return GED_OK;
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
