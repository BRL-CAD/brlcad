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

#include "bu/exit.h"
#include "bu/hook.h"
#include "bu/cmd.h"
#include "bn/tol.h"
#include "bg/spsr.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "rt/op.h"
#include "rt/search.h"
#include "raytrace.h"
#include "analyze.h"
#include "wdb.h"
#include "./ged_private.h"

#define SOLID_OBJ_NAME 1
#define COMB_OBJ_NAME 2

#define GED_FACETIZE_NMGBOOL  0x1
#define GED_FACETIZE_SPSR  0x2
#define GED_FACETIZE_MC  0x4

struct _ged_facetize_opts {

    int quiet;
    int verbosity;

    /* NMG specific options */
    int triangulate;
    int make_nmg;
    int nmgbool;
    int screened_poisson;
    int marching_cube;
    int method_flags;

    int nmg_use_tnurbs;
    int regions;
    int in_place;
    fastf_t decim_feat_perc;

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

    /* internal */
    struct bu_attribute_value_set *c_map;
    struct bu_attribute_value_set *s_map;
    struct bu_hook_list *saved_bomb_hooks;
    struct bu_hook_list *saved_log_hooks;
    struct bu_vls *nmg_log;
    struct bu_vls *nmg_log_header;
    int nmg_log_print_header;
    int stderr_stashed;
    int serr;
    int fnull;
};

struct _ged_facetize_opts * _ged_facetize_opts_create()
{
    struct bg_3d_spsr_opts s_opts = BG_3D_SPSR_OPTS_DEFAULT;
    struct _ged_facetize_opts *o = NULL;
    BU_GET(o, struct _ged_facetize_opts);
    o->verbosity = 0;
    o->triangulate = 0;
    o->make_nmg = 0;
    o->nmgbool = 0;
    o->marching_cube = 0;
    o->screened_poisson = 0;
    o->nmg_use_tnurbs = 0;
    o->regions = 0;
    o->in_place = 0;
    o->decim_feat_perc = 0.15;
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

    return o;
}
void _ged_facetize_opts_destroy(struct _ged_facetize_opts *o)
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

    BU_PUT(o, struct _ged_facetize_opts);
}

HIDDEN db_op_t
_int_to_opt(int op)
{
    if (op == 2) return DB_OP_UNION;
    if (op == 3) return DB_OP_INTERSECT;
    if (op == 4) return DB_OP_SUBTRACT;
    return DB_OP_NULL;
}

HIDDEN int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct ged *gedp = (struct ged *)data;
    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

HIDDEN int
_ged_facetize_bomb_hook(void *cdata, void *str)
{
    struct _ged_facetize_opts *o = (struct _ged_facetize_opts *)cdata;
    if (o->nmg_log_print_header) {
	bu_vls_printf(o->nmg_log, "%s\n", bu_vls_addr(o->nmg_log_header));
	o->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->nmg_log, "%s\n", (const char *)str);
    return 0;
}

HIDDEN int
_ged_facetize_nmg_logging_hook(void *data, void *str)
{
    struct _ged_facetize_opts *o = (struct _ged_facetize_opts *)data;
    if (o->nmg_log_print_header) {
	bu_vls_printf(o->nmg_log, "%s\n", bu_vls_addr(o->nmg_log_header));
	o->nmg_log_print_header = 0;
    }
    bu_vls_printf(o->nmg_log, "%s\n", (const char *)str);
    return 0;
}

HIDDEN void
_ged_facetize_log_nmg(struct _ged_facetize_opts *o)
{
    /* Seriously, bu_bomb, we don't want you blathering
     * to stderr... shut down stderr temporarily, assuming
     * we can find /dev/null or something similar */
    o->fnull = open("/dev/null", O_WRONLY);
    if (o->fnull == -1) {
	/* https://gcc.gnu.org/ml/gcc-patches/2005-05/msg01793.html */
	o->fnull = open("nul", O_WRONLY);
    }
    if (o->fnull != -1) {
	o->serr = fileno(stderr);
	o->stderr_stashed = dup(o->serr);
	dup2(o->fnull, o->serr);
	close(o->fnull);
    }

    /* Set bu_log logging to capture in nmg_log, rather than the
     * application defaults */
    bu_log_hook_delete_all();
    bu_log_add_hook(_ged_facetize_nmg_logging_hook, (void *)o);

    /* Also engage the nmg bomb hooks */
    bu_bomb_delete_all_hooks();
    bu_bomb_add_hook(_ged_facetize_bomb_hook, (void *)o);
}


HIDDEN void
_ged_facetize_log_default(struct _ged_facetize_opts *o)
{
    /* Put stderr back */
    if (o->fnull != -1) {
	fflush(stderr);
	dup2(o->stderr_stashed, o->serr);
	close(o->stderr_stashed);
	o->fnull = -1;
    }

    /* Restore bu_bomb hooks to the application defaults */
    bu_bomb_delete_all_hooks();
    bu_bomb_restore_hooks(o->saved_bomb_hooks);

    /* Restore bu_log hooks to the application defaults */
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(o->saved_log_hooks);
}

