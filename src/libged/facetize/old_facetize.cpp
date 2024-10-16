/*                  O L D _ F A C E T I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize.cpp
 *
 * The facetize command, circa v7.38.2
 *
 */

#include "common.h"

#include <map>
#include <set>
#include <vector>

#include <string.h>

#ifdef USE_MANIFOLD
#include "manifold/manifold.h"
#ifdef USE_ASSETIMPORT
#include "manifold/meshIO.h"
#endif
#endif

#include "bu/app.h"
#include "bu/env.h"
#include "bu/exit.h"
#include "bu/hook.h"
#include "bu/interrupt.h"
#include "bu/cmd.h"
#include "bu/time.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"
#include "brep/cdt.h"
#include "rt/geom.h"
#include "rt/op.h"
#include "rt/conv.h"
#include "rt/search.h"
#include "raytrace.h"
#include "analyze.h"
#include "wdb.h"
#include "../ged_private.h"
#include "./old_facetize.h"

#define SOLID_OBJ_NAME 1
#define COMB_OBJ_NAME 2

#define FACETIZE_NULL  0x0
#define FACETIZE_NMGBOOL  0x1
#define FACETIZE_SPSR  0x2
#define FACETIZE_CONTINUATION  0x4
#define FACETIZE_MANIFOLD  0x8

#define FACETIZE_SUCCESS 0
#define FACETIZE_FAILURE 1
#define FACETIZE_FAILURE_PNTGEN 2
#define FACETIZE_FAILURE_PNTBBOX 3
#define FACETIZE_FAILURE_BOTBBOX 4
#define FACETIZE_FAILURE_BOTINVALID 5
#define FACETIZE_FAILURE_DECIMATION 6
#define FACETIZE_FAILURE_NMG 7
#define FACETIZE_FAILURE_CONTINUATION_SURFACE 10
#define FACETIZE_FAILURE_SPSR_SURFACE 11
#define FACETIZE_FAILURE_SPSR_NONMATCHING 12

/* size of available memory (in bytes) below which we can't continue */
#define FACETIZE_MEMORY_THRESHOLD 150000000

struct _ged_facetize_report_info {
    double feature_size;
    double avg_thickness;
    double obj_bbox_vol;
    double pnts_bbox_vol;
    double bot_bbox_vol;
    int failure_mode;
};
#define FACETIZE_REPORT_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0, 0}

