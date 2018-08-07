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
#include "rt/op.h"
#include "rt/search.h"
#include "raytrace.h"
#include "analyze.h"
#include "wdb.h"
#include "./ged_private.h"

/* Sort the argv array to list existing objects first and everything else at
 * the end.  Returns the number of argv entries where db_lookup failed */
HIDDEN int
_ged_sort_existing_objs(struct ged *gedp, int argc, const char *argv[], struct directory **dpa)
{
    int i = 0;
    int exist_cnt = 0;
    int nonexist_cnt = 0;
    struct directory *dp;
    const char **exists = (const char **)bu_calloc(argc, sizeof(const char *), "obj exists array");
    const char **nonexists = (const char **)bu_calloc(argc, sizeof(const char *), "obj nonexists array");
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    for (i = 0; i < argc; i++) {
	dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    nonexists[nonexist_cnt] = argv[i];
	    nonexist_cnt++;
	} else {
	    exists[exist_cnt] = argv[i];
	    if (dpa) dpa[exist_cnt] = dp;
	    exist_cnt++;
	}
    }
    for (i = 0; i < exist_cnt; i++) {
	argv[i] = exists[i];
    }
    for (i = 0; i < nonexist_cnt; i++) {
	argv[i + exist_cnt] = nonexists[i];
    }

    bu_free(exists, "exists array");
    bu_free(nonexists, "nonexists array");

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

HIDDEN db_op_t
_int_to_opt(int op)
{
    if (op == 2) return DB_OP_UNION;
    if (op == 3) return DB_OP_INTERSECT;
    if (op == 4) return DB_OP_SUBTRACT;
    return DB_OP_NULL;
}

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

HIDDEN struct model *
_try_nmg_facetize(struct ged *gedp, int argc, const char **argv, int nmg_use_tnurbs)
{
    int i;
    int failed = 0;
    struct db_tree_state init_state;
    union tree *facetize_tree;
    struct model *nmg_model;

    db_init_db_tree_state(&init_state, gedp->ged_wdbp->dbip, gedp->ged_wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &gedp->ged_wdbp->wdb_ttol;
    init_state.ts_tol = &gedp->ged_wdbp->wdb_tol;

    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    i = db_walk_tree(gedp->ged_wdbp->dbip, argc, (const char **)argv,
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
	/* Destroy NMG */
	nmg_km(nmg_model);
	return NULL;
    }

    if (facetize_tree) {
	if (!BU_SETJUMP) {
	    /* try */
	    failed = nmg_boolean(facetize_tree, nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol, &rt_uniresource);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    db_free_tree(facetize_tree, &rt_uniresource);
	    nmg_km(nmg_model);
	    return NULL;
	} BU_UNSETJUMP;

    } else {
	failed = 1;
    }

    if (!failed && facetize_tree) {
	NMG_CK_REGION(facetize_tree->tr_d.td_r);
	bu_vls_printf(gedp->ged_result_str, "facetize:  %s\n", facetize_tree->tr_d.td_name);
	facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;
    }

    if (facetize_tree) {
	db_free_tree(facetize_tree, &rt_uniresource);
    }

    return (failed) ? NULL : nmg_model;
}


struct _ged_facetize_opts {
    /* NMG specific options */
    int triangulate;
    int make_nmg;
    int nmgbool;
    int screened_poisson;
    int marching_cube;
    int nmg_use_tnurbs;
    int regions;

    /* Poisson specific options */ 
    int pnt_surf_mode;
    int pnt_grid_mode;
    int pnt_rand_mode;
    int pnt_sobol_mode;
    int max_pnts;
    int max_time;
    fastf_t len_tol;
    struct bu_vls *faceted_suffix;
    struct bg_3d_spsr_opts s_opts;
};

struct _ged_facetize_opts * _ged_facetize_opts_create()
{
    struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    struct _ged_facetize_opts *o = NULL;
    BU_GET(o, struct _ged_facetize_opts);
    o->triangulate = 0;
    o->make_nmg = 0;
    o->nmgbool = 0;
    o->marching_cube = 0;
    o->screened_poisson = 0;
    o->nmg_use_tnurbs = 0;
    o->regions = 0;
    BU_GET(o->faceted_suffix, struct bu_vls);
    bu_vls_init(o->faceted_suffix);
    bu_vls_sprintf(o->faceted_suffix, ".bot");

    o->pnt_surf_mode = 0;
    o->pnt_grid_mode = 0;
    o->pnt_rand_mode = 0;
    o->pnt_sobol_mode = 0;
    o->max_pnts = 0;
    o->max_time = 0;
    o->len_tol = 0.0;
    o->s_opts = s_opts;
    return o;
}
void _ged_facetize_opts_destroy(struct _ged_facetize_opts *o)
{
    if (!o) return;
    if (o->faceted_suffix) {
	bu_vls_free(o->faceted_suffix);
	BU_PUT(o->faceted_suffix, struct bu_vls);
    }
    BU_PUT(o, struct _ged_facetize_opts);
}


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
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
    int flags = 0;
    int newobj_cnt = 0;
    int i;
    char *newname;
    point_t *input_points_3d;
    vect_t *input_normals_3d;

    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, NULL);
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

    if (!opts->make_nmg) {
	/* Export BoT as a new solid */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_meth = &OBJ[ID_BOT];
	intern.idb_ptr = (void *) bot;

	bu_free(input_points_3d, "3d pnts");
	bu_free(input_normals_3d, "3d pnts");
	rt_db_free_internal(&in_intern);

    } else {
	/* Convert BoT to NMG */
	struct model *m = nmg_mm();
	struct nmgregion *r;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;

	/* Use our new intern to fake out rt_bot_tess, since it expects an
	 * rt_db_internal wrapper */
	intern.idb_type = ID_BOT;
	intern.idb_ptr = (void *)bot;
	if (rt_bot_tess(&r, m, &intern, NULL, &btol) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to convert Bot to NMG\n");
	    rt_db_free_internal(&intern);
	} else {
	    /* OK,have NMG now - write it out */
	    intern.idb_type = ID_NMG;
	    intern.idb_meth = &OBJ[ID_NMG];
	    intern.idb_ptr = (void *)m;
	}
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

    return GED_OK;
}
#endif