HIDDEN void
_ged_facetize_mkname(struct ged *gedp, struct _ged_facetize_opts *opts, const char *n, int type)
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
    if (!bu_vls_strlen(&incr_template)) return;
    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&incr_template), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(&incr_template, "-0");
	bu_vls_incr(&incr_template, NULL, NULL, &_db_uniq_test, (void *)gedp);
    }

    if (type == SOLID_OBJ_NAME) {
	bu_avs_add(opts->s_map, n, bu_vls_addr(&incr_template));
    }
    if (type == COMB_OBJ_NAME) {
	bu_avs_add(opts->c_map, n, bu_vls_addr(&incr_template));
    }
}

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
_ged_validate_objs_list(struct ged *gedp, int argc, const char *argv[], struct _ged_facetize_opts *o, int newobj_cnt)
{
    int i;

    if (o->in_place && newobj_cnt) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but object list includes objects that do not exist:\n");
	for (i = argc - newobj_cnt; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  When performing an in-place facetization, a single pre-existing object must be specified.\n");
	return GED_ERROR;

    }

    if (o->in_place && argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but multiple objects listed:\n");
	for (i = 0; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  An in-place conversion replaces the specified object (or regions in a hierarchy in -r mode) with a faceted approximation.  Because a single object is generated, in-place conversion has no clear interpretation when multiple input objects are specified.\n");
	return GED_ERROR;
    }

    if (!o->in_place) {
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
    }
    return GED_OK;
}

HIDDEN int
_ged_facetize_obj_swap(struct ged *gedp, const char *o, const char *n)
{
    int ret = GED_OK;
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
    bu_vls_incr(&tname, NULL, "0:0:0:0:-", &_db_uniq_test, (void *)gedp);
    mav[1] = bu_vls_addr(&oname);
    mav[2] = bu_vls_addr(&tname);
    if (ged_move(gedp, 3, (const char **)mav) != GED_OK) {
	ret = GED_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&nname);
    mav[2] = bu_vls_addr(&oname);
    if (ged_move(gedp, 3, (const char **)mav) != GED_OK) {
	ret = GED_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&tname);
    mav[2] = bu_vls_addr(&nname);
    if (ged_move(gedp, 3, (const char **)mav) != GED_OK) {
	ret = GED_ERROR;
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

HIDDEN struct model *
_try_nmg_facetize(struct ged *gedp, int argc, const char **argv, int nmg_use_tnurbs, struct _ged_facetize_opts *o)
{
    int i;
    int failed = 0;
    struct db_tree_state init_state;
    union tree *facetize_tree;
    struct model *nmg_model;

    _ged_facetize_log_nmg(o);

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
	_ged_facetize_log_default(o);
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
	    _ged_facetize_log_default(o);
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

    _ged_facetize_log_default(o);
    return (failed) ? NULL : nmg_model;
}

HIDDEN int
_try_nmg_triangulate(struct ged *gedp, struct model *nmg_model, int marching_cube, struct _ged_facetize_opts *o)
{
    _ged_facetize_log_nmg(o);
    if (!BU_SETJUMP) {
	/* try */
	if (marching_cube == 1)
	    nmg_triangulate_model_mc(nmg_model, &gedp->ged_wdbp->wdb_tol);
	else
	    nmg_triangulate_model(nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_log("WARNING: triangulation failed!!!\n");
	nmg_km(nmg_model);
	_ged_facetize_log_default(o);
	return GED_ERROR;
    } BU_UNSETJUMP;
    _ged_facetize_log_default(o);
    return GED_OK;
}

HIDDEN struct rt_bot_internal *
_try_nmg_to_bot(struct ged *gedp, struct model *nmg_model, struct _ged_facetize_opts *o)
{
    struct rt_bot_internal *bot = NULL;
    _ged_facetize_log_nmg(o);
    if (!BU_SETJUMP) {
	/* try */
	bot = (struct rt_bot_internal *)nmg_mdl_to_bot(nmg_model, &RTG.rtg_vlfree, &gedp->ged_wdbp->wdb_tol);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_ged_facetize_log_default(o);
	return NULL;
    } BU_UNSETJUMP;

    _ged_facetize_log_default(o);
    return bot;
}


HIDDEN struct rt_bot_internal *
_try_decimate(struct rt_bot_internal *bot, fastf_t feature_size, struct _ged_facetize_opts *o)
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

    _ged_facetize_log_nmg(o);
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
	_ged_facetize_log_default(o);
	return obot;
    } BU_UNSETJUMP;

    if (success) {
	/* Success - free the old BoT, return the new one */
	bu_free(obot->faces, "free faces");
	bu_free(obot->vertices, "free vertices");
	bu_free(obot, "free bot");
	_ged_facetize_log_default(o);
	return nbot;
    } else {
	/* Failed - free the working copy */
	bu_free(nbot->faces, "free faces");
	bu_free(nbot->vertices, "free vertices");
	bu_free(nbot, "free bot");
	_ged_facetize_log_default(o);
	return obot;
    }
}

HIDDEN int
_write_bot(struct ged *gedp, struct rt_bot_internal *bot, const char *name, struct _ged_facetize_opts *opts)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    /* Export BOT as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    dp=db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (opts->verbosity) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Failed to write %s to database\n", name);
	}
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    return GED_OK;
}

HIDDEN int
_write_nmg(struct ged *gedp, struct model *nmg_model, const char *name, struct _ged_facetize_opts *opts)
{
    struct rt_db_internal intern;
    struct directory *dp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)nmg_model;

    dp=db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (opts->verbosity) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	return GED_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Failed to write %s to database\n", name);
	}
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    return GED_OK;
}

HIDDEN int
_ged_spsr_obj(int *is_valid, struct ged *gedp, const char *objname, const char *newname, struct _ged_facetize_opts *opts)
{
    int ret = GED_OK;
    struct directory *dp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct rt_db_internal in_intern;
    struct bn_tol btol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1e-6, 1.0 - 1e-6 };
    struct rt_pnts_internal *pnts;
    struct rt_bot_internal *bot = NULL;
    struct pnt_normal *pn, *pl;
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

    if (!opts->quiet) {
	bu_log("SPSR: tessellating %s with depth %d, interpolation weight %g, and samples-per-node %g\n", objname, opts->s_opts.depth, opts->s_opts.point_weight, opts->s_opts.samples_per_node);
    }

    if (rt_db_get_internal(&in_intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	if (opts->verbosity) {
	    bu_log("Error: could not determine type of object %s, skipping\n", objname);
	}
	return GED_ERROR;
    }

    if (in_intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_PNTS) {
	/* If we have a point cloud, there's no need to raytrace it */
	pnts = (struct rt_pnts_internal *)in_intern.idb_ptr;
	pl = (struct pnt_normal *)pnts->point;

	/* Key some settings off the bbox size - ged_get_obj_bounds not
	 * currently working on pnt sets, so just run through the pnts. */
	for (BU_LIST_FOR(pn, pnt_normal, &(pl->l))) {
	    VMINMAX(rpp_min, rpp_max, (double *)pn->v);
	    i++;
	}
    } else {
	struct bu_vls pnt_msg = BU_VLS_INIT_ZERO;
	BU_ALLOC(pnts, struct rt_pnts_internal);
	pnts->magic = RT_PNTS_INTERNAL_MAGIC;
	pnts->scale = 0.0;
	pnts->type = RT_PNT_TYPE_NRM;
	free_pnts = 1;

	/* Key some settings off the bbox size */
	ged_get_obj_bounds(gedp, 1, (const char **)&objname, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);

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
	    opts->len_tol = DIST_PT_PT(rpp_max, rpp_min) * 0.01;
	    if (opts->verbosity > 1) {
		bu_log("SPSR: no tolerance specified for %s, using %f\n", newname, opts->len_tol);
	    }
	}
	btol.dist = opts->len_tol;

	if (opts->verbosity) {
	    bu_vls_printf(&pnt_msg, "SPSR: generating points from %s with the following settings:\n", objname);
	    bu_vls_printf(&pnt_msg, "      point sampling methods:");
	    if (flags & ANALYZE_OBJ_TO_PNTS_GRID) bu_vls_printf(&pnt_msg, " grid");
	    if (flags & ANALYZE_OBJ_TO_PNTS_RAND) bu_vls_printf(&pnt_msg, " rand");
	    if (flags & ANALYZE_OBJ_TO_PNTS_SOBOL) bu_vls_printf(&pnt_msg, " sobol");
	    bu_vls_printf(&pnt_msg, "\n");
	    if (opts->max_pnts) {
		bu_vls_printf(&pnt_msg, "      Maximum pnt count per method: %d", opts->max_pnts);
	    }
	    if (opts->max_time) {
		bu_vls_printf(&pnt_msg, "      Maximum time per method (sec): %d", opts->max_time);
	    }
	    bu_log("%s\n", bu_vls_addr(&pnt_msg));
	    bu_vls_free(&pnt_msg);
	}

	if (analyze_obj_to_pnts(pnts, gedp->ged_wdbp->dbip, objname, &btol, flags, opts->max_pnts, opts->max_time)) {
	    if (!opts->quiet) {
		bu_log("SPSR: point generation failed: %s\n", objname);
	    }
	    ret = GED_ERROR;
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
		pnts->count, &(opts->s_opts)) ) {
	if (!opts->quiet) {
	    bu_log("SPSR: Screened Poisson surface reconstruction failed: %s\n", objname);
	}
	ret = GED_ERROR;
	goto ged_facetize_spsr_memfree;
    }