static void
_ged_facetize_failure_msg(struct bu_vls *msg, int type, const char *prefix, struct _ged_facetize_report_info *r)
{
    if (!msg) return;
    switch (type) {
	case FACETIZE_SUCCESS:
	    bu_vls_printf(msg, "%s: success\n", prefix);
	    break;
	case FACETIZE_FAILURE:
	    bu_vls_printf(msg, "%s: unknown failure\n", prefix);
	    break;
	case FACETIZE_FAILURE_PNTGEN:
	    bu_vls_printf(msg, "%s: point generation failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_PNTBBOX:
	    bu_vls_printf(msg, "%s: object bbox volume (%g) is widely different than the sampled point cloud bbox volume (%g).\n", prefix, r->obj_bbox_vol, r->pnts_bbox_vol);
	    break;
	case FACETIZE_FAILURE_BOTBBOX:
	    bu_vls_printf(msg, "%s: BoT bbox volume (%g) is widely different than the sampled point cloud bbox volume (%g).\n", prefix, r->bot_bbox_vol, r->pnts_bbox_vol);
	    break;
	case FACETIZE_FAILURE_BOTINVALID:
	    bu_vls_printf(msg, "%s: unable to create a valid BoT.\n", prefix);
	    break;
	case FACETIZE_FAILURE_DECIMATION:
	    bu_vls_printf(msg, "%s: decimation of mesh failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_NMG:
	    bu_vls_printf(msg, "%s: unable to successfully generate NMG object\n", prefix);
	    break;
	case FACETIZE_FAILURE_CONTINUATION_SURFACE:
	    bu_vls_printf(msg, "%s: continuation polygonization surface build failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_SPSR_SURFACE:
	    bu_vls_printf(msg, "%s: Screened Poisson surface reconstruction failed.\n", prefix);
	    break;
	case FACETIZE_FAILURE_SPSR_NONMATCHING:
	    bu_vls_printf(msg, "%s: Screened Poisson surface reconstruction did not produce a BoT matching the original shape.\n", prefix);
	    break;
	default:
	    return;
	    break;
    }
}


static const char *
_ged_facetize_attr(int method)
{
    static const char *nmg_flag = "facetize:NMG";
    static const char *continuation_flag = "facetize:CM";
    static const char *spsr_flag = "facetize:SPSR";
    static const char *manifold_flag = "facetize:MANIFOLD";
    if (method == FACETIZE_NMGBOOL) return nmg_flag;
    if (method == FACETIZE_CONTINUATION) return continuation_flag;
    if (method == FACETIZE_SPSR) return spsr_flag;
    if (method == FACETIZE_MANIFOLD) return manifold_flag;
    return NULL;
}


static int
_ged_facetize_attempted(struct ged *gedp, const char *oname, int method)
{
    int ret = 0;
    struct bu_attribute_value_set avs;
    struct directory *dp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);
    if (!dp) return 0;
    if (db5_get_attributes(gedp->dbip, &avs, dp)) return 0;
    if (bu_avs_get(&avs, _ged_facetize_attr(method))) ret = 1;
    bu_avs_free(&avs);
    return ret;
}


static struct _old_ged_facetize_opts * _old_ged_facetize_opts_create()
{
    struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    struct _old_ged_facetize_opts *o = NULL;
    BU_GET(o, struct _old_ged_facetize_opts);
    o->verbosity = 0;
    o->triangulate = 0;
    o->make_nmg = 0;
    o->nmgbool = 0;
    o->continuation = 0;
    o->screened_poisson = 0;
    o->nmg_use_tnurbs = 0;
    o->regions = 0;
    o->resume = 0;
    o->retry = 0;
    o->in_place = 0;
    o->feature_size = 0.0;
    o->feature_scale = 0.15;
    o->d_feature_size = 0.0;
    BU_GET(o->faceted_suffix, struct bu_vls);
    bu_vls_init(o->faceted_suffix);
    bu_vls_sprintf(o->faceted_suffix, ".bot");

    o->max_pnts = 0;
    o->max_time = 30;

    o->nonovlp_threshold = 0;

    o->s_opts = s_opts;

    BU_ALLOC(o->c_map, struct bu_attribute_value_set);
    BU_ALLOC(o->s_map, struct bu_attribute_value_set);

    bu_avs_init_empty(o->c_map);
    bu_avs_init_empty(o->s_map);

    BU_ALLOC(o->saved_log_hooks, struct bu_hook_list);
    bu_hook_list_init(o->saved_log_hooks);

    BU_ALLOC(o->saved_bomb_hooks, struct bu_hook_list);
    bu_hook_list_init(o->saved_bomb_hooks);

    BU_GET(o->nmg_log, struct bu_vls);
    bu_vls_init(o->nmg_log);

    BU_GET(o->nmg_log_header, struct bu_vls);
    bu_vls_init(o->nmg_log_header);


    BU_GET(o->froot, struct bu_vls);
    bu_vls_init(o->froot);

    BU_GET(o->nmg_comb, struct bu_vls);
    bu_vls_init(o->nmg_comb);

    BU_GET(o->manifold_comb, struct bu_vls);
    bu_vls_init(o->manifold_comb);

    BU_GET(o->continuation_comb, struct bu_vls);
    bu_vls_init(o->continuation_comb);

    BU_GET(o->spsr_comb, struct bu_vls);
    bu_vls_init(o->spsr_comb);

    return o;
}

static void _old_ged_facetize_opts_destroy(struct _old_ged_facetize_opts *o)
{
    if (!o) return;
    if (o->faceted_suffix) {
	bu_vls_free(o->faceted_suffix);
	BU_PUT(o->faceted_suffix, struct bu_vls);
    }

    bu_avs_free(o->c_map);
    bu_avs_free(o->s_map);
    bu_free(o->c_map, "comb map");
    bu_free(o->s_map, "solid map");

    bu_hook_delete_all(o->saved_log_hooks);
    bu_free(o->saved_log_hooks, "saved log hooks");

    bu_hook_delete_all(o->saved_bomb_hooks);
    bu_free(o->saved_bomb_hooks, "saved log hooks");

    bu_vls_free(o->nmg_log);
    bu_vls_free(o->nmg_log_header);
    BU_PUT(o->nmg_log, struct bu_vls);
    BU_PUT(o->nmg_log_header, struct bu_vls);


    bu_vls_free(o->froot);
    BU_PUT(o->froot, struct bu_vls);

    bu_vls_free(o->nmg_comb);
    BU_PUT(o->nmg_comb, struct bu_vls);

    bu_vls_free(o->manifold_comb);
    BU_PUT(o->manifold_comb, struct bu_vls);

    bu_vls_free(o->continuation_comb);
    BU_PUT(o->continuation_comb, struct bu_vls);

    bu_vls_free(o->spsr_comb);
    BU_PUT(o->spsr_comb, struct bu_vls);

    BU_PUT(o, struct _old_ged_facetize_opts);
}


static db_op_t
_int_to_opt(int op)
{
    if (op == 2) return DB_OP_UNION;
    if (op == 3) return DB_OP_INTERSECT;
    if (op == 4) return DB_OP_SUBTRACT;
    return DB_OP_NULL;
}


static int
_old_db_uniq_test(struct bu_vls *n, void *data)
{
    struct ged *gedp = (struct ged *)data;
    if (db_lookup(gedp->dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}


static double
_bbox_vol(point_t b_min, point_t b_max)
{
    double bbox_vol = 0.0;
    fastf_t b_xlen, b_ylen, b_zlen;
    b_xlen = fabs(b_max[X] - b_min[X]);
    b_ylen = fabs(b_max[Y] - b_min[Y]);
    b_zlen = fabs(b_max[Z] - b_min[Z]);
    bbox_vol = b_xlen * b_ylen * b_zlen;
    return bbox_vol;
}


static void
_rt_pnts_bbox(point_t rpp_min, point_t rpp_max, struct rt_pnts_internal *pnts)
{
    struct pnt_normal *pn = NULL;
    struct pnt_normal *pl = NULL;
    if (!rpp_min || !rpp_max || !pnts || !pnts->point) return;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    pl = (struct pnt_normal *)pnts->point;
    for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	VMINMAX(rpp_min, rpp_max, pn->v);
    }
}


static void
_pnts_bbox(point_t rpp_min, point_t rpp_max, int pnt_cnt, point_t *pnts)
{
    int i = 0;
    if (!rpp_min || !rpp_max || !pnts || !pnts || !pnt_cnt) return;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    for (i = 0; i < pnt_cnt; i++) {
	VMINMAX(rpp_min, rpp_max, pnts[i]);
    }
}


static void
_ged_facetize_mkname(struct ged *gedp, struct _old_ged_facetize_opts *opts, const char *n, int type)
{
    struct bu_vls incr_template = BU_VLS_INIT_ZERO;

    if (type == SOLID_OBJ_NAME) {
	bu_vls_sprintf(&incr_template, "%s%s", n, bu_vls_addr(opts->faceted_suffix));
    }
    if (type == COMB_OBJ_NAME) {
	if (opts->in_place) {
	    bu_vls_sprintf(&incr_template, "%s_orig", n);
	} else {
	    bu_vls_sprintf(&incr_template, "%s", n);
	}
    }
    if (!bu_vls_strlen(&incr_template)) {
	bu_vls_free(&incr_template);
	return;
    }
    if (db_lookup(gedp->dbip, bu_vls_addr(&incr_template), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(&incr_template, "-0");
	bu_vls_incr(&incr_template, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
    }

    if (type == SOLID_OBJ_NAME) {
	bu_avs_add(opts->s_map, n, bu_vls_addr(&incr_template));
    }
    if (type == COMB_OBJ_NAME) {
	bu_avs_add(opts->c_map, n, bu_vls_addr(&incr_template));
    }

    bu_vls_free(&incr_template);
}


static int
_ged_validate_objs_list(struct ged *gedp, int argc, const char *argv[], struct _old_ged_facetize_opts *o, int newobj_cnt)
{
    int i;

    if (o->in_place && newobj_cnt) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but object list includes objects that do not exist:\n");
	for (i = argc - newobj_cnt; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  When performing an in-place facetization, a single pre-existing object must be specified.\n");
	return BRLCAD_ERROR;

    }

    if (o->in_place && argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but multiple objects listed:\n");
	for (i = 0; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  An in-place conversion replaces the specified object (or regions in a hierarchy in -r mode) with a faceted approximation.  Because a single object is generated, in-place conversion has no clear interpretation when multiple input objects are specified.\n");
	return BRLCAD_ERROR;
    }

    if (!o->in_place) {
	if (newobj_cnt < 1) {
	    bu_vls_printf(gedp->ged_result_str, "all objects listed already exist, aborting.  (Need new object name to write out results to.)\n");
	    return BRLCAD_ERROR;
	}

	if (newobj_cnt > 1) {
	    bu_vls_printf(gedp->ged_result_str, "More than one object listed does not exist:\n");
	    for (i = argc - newobj_cnt; i < argc; i++) {
		bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	    }
	    bu_vls_printf(gedp->ged_result_str, "\nAborting.  Need to specify exactly one object name that does not exist to hold facetization output.\n");
	    return BRLCAD_ERROR;
	}

	if (argc - newobj_cnt == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No existing objects specified, nothing to facetize.  Aborting.\n");
	    return BRLCAD_ERROR;
	}
    }
    return BRLCAD_OK;
}


static
int
_ged_facetize_solid_objs(struct ged *gedp, int argc, struct directory **dpa, struct _old_ged_facetize_opts *opts)
{
    unsigned int i;
    int ret = 1;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    const char *pnt_objs = "-type pnts";
    if (argc < 1 || !dpa || !gedp) return 0;

    /* If we have pnts, it's not a solid tree */
    if (db_search(NULL, DB_SEARCH_QUIET, pnt_objs, argc, dpa, gedp->dbip, NULL) > 0) {
	if (opts->verbosity) {
	    bu_log("-- Found pnts objects in tree\n");
	}
	return 0;
    }

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, argc, dpa, gedp->dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for BoTs - aborting.\n");
	}
	ret = 0;
	goto ged_facetize_solid_objs_memfree;
    }

    /* Got all the BoT objects in the tree, check each of them for validity */
    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int not_solid;
	struct directory *bot_dp = (struct directory *)BU_PTBL_GET(bot_dps, i);
	GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    not_solid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
	    if (not_solid) {
		if (opts->verbosity) {
		    bu_log("-- Found non solid BoT: %s\n", bot_dp->d_namep);
		}
		ret = 0;
	    }
	}
	rt_db_free_internal(&intern);
    }

    /* TODO - any other objects that need a pre-boolean validity check? */

ged_facetize_solid_objs_memfree:
    bu_ptbl_free(bot_dps);
    bu_free(bot_dps, "bot directory pointer table");
    return ret;
}


static int
_ged_facetize_obj_swap(struct ged *gedp, const char *o, const char *n)
{
    int ret = BRLCAD_OK;
    const char *mav[3];
    const char *cmdname = "facetize";
    /* o or n may point to a d_namep location, which will change with
     * moves, so copy them up front for consistent values */
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    struct bu_vls nname = BU_VLS_INIT_ZERO;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    mav[0] = cmdname;
    bu_vls_sprintf(&oname, "%s", o);
    bu_vls_sprintf(&nname, "%s", n);
    bu_vls_sprintf(&tname, "%s", o);
    bu_vls_incr(&tname, NULL, "0:0:0:0:-", &_old_db_uniq_test, (void *)gedp);
    mav[1] = bu_vls_addr(&oname);
    mav[2] = bu_vls_addr(&tname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&nname);
    mav[2] = bu_vls_addr(&oname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&tname);
    mav[2] = bu_vls_addr(&nname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }

ged_facetize_obj_swap_memfree:
    bu_vls_free(&oname);
    bu_vls_free(&nname);
    bu_vls_free(&tname);
    return ret;
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


static struct model *
_try_nmg_facetize(struct ged *gedp, int argc, const char **argv, int nmg_use_tnurbs, struct _old_ged_facetize_opts *o)
{
    int i;
    int failed = 0;
    struct db_tree_state init_state;
    union tree *facetize_tree;
    struct model *nmg_model;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    _old_ged_facetize_log_nmg(o);

    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);

    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;

    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			 &init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 nmg_use_tnurbs ?
			 nmg_booltree_leaf_tnurb :
			 rt_booltree_leaf_tess,
			 (void *)&facetize_tree
	    );
    } else {
	/* catch */
	BU_UNSETJUMP;
	_old_ged_facetize_log_default(o);
	return NULL;
    } BU_UNSETJUMP;

    if (i < 0) {
	/* Destroy NMG */
	_old_ged_facetize_log_default(o);
	return NULL;
    }

    if (facetize_tree) {
	if (!BU_SETJUMP) {
	    /* try */
	    failed = nmg_boolean(facetize_tree, nmg_model, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    _old_ged_facetize_log_default(o);
	    return NULL;
	} BU_UNSETJUMP;

    } else {
	failed = 1;
    }

    if (!failed && facetize_tree) {
	NMG_CK_REGION(facetize_tree->tr_d.td_r);
	facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;
    }

    if (facetize_tree) {
	db_free_tree(facetize_tree, &rt_uniresource);
    }

    _old_ged_facetize_log_default(o);
    return (failed) ? NULL : nmg_model;
}


static int
_try_nmg_triangulate(struct ged *gedp, struct model *nmg_model, struct _old_ged_facetize_opts *o)
{
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    _old_ged_facetize_log_nmg(o);

    if (!BU_SETJUMP) {
	/* try */
	nmg_triangulate_model(nmg_model, &RTG.rtg_vlfree, &wdbp->wdb_tol);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_log("WARNING: triangulation failed!!!\n");
	_old_ged_facetize_log_default(o);
	return BRLCAD_ERROR;
    } BU_UNSETJUMP;
    _old_ged_facetize_log_default(o);
    return BRLCAD_OK;
}


static struct rt_bot_internal *
_try_nmg_to_bot(struct ged *gedp, struct model *nmg_model, struct _old_ged_facetize_opts *o)
{
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct rt_bot_internal *bot = NULL;
    _old_ged_facetize_log_nmg(o);
    if (!BU_SETJUMP) {
	/* try */
	bot = (struct rt_bot_internal *)nmg_mdl_to_bot(nmg_model, &RTG.rtg_vlfree, &wdbp->wdb_tol);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_old_ged_facetize_log_default(o);
	return NULL;
    } BU_UNSETJUMP;
    _old_ged_facetize_log_default(o);
    return bot;
}


static struct rt_bot_internal *
_try_decimate(struct rt_bot_internal *bot, fastf_t feature_size, struct _old_ged_facetize_opts *o)
{
    size_t success = 0;
    /* these are static for longjmp */
    static struct rt_bot_internal *nbot;
    static struct rt_bot_internal *obot;

    obot = bot;
    nbot = NULL;

    BU_ALLOC(nbot, struct rt_bot_internal);
    nbot->magic = RT_BOT_INTERNAL_MAGIC;
    nbot->mode = RT_BOT_SOLID;
    nbot->orientation = RT_BOT_UNORIENTED;
    nbot->thickness = (fastf_t *)NULL;
    nbot->face_mode = (struct bu_bitv *)NULL;
    nbot->num_faces = bot->num_faces;
    nbot->num_vertices = bot->num_vertices;
    nbot->faces = (int *)bu_calloc(nbot->num_faces*3, sizeof(int), "copy of faces array");
    nbot->vertices = (fastf_t *)bu_calloc(nbot->num_vertices*3, sizeof(fastf_t), "copy of faces array");
    memcpy(nbot->faces, bot->faces, nbot->num_faces*3*sizeof(int));
    memcpy(nbot->vertices, bot->vertices, nbot->num_vertices*3*sizeof(fastf_t));

    _old_ged_facetize_log_nmg(o);
    if (!BU_SETJUMP) {
	/* try */
	success = rt_bot_decimate_gct(nbot, feature_size);
    } else {
	/* catch */
	BU_UNSETJUMP;
	/* Failed - free the working copy */
	bu_free(nbot->faces, "free faces");
	bu_free(nbot->vertices, "free vertices");
	bu_free(nbot, "free bot");
	_old_ged_facetize_log_default(o);
	return obot;
    } BU_UNSETJUMP;

    if (success) {
	/* Success - free the old BoT, return the new one */
	bu_free(obot->faces, "free faces");
	bu_free(obot->vertices, "free vertices");
	bu_free(obot, "free bot");
	_old_ged_facetize_log_default(o);
	return nbot;
    } else {
	/* Failed - free the working copy */
	bu_free(nbot->faces, "free faces");
	bu_free(nbot->vertices, "free vertices");
	bu_free(nbot, "free bot");
	_old_ged_facetize_log_default(o);
	return obot;
    }
}


static int
_write_bot(struct ged *gedp, struct rt_bot_internal *bot, const char *name, struct _old_ged_facetize_opts *opts)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct db_i *dbip = gedp->dbip;

    /* Export BOT as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (opts->verbosity) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Failed to write %s to database\n", name);
	}
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
_write_nmg(struct ged *gedp, struct model *nmg_model, const char *name, struct _old_ged_facetize_opts *opts)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct db_i *dbip = gedp->dbip;

    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)nmg_model;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (opts->verbosity) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Failed to write %s to database\n", name);
	}
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
_ged_spsr_obj(struct _ged_facetize_report_info *r, struct ged *gedp, const char *objname, const char *newname, struct _old_ged_facetize_opts *opts)
{
    int ret = BRLCAD_OK;
    struct directory *dp;
    int decimation_succeeded = 0;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal in_intern;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
    double avg_thickness = 0.0;
    int flags = 0;
    int i = 0;
    int free_pnts = 0;
    point_t *input_points_3d = NULL;
    vect_t *input_normals_3d = NULL;
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);

    dp = db_lookup(dbip, objname, LOOKUP_QUIET);

    r->failure_mode = FACETIZE_FAILURE;

    if (!r) return FACETIZE_FAILURE;

    /* From here on out, assume success until we fail */
    r->failure_mode = FACETIZE_SUCCESS;

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Error: could not determine type of object %s, skipping\n", objname);
	}
	r->failure_mode = FACETIZE_FAILURE;
	return FACETIZE_FAILURE;
    }

    /* If we have a half, this won't work */
    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_HALF) {
	r->failure_mode = FACETIZE_FAILURE;
	return FACETIZE_FAILURE;
    }

    /* Key some settings off the bbox size */
    rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&objname, 0, obj_min, obj_max);
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);


    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS) {
	/* If we have a point cloud, there's no need to raytrace it */
	pnts = (struct rt_pnts_internal *)in_intern.idb_ptr;

    } else {
	int max_pnts = (opts->max_pnts) ? opts->max_pnts : 200000;
	BU_ALLOC(pnts, struct rt_pnts_internal);
	pnts->magic = RT_PNTS_INTERNAL_MAGIC;
	pnts->scale = 0.0;
	pnts->type = RT_PNT_TYPE_NRM;
	flags = ANALYZE_OBJ_TO_PNTS_RAND;
	free_pnts = 1;

	if (opts->verbosity) {
	    bu_log("SPSR: generating %d points from %s\n", max_pnts, objname);
	}

	if (analyze_obj_to_pnts(pnts, &avg_thickness, gedp->dbip, objname, &btol, flags, max_pnts, opts->max_time, opts->verbosity)) {
	    r->failure_mode = FACETIZE_FAILURE_PNTGEN;
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
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
		   pnts->count, &(opts->s_opts))) {
	r->failure_mode = FACETIZE_FAILURE_SPSR_SURFACE;
	ret = FACETIZE_FAILURE;
	goto ged_facetize_spsr_memfree;
    }

    /* do decimation */
    {
	struct rt_bot_internal *obot = bot;
	fastf_t feature_size = 0.0;
	if (opts->feature_size > 0) {
	    feature_size = opts->feature_size;
	} else {
	    fastf_t xlen = fabs(rpp_max[X] - rpp_min[X]);
	    fastf_t ylen = fabs(rpp_max[Y] - rpp_min[Y]);
	    fastf_t zlen = fabs(rpp_max[Z] - rpp_min[Z]);
	    feature_size = (xlen < ylen) ? xlen : ylen;
	    feature_size = (feature_size < zlen) ? feature_size : zlen;
	    feature_size = feature_size * 0.15;
	}

	if (opts->verbosity) {
	    bu_log("SPSR: decimating with feature size: %g\n", feature_size);
	}

	bot = _try_decimate(bot, feature_size, opts);

	if (bot == obot) {
	    r->failure_mode = FACETIZE_FAILURE_DECIMATION;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	}
	if (bot != obot) {
	    decimation_succeeded = 1;
	}
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    r->failure_mode = FACETIZE_FAILURE_BOTINVALID;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    if (!opts->quiet) {
		bu_log("SPSR: facetization failed, final BoT was not solid\n");
	    }
	    goto ged_facetize_spsr_memfree;
	}
    }

    /* Because SPSR has some observed failure modes that produce a "valid" BoT totally different
     * from the original shape, if we know the avg. thickness of the original object raytrace
     * the candidate BoT and compare the avg. thicknesses */
    if (avg_thickness > 0) {
	const char *av[3];
	int max_pnts = (opts->max_pnts) ? opts->max_pnts : 200000;
	double navg_thickness = 0.0;
	struct bu_vls tmpname = BU_VLS_INIT_ZERO;
	struct rt_bot_internal *tbot = NULL;
	BU_ALLOC(tbot, struct rt_bot_internal);
	tbot->magic = RT_BOT_INTERNAL_MAGIC;
	tbot->mode = RT_BOT_SOLID;
	tbot->orientation = RT_BOT_UNORIENTED;
	tbot->thickness = (fastf_t *)NULL;
	tbot->face_mode = (struct bu_bitv *)NULL;

	tbot->num_vertices = bot->num_vertices;
	tbot->num_faces = bot->num_faces;
	tbot->vertices = (fastf_t *)bu_malloc(sizeof(fastf_t) * tbot->num_vertices * 3, "vert array");
	memcpy(tbot->vertices, bot->vertices, sizeof(fastf_t) * tbot->num_vertices * 3);
	tbot->faces = (int *)bu_malloc(sizeof(int) * tbot->num_faces * 3, "faces array");
	memcpy(tbot->faces, bot->faces, sizeof(int) * tbot->num_faces *3);

	flags = ANALYZE_OBJ_TO_PNTS_RAND;
	bu_vls_sprintf(&tmpname, "%s.tmp", newname);
	if (db_lookup(gedp->dbip, bu_vls_addr(&tmpname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(&tmpname, "-0");
	    bu_vls_incr(&tmpname, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
	}
	if (_write_bot(gedp, tbot, bu_vls_addr(&tmpname), opts) & BRLCAD_ERROR) {
	    bu_log("SPSR: could not write BoT to temporary name %s\n", bu_vls_addr(&tmpname));
	    bu_vls_free(&tmpname);
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_spsr_memfree;
	}

	if (analyze_obj_to_pnts(NULL, &navg_thickness, gedp->dbip, bu_vls_addr(&tmpname), &btol, flags, max_pnts, opts->max_time, opts->verbosity)) {
	    bu_log("SPSR: could not raytrace temporary BoT %s\n", bu_vls_addr(&tmpname));
	    ret = FACETIZE_FAILURE;
	}

	/* Remove the temporary BoT object, succeed or fail. */
	av[0] = "kill";
	av[1] = bu_vls_addr(&tmpname);
	av[2] = NULL;
	(void)ged_exec(gedp, 2, (const char **)av);

	if (ret == FACETIZE_FAILURE) {
	    bu_vls_free(&tmpname);
	    goto ged_facetize_spsr_memfree;
	}


	if (fabs(avg_thickness - navg_thickness) > avg_thickness * 0.5) {
	    bu_log("SPSR: BoT average sampled thickness %f is widely different from original sampled thickness %f\n", navg_thickness, avg_thickness);
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_SPSR_NONMATCHING;
	    bu_vls_free(&tmpname);
	    goto ged_facetize_spsr_memfree;
	}

	/* Passed test, continue */
	bu_vls_free(&tmpname);
    }

    if (decimation_succeeded && !opts->quiet) {
	bu_log("SPSR: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

    if (!opts->make_nmg) {

	ret = _write_bot(gedp, bot, newname, opts);

    } else {
	/* Convert BoT to NMG */
	struct model *m = nmg_mm();
	struct nmgregion *nr;
	struct rt_db_internal intern;

	/* Use intern to fake out rt_bot_tess, since it expects an
	 * rt_db_internal wrapper */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_ptr = (void *)bot;
	if (rt_bot_tess(&nr, m, &intern, NULL, &btol) < 0) {
	    if (!opts->quiet) {
		bu_log("SPSR: failed to convert BoT to NMG: %s\n", objname);
	    }
	    rt_db_free_internal(&intern);
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_NMG;
	    goto ged_facetize_spsr_memfree;
	} else {
	    /* OK, have NMG now - write it out */
	    ret = _write_nmg(gedp, m, newname, opts);
	    rt_db_free_internal(&intern);
	}
    }

ged_facetize_spsr_memfree:
    if (free_pnts) bu_free(pnts, "free pnts");
    if (input_points_3d) bu_free(input_points_3d, "3d pnts");
    if (input_normals_3d) bu_free(input_normals_3d, "3d pnts");
    rt_db_free_internal(&in_intern);

    return ret;
}


static int
_ged_check_plate_mode(struct ged *gedp, struct directory *dp)
{
    unsigned int i;
    int ret = 0;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    if (!dp || !gedp) return 0;

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, 1, &dp, gedp->dbip, NULL) < 0) {
	goto ged_check_plate_mode_memfree;
    }

    /* Got all the BoT objects in the tree, check each of them for validity */
    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	struct directory *bot_dp = (struct directory *)BU_PTBL_GET(bot_dps, i);
	GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    ret = 1;
	    rt_db_free_internal(&intern);
	    goto ged_check_plate_mode_memfree;
	}
	rt_db_free_internal(&intern);
    }

ged_check_plate_mode_memfree:
    bu_ptbl_free(bot_dps);
    bu_free(bot_dps, "bot directory pointer table");
    return ret;
}