int
_ged_nmg_facetize(struct ged *gedp, int argc, const char **argv, struct _ged_facetize_opts *opts)
{
    int newobj_cnt;
    char *newname;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct rt_db_internal intern;
    struct directory *dp;
    struct model *nmg_model = NULL;
    struct rt_bot_internal *bot = NULL;
    /* static due to jumping */
    static int triangulate;
    static int make_nmg;
    static int marching_cube;
    triangulate = opts->triangulate;
    make_nmg = opts->make_nmg;
    marching_cube = opts->marching_cube;

    RT_CHECK_DBI(dbip);

    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "facetize: missing argument\n");
	return GED_ERROR;
    }

    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, NULL);
    if (_ged_validate_objs_list(gedp, argc, argv, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
    }

    newname = (char *)argv[argc-1];
    argc--;

    bu_vls_printf(gedp->ged_result_str,
	    "facetize:  tessellating primitives with tolerances a=%g, r=%g, n=%g\n",
	    gedp->ged_wdbp->wdb_ttol.abs, gedp->ged_wdbp->wdb_ttol.rel, gedp->ged_wdbp->wdb_ttol.norm);

    nmg_model = _try_nmg_facetize(gedp, argc, argv, opts->nmg_use_tnurbs);
    if (nmg_model == NULL) {
	bu_vls_printf(gedp->ged_result_str, "facetize:  no resulting region, aborting\n");
	return GED_ERROR;
    }

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
	    nmg_km(nmg_model);
	    return GED_ERROR;
	} BU_UNSETJUMP;
    }

    if (!make_nmg) {
	if (!BU_SETJUMP) {
	    /* try */
	    bot = (struct rt_bot_internal *)nmg_mdl_to_bot(nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    bu_vls_printf(gedp->ged_result_str, "WARNING: conversion to BOT failed!\n");
	    nmg_km(nmg_model);
	    return GED_ERROR;
	} BU_UNSETJUMP;

	nmg_km(nmg_model);

	if (!bot) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: conversion to BOT failed!\n");
	    return GED_ERROR;
	}

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

    return GED_OK;
}