/* do decimation */
    if (opts->decim_feat_perc > 0) {
	struct rt_bot_internal *obot = bot;
	fastf_t feature_size = 0.0;
	fastf_t xlen = fabs(rpp_max[X] - rpp_min[X]);
	fastf_t ylen = fabs(rpp_max[Y] - rpp_min[Y]);
	fastf_t zlen = fabs(rpp_max[Z] - rpp_min[Z]);
	feature_size = (xlen < ylen) ? xlen : ylen;
	feature_size = (feature_size < zlen) ? feature_size : zlen;
	feature_size = feature_size * ((opts->decim_feat_perc > 1.0) ? 1.0 : opts->decim_feat_perc);

	if (opts->verbosity) {
	    bu_log("SPSR: trying decimation with decimation size percentage: %g\n", opts->decim_feat_perc);
	}

	bot = _try_decimate(bot, feature_size, opts);
	if (bot == obot && opts->verbosity) {
	    bu_log("SPSR: decimation failed, returning original BoT (may be large)\n");
	}
    }

    if (is_valid) {
	int is_v = !bg_trimesh_solid(bot->num_vertices, bot->num_faces, (fastf_t *)bot->vertices, (int *)bot->faces, NULL);
	(*is_valid) = is_v;
	if (!is_v && !opts->quiet) {
	    bu_log("SPSR: created invalid bot: %s\n", objname);
	}
    }

    if (!opts->make_nmg) {

	ret = _write_bot(gedp, bot, newname, opts);

    } else {
	/* Convert BoT to NMG */
	struct model *m = nmg_mm();
	struct nmgregion *r;
	struct rt_db_internal intern;

	/* Use intern to fake out rt_bot_tess, since it expects an
	 * rt_db_internal wrapper */
	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_BOT;
	intern.idb_ptr = (void *)bot;
	if (rt_bot_tess(&r, m, &intern, NULL, &btol) < 0) {
	    if (!opts->quiet) {
		bu_log("SPSR: failed to convert BoT to NMG: %s\n", objname);
	    }
	    rt_db_free_internal(&intern);
	    ret = GED_ERROR;
	    goto ged_facetize_spsr_memfree;
	} else {
	    /* OK,have NMG now - write it out */
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

int
_ged_nmg_obj(struct ged *gedp, int argc, const char **argv, const char *newname, struct _ged_facetize_opts *opts)
{
    int ret = GED_OK;
    struct model *nmg_model = NULL;
    struct rt_bot_internal *bot = NULL;

    nmg_model = _try_nmg_facetize(gedp, argc, argv, opts->nmg_use_tnurbs, opts);
    if (nmg_model == NULL) {
	if (opts->verbosity > 1) {
	    bu_log("NMG(%s):  no resulting region, aborting\n", newname);
	}
	ret = GED_ERROR;
	goto ged_nmg_obj_memfree;
    }

    /* Triangulate model, if requested */
    if (opts->triangulate && opts->make_nmg) {
	if (_try_nmg_triangulate(gedp, nmg_model, opts->marching_cube, opts) != GED_OK) {
	    if (opts->verbosity > 1) {
		bu_log("NMG(%s):  triangulation failed, aborting\n", newname);
	    }
	    ret = GED_ERROR;
	    goto ged_nmg_obj_memfree;
	}
    }

    if (!opts->make_nmg) {

	/* Make and write out the bot */
	bot = _try_nmg_to_bot(gedp, nmg_model, opts);
	nmg_km(nmg_model);

	if (!bot) {
	    if (opts->verbosity > 1) {
		bu_log("NMG(%s): conversion to BOT failed, aborting\n", newname);
	    }
	    ret = GED_ERROR;
	    goto ged_nmg_obj_memfree;
	}

	ret = _write_bot(gedp, bot, newname, opts);

    } else {

	/* Just write the NMG */
	ret = _write_nmg(gedp, nmg_model, newname, opts);

    }

ged_nmg_obj_memfree:
    if (!opts->quiet && ret != GED_OK) {
	bu_log("NMG: failed to generate %s\n", newname);
    }

    return ret;
}


int
_ged_facetize_objlist(struct ged *gedp, int argc, const char **argv, struct _ged_facetize_opts *opts)
{
    int ret = GED_ERROR;
    int done_trying = 0;
    int newobj_cnt;
    char *newname;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    int flags = opts->method_flags;
    struct rt_tess_tol *tol = &(gedp->ged_wdbp->wdb_ttol);

    RT_CHECK_DBI(dbip);

    if (argc < 0) return GED_ERROR;

    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, NULL);
    if (_ged_validate_objs_list(gedp, argc, argv, opts, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
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
	    bu_vls_incr(&oname, NULL, NULL, &_db_uniq_test, (void *)gedp);
	}
	newname = (char *)bu_vls_addr(&oname);
    }

    while (!done_trying) {

	if (flags & GED_FACETIZE_NMGBOOL) {
	    opts->nmg_log_print_header = 1;
	    if (argc == 1) {
		bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %s with tolerances a=%g, r=%g, n=%g\n", argv[0], tol->abs, tol->rel, tol->norm);
	    } else {
		bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %d objects with tolerances a=%g, r=%g, n=%g\n", argc, tol->abs, tol->rel, tol->norm);
	    }
	    /* Let the user know what's going on, unless output is suppressed */
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }

	    if (_ged_nmg_obj(gedp, argc, argv, newname, opts) == GED_OK) {
		done_trying = 1;
		ret = GED_OK;
		break;
	    } else {
		flags = flags & ~(GED_FACETIZE_NMGBOOL);
		continue;
	    }
	}

	if (flags & GED_FACETIZE_SPSR) {
	    if (argc != 1) {
		if (opts->verbosity) {
		    bu_log("Screened Poisson mode (currently) only supports one existing object at a time as input - not attempting.\n");
		}
		flags = flags & ~(GED_FACETIZE_SPSR);
	    } else {
		if (_ged_spsr_obj(NULL, gedp, argv[0], newname, opts) == GED_OK) {
		    done_trying = 1;
		    ret = GED_OK;
		} else {
		    flags = flags & ~(GED_FACETIZE_SPSR);
		    continue;
		}
	    }
	}

	/* Out of options */
	done_trying = 1;

    }

    if ((ret == GED_OK) && opts->in_place) {
	if (_ged_facetize_obj_swap(gedp, argv[0], newname) != GED_OK) {
	    return GED_ERROR;
	}
    }

    if (bu_vls_strlen(opts->nmg_log) && opts->method_flags & GED_FACETIZE_NMGBOOL && opts->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(opts->nmg_log));
    }

    return ret;
}

