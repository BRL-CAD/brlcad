/*                     B O O L _ T E S S . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2023 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file bool_tess.c
 *
 * Tree walking routines for tessellation/facetization of CSG trees.
 *
 * Originally these routines were specific to NMG, but they have
 * been made generic here with callbacks.
 *
 * The leaf tessellation routine, however, does still use NMG - that is
 * because for CSG primitives, in particular, NMG is how they are converted
 * from implicit to explicit form.  For the purposes of these routines,
 * that should be considered an implementation detail.
 */
/** @} */

#include "common.h"
#include "string.h"
#include "nmg.h"
#include "raytrace.h"

/**
 * Called from db_walk_tree() each time a tree leaf is encountered.
 * The primitive solid, in external format, is provided in 'ep', and
 * the type of that solid (e.g. ID_ELL) is in 'id'.  The full tree
 * state including the accumulated transformation matrix and the
 * current tolerancing is in 'tsp', and the full path from root to
 * leaf is in 'pathp'.
 *
 * Import the solid, tessellate it into an NMG, stash a pointer to the
 * tessellation in a new tree structure (union), and return a pointer
 * to that.
 *
 * Usually given as an argument to, and called from db_walk_tree().
 *
 * This routine must be prepared to run in parallel.
 */
union tree *
rt_booltree_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *UNUSED(client_data))
{
    struct model *m;
    struct nmgregion *r1 = (struct nmgregion *)NULL;
    union tree *curtree;
    struct directory *dp;

    if (!tsp || !pathp || !ip)
	return TREE_NULL;

    RT_CK_DB_INTERNAL(ip);
    RT_CK_FULL_PATH(pathp);
    dp = DB_FULL_PATH_CUR_DIR(pathp);
    RT_CK_DIR(dp);

    if (!ip->idb_meth || !ip->idb_meth->ft_tessellate) {
	bu_log("ERROR(%s): tessellation support not available\n", dp->d_namep);
	return TREE_NULL;
    }

    if (tsp->ts_m)
	NMG_CK_MODEL(*tsp->ts_m);
    BN_CK_TOL(tsp->ts_tol);
    BG_CK_TESS_TOL(tsp->ts_ttol);
    RT_CK_RESOURCE(tsp->ts_resp);

    m = nmg_mm();

    if (ip->idb_meth->ft_tessellate(&r1, m, ip, tsp->ts_ttol, tsp->ts_tol) < 0) {
	bu_log("ERROR(%s): tessellation failure\n", dp->d_namep);
	return TREE_NULL;
    }

    NMG_CK_REGION(r1);
    if (nmg_debug & NMG_DEBUG_VERIFY) {
	nmg_vshell(&r1->s_hd, r1);
    }

    BU_GET(curtree, union tree);
    RT_TREE_INIT(curtree);
    curtree->tr_op = OP_TESS;
    curtree->tr_d.td_name = bu_strdup(dp->d_namep);
    curtree->tr_d.td_r = r1;
    curtree->tr_d.td_d = NULL;

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK)
	bu_log("booltree_leaf_tess(%s) OK\n", dp->d_namep);

    return curtree;
}

/**
 * Given a tree of leaf nodes tessellated earlier by
 * rt_booltree_leaf_tess(), use recursion to do a depth-first
 * traversal of the tree, evaluating each pair of boolean operations
 * and reducing that result to a single result.
 *
 * Returns an OP_TESS union tree node, which will contain the
 * resulting region and its name, as a dynamic string.  The caller is
 * responsible for releasing the string, and the node, by calling
 * db_free_tree() on the node.
 *
 * Returns TREE_NULL if there is no geometry to return.
 */
