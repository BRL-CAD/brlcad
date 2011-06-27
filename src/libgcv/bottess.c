/*                    B O T T E S S . C
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

/** @file libgcv/region_end.c
 *
 * Generate a tree of resolved BoT's, one per region. Roughly based
 * on UnBBoolean's j3dbool (and associated papers).
 *
 */

#include "common.h"

#include "raytrace.h"
#include "nmg.h"
#include "gcv.h"

/* these are just for debugging during development. */
static void prbot(char *prefix, union tree *tr)
{
    struct bu_vls s;
    struct rt_db_internal ip;
    const struct rt_functab *ft = &rt_functab[ID_BOT];

    bu_vls_init(&s);
    ip.idb_ptr = (struct rt_bot_internal*)tr->tr_d.td_r->m_p;
    ft->ft_describe(&s, &ip, 1, 1.0, 0, 0);
    if(prefix)bu_log("*** %s\n", prefix);
    bu_log(bu_vls_addr(&s));
}

/* hijack the top four bits of mode. For these operations, the mode should
 * necessarily be 0x02 */
#define INSIDE		0x10
#define OUTSIDE		0x20
#define SAME		0x40
#define OPPOSITE	0x80

static union tree *
invert(union tree *tree)
{
    struct rt_bot_internal *b;
    RT_CK_TREE(tree);
    b = (struct rt_bot_internal *)tree->tr_d.td_r->m_p;
    RT_BOT_CK_MAGIC(b);
    if(b->orientation != RT_BOT_CW)
	bu_bomb("bad order fed in\n");
    if(rt_bot_flip(b))
	bu_bomb("0 is no longer 0, universe ending\n");
    b->orientation = RT_BOT_CW;
    return tree;
}