int
_ged_facetize_cpcomb(struct ged *gedp, const char *o, struct _ged_facetize_opts *opts)
{
    int ret = GED_OK;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    struct directory *dp;
    struct rt_db_internal ointern, intern;
    struct rt_comb_internal *ocomb, *comb;
    struct bu_attribute_value_set avs;
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

    return ret;
}

int
_ged_facetize_regions(struct ged *gedp, int argc, const char **argv, struct _ged_facetize_opts *opts)
{
    char *newname = NULL;
    int newobj_cnt = 0;
    int ret = GED_OK;
    unsigned int i = 0;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->ged_wdbp->dbip;
    const char **ntop;
    struct bu_ptbl *pc = NULL;
    struct bu_ptbl *ar = NULL;
    struct bu_ptbl ar_failed_nmg = BU_PTBL_INIT_ZERO;
    struct bu_ptbl spsr_succeeded = BU_PTBL_INIT_ZERO;
    struct bu_ptbl facetize_failed = BU_PTBL_INIT_ZERO;
    struct bu_ptbl tmpnames = BU_PTBL_INIT_ZERO;
    struct rt_tess_tol *tol = &(gedp->ged_wdbp->wdb_ttol);
    const char *sname;
    const char *mav[3];
    const char *cmdname = "facetize";
    struct bu_vls invalid_name = BU_VLS_INIT_ZERO;

    /* We need to copy combs above regions that are not themselves regions.
     * Also, facetize will need all "active" regions that will define shapes.
     * Construct searches to get these sets. */
    const char *preserve_combs = "-type c ! -type r ! -below -type r";
    const char *active_regions = "-type r ! -below -type r";

    if (argc) dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(gedp, argc, argv, opts, newobj_cnt) == GED_ERROR) {
	return GED_ERROR;
    }

    if (!opts->in_place) {
	newname = (char *)argv[argc-1];
	argc--;
    }

    /* Set up mapping names for the original toplevel object(s).  If we have
     * top level solids, deal with them now. */
    for (i = 0; i < (unsigned int)argc; i++) {
	struct directory *dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    /* solid object in list at top level - handle directly */
	    _ged_facetize_mkname(gedp, opts, argv[i], SOLID_OBJ_NAME);

	    /* Let the user know what's going on, unless output is suppressed */
	    bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating solid %s with tolerances a=%g, r=%g, n=%g\n", argv[0], tol->abs, tol->rel, tol->norm);
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }
	    opts->nmg_log_print_header = 1;

	    if (_ged_nmg_obj(gedp, 1, (const char **)&argv[i], bu_avs_get(opts->s_map, argv[i]), opts) != GED_OK) {
		return GED_ERROR;
	    }
	}
    }


    /* Find assemblies and regions */
    BU_ALLOC(pc, struct bu_ptbl);
    if (db_search(pc, DB_SEARCH_RETURN_UNIQ_DP, preserve_combs, argc, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for parent combs - aborting.\n");
	}
	ret = GED_ERROR;
	goto ged_facetize_regions_memfree;
    }
    BU_ALLOC(ar, struct bu_ptbl);
    if (db_search(ar, DB_SEARCH_RETURN_UNIQ_DP, active_regions, argc, dpa, dbip, NULL) < 0) {
	if (opts->verbosity) {
	    bu_log("Problem searching for active regions - aborting.\n");
	}
	ret = GED_ERROR;
	goto ged_facetize_regions_memfree;
    }
    if (!BU_PTBL_LEN(ar)) {
	/* No active regions (unlikely but technically possible), nothing to do */
	ret = GED_OK;
	goto ged_facetize_regions_memfree;
    }


    /* Set up all the names we will need */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	/* Regions will have a name mapping both to a new region comb AND a facetized
	 * solid object - set up both names, and create the region combs */
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	_ged_facetize_mkname(gedp, opts, n->d_namep, SOLID_OBJ_NAME);
	_ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);
	_ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
    }



    /* First, add the new toplevel comb to hold all the new geometry */
    if (!opts->in_place) {
	ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
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
    }


    /* For the assemblies, make new versions with the suffixed names */
    if (!opts->quiet) {
	bu_log("Initializing copies of assembly combinations...\n");
    }
    for (i = 0; i < BU_PTBL_LEN(pc); i++) {
	int j = 0;
	struct rt_db_internal intern;
	struct directory *cdp = RT_DIR_NULL;
	struct directory **children = NULL;
	struct rt_comb_internal *comb = NULL;
	int *bool_ops = NULL;
	matp_t *mats = NULL;
	const char *nparent;
	struct directory *n = (struct directory *)BU_PTBL_GET(pc, i);

	if (_ged_facetize_cpcomb(gedp, n->d_namep, opts) != GED_OK) {
	    if (opts->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(opts->c_map, n->d_namep), n->d_namep);
	    }
	    continue;
	}

	/* Add the members from the map with the settings from the original
	 * comb */

	RT_DB_INTERNAL_INIT(&intern);
	cdp = (struct directory *)BU_PTBL_GET(pc, i);
	nparent = bu_avs_get(opts->c_map, cdp->d_namep);
	if (!opts->quiet) {
	    bu_log("Rebuilding assembly %s (%d of %d) using facetize object names...\n", nparent, i+1, BU_PTBL_LEN(pc));
	}
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
	    const char *nc = bu_avs_get(opts->c_map, cdp->d_namep);
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

    /* For regions, make the new region comb and add a reference to the to-be-created solid */
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	_ged_facetize_mkname(gedp, opts, n->d_namep, SOLID_OBJ_NAME);
	_ged_facetize_mkname(gedp, opts, n->d_namep, COMB_OBJ_NAME);
	if (!opts->quiet) {
	    bu_log("Copying (sans tree) region comb %s to %s (%d of %d)...\n", n->d_namep, bu_avs_get(opts->c_map, n->d_namep), i+1, BU_PTBL_LEN(ar));
	}
	if (_ged_facetize_cpcomb(gedp, n->d_namep, opts) != GED_OK) {
	    if (opts->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(opts->c_map, n->d_namep), n->d_namep);
	    }
	} else {
	    const char *rcname = bu_avs_get(opts->c_map, n->d_namep);
	    const char *ssname = bu_avs_get(opts->s_map, n->d_namep);
	    if (_ged_combadd2(gedp, (char *)rcname, 1, (const char **)&ssname, 0, DB_OP_UNION, 0, 0, NULL, 0) != GED_OK) {
		if (opts->verbosity) {
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
	    } else {
		/* By default, store the original region name and target bot name in attributes to make resuming easier */
		const char *attrav[5];
		attrav[0] = "attr";
		attrav[1] = "set";
		attrav[2] = rcname;
		attrav[3] = "facetize_original_region";
		attrav[4] = n->d_namep;
		if (ged_attr(gedp, 5, (const char **)&attrav) != GED_OK && opts->verbosity) {
		    bu_log("Error adding %s to comb %s", ssname, rcname);
		}
		attrav[3] = "facetize_target_bot_name";
		attrav[4] = ssname;
		if (ged_attr(gedp, 5, (const char **)&attrav) != GED_OK && opts->verbosity) {
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
    for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	const char *oname = n->d_namep;
	const char *nname = bu_avs_get(opts->s_map, n->d_namep);

	if (opts->method_flags & GED_FACETIZE_NMGBOOL) {

	    /* We're staring a new object, so we want to write out the header in the
	     * log file the first time we get an NMG logging event.  (Re)set the flag
	     * so the logger knows to do so. */
	    opts->nmg_log_print_header = 1;
	    bu_vls_sprintf(opts->nmg_log_header, "NMG: tessellating %s (%d of %d) with tolerances a=%g, r=%g, n=%g\n", oname, i+1, BU_PTBL_LEN(ar), tol->abs, tol->rel, tol->norm);

	    /* Let the user know what's going on, unless output is suppressed */
	    if (!opts->quiet) {
		bu_log("%s", bu_vls_addr(opts->nmg_log_header));
	    }

	    if (_ged_nmg_obj(gedp, 1, (const char **)&oname, nname, opts) != GED_OK) {
		bu_ptbl_ins(&ar_failed_nmg, (long *)oname);
	    } else {
		/* Success - remove the restart attributes */
		const char *rcname = bu_avs_get(opts->c_map, n->d_namep);
		const char *attrav[4];
		attrav[0] = "attr";
		attrav[1] = "rm";
		attrav[2] = rcname;
		attrav[3] = "facetize_original_region";
		if (ged_attr(gedp, 4, (const char **)&attrav) != GED_OK && opts->verbosity) {
		    bu_log("Error removing attribute \"facetize_original_region\" from comb %s", rcname);
		}
		attrav[3] = "facetize_target_bot_name";
		if (ged_attr(gedp, 4, (const char **)&attrav) != GED_OK && opts->verbosity) {
		    bu_log("Error removing attribute \"facetize_target_bot_name\" from comb %s", rcname);
		}
	    }
	}

	/* If nmgbool is OFF and SPSR is ON, we'll have to try SPSR for all of them */
	if (opts->method_flags == GED_FACETIZE_SPSR) {
	    int is_valid = 0;
	    if (_ged_spsr_obj(&is_valid, gedp, oname, nname, opts) == GED_OK) {
		/* Check the validity */
		if (is_valid) {
		    /* Success - remove the restart attributes */
		    const char *rcname = bu_avs_get(opts->c_map, n->d_namep);
		    const char *attrav[4];
		    attrav[0] = "attr";
		    attrav[1] = "rm";
		    attrav[2] = rcname;
		    attrav[3] = "facetize_original_region";
		    if (ged_attr(gedp, 4, (const char **)&attrav) != GED_OK && opts->verbosity) {
			bu_log("Error removing attribute \"facetize_original_region\" from comb %s", rcname);
		    }
		    attrav[3] = "facetize_target_bot_name";
		    if (ged_attr(gedp, 4, (const char **)&attrav) != GED_OK && opts->verbosity) {
			bu_log("Error removing attribute \"facetize_target_bot_name\" from comb %s", rcname);
		    }

		    /* Make a note of SPSR success in particular - user may want to inspect */
		    bu_ptbl_ins(&spsr_succeeded, (long *)bu_avs_get(opts->s_map, oname));
		} else {
		    /* rename to INVALID and add the bot to the facetize_failed list for inspection */
		    bu_vls_sprintf(&invalid_name, "%s_SPSR_INVALID-0", nname);
		    sname = bu_strdup(bu_vls_addr(&invalid_name));
		    mav[0] = cmdname;
		    mav[1] = nname;
		    mav[2] = sname;
		    ret = ged_move(gedp, 3, (const char **)mav);
		    bu_ptbl_ins(&tmpnames, (long *)sname);
		    bu_ptbl_ins(&facetize_failed, (long *)sname);
		}
	    } else {
		bu_ptbl_ins(&facetize_failed, (long *)oname);
	    }
	}
    }

    /* Try SPSR as a fallback, if NMG booleans failed and SPSR is enabled */
    if (BU_PTBL_LEN(&ar_failed_nmg) > 0 && (opts->method_flags & GED_FACETIZE_SPSR)) {
	for (i = 0; i < BU_PTBL_LEN(&ar_failed_nmg); i++) {
	    const char *oname = (const char *)BU_PTBL_GET(&ar_failed_nmg, i);
	    const char *nname = bu_avs_get(opts->s_map, oname);
	    int is_valid = 0;
	    if (_ged_spsr_obj(&is_valid, gedp, oname, nname, opts) == GED_OK) {
		/* Check the validity */
		if (is_valid) {
		    bu_ptbl_ins(&spsr_succeeded, (long *)bu_avs_get(opts->s_map, oname));
		} else {
		    /* rename to INVALID and add the bot to the facetize_failed list for inspection */
		    bu_vls_sprintf(&invalid_name, "%s_SPSR_INVALID-0", nname);
		    sname = bu_strdup(bu_vls_addr(&invalid_name));
		    mav[0] = cmdname;
		    mav[1] = nname;
		    mav[2] = sname;
		    ret = ged_move(gedp, 3, (const char **)mav);
		    bu_ptbl_ins(&tmpnames, (long *)sname);
		    bu_ptbl_ins(&facetize_failed, (long *)sname);
		}
	    } else {
		bu_ptbl_ins(&facetize_failed, (long *)oname);
	    }
	}
	if (BU_PTBL_LEN(&spsr_succeeded) > 0) {
	    /* Put all of the spsr tessellations into their own top level comb for
	     * easy manual inspection */
	    struct bu_vls spsr_name = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&spsr_name, "%s_SPSR-0", newname);
	    bu_vls_incr(&spsr_name, NULL, NULL, &_db_uniq_test, (void *)gedp);
	    ret = _ged_combadd2(gedp, bu_vls_addr(&spsr_name), (int)BU_PTBL_LEN(&spsr_succeeded), (const char **)spsr_succeeded.buffer, 0, DB_OP_UNION, 0, 0, NULL, 0);
	}
    } else {
	for (i = 0; i < BU_PTBL_LEN(&ar_failed_nmg); i++) {
	    const char *oname = (const char *)BU_PTBL_GET(&ar_failed_nmg, i);
	    bu_ptbl_ins(&facetize_failed, (long *)oname);
	}
    }

    if (BU_PTBL_LEN(&facetize_failed) > 0) {
	/* Stash any failed regions into a top level comb for easy subsequent examination */
	struct bu_vls failed_name = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&failed_name, "%s_FAILED-0", newname);
	bu_vls_incr(&failed_name, NULL, NULL, &_db_uniq_test, (void *)gedp);
	ret = _ged_combadd2(gedp, bu_vls_addr(&failed_name), (int)BU_PTBL_LEN(&facetize_failed), (const char **)facetize_failed.buffer, 0, DB_OP_UNION, 0, 0, NULL, 0);
    }

    if (opts->in_place) {
	/* The "new" tree is actually the preservation of the old tree in this
	 * scenario, so swap all the region names */
	if (opts->verbosity) {
	    bu_log("Generation complete, swapping new geometry into original tree...\n");
	}
	for (i = 0; i < BU_PTBL_LEN(ar); i++) {
	    struct directory *n = (struct directory *)BU_PTBL_GET(ar, i);
	    if (_ged_facetize_obj_swap(gedp, n->d_namep, bu_avs_get(opts->c_map, n->d_namep)) != GED_OK) {
		ret = GED_ERROR;
		goto ged_facetize_regions_memfree;
	    }
	}
    }

ged_facetize_regions_memfree:

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    if (bu_vls_strlen(opts->nmg_log) && opts->method_flags & GED_FACETIZE_NMGBOOL && opts->verbosity > 1) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(opts->nmg_log));
    }

    /* Final report */
    bu_vls_printf(gedp->ged_result_str, "Objects successfully converted: %d of %d\n", BU_PTBL_LEN(ar) - BU_PTBL_LEN(&facetize_failed), BU_PTBL_LEN(ar));
    if (BU_PTBL_LEN(&spsr_succeeded)) {
	bu_vls_printf(gedp->ged_result_str, "Objects converted with SPSR: %d\n", BU_PTBL_LEN(&spsr_succeeded));
    }
    if (BU_PTBL_LEN(&facetize_failed)) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: %d objects failed:\n", BU_PTBL_LEN(&facetize_failed));
	for (i = 0; i < BU_PTBL_LEN(&facetize_failed); i++) {
	    bu_vls_printf(gedp->ged_result_str, "	%s\n", (const char *)BU_PTBL_GET(&facetize_failed, i));
	}
    }

    for (i = 0; i < BU_PTBL_LEN(&tmpnames); i++) {
	bu_free((char *)BU_PTBL_GET(&tmpnames, i), "temp name string");
    }
    bu_vls_free(&invalid_name);
    bu_ptbl_free(&tmpnames);
    bu_ptbl_free(pc);
    bu_ptbl_free(ar);
    bu_ptbl_free(&ar_failed_nmg);
    bu_ptbl_free(&facetize_failed);
    bu_ptbl_free(&spsr_succeeded);
    bu_free(pc, "pc table");
    bu_free(ar, "ar table");
    return ret;
}