static int
_ged_continuation_obj(struct _ged_facetize_report_info *r, struct ged *gedp, const char *objname, const char *newname, struct _old_ged_facetize_opts *opts)
{
    int first_run = 1;
    int fatal_error_cnt = 0;
    int ret = BRLCAD_OK;
    double avg_thickness = 0.0;
    double min_len = 0.0;
    fastf_t feature_size = 0.0;
    fastf_t target_feature_size = 0.0;
    int face_cnt = 0;
    double successful_feature_size = 0.0;
    unsigned int successful_bot_count = 0;
    int decimation_succeeded = 0;
    double xlen, ylen, zlen;
    struct directory *dp;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal in_intern;
    struct bn_tol btol = BN_TOL_INIT_TOL;
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
    int polygonize_failure = 0;
    struct analyze_polygonize_params params = ANALYZE_POLYGONIZE_PARAMS_DEFAULT;
    int flags = 0;
    int free_pnts = 0;
    int max_pnts = 50000;
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);

    dp = db_lookup(dbip, objname, LOOKUP_QUIET);

    r->failure_mode = FACETIZE_FAILURE;

    if (!r) return FACETIZE_FAILURE;

    if (_ged_check_plate_mode(gedp, dp)) return FACETIZE_FAILURE;

    /* From here on out, assume success until we fail */
    r->failure_mode = FACETIZE_SUCCESS;

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Error: could not determine type of object %s, skipping\n", objname);
	}
	r->failure_mode = FACETIZE_FAILURE;
	return FACETIZE_FAILURE;
    }

    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS || in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_HALF) {
	/* If we have a point cloud or half, this won't work */
	r->failure_mode = FACETIZE_FAILURE;
	return FACETIZE_FAILURE;
    }


    BU_ALLOC(pnts, struct rt_pnts_internal);
    pnts->magic = RT_PNTS_INTERNAL_MAGIC;
    pnts->scale = 0.0;
    pnts->count = 0;
    pnts->type = RT_PNT_TYPE_NRM;
    pnts->point = NULL;
    free_pnts = 1;

    /* Key some settings off the bbox size */
    rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&objname, 0, obj_min, obj_max);
    VMINMAX(rpp_min, rpp_max, (double *)obj_min);
    VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    xlen = fabs(rpp_max[X] - rpp_min[X]);
    ylen = fabs(rpp_max[Y] - rpp_min[Y]);
    zlen = fabs(rpp_max[Z] - rpp_min[Z]);

    /* Pick our mode(s) */
    flags |= ANALYZE_OBJ_TO_PNTS_RAND;
    flags |= ANALYZE_OBJ_TO_PNTS_SOBOL;

    if (opts->max_pnts) {
	max_pnts = opts->max_pnts;
    }

    /* Shoot - we need both the avg thickness of the hit partitions and seed points */
    if (analyze_obj_to_pnts(pnts, &avg_thickness, gedp->dbip, objname, &btol, flags, max_pnts, opts->max_time, opts->verbosity) || pnts->count <= 0) {
	r->failure_mode = FACETIZE_FAILURE_PNTGEN;
	ret = FACETIZE_FAILURE;
	goto ged_facetize_continuation_memfree;
    }

    /* Check the volume of the bounding box of input object against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  This one we are a bit more generous with, since the parent bbox
     * may not be terribly tight. */
    {
	point_t p_min, p_max;
	VSETALL(p_min, INFINITY);
	VSETALL(p_max, -INFINITY);
	_rt_pnts_bbox(p_min, p_max, pnts);
	r->pnts_bbox_vol = _bbox_vol(p_min, p_max);
	r->obj_bbox_vol = _bbox_vol(rpp_min, rpp_max);
	if (fabs(r->obj_bbox_vol - r->pnts_bbox_vol)/r->obj_bbox_vol > 1) {
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_PNTBBOX;
	    goto ged_facetize_continuation_memfree;
	}
    }

    if (opts->verbosity) {
	bu_log("CM: average raytrace thickness: %g\n", avg_thickness);
    }

    /* Find the smallest value from either the bounding box lengths or the avg
     * thickness observed by the rays.  Some fraction of this value is our box
     * size for the polygonizer and the decimation routine */
    min_len = (xlen < ylen) ? xlen : ylen;
    min_len = (min_len < zlen) ? min_len : zlen;
    min_len = (min_len < avg_thickness) ? min_len : avg_thickness;

    if (opts->feature_size > 0) {
	target_feature_size = 0.5*feature_size;
    } else {
	target_feature_size = min_len * opts->feature_scale;
    }

    if (opts->verbosity) {
	bu_log("CM: targeting feature size %g\n", target_feature_size);
    }

    /* Build the BoT */
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SOLID;
    bot->orientation = RT_BOT_UNORIENTED;
    bot->thickness = (fastf_t *)NULL;
    bot->face_mode = (struct bu_bitv *)NULL;
    bot->faces = NULL;
    bot->vertices = NULL;

    /* Run the polygonize routine.  Because it is quite simple to accidentally
     * specify inputs that will take huge amounts of time to run, we will
     * attempt a series of progressively courser polygonize runs until we
     * either succeed, or reach a feature size that is greater than 2x the
     * average thickness according to the raytracer without succeeding. If
     * max_time has been explicitly set to 0 by the caller this will run
     * unbounded, but the algorithm is n**2 and we're trying the finest level
     * first so may run a *very* long time... */
    pl = (struct pnt_normal *)pnts->point;
    pn = BU_LIST_PNEXT(pnt_normal, pl);
    if (opts->feature_size > 0) {
	feature_size = opts->feature_size;
    } else {
	feature_size = 2*avg_thickness;
    }

    params.max_time = opts->max_time;
    params.verbosity = opts->verbosity;
    params.minimum_free_mem = FACETIZE_MEMORY_THRESHOLD;

    while (!polygonize_failure && (feature_size > 0.9*target_feature_size || face_cnt < 1000) && fatal_error_cnt < 8) {
	double timestamp = bu_gettime();
	int delta;
	fastf_t *verts = bot->vertices;
	int *faces = bot->faces;
	int num_faces = bot->num_faces;
	int num_verts = bot->num_vertices;
	bot->vertices = NULL;
	bot->faces = NULL;
	polygonize_failure = analyze_polygonize(&(bot->faces), (int *)&(bot->num_faces),
						(point_t **)&(bot->vertices),
						(int *)&(bot->num_vertices),
						feature_size, pn->v, objname, gedp->dbip, &params);
	delta = (int)((bu_gettime() - timestamp)/1e6);
	if (polygonize_failure || bot->num_faces < successful_bot_count || delta < 2) {
	    if (polygonize_failure == 3) {
		bu_log("CM: Too little available memory to continue, aborting\n");
		ret = FACETIZE_FAILURE;
		goto ged_facetize_continuation_memfree;
	    }
	    if (polygonize_failure == 2) {
		if (!opts->quiet) {
		    bu_log("CM: timed out after %d seconds with size %g\n", opts->max_time, feature_size);
		}
		/* If we still haven't had a successful run, back the feature size out and try again */
		if (first_run) {
		    polygonize_failure = 0;
		    feature_size = feature_size * 5;
		    fatal_error_cnt++;
		    continue;
		}
	    }
	    bot->faces = faces;
	    bot->vertices = verts;
	    bot->num_vertices = num_verts;
	    bot->num_faces = num_faces;
	    if (polygonize_failure != 2 && opts->feature_size <= 0) {
		/* Something about the previous size didn't work - nudge the feature size and try again
		 * unless we've had multiple fatal errors. */
		polygonize_failure = 0;
		if (!opts->quiet) {
		    bu_log("CM: error at size %g\n", feature_size);
		}
		/* If we've had a successful first run, just nudge the feature
		 * size down and retry.  If we haven't succeeded yet, and we've
		 * got just this one error, try dropping the feature size down by an order
		 * of magnitude.  If we haven't succeed yet *and* we've got
		 * multiple fatal errors, try dropping it by half. */
		feature_size = feature_size * ((!first_run) ? 0.95 : ((fatal_error_cnt) ? 0.5 : 0.1));
		if (!opts->quiet) {
		    bu_log("CM: retrying with size %g\n", feature_size);
		}
		fatal_error_cnt++;
		continue;
	    }
	    feature_size = successful_feature_size;
	    if (!opts->quiet && bot->faces) {
		if (opts->feature_size <= 0) {
		    bu_log("CM: unable to polygonize at target size (%g), using last successful BoT with %d faces, feature size %g\n", target_feature_size, (int)bot->num_faces, successful_feature_size);
		} else {
		    bu_log("CM: successfully created %d faces, feature size %g\n", (int)bot->num_faces, successful_feature_size);
		}
	    }
	} else {
	    if (verts) bu_free(verts, "old verts");
	    if (faces) bu_free(faces, "old faces");
	    /* if we have had a fatal error in the past, reset on subsequent success */
	    fatal_error_cnt = 0;
	    successful_feature_size = feature_size;
	    if (!opts->quiet) {
		bu_log("CM: completed in %d seconds with size %g\n", delta, feature_size);
	    }
	    feature_size = feature_size * ((delta < 5) ? 0.7 : 0.9);
	    face_cnt = bot->num_faces;
	    successful_bot_count = bot->num_faces;
	}
	first_run = 0;
    }

    if (bot->num_faces && feature_size < target_feature_size && !opts->quiet) {
	bu_log("CM: successfully polygonized BoT with %d faces at feature size %g\n", (int)bot->num_faces, feature_size);
    }

    if (!bot->faces) {
	if (!opts->quiet) {
	    bu_log("CM: surface reconstruction failed: %s\n", objname);
	}
	r->failure_mode = FACETIZE_FAILURE_CONTINUATION_SURFACE;
	ret = FACETIZE_FAILURE;
	goto ged_facetize_continuation_memfree;
    }

    /* do decimation */
    {
	fastf_t d_feature_size = (opts->d_feature_size > 0) ? opts->d_feature_size : 1.5 * feature_size;
	struct rt_bot_internal *obot = bot;

	if (opts->verbosity) {
	    bu_log("CM: decimating with feature size %g\n", d_feature_size);
	}

	bot = _try_decimate(bot, d_feature_size, opts);

	if (bot == obot) {
	    r->failure_mode = FACETIZE_FAILURE_DECIMATION;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    goto ged_facetize_continuation_memfree;
	}
	if (bot != obot) {
	    decimation_succeeded = 1;
	}
    }

    /* Check the volume of the bounding box of the BoT against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  For the moment, use >50% difference. */
    {
	point_t b_min, b_max;
	VSETALL(b_min, INFINITY);
	VSETALL(b_max, -INFINITY);
	_pnts_bbox(b_min, b_max, bot->num_vertices, (point_t *)bot->vertices);
	r->bot_bbox_vol = _bbox_vol(b_min, b_max);
	if (fabs(r->pnts_bbox_vol - r->bot_bbox_vol) > r->pnts_bbox_vol * 0.5) {
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_BOTBBOX;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    goto ged_facetize_continuation_memfree;
	}
    }

    /* Check the volume of the bounding box of the BoT against the bounding box
     * of the point cloud - a large difference means something probably isn't
     * right.  For the moment, use >50% difference. */
    {
	point_t b_min, b_max;
	VSETALL(b_min, INFINITY);
	VSETALL(b_max, -INFINITY);
	_pnts_bbox(b_min, b_max, bot->num_vertices, (point_t *)bot->vertices);
	r->bot_bbox_vol = _bbox_vol(b_min, b_max);
	if (fabs(r->pnts_bbox_vol - r->bot_bbox_vol) > r->pnts_bbox_vol * 0.5) {
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_BOTBBOX;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    goto ged_facetize_continuation_memfree;
	}
    }

    /* Check validity - do not return an invalid BoT */
    {
	int not_solid = bg_trimesh_solid2(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	if (not_solid) {
	    r->failure_mode = FACETIZE_FAILURE_BOTINVALID;
	    if (bot->vertices) bu_free(bot->vertices, "verts");
	    if (bot->faces) bu_free(bot->faces, "verts");
	    ret = FACETIZE_FAILURE;
	    if (!opts->quiet) {
		bu_log("CM: facetization failed, final BoT was not solid\n");
	    }
	    goto ged_facetize_continuation_memfree;
	}
    }

    if (decimation_succeeded && !opts->quiet) {
	bu_log("CM: decimation succeeded, final BoT has %d faces\n", (int)bot->num_faces);
    }

    if (!opts->make_nmg) {

	ret = _write_bot(gedp, bot, newname, opts);

    } else {
	/* Convert BoT to NMG */
	struct model *m = nmg_mm();
	struct nmgregion *nr;
	struct rt_db_internal intern;

	/* Use intern to fake out rt_bot_tess, since it expects an
	 * rt_db_internal wrapper */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_ptr = (void *)bot;
	if (rt_bot_tess(&nr, m, &intern, NULL, &btol) < 0) {
	    rt_db_free_internal(&intern);
	    ret = FACETIZE_FAILURE;
	    r->failure_mode = FACETIZE_FAILURE_NMG;
	    goto ged_facetize_continuation_memfree;
	} else {
	    /* OK, have NMG now - write it out */
	    ret = _write_nmg(gedp, m, newname, opts);
	    rt_db_free_internal(&intern);
	}
    }

ged_facetize_continuation_memfree:
    r->feature_size = feature_size;
    r->avg_thickness = avg_thickness;

    if (free_pnts && pnts) {
	struct pnt_normal *rpnt = (struct pnt_normal *)pnts->point;
	if (rpnt) {
	    struct pnt_normal *entry;
	    while (BU_LIST_WHILE(entry, pnt_normal, &(rpnt->l))) {
		BU_LIST_DEQUEUE(&(entry->l));
		BU_PUT(entry, struct pnt_normal);
	    }
	    BU_PUT(rpnt, struct pnt_normal);
	}
	bu_free(pnts, "free pnts");
    }
    rt_db_free_internal(&in_intern);

    return ret;
}


