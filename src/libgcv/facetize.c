/*                      F A C E T I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file facetize.c
 *
 * Convenience functions for facetization.
 */


#include "./facetize.h"


HIDDEN union tree *
	gcv_facetize_region_end(struct db_tree_state *tree_state,
				const struct db_full_path *path, union tree *current_tree, void *client_data)
{
    union tree **facetize_tree;

    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_TREE(current_tree);

    facetize_tree = (union tree **)client_data;

    if (current_tree->tr_op == OP_NOP)
	return current_tree;

    if (*facetize_tree) {
	union tree *temp;
	BU_ALLOC(temp, union tree);
	RT_TREE_INIT(temp);
	temp->tr_op = OP_UNION;
	temp->tr_b.tb_regionp = REGION_NULL;
	temp->tr_b.tb_left = *facetize_tree;
	temp->tr_b.tb_right = current_tree;
	*facetize_tree = temp;
    } else {
	*facetize_tree = current_tree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


HIDDEN struct rt_bot_internal *
gcv_facetize_cleanup(struct model *nmg_model, union tree *facetize_tree)
{
    nmg_km(nmg_model);
    db_free_tree(facetize_tree, &rt_uniresource);
    return NULL;
}


struct rt_bot_internal *
gcv_facetize(struct db_i *db, const struct db_full_path *path,
	     const struct bn_tol *tol, const struct rt_tess_tol *tess_tol)
{
    union tree *facetize_tree;
    struct model *nmg_model;

    RT_CK_DBI(db);
    RT_CK_FULL_PATH(path);
    BN_CK_TOL(math_tol);
    RT_CK_TESS_TOL(tess_tol);

    {
	char *str_path = db_path_to_string(path);
	struct db_tree_state initial_tree_state = rt_initial_tree_state;
	initial_tree_state.ts_tol = math_tol;
	initial_tree_state.ts_ttol = tess_tol;
	initial_tree_state.ts_m = &nmg_model;

	facetize_tree = NULL;
	nmg_model = nmg_mm();

	if (db_walk_tree(db, 1, (const char **)&str_path, 1, &initial_tree_state, NULL,
			 gcv_facetize_region_end, nmg_booltree_leaf_tess, &facetize_tree)) {
	    bu_log("gcv_facetize(): error in db_walk_tree()\n");
	    bu_free(str_path, "str_path");
	    return gcv_facetize_cleanup(nmg_model, facetize_tree);
	}

	bu_free(str_path, "str_path");
    }

    if (!facetize_tree)
	return gcv_facetize_cleanup(nmg_model, facetize_tree);

    /* Now, evaluate the boolean tree into ONE region */
    if (!BU_SETJUMP) {
	/* try */
	if (nmg_boolean(facetize_tree, nmg_model, math_tol, &rt_uniresource)) {
	    BU_UNSETJUMP;
	    return gcv_facetize_cleanup(nmg_model, facetize_tree);
	}
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_log("gcv_facetize(): boolean evaluation failed\n");
	return gcv_facetize_cleanup(nmg_model, facetize_tree);
    }

    BU_UNSETJUMP;

    /* New region remains part of this nmg "model" */
    NMG_CK_REGION(facetize_tree->tr_d.td_r);

    {
	struct rt_bot_internal *result;
	struct nmgregion *nmg_region;
	struct shell *current_shell;

	/* FIXME: only dumping the first shell of the first region */
	nmg_region = BU_LIST_FIRST(nmgregion, &nmg_model->r_hd);
	current_shell = BU_LIST_FIRST(shell, &nmg_region->s_hd);

	if (!BU_SETJUMP) {
	    /* try */
	    result = nmg_bot(current_shell, math_tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    bu_log("gcv_facetize(): conversion to BOT failed\n");
	    return gcv_facetize_cleanup(nmg_model, facetize_tree);
	}

	BU_UNSETJUMP;
	gcv_facetize_cleanup(nmg_model, facetize_tree);
	return result;
    }
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
