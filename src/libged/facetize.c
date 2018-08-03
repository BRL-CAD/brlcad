/*                         F A C E T I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2018 United States Government as represented by
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

#include "bu/cmd.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "analyze.h"
#include "./ged_private.h"

/* Sort the argv array to list existing objects first and everything else at
 * the end.  Returns the number of argv entries where db_lookup failed */
HIDDEN int
_ged_sort_existing_objs(struct ged *gedp, int argc, const char *argv[])
{
    int i = 0;
    int exist_cnt = 0;
    int nonexist_cnt = 0;
    const char **exists = (const char **)bu_calloc(argc, sizeof(const char *), "obj exists array");
    const char **nonexists = (const char **)bu_calloc(argc, sizeof(const char *), "obj nonexists array");
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    for (i = 0; i < argc; i++) {
	if (db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET) == RT_DIR_NULL) {
	    nonexists[nonexist_cnt] = argv[i];
	    nonexist_cnt++;
	} else {
	    exists[exist_cnt] = argv[i];
	    exist_cnt++;
	}
    }
    for (i = 0; i < exist_cnt; i++) {
	argv[i] = exists[i];
    }
    for (i = 0; i < nonexist_cnt; i++) {
	argv[i + exist_cnt] = nonexists[i];
    }

    return nonexist_cnt;
}

