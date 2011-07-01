/*                        F R A C T U R E . C
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
/** @file libged/fracture.c
 *
 * The fracture command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


static int frac_stat;


static void
fracture_add_nmg_part(struct ged *gedp, char *newname, struct model *m)
{
    struct rt_db_internal new_intern;
    struct directory *new_dp;
    struct nmgregion *r;

    if (db_lookup(gedp->ged_wdbp->dbip,  newname, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists\n", newname);
	/* Free memory here */
	nmg_km(m);
	frac_stat = 1;
	return;
    }

    new_dp=db_diradd(gedp->ged_wdbp->dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&new_intern.idb_type);
    if (new_dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to add new object name (%s) to directory - aborting!!\n",
		      newname);
	return;
    }

    /* make sure the geometry/bounding boxes are up to date */
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd))
	nmg_region_a(r, &gedp->ged_wdbp->wdb_tol);


    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&new_intern);
    new_intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    new_intern.idb_type = ID_NMG;
    new_intern.idb_meth = &rt_functab[ID_NMG];
    new_intern.idb_ptr = (genptr_t)m;

    if (rt_db_put_internal(new_dp, gedp->ged_wdbp->dbip, &new_intern, &rt_uniresource) < 0) {
	/* Free memory */
	nmg_km(m);
	bu_vls_printf(gedp->ged_result_str, "rt_db_put_internal() failure\n");
	frac_stat = 1;
	return;
    }
    /* Internal representation has been freed by rt_db_put_internal */
    new_intern.idb_ptr = (genptr_t)NULL;
    frac_stat = 0;
}


int
ged_fracture(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct directory *old_dp;
    struct rt_db_internal old_intern;
    struct model *m, *new_model;
    char newname[32];
    char prefix[32];
    int maxdigits;
    struct nmgregion *r, *new_r;
    struct shell *s, *new_s;
    struct faceuse *fu;
    struct vertex *v_new, *v;
    unsigned long tw, tf, tp;
    static const char *usage = "nmg_solid [prefix]";

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

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "fracture:");
    for (i=0; i < argc; i++)
	bu_vls_printf(gedp->ged_result_str, " %s", argv[i]);
    bu_vls_printf(gedp->ged_result_str, "\n");

    if ((old_dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (rt_db_get_internal(&old_intern, old_dp, gedp->ged_wdbp->dbip, bn_mat_identity, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal() error\n");
	return GED_ERROR;
    }

    if (old_intern.idb_type != ID_NMG) {
	bu_vls_printf(gedp->ged_result_str, " is not an NMG solid!!\n");
	rt_db_free_internal(&old_intern);
	return GED_ERROR;
    }

    m = (struct model *)old_intern.idb_ptr;
    NMG_CK_MODEL(m);

    /* how many characters of the solid names do we reserve for digits? */
    nmg_count_shell_kids(m, &tf, &tw, &tp);

    maxdigits = (int)(log10((double)(tf+tw+tp)) + 1.0);

    bu_vls_printf(gedp->ged_result_str, "%ld = %d digits\n", (long)(tf+tw+tp), maxdigits);

    /* for (maxdigits=1, i=tf+tw+tp; i > 0; i /= 10)
     * maxdigits++;
     */

    /* get the prefix for the solids to be created. */
    memset(prefix, 0, sizeof(prefix));
    bu_strlcpy(prefix, argv[argc-1], sizeof(prefix));
    bu_strlcat(prefix, "_", sizeof(prefix));

    /* Bust it up here */

    i = 1;
    for (BU_LIST_FOR(r, nmgregion, &m->r_hd)) {
	NMG_CK_REGION(r);
	for (BU_LIST_FOR(s, shell, &r->s_hd)) {
	    NMG_CK_SHELL(s);
	    if (s->vu_p) {
		NMG_CK_VERTEXUSE(s->vu_p);
		NMG_CK_VERTEX(s->vu_p->v_p);
		v = s->vu_p->v_p;

		new_model = nmg_mm();
		new_r = nmg_mrsv(new_model);
		new_s = BU_LIST_FIRST(shell, &r->s_hd);
		v_new = new_s->vu_p->v_p;
		if (v->vg_p) {
		    nmg_vertex_gv(v_new, v->vg_p->coord);
		}

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);

		fracture_add_nmg_part(gedp, newname, new_model);
		if (frac_stat) return GED_ERROR;
		continue;
	    }
	    for (BU_LIST_FOR(fu, faceuse, &s->fu_hd)) {
		if (fu->orientation != OT_SAME)
		    continue;

		NMG_CK_FACEUSE(fu);

		new_model = nmg_mm();
		NMG_CK_MODEL(new_model);
		new_r = nmg_mrsv(new_model);
		NMG_CK_REGION(new_r);
		new_s = BU_LIST_FIRST(shell, &new_r->s_hd);

		NMG_CK_SHELL(new_s);
		nmg_dup_face(fu, new_s);

		snprintf(newname, 32, "%s%0*d", prefix, maxdigits, i++);
		fracture_add_nmg_part(gedp, newname, new_model);
		if (frac_stat) return GED_ERROR;
	    }
	}
    }

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