union tree *
rt_booltree_evaluate(
	register union tree *tp,
	struct bu_list *vlfree,
	const struct bn_tol *tol,
	struct resource *resp,
	int (*do_bool)(union tree *, union tree *, union tree *, int op, struct bu_list *, const struct bn_tol *),
	int verbose
	)
{
    if (!do_bool) {
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    union tree *tl;
    union tree *tr;
    const char *op_str = " u "; /* default value */
    size_t rem;
    char *name;

    RT_CK_TREE(tp);
    if (tol)
	BN_CK_TOL(tol);
    RT_CK_RESOURCE(resp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return TREE_NULL;
	case OP_TESS:
	    /* Hit a tree leaf */
	    if (tp->tr_d.td_r && nmg_debug & NMG_DEBUG_VERIFY) {
		nmg_vshell(&tp->tr_d.td_r->s_hd, tp->tr_d.td_r);
	    }
	    return tp;
	case OP_UNION:
	    op_str = " u ";
	    break;
	case OP_INTERSECT:
	    op_str = " + ";
	    break;
	case OP_SUBTRACT:
	    op_str = " - ";
	    break;
	default:
	    bu_bomb("rt_booltree_evaluate(): bad op\n");
    }

    /* Handle a boolean operation node.  First get its leaves. */
    tl = rt_booltree_evaluate(tp->tr_b.tb_left, vlfree, tol, resp, do_bool, verbose);
    tr = rt_booltree_evaluate(tp->tr_b.tb_right, vlfree, tol, resp, do_bool, verbose);

    if (tl) {
	RT_CK_TREE(tl);
	if (tl != tp->tr_b.tb_left) {
	    bu_bomb("rt_booltree_evaluate(): tl != tp->tr_b.tb_left\n");
	}
    }
    if (tr) {
	RT_CK_TREE(tr);
	if (tr != tp->tr_b.tb_right) {
	    bu_bomb("rt_booltree_evaluate(): tr != tp->tr_b.tb_right\n");
	}
    }

    if (!tl && !tr) {
	/* left-r == null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	db_free_tree(tp->tr_b.tb_right, resp);
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    if (tl && !tr) {
	/* left-r != null && right-r == null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_right, resp);
	if (tp->tr_op == OP_INTERSECT) {
	    /* OP_INTERSECT '+' */
	    RT_CK_TREE(tp);
	    db_free_tree(tl, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;
	} else {
	    /* copy everything from tl to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tl->tr_op;
	    tp->tr_b.tb_regionp = tl->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tl->tr_b.tb_left;
	    tp->tr_b.tb_right = tl->tr_b.tb_right;

	    /* null data from tl so only to free this node */
	    tl->tr_b.tb_regionp = (struct region *)NULL;
	    tl->tr_b.tb_left = TREE_NULL;
	    tl->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tl, resp);
	    return tp;
	}
    }

    if (!tl && tr) {
	/* left-r == null && right-r != null */
	RT_CK_TREE(tp);
	db_free_tree(tp->tr_b.tb_left, resp);
	if (tp->tr_op == OP_UNION) {
	    /* OP_UNION 'u' */
	    /* copy everything from tr to tp no matter which union type
	     * could probably have done a mem-copy
	     */
	    tp->tr_op = tr->tr_op;
	    tp->tr_b.tb_regionp = tr->tr_b.tb_regionp;
	    tp->tr_b.tb_left = tr->tr_b.tb_left;
	    tp->tr_b.tb_right = tr->tr_b.tb_right;

	    /* null data from tr so only to free this node */
	    tr->tr_b.tb_regionp = (struct region *)NULL;
	    tr->tr_b.tb_left = TREE_NULL;
	    tr->tr_b.tb_right = TREE_NULL;

	    db_free_tree(tr, resp);
	    return tp;

	} else if ((tp->tr_op == OP_SUBTRACT) || (tp->tr_op == OP_INTERSECT)) {
	    /* for sub and intersect, if left-hand-side is null, result is null */
	    RT_CK_TREE(tp);
	    db_free_tree(tr, resp);
	    tp->tr_op = OP_NOP;
	    return TREE_NULL;

	} else {
	    bu_bomb("rt_booltree_evaluate(): error, unknown operation\n");
	}
    }

    if (tl->tr_op != OP_TESS) {
	bu_bomb("rt_booltree_evaluate(): bad left tree\n");
    }
    if (tr->tr_op != OP_TESS) {
	bu_bomb("rt_booltree_evaluate(): bad right tree\n");
    }

    if (verbose) {
	bu_log(" {%s}%s{%s}\n", tl->tr_d.td_name, op_str, tr->tr_d.td_name);
    }

    // Execute the specified method that actually evaluates the mesh geometries
    int b = do_bool(tp, tl, tr, tp->tr_op, vlfree, tol);

    if (b) {
	/* result was null */
	tp->tr_op = OP_NOP;
	return TREE_NULL;
    }

    /* build string of result name */
    rem = strlen(tl->tr_d.td_name) + 3 + strlen(tr->tr_d.td_name) + 2 + 1;
    name = (char *)bu_calloc(rem, sizeof(char), "rt_booltree_evaluate name");
    snprintf(name, rem, "(%s%s%s)", tl->tr_d.td_name, op_str, tr->tr_d.td_name);
    tp->tr_d.td_name = name;

    /* clean up child tree nodes */
    tl->tr_d.td_r = NULL;
    tr->tr_d.td_r = NULL;
    tl->tr_d.td_d = NULL;
    tr->tr_d.td_d = NULL;
    db_free_tree(tl, resp);
    db_free_tree(tr, resp);

    return tp;
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