static int
_ged_nmg_obj(struct ged *gedp, int argc, const char **argv, const char *newname, struct _old_ged_facetize_opts *opts)
{
    int ret = FACETIZE_SUCCESS;
    struct model *nmg_model = NULL;
    struct rt_bot_internal *bot = NULL;

    nmg_model = _try_nmg_facetize(gedp, argc, argv, opts->nmg_use_tnurbs, opts);
    if (nmg_model == NULL) {
	if (opts->verbosity > 1) {
	    bu_log("NMG(%s):  no resulting region, aborting\n", newname);
	}
	ret = FACETIZE_FAILURE;
	goto ged_nmg_obj_memfree;
    }

    /* Triangulate model, if requested */
    if (opts->triangulate && opts->make_nmg) {
	if (_try_nmg_triangulate(gedp, nmg_model, opts) != BRLCAD_OK) {
	    if (opts->verbosity > 1) {
		bu_log("NMG(%s):  triangulation failed, aborting\n", newname);
	    }
	    ret = FACETIZE_FAILURE;
	    goto ged_nmg_obj_memfree;
	}
    }

    if (!opts->make_nmg) {

	/* Make and write out the bot */
	bot = _try_nmg_to_bot(gedp, nmg_model, opts);

	if (!bot) {
	    if (opts->verbosity > 1) {
		bu_log("NMG(%s): conversion to BOT failed, aborting\n", newname);
	    }
	    ret = FACETIZE_FAILURE;
	    goto ged_nmg_obj_memfree;
	}

	ret = _write_bot(gedp, bot, newname, opts);

    } else {

	/* Just write the NMG */
	ret = _write_nmg(gedp, nmg_model, newname, opts);

    }

ged_nmg_obj_memfree:
    if (!opts->quiet && ret != BRLCAD_OK) {
	bu_log("NMG: failed to generate %s\n", newname);
    }

    return ret;
}


struct manifold_mesh {
    int num_vertices;
    int num_faces;
    fastf_t *vertices;
    unsigned int *faces;
};


static struct manifold_mesh *
bot_to_mmesh(struct rt_bot_internal *bot)
{
    struct manifold_mesh *omesh = NULL;
    BU_GET(omesh, struct manifold_mesh);
    omesh->num_vertices = bot->num_vertices;
    omesh->num_faces = bot->num_faces;
    omesh->faces = (unsigned int *)bu_calloc(bot->num_faces * 3, sizeof(unsigned int), "faces array");
    for (size_t i = 0; i < bot->num_faces * 3; i++) {
	omesh->faces[i] = bot->faces[i];
    }
    omesh->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "vert array");
    for (size_t i = 0; i < bot->num_vertices * 3; i++) {
	omesh->vertices[i] = bot->vertices[i];
    }
    return omesh;
}


long
bool_meshes(
    double **o_coords, int *o_ccnt, unsigned int **o_tris, int *o_tricnt,
    manifold::OpType b_op,
    double *a_coords, int a_ccnt, unsigned int *a_tris, int a_tricnt,
    double *b_coords, int b_ccnt, unsigned int *b_tris, int b_tricnt,
    const char *lname, const char *rname)
{
    if (!o_coords || !o_ccnt || !o_tris || !o_tricnt)
	return -1;

    manifold::MeshGL64 a_mesh;
    for (int i = 0; i < a_ccnt*3; i++)
	a_mesh.vertProperties.insert(a_mesh.vertProperties.end(), a_coords[i]);
    for (int i = 0; i < a_tricnt*3; i++)
	a_mesh.triVerts.insert(a_mesh.triVerts.end(), a_tris[i]);
    manifold::MeshGL64 b_mesh;
    for (int i = 0; i < b_ccnt*3; i++)
	b_mesh.vertProperties.insert(b_mesh.vertProperties.end(), b_coords[i]);
    for (int i = 0; i < b_tricnt*3; i++)
	b_mesh.triVerts.insert(b_mesh.triVerts.end(), b_tris[i]);

    manifold::Manifold a_manifold(a_mesh);
    if (a_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - a invalid\n");
	return -1;
    }
    manifold::Manifold b_manifold(b_mesh);
    if (b_manifold.Status() != manifold::Manifold::Error::NoError) {
	bu_log("Error - b invalid\n");
	return -1;
    }

#if 0
#ifdef USE_ASSETIMPORT
    std::cerr << lname << " " << (int)b_op << " " << rname << "\n";
    std::remove("left.glb");
    std::remove("right.glb");
    manifold::ExportMesh(std::string("left.glb"), a_manifold.GetMeshGL(), {});
    manifold::ExportMesh(std::string("right.glb"), b_manifold.GetMeshGL(), {});
#endif
#endif

    manifold::Manifold result;
    try {
	result = a_manifold.Boolean(b_manifold, b_op);
	if (result.Status() != manifold::Manifold::Error::NoError) {
	    bu_log("Error - bool result invalid\n");
	    return -1;
	}
    } catch (...) {
	bu_log("Manifold boolean library threw failure\n");
#ifdef USE_ASSETIMPORT
	const char *evar = getenv("GED_MANIFOLD_DEBUG");
	// write out the failing inputs to files to aid in debugging
	if (evar && strlen(evar)) {
	    std::cerr << "Manifold op: " << (int)b_op << "\n";
	    manifold::ExportMesh(std::string(lname)+std::string(".glb"), a_manifold.GetMeshGL(), {});
	    manifold::ExportMesh(std::string(rname)+std::string(".glb"), b_manifold.GetMeshGL(), {});
	    bu_exit(EXIT_FAILURE, "Exiting to avoid overwriting debug outputs from Manifold boolean failure.\n");
	}
#endif
	return -1;
    }
    manifold::MeshGL64 rmesh = result.GetMeshGL64();

    (*o_coords) = (double *)calloc(rmesh.vertProperties.size(), sizeof(double));
    (*o_tris) = (unsigned int *)calloc(rmesh.triVerts.size(), sizeof(unsigned int));
    for (size_t i = 0; i < rmesh.vertProperties.size(); i++)
	(*o_coords)[i] = rmesh.vertProperties[i];
    for (size_t i = 0; i < rmesh.triVerts.size(); i++)
	(*o_tris)[i] = rmesh.triVerts[i];
    *o_ccnt = (int)rmesh.vertProperties.size()/3;
    *o_tricnt = (int)rmesh.triVerts.size()/3;


    int not_solid = bg_trimesh_solid2(*o_ccnt, *o_tricnt, (fastf_t *)*o_coords, (int *)*o_tris, NULL);
    if (not_solid) {
	*o_ccnt = 0;
	*o_tricnt = 0;
	bu_free(*o_coords, "coords");
	*o_coords = NULL;
	bu_free(*o_tris, "tris");
	*o_tris = NULL;
	bu_log("Error: Manifold boolean succeeded, but result reports as non-solid: failure.\n");
	return -1;
    }

    return rmesh.triVerts.size();
}

static struct manifold_mesh *
do_mesh(union tree *tree, const struct bn_tol *tol)
{
    struct rt_bot_internal *bot = NULL;

    if (!tree->tr_d.td_r) {
	return (struct manifold_mesh *)tree->tr_d.td_d;
    }

    if (!BU_SETJUMP) {
	/* try */
	bot = (struct rt_bot_internal *)nmg_mdl_to_bot(tree->tr_d.td_r->m_p, &RTG.rtg_vlfree, tol);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bot = NULL;
    } BU_UNSETJUMP;

    if (bot) {
	struct manifold_mesh *mesh = bot_to_mmesh(bot);
	// We created this locally if it wasn't originally a BoT - clean up
	if (bot->vertices)
	    bu_free(bot->vertices, "verts");
	if (bot->faces)
	    bu_free(bot->faces, "faces");
	BU_FREE(bot, struct rt_bot_internal);
	bot = NULL;
	return mesh;
    }

    return (struct manifold_mesh *)tree->tr_d.td_d;
}

static int
_manifold_do_bool(
    union tree *tp, union tree *tl, union tree *tr,
    //int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *tol, void *data)
    int op, struct bu_list *UNUSED(vlfree), const struct bn_tol *tol, void *UNUSED(data))
{
    struct manifold_mesh *lmesh = NULL;
    struct manifold_mesh *rmesh = NULL;
    struct manifold_mesh *omesh = NULL;

    // We're either working with the results of CSG NMG tessellations,
    // or we have prior manifold_mesh results - figure out which.
    // If it's the NMG results, we need to make manifold_meshes for
    // processing.
    lmesh = do_mesh(tl, tol);
    rmesh = do_mesh(tr, tol);

    int failed = 0;
    if (!lmesh || !rmesh) {
	failed = 1;
    } else {

	// Translate op for MANIFOLD
	manifold::OpType manifold_op = manifold::OpType::Add;
	switch (op) {
	    case OP_UNION:
		manifold_op = manifold::OpType::Add;
		break;
	    case OP_INTERSECT:
		manifold_op = manifold::OpType::Intersect;
		break;
	    case OP_SUBTRACT:
		manifold_op = manifold::OpType::Subtract;
		break;
	    default:
		manifold_op = manifold::OpType::Add;
	};

	BU_GET(omesh, struct manifold_mesh);
	int mret = bool_meshes(
	    (double **)&omesh->vertices, &omesh->num_vertices, &omesh->faces, &omesh->num_faces,
	    manifold_op,
	    (double *)lmesh->vertices, lmesh->num_vertices, lmesh->faces, lmesh->num_faces,
	    (double *)rmesh->vertices, rmesh->num_vertices, rmesh->faces, rmesh->num_faces,
	    tl->tr_d.td_name,
	    tr->tr_d.td_name
	    );

	failed = (mret < 0) ? 1 : 0;

	if (failed && omesh) {
	    // TODO - we should be able to try an NMG boolean op here as a
	    // fallback, but we may need to translate one or both of the inputs
	    // into NMG
	    bu_free(omesh->faces, "faces");
	    bu_free(omesh->vertices, "vertices");
	    BU_PUT(omesh, struct manifold_mesh);
	    omesh = NULL;
	}
    }

    // Memory cleanup
    if (tl->tr_d.td_r) {
	nmg_km(tl->tr_d.td_r->m_p);
	tl->tr_d.td_r = NULL;
    }
    if (tr->tr_d.td_r) {
	nmg_km(tr->tr_d.td_r->m_p);
	tr->tr_d.td_r = NULL;
    }
    if (tl->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tl->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tl->tr_d.td_d = NULL;
    }
    if (tr->tr_d.td_d) {
	struct manifold_mesh *km = (struct manifold_mesh *)tr->tr_d.td_d;
	bu_free(km->faces, "faces");
	bu_free(km->vertices, "vertices");
	BU_PUT(km, struct manifold_mesh);
	tr->tr_d.td_d = NULL;
    }

    tp->tr_d.td_r = NULL;

    if (failed) {
	tp->tr_d.td_d = NULL;
	return -1;
    }

    tp->tr_op = OP_TESS;
    tp->tr_d.td_d = omesh;
    return 0;
}


static struct manifold_mesh *
_try_manifold_facetize(struct ged *gedp, int argc, const char **argv, struct _old_ged_facetize_opts *o)
{
    int i;
    union tree *ftree = NULL;

    _old_ged_facetize_log_nmg(o);

    struct db_tree_state init_state;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    db_init_db_tree_state(&init_state, gedp->dbip, wdbp->wdb_resp);
    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;
    init_state.ts_m = NULL;
    union tree *facetize_tree = (union tree *)0;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(gedp->dbip, argc, (const char **)argv,
			 1,
			 &init_state,
			 0,			/* take all regions */
			 facetize_region_end,
			 rt_booltree_leaf_tess,
			 (void *)&facetize_tree
	    );
    } else {
	/* catch */
	BU_UNSETJUMP;
	_old_ged_facetize_log_default(o);
	return NULL;
    } BU_UNSETJUMP;

    if (i < 0) {
	/* Destroy NMG */
	_old_ged_facetize_log_default(o);
	return NULL;
    }

    if (facetize_tree) {
	ftree = rt_booltree_evaluate(facetize_tree, &RTG.rtg_vlfree, &wdbp->wdb_tol, &rt_uniresource, &_manifold_do_bool, 0, (void *)o);
	if (!ftree) {
	    _old_ged_facetize_log_default(o);
	    return NULL;
	}
	if (ftree->tr_d.td_d) {
	    _old_ged_facetize_log_default(o);
	    return (struct manifold_mesh *)ftree->tr_d.td_d;
	} else if (ftree->tr_d.td_r) {
	    // If we had only one NMG mesh, there was no bool
	    // operation - we need to set up a mesh
	    struct rt_bot_internal *bot = NULL;
	    if (!BU_SETJUMP) {
		/* try */
		bot = (struct rt_bot_internal *)nmg_mdl_to_bot(ftree->tr_d.td_r->m_p, &RTG.rtg_vlfree, &wdbp->wdb_tol);
	    } else {
		/* catch */
		BU_UNSETJUMP;
		bot = NULL;
	    } BU_UNSETJUMP;

	    if (bot) {
		struct manifold_mesh *omesh = bot_to_mmesh(bot);
		// We created this locally if it wasn't originally a BoT - clean up
		if (bot->vertices)
		    bu_free(bot->vertices, "verts");
		if (bot->faces)
		    bu_free(bot->faces, "faces");
		BU_FREE(bot, struct rt_bot_internal);
		bot = NULL;
		_old_ged_facetize_log_default(o);
		return omesh;
	    }
	}
    }

    _old_ged_facetize_log_default(o);
    return NULL;
}