int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct ged *gedp = (struct ged *)data;
    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

int
_ged_facetize_cpcomb(struct ged *gedp, const char *o, struct bu_attribute_value_set *nmap)
{
    int ret = GED_OK;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct directory *dp;
    struct rt_db_internal ointern, intern;
    struct rt_comb_internal *ocomb, *comb;
    struct bu_attribute_value_set avs;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    const char *fname;
    int flags;

    /* Unpack original comb to get info to duplicate in new comb */
    dp = db_lookup(dbip, o, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB)) return GED_ERROR;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp)) return GED_ERROR;
    if (rt_db_get_internal(&ointern, dp, dbip, NULL, &rt_uniresource) < 0) return GED_ERROR;
    ocomb = (struct rt_comb_internal *)ointern.idb_ptr;
    RT_CK_COMB(ocomb);
    flags = dp->d_flags;

    /* Get new name */
    bu_vls_sprintf(&tmp, "%s", o);
    bu_vls_incr(&tmp, NULL, "0:0:0:0:-", &_db_uniq_test, (void *)gedp);


    /* Make a new empty comb with the same properties as the original, sans tree */
    RT_DB_INTERNAL_INIT(&intern);
    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_ptr = (void *)comb;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    GED_DB_DIRADD(gedp, dp, bu_vls_addr(&tmp), -1, 0, flags, (void *)&intern.idb_type, 0);
    comb->region_flag = ocomb->region_flag;
    bu_vls_vlscat(&comb->shader, &ocomb->shader);
    comb->rgb_valid = ocomb->rgb_valid;
    comb->rgb[0] = ocomb->rgb[0];
    comb->rgb[1] = ocomb->rgb[1];
    comb->rgb[2] = ocomb->rgb[2];
    comb->region_id = ocomb->region_id;
    comb->aircode = ocomb->aircode;
    comb->GIFTmater = ocomb->GIFTmater;
    comb->los = ocomb->los;
    comb->inherit = ocomb->inherit;
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, 0);

    /* apply attributes to new comb */
    db5_update_attributes(dp, &avs, dbip);

    /* If we already have a matching object name in the map, add it to the comb - the
     * comb is going to take over now as the "representative" object for the original
     * in the map. */
    fname = bu_avs_get(nmap, o);
    if (fname) {
	ret = _ged_combadd2(gedp, bu_vls_addr(&tmp), 1, (const char **)&fname, 0, DB_OP_UNION, 0, 0, NULL, 0);
    }

    /* Update the map */
    if (!bu_avs_add(nmap, o, bu_vls_addr(&tmp))) {
	return GED_ERROR;
    }
    return ret;
}

