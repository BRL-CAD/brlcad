/*                      F A C E T I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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


#include "common.h"

#include "bu/parallel.h"
#include "gcv/util.h"
#include "rt/db5.h"
#include "rt/db_internal.h"
#include "rt/wdb.h"
#include "rt/global.h"
#include "rt/primitives/bot.h"
#include "rt/functab.h"

HIDDEN union tree *
_gcv_facetize_region_end(struct db_tree_state *tree_state,
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

	RT_CK_TREE(*facetize_tree);

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
_gcv_facetize_cleanup(struct model *nmg_model, union tree *facetize_tree)
{
    if (nmg_model) {
	NMG_CK_MODEL(nmg_model);
	nmg_km(nmg_model);
    }

    if (facetize_tree) {
	RT_CK_TREE(facetize_tree);
	db_free_tree(facetize_tree, &rt_uniresource);
    }

    return NULL;
}


HIDDEN void
_gcv_facetize_free_bot(struct rt_bot_internal *bot)
{
    /* fill in an rt_db_internal so we can free it */
    struct rt_db_internal internal;

    RT_BOT_CK_MAGIC(bot);

    RT_DB_INTERNAL_INIT(&internal);
    internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    internal.idb_minor_type = ID_BOT;
    internal.idb_meth = &OBJ[ID_BOT];
    internal.idb_ptr = bot;

    rt_db_free_internal(&internal);
}


HIDDEN void
_gcv_optimize_model(struct model *nmg_model)
{
    struct nmgregion *current_region;

    NMG_CK_MODEL(nmg_model);

    for (BU_LIST_FOR(current_region, nmgregion, &nmg_model->r_hd)) {
	struct shell *current_shell;

	NMG_CK_REGION(current_region);
	current_shell = BU_LIST_FIRST(shell, &current_region->s_hd);

	while (BU_LIST_NOT_HEAD(&current_shell->l, &current_region->s_hd)) {
	    struct shell *next_shell;

	    NMG_CK_SHELL(current_shell);
	    next_shell = BU_LIST_PNEXT(shell, &current_shell->l);

	    if (nmg_kill_cracks(current_shell))
		nmg_ks(current_shell);

	    current_shell = next_shell;
	}
    }

    nmg_kill_zero_length_edgeuses(nmg_model);
}


struct rt_bot_internal *
gcv_facetize(struct db_i *db, const struct db_full_path *path,
	     const struct bn_tol *tol, const struct bg_tess_tol *tess_tol)
{
    union tree *facetize_tree;
    struct model *nmg_model;

    /* volatile to silence warnings over longjmp */
    struct nmgregion * volatile current_region = NULL;
    struct rt_bot_internal * volatile result = NULL;
    struct shell * volatile current_shell = NULL;

    RT_CK_DBI(db);
    RT_CK_FULL_PATH(path);
    BN_CK_TOL(tol);
    BG_CK_TESS_TOL(tess_tol);

    {
	char * const str_path = db_path_to_string(path);
	struct db_tree_state initial_tree_state = rt_initial_tree_state;
	initial_tree_state.ts_tol = tol;
	initial_tree_state.ts_ttol = tess_tol;
	initial_tree_state.ts_m = &nmg_model;

	facetize_tree = NULL;
	nmg_model = nmg_mm();

	if (db_walk_tree(db, 1, (const char **)&str_path, 1, &initial_tree_state, NULL,
			 _gcv_facetize_region_end, nmg_booltree_leaf_tess, &facetize_tree)) {
	    bu_log("gcv_facetize(): error in db_walk_tree()\n");
	    bu_free(str_path, "str_path");
	    return _gcv_facetize_cleanup(nmg_model, facetize_tree);
	}

	bu_free(str_path, "str_path");
    }

    if (!facetize_tree)
	return _gcv_facetize_cleanup(nmg_model, facetize_tree);

    /* Now, evaluate the boolean tree into ONE region */
    if (!BU_SETJUMP) {
	/* try */
	if (nmg_boolean(facetize_tree, nmg_model, &RTG.rtg_vlfree, tol, &rt_uniresource)) {
	    BU_UNSETJUMP;
	    return _gcv_facetize_cleanup(nmg_model, facetize_tree);
	}
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_log("gcv_facetize(): boolean evaluation failed\n");
	return _gcv_facetize_cleanup(nmg_model, facetize_tree);
    }

    BU_UNSETJUMP;

    /* New region remains part of this nmg "model" */
    NMG_CK_REGION(facetize_tree->tr_d.td_r);

    _gcv_optimize_model(nmg_model);

    for (BU_LIST_FOR(current_region, nmgregion, &nmg_model->r_hd)) {
	current_shell = NULL;

	NMG_CK_REGION(current_region);

	for (BU_LIST_FOR(current_shell, shell, &current_region->s_hd)) {
	    NMG_CK_SHELL(current_shell);

	    if (!BU_SETJUMP) {
		/* try */
		if (!result)
		    result = nmg_bot(current_shell, &RTG.rtg_vlfree, tol);
		else {
		    struct rt_bot_internal *bots[2];
		    bots[0] = result;
		    bots[1] = nmg_bot(current_shell, &RTG.rtg_vlfree, tol);
		    result = rt_bot_merge(sizeof(bots) / sizeof(bots[0]),
					  (const struct rt_bot_internal * const *)bots);
		    _gcv_facetize_free_bot(bots[0]);
		    _gcv_facetize_free_bot(bots[1]);
		}
	    } else {
		/* catch */
		BU_UNSETJUMP;
		bu_log("gcv_facetize(): conversion to BoT failed\n");

		if (result)
		    _gcv_facetize_free_bot(result);

		return _gcv_facetize_cleanup(nmg_model, facetize_tree);
	    }

	    BU_UNSETJUMP;
	}
    }

    if (result) {
	rt_bot_vertex_fuse(result, tol);
	rt_bot_face_fuse(result);
	rt_bot_condense(result);
    }

    _gcv_facetize_cleanup(nmg_model, facetize_tree);
    return result;
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