// For MANIFOLD, we do a tree walk.  Each solid is individually triangulated, and
// then the boolean op is applied with the result of the mesh generated thus
// far.  This is conceptually similar to how NMG does its processing, with the
// main difference being that the external code is actually doing the mesh
// boolean processing rather than libnmg.
static int
_ged_manifold_obj(struct ged *gedp, int argc, const char **argv, const char *newname, struct _old_ged_facetize_opts *opts)
{
    int ret = BRLCAD_ERROR;
    struct manifold_mesh *mesh = NULL;
    if (!gedp || !argc || !argv || !opts)
	goto ged_manifold_obj_memfree;

    mesh = _try_manifold_facetize(gedp, argc, argv, opts);

    if (mesh && mesh->num_faces > 0) {
	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = mesh->num_vertices;
	bot->num_faces = mesh->num_faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = 0;
	bot->num_face_normals = 0;
	bot->normals = NULL;
	bot->face_normals = NULL;
	bot->vertices = (fastf_t *)bu_calloc(mesh->num_vertices * 3, sizeof(fastf_t), "vertices");
	for (int i = 0; i < mesh->num_vertices * 3; i++) {
	    bot->vertices[i] = mesh->vertices[i];
	}
	bot->faces = (int *)bu_calloc(mesh->num_faces * 3, sizeof(int), "faces");
	for (int i = 0; i < mesh->num_faces * 3; i++) {
	    bot->faces[i] = mesh->faces[i];
	}

	ret = _write_bot(gedp, bot, newname, opts);
    } else {
	goto ged_manifold_obj_memfree;
    }

ged_manifold_obj_memfree:
    if (mesh) {
	if (mesh->vertices)
	    bu_free(mesh->vertices, "verts");
	if (mesh->faces)
	    bu_free(mesh->faces, "faces");
	BU_FREE(mesh, struct manifold_mesh);
    }
    if (!opts->quiet && ret != BRLCAD_OK) {
	bu_log("MANIFOLD: failed to generate %s\n", newname);
    }

    return ret;
}


static int
_ged_facetize_objlist(struct ged *gedp, int argc, const char **argv, struct _old_ged_facetize_opts *opts)
{
    int ret = BRLCAD_ERROR;
    int done_trying = 0;
    int newobj_cnt;
    char *newname;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    int flags = opts->method_flags;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct bg_tess_tol *tol = &(wdbp->wdb_ttol);

    RT_CHECK_DBI(dbip);

    if (argc < 0) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp->dbip, argc, argv, dpa);
    if (_ged_validate_objs_list(gedp, argc, argv, opts, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }

    if (!opts->in_place) {
	newname = (char *)argv[argc-1];
	argc--;
    } else {
	/* Find a new name for the original object - that's also our working
	 * "newname" for the initial processing, until we swap at the end. */
	bu_vls_sprintf(&oname, "%s_original", argv[0]);
	if (db_lookup(dbip, bu_vls_addr(&oname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(&oname, "-0");
	    bu_vls_incr(&oname, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
	}
	newname = (char *)bu_vls_addr(&oname);
    }

    /* Before we try this, check that all the objects in the specified tree(s) are valid solids */
    if (!_ged_facetize_solid_objs(gedp, argc, dpa, opts)) {
	if (flags & FACETIZE_SPSR) {
	    if (flags != FACETIZE_SPSR) {
		bu_log("non-solid objects in specified tree(s) - falling back on point sampling/reconstruction methodology\n");
	    }
	    flags = FACETIZE_SPSR;
	} else {
	    if (!opts->quiet) {
		bu_log("Facetization aborted: non-solid objects in specified tree(s).\n");
	    }
	    bu_free(dpa, "dp array");
	    return BRLCAD_ERROR;
	}
    }

    /* Done with dpa */
    bu_free(dpa, "dp array");

    while (!done_trying) {

	if (flags & FACETIZE_MANIFOLD) {
	    if (argc == 1) {
		bu_vls_sprintf(opts->nmg_log_header, "MANIFOLD: tessellating %s...\n", argv[0]);
	    } else {
		bu_vls_sprintf(opts->nmg_log_header, "MANIFOLD: tessellating %d objects with tolerances a=%g, r=%g, n=%g\n", argc, tol->abs, tol->rel, tol->norm);
	    }
	    /* Let the user know what's going on, unless output is suppressed */
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }

	    if (_ged_manifold_obj(gedp, argc, argv, newname, opts) == BRLCAD_OK) {
		ret = BRLCAD_OK;
		break;
	    } else {
		flags = flags & ~(FACETIZE_MANIFOLD);
		continue;
	    }
	}

	if (flags & FACETIZE_NMGBOOL) {
	    opts->nmg_log_print_header = 1;
	    if (argc == 1) {
		bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %s...\n", argv[0]);
	    } else {
		bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %d objects with tolerances a=%g, r=%g, n=%g\n", argc, tol->abs, tol->rel, tol->norm);
	    }
	    /* Let the user know what's going on, unless output is suppressed */
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }

	    if (_ged_nmg_obj(gedp, argc, argv, newname, opts) == BRLCAD_OK) {
		ret = BRLCAD_OK;
		break;
	    } else {
		flags = flags & ~(FACETIZE_NMGBOOL);
		continue;
	    }
	}

	if (flags & FACETIZE_CONTINUATION) {
	    if (argc != 1) {
		if (opts->verbosity) {
		    bu_log("Continuation mode (currently) only supports one existing object at a time as input - not attempting.\n");
		}
		flags = flags & ~(FACETIZE_CONTINUATION);
	    } else {
		struct _ged_facetize_report_info cinfo;
		if (_ged_continuation_obj(&cinfo, gedp, argv[0], newname, opts) == FACETIZE_SUCCESS) {
		    ret = BRLCAD_OK;
		    break;
		} else {
		    if (!opts->quiet) {
			struct bu_vls lmsg = BU_VLS_INIT_ZERO;
			_ged_facetize_failure_msg(&lmsg, cinfo.failure_mode, "CM", &cinfo);
			bu_log("%s", bu_vls_addr(&lmsg));
			bu_vls_free(&lmsg);
		    }
		    flags = flags & ~(FACETIZE_CONTINUATION);
		    continue;
		}
	    }
	}

	if (flags & FACETIZE_SPSR) {
	    if (argc != 1) {
		if (opts->verbosity) {
		    bu_log("Screened Poisson mode (currently) only supports one existing object at a time as input - not attempting.\n");
		}
		flags = flags & ~(FACETIZE_SPSR);
	    } else {
		struct _ged_facetize_report_info cinfo;
		if (_ged_spsr_obj(&cinfo, gedp, argv[0], newname, opts) == FACETIZE_SUCCESS) {
		    ret = BRLCAD_OK;
		    break;
		} else {
		    if (!opts->quiet) {
			struct bu_vls lmsg = BU_VLS_INIT_ZERO;
			_ged_facetize_failure_msg(&lmsg, cinfo.failure_mode, "SPSR", &cinfo);
			bu_log("%s", bu_vls_addr(&lmsg));
			bu_vls_free(&lmsg);
		    }
		    flags = flags & ~(FACETIZE_SPSR);
		    continue;
		}
	    }
	}

	/* Out of options */
	done_trying = 1;

    }

    if ((ret == BRLCAD_OK) && opts->in_place) {
	if (_ged_facetize_obj_swap(gedp, argv[0], newname) != BRLCAD_OK) {
	    return BRLCAD_ERROR;
	}
    }

    if (bu_vls_strlen(opts->nmg_log) && opts->method_flags & FACETIZE_NMGBOOL && opts->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(opts->nmg_log));
    }

    return ret;
}


static int
_ged_facetize_cpcomb(struct ged *gedp, const char *o, struct _old_ged_facetize_opts *opts)
{
    int ret = BRLCAD_OK;
    struct db_i *dbip = gedp->dbip;
    struct directory *dp;
    struct rt_db_internal ointern, intern;
    struct rt_comb_internal *ocomb, *comb;
    struct bu_attribute_value_set avs;
    int flags;

    /* Unpack original comb to get info to duplicate in new comb */
    dp = db_lookup(dbip, o, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB)) return BRLCAD_ERROR;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(dbip, &avs, dp)) return BRLCAD_ERROR;
    if (rt_db_get_internal(&ointern, dp, dbip, NULL, &rt_uniresource) < 0) return BRLCAD_ERROR;
    ocomb = (struct rt_comb_internal *)ointern.idb_ptr;
    RT_CK_COMB(ocomb);
    flags = dp->d_flags;

    /* Make a new empty comb with the same properties as the original, sans tree */
    RT_DB_INTERNAL_INIT(&intern);
    BU_ALLOC(comb, struct rt_comb_internal);
    RT_COMB_INTERNAL_INIT(comb);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_ptr = (void *)comb;
    intern.idb_meth = &OBJ[ID_COMBINATION];
    GED_DB_DIRADD(gedp, dp, bu_avs_get(opts->c_map, o), -1, 0, flags, (void *)&intern.idb_type, 0);
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

    rt_db_free_internal(&ointern);

    return ret;
}