static void
split_faces(union tree *left_tree, union tree *right_tree)
{
    struct rt_bot_internal *l, *r;
    bu_log(":%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct rt_bot_internal *)left_tree->tr_d.td_r->m_p;
    r = (struct rt_bot_internal *)right_tree->tr_d.td_r->m_p;
    RT_BOT_CK_MAGIC(l);
    RT_BOT_CK_MAGIC(r);
    /* this is going to be big and hairy. Has to walk both meshes finding
     * all intersections and split intersecting faces so there are edges at
     * the intersections. Initially going to be O(n^2), then change to use
     * space partitioning (binning? octree? kd?). */
}

static void
classify_faces(union tree *left_tree, union tree *right_tree)
{
    struct rt_bot_internal *l, *r;
    bu_log(":%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct rt_bot_internal *)left_tree->tr_d.td_r->m_p;
    r = (struct rt_bot_internal *)right_tree->tr_d.td_r->m_p;
    RT_BOT_CK_MAGIC(l);
    RT_BOT_CK_MAGIC(r);
    /* walk the two trees, marking each face as being inside or outside.
     * O(n)? n^2? */
}

static union tree *
compose(union tree *left_tree, union tree *right_tree, int face1, int face2, int face3)
{
    struct rt_bot_internal *l, *r;
    bu_log(":%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
    RT_CK_TREE(left_tree);
    RT_CK_TREE(right_tree);
    l = (struct rt_bot_internal *)left_tree->tr_d.td_r->m_p;
    r = (struct rt_bot_internal *)right_tree->tr_d.td_r->m_p;
    RT_BOT_CK_MAGIC(l);
    RT_BOT_CK_MAGIC(r);
    /* remove unnecessary faces and compose a single new internal */
    face1=face2=face3;
    prbot("left_tree", left_tree);
    prbot("right_tree", right_tree);
    return left_tree;
}

static union tree *
evaluate(union tree *tr, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
    bu_log(":%s:%s:%d\n",__FILE__,__FUNCTION__,__LINE__);
    RT_CK_TREE(tr);
    bu_log("	op: %x\t", tr->tr_op);
    switch(tr->tr_op) {
	case OP_SOLID: bu_log("OP_SOLID\n"); break;
	case OP_UNION: bu_log("OP_UNION\n"); break;
	case OP_INTERSECT: bu_log("OP_INTERSECT\n"); break;
	case OP_SUBTRACT: bu_log("OP_SUBTRACT\n"); break;
	case OP_XOR: bu_log("OP_XOR\n"); break;
	case OP_REGION: bu_log("OP_REGION\n"); break;
	case OP_NOP: bu_log("OP_NOP\n"); break;
	case OP_NOT: bu_log("OP_NOT\n"); break;
	case OP_GUARD: bu_log("OP_GUARD\n"); break;
	case OP_XNOP: bu_log("OP_XNOP\n"); break;
	case OP_NMG_TESS: bu_log("OP_NMG_TESS\n"); break;
	case OP_DB_LEAF: bu_log("OP_DB_LEAF\n"); break;
	case OP_FREE: bu_log("OP_FREE\n"); break;
    }

    switch(tr->tr_op) {
	case OP_NOP:
	    return tr;
	case OP_NMG_TESS:
	    /* ugh, keep it as nmg_tess and just shove the rt_bot_internal ptr
	     * in as nmgregion. :/ Also, only doing the first shell of the first
	     * model. Primitives should only provide a single shell, right? */
	    tr->tr_d.td_r->m_p = (struct model *)nmg_bot(BU_LIST_FIRST(shell, &BU_LIST_FIRST(nmgregion, &tr->tr_d.td_r->m_p->r_hd)->s_hd), tol);
	    return tr;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    tr->tr_b.tb_left = evaluate(tr->tr_b.tb_left, ttol, tol);
	    tr->tr_b.tb_right = evaluate(tr->tr_b.tb_right, ttol, tol);
	    split_faces(tr->tr_b.tb_left, tr->tr_b.tb_right);
	    classify_faces(tr->tr_b.tb_left, tr->tr_b.tb_right);
	    break;
	default:
	    bu_bomb("bottess evaluate(): bad op (first pass)\n");
    }

    switch(tr->tr_op) {
	case OP_UNION:
	    return compose(tr->tr_b.tb_left, tr->tr_b.tb_right, OUTSIDE, SAME, OUTSIDE);
	case OP_INTERSECT:
	    return compose(tr->tr_b.tb_left, tr->tr_b.tb_right, INSIDE, SAME, INSIDE);
	case OP_SUBTRACT:
	    return invert(compose(tr->tr_b.tb_left, invert(tr->tr_b.tb_right), OUTSIDE, OPPOSITE, INSIDE));
	default:
	    bu_bomb("bottess evaluate(): bad op (second pass, CSG)\n");
    }
    bu_bomb("Got somewhere I shouldn't have\n");
    return NULL;
}

union tree *
gcv_bottess_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree, genptr_t client_data)
{
    union tree *ret_tree = NULL;

    bu_bomb("No\n");

    if (!tsp || !curtree || !pathp || !client_data) {
	bu_log("INTERNAL ERROR: gcv_bottess_region_end missing parameters\n");
	return TREE_NULL;
    }

    RT_CK_FULL_PATH(pathp);
    RT_CK_TREE(curtree);
    RT_CK_TESS_TOL(tsp->ts_ttol);
    BN_CK_TOL(tsp->ts_tol);
    NMG_CK_MODEL(*tsp->ts_m);

    if (curtree->tr_op == OP_NOP)
	return curtree;

    evaluate(curtree, tsp->ts_ttol, tsp->ts_tol);
    /*
    rt_bot_tess(curtree->tr_d.td_r
    */

    return ret_tree;
}

union tree *
gcv_bottess(int argc, const char **argv, struct db_i *dbip, struct rt_tess_tol *ttol)
{
    struct db_tree_state tree_state = rt_initial_tree_state;
    tree_state.ts_ttol = ttol;

    db_walk_tree(dbip, argc, argv, 1, &tree_state, NULL, gcv_bottess_region_end, nmg_booltree_leaf_tess, NULL);

    return NULL;
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
