/*                       D B _ W A L K . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_walk.c
 *
 * No-frills tree-walker.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 * A generic traversal function.
 */
void
db_traverse_subtree(union tree *tp,
		    void (*traverse_func) (struct directory *, struct db_traverse *),
		    struct db_traverse *dtp)
{
    struct directory *dp;

    if (!tp)
	return;

    RT_CK_DB_TRAVERSE(dtp);
    RT_CHECK_DBI(dtp->dbip);
    RT_CK_TREE(tp);
    if (dtp->resp) {
	RT_CK_RESOURCE(dtp->resp);
    }

    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    if ((dp=db_lookup(dtp->dbip, tp->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
		return;
	    traverse_func(dp, dtp);
	    break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_traverse_subtree(tp->tr_b.tb_left, traverse_func, dtp);
	    db_traverse_subtree(tp->tr_b.tb_right, traverse_func, dtp);
	    break;
	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}


/*
 * D B _ P R E O R D E R _ T R A V E R S E
 *
 * This subroutine is called for a no-frills tree-walk,
 * with the provided subroutines being called when entering and
 * exiting combinations and at leaf (solid) nodes.
 *
 * This routine is recursive, so no variables may be declared static.
 *
 */
void
db_preorder_traverse(struct directory *dp,
		     struct db_traverse *dtp)
{
    register size_t i;
    RT_CK_DB_TRAVERSE(dtp);
    RT_CK_DBI(dtp->dbip);
    if (dtp->resp) {
	RT_CK_RESOURCE(dtp->resp);
    }

    if (RT_G_DEBUG & DEBUG_DB)
	bu_log("db_preorder_traverse(%s) x%x, x%x, comb_enter=x%x, comb_exit=x%x, leaf=x%x, client_data=x%x\n",
	       dp->d_namep, dtp->dbip, dp, dtp->comb_enter_func, dtp->comb_exit_func, dtp->leaf_func, dtp->client_data);

    if (dp->d_flags & RT_DIR_COMB) {
	/* entering region */
	if (dtp->comb_enter_func)
	    dtp->comb_enter_func(dtp->dbip, dp, dtp->client_data);
	if (db_version(dtp->dbip) < 5) {
	    register union record *rp;
	    register struct directory *mdp;
	    /*
	     * Load the combination into local record buffer
	     * This is in external v4 format.
	     */
	    if ((rp = db_getmrec(dtp->dbip, dp)) == (union record *)0)
		return;
	    /* recurse */
	    for (i=1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(dtp->dbip, rp[i].M.m_instname,
				     LOOKUP_NOISY)) == RT_DIR_NULL)
		    continue;
		db_preorder_traverse(mdp, dtp);
	    }
	    bu_free((char *)rp, "db_preorder_traverse[]");
	} else {
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal5(&in, dp, dtp->dbip, NULL, dtp->resp) < 0)
		return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;

	    db_traverse_subtree(comb->tree, db_preorder_traverse, dtp);

	    rt_db_free_internal(&in);
	}
	/* exiting region */
	if (dtp->comb_exit_func)
	    dtp->comb_exit_func(dtp->dbip, dp, dtp->client_data);
    } else if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK) {
	/* at leaf */
	if (dtp->leaf_func)
	    dtp->leaf_func(dtp->dbip, dp, dtp->client_data);
    } else {
	bu_log("db_preorder_traverse:  %s is neither COMB nor SOLID?\n",
	       dp->d_namep);
    }
}


/* D B _ F U N C T R E E _ S U B T R E E
 *
 * The only reason for this to be broken out is that
 * 2 separate locations in db_functree() call it.
 */
void
db_functree_subtree(struct db_i *dbip,
		    union tree *tp,
		    void (*comb_func) (struct db_i *, struct directory *, genptr_t),
		    void (*leaf_func) (struct db_i *, struct directory *, genptr_t),
		    struct resource *resp,
		    genptr_t client_data)
{
    struct directory *dp;

    if (!tp)
	return;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);
    if (resp) {
	RT_CK_RESOURCE(resp);
    }

    switch (tp->tr_op) {

	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
		return;
	    db_functree(dbip, dp, comb_func, leaf_func, resp, client_data);
	    break;

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    db_functree_subtree(dbip, tp->tr_b.tb_left, comb_func, leaf_func, resp, client_data);
	    db_functree_subtree(dbip, tp->tr_b.tb_right, comb_func, leaf_func, resp, client_data);
	    break;
	default:
	    bu_log("db_functree_subtree: unrecognized operator %d\n", tp->tr_op);
	    bu_bomb("db_functree_subtree: unrecognized operator\n");
    }
}


/**
 * This subroutine is called for a no-frills tree-walk, with the
 * provided subroutines being called at every combination and leaf
 * (solid) node, respectively.
 *
 * This routine is recursive, so no variables may be declared static.
 */
void
db_functree(struct db_i *dbip,
	    struct directory *dp,
	    void (*comb_func) (struct db_i *, struct directory *, genptr_t),
	    void (*leaf_func) (struct db_i *, struct directory *, genptr_t),
	    struct resource *resp,
	    genptr_t client_data)
{
    register size_t i;

    RT_CK_DBI(dbip);
    if (resp) {
	RT_CK_RESOURCE(resp);
    }

    if ((!dp) || (!comb_func && !leaf_func)) {
	return; /* nothing to do */
    }

    if (RT_G_DEBUG&DEBUG_DB) {
	bu_log("db_functree(%s) x%x, x%x, comb=x%x, leaf=x%x, client_data=x%x\n",
	       dp->d_namep, dbip, dp, comb_func, leaf_func, client_data);
    }

    if (dp->d_flags & RT_DIR_COMB) {
	if (db_version(dbip) < 5) {
	    register union record *rp;
	    register struct directory *mdp;
	    /*
	     * Load the combination into local record buffer
	     * This is in external v4 format.
	     */
	    if ((rp = db_getmrec(dbip, dp)) == (union record *)0)
		return;

	    /* recurse */
	    for (i=1; i < dp->d_len; i++) {
		if ((mdp = db_lookup(dbip, rp[i].M.m_instname, LOOKUP_NOISY)) == RT_DIR_NULL)
		    continue;
		db_functree(dbip, mdp, comb_func, leaf_func, resp, client_data);
	    }
	    bu_free((char *)rp, "db_functree record[]");
	} else {
	    struct rt_db_internal in;
	    struct rt_comb_internal *comb;

	    if (rt_db_get_internal5(&in, dp, dbip, NULL, resp) < 0)
		return;

	    comb = (struct rt_comb_internal *)in.idb_ptr;
	    db_functree_subtree(dbip, comb->tree, comb_func, leaf_func, resp, client_data);
	    rt_db_free_internal(&in);
	}

	/* Finally, the combination itself */
	if (comb_func)
	    comb_func(dbip, dp, client_data);

    } else if (dp->d_flags & RT_DIR_SOLID || dp->d_major_type & DB5_MAJORTYPE_BINARY_MASK) {
	if (leaf_func)
	    leaf_func(dbip, dp, client_data);
    } else {
	bu_log("db_functree:  %s is neither COMB nor SOLID?\n",
	       dp->d_namep);
    }
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
