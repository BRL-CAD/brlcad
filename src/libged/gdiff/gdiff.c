/*                         G D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2020 United States Government as represented by
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
/** @file libged/gdiff.c
 *
 * The gdiff command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "rt/db_fullpath.h"
#include "rt/db_diff.h"
#include "analyze.h"

#include "../ged_private.h"

static void check_walk(
	int *diff,
       	struct bu_vls *msgs,
       	struct db_i *dbip,
	struct db_full_path *p1,
	struct db_full_path *p2);
static void
check_walk_subtree(int *diff, struct bu_vls *msgs, struct db_i *dbip, struct db_full_path *p1, struct db_full_path *p2, union tree *tp1, union tree *tp2)
{
    int idn1, idn2;
    struct directory *dp1, *dp2;

    if (!diff)
       	return;

    if ((!tp1 && tp2) || (tp1 && !tp2)) {
	(*diff) = 1;
	return;
    }

    if (!tp1 || !tp2)
	return;

    if (tp1->tr_op != tp2->tr_op) {
	(*diff) = 1;
	return;
    }

    switch (tp1->tr_op) {
	case OP_DB_LEAF:
	    idn1 = (!tp1->tr_l.tl_mat || bn_mat_is_identity(tp1->tr_l.tl_mat));
	    idn2 = (!tp2->tr_l.tl_mat || bn_mat_is_identity(tp2->tr_l.tl_mat));
	    if (idn1 != idn2) {
		(*diff) = 1;
		return;
	    }
	    if (tp1->tr_l.tl_mat && tp2->tr_l.tl_mat) {
		if (!bn_mat_is_equal(tp1->tr_l.tl_mat, tp2->tr_l.tl_mat, &dbip->dbi_wdbp->wdb_tol)) {
		    (*diff) = 1;
		    return;
		}
	    }

	    dp1 = db_lookup(dbip, tp1->tr_l.tl_name, LOOKUP_NOISY);
	    dp2 = db_lookup(dbip, tp2->tr_l.tl_name, LOOKUP_NOISY);
	    if (dp1 != RT_DIR_NULL)
		db_add_node_to_full_path(p1, dp1);
	    if (dp2 != RT_DIR_NULL)
		db_add_node_to_full_path(p2, dp2);

	    check_walk(diff, msgs, dbip, p1, p2);

	    if (dp1 != RT_DIR_NULL)
		DB_FULL_PATH_POP(p1);
	    if (dp2 != RT_DIR_NULL)
		DB_FULL_PATH_POP(p2);

	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    check_walk_subtree(diff, msgs, dbip, p1, p2, tp1->tr_b.tb_left, tp2->tr_b.tb_left);
	    check_walk_subtree(diff, msgs, dbip, p1, p2, tp1->tr_b.tb_right, tp2->tr_b.tb_right);
	    break;
	default:
	    bu_log("check_walk_subtree: unrecognized operator %d\n", tp1->tr_op);
	    bu_bomb("check_walk_subtree: unrecognized operator\n");
    }
}

static void
check_walk(int *diff,
	struct bu_vls *msgs,
	struct db_i *dbip,
	struct db_full_path *p1,
	struct db_full_path *p2
       	)
{
    if ((!p1 && !p2) || !diff || (*diff)) {
	return; /* nothing to do */
    }

    if ((!p1 && p2) || (p1 && !p2)) {
	*diff = 1;
	return;
    }

    if (p1->fp_len != p2->fp_len) {
	*diff = 1;
	if (msgs) {
	    char *p1s = db_path_to_string(p1);
	    char *p2s = db_path_to_string(p2);
	    bu_vls_printf(msgs, "%s and %s have different lengths - tree difference found.\n", p1s, p2s);
	    bu_free(p1s, "p1s");
	    bu_free(p2s, "p2s");
	}
	return;
    }

    struct directory *dp1 = DB_FULL_PATH_CUR_DIR(p1);
    struct directory *dp2 = DB_FULL_PATH_CUR_DIR(p2);

    if (dp1->d_flags != dp2->d_flags) {
	*diff = 1;
	if (msgs) {
	    char *p1s = db_path_to_string(p1);
	    char *p2s = db_path_to_string(p2);
	    bu_vls_printf(msgs, "%s and %s have flag differences.\n", p1s, p2s);
	    bu_free(p1s, "p1s");
	    bu_free(p2s, "p2s");
	}
	return;
    }

    if (dp1->d_flags & RT_DIR_COMB) {

	struct rt_db_internal in1, in2;
	struct rt_comb_internal *comb1, *comb2;

	if (rt_db_get_internal5(&in1, dp1, dbip, NULL, &rt_uniresource) < 0) {
	    *diff = 1;
	    return;
	}

	if (rt_db_get_internal5(&in2, dp2, dbip, NULL, &rt_uniresource) < 0) {
	    *diff = 1;
	    return;
	}

	comb1 = (struct rt_comb_internal *)in1.idb_ptr;
	comb2 = (struct rt_comb_internal *)in2.idb_ptr;
	check_walk_subtree(diff, msgs, dbip, p1, p2, comb1->tree, comb2->tree);
	rt_db_free_internal(&in1);
	rt_db_free_internal(&in2);

	return;
    }

    /* If we have two solids, use db_diff_dp */
    int dr = db_diff_dp(dbip, dbip, dp1, dp2, &dbip->dbi_wdbp->wdb_tol, DB_COMPARE_ALL, NULL);
    if (dr != DIFF_UNCHANGED) {
	char *p1s = db_path_to_string(p1);
	char *p2s = db_path_to_string(p2);
	bu_vls_printf(msgs, "%s and %s differ.\n", p1s, p2s);
	bu_free(p1s, "p1s");
	bu_free(p2s, "p2s");
	*diff = 1;
    }
}


