/*                         S H E L L S . C
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
/** @file libged/shells.c
 *
 * The shells command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_shells(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal old_intern, new_intern;
    struct model *m_tmp, *m;
    struct nmgregion *r_tmp, *r;
    struct shell *s_tmp, *s;
    int shell_count=0;
    struct bu_vls shell_name;
    long **trans_tbl;
    static const char *usage = "nmg_model";

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

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((old_dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (rt_db_get_internal(&old_intern, old_dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return GED_ERROR;
    }

    if (old_intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, "Object is not an NMG!!!\n");
	return GED_ERROR;
    }

    m = (struct model *)old_intern.idb_ptr;
    NMG_CK_MODEL(m);

    bu_vls_init(&shell_name);
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    s_tmp = nmg_dup_shell(s, &trans_tbl, &gedp->ged_wdbp->wdb_tol);
	    bu_free((genptr_t)trans_tbl, "trans_tbl");

	    m_tmp = nmg_mmr();
	    r_tmp = BU_LIST_FIRST(nmgregion, &m_tmp->r_hd);

	    BU_LIST_DEQUEUE(&s_tmp->l);
	    BU_LIST_APPEND(&r_tmp->s_hd, &s_tmp->l);
	    s_tmp->r_p = r_tmp;
	    nmg_m_reindex(m_tmp, 0);
	    nmg_m_reindex(m, 0);

	    bu_vls_printf(&shell_name, "shell.%d", shell_count);
	    while (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&shell_name), 0) != RT_DIR_NULL) {
		bu_vls_trunc(&shell_name, 0);
		shell_count++;
		bu_vls_printf(&shell_name, "shell.%d", shell_count);
	    }

	    /* Export NMG as a new solid */
	    RT_DB_INTERNAL_INIT(&new_intern);
	    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	    new_intern.idb_type = ID_NMG;
	    new_intern.idb_meth = &rt_functab[ID_NMG];
	    new_intern.idb_ptr = (genptr_t)m_tmp;

	    new_dp=db_diradd(gedp->ged_wdbp->dbip, bu_vls_addr(&shell_name), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&new_intern.idb_type);
	    if (new_dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "An error has occured while adding a new object to the database.\n");
		return GED_ERROR;
	    }

	    /* make sure the geometry/bounding boxes are up to date */
	    nmg_rebound(m_tmp, &gedp->ged_wdbp->wdb_tol);

	    if (rt_db_put_internal(new_dp, gedp->ged_wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
		/* Free memory */
		nmg_km(m_tmp);
		bu_vls_printf(gedp->ged_result_str, "rt_db_put_internal() failure\n");
		return GED_ERROR;
	    }
	    /* Internal representation has been freed by rt_db_put_internal */
	    new_intern.idb_ptr = (genptr_t)NULL;
	}
    }
    bu_vls_free(&shell_name);

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
