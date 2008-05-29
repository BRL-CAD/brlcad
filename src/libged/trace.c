/*                         T R A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2008 United States Government as represented by
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
/** @file trace.c
 *
 * The trace command.
 *
 */

#include "ged.h"
#include "ged_private.h"

static void
ged_do_trace(struct db_i		*dbip,
	     struct rt_comb_internal	*comb,
	     union tree			*comb_leaf,
	     genptr_t			user_ptr1,
	     genptr_t			user_ptr2,
	     genptr_t			user_ptr3)
{
    int			*pathpos;
    matp_t			old_xlate;
    mat_t			new_xlate;
    struct directory	*nextdp;
    struct wdb_trace_data	*wtdp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((nextdp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL)
	return;

    pathpos = (int *)user_ptr1;
    old_xlate = (matp_t)user_ptr2;
    wtdp = (struct wdb_trace_data *)user_ptr3;

    /*
     * In WDB_EVAL_ONLY mode we're collecting the matrices along
     * the path in order to perform some type of edit where the object
     * lives (i.e. after applying the accumulated transforms). So, if
     * we're doing a matrix edit (i.e. the last object in the path is
     * a combination), we skip its leaf matrices because those are the
     * one's we'll be editing.
     */
    if (wtdp->wtd_flag != WDB_EVAL_ONLY ||
	(*pathpos)+1 < wtdp->wtd_objpos) {
	if (comb_leaf->tr_l.tl_mat) {
	    bn_mat_mul(new_xlate, old_xlate, comb_leaf->tr_l.tl_mat);
	} else {
	    MAT_COPY(new_xlate, old_xlate);
	}
    } else {
	MAT_COPY(new_xlate, old_xlate);
    }

    ged_trace(nextdp, (*pathpos)+1, new_xlate, wtdp);
}

/**
 *
 *
 */
void
ged_trace(register struct directory	*dp,
	  int				pathpos,
	  const mat_t			old_xlate,
	  struct wdb_trace_data		*wtdp)
{
    struct rt_db_internal	intern;
    struct rt_comb_internal	*comb;
    int			i;
    int			id;
    struct bu_vls		str;

    bu_vls_init(&str);

    if (pathpos >= WDB_MAX_LEVELS) {
	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "nesting exceeds %d levels\n", WDB_MAX_LEVELS);

	for (i=0; i<WDB_MAX_LEVELS; i++)
	    bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "/%s", wtdp->wtd_path[i]->d_namep);

	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "\n");
	return;
    }

    if (dp->d_flags & DIR_COMB) {
	if (rt_db_get_internal(&intern, dp, wtdp->wtd_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "Database read error, aborting");
	    return;
	}

	wtdp->wtd_path[pathpos] = dp;
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if (comb->tree)
	    db_tree_funcleaf(wtdp->wtd_wdbp->dbip, comb, comb->tree, ged_do_trace,
			     (genptr_t)&pathpos, (genptr_t)old_xlate, (genptr_t)wtdp);
#if USE_RT_COMB_IFREE
	rt_comb_ifree(&intern, &rt_uniresource);
#else
	rt_db_free_internal(&intern, &rt_uniresource);
#endif
	return;
    }

    /* not a combination  -  should have a solid */

    /* last (bottom) position */
    wtdp->wtd_path[pathpos] = dp;

    /* check for desired path */
    if ( wtdp->wtd_flag == WDB_CPEVAL ) {
	for (i=0; i<=pathpos; i++) {
	    if (wtdp->wtd_path[i]->d_addr != wtdp->wtd_obj[i]->d_addr) {
		/* not the desired path */
		return;
	    }
	}
    } else {
	for (i=0; i<wtdp->wtd_objpos; i++) {
	    if (wtdp->wtd_path[i]->d_addr != wtdp->wtd_obj[i]->d_addr) {
		/* not the desired path */
		return;
	    }
	}
    }

    /* have the desired path up to objpos */
    MAT_COPY(wtdp->wtd_xform, old_xlate);
    wtdp->wtd_prflag = 1;

    if (wtdp->wtd_flag == WDB_CPEVAL ||
	wtdp->wtd_flag == WDB_EVAL_ONLY)
	return;

    /* print the path */
    for (i=0; i<pathpos; i++)
	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "/%s", wtdp->wtd_path[i]->d_namep);

    if (wtdp->wtd_flag == WDB_LISTPATH) {
	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "/%s:\n", dp->d_namep); 
	return;
    }

    /* NOTE - only reach here if wtd_flag == WDB_LISTEVAL */
    bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "/");
    if ((id=rt_db_get_internal(&intern, dp, wtdp->wtd_wdbp->dbip, wtdp->wtd_xform, &rt_uniresource)) < 0) {
	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "rt_db_get_internal(%s) failure", dp->d_namep);
	return;
    }
    bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "%s:\n", dp->d_namep);
    if (rt_functab[id].ft_describe(&wtdp->wtd_wdbp->wdb_result_str,
				   &intern,
				   1,
				   wtdp->wtd_wdbp->dbip->dbi_base2local,
				   &rt_uniresource,
				   wtdp->wtd_wdbp->dbip) < 0)
	bu_vls_printf(&wtdp->wtd_wdbp->wdb_result_str, "%s: describe error\n", dp->d_namep);
    rt_db_free_internal(&intern, &rt_uniresource);
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
