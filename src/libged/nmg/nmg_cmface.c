/*                         N M G _ C M F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
/** @file libged/nmg_cmface.c
 *
 * The cmface command.
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
ged_nmg_cmface_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    struct nmgregion* r;
    struct shell* s;
    struct tmp_v* verts;
    struct faceuse *fu;
    struct bn_tol tol;
    struct vertex ***face_verts;
    int idx, num_verts;

    static const char *usage = "nmg_name cmface x0 y0 z0 ... xn yn zn";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    num_verts = (argc - 2) / 3;

    /* check for less than three vertices or incomplete vertex coordinates */
    if (argc < ELEMENTS_PER_POINT * 3 + 2 || (argc - 2) % 3 != 0) {
       bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
       return BRLCAD_HELP;
    }

    /* attempt to resolve and verify */
    name = argv[0];

    dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
       bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", name);
       return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&internal, dp, gedp->dbip,
       bn_mat_identity, &rt_uniresource) < 0) {
       bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
       return BRLCAD_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
       bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
       rt_db_free_internal(&internal);
       return BRLCAD_ERROR;
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
    verts = (struct tmp_v *)bu_calloc(num_verts,
				      sizeof(struct tmp_v), "verts");
    face_verts = (struct vertex ***) bu_calloc( num_verts,
						sizeof(struct vertex **), "face_verts");

    for (idx=0; idx < num_verts; idx++){
	verts[idx].pt[0] = (fastf_t)atof(argv[idx*3+2]);
	verts[idx].pt[1] = (fastf_t)atof(argv[idx*3+3]);
	verts[idx].pt[2] = (fastf_t)atof(argv[idx*3+4]);
	face_verts[idx] = &verts[idx].v;
    }

    nmg_cmface( s, face_verts, num_verts );
    bu_free((char *) face_verts, "face_verts");

    /* assign geometry for entire vertex list (if we have one) */
    for (idx=0; idx < num_verts; idx++) {
	if (verts[idx].v) {
	    nmg_vertex_gv(verts[idx].v, verts[idx].pt);
	}
    }

    /* assign face geometry */
    if (s) {
	for (BU_LIST_FOR (fu, faceuse, &s->fu_hd)) {
	    if (fu->orientation != OT_SAME) {
		continue;
	    }
	    nmg_calc_face_g(fu, &RTG.rtg_vlfree);
	}
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
	return BRLCAD_ERROR;
    }

    rt_db_free_internal(&internal);

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
