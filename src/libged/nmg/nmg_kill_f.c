/*                         N M G _ K I L L _ F. C
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
/** @file libged/nmg_kill_f.c
 *
 * The kill F subcommand for nmg top-level command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "rt/geom.h"

#include "../ged_private.h"

void remove_face(const struct model* m, long int fid)
{
    struct nmgregion *r;
    struct shell *s;
    struct faceuse *fu;
    struct face *f;

    NMG_CK_MODEL(m);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
       NMG_CK_REGION(r);

       if (r->ra_p) {
	   NMG_CK_REGION_A(r->ra_p);
       }

       for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	   NMG_CK_SHELL(s);

	   if (s->sa_p) {
	       NMG_CK_SHELL_A(s->sa_p);
	   }

	   /* Faces in shell */
	   for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
	       NMG_CK_FACEUSE(fu);
	       f = fu->f_p;
	       NMG_CK_FACE(f);

	       if ( fid == f->index ) {
		   /* this kills both facesuses using the face,
		    * and the face itself.
		    */
		   nmg_kfu(fu);
	       }
	   }
       }
    }
}

int
ged_nmg_kill_f_core(struct ged* gedp, int argc, const char* argv[])
{
    struct rt_db_internal internal;
    struct directory *dp;
    struct model* m;
    const char* name;
    long int fid;

    static const char *usage = "kill F id";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 4) {
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

    if (rt_db_get_internal(&internal, dp, gedp->dbip,	bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return GED_ERROR;
    }

    if (internal.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", name);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    /* get face index from command line */
    fid = (long int)atoi(argv[3]);

    m = (struct model *)internal.idb_ptr;
    NMG_CK_MODEL(m);

    remove_face(m, fid);

    if ( wdb_put_internal(gedp->ged_wdbp, name, &internal, 1.0) < 0 ) {
	bu_vls_printf(gedp->ged_result_str, "wdb_put_internal(%s)", argv[1]);
	rt_db_free_internal(&internal);
	return GED_ERROR;
    }

    rt_db_free_internal(&internal);

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
