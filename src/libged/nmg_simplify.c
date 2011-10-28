/*                         N M G _ S I M P L I F Y . C
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
/** @file libged/nmg_simplify.c
 *
 * The nmg_simplify command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"

#include "./ged_private.h"


int
ged_nmg_simplify(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal nmg_intern;
    struct rt_db_internal new_intern;
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct bu_ptbl faces;
    struct face *fp;
    int do_all=1;
    int do_arb=0;
    int do_tgc=0;
    int do_poly=0;
    char *new_name;
    char *nmg_name;
    int success = 0;
    int shell_count=0;

    static const char *usage = "[arb|tgc|ell|poly] new_prim nmg_prim";

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

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    RT_DB_INTERNAL_INIT(&new_intern);

    if (argc == 4) {
	do_all = 0;
	if (!strncmp(argv[1], "arb", 3))
	    do_arb = 1;
	else if (!strncmp(argv[1], "tgc", 3))
	    do_tgc = 1;
	else if (!strncmp(argv[1], "poly", 4))
	    do_poly = 1;
	else {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

	new_name = (char *)argv[2];
	nmg_name = (char *)argv[3];
    } else {
	new_name = (char *)argv[1];
	nmg_name = (char *)argv[2];
    }

    if (db_lookup(gedp->ged_wdbp->dbip, new_name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s already exists\n", new_name);
	return GED_ERROR;
    }

    if ((dp=db_lookup(gedp->ged_wdbp->dbip, nmg_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s does not exist\n", nmg_name);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&nmg_intern, dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return GED_ERROR;
    }

    if (nmg_intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an NMG solid\n", nmg_name);
	rt_db_free_internal(&nmg_intern);
	return GED_ERROR;
    }

    m = (struct model *)nmg_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* check that all faces are planar */
    nmg_face_tabulate(&faces, &m->magic);
    for (BU_PTBL_FOR(fp, (struct face *), &faces)) {
	if (fp->g.magic_p != NULL && *(fp->g.magic_p) != NMG_FACE_G_PLANE_MAGIC) {
	    bu_ptbl_free(&faces);
	    bu_vls_printf(gedp->ged_result_str,
			  "%s cannot be applied to \"%s\" because it has non-planar faces\n",
			  argv[0], nmg_name);
	    return GED_ERROR;
	}
    }
    bu_ptbl_free(&faces);

    /* count shells */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd))
	    shell_count++;
    }

    if ((do_arb || do_all) && shell_count == 1) {
	struct rt_arb_internal *arb_int;

	BU_GETSTRUCT(arb_int, rt_arb_internal);

	if (nmg_to_arb(m, arb_int)) {
	    new_intern.idb_ptr = (genptr_t)(arb_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_ARB8;
	    new_intern.idb_meth = &rt_functab[ID_ARB8];
	    success = 1;
	} else if (do_arb) {
	    /* see if we can get an arb by simplifying the NMG */

	    r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	    s = BU_LIST_FIRST(shell, &r->s_hd);
	    nmg_shell_coplanar_face_merge(s, &gedp->ged_wdbp->wdb_tol, 1);
	    if (!nmg_kill_cracks(s)) {
		(void) nmg_model_edge_fuse(m, &gedp->ged_wdbp->wdb_tol);
		(void) nmg_edge_g_fuse(&m->magic, &gedp->ged_wdbp->wdb_tol);
		(void) nmg_unbreak_region_edges(&r->l.magic);
		if (nmg_to_arb(m, arb_int)) {
		    new_intern.idb_ptr = (genptr_t)(arb_int);
		    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		    new_intern.idb_type = ID_ARB8;
		    new_intern.idb_meth = &rt_functab[ID_ARB8];
		    success = 1;
		}
	    }
	    if (!success) {
		rt_db_free_internal(&nmg_intern);
		bu_vls_printf(gedp->ged_result_str, "Failed to construct an ARB equivalent to %s\n", nmg_name);
		return GED_OK;
	    }
	}
    }

    if ((do_tgc || do_all) && !success && shell_count == 1) {
	struct rt_tgc_internal *tgc_int;

	BU_GETSTRUCT(tgc_int, rt_tgc_internal);

	if (nmg_to_tgc(m, tgc_int, &gedp->ged_wdbp->wdb_tol)) {
	    new_intern.idb_ptr = (genptr_t)(tgc_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_TGC;
	    new_intern.idb_meth = &rt_functab[ID_TGC];
	    success = 1;
	} else if (do_tgc) {
	    rt_db_free_internal(&nmg_intern);
	    bu_vls_printf(gedp->ged_result_str, "Failed to construct an TGC equivalent to %s\n", nmg_name);
	    return GED_OK;
	}
    }

    /* see if we can get an arb by simplifying the NMG */
    if ((do_arb || do_all) && !success && shell_count == 1) {
	struct rt_arb_internal *arb_int;

	BU_GETSTRUCT(arb_int, rt_arb_internal);

	r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	s = BU_LIST_FIRST(shell, &r->s_hd);
	nmg_shell_coplanar_face_merge(s, &gedp->ged_wdbp->wdb_tol, 1);
	if (!nmg_kill_cracks(s)) {
	    (void) nmg_model_edge_fuse(m, &gedp->ged_wdbp->wdb_tol);
	    (void) nmg_edge_g_fuse(&m->magic, &gedp->ged_wdbp->wdb_tol);
	    (void) nmg_unbreak_region_edges(&r->l.magic);
	    if (nmg_to_arb(m, arb_int)) {
		new_intern.idb_ptr = (genptr_t)(arb_int);
		new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		new_intern.idb_type = ID_ARB8;
		new_intern.idb_meth = &rt_functab[ID_ARB8];
		success = 1;
	    } else if (do_arb) {
		rt_db_free_internal(&nmg_intern);
		bu_vls_printf(gedp->ged_result_str, "Failed to construct an ARB equivalent to %s\n", nmg_name);
		return GED_OK;
	    }
	}
    }

    if ((do_poly || do_all) && !success) {
	struct rt_pg_internal *poly_int;

	poly_int = (struct rt_pg_internal *)bu_malloc(sizeof(struct rt_pg_internal), "f_nmg_simplify: poly_int");

	if (nmg_to_poly(m, poly_int, &gedp->ged_wdbp->wdb_tol)) {
	    new_intern.idb_ptr = (genptr_t)(poly_int);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_POLY;
	    new_intern.idb_meth = &rt_functab[ID_POLY];
	    success = 1;
	} else if (do_poly) {
	    rt_db_free_internal(&nmg_intern);
	    bu_vls_printf(gedp->ged_result_str, "%s is not a closed surface, cannot make a polysolid\n", nmg_name);
	    return GED_OK;
	}
    }

    if (success) {
	r = BU_LIST_FIRST(nmgregion, &m->r_hd);
	s = BU_LIST_FIRST(shell, &r->s_hd);

	if (BU_LIST_NON_EMPTY(&s->lu_hd))
	    bu_vls_printf(gedp->ged_result_str, "wire loops in %s have been ignored in conversion\n",  nmg_name);

	if (BU_LIST_NON_EMPTY(&s->eu_hd))
	    bu_vls_printf(gedp->ged_result_str, "wire edges in %s have been ignored in conversion\n",  nmg_name);

	if (s->vu_p)
	    bu_vls_printf(gedp->ged_result_str, "Single vertexuse in shell of %s has been ignored in conversion\n", nmg_name);

	rt_db_free_internal(&nmg_intern);

	dp=db_diradd(gedp->ged_wdbp->dbip, new_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&new_intern.idb_type);
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", new_name);
	    return GED_ERROR;
	}

	if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
	    rt_db_free_internal(&new_intern);
	    bu_vls_printf(gedp->ged_result_str, "Database write error, aborting.\n");
	    return GED_ERROR;
	}
	return GED_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "simplification to %s is not yet supported\n", argv[1]);
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
