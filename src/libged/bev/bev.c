/*                         B E V . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/bev.c
 *
 * The bev command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bu/parallel.h"
#include "rt/geom.h"

#include "../ged_private.h"
#include "ged/commands.h"


static union tree *bev_facetize_tree;
static struct model *bev_nmg_model;

struct bev_args {
    int triangulate;
};

static const struct bu_cmd_option bev_schema_options[] = {
    BU_CMD_FLAG("t", NULL, struct bev_args, triangulate, "Triangulate the result"),
    BU_CMD_OPTION_NULL
};
static const char * const bev_op_keywords[] = {"u", "-", "+", NULL};

static int
bev_op_validate(struct bu_vls *msg, const char *arg)
{
    if (db_str2op(arg) != DB_OP_NULL)
	return 0;
    if (msg)
	bu_vls_printf(msg, "invalid Boolean operation: %s\n", arg ? arg : "");
    return -1;
}

static const struct bu_cmd_operand bev_schema_operands[] = {
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 1, 1,
	"New evaluated object", NULL),
    GED_CMD_OPERAND_DB_OBJECT("input_object", 1, 1,
	"First input object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand bev_expression_roles[] = {
    BU_CMD_OPERAND_KEYWORDS_VALIDATE("operation", BU_CMD_VALUE_KEYWORD, 1, 1,
	bev_op_validate, "Boolean operation", NULL, bev_op_keywords),
    GED_CMD_OPERAND_DB_OBJECT("input_object", 1, 1,
	"Additional input object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand_group bev_schema_groups[] = {
    BU_CMD_OPERAND_GROUP("boolean_expression", bev_expression_roles, 0,
	BU_CMD_COUNT_UNLIMITED, "Additional operation/object pairs"),
    BU_CMD_OPERAND_GROUP_NULL
};

static const struct bu_cmd_schema bev_cmd_schema = {
    "bev", "Evaluate a boolean expression into an NMG object", bev_schema_options,
	bev_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
	BU_CMD_SCHEMA_GROUPS(NULL, NULL, bev_schema_groups)
};


static union tree *
bev_facetize_region_end(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, union tree *curtree, void *client_data)
{
    struct bu_list vhead;
    struct ged *gedp = (struct ged *)client_data;

    BU_LIST_INIT(&vhead);

    if (RT_G_DEBUG&RT_DEBUG_TREEWALK) {
	char *sofar = db_path_to_string(pathp);

	bu_vls_printf(gedp->ged_result_str, "bev_facetize_region_end() path='%s'\n", sofar);
	bu_free((void *)sofar, "path string");
    }

    if (curtree->tr_op == OP_NOP) return curtree;

    bu_semaphore_acquire(RT_SEM_MODEL);
    if (bev_facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = bev_facetize_tree;
	tr->tr_b.tb_right = curtree;
	bev_facetize_tree = tr;
    } else {
	bev_facetize_tree = curtree;
    }
    bu_semaphore_release(RT_SEM_MODEL);

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


int
ged_bev_core(struct ged *gedp, int argc, const char *argv[])
{

    int i;
    int ncpu;
    int operand_index;
    struct bev_args args = {0};
    const char *cmdname;
    char *newname;
    struct rt_db_internal intern;
    struct directory *dp;
    const char *opstr;
    int failed;
    struct bu_list *vlfree = &rt_vlfree;

    /* static due to longjmp */
    static int triangulate = 0;
    static union tree *tmp_tree = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, argv[0], argv[0]);
	return GED_HELP;
    }

    cmdname = argv[0];

    /* Initial values for options, must be reset each time */
    ncpu = 1;
    operand_index = bu_cmd_schema_parse_complete(&bev_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, cmdname, cmdname);
	return BRLCAD_ERROR;
    }
    triangulate = args.triangulate;
    argc -= operand_index + 1;
    argv += operand_index + 1;

    newname = (char *)argv[0];
    argv++;
    argc--;

    if (argc < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: Nothing to evaluate!!!\n", cmdname);
	return BRLCAD_ERROR;
    }

    GED_CHECK_EXISTS(gedp, newname, LOOKUP_QUIET, BRLCAD_ERROR);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    bu_vls_printf(gedp->ged_result_str,
		  "%s:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
		  argv[0],
		  wdbp->wdb_ttol.abs,
		  wdbp->wdb_ttol.rel,
		  wdbp->wdb_ttol.norm);

    bev_facetize_tree = (union tree *)0;
    bev_nmg_model = nmg_mm();
    wdbp->wdb_initial_tree_state.ts_m = &bev_nmg_model;

    opstr = NULL;
    tmp_tree = (union tree *)NULL;

    while (argc) {
	i = db_walk_tree(gedp->dbip, 1, (const char **)argv,
			 ncpu,
			 &wdbp->wdb_initial_tree_state,
			 0,			/* take all regions */
			 bev_facetize_region_end,
			 rt_booltree_leaf_tess,
			 (void *)gedp);

	if (i < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s: error in db_walk_tree()\n", cmdname);
	    /* Destroy NMG */
	    nmg_km(bev_nmg_model);
	    return BRLCAD_ERROR;
	}
	argc--;
	argv++;

	if (tmp_tree && opstr) {
	    union tree *new_tree;
	    db_op_t op = db_str2op(opstr);

	    BU_ALLOC(new_tree, union tree);
	    RT_TREE_INIT(new_tree);

	    new_tree->tr_b.tb_regionp = REGION_NULL;
	    new_tree->tr_b.tb_left = tmp_tree;
	    new_tree->tr_b.tb_right = bev_facetize_tree;

	    switch (op) {
		case DB_OP_UNION:
		    new_tree->tr_op = OP_UNION;
		    break;
		case DB_OP_SUBTRACT:
		    new_tree->tr_op = OP_SUBTRACT;
		    break;
		case DB_OP_INTERSECT:
		    new_tree->tr_op = OP_INTERSECT;
		    break;
		default: {
		    bu_vls_printf(gedp->ged_result_str, "%s: Unrecognized operator: (%c)\nAborting\n",
				  argv[0], opstr[0]);
		    db_free_tree(bev_facetize_tree);
		    nmg_km(bev_nmg_model);
		    return BRLCAD_ERROR;
		}
	    }

	    tmp_tree = new_tree;
	    bev_facetize_tree = (union tree *)NULL;
	} else if (!tmp_tree && !opstr) {
	    /* just starting out */
	    tmp_tree = bev_facetize_tree;
	    bev_facetize_tree = (union tree *)NULL;
	}

	if (argc) {
	    opstr = argv[0];
	    argc--;
	    argv++;
	} else
	    opstr = NULL;

    }

    if (tmp_tree) {
	/* Now, evaluate the boolean tree into ONE region */
	bu_vls_printf(gedp->ged_result_str, "%s: evaluating boolean expressions\n", cmdname);

	if (BU_SETJUMP) {
	    BU_UNSETJUMP;

	    bu_vls_printf(gedp->ged_result_str, "%s: WARNING: Boolean evaluation failed!!!\n", cmdname);
	    if (tmp_tree)
		db_free_tree(tmp_tree);
	    tmp_tree = (union tree *)NULL;
	    nmg_km(bev_nmg_model);
	    bev_nmg_model = (struct model *)NULL;
	    return BRLCAD_ERROR;
	}

	failed = nmg_boolean(tmp_tree, bev_nmg_model, vlfree, &wdbp->wdb_tol);
	BU_UNSETJUMP;
    } else
	failed = 1;

    if (failed) {
	bu_vls_printf(gedp->ged_result_str, "%s: no resulting region, aborting\n", cmdname);
	if (tmp_tree)
	    db_free_tree(tmp_tree);
	tmp_tree = (union tree *)NULL;
	nmg_km(bev_nmg_model);
	bev_nmg_model = (struct model *)NULL;
	return BRLCAD_ERROR;
    }
    /* New region remains part of this nmg "model" */
    NMG_CK_REGION(tmp_tree->tr_d.td_r);
    bu_vls_printf(gedp->ged_result_str, "%s: facetize %s\n", cmdname, tmp_tree->tr_d.td_name);

    nmg_vmodel(bev_nmg_model);

    /* Triangulate model, if requested */
    if (triangulate) {
	bu_vls_printf(gedp->ged_result_str, "%s: triangulating resulting object\n", cmdname);
	if (BU_SETJUMP) {
	    BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "%s: WARNING: Triangulation failed!!!\n", cmdname);
	    if (tmp_tree)
		db_free_tree(tmp_tree);
	    tmp_tree = (union tree *)NULL;
	    nmg_km(bev_nmg_model);
	    bev_nmg_model = (struct model *)NULL;
	    return BRLCAD_ERROR;
	}
	nmg_triangulate_model(bev_nmg_model, vlfree, &wdbp->wdb_tol);
	BU_UNSETJUMP;
    }

    bu_vls_printf(gedp->ged_result_str, "%s: converting NMG to database format\n", cmdname);

    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)bev_nmg_model;
    bev_nmg_model = (struct model *)NULL;

    GED_DB_DIRADD(gedp, dp, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    tmp_tree->tr_d.td_r = (struct nmgregion *)NULL;

    /* Free boolean tree, and the regions in it. */
    db_free_tree(tmp_tree);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_BEV_COMMANDS(X, XID) \
    X(bev, ged_bev_core, GED_CMD_DEFAULT, &bev_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_BEV_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_bev", 1, GED_BEV_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
