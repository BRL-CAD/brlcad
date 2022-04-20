/*                         L O D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/view/lod.cpp
 *
 * The view lod subcommand.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <unordered_map>

#include "bu/cmd.h"
#include "bu/vls.h"
#include "bg/lod.h"

#include "../ged_private.h"
#include "./ged_view.h"

struct cache_data {
    int total;
    int done;
    std::unordered_map<unsigned long long, unsigned long long> hmap;
};

void
gen_cache_tree(struct db_full_path *path, struct db_i *dbip, union tree *tp, mat_t *curr_mat,
	void (*traverse_func) (struct db_full_path *, struct db_i *, mat_t *, void *), void *client_data)
{
    mat_t om, nm;
    struct directory *dp;

    if (!tp)
	return;

    RT_CK_FULL_PATH(path);
    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    gen_cache_tree(path, dbip, tp->tr_b.tb_right, curr_mat, traverse_func, client_data);
	    /* fall through */
	case OP_NOT:
	case OP_GUARD:
	case OP_XNOP:
	    gen_cache_tree(path, dbip, tp->tr_b.tb_left, curr_mat, traverse_func, client_data);
	    return;
	case OP_DB_LEAF:
	    if ((dp=db_lookup(dbip, tp->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		return;
	    } else {
		/* Update current matrix state to reflect the new branch of
		 * the tree. Either we have a local matrix, or we have an
		 * implicit IDN matrix. */
		MAT_COPY(om, *curr_mat);
		if (tp->tr_l.tl_mat) {
		    MAT_COPY(nm, tp->tr_l.tl_mat);
		} else {
		    MAT_IDN(nm);
		}
		bn_mat_mul(*curr_mat, om, nm);

		// Two things may prevent further processing - a hidden dp, or
		// a cyclic path.  Can check either here or in traverse_func -
		// just do it here since otherwise the logic would have to be
		// duplicated in all traverse functions.
		if (!(dp->d_flags & RT_DIR_HIDDEN)) {
		    db_add_node_to_full_path(path, dp);
		    if (!db_full_path_cyclic(path, NULL, 0)) {
			/* Keep going */
			traverse_func(path, dbip, curr_mat, client_data);
		    }
		}

		/* Done with branch - restore path, put back the old matrix state,
		 * and restore previous color settings */
		DB_FULL_PATH_POP(path);
		MAT_COPY(*curr_mat, om);
		return;
	    }
    }
    bu_log("gen_cache_tree: unrecognized operator %d\n", tp->tr_op);
    bu_bomb("gen_cache_tree: unrecognized operator\n");
}
/* To prepare a cache for all mesh instances in the tree, we need to walk it
 * and track the matrix */
void
gen_cache_cnt(struct db_full_path *path, struct db_i *dbip, mat_t *curr_mat, void *client_data)
{
    RT_CK_DBI(dbip);
    RT_CK_FULL_PATH(path);
    struct directory *dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;

	gen_cache_tree(path, dbip, comb->tree, curr_mat, gen_cache_cnt, client_data);
	rt_db_free_internal(&in);
    } else {
	// If we have a bot, it's cache time
	struct rt_db_internal dbintern;
	RT_DB_INTERNAL_INIT(&dbintern);
	struct rt_db_internal *ip = &dbintern;
	int ret = rt_db_get_internal(ip, dp, dbip, *curr_mat, &rt_uniresource);
	if (ret < 0)
	    return;
	if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
	    struct cache_data *d= (struct cache_data *)client_data;
	    d->total++;
	}
	rt_db_free_internal(&dbintern);
    }
}

/* To prepare a cache for all mesh instances in the tree, we need to walk it
 * and track the matrix */
void
gen_cache(struct db_full_path *path, struct db_i *dbip, mat_t *curr_mat, void *client_data)
{
    RT_CK_DBI(dbip);
    RT_CK_FULL_PATH(path);
    struct directory *dp = DB_FULL_PATH_CUR_DIR(path);
    if (!dp)
	return;
    if (dp->d_flags & RT_DIR_COMB) {
	struct rt_db_internal in;
	struct rt_comb_internal *comb;

	if (rt_db_get_internal(&in, dp, dbip, NULL, &rt_uniresource) < 0)
	    return;

	comb = (struct rt_comb_internal *)in.idb_ptr;

	gen_cache_tree(path, dbip, comb->tree, curr_mat, gen_cache, client_data);
	rt_db_free_internal(&in);
    } else {
	// If we have a bot, it's cache time
	struct rt_db_internal dbintern;
	RT_DB_INTERNAL_INIT(&dbintern);
	struct rt_db_internal *ip = &dbintern;
	int ret = rt_db_get_internal(ip, dp, dbip, *curr_mat, &rt_uniresource);
	if (ret < 0)
	    return;
	if (ip->idb_minor_type == DB5_MINORTYPE_BRLCAD_BOT) {
	    struct cache_data *d= (struct cache_data *)client_data;
	    d->done++;
	    struct bu_vls pname = BU_VLS_INIT_ZERO;
	    db_path_to_vls(&pname, path);
	    bu_log("Caching %s (%d of %d)\n", bu_vls_cstr(&pname), d->done, d->total);
	    bu_vls_free(&pname);
	    struct rt_bot_internal *bot = (struct rt_bot_internal *)ip->idb_ptr;
	    RT_BOT_CK_MAGIC(bot);
	    bg_mesh_lod_cache((const point_t *)bot->vertices, bot->num_vertices, bot->faces, bot->num_faces);
	}
	rt_db_free_internal(&dbintern);
    }
}

