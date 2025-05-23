/*                         T R A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/trace.c
 *
 * The trace command.
 *
 */

#include "ged.h"

#include "./ged_private.h"


static void
trace_do(struct db_i *dbip,
	     struct rt_comb_internal *UNUSED(comb),
	     union tree *comb_leaf,
	     void *user_ptr1,
	     void *user_ptr2,
	     void *user_ptr3,
	     void *user_ptr4)
{
    int *verbose;
    int *pathpos;
    matp_t old_xlate;
    mat_t new_xlate;
    struct directory *nextdp;
    struct _ged_trace_data *gtdp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((nextdp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    pathpos = (int *)user_ptr1;
    old_xlate = (matp_t)user_ptr2;
    gtdp = (struct _ged_trace_data *)user_ptr3;
    verbose = (int *)user_ptr4;

    /*
     * In _GED_EVAL_ONLY mode we're collecting the matrices along
     * the path in order to perform some type of edit where the object
     * lives (i.e. after applying the accumulated transforms). So, if
     * we're doing a matrix edit (i.e. the last object in the path is
     * a combination), we skip its leaf matrices because those are the
     * one's we'll be editing.
     */
    if (gtdp->gtd_flag != _GED_EVAL_ONLY ||
	(*pathpos)+1 < gtdp->gtd_objpos) {
	if (comb_leaf->tr_l.tl_mat) {
	    bn_mat_mul(new_xlate, old_xlate, comb_leaf->tr_l.tl_mat);
	} else {
	    MAT_COPY(new_xlate, old_xlate);
	}
    } else {
	MAT_COPY(new_xlate, old_xlate);
    }

    ged_trace(nextdp, (*pathpos)+1, new_xlate, gtdp, *verbose);
}


void
ged_trace(struct directory *dp,
	   int pathpos,
	   const mat_t old_xlate,
	   struct _ged_trace_data *gtdp,
	   int verbose)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int i;
    int id;

    if (pathpos >= _GED_TRACE_MAX_LEVELS) {
	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "nesting exceeds %d levels\n", _GED_TRACE_MAX_LEVELS);

	for (i = 0; i < _GED_TRACE_MAX_LEVELS; i++)
	    bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "/%s", gtdp->gtd_path[i]->d_namep);

	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "\n");
	return;
    }

    if (dp->d_flags & RT_DIR_COMB) {
	if (rt_db_get_internal(&intern, dp, gtdp->gtd_gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "Database read error, aborting");
	    return;
	}

	gtdp->gtd_path[pathpos] = dp;
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if (comb->tree)
	    db_tree_funcleaf(gtdp->gtd_gedp->dbip, comb, comb->tree, trace_do,
			     (void *)&pathpos, (void *)old_xlate, (void *)gtdp, (void *)&verbose);

	rt_db_free_internal(&intern);

	return;
    }

    /* not a combination -  should have a solid */

    /* last (bottom) position */
    gtdp->gtd_path[pathpos] = dp;

    /* check for desired path */
    if (gtdp->gtd_flag == _GED_CPEVAL) {
	for (i = 0; i <= pathpos; i++) {
	    if (gtdp->gtd_path[i]->d_addr != gtdp->gtd_obj[i]->d_addr) {
		/* not the desired path */
		return;
	    }
	}
    } else {
	for (i = 0; i < gtdp->gtd_objpos; i++) {
	    if (gtdp->gtd_path[i]) {
		if (gtdp->gtd_path[i]->d_addr != gtdp->gtd_obj[i]->d_addr) {
		    /* not the desired path */
		    return;
		}
	    } else {
		/* not the desired path */
		return;
	    }
	}
    }

    /* have the desired path up to objpos */
    MAT_COPY(gtdp->gtd_xform, old_xlate);
    gtdp->gtd_prflag = 1;

    if (gtdp->gtd_flag == _GED_CPEVAL ||
	gtdp->gtd_flag == _GED_EVAL_ONLY)
	return;

    /* print the path */
    for (i = 0; i < pathpos; i++)
	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "/%s", gtdp->gtd_path[i]->d_namep);

    if (gtdp->gtd_flag == _GED_LISTPATH) {
	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "/%s:\n", dp->d_namep);
	return;
    }

    /* NOTE - only reach here if gtd_flag == _GED_LISTEVAL */
    bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "/");
    id = rt_db_get_internal(&intern, dp, gtdp->gtd_gedp->dbip, gtdp->gtd_xform, &rt_uniresource);
    if (id < 0) {
	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
	return;
    }
    bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "%s:\n", dp->d_namep);
    if (!OBJ[id].ft_describe ||
	OBJ[id].ft_describe(gtdp->gtd_gedp->ged_result_str,
			    &intern,
			    verbose,
			    gtdp->gtd_gedp->dbip->dbi_base2local) < 0)

	bu_vls_printf(gtdp->gtd_gedp->ged_result_str, "%s: describe error\n", dp->d_namep);
    rt_db_free_internal(&intern);
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