static int
_ged_methodcomb_add(struct ged *gedp, struct _old_ged_facetize_opts *opts, const char *objname, int method)
{
    int ret = BRLCAD_OK;
    struct bu_vls method_cname = BU_VLS_INIT_ZERO;
    if (!objname || method == FACETIZE_NULL) return BRLCAD_ERROR;

    if (method == FACETIZE_MANIFOLD && !bu_vls_strlen(opts->manifold_comb)) {
	bu_vls_sprintf(opts->manifold_comb, "%s_MANIFOLD-0", bu_vls_addr(opts->froot));
	bu_vls_incr(opts->manifold_comb, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
    }
    if (method == FACETIZE_NMGBOOL && !bu_vls_strlen(opts->nmg_comb)) {
	bu_vls_sprintf(opts->nmg_comb, "%s_NMGBOOL-0", bu_vls_addr(opts->froot));
	bu_vls_incr(opts->nmg_comb, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
    }
    if (method == FACETIZE_CONTINUATION && !bu_vls_strlen(opts->continuation_comb)) {
	bu_vls_sprintf(opts->continuation_comb, "%s_CONTINUATION-0", bu_vls_addr(opts->froot));
	bu_vls_incr(opts->continuation_comb, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
    }
    if (method == FACETIZE_SPSR && !bu_vls_strlen(opts->spsr_comb)) {
	bu_vls_sprintf(opts->spsr_comb, "%s_SPSR-0", bu_vls_addr(opts->froot));
	bu_vls_incr(opts->spsr_comb, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
    }

    switch (method) {
	case FACETIZE_NMGBOOL:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(opts->nmg_comb));
	    break;
	case FACETIZE_CONTINUATION:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(opts->continuation_comb));
	    break;
	case FACETIZE_SPSR:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(opts->spsr_comb));
	    break;
	case FACETIZE_MANIFOLD:
	    bu_vls_sprintf(&method_cname, "%s", bu_vls_addr(opts->manifold_comb));
	    break;
	default:
	    bu_vls_free(&method_cname);
	    return BRLCAD_ERROR;
	    break;
    }

    ret =_ged_combadd2(gedp, bu_vls_addr(&method_cname), 1, (const char **)&objname, 0, DB_OP_UNION, 0, 0, NULL, 0);
    bu_vls_free(&method_cname);
    return ret;
}


static void
_ged_methodattr_set(struct ged *gedp, struct _old_ged_facetize_opts *opts, const char *rcname, int method, struct _ged_facetize_report_info *info)
{
    struct bu_vls anum = BU_VLS_INIT_ZERO;
    const char *attrav[5];
    attrav[0] = "attr";
    attrav[1] = "set";
    attrav[2] = rcname;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (method == FACETIZE_NMGBOOL) {
	struct bg_tess_tol *tol = &(wdbp->wdb_ttol);
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_abs";
	bu_vls_sprintf(&anum, "%g", tol->abs);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_rel";
	bu_vls_sprintf(&anum, "%g", tol->rel);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:nmg_norm";
	bu_vls_sprintf(&anum, "%g", tol->norm);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (info && method == FACETIZE_CONTINUATION) {
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:continuation_feature_size";
	bu_vls_sprintf(&anum, "%g", info->feature_size);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:continuation_average_thickness";
	bu_vls_sprintf(&anum, "%g", info->avg_thickness);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (method == FACETIZE_SPSR) {
	attrav[3] = _ged_facetize_attr(method);
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_depth";
	bu_vls_sprintf(&anum, "%d", opts->s_opts.depth);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_weight";
	bu_vls_sprintf(&anum, "%g", opts->s_opts.point_weight);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
	attrav[3] = "facetize:spsr_samples_per_node";
	bu_vls_sprintf(&anum, "%g", opts->s_opts.samples_per_node);
	attrav[4] = bu_vls_addr(&anum);
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    if (info && info->failure_mode == FACETIZE_FAILURE_PNTGEN) {
	attrav[3] = "facetize:EMPTY";
	attrav[4] = "1";
	if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
	    bu_log("Error adding attribute %s to comb %s", attrav[3], rcname);
	}
    }

    bu_vls_free(&anum);
}


static int
_ged_facetize_region_obj(struct ged *gedp, const char *oname, const char *sname, struct _old_ged_facetize_opts *opts, int ocnt, int max_cnt, int cmethod, struct _ged_facetize_report_info *cinfo)
{
    int ret = FACETIZE_FAILURE;
    struct directory *dp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);

    if (dp == RT_DIR_NULL) {
	return BRLCAD_ERROR;
    }

    if (cmethod == FACETIZE_MANIFOLD) {

	/* We're staring a new object, so we want to write out the header in the
	 * log file the first time we get an NMG logging event.  (Re)set the flag
	 * so the logger knows to do so. */
	opts->nmg_log_print_header = 1;
	bu_vls_sprintf(opts->nmg_log_header, "MANIFOLD: tessellating %s (%d of %d) with tolerances a=%g, r=%g, n=%g\n", oname, ocnt, max_cnt, opts->tol->abs, opts->tol->rel, opts->tol->norm);

	/* Let the user know what's going on, unless output is suppressed */
	if (!opts->quiet) {
	    bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	}

	ret = _ged_manifold_obj(gedp, 1, (const char **)&oname, sname, opts);

	if (ret != FACETIZE_FAILURE) {
	    if (_ged_methodcomb_add(gedp, opts, sname, FACETIZE_MANIFOLD) != BRLCAD_OK && opts->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    if (cmethod == FACETIZE_NMGBOOL) {

	/* We're staring a new object, so we want to write out the header in the
	 * log file the first time we get an NMG logging event.  (Re)set the flag
	 * so the logger knows to do so. */
	opts->nmg_log_print_header = 1;
	bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %s (%d of %d) with tolerances a=%g, r=%g, n=%g\n", oname, ocnt, max_cnt, opts->tol->abs, opts->tol->rel, opts->tol->norm);

	/* Let the user know what's going on, unless output is suppressed */
	if (!opts->quiet) {
	    bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	}

	ret = _ged_nmg_obj(gedp, 1, (const char **)&oname, sname, opts);

	if (ret != FACETIZE_FAILURE) {
	    if (_ged_methodcomb_add(gedp, opts, sname, FACETIZE_NMGBOOL) != BRLCAD_OK && opts->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    if (cmethod == FACETIZE_CONTINUATION) {

	if (!opts->quiet) {
	    bu_log("CM: tessellating %s (%d of %d)\n", oname, ocnt, max_cnt);
	}

	ret = _ged_continuation_obj(cinfo, gedp, oname, sname, opts);
	if (ret == FACETIZE_FAILURE) {
	    if (!opts->quiet) {
		struct bu_vls lmsg = BU_VLS_INIT_ZERO;
		_ged_facetize_failure_msg(&lmsg, cinfo->failure_mode, "CM", cinfo);
		bu_log("%s", bu_vls_addr(&lmsg));
		bu_vls_free(&lmsg);
	    }
	} else {
	    if (_ged_methodcomb_add(gedp, opts, sname, FACETIZE_CONTINUATION) != BRLCAD_OK && opts->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    if (cmethod == FACETIZE_SPSR) {

	if (!opts->quiet) {
	    bu_log("SPSR: tessellating %s (%d of %d)\n", oname, ocnt, max_cnt);
	}

	if (opts->verbosity) {
	    bu_log("SPSR: tessellating %s with depth %d, interpolation weight %g, and samples-per-node %g\n", oname, opts->s_opts.depth, opts->s_opts.point_weight, opts->s_opts.samples_per_node);
	}

	ret =_ged_spsr_obj(cinfo, gedp, oname, sname, opts);
	if (ret == FACETIZE_FAILURE) {
	    if (!opts->quiet) {
		struct bu_vls lmsg = BU_VLS_INIT_ZERO;
		_ged_facetize_failure_msg(&lmsg, cinfo->failure_mode, "SPSR", cinfo);
		bu_log("%s", bu_vls_addr(&lmsg));
		bu_vls_free(&lmsg);
	    }
	} else {
	    if (_ged_methodcomb_add(gedp, opts, sname, FACETIZE_SPSR) != BRLCAD_OK && opts->verbosity > 1) {
		bu_log("Error adding %s to methodology combination\n", sname);
	    }
	}

	return ret;
    }

    return FACETIZE_FAILURE;
}


static int
_ged_facetize_regions_resume(struct ged *gedp, int argc, const char **argv, struct _old_ged_facetize_opts *opts)
{
    int to_convert = 0;
    int methods = opts->method_flags;
    unsigned int i = 0;
    int newobj_cnt = 0;
    int ret = BRLCAD_OK;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl *ar2 = NULL;
    const char *resume_regions = "-attr facetize:original_region";
    struct directory **dpa = NULL;
    struct bu_attribute_value_set rnames;
    struct bu_attribute_value_set bnames;
    struct db_i *dbip = gedp->dbip;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (!argc) return BRLCAD_ERROR;

    bu_avs_init_empty(&bnames);
    bu_avs_init_empty(&rnames);

    /* Use the first object name for the root */
    bu_vls_sprintf(opts->froot, "%s", argv[0]);

    /* Used the libged tolerances */
    opts->tol = &(wdbp->wdb_ttol);

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp->dbip, argc, argv, dpa);
    if (newobj_cnt) {
	bu_vls_sprintf(gedp->ged_result_str, "one or more new object names supplied to resume.");
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, resume_regions, argc, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_resume_memfree;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (possible), nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_resume_memfree;
    }

    /* Only work on regions with conversion information */
    BU_ALLOC(ar2, struct bu_ptbl);
    bu_ptbl_init(ar2, 8, "second table");
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	struct bu_attribute_value_set avs;
	const char *rname;
	const char *bname;
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, n)) continue;
	rname = bu_avs_get(&avs, "facetize:original_region");
	bname = bu_avs_get(&avs, "facetize:target_name");
	if (!rname || !bname) {
	    bu_avs_free(&avs);
	    continue;
	}
	bu_avs_add(&bnames, n->d_namep, bname);
	bu_avs_add(&rnames, n->d_namep, rname);
	bu_ptbl_ins(ar2, (long *)n);
	bu_avs_free(&avs);
    }

    to_convert = BU_PTBL_LEN(ar2);
    if (!to_convert) {
	/* No regions with conversion information, nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_resume_memfree;
    }

    while (methods && BU_PTBL_LEN(ar2) > 0) {
	struct bu_ptbl *tmp;
	int cmethod = 0;
	ssize_t avail_mem = 0;
	bu_ptbl_reset(ar);


	if (!cmethod && (methods & FACETIZE_MANIFOLD)) {
	    cmethod = FACETIZE_MANIFOLD;
	    methods = methods & ~(FACETIZE_MANIFOLD);
	}

	if (!cmethod && (methods & FACETIZE_NMGBOOL)) {
	    cmethod = FACETIZE_NMGBOOL;
	    methods = methods & ~(FACETIZE_NMGBOOL);
	}

	if (!cmethod && (methods & FACETIZE_CONTINUATION)) {
	    cmethod = FACETIZE_CONTINUATION;
	    methods = methods & ~(FACETIZE_CONTINUATION);
	}

	if (!cmethod && (methods & FACETIZE_SPSR)) {
	    cmethod = FACETIZE_SPSR;
	    methods = methods & ~(FACETIZE_SPSR);
	}

	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *cname = n->d_namep;
	    const char *sname = bu_avs_get(&bnames, cname);
	    const char *oname = bu_avs_get(&rnames, cname);
	    struct directory *dp = db_lookup(dbip, sname, LOOKUP_QUIET);
	    struct _ged_facetize_report_info cinfo;

	    if (dp == RT_DIR_NULL) {
		if (opts->retry || !_ged_facetize_attempted(gedp, cname, cmethod)) {
		    /* Before we try this (unless we're point sampling), check that all the objects in the specified tree(s) are valid solids */
		    struct directory *odp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);

		    /* Regardless of the outcome, record what settings were tried. */
		    _ged_methodattr_set(gedp, opts, cname, cmethod, &cinfo);

		    if (odp == RT_DIR_NULL || (!_ged_facetize_solid_objs(gedp, 1, &odp, opts) && cmethod != FACETIZE_SPSR)) {
			if (!opts->quiet) {
			    bu_log("%s: non-solid objects in specified tree(s) - cannot apply facetization method %s\n", oname, _ged_facetize_attr(cmethod));
			}
			bu_ptbl_ins(ar, (long *)n);
			continue;
		    }

		    if (_ged_facetize_region_obj(gedp, oname, sname, opts, i+1, (int)BU_PTBL_LEN(ar2), cmethod, &cinfo) == FACETIZE_FAILURE) {
			bu_ptbl_ins(ar, (long *)n);

			avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
			if (avail_mem > 0 && avail_mem < FACETIZE_MEMORY_THRESHOLD) {
			    bu_log("Too little available memory to continue, aborting\n");
			    ret = BRLCAD_ERROR;
			    goto ged_facetize_regions_resume_memfree;
			}
		    }
		} else {
		    bu_ptbl_ins(ar, (long *)n);
		}
	    }
	}

	tmp = ar;
	ar = ar2;
	ar2 = tmp;
    }

    /* Stash the failures */
    if (BU_PTBL_LEN(ar2) > 0) {
	/* Stash any failed regions into a top level comb for easy subsequent examination */
	const char **avv = (const char **)bu_calloc(BU_PTBL_LEN(ar2)+1, sizeof(char *), "argv array");
	struct bu_vls failed_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&failed_name, "%s_FAILED-0", argv[0]);
	bu_vls_incr(&failed_name, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *oname = bu_avs_get(&rnames, n->d_namep);
	    avv[i] = oname;
	}
	ret = _ged_combadd2(gedp, bu_vls_addr(&failed_name), (int)BU_PTBL_LEN(ar2), avv, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_vls_free(&failed_name);
	bu_free(avv, "argv array");
    }

ged_facetize_regions_resume_memfree:

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    if (bu_vls_strlen(opts->nmg_log) && opts->method_flags & FACETIZE_NMGBOOL && opts->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(opts->nmg_log));
    }

    /* Final report */
    bu_vls_printf(gedp->ged_result_str, "Objects successfully converted: %d of %d\n", (int)(to_convert - BU_PTBL_LEN(ar2)), to_convert);
    if (BU_PTBL_LEN(ar2)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: %d objects failed:\n", (int)BU_PTBL_LEN(ar2));
	for (i = 0; i < BU_PTBL_LEN(ar2); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar2, i);
	    const char *oname = bu_avs_get(&rnames, n->d_namep);
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", oname);
	}
    }

    bu_avs_free(&bnames);
    bu_avs_free(&rnames);
    if (ar2) {
	bu_ptbl_free(ar2);
	bu_free(ar2, "ar table");
    }
    if (ar) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
    }
    bu_free(dpa, "free dpa");

    return ret;
}


