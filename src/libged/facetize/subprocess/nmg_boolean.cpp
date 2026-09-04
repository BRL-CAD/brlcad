/*               N M G _ B O O L E A N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libged/facetize/subprocess/nmg_boolean.cpp
 *
 * Isolated NMG tree evaluation for the facetize worker.  An NMG bomb may
 * invalidate the model's topology, so a bombed request intentionally leaves
 * its allocations to process teardown rather than risking another bomb while
 * attempting cleanup.
 */

#include "common.h"

#include <string>
#include <vector>

#include "raytrace.h"
#include "wdb.h"

#include "../../ged_private.h"
#include "../ged_facetize.h"
#include "./nmg_boolean.h"

struct FacetizeNmgBooleanResult {
    struct model *nmg_model = NULL;
    struct rt_bot_internal *bot = NULL;
};

struct FacetizeNmgBooleanState {
    union tree *tree = NULL;
};

static union tree *
facetize_nmg_region_end(struct db_tree_state *tsp,
	const struct db_full_path *pathp, union tree *current_tree,
	void *client_data)
{
    if (tsp)
	RT_CK_DBTS(tsp);
    if (pathp)
	RT_CK_FULL_PATH(pathp);

    FacetizeNmgBooleanState *state =
	static_cast<FacetizeNmgBooleanState *>(client_data);
    if (!state || !current_tree || current_tree->tr_op == OP_NOP)
	return current_tree;

    if (!state->tree) {
	state->tree = current_tree;
	return TREE_NULL;
    }

    union tree *union_tree;
    BU_ALLOC(union_tree, union tree);
    RT_TREE_INIT(union_tree);
    union_tree->tr_op = OP_UNION;
    union_tree->tr_b.tb_regionp = REGION_NULL;
    union_tree->tr_b.tb_left = state->tree;
    union_tree->tr_b.tb_right = current_tree;
    state->tree = union_tree;
    return TREE_NULL;
}

static union tree *
facetize_nmg_leaf(struct db_tree_state *tsp,
	const struct db_full_path *pathp, struct rt_db_internal *ip,
	void *UNUSED(client_data))
{
    union tree *leaf = rt_booltree_leaf_tess(tsp, pathp, ip, NULL);
    if (!leaf && pathp) {
	char *path = db_path_to_string(pathp);
	bu_log("NMG leaf tessellation failed for '%s'\n",
		path ? path : "(unknown)");
	if (path)
	    bu_free(path, "NMG leaf path");
    }
    return leaf;
}

static int
facetize_nmg_compute(struct ged *gedp,
	const FacetizeWorkerRequest &request,
	FacetizeNmgBooleanResult *result)
{
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp)
	return BRLCAD_ERROR;

    struct db_tree_state initial_state;
    db_init_db_tree_state(&initial_state, gedp->dbip);
    initial_state.ts_ttol = &wdbp->wdb_ttol;
    initial_state.ts_tol = &wdbp->wdb_tol;

    struct model *nmg_model = nmg_mm();
    initial_state.ts_m = &nmg_model;
    FacetizeNmgBooleanState state;
    std::vector<const char *> input_argv;
    input_argv.reserve(request.input_names.size());
    for (const std::string &input_name : request.input_names)
	input_argv.push_back(input_name.c_str());

    int walk_result = BRLCAD_ERROR;
    if (!BU_SETJUMP) {
	walk_result = db_walk_tree(gedp->dbip, (int)input_argv.size(),
		input_argv.data(), 1, &initial_state, NULL,
		facetize_nmg_region_end, facetize_nmg_leaf, &state);
    } else {
	BU_UNSETJUMP;
	bu_log("NMG tree tessellation bombed while processing %s\n",
		request.output_name.c_str());
	return BRLCAD_ERROR;
    }
    BU_UNSETJUMP;

    if (walk_result < 0 || !state.tree) {
	if (state.tree)
	    db_free_tree(state.tree);
	nmg_km(nmg_model);
	return BRLCAD_ERROR;
    }

    int boolean_result = BRLCAD_ERROR;
    if (!BU_SETJUMP) {
	boolean_result = nmg_boolean(state.tree, nmg_model, &rt_vlfree,
		&wdbp->wdb_tol);
	if (boolean_result == 0) {
	    NMG_CK_REGION(state.tree->tr_d.td_r);
	    state.tree->tr_d.td_r = NULL;
	}
    } else {
	BU_UNSETJUMP;
	bu_log("NMG Boolean evaluation bombed while processing %s\n",
		request.output_name.c_str());
	return BRLCAD_ERROR;
    }
    BU_UNSETJUMP;

    if (boolean_result != 0) {
	db_free_tree(state.tree);
	nmg_km(nmg_model);
	return BRLCAD_ERROR;
    }
    db_free_tree(state.tree);

    if (request.operation == FacetizeWorkerOperation::NmgBooleanToNmg) {
	result->nmg_model = nmg_model;
	return BRLCAD_OK;
    }

    struct rt_bot_internal *bot = NULL;
    if (!BU_SETJUMP) {
	bot = static_cast<struct rt_bot_internal *>(
		nmg_mdl_to_bot(nmg_model, &rt_vlfree, &wdbp->wdb_tol));
    } else {
	BU_UNSETJUMP;
	bu_log("NMG-to-BoT conversion bombed while processing %s\n",
		request.output_name.c_str());
	return BRLCAD_ERROR;
    }
    BU_UNSETJUMP;

    nmg_km(nmg_model);
    if (!bot)
	return BRLCAD_ERROR;
    result->bot = bot;
    return BRLCAD_OK;
}