int
ged_gdiff_core(struct ged *gedp, int argc, const char *argv[])
{
    size_t i;
    struct analyze_raydiff_results *results;
    struct bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6 };

    long verbosity = 0;
    int structure_diff = 0;
    int view_left = 0;
    int view_right = 0;
    int view_overlap = 0;
    int grazereport = 0;
    int print_help = 0;
    const char *left_obj;
    const char *right_obj;
    fastf_t len_tol = 0;
    int ret_ac = 0;
    /* Skip command name */
    int ac = argc - 1;
    const char **av = argv+1;

    struct bu_opt_desc d[9];
    BU_OPT(d[0], "h", "help",         "",  NULL,            &print_help,   "Print help.");
    BU_OPT(d[1], "g", "grid-spacing", "#", &bu_opt_fastf_t, &len_tol,      "Controls spacing of test ray grids (units are mm.)");
    BU_OPT(d[2], "l", "view-left",    "",  NULL,            &view_left,    "Visualize volumes occurring only in the left object");
    BU_OPT(d[3], "b", "view-both",    "",  NULL,            &view_overlap, "Visualize volumes common to both objects");
    BU_OPT(d[4], "r", "view-right",   "",  NULL,            &view_right,   "Visualize volumes occurring only in the right object");
    BU_OPT(d[5], "G", "grazing",      "",  NULL,            &grazereport,  "Report differences in grazing hits");
    BU_OPT(d[6], "S", "structure",    "",  NULL,            &structure_diff,  "Do a diff of tree structures (matrices and objects, ignoring object names.)  This mode is not raytrace based.");
    BU_OPT(d[7], "v", "verbosity",      "",  &bu_opt_incr_long, &verbosity,   "Increase output verbosity.");
    BU_OPT_NULL(d[8]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    ret_ac = bu_opt_parse(NULL, ac, av, d);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (print_help) {
	char *usage = bu_opt_describe((struct bu_opt_desc *)&d, NULL);
	bu_vls_printf(gedp->ged_result_str, "Usage: gdiff [opts] left_obj right_obj\n");
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", usage);
	bu_vls_printf(gedp->ged_result_str, "When visualizing raytrace based diff results, red segments are those generated\nonly from intersections with \"left_obj\" while blue segments represent\nintersections unique to \"right_obj\".  White segments represent intersections\ncommon to both objects. By default, in raytracing mode, segments unique to left and right objects are displayed.  ");
	bu_vls_printf(gedp->ged_result_str, "If no tolerance is given, a default of 100mm is used.\n\n Be careful of using too fine a grid - finer grides will (up to a point) yield better visuals, but too fine a grid can cause very long raytracing times.");
	bu_free(usage, "help str");
	return GED_OK;
    }

    if (ret_ac != 2) {
	const char *usage = bu_opt_describe((struct bu_opt_desc *)&d, NULL);
	bu_vls_printf(gedp->ged_result_str, "wrong number of args.\nUsage: gdiff [opts] left_obj right_obj\nOptions:\n%s", usage);
	bu_free((char *)usage, "help str");
	return GED_ERROR;
    } else {
	left_obj = av[0];
	right_obj = av[1];
    }

    if (structure_diff) {
	int diff = 0;
	struct bu_vls smsgs = BU_VLS_INIT_ZERO;
	struct db_full_path *lp, *rp;
	BU_GET(lp, struct db_full_path);
	db_full_path_init(lp);
	BU_GET(rp, struct db_full_path);
	db_full_path_init(rp);

	struct directory *dp1, *dp2;
	if ((dp1 = db_lookup(gedp->ged_wdbp->dbip, left_obj, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    return GED_ERROR;
	}
	if ((dp2 = db_lookup(gedp->ged_wdbp->dbip, right_obj, LOOKUP_NOISY)) == RT_DIR_NULL) {
	    return GED_ERROR;
	}

	db_add_node_to_full_path(lp, dp1);
	db_add_node_to_full_path(rp, dp2);

	check_walk(&diff, &smsgs, gedp->ged_wdbp->dbip, lp, rp);

	db_free_full_path(lp);
	BU_PUT(lp, struct db_full_path);
	db_free_full_path(rp);
	BU_PUT(rp, struct db_full_path);

	if (bu_vls_strlen(&smsgs))
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&smsgs));

	bu_vls_free(&smsgs);

	bu_vls_printf(gedp->ged_result_str, "%d", diff);

	return GED_OK;
    }

    /* There are possible convention-based interpretations of 1, 2, 3, 4 and n args
     * beyond those used as options.  For the shortest cases, the interpretation depends
     * on whether one or two .g files are known:
     *
     * a) No .g file specified
     * 1 - objname (error)
     * 1 - file.g (error)
     * 2 - file.g objname (error)
     * 2 - obj1 obj2 (error)
     * 3 - file.g obj1 obj2 (diff two objects in file.g)
     *
     * b) Current .g file known
     * 1 - objname (error)
     * 1 - file.g (diff full .g file contents)
     * 2 - file.g objname (diff obj between current.g and file.g)
     * 2 - obj1 obj2 (diff two objects in current .g)
     * 3 - file.g obj1 obj2 (diff all listed objects between current.g and file.g)
     *
     * .g file args must always come first.
     *
     * A maximum of two .g files can be specified - any additional specification
     * of .g files is an error.
     *
     * When only one environment is specified, all other args must define pairs of
     * objects to compare.
     *
     * When two environments are known, all args will be compared by their instances
     * in the first environment and the second, not against each other in either
     * environment.
     *
     * When there is a current .g environment and two additional .g files are
     * specified, the argv environments will override use of the "current" .g environment.
     */

    if (db_lookup(gedp->ged_wdbp->dbip, left_obj, LOOKUP_NOISY) == RT_DIR_NULL) {
	return GED_ERROR;
    }
    if (db_lookup(gedp->ged_wdbp->dbip, right_obj, LOOKUP_NOISY) == RT_DIR_NULL) {
	return GED_ERROR;
    }

    /* If we don't have a tolerance, try to guess something sane from the bbox */
    if (NEAR_ZERO(len_tol, RT_LEN_TOL)) {
	point_t rpp_min, rpp_max;
	point_t obj_min, obj_max;
	VSETALL(rpp_min, INFINITY);
	VSETALL(rpp_max, -INFINITY);
	ged_get_obj_bounds(gedp, 1, (const char **)&left_obj, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	ged_get_obj_bounds(gedp, 1, (const char **)&right_obj, 0, obj_min, obj_max);
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
	len_tol = DIST_PNT_PNT(rpp_max, rpp_min) * 0.01;
    }
    tol.dist = len_tol;

    analyze_raydiff(&results, gedp->ged_wdbp->dbip, left_obj, right_obj, &tol, !grazereport);

    /* TODO - may want to integrate with a "regular" diff and report intelligently.  Needs
     * some thought. */
    if (BU_PTBL_LEN(results->left) > 0 || BU_PTBL_LEN(results->right) > 0) {
	bu_vls_printf(gedp->ged_result_str, "1");
    } else {
	bu_vls_printf(gedp->ged_result_str, "0");
    }

    /* For now, graphical output is the main output of this mode, so if we don't have any
     * specifics do left and right */
    if (!view_left && !view_overlap && !view_right) {
	view_left = 1;
	view_right = 1;
	view_overlap = 0;
    }

    if (view_left || view_overlap || view_right) {
	/* Visualize the differences */
	struct bu_list *vhead;
	point_t a, b;
	struct bn_vlblock *vbp;
	struct bu_list local_vlist;
	BU_LIST_INIT(&local_vlist);
	vbp = bn_vlblock_init(&local_vlist, 32);

	/* Clear any previous diff drawing */
	if (db_lookup(gedp->ged_wdbp->dbip, "diff_visualff", LOOKUP_QUIET) != RT_DIR_NULL)
	    dl_erasePathFromDisplay(gedp, "diff_visualff", 1);
	if (db_lookup(gedp->ged_wdbp->dbip, "diff_visualff0000", LOOKUP_QUIET) != RT_DIR_NULL)
	    dl_erasePathFromDisplay(gedp, "diff_visualff0000", 1);
	if (db_lookup(gedp->ged_wdbp->dbip, "diff_visualffffff", LOOKUP_QUIET) != RT_DIR_NULL)
	    dl_erasePathFromDisplay(gedp, "diff_visualffffff", 1);

	/* Draw left-only lines */
	if (view_left) {
	    for (i = 0; i < BU_PTBL_LEN(results->left); i++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->left, i);
		VMOVE(a, dseg->in_pt);
		VMOVE(b, dseg->out_pt);
		vhead = bn_vlblock_find(vbp, 255, 0, 0); /* should be red */
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BN_VLIST_LINE_DRAW);
	    }
	}
	/* Draw overlap lines */
	if (view_overlap) {
	    for (i = 0; i < BU_PTBL_LEN(results->both); i++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->both, i);
		VMOVE(a, dseg->in_pt);
		VMOVE(b, dseg->out_pt);
		vhead = bn_vlblock_find(vbp, 255, 255, 255); /* should be white */
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BN_VLIST_LINE_DRAW);

	    }
	}
	/* Draw right lines */
	if (view_right) {
	    for (i = 0; i < BU_PTBL_LEN(results->right); i++) {
		struct diff_seg *dseg = (struct diff_seg *)BU_PTBL_GET(results->right, i);
		VMOVE(a, dseg->in_pt);
		VMOVE(b, dseg->out_pt);
		vhead = bn_vlblock_find(vbp, 0, 0, 255); /* should be blue */
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, a, BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vbp->free_vlist_hd, vhead, b, BN_VLIST_LINE_DRAW);
	    }
	}

	_ged_cvt_vlblock_to_solids(gedp, vbp, "diff_visual", 0);

	bn_vlist_cleanup(&local_vlist);
	bn_vlblock_free(vbp);
    }
    analyze_raydiff_results_free(results);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl gdiff_cmd_impl = {
    "gdiff",
    ged_gdiff_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd gdiff_cmd = { &gdiff_cmd_impl };
const struct ged_cmd *gdiff_cmds[] = { &gdiff_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  gdiff_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
