/*                         F A C E T I Z E . C
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
/** @file libged/facetize.c
 *
 * The facetize command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"
#include "raytrace.h"

#include "./ged_private.h"


static union tree *
facetize_region_end(struct db_tree_state *tsp,
		    const struct db_full_path *pathp,
		    union tree *curtree,
		    genptr_t client_data)
{
    struct bu_list vhead;
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    facetize_tree = (union tree **)client_data;
    BU_LIST_INIT(&vhead);

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_GET(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


int
ged_facetize(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int c;
    char *newname;
    struct rt_db_internal intern;
    struct directory *dp;
    int failed;
    int nmg_use_tnurbs = 0;
    struct db_tree_state init_state;
    struct db_i *dbip;
    union tree *facetize_tree;
    struct model *nmg_model;

    static const char *usage = "[-n] [-t] [-T] new_obj old_obj [old_obj2 old_obj3 ...]";

    /* static due to jumping */
    static int triangulate;
    static int make_bot;
    static int marching_cube;

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

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    dbip = gedp->ged_wdbp->dbip;
    RT_CHECK_DBI(dbip);

    db_init_db_tree_state(&init_state, dbip, gedp->ged_wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &gedp->ged_wdbp->wdb_ttol;
    init_state.ts_tol = &gedp->ged_wdbp->wdb_tol;

    /* Initial vaues for options, must be reset each time */
    marching_cube = 0;
    triangulate = 0;
    make_bot = 1;

    /* Parse options. */
    bu_optind = 1;		/* re-init bu_getopt() */
    while ((c=bu_getopt(argc, (char * const *)argv, "mntT")) != -1) {
	switch (c) {
	    case 'm':
		marching_cube = triangulate = 1;
		/* no break, marching cubes assumes nmg for now */
	    case 'n':
		make_bot = 0;
		break;
	    case 'T':
		triangulate = 1;
		break;
	    case 't':
		nmg_use_tnurbs = 1;
		break;
	    default: {
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		return GED_ERROR;
	    }
	}
    }
    argc -= bu_optind;
    argv += bu_optind;
    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "facetize: missing argument\n");
	return GED_ERROR;
    }

    newname = (char *)argv[0];
    argv++;
    argc--;
    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "facetize: missing argument\n");
	return GED_ERROR;
    }

    if (db_lookup(dbip, newname, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "error: solid '%s' already exists, aborting\n", newname);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str,
		  "facetize:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
		  gedp->ged_wdbp->wdb_ttol.abs, gedp->ged_wdbp->wdb_ttol.rel, gedp->ged_wdbp->wdb_ttol.norm);

    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    i = db_walk_tree(dbip, argc, (const char **)argv,
		     1,
		     &init_state,
		     0,			/* take all regions */
		     facetize_region_end,
		     nmg_use_tnurbs ?
		     nmg_booltree_leaf_tnurb :
		     nmg_booltree_leaf_tess,
		     (genptr_t)&facetize_tree
	);


    if (i < 0) {
	bu_vls_printf(gedp->ged_result_str, "facetize: error in db_walk_tree()\n");
	/* Destroy NMG */
	nmg_km(nmg_model);
	return GED_ERROR;
    }

    if (facetize_tree) {
	/* Now, evaluate the boolean tree into ONE region */
	bu_vls_printf(gedp->ged_result_str, "facetize:  evaluating boolean expressions\n");

	if (!BU_SETJUMP) {
	    /* try */
	    failed = nmg_boolean(facetize_tree, nmg_model, &gedp->ged_wdbp->wdb_tol, &rt_uniresource);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "WARNING: facetization failed!!!\n");
	    db_free_tree(facetize_tree, &rt_uniresource);
	    facetize_tree = (union tree *)NULL;
	    nmg_km(nmg_model);
	    nmg_model = (struct model *)NULL;
	    return GED_ERROR;
	} BU_UNSETJUMP;

    } else
	failed = 1;

    if (failed) {
	bu_vls_printf(gedp->ged_result_str, "facetize:  no resulting region, aborting\n");
	db_free_tree(facetize_tree, &rt_uniresource);
	facetize_tree = (union tree *)NULL;
	nmg_km(nmg_model);
	nmg_model = (struct model *)NULL;
	return GED_ERROR;
    }
    /* New region remains part of this nmg "model" */
    NMG_CK_REGION(facetize_tree->tr_d.td_r);
    bu_vls_printf(gedp->ged_result_str, "facetize:  %s\n", facetize_tree->tr_d.td_name);

    /* Triangulate model, if requested */
    if (triangulate && !make_bot) {
	bu_vls_printf(gedp->ged_result_str, "facetize:  triangulating resulting object\n");
	if (!BU_SETJUMP) {
	    /* try */
	    if (marching_cube == 1)
		nmg_triangulate_model_mc(nmg_model, &gedp->ged_wdbp->wdb_tol);
	    else
		nmg_triangulate_model(nmg_model, &gedp->ged_wdbp->wdb_tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "WARNING: triangulation failed!!!\n");
	    db_free_tree(facetize_tree, &rt_uniresource);
	    facetize_tree = (union tree *)NULL;
	    nmg_km(nmg_model);
	    nmg_model = (struct model *)NULL;
	    return GED_ERROR;
	} BU_UNSETJUMP;
    }

    if (make_bot) {
	struct rt_bot_internal *bot;
	struct nmgregion *r;
	struct shell *s;

	bu_vls_printf(gedp->ged_result_str, "facetize:  converting to BOT format\n");

	/* WTF, FIXME: this is only dumping the first shell of the first region */

	r = BU_LIST_FIRST(nmgregion, &nmg_model->r_hd);
	if (r && BU_LIST_NEXT(nmgregion, &r->l) !=  (struct nmgregion *)&nmg_model->r_hd)
	    bu_vls_printf(gedp->ged_result_str, "WARNING: model has more than one region, only facetizing the first\n");

	s = BU_LIST_FIRST(shell, &r->s_hd);
	if (s && BU_LIST_NEXT(shell, &s->l) != (struct shell *)&r->s_hd)
	    bu_vls_printf(gedp->ged_result_str, "WARNING: model has more than one shell, only facetizing the first\n");

	if (!BU_SETJUMP) {
	    /* try */
	    bot = (struct rt_bot_internal *)nmg_bot(s, &gedp->ged_wdbp->wdb_tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "WARNING: conversion to BOT failed!\n");
	    db_free_tree(facetize_tree, &rt_uniresource);
	    facetize_tree = (union tree *)NULL;
	    nmg_km(nmg_model);
	    nmg_model = (struct model *)NULL;
	    return GED_ERROR;
	} BU_UNSETJUMP;

	nmg_km(nmg_model);
	nmg_model = (struct model *)NULL;

	/* Export BOT as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &rt_functab[ID_BOT];
	intern.idb_ptr = (genptr_t) bot;
    } else {

	bu_vls_printf(gedp->ged_result_str, "facetize:  converting NMG to database format\n");

	/* Export NMG as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_NMG;
	intern.idb_meth = &rt_functab[ID_NMG];
	intern.idb_ptr = (genptr_t)nmg_model;
	nmg_model = (struct model *)NULL;
    }

    dp=db_diradd(dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", newname);
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Failed to write %s to database\n", newname);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;

    /* Free boolean tree, and the regions in it */
    db_free_tree(facetize_tree, &rt_uniresource);
    facetize_tree = (union tree *)NULL;

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