int
_ged_facetize_regions(struct ged *gedp, int argc, const char **argv, struct _ged_facetize_opts *opts)
{
    char *newname = NULL;
    int newobj_cnt = 0;
    int ret = GED_OK;
    unsigned int i = 0;
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    struct bu_attribute_value_set nmap = BU_AVS_INIT_ZERO;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    const char **ntop;
    /* We need to copy combs above regions that are not themselves regions.
     * Also, facetize will need all "active" regions that will define shapes.
     * Construct searches to get these sets. */
    struct bu_ptbl *pc = NULL;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl *ar_failed_nmg = NULL;
    const char *preserve_combs = "-type c ! -type r ! -below -type r";
    const char *active_regions = "-type r ! -below -type r";

    if (argc) dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(gedp, argc, argv, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
    }

    newname = (char *)argv[argc-1];
    argc--;


    BU_ALLOC(pc, struct bu_ptbl);
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, preserve_combs, newobj_cnt, dpa, dbip, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "problem searching for parent combs - aborting.\n");
	ret = GED_ERROR;
	goto ged_facetize_regions_memfree;
    }
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, newobj_cnt, dpa, dbip, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "problem searching for active regions - aborting.\n");
	ret = GED_ERROR;
	goto ged_facetize_regions_memfree;
    }

    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (unlikely but technically possible), nothing to do */
	ret = GED_OK;
	goto ged_facetize_regions_memfree;
    }

    /* TODO - someday this object-level facetization should be done in
     * parallel, but that's one deep rabbit hole - for now, just try them in
     * order and make sure we can handle (non-crashing) failures to convert
     * sanely. */
    BU_ALLOC(ar_failed_nmg, struct bu_ptbl);
    bu_ptbl_init(ar_failed_nmg, 64, "failed list init");
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	const char *nargv[2];
	const char *narg;
	bu_vls_sprintf(&tmp, "%s%s", n->d_namep, bu_vls_addr(opts->faceted_suffix));
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&tmp), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_incr(&tmp, NULL, "0:0:0:0:-", &_db_uniq_test, (void *)gedp);
	}
	bu_avs_add(&nmap, n->d_namep, bu_vls_addr(&tmp));
	narg = bu_avs_get(&nmap, n->d_namep);
	nargv[0] = n->d_namep;
	nargv[1] = narg;

	bu_vls_printf(gedp->ged_result_str, "NMG tessellating %s from %s\n", narg, n->d_namep);
	if (_ged_nmg_facetize(gedp, 2, nargv, opts) != GED_OK) {
	    bu_ptbl_ins(ar_failed_nmg, (long *)narg);
	} else {
	    if (_ged_facetize_cpcomb(gedp, n->d_namep, &nmap) != GED_OK) {
		bu_vls_printf(gedp->ged_result_str, "Error creating comb for %s \n", n->d_namep);
	    }
	}
    }

    /* For all the pc combs, make new versions with the suffixed names */
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);
	if (_ged_facetize_cpcomb(gedp, n->d_namep, &nmap) != GED_OK) {
	    bu_vls_printf(gedp->ged_result_str, "Error creating comb for %s \n", n->d_namep);
	}
	bu_vls_printf(gedp->ged_result_str, "Making new comb %s from %s\n", bu_avs_get(&nmap, n->d_namep), n->d_namep);
    }

    /* For all the pc combs, add the members from the map with the settings from the
     * original comb */
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	int j = 0;
	struct rt_db_internal intern;
	struct directory *cdp = RT_DIR_NULL;
	struct directory **children = NULL;
	struct rt_comb_internal *comb = NULL;
	int *bool_ops = NULL;
	matp_t *mats = NULL;
	const char *nparent;
	RT_DB_INTERNAL_INIT(&intern);
	cdp = (struct directory *)BU_PTBL_GET(pc, i);
	nparent = bu_avs_get(&nmap, cdp->d_namep);
	if (rt_db_get_internal(&intern, cdp, dbip, NULL, &rt_uniresource) < 0) {
	    ret = GED_ERROR;
	    goto ged_facetize_regions_memfree;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if (db_comb_children(dbip, comb, &children, &bool_ops, &mats) < 0) {
	    ret = GED_ERROR;
	    goto ged_facetize_regions_memfree;
	}
	j = 0;
	cdp = children[0];
	while (cdp != RT_DIR_NULL) {
	    const char *nc = bu_avs_get(&nmap, cdp->d_namep);
	    matp_t m = (mats[j]) ? mats[j] : NULL;
	    ret = _ged_combadd2(gedp, (char *)nparent, 1, (const char **)&nc, 0, _int_to_opt(bool_ops[j]), 0, 0, m, 0);
	    j++;
	    cdp = children[j];
	}

	j = 0;
	while (mats[j]) {
	    bu_free(mats[j], "free matrix");
	    j++;
	}
	bu_free(mats, "free mats array");
	bu_free(bool_ops, "free ops");
	bu_free(children, "free children struct directory ptr array");
    }

    /* Last one - add the new toplevel comb to hold all the new geometry */
    ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
    for (i = 0; i < (unsigned int)argc; i++) {
	ntop[i] = bu_avs_get(&nmap, argv[i]);
    }
    ret = _ged_combadd2(gedp, newname, argc, ntop, 0, DB_OP_UNION, 0, 0, NULL, 0);

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