int
facetize_nmg_boolean_evaluate(struct ged *gedp,
	const FacetizeWorkerRequest &request,
	FacetizeNmgBooleanResult **result)
{
    if (!gedp || !result || !request.valid() ||
	    (request.operation != FacetizeWorkerOperation::NmgBooleanToBot &&
	     request.operation != FacetizeWorkerOperation::NmgBooleanToNmg))
	return BRLCAD_ERROR;
    *result = NULL;

    FacetizeNmgBooleanResult *evaluation =
	new FacetizeNmgBooleanResult;
    if (facetize_nmg_compute(gedp, request, evaluation) != BRLCAD_OK) {
	/* Bomb paths deliberately abandon unsafe NMG allocations until this
	 * one-shot worker exits. */
	facetize_nmg_boolean_destroy(evaluation);
	return BRLCAD_ERROR;
    }

    *result = evaluation;
    return BRLCAD_OK;
}

size_t
facetize_nmg_boolean_payload_size(
	const FacetizeNmgBooleanResult *UNUSED(result))
{
    /* NMG model storage is graph-shaped and has no cheap serialized-size
     * estimate.  The supervisor applies its conservative default write
     * timeout when the estimate is zero. */
    return 0;
}

int
facetize_nmg_boolean_write(struct db_i *dbip,
	FacetizeNmgBooleanResult *result)
{
    if (!dbip || !result || (!result->bot && !result->nmg_model))
	return BRLCAD_ERROR;

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = result->bot ? ID_BOT : ID_NMG;
    intern.idb_meth = &OBJ[intern.idb_type];
    intern.idb_ptr = result->bot ? static_cast<void *>(result->bot) :
	static_cast<void *>(result->nmg_model);
    bu_avs_init_empty(&intern.idb_avs);
    (void)bu_avs_add(&intern.idb_avs, "facetized", "1");
    (void)bu_avs_add(&intern.idb_avs, FACETIZE_METHOD_ATTR, "NMG Boolean");

    struct directory *output = db_diradd(dbip,
	    FACETIZE_WORKER_RESULT_OBJECT, RT_DIR_PHONY_ADDR, 0,
	    RT_DIR_SOLID, &intern.idb_type);
    if (!output) {
	rt_db_free_internal(&intern);
	result->bot = NULL;
	result->nmg_model = NULL;
	return BRLCAD_ERROR;
    }

    int write_result = rt_db_put_internal(output, dbip, &intern);
    result->bot = NULL;
    result->nmg_model = NULL;
    return write_result < 0 ? BRLCAD_ERROR : BRLCAD_OK;
}

void
facetize_nmg_boolean_destroy(FacetizeNmgBooleanResult *result)
{
    if (!result)
	return;
    if (result->bot) {
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = result->bot;
	rt_db_free_internal(&intern);
    }
    if (result->nmg_model)
	nmg_km(result->nmg_model);
    delete result;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