int
_view_cmd_lod(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    const char *usage_string = "view [options] lod [subcommand] [vals]";
    const char *purpose_string = "manage Level of Detail drawing settings";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct ged *gedp = gd->gedp;
    struct bview *gvp;
    int print_help = 0;
    static const char *usage = "view lod [0|1]\n"
	"view lod cache [clear]\n"
	"view lod enabled [0|1]\n"
	"view lod scale [factor]\n"
	"view lod point_scale [factor]\n"
	"view lod curve_scale [factor]\n"
	"view lod bot_threshold [face_cnt]\n";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &print_help,      "Print help");
    BU_OPT_NULL(d[1]);

    // We know we're the lod command - start processing args
    argc--; argv++;

    int ac = bu_opt_parse(NULL, argc, argv, d);
    argc = ac;

    if (print_help) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_HELP;
    }

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage:\n%s", usage);
	return BRLCAD_ERROR;
    }

    gvp = gedp->ged_gvp;
    if (gvp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "no current view defined\n");
	return BRLCAD_ERROR;
    }

    /* Print current state if no args are supplied */
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "enabled: %d\n", gvp->gv_s->adaptive_plot);
	bu_vls_printf(gedp->ged_result_str, "scale: %g\n", gvp->gv_s->lod_scale);
	bu_vls_printf(gedp->ged_result_str, "point_scale: %g\n", gvp->gv_s->point_scale);
	bu_vls_printf(gedp->ged_result_str, "curve_scale: %g\n", gvp->gv_s->curve_scale);
	bu_vls_printf(gedp->ged_result_str, "bot_threshold: %zd\n", gvp->gv_s->bot_threshold);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUIV(argv[0], "1")) {
	if (!gvp->gv_s->adaptive_plot) {
	    gvp->gv_s->adaptive_plot = 1;
	    int rac = 1;
	    const char *rav[2] = {"redraw", NULL};
	    ged_exec(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }
    if (BU_STR_EQUIV(argv[0], "0")) {
	if (gvp->gv_s->adaptive_plot) {
	    gvp->gv_s->adaptive_plot = 0;
	    int rac = 1;
	    const char *rav[2] = {"redraw", NULL};
	    ged_exec(gedp, rac, (const char **)rav);
	}
	return BRLCAD_OK;
    }


    if (BU_STR_EQUAL(argv[0], "cache")) {
	if (argc == 1) {
	    struct directory **all_paths;
	    int tops_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &all_paths);

	    // Count how many paths we're going to cache, so we can give
	    // the user at least some idea of progress
	    struct cache_data cache_data;
	    cache_data.done = 0;
	    cache_data.total = 0;
	    for (int i = 0; i < tops_cnt; i++) {
		mat_t mat;
		MAT_IDN(mat);
		struct db_full_path *fp;
		BU_GET(fp, struct db_full_path);
		db_full_path_init(fp);
		db_add_node_to_full_path(fp, all_paths[i]);
		gen_cache_cnt(fp, gedp->dbip, &mat, &cache_data);
		db_free_full_path(fp);
		BU_PUT(fp, struct db_full_path);
	    }

	    for (int i = 0; i < tops_cnt; i++) {
		mat_t mat;
		MAT_IDN(mat);
		struct db_full_path *fp;
		BU_GET(fp, struct db_full_path);
		db_full_path_init(fp);
		db_add_node_to_full_path(fp, all_paths[i]);
		gen_cache(fp, gedp->dbip, &mat, &cache_data);
		db_free_full_path(fp);
		BU_PUT(fp, struct db_full_path);
	    }

	    bu_free(all_paths, "all_paths");
	    bu_vls_printf(gedp->ged_result_str, "Caching complete");
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL(argv[1], "clear")) {
	    bg_mesh_lod_clear_cache(0);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to cache: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }


    if (BU_STR_EQUAL(argv[0], "enabled")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%d\n", gvp->gv_s->adaptive_plot);
	    return BRLCAD_OK;
	}
	int rac = 1;
	const char *rav[2] = {"redraw", NULL};
	if (bu_str_true(argv[1])) {
	    gvp->gv_s->adaptive_plot = 1;
	    ged_exec(gedp, rac, (const char **)rav);
	    return BRLCAD_OK;
	}
	if (bu_str_false(argv[1])) {
	    gvp->gv_s->adaptive_plot = 0;
	    ged_exec(gedp, rac, (const char **)rav);
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "unknown argument to enabled: %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->lod_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->lod_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "point_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->point_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to point_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->point_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "curve_scale")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%g\n", gvp->gv_s->curve_scale);
	    return BRLCAD_OK;
	}
	fastf_t scale = 1.0;
	if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[1], (void *)&scale) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to curve_scale: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->curve_scale = scale;
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "bot_threshold")) {
	if (argc == 1) {
	    bu_vls_printf(gedp->ged_result_str, "%zd\n", gvp->gv_s->bot_threshold);
	    return BRLCAD_OK;
	}
	int bcnt = 0;
	if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&bcnt) != 1 || bcnt < 0) {
	    bu_vls_printf(gedp->ged_result_str, "unknown argument to bot_threshold: %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}
	gvp->gv_s->bot_threshold = bcnt;
	return BRLCAD_OK;

    }

    bu_vls_printf(gedp->ged_result_str, "unknown subcommand: %s\n", argv[0]);
    return BRLCAD_ERROR;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