HIDDEN int
_ged_vopt(struct bu_vls *UNUSED(msg), int UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    int *v_set = (int *)set_var;
    (*v_set) = (*v_set) + 1;
    return 0;
}


int
ged_facetize(struct ged *gedp, int argc, const char *argv[])
{
    int ret = GED_OK;
    static const char *usage = "Usage: facetize [ -nmhT | [--poisson] ] [old_obj1 | new_obj] [old_obj* ...] [old_objN | new_obj]\n";
    static const char *pusage = "Usage: facetize --poisson [-d #] [-w #] [ray sampling options] old_obj new_obj\n";
    int print_help = 0;
    struct _ged_facetize_opts *opts = _ged_facetize_opts_create();
    struct bu_opt_desc d[12];
    struct bu_opt_desc pd[12];

    BU_OPT(d[0], "h", "help",          "",  NULL,  &print_help,               "Print help and exit");
    BU_OPT(d[1], "v", "verbose",       "",  &_ged_vopt,  &(opts->verbosity),  "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[2], "q", "quiet",         "",  NULL,  &(opts->quiet),            "Suppress all output (overrides verbose flag)");
    BU_OPT(d[3], "",  "nmgbool",       "",  NULL,  &(opts->nmgbool),          "Use the standard libnmg boolean mesh evaluation to create output (Default)");
    BU_OPT(d[4], "m", "marching-cube", "",  NULL,  &(opts->marching_cube),    "Use the raytraced points and marching cube algorithm to create output");
    BU_OPT(d[5], "",  "poisson",       "",  NULL,  &(opts->screened_poisson), "Use raytraced points and SPSR to create output - run -h --poisson to see more options for this mode");
    BU_OPT(d[6], "n", "NMG",           "",  NULL,  &(opts->make_nmg),         "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh)");
    BU_OPT(d[7], "",  "TNURB",         "",  NULL,  &(opts->nmg_use_tnurbs),   "Create TNURB faces rather than planar approximations (experimental)");
    BU_OPT(d[8], "T", "triangles",     "",  NULL,  &(opts->triangulate),      "Generate a NMG solid using only triangles (BoTs, the default output, can only use triangles - this option mimics that behavior for NMG output.)");
    BU_OPT(d[9], "r", "regions",       "",  NULL,  &(opts->regions),          "For combs, walk the trees and create new copies of the hierarchies with each region replaced by a facetized evaluation of that region. (Default is to create one facetized object for all specified inputs.)");
    BU_OPT(d[10], "",  "in-place",      "",  NULL,  &(opts->in_place),         "Alter the existing tree/object to reference the facetized object.  May only specify one input object with this mode, and no output name.  (Warning: this option changes pre-existing geometry!)");
    BU_OPT_NULL(d[11]);

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
    BU_OPT(pd[10], "F",  "decimation-size-percentage",         "#", &bu_opt_fastf_t,     &(opts->decim_feat_perc),       "Percentage of the minimum bounding box length to use as a feature size when decimating. Default is 0.15, max is 1 (Note: setting this to 0 disables decimation, which for SPSR outputs may produce very large meshes).");
    BU_OPT_NULL(pd[11]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* It is known that libnmg will (as of 2018 anyway) throw a lot of
     * bu_bomb calls during operation. Because we need facetize to run
     * to completion and potentially try multiple ways to convert before
     * giving up, we need to un-hook any pre-existing bu_bomb hooks */
    bu_bomb_save_all_hooks(opts->saved_bomb_hooks);

    /* We will need to catch libnmg output and store it up for later
     * use, while still bu_logging our own status updates. Cache the
     * current bu_log hooks so they can be restored at need */
    bu_log_hook_save_all(opts->saved_log_hooks);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

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
    if (!opts->nmgbool && !opts->screened_poisson && !opts->marching_cube) {
	/* Default to NMGBOOL and SPSR active */
	opts->method_flags |= GED_FACETIZE_NMGBOOL;
	opts->method_flags |= GED_FACETIZE_SPSR;
    } else {
	if (opts->nmgbool)          opts->method_flags |= GED_FACETIZE_NMGBOOL;
	if (opts->screened_poisson) opts->method_flags |= GED_FACETIZE_SPSR;
	if (opts->marching_cube)    opts->method_flags |= GED_FACETIZE_MC;
    }

    if (opts->method_flags & GED_FACETIZE_SPSR) {
	/* Parse Poisson specific options, if we might be using that method */
	argc = bu_opt_parse(NULL, argc, argv, pd);

	if (argc < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Screened Poisson option parsing failed\n");
	    ret = GED_ERROR;
	}
    }

    /* Check for a couple of non-valid combinations */
    if (opts->method_flags == GED_FACETIZE_SPSR && opts->nmg_use_tnurbs) {
	bu_vls_printf(gedp->ged_result_str, "Note: Screened Poisson reconstruction does not support TNURBS output\n");
	ret = GED_ERROR;
	goto ged_facetize_memfree;
    }

    if (opts->triangulate && opts->nmg_use_tnurbs) {
	bu_vls_printf(gedp->ged_result_str, "both -T and -t specified!\n");
	ret = GED_ERROR;
	goto ged_facetize_memfree;
    }

    /* Check if we want/need help */
    if (print_help || (argc < 2 && !opts->in_place) || argc < 1) {
	_ged_cmd_help(gedp, usage, d);
	if (opts->method_flags & GED_FACETIZE_SPSR) {
	    _ged_cmd_help(gedp, pusage, pd);
	}
	ret = (print_help || argc < 1) ? GED_OK : GED_ERROR;
	goto ged_facetize_memfree;
    }

    /* Multi-region mode has a different processing logic */
    if (opts->regions) {
	ret = _ged_facetize_regions(gedp, argc, argv, opts);
    } else {
	ret = _ged_facetize_objlist(gedp, argc, argv, opts);
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