ged_facetize_regions_memfree:
    bu_ptbl_free(pc);
    bu_ptbl_free(ar);
    bu_free(pc, "pc table");
    bu_free(ar, "ar table");
    return ret;
}


int
ged_facetize(struct ged *gedp, int argc, const char *argv[])
{
    int ret = GED_OK;
    static const char *usage = "Usage: facetize [ -nmhT | [--poisson] ] [old_obj1 | new_obj] [old_obj* ...] [old_objN | new_obj]\n";
    static const char *pusage = "Usage: facetize --poisson [-d #] [-w #] [ray sampling options] old_obj new_obj\n";

    int print_help = 0;
    struct _ged_facetize_opts *opts = _ged_facetize_opts_create();
    struct bu_opt_desc d[9];
    struct bu_opt_desc pd[11];

    BU_OPT(d[0], "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1], "",  "nmgbool",       "",  NULL,  &(opts->nmgbool),          "Use the standard libnmg boolean mesh evaluation to create output (Default)");
    BU_OPT(d[2], "m", "marching-cube", "",  NULL,  &(opts->marching_cube),    "Use the raytraced points and marching cube algorithm to create output");
    BU_OPT(d[3], "",  "poisson",       "",  NULL,  &(opts->screened_poisson), "Use raytraced points and SPSR to create output - run -h --poisson to see more options for this mode");
    BU_OPT(d[4], "n", "NMG",           "",  NULL,  &(opts->make_nmg),         "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh)");
    BU_OPT(d[5], "",  "TNURB",         "",  NULL,  &(opts->nmg_use_tnurbs),   "Create TNURB faces rather than planar approximations (experimental)");
    BU_OPT(d[6], "T", "triangles",     "",  NULL,  &(opts->triangulate),      "Generate a NMG solid using only triangles (BoTs, the default output, can only use triangles - this option mimics that behavior for NMG output.)");
    BU_OPT(d[7], "r", "regions",       "",  NULL,  &(opts->regions),          "For combs, walk the trees and create new copies of the hierarchies with each region replaced by a facetized evaluation of that region. (Default is to create one facetized object for all specified inputs.)");
    BU_OPT_NULL(d[8]);

    /* Poisson specific options */
    BU_OPT(pd[0], "d", "depth",            "#", &bu_opt_int,     &(opts->s_opts.depth),            "Maximum reconstruction depth (default 8)");
    BU_OPT(pd[1], "w", "interpolate",      "#", &bu_opt_fastf_t, &(opts->s_opts.point_weight),     "Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. (Default 2.0)");
    BU_OPT(pd[2], "",  "samples-per-node", "#", &bu_opt_fastf_t, &(opts->s_opts.samples_per_node), "How many samples should go into a cell before it is refined. (Default 1.5)");
    BU_OPT(pd[3], "t", "tolerance",        "#", &bu_opt_fastf_t, &(opts->len_tol),        "Specify sampling grid spacing (in mm).");
    BU_OPT(pd[4], "",  "surface",          "",  NULL,            &(opts->pnt_surf_mode),  "Save only first and last points along ray.");
    BU_OPT(pd[5], "",  "grid",             "",  NULL,            &(opts->pnt_grid_mode),  "Sample using a gridded ray pattern (default).");
    BU_OPT(pd[6], "",  "rand",             "",  NULL,            &(opts->pnt_rand_mode),  "Sample using a random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(pd[7], "",  "sobol",            "",  NULL,            &(opts->pnt_sobol_mode), "Sample using a Sobol pseudo-random Marsaglia ray pattern on the bounding sphere.");
    BU_OPT(pd[8], "",  "max-pnts",         "#", &bu_opt_int,     &(opts->max_pnts),       "Maximum number of pnts to return per non-grid sampling method.");
    BU_OPT(pd[9], "",  "max-time",         "#", &bu_opt_int,     &(opts->max_time),       "Maximum time to spend per-method (in seconds) when using non-grid sampling.");
    BU_OPT_NULL(pd[10]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	ret = GED_HELP;
	goto ged_facetize_memfree;
    }

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    /* Enforce type matching on suffix */
    if (opts->make_nmg && BU_STR_EQUAL(bu_vls_addr(opts->faceted_suffix), ".bot")) {
	bu_vls_sprintf(opts->faceted_suffix, ".nmg");
    }
    if (!opts->make_nmg && BU_STR_EQUAL(bu_vls_addr(opts->faceted_suffix), ".nmg")) {
	bu_vls_sprintf(opts->faceted_suffix, ".bot");
    }

    /* If we're in multi-region mode, we may employ more than one technique, so a
     * lot of options may make sense here - just pull the other options out and
     * let the subsequent logic use them (or not). */
    if (opts->regions) {

	/* Parse Poisson specific options */
	argc = bu_opt_parse(NULL, argc, argv, pd);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    ret = GED_ERROR;
	}

	ret = _ged_facetize_regions(gedp, argc, argv, opts);
	goto ged_facetize_memfree;
    }


    if (!opts->nmgbool && !opts->screened_poisson && !opts->marching_cube) {
	opts->nmgbool = 1;
    }

    if (opts->screened_poisson && opts->nmg_use_tnurbs && !opts->nmgbool && !opts->marching_cube) {
	bu_vls_printf(gedp->ged_result_str, "Note: Screened Poisson reconstruction does not support TNURBS output\n");
	ret = GED_ERROR;
	goto ged_facetize_memfree;
    }

    if (opts->triangulate && opts->nmg_use_tnurbs) {
	bu_vls_printf(gedp->ged_result_str, "both -T and -t specified!\n");
	ret = GED_ERROR;
	goto ged_facetize_memfree;
    }


    /* If we're making one object using one technique, be more alert about which
     * options don't make sense in combination */
    if (!opts->screened_poisson) {

	/* must be wanting help */
	if (print_help || argc < 2) {
	    _ged_cmd_help(gedp, usage, d);
	    ret = GED_OK;
	    goto ged_facetize_memfree;
	}

	ret =  _ged_nmg_facetize(gedp, argc, argv, opts);
	goto ged_facetize_memfree;

    } else {

	if (argc < 2 || print_help || opts->marching_cube || opts->nmg_use_tnurbs) {
	    if (print_help || argc < 2) {
		_ged_cmd_help(gedp, pusage, pd);
		ret = GED_OK;
		goto ged_facetize_memfree;
	    }
	    if (opts->nmg_use_tnurbs) {
		bu_vls_printf(gedp->ged_result_str, "Screened Poisson reconstruction does not support TNURBS output\n");
		ret = GED_ERROR;
		goto ged_facetize_memfree;
	    }
	    if (opts->marching_cube) {
		bu_vls_printf(gedp->ged_result_str, "multiple reconstruction methods (Marching Cubes and Screened Poisson) specified\n");
		ret = GED_ERROR;
		goto ged_facetize_memfree;
	    }
	}

	/* Parse Poisson specific options */
	argc = bu_opt_parse(NULL, argc, argv, pd);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    ret = GED_ERROR;
	}

#ifdef ENABLE_SPR
	ret =  _ged_spsr_facetize(gedp, argc, argv, opts);
#else
	bu_vls_printf(gedp->ged_result_str, "Screened Poisson support was not enabled for this build.  To test, pass -DBRLCAD_ENABLE_SPR=ON to the cmake configure.\n");
	ret = GED_ERROR;
#endif

    }

ged_facetize_memfree:
    _ged_facetize_opts_destroy(opts);
    return ret;
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
