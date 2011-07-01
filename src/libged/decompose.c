/*                         D E C O M P O S E . C
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
/** @file libged/decompose.c
 *
 * The decompose command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"

int
ged_decompose(struct ged *gedp, int argc, const char *argv[])
{
    int count;
    struct bu_vls solid_name;
    char *nmg_solid_name;
    char *prefix;
    char *def_prefix="sh";
    struct model *m;
    struct nmgregion *r;
    struct model *new_m;
    struct nmgregion *tmp_r;
    struct shell *kill_s;
    struct directory *dp;
    struct rt_db_internal nmg_intern;
    static const char *usage = "nmg [prefix]";

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

    if (argc < 2 || 3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    count = 0;
    nmg_solid_name = (char *)argv[1];

    if (argc > 2) {
	prefix = (char *)argv[2];
	if (db_version(gedp->ged_wdbp->dbip) < 5 && strlen(prefix) > NAMESIZE) {
	    bu_vls_printf(gedp->ged_result_str, "%s: Prefix %s is too long", argv[0], prefix);
	    return GED_ERROR;
	}
    } else {
	prefix = def_prefix;
    }

    if ((dp=db_lookup(gedp->ged_wdbp->dbip, nmg_solid_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (rt_db_get_internal(&nmg_intern, dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: rt_db_get_internal(%s) error\n", argv[0], nmg_solid_name);
	return GED_ERROR;
    }

    if (nmg_intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not an NMG solid!", argv[0], nmg_solid_name);
	return GED_ERROR;
    }

    bu_vls_init(&solid_name);

    m = (struct model *)nmg_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* create temp region to hold duplicate shell */
    tmp_r = nmg_mrsv(m);	/* temp nmgregion to hold dup shells */
    kill_s = BU_LIST_FIRST(shell, &tmp_r->s_hd);
    (void)nmg_ks(kill_s);

    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	struct shell *s;

	if (r == tmp_r)
	    continue;

	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    struct shell *tmp_s;
	    struct shell *decomp_s;
	    long **trans_tbl;

	    /* duplicate shell */
	    tmp_s = (struct shell *)nmg_dup_shell(s, &trans_tbl, &gedp->ged_wdbp->wdb_tol);
	    bu_free((char *)trans_tbl, "trans_tbl");

	    /* move duplicate to temp region */
	    (void) nmg_mv_shell_to_region(tmp_s, tmp_r);

	    /* decompose this shell */
	    (void) nmg_decompose_shell(tmp_s, &gedp->ged_wdbp->wdb_tol);

	    /* move each decomposed shell to yet another region */
	    decomp_s = BU_LIST_FIRST(shell, &tmp_r->s_hd);
	    while (BU_LIST_NOT_HEAD(&decomp_s->l, &tmp_r->s_hd)) {
		struct shell *next_s;
		struct shell *new_s;
		struct rt_db_internal new_intern;
		struct directory *new_dp;
		struct nmgregion *decomp_r;
		char shell_no[32];
		size_t end_prefix;

		next_s = BU_LIST_NEXT(shell, &decomp_s->l);

		decomp_r = nmg_mrsv(m);
		kill_s = BU_LIST_FIRST(shell, &decomp_r->s_hd);
		(void)nmg_ks(kill_s);
		nmg_shell_a(decomp_s, &gedp->ged_wdbp->wdb_tol);
		new_s = (struct shell *)nmg_dup_shell(decomp_s, &trans_tbl, &gedp->ged_wdbp->wdb_tol);
		(void)nmg_mv_shell_to_region(new_s, decomp_r);

		/* move this region to a different model */
		new_m = (struct model *)nmg_mk_model_from_region(decomp_r, 1);
		(void)nmg_rebound(new_m, &gedp->ged_wdbp->wdb_tol);

		/* create name for this shell */
		count++;
		bu_vls_strcpy(&solid_name, prefix);
		sprintf(shell_no, "_%d", count);
		if (db_version(gedp->ged_wdbp->dbip) < 5) {
		    end_prefix = strlen(prefix);
		    if (end_prefix + strlen(shell_no) > NAMESIZE)
			end_prefix = NAMESIZE - strlen(shell_no);
		    bu_vls_trunc(&solid_name, (int)end_prefix);
		    bu_vls_strncat(&solid_name, shell_no, NAMESIZE-bu_vls_strlen(&solid_name));
		} else {
		    bu_vls_strcat(&solid_name, shell_no);
		}

		if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&solid_name), LOOKUP_QUIET) != RT_DIR_NULL) {
		    bu_vls_printf(gedp->ged_result_str, "%s: cannot create unique solid name (%s)",
				  argv[0], bu_vls_addr(&solid_name));
		    return GED_ERROR;
		}

		/* write this model as a seperate nmg solid */
		RT_DB_INTERNAL_INIT(&new_intern);
		new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		new_intern.idb_type = ID_NMG;
		new_intern.idb_meth = &rt_functab[ID_NMG];
		new_intern.idb_ptr = (genptr_t)new_m;

		new_dp=db_diradd(gedp->ged_wdbp->dbip, bu_vls_addr(&solid_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&new_intern.idb_type);
		if (new_dp == RT_DIR_NULL) {
		    bu_vls_free(&solid_name);
		    bu_vls_printf(gedp->ged_result_str, "%s: Database alloc error, aborting", argv[0]);
		    return GED_ERROR;;
		}

		if (rt_db_put_internal(new_dp, gedp->ged_wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
		    (void)nmg_km(new_m);
		    bu_vls_printf(gedp->ged_result_str, "%s: rt_db_put_internal(%s) failure\n",
				  argv[0], bu_vls_addr(&solid_name));
		    bu_vls_free(&solid_name);
		    return GED_ERROR;
		}

		(void)nmg_ks(decomp_s);
		decomp_s = next_s;
	    }
	}
    }

    rt_db_free_internal(&nmg_intern);
    bu_vls_free(&solid_name);

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