static int
_ged_facetize_add_children(struct ged *gedp, struct directory *cdp, struct _old_ged_facetize_opts *opts)
{
    int i = 0;
    int ret = BRLCAD_OK;
    struct db_i *dbip = gedp->dbip;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    struct directory **children = NULL;
    int child_cnt = 0;
    int *bool_ops = NULL;
    matp_t *mats = NULL;
    int non_ident_mat = 0;
    int non_union_bool = 0;
    const char *nparent;

    RT_DB_INTERNAL_INIT(&intern);
    nparent = bu_avs_get(opts->c_map, cdp->d_namep);
    if (rt_db_get_internal(&intern, cdp, dbip, NULL, &rt_uniresource) < 0) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_add_children_memfree;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    child_cnt = db_comb_children(dbip, comb, &children, &bool_ops, &mats);
    if (child_cnt <= 0) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_add_children_memfree;
    }

    /* See if anything fancy is going in with the comb children... */
    for (i = 0; i < child_cnt; i++) {
	if (mats[i] && !bn_mat_is_identity(mats[i])) non_ident_mat++;
    }
    for (i = 0; i < child_cnt; i++) {
	if (_int_to_opt(bool_ops[i]) != DB_OP_UNION) non_union_bool++;
    }

    if (non_ident_mat || non_union_bool) {
	/* More complicated comb, have to rebuild item by item */
	for (i = 0; i < child_cnt; i++) {
	    matp_t m = NULL;
	    const char *nc = bu_avs_get(opts->c_map, children[i]->d_namep);
	    if (!nc) {
		nc = bu_avs_get(opts->s_map, children[i]->d_namep);
	    }
	    if (!nc) {
		bu_log("Error - object %s has no name mapping??\n", children[i]->d_namep);
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	    m = (mats[i]) ? mats[i] : NULL;
	    if (_ged_combadd2(gedp, (char *)nparent, 1, (const char **)&nc, 0, _int_to_opt(bool_ops[i]), 0, 0, m, 0) != BRLCAD_OK) {
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	}
    } else {
	/* Simple comb, rebuild in one shot */
	const char **av = (const char **)bu_calloc(child_cnt, sizeof(const char *), "av array");
	for (i = 0; i < child_cnt; i++) {
	    av[i] = bu_avs_get(opts->c_map, children[i]->d_namep);
	    if (!av[i]) {
		av[i] = bu_avs_get(opts->s_map, children[i]->d_namep);
	    }
	    if (!av[i]) {
		bu_log("Error - object %s has no name mapping??\n", children[i]->d_namep);
		ret = BRLCAD_ERROR;
		goto ged_facetize_add_children_memfree;
	    }
	}
	ret = _ged_combadd2(gedp, (char *)nparent, child_cnt, av, 0, DB_OP_UNION, 0, 0, NULL, 0);
    }

ged_facetize_add_children_memfree:

    if (mats) {
	for (i = 0; i < child_cnt; i++) {
	    if (mats[i]) {
		bu_free(mats[i], "free matrix");
	    }
	}
	bu_free(mats, "free mats array");
    }

    if (bool_ops) {
	bu_free(bool_ops, "free ops");
    }

    bu_free(children, "free children struct directory ptr array");

    return ret;
}


static int
_ged_facetize_regions(struct ged *gedp, int argc, const char **argv, struct _old_ged_facetize_opts *opts)
{
    int to_convert = 0;
    int methods = opts->method_flags;
    char *newname = NULL;
    int newobj_cnt = 0;
    int ret = BRLCAD_OK;
    unsigned int i = 0;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_ptbl *pc = NULL;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl *ar2 = NULL;

    /* We need to copy combs above regions that are not themselves regions.
     * Also, facetize will need all "active" regions that will define shapes.
     * Construct searches to get these sets. */
    const char *preserve_combs = "-type c ! -type r ! -below -type r";
    const char *active_regions = "( -type r ! -below -type r ) -or ( ! -below -type r ! -type comb )";

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    opts->tol = &(wdbp->wdb_ttol);

    if (!argc) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp->dbip, argc, argv, dpa);
    if (_ged_validate_objs_list(gedp, argc, argv, opts, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    if (!opts->in_place) {
	newname = (char *)argv[argc-1];
	argc--;
    }

    if (!newname) {
	bu_vls_sprintf(opts->froot, "%s", argv[0]);
    } else {
	/* Use the new name for the root */
	bu_vls_sprintf(opts->froot, "%s", newname);
    }


    /* Set up mapping names for the original toplevel object(s).  If we have
     * top level solids, deal with them now. */
    for (i = 0; i < (unsigned int)argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    /* solid object in list at top level - handle directly */
	    _ged_facetize_mkname(gedp, opts, argv[i], SOLID_OBJ_NAME);

	    /* Let the user know what's going on, unless output is suppressed */
	    bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating solid %s with tolerances a=%g, r=%g, n=%g\n", argv[0], opts->tol->abs, opts->tol->rel, opts->tol->norm);
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }
	    opts->nmg_log_print_header = 1;

	    if (_ged_nmg_obj(gedp, 1, (const char **)&argv[i], bu_avs_get(opts->s_map, argv[i]), opts) != BRLCAD_OK) {
		return BRLCAD_ERROR;
	    }
	}
    }


    /* Find assemblies and regions */
    BU_ALLOC(pc, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, preserve_combs, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for parent combs - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_memfree;
    }
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	ret = BRLCAD_ERROR;
	goto ged_facetize_regions_memfree;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (unlikely but technically possible), nothing to do */
	ret = BRLCAD_OK;
	goto ged_facetize_regions_memfree;
    }


    /* Set up all the names we will need */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	/* Regions will have a name mapping both to a new region comb AND a facetized
	 * solid object - set up both names, and create the region combs */
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);

	_ged_facetize_mkname(gedp, opts, n->d_namep, SOLID_OBJ_NAME);

	/* Only generate a comb name if the "region" is actually a comb...
	 * this may not be true for solids with no regions above them. */
	if ((n->d_flags & RT_DIR_COMB)) {
	    _ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
	}
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);
	_ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
    }


    /* First, add the new toplevel comb to hold all the new geometry */
    if (!opts->in_place) {
	const char **ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
	for (i = 0; i < (unsigned int)argc; i++) {
	    ntop[i] = bu_avs_get(opts->c_map, argv[i]);
	    if (!ntop[i]) {
		ntop[i] = bu_avs_get(opts->s_map, argv[i]);
	    }
	}
	if (!opts->quiet) {
	    bu_log("Creating new top level assembly object %s...\n", newname);
	}
	ret = _ged_combadd2(gedp, newname, argc, ntop, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_free(ntop, "new top level names");
    }


    /* For the assemblies, make new versions with the suffixed names */
    if (!opts->quiet) {
	bu_log("Initializing copies of assembly combinations...\n");
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *cdp = RT_DIR_NULL;
	const char *nparent;
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);

	if (_ged_facetize_cpcomb(gedp, n->d_namep, opts) != BRLCAD_OK) {
	    if (opts->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(opts->c_map, n->d_namep), n->d_namep);
	    }
	    continue;
	}

	/* Add the members from the map with the settings from the original
	 * comb */
	cdp = (struct directory *)BU_PTBL_GET(pc, i);
	nparent = bu_avs_get(opts->c_map, cdp->d_namep);
	if (!opts->quiet) {
	    bu_log("Duplicating assembly (%d of %zu) %s -> %s\n", i+1, BU_PTBL_LEN(pc), cdp->d_namep, nparent);
	}
	if (_ged_facetize_add_children(gedp, cdp, opts) != BRLCAD_OK) {
	    if (!opts->quiet) {
		bu_log("Error: duplication of assembly %s failed!\n", cdp->d_namep);
	    }
	    continue;
	}
    }

    /* For regions, make the new region comb and add a reference to the to-be-created solid */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	_ged_facetize_mkname(gedp, opts, n->d_namep, SOLID_OBJ_NAME);
	_ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
	if (!opts->quiet) {
	    bu_log("Copying region (%d of %d) %s -> %s\n", (int)(i+1), (int)BU_PTBL_LEN(ar), n->d_namep, bu_avs_get(opts->c_map, n->d_namep));
	}
	if (_ged_facetize_cpcomb(gedp, n->d_namep, opts) != BRLCAD_OK) {
	    if (opts->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(opts->c_map, n->d_namep), n->d_namep);
	    }
	} else {
	    const char *rcname = bu_avs_get(opts->c_map, n->d_namep);
	    const char *ssname = bu_avs_get(opts->s_map, n->d_namep);
	    if (_ged_combadd2(gedp, (char *)rcname, 1, (const char **)&ssname, 0, DB_OP_UNION, 0, 0, NULL, 0) != BRLCAD_OK) {
		if (opts->verbosity) {
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
	    } else {
		/* By default, store the original region name and target bot name in attributes to make resuming easier */
		const char *attrav[5];
		attrav[0] = "attr";
		attrav[1] = "set";
		attrav[2] = rcname;
		attrav[3] = "facetize:original_region";
		attrav[4] = n->d_namep;
		if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
		    bu_log("Error adding attribute facetize_original_region to comb %s", rcname);
		}
		attrav[3] = "facetize:target_name";
		attrav[4] = ssname;
		if (ged_exec(gedp, 5, (const char **)&attrav) != BRLCAD_OK && opts->verbosity) {
		    bu_log("Error adding attribute facetize_target_name to comb %s", rcname);
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
	    }
	}
    }


    /* Now, actually trigger the facetize logic on each region.
     *
     * TODO - someday this should be done in parallel, but that's one deep
     * rabbit hole - for now, just try them in order and make sure we can
     * handle (non-crashing) failures to convert sanely. */
    BU_ALLOC(ar2, struct bu_ptbl);
    bu_ptbl_init(ar2, 8, "second table");
    to_convert = BU_PTBL_LEN(ar);

    while (methods && BU_PTBL_LEN(ar) > 0) {
	struct bu_ptbl *tmp;
	int cmethod = 0;
	ssize_t avail_mem = 0;
	bu_ptbl_reset(ar2);

#ifdef USE_MANIFOLD
	if (!cmethod && (methods & FACETIZE_MANIFOLD)) {
	    cmethod = FACETIZE_MANIFOLD;
	    methods = methods & ~(FACETIZE_MANIFOLD);
	}
#endif

	if (!cmethod && (methods & FACETIZE_NMGBOOL)) {
	    cmethod = FACETIZE_NMGBOOL;
	    methods = methods & ~(FACETIZE_NMGBOOL);
	}

	if (!cmethod && (methods & FACETIZE_CONTINUATION)) {
	    cmethod = FACETIZE_CONTINUATION;
	    methods = methods & ~(FACETIZE_CONTINUATION);
	}

	if (!cmethod && (methods & FACETIZE_SPSR)) {
	    cmethod = FACETIZE_SPSR;
	    methods = methods & ~(FACETIZE_SPSR);
	}

	if (!cmethod) {
	    bu_log("Error: methods isn't empty (%d), but no method selected??\n", methods);
	    break;
	}

	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    const char *oname = n->d_namep;
	    const char *cname = bu_avs_get(opts->c_map, oname);
	    const char *sname = bu_avs_get(opts->s_map, oname);
	    struct directory *dp = db_lookup(dbip, sname, LOOKUP_QUIET);

	    if (dp == RT_DIR_NULL) {
		/* Before we try this (unless we're point sampling), check that all the objects in the specified tree(s) are valid solids */
		struct directory *odp = db_lookup(gedp->dbip, oname, LOOKUP_QUIET);
		struct _ged_facetize_report_info cinfo = FACETIZE_REPORT_INFO_INIT;

		/* Regardless of the outcome, record what settings were tried. */
		_ged_methodattr_set(gedp, opts, cname, cmethod, &cinfo);

		if (odp == RT_DIR_NULL || (!_ged_facetize_solid_objs(gedp, 1, &odp, opts) && cmethod != FACETIZE_SPSR)) {
		    if (!opts->quiet) {
			bu_log("%s: non-solid objects in specified tree(s) - cannot apply facetization method %s\n", oname, _ged_facetize_attr(cmethod));
		    }
		    bu_ptbl_ins(ar2, (long *)n);
		    continue;
		}

		if (_ged_facetize_region_obj(gedp, oname, sname, opts, i+1, (int)BU_PTBL_LEN(ar), cmethod, &cinfo) == FACETIZE_FAILURE) {
		    bu_ptbl_ins(ar2, (long *)n);

		    avail_mem = bu_mem(BU_MEM_AVAIL, NULL);
		    if (avail_mem > 0 && avail_mem < FACETIZE_MEMORY_THRESHOLD) {
			bu_log("Too little available memory to continue, aborting\n");
			ret = BRLCAD_ERROR;
			goto ged_facetize_regions_memfree;
		    }

		}
	    }
	}

	tmp = ar;
	ar = ar2;
	ar2 = tmp;

    }

    /* For easier user inspection of what happened, make some high level debugging combs */
    if (BU_PTBL_LEN(ar) > 0) {
	/* Stash any failed regions into a top level comb for easy subsequent examination */
	const char **avv = (const char **)bu_calloc(BU_PTBL_LEN(ar)+1, sizeof(char *), "argv array");
	struct bu_vls failed_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&failed_name, "%s_FAILED-0", newname);
	bu_vls_incr(&failed_name, NULL, NULL, &_old_db_uniq_test, (void *)gedp);
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    avv[i] = n->d_namep;
	}
	ret = _ged_combadd2(gedp, bu_vls_addr(&failed_name), (int)BU_PTBL_LEN(ar), avv, 0, DB_OP_UNION, 0, 0, NULL, 0);
	bu_vls_free(&failed_name);
	bu_free(avv, "argv array");
    }

    if (opts->in_place) {
	/* The "new" tree is actually the preservation of the old tree in this
	 * scenario, so swap all the region names */
	if (opts->verbosity) {
	    bu_log("Generation complete, swapping new geometry into original tree...\n");
	}
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    if (_ged_facetize_obj_swap(gedp, n->d_namep, bu_avs_get(opts->c_map, n->d_namep)) != BRLCAD_OK) {
		ret = BRLCAD_ERROR;
		goto ged_facetize_regions_memfree;
	    }
	}
    }

ged_facetize_regions_memfree:

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    if (bu_vls_strlen(opts->nmg_log) && opts->method_flags & FACETIZE_NMGBOOL && opts->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(opts->nmg_log));
    }

    /* Final report */
    bu_vls_printf(gedp->ged_result_str, "Objects successfully converted: %d of %d\n", (int)(to_convert - BU_PTBL_LEN(ar)), to_convert);
    if (BU_PTBL_LEN(ar)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: %d objects failed:\n", (int)BU_PTBL_LEN(ar));
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", n->d_namep);
	}
    }

    if (ar2) {
	bu_ptbl_free(ar2);
	bu_free(ar2, "ar table");
    }
    if (ar) {
	bu_ptbl_free(ar);
	bu_free(ar, "ar table");
    }
    if (pc) {
	bu_ptbl_free(pc);
	bu_free(pc, "pc table");
    }
    bu_free(dpa, "dpa array");
    return ret;
}


static int
_nonovlp_brep_facetize(struct ged *gedp, int argc, const char **argv, struct _old_ged_facetize_opts *opts)
{
    char *newname = NULL;
    int newobj_cnt = 0;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_ptbl *ac = NULL;
    struct bu_ptbl *br = NULL;
    std::vector<ON_Brep_CDT_State *> ss_cdt;
    struct ON_Brep_CDT_State **s_a = NULL;

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    opts->tol = &(wdbp->wdb_ttol);
    struct bg_tess_tol cdttol;
    cdttol.abs = opts->tol->abs;
    cdttol.rel = opts->tol->rel;
    cdttol.norm = opts->tol->norm;

    if (!argc) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp->dbip, argc, argv, dpa);
    if (_ged_validate_objs_list(gedp, argc, argv, opts, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    /* If anything specified has subtractions or intersections, we can't facetize it with
     * this logic - that would require all-up Boolean evaluation processing. */
    const char *non_union = "-bool + -or -bool -";
    if (db_search(NULL, DB_SEARCH_QUIET, non_union, newobj_cnt, dpa, dbip, NULL) > 0) {
	bu_vls_printf(gedp->ged_result_str, "Found intersection or subtraction objects in specified inputs - currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* If anything other than combs or breps exists in the specified inputs, we can't
     * process with this logic - requires a preliminary brep conversion. */
    const char *obj_types = "! -type c -and ! -type brep";
    if (db_search(NULL, DB_SEARCH_QUIET, obj_types, newobj_cnt, dpa, dbip, NULL) > 0) {
	bu_vls_printf(gedp->ged_result_str, "Found objects in specified inputs which are not of type comb or brep- currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* Find breps (need full paths to do uniqueness checking)*/
    const char *active_breps = "-type brep";
    BU_ALLOC(br, struct bu_ptbl);
    if (db_search(br, DB_SEARCH_TREE, active_breps, newobj_cnt, dpa, dbip, NULL) < 0) {
	bu_free(br, "brep results");
	return BRLCAD_ERROR;
    }
    if (!BU_PTBL_LEN(br)) {
	/* No active breps (unlikely but technically possible), nothing to do */
	bu_vls_printf(gedp->ged_result_str, "No brep objects present in specified inputs - nothing to convert.\n");
	bu_free(br, "brep results");
	return BRLCAD_OK;
    }

    /* Find combs (need full paths to do uniqueness checking) */
    const char *active_combs = "-type c";
    BU_ALLOC(ac, struct bu_ptbl);
    if (db_search(ac, DB_SEARCH_TREE, active_combs, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for parent combs - aborting.\n");
	}
	bu_free(br, "brep results");
	bu_free(ac, "comb results");
	return BRLCAD_ERROR;
    }

    bu_free(dpa, "dpa array");

    /* When doing a non-overlapping tessellation, non-unique object instances won't work -
     * a tessellation of one instance of an object may be clear of overlaps, but the same
     * tessellation in another location may interfere.  There are various situations where
     * this is may be OK, but if I'm not mistaken real safety requires a spatial check of some
     * sort to ensure breps or combs used in multiple places are isolated enough that their
     * tessellations won't be a potential source of overlaps.
     *
     * For now, just report the potential issue (recommending xpush, which is the best option
     * I know of to try and deal with this currently) and quit.
     */

    std::set<struct directory *> brep_objs;
    bool multi_instance = false;
    for (int i = BU_PTBL_LEN(br) - 1; i >= 0; i--) {
	struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(br, i);
	struct directory *cobj = DB_FULL_PATH_CUR_DIR(dfptr);
	if (brep_objs.find(cobj) != brep_objs.end()) {
	    bu_vls_printf(gedp->ged_result_str, "Multiple instances of %s observed.\n", cobj->d_namep);
	    multi_instance = true;
	} else {
	    brep_objs.insert(cobj);
	}
    }
    std::set<struct directory *> comb_objs;
    for (int i = BU_PTBL_LEN(ac) - 1; i >= 0; i--) {
	struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(ac, i);
	struct directory *cobj = DB_FULL_PATH_CUR_DIR(dfptr);
	if (comb_objs.find(cobj) != comb_objs.end()) {
	    bu_vls_printf(gedp->ged_result_str, "Multiple instances of %s observed.\n", cobj->d_namep);
	    multi_instance = true;
	} else {
	    comb_objs.insert(cobj);
	}
    }
    if (multi_instance) {
	bu_vls_printf(gedp->ged_result_str, "Multiple object instances found - suggest using xpush command to create unique objects.\n");
	bu_free(br, "brep results");
	bu_free(ac, "comb results");
	return BRLCAD_ERROR;
    }
    bu_free(br, "brep results");
    bu_free(ac, "comb results");


    newname = (char *)argv[argc-1];
    argc--;

    /* Use the new name for the root */
    bu_vls_sprintf(opts->froot, "%s", newname);

    /* Set up all the names we will need */
    std::set<struct directory *>::iterator d_it;
    for (d_it = brep_objs.begin(); d_it != brep_objs.end(); d_it++) {
	_ged_facetize_mkname(gedp, opts, (*d_it)->d_namep, SOLID_OBJ_NAME);
    }
    for (d_it = comb_objs.begin(); d_it != comb_objs.end(); d_it++) {
	_ged_facetize_mkname(gedp, opts, (*d_it)->d_namep, COMB_OBJ_NAME);
    }

    /* First, add the new toplevel comb to hold all the new geometry */
    const char **ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
    for (int i = 0; i < argc; i++) {
	ntop[i] = bu_avs_get(opts->c_map, argv[i]);
	if (!ntop[i]) {
	    ntop[i] = bu_avs_get(opts->s_map, argv[i]);
	}
    }
    if (!opts->quiet) {
	bu_log("Creating new top level assembly object %s...\n", newname);
    }
    _ged_combadd2(gedp, newname, argc, ntop, 0, DB_OP_UNION, 0, 0, NULL, 0);
    bu_free(ntop, "new top level names");

    /* For the combs, make new versions with the suffixed names */
    if (!opts->quiet) {
	bu_log("Initializing copies of combinations...\n");
    }
    for (d_it = comb_objs.begin(); d_it != comb_objs.end(); d_it++) {
	struct directory *n = *d_it;

	if (_ged_facetize_cpcomb(gedp, n->d_namep, opts) != BRLCAD_OK) {
	    if (opts->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(opts->c_map, n->d_namep), n->d_namep);
	    }
	    continue;
	}

	/* Add the members from the map with the settings from the original
	 * comb */
	if (_ged_facetize_add_children(gedp, n, opts) != BRLCAD_OK) {
	    if (!opts->quiet) {
		bu_log("Error: duplication of assembly %s failed!\n", n->d_namep);
	    }
	    continue;
	}
    }

    /* Now, actually trigger the facetize logic. */
    for (d_it = brep_objs.begin(); d_it != brep_objs.end(); d_it++) {
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	GED_DB_GET_INTERNAL(gedp, &intern, *d_it, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", (*d_it)->d_namep);
	    for (size_t i = 0; i < ss_cdt.size(); i++) {
		ON_Brep_CDT_Destroy(ss_cdt[i]);
	    }
	    return BRLCAD_ERROR;
	}
	ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, (*d_it)->d_namep);
	ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
	ss_cdt.push_back(s_cdt);
    }

    for (size_t i = 0; i < ss_cdt.size(); i++) {
	ON_Brep_CDT_Tessellate(ss_cdt[i], 0, NULL);
    }

    // Do comparison/resolution
    s_a = (struct ON_Brep_CDT_State **)bu_calloc(ss_cdt.size(), sizeof(struct ON_Brep_CDT_State *), "state array");
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	s_a[i] = ss_cdt[i];
    }

    int resolve_result = ON_Brep_CDT_Ovlp_Resolve(s_a, ss_cdt.size(), opts->nonovlp_threshold, opts->max_time);
    if (resolve_result < 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: RESOLVE fail.\n");
#if 0
	for (size_t i = 0; i < ss_cdt.size(); i++) {
	    ON_Brep_CDT_Destroy(ss_cdt[i]);
	}
	bu_free(s_a, "array of states");
	return BRLCAD_ERROR;
#endif
    }

    if (resolve_result > 0) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: Timeout of %d seconds overlap processing reached, but triangles not fully refined to specified threshold.\nGenerating meshes, but larger overlaps will be present.\n", opts->max_time);
    }


    bu_free(s_a, "array of states");

    // Make final meshes
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	int fcnt, fncnt, ncnt, vcnt;
	int *faces = NULL;
	fastf_t *vertices = NULL;
	int *face_normals = NULL;
	fastf_t *normals = NULL;

	ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, ss_cdt[i], 0, NULL);

	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = vcnt;
	bot->num_faces = fcnt;
	bot->vertices = vertices;
	bot->faces = faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = ncnt;
	bot->num_face_normals = fncnt;
	bot->normals = normals;
	bot->face_normals = face_normals;

	if (wdb_export(wdbp, bu_avs_get(opts->s_map, ON_Brep_CDT_ObjName(ss_cdt[i])), (void *)bot, ID_BOT, 1.0)) {
	    bu_vls_printf(gedp->ged_result_str, "Error exporting object %s.", bu_avs_get(opts->s_map, ON_Brep_CDT_ObjName(ss_cdt[i])));
	    for (size_t j = 0; j < ss_cdt.size(); j++) {
		ON_Brep_CDT_Destroy(ss_cdt[j]);
	    }
	    return BRLCAD_ERROR;
	}
    }

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    for (size_t i = 0; i < ss_cdt.size(); i++) {
	ON_Brep_CDT_Destroy(ss_cdt[i]);
    }
    return BRLCAD_OK;
}