HIDDEN int
_ged_validate_objs_list(struct ged *gedp, int argc, const char *argv[], int newobj_cnt)
{
    int i;

    if (newobj_cnt < 1) {
	bu_vls_printf(gedp->ged_result_str, "all objects listed already exist, aborting.  (Need new object name to write out results to.)\n");
	return GED_ERROR;
    }

    if (newobj_cnt > 1) {
	bu_vls_printf(gedp->ged_result_str, "More than one object listed does not exist:\n");
	for (i = argc - newobj_cnt; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  Need to specify exactly one object name that does not exist to hold facetization output.\n");
	return GED_ERROR;
    }

    if (argc - newobj_cnt == 0) {
	bu_vls_printf(gedp->ged_result_str, "No existing objects specified, nothing to facetize.  Aborting.\n");
	return GED_ERROR;
    }
    return GED_OK;
}

struct _ged_facetize_opts {
    /* NMG specific options */
    int triangulate;
    int make_nmg;
    int marching_cube;
    int nmg_use_tnurbs;

    /* Poisson specific options */ 
    int pnt_surf_mode;
    int pnt_grid_mode;
    int pnt_rand_mode;
    int pnt_sobol_mode;
    int max_pnts;
    int max_time;
    fastf_t len_tol;
    struct bg_3d_spsr_opts s_opts;
};
#define _GED_SPSR_OPTS_INIT {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.0, BG_3D_SPSR_OPTS_DEFAULT}

#ifdef ENABLE_SPR
HIDDEN int
_ged_spsr_facetize(struct ged *gedp, int argc, const char *argv[], struct _ged_facetize_opts *opts)
{
    struct directory *dp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct rt_db_internal intern;
    struct rt_db_internal in_intern;
    struct bn_tol btol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot;
    struct pnt_normal *pn, *pl;
    int flags = 0;
    int newobj_cnt = 0;
    int i;
    char *newname;
    point_t *input_points_3d;
    vect_t *input_normals_3d;

    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv);
    if (_ged_validate_objs_list(gedp, argc, argv, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
    }

    if ((argc - newobj_cnt) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Screened Poisson mode (currently) only supports one existing object at a time as input.\n");
	return GED_ERROR;
    }

    newname = (char *)argv[argc-1];
    argc--;

    dp = db_lookup(dbip, argv[0], LOOKUP_QUIET);

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: could not determine type of object %s\n", argv[0]);
	return GED_ERROR;
    }

    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS) {
	/* If we have a point cloud, there's no need to raytrace it */
	pnts = (struct rt_pnts_internal *)in_intern.idb_ptr;
    } else {
	BU_ALLOC(pnts, struct rt_pnts_internal);
	pnts->magic = RT_PNTS_INTERNAL_MAGIC;
	pnts->scale = 0.0;
	pnts->type = RT_PNT_TYPE_NRM;

	/* Pick our mode(s) */
	if (!opts->pnt_grid_mode && !opts->pnt_rand_mode && !opts->pnt_sobol_mode) {
	    flags |= ANALYZE_OBJ_TO_PNTS_GRID;
	} else {
	    if (opts->pnt_grid_mode)  flags |= ANALYZE_OBJ_TO_PNTS_GRID;
	    if (opts->pnt_rand_mode)  flags |= ANALYZE_OBJ_TO_PNTS_RAND;
	    if (opts->pnt_sobol_mode) flags |= ANALYZE_OBJ_TO_PNTS_SOBOL;
	}

	/* If we don't have a tolerance, try to guess something sane from the bbox */
	if (NEAR_ZERO(opts->len_tol, RT_LEN_TOL)) {
	    point_t rpp_min, rpp_max;
	    point_t obj_min, obj_max;
	    VSETALL(rpp_min, INFINITY);
	    VSETALL(rpp_max, -INFINITY);
	    ged_get_obj_bounds(gedp, 1, (const char **)&(argv[0]), 0, obj_min, obj_max);
	    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	    opts->len_tol = DIST_PT_PT(rpp_max, rpp_min) * 0.01;
	    bu_log("Note - no tolerance specified, using %f\n", opts->len_tol);
	}
	btol.dist = opts->len_tol;

	if (analyze_obj_to_pnts(pnts, gedp->ged_wdbp->dbip, argv[0], &btol, flags, opts->max_pnts, opts->max_time)) {
	    bu_vls_sprintf(gedp->ged_result_str, "Error: point generation failed\n");
	    bu_free(pnts, "free pnts");
	    return GED_ERROR;
	}
    }

    input_points_3d = (point_t *)bu_calloc(pnts->count, sizeof(point_t), "points");
    input_normals_3d = (vect_t *)bu_calloc(pnts->count, sizeof(vect_t), "normals");
    i = 0;
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMOVE(input_points_3d[i], pn->v);
	VMOVE(input_normals_3d[i], pn->n);
	i++;
    }

    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;

    if (bg_3d_spsr(&(bot->faces), (int *)&(bot->num_faces),
		(point_t **)&(bot->vertices),
		(int *)&(bot->num_vertices),
		(const point_t *)input_points_3d,
		(const vect_t *)input_normals_3d,
		pnts->count, &(opts->s_opts)) ) {
	bu_vls_sprintf(gedp->ged_result_str, "Error: Screened Poisson reconstruction failed\n");
	bu_free(input_points_3d, "3d pnts");
	bu_free(input_normals_3d, "3d pnts");
	rt_db_free_internal(&in_intern);
	return GED_ERROR;
    }

    /* Export BOT as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *) bot;

    bu_free(input_points_3d, "3d pnts");
    bu_free(input_normals_3d, "3d pnts");
    rt_db_free_internal(&in_intern);

    dp=db_diradd(dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Cannot add %s to directory\n", newname);
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Failed to write %s to database\n", newname);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    return GED_OK;
}
#endif

static union tree *
facetize_region_end(struct db_tree_state *tsp,
		    const struct db_full_path *pathp,
		    union tree *curtree,
		    void *client_data)
{
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    facetize_tree = (union tree **)client_data;

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
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
_ged_nmg_facetize(struct ged *gedp, int argc, const char **argv, struct _ged_facetize_opts *opts)
{
    int i;
    int newobj_cnt;
    int failed;
    char *newname;
    struct db_tree_state init_state;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct rt_db_internal intern;
    struct directory *dp;
    union tree *facetize_tree;
    struct model *nmg_model;
    int nmg_use_tnurbs = opts->nmg_use_tnurbs;
    /* static due to jumping */
    static int triangulate;
    static int make_nmg;
    static int marching_cube;
    triangulate = opts->triangulate;
    make_nmg = opts->make_nmg;
    marching_cube = opts->marching_cube;

    RT_CHECK_DBI(dbip);

    db_init_db_tree_state(&init_state, dbip, gedp->ged_wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &gedp->ged_wdbp->wdb_ttol;
    init_state.ts_tol = &gedp->ged_wdbp->wdb_tol;

    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "facetize: missing argument\n");
	return GED_ERROR;
    }

    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv);
    if (_ged_validate_objs_list(gedp, argc, argv, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
    }

    newname = (char *)argv[argc-1];
    argc--;

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
	    (void *)&facetize_tree
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
	    failed = nmg_boolean(facetize_tree, nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol, &rt_uniresource);
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
    if (triangulate && make_nmg) {
	bu_vls_printf(gedp->ged_result_str, "facetize:  triangulating resulting object\n");
	if (!BU_SETJUMP) {
	    /* try */
	    if (marching_cube == 1)
		nmg_triangulate_model_mc(nmg_model, &gedp->ged_wdbp->wdb_tol);
	    else
		nmg_triangulate_model(nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol);
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

    if (!make_nmg) {
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
	    bot = (struct rt_bot_internal *)nmg_bot(s, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol);
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
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *) bot;
    } else {

	bu_vls_printf(gedp->ged_result_str, "facetize:  converting NMG to database format\n");

	/* Export NMG as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_NMG;
	intern.idb_meth = &OBJ[ID_NMG];
	intern.idb_ptr = (void *)nmg_model;
	nmg_model = (struct model *)NULL;
    }

    dp=db_diradd(dbip, newname, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
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

int
ged_facetize(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "Usage: facetize [ -nmhtT | [--poisson] ] [old_obj1 | new_obj] [old_obj* ...] [old_objN | new_obj]\n";
    static const char *pusage = "Usage: facetize --poisson [-d #] [-w #] [ray sampling options] old_obj new_obj\n";

    int print_help;
    int screened_poisson = 0;
    struct _ged_facetize_opts opts = _GED_SPSR_OPTS_INIT;
    struct bu_opt_desc d[7];
    struct bu_opt_desc pd[10];

    BU_OPT(d[0], "h", "help",            "",  NULL,  &print_help,       "Print help and exit");
    BU_OPT(d[1], "m", "marching-cube",   "",  NULL,  &(opts.marching_cube),    "Use the raytraced points and marching cube algorithm to construct an NMG");
    BU_OPT(d[2], "",  "poisson",         "",  NULL,  &screened_poisson, "Use raytraced points and SPSR - run -h --poisson to see more options for this mode");
    BU_OPT(d[3], "n", "NMG",             "",  NULL,  &(opts.make_nmg),         "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh)");
    BU_OPT(d[4], "t", "TNURB",           "",  NULL,  &(opts.nmg_use_tnurbs),   "Create TNURB faces rather than planar approximations (experimental)");
    BU_OPT(d[5], "T", "triangles",       "",  NULL,  &(opts.triangulate),      "Generate a mesh using only triangles");
    BU_OPT_NULL(d[6]);

    /* Poisson specific options */
    BU_OPT(pd[0], "d", "depth",       "",  NULL,            &(opts.s_opts.depth),         "Maximum reconstruction depth");
    BU_OPT(pd[1], "w", "interpolate", "",  NULL,            &(opts.s_opts.point_weight),  "Weights importance of interpolation (value significance - NEED TO EXPLAIN)");
    BU_OPT(pd[2], "t", "tolerance",   "#", &bu_opt_fastf_t, &(opts.len_tol),        "Specify sampling grid spacing (in mm).");
    BU_OPT(pd[3], "",  "surface",     "",  NULL,            &(opts.pnt_surf_mode),  "Save only first and last points along ray.");
    BU_OPT(pd[4], "",  "grid",        "",  NULL,            &(opts.pnt_grid_mode),  "Sample using a gridded ray pattern (default).");
    BU_OPT(pd[5], "",  "rand",        "",  NULL,            &(opts.pnt_rand_mode),  "Sample using a random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(pd[6], "",  "sobol",       "",  NULL,            &(opts.pnt_sobol_mode), "Sample using a Sobol pseudo-random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(pd[7], "",  "max-pnts",    "",  &bu_opt_int,     &(opts.max_pnts),       "Maximum number of pnts to return per non-grid sampling method.");
    BU_OPT(pd[8], "",  "max-time",    "",  &bu_opt_int,     &(opts.max_time),       "Maximum time to spend per-method (in seconds) when using non-grid sampling.");
    BU_OPT_NULL(pd[9]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (!screened_poisson) {

	/* must be wanting help */
	if (print_help || argc < 2) {
	    _ged_cmd_help(gedp, usage, d);
	    return GED_OK;
	}

	if (opts.triangulate && opts.nmg_use_tnurbs) {
	    bu_vls_printf(gedp->ged_result_str, "both -T and -t specified!\n");
	    return GED_ERROR;
	}
	return _ged_nmg_facetize(gedp, argc, argv, &opts);

    } else {

	if (argc < 2 || print_help || opts.make_nmg || opts.marching_cube || opts.nmg_use_tnurbs) {
	    if (print_help || argc < 2) {
		_ged_cmd_help(gedp, pusage, pd);
		return GED_OK;
	    }
	    if (opts.make_nmg || opts.nmg_use_tnurbs) {
		bu_vls_printf(gedp->ged_result_str, "Screened Poisson reconstruction only supports BoT output currently\n");
		return GED_ERROR;
	    }
	    if (opts.marching_cube) {
		bu_vls_printf(gedp->ged_result_str, "multiple reconstruction methods (Marching Cubes and Screened Poisson) specified\n");
		return GED_ERROR;
	    }
	}

	/* Parse Poisson specific options */
	argc = bu_opt_parse(NULL, argc, argv, d);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    return GED_ERROR;
	}

#ifdef ENABLE_SPR
	return _ged_spsr_facetize(gedp, argc, argv, &opts);
#else
	bu_vls_printf(gedp->ged_result_str, "Screened Poisson support was not enabled for this build.  To test, pass -DBRLCAD_ENABLE_SPR=ON to the cmake configure.\n");
	return GED_ERROR;
#endif

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