extern "C" int
ged_facetize_old_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: facetize [ -nmhT | [--NMG] [--CM] [--SPSR] ] [old_obj1 | new_obj] [old_obj* ...] [old_objN | new_obj]\n";
    static const char *pusage = "Usage: facetize --SPSR [-d #] [-w #] [ray sampling options] old_obj new_obj\n";
    static const char *busage = "Usage: facetize -B -t # [--max-time #] old_obj new_obj\n";
    int print_help = 0;
    int need_help = 0;
    int nonovlp_brep = 0;
    struct _old_ged_facetize_opts *opts = _old_ged_facetize_opts_create();
    struct bu_opt_desc d[22];
    struct bu_opt_desc pd[4];

    BU_OPT(d[0],  "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1],  "v", "verbose",       "",  &_ged_vopt,  &(opts->verbosity),  "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2],  "q", "quiet",         "",  NULL,  &(opts->quiet),            "Suppress all output (overrides verbose flag)");
    BU_OPT(d[3],  "",  "NMG",           "",  NULL,  &(opts->nmgbool),          "Use the standard libnmg boolean mesh evaluation to create output (Default)");
    BU_OPT(d[4],  "",  "CM",            "",  NULL,  &(opts->continuation),     "Use the Continuation Method to sample the object and create output");
    BU_OPT(d[5],  "",  "SPSR",          "",  NULL,  &(opts->screened_poisson), "Use raytraced points and SPSR to create output - run -h --SPSR to see more options for this mode");
    BU_OPT(d[6],  "n", "NMG",           "",  NULL,  &(opts->make_nmg),         "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh)");
    BU_OPT(d[7],  "",  "TNURB",         "",  NULL,  &(opts->nmg_use_tnurbs),   "Create TNURB faces rather than planar approximations (experimental)");
    BU_OPT(d[8],  "T", "triangles",     "",  NULL,  &(opts->triangulate),      "Generate a NMG solid using only triangles (BoTs, the default output, can only use triangles - this option mimics that behavior for NMG output.)");
    BU_OPT(d[9],  "r", "regions",       "",  NULL,  &(opts->regions),          "For combs, walk the trees and create new copies of the hierarchies with each region replaced by a facetized evaluation of that region. (Default is to create one facetized object for all specified inputs.)");
    BU_OPT(d[10], "",  "resume",        "",  NULL,  &(opts->resume),           "Resume an interrupted conversion (region mode only)");
    BU_OPT(d[11], "",  "retry",         "",  NULL,  &(opts->retry),            "When resuming an interrupted conversion, re-try operations that previously failed (default is to not repeat previous attempts with already-attempted methods.)");
    BU_OPT(d[12], "",  "in-place",      "",  NULL,  &(opts->in_place),         "Alter the existing tree/object to reference the facetized object.  May only specify one input object with this mode, and no output name.  (Warning: this option changes pre-existing geometry!)");
    BU_OPT(d[13], "F", "feature-scale", "#", &bu_opt_fastf_t, &(opts->feature_scale),  "Percentage of the average thickness observed by the raytracer to use for a targeted feature size.  Defaults to 0.15, overridden by --feature-size option");
    BU_OPT(d[14], "",  "feature-size",  "#", &bu_opt_fastf_t, &(opts->feature_size),  "Explicit feature length to try for sampling based methods - overrides feature-scale.");
    BU_OPT(d[15], "", "decimation-feature-size",  "#", &bu_opt_fastf_t, &(opts->d_feature_size),  "Initial feature length to try for decimation in sampling based methods.  By default, this value is set to 1.5x the feature size.");
    BU_OPT(d[16], "",  "max-time",      "#", &bu_opt_int,     &(opts->max_time),       "Maximum time to spend per processing step (in seconds).  Default is 30.  Zero means either the default (for routines which could run indefinitely) or run to completion (if there is a theoretical termination point for the algorithm) - be careful of specifying zero because it is quite easy to produce extremely long runs!.");
    BU_OPT(d[17], "",  "max-pnts",      "#", &bu_opt_int,     &(opts->max_pnts),                "Maximum number of pnts to use when applying ray sampling methods.");
    BU_OPT(d[18], "B",  "",             "",  NULL,  &nonovlp_brep,              "EXPERIMENTAL: non-overlapping facetization to BoT objects of union-only brep comb tree.");
    BU_OPT(d[19], "t",  "threshold",    "#",  &bu_opt_fastf_t, &(opts->nonovlp_threshold),  "EXPERIMENTAL: max ovlp threshold length.");
#ifdef USE_MANIFOLD
    BU_OPT(d[20],  "",  "MANIFOLD",           "",  NULL,  &(opts->manifold),          "Use the experimental MANIFOLD boolean evaluation");
    BU_OPT_NULL(d[21]);
#else
    BU_OPT_NULL(d[20]);
#endif

    /* Poisson specific options */
    BU_OPT(pd[0], "d", "depth",            "#", &bu_opt_int,     &(opts->s_opts.depth),            "Maximum reconstruction depth (default 8)");
    BU_OPT(pd[1], "w", "interpolate",      "#", &bu_opt_fastf_t, &(opts->s_opts.point_weight),     "Lower values (down to 0.0) bias towards a smoother mesh, higher values bias towards interpolation accuracy. (Default 2.0)");
    BU_OPT(pd[2], "",  "samples-per-node", "#", &bu_opt_fastf_t, &(opts->s_opts.samples_per_node), "How many samples should go into a cell before it is refined. (Default 1.5)");
    BU_OPT_NULL(pd[3]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing failed: %s\n", bu_vls_cstr(&omsg));
	ret = BRLCAD_ERROR;
	bu_vls_free(&omsg);
	goto ged_facetize_memfree;
    }
    bu_vls_free(&omsg);

    /* It is known that libnmg will (as of 2018 anyway) throw a lot of
     * bu_bomb calls during operation. Because we need facetize to run
     * to completion and potentially try multiple ways to convert before
     * giving up, we need to un-hook any pre-existing bu_bomb hooks */
    bu_bomb_save_all_hooks(opts->saved_bomb_hooks);

    /* We will need to catch libnmg output and store it up for later
     * use, while still bu_logging our own status updates. Cache the
     * current bu_log hooks so they can be restored at need */
    bu_log_hook_save_all(opts->saved_log_hooks);

    /* Sync -q and -v options */
    if (opts->quiet && opts->verbosity) opts->verbosity = 0;

    /* Enforce type matching on suffix */
    if (opts->make_nmg && BU_STR_EQUAL(bu_vls_addr(opts->faceted_suffix), ".bot")) {
	bu_vls_sprintf(opts->faceted_suffix, ".nmg");
    }
    if (!opts->make_nmg && BU_STR_EQUAL(bu_vls_addr(opts->faceted_suffix), ".nmg")) {
	bu_vls_sprintf(opts->faceted_suffix, ".bot");
    }

    /* Sort out which methods we can try */
    if (!opts->nmgbool && !opts->screened_poisson && !opts->continuation && !opts->manifold) {
	/* Default to MANIFOLD, NMGBOOL and Continuation active */
	opts->method_flags |= FACETIZE_MANIFOLD;
	opts->method_flags |= FACETIZE_NMGBOOL;
	opts->method_flags |= FACETIZE_CONTINUATION;
    } else {
	if (opts->manifold) opts->method_flags |= FACETIZE_MANIFOLD;
	if (opts->nmgbool) opts->method_flags |= FACETIZE_NMGBOOL;
	if (opts->screened_poisson) opts->method_flags |= FACETIZE_SPSR;
	if (opts->continuation) opts->method_flags |= FACETIZE_CONTINUATION;
    }

    if (opts->method_flags & FACETIZE_SPSR) {
	/* Parse Poisson specific options, if we might be using that method */
	argc = bu_opt_parse(NULL, argc, argv, pd);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    ret = BRLCAD_ERROR;
	    goto ged_facetize_memfree;
	}
    }

    /* Check for a couple of non-valid combinations */
    if ((opts->method_flags == FACETIZE_SPSR || opts->method_flags == FACETIZE_CONTINUATION || opts->method_flags == FACETIZE_MANIFOLD) && opts->nmg_use_tnurbs) {
	bu_vls_printf(gedp->ged_result_str, "Note: Specified reconstruction method(s) do not all support TNURBS output\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_memfree;
    }

    if (opts->triangulate && opts->nmg_use_tnurbs) {
	bu_vls_printf(gedp->ged_result_str, "both -T and -t specified!\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_memfree;
    }

    if (opts->resume && !opts->regions) {
	bu_vls_printf(gedp->ged_result_str, "--resume is only supported with with region (-r) mode\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_memfree;
    }

    /* Check if we want/need help */
    need_help += (argc < 1);
    need_help += (argc < 2 && !opts->in_place && !opts->resume && !nonovlp_brep);
    if (print_help || need_help || argc < 1) {
	if (nonovlp_brep) {
	    _ged_cmd_help(gedp, busage, d);
	    ret = (need_help) ? BRLCAD_ERROR : BRLCAD_OK;
	    goto ged_facetize_memfree;
	}

	_ged_cmd_help(gedp, usage, d);
	if (opts->method_flags & FACETIZE_SPSR) {
	    _ged_cmd_help(gedp, pusage, pd);
	}
	ret = (need_help) ? BRLCAD_ERROR : BRLCAD_OK;
	goto ged_facetize_memfree;
    }

    /* If we're doing the experimental brep-only logic, it's a separate process */
    if (nonovlp_brep) {
	if (NEAR_ZERO(opts->nonovlp_threshold, SMALL_FASTF)) {
	    bu_vls_printf(gedp->ged_result_str, "-B option requires a specified length threshold\n");
	    return BRLCAD_ERROR;
	}
	return _nonovlp_brep_facetize(gedp, argc, argv, opts);
    }

    /* Multi-region mode has a different processing logic */
    if (opts->regions) {
	if (opts->resume) {
	    ret = _ged_facetize_regions_resume(gedp, argc, argv, opts);
	} else {
	    ret = _ged_facetize_regions(gedp, argc, argv, opts);
	}
    } else {
	ret = _ged_facetize_objlist(gedp, argc, argv, opts);
    }

ged_facetize_memfree:
    _old_ged_facetize_opts_destroy(opts);

    return ret;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
struct ged_cmd_impl facetize_old_cmd_impl = { "facetize_old", ged_facetize_old_core, GED_CMD_DEFAULT };
const struct ged_cmd facetize_old_cmd = { &facetize_old_cmd_impl };
const struct ged_cmd *facetize_old_cmds[] = { &facetize_old_cmd,  NULL };

static const struct ged_plugin pinfo = { GED_API,  facetize_old_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
}
#endif


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

