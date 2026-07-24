/*                  C O M B . C
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
/** @file libged/comb.c
 *
 * The comb command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/sort.h"
#include "bg/trimesh.h"
#include "wdb.h"
#include "analyze.h"

#include "../ged_private.h"


struct comb_args {
    int set_comb;
    int do_decimation;
    int set_region;
    int wrap;
    int flatten;
    int lift_region;
    int require_new;
};

static const struct bu_cmd_schema *comb_schema(void);
static int comb_dispatch_modern(struct ged *gedp, int argc, const char **argv);
static void comb_tree_show_help(struct ged *gedp);

static int
region_flag_set(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    dp->d_flags |= RT_DIR_REGION;
    (void)bu_avs_add(&avs, "region", "R");
    if (db5_update_attributes(dp, &avs, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str,
		"Error: failed to update attributes\n");
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
region_flag_clear(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    dp->d_flags = dp->d_flags & ~(RT_DIR_REGION);
    (void)bu_avs_remove(&avs, "region");
    if (db5_replace_attributes(dp, &avs, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str,
		"Error: failed to update attributes\n");
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
color_shader_clear(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    (void)bu_avs_remove(&avs, "rgb");
    (void)bu_avs_remove(&avs, "color");
    (void)bu_avs_remove(&avs, "shader");
    (void)bu_avs_remove(&avs, "oshader");
    if (db5_replace_attributes(dp, &avs, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str,
		"Error: failed to update attributes\n");
	bu_avs_free(&avs);
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
comb_tree_clear(struct ged *gedp, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    /* Clear the tree from the original object */
    GED_DB_GET_INTERN(gedp, &intern, dp, (matp_t)NULL, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);
    comb = (struct rt_comb_internal *)(&intern)->idb_ptr;
    RT_CK_COMB(comb);
    db_free_tree(comb->tree);
    comb->tree = TREE_NULL;
    db5_sync_comb_to_attr(&((&intern)->idb_avs), comb);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_put_internal(wdbp, dp->d_namep, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", dp->d_namep);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


static int
comb_wrap(struct ged *gedp, struct directory *dp) {

    struct bu_vls orig_name, comb_child_name;
    struct bu_external external;
    struct directory *orig_dp = dp;
    struct directory *new_dp;

    bu_vls_init(&orig_name);
    bu_vls_init(&comb_child_name);

    bu_vls_sprintf(&orig_name, "%s", dp->d_namep);
    bu_vls_sprintf(&comb_child_name, "%s.c", dp->d_namep);

    /* First, make sure the target comb name for wrapping doesn't already exist */
    new_dp = db_lookup(gedp->dbip, bu_vls_addr(&comb_child_name), LOOKUP_QUIET);
    if (new_dp != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s already exists in the database, cannot wrap %s", bu_vls_addr(&comb_child_name), dp->d_namep);
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }

    /* Create a copy of the comb using a new name */
    if (db_get_external(&external, dp, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Wrapping %s: Database read error retrieving external, aborting\n", dp->d_namep);
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_export_external(wdbp, &external, bu_vls_addr(&comb_child_name), dp->d_flags, dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(gedp->ged_result_str, "Failed to write new object (%s) to database - aborting!!\n", bu_vls_addr(&comb_child_name));
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    bu_free_external(&external);

    /* Load new obj.c comb and clear its region flag, if any */
    new_dp = db_lookup(gedp->dbip, bu_vls_addr(&comb_child_name), LOOKUP_QUIET);
    if (new_dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Wrapping %s: creation of %s failed!", dp->d_namep, bu_vls_addr(&comb_child_name));
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    if (region_flag_clear(gedp, new_dp) & BRLCAD_ERROR) {
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    if (color_shader_clear(gedp, new_dp) == BRLCAD_ERROR) {
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }

    /* Clear the tree from the original object */
    if (comb_tree_clear(gedp, dp) == BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s tree clearing failed", bu_vls_addr(&orig_name));
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    orig_dp = db_lookup(gedp->dbip, bu_vls_addr(&orig_name), LOOKUP_QUIET);
    if (orig_dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s tree clearing failed", bu_vls_addr(&orig_name));
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }

    /* add "child" comb to the newly cleared parent */
    if (_ged_combadd(gedp, new_dp, orig_dp->d_namep, 0, WMOP_UNION, 0, 0) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error adding '%s' (with op '%c') to '%s'\n", bu_vls_addr(&comb_child_name), WMOP_UNION, dp->d_namep);
	bu_vls_free(&comb_child_name);
	bu_vls_free(&orig_name);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&comb_child_name);
    bu_vls_free(&orig_name);

    return BRLCAD_OK;
}

/* bu_sort functions for solids */
static int
name_compare(const void *d1, const void *d2, void *UNUSED(arg))
{
    struct directory *dp1 = *(struct directory **)d1;
    struct directory *dp2 = *(struct directory **)d2;
    return bu_strcmp((const char *)dp2->d_namep, (const char *)dp1->d_namep);
}

/* Define search strings that describe plans for finding:
 *
 *  1. non-unioned objects in a comb's tree
 *  2. solid objects in a comb's tree
 *  3. comb objects in a comb's tree
 *  4. All comb objects below any comb's tree except the current comb
 *
 *  Run the first search, and quit if any non-union ops are present.
 *  If all clear, search for the solid and comb lists.  Clear the old
 *  tree and union in all the solids - solids are sorted via bu_sort.
 *  If we have combs, run the not-in-this-comb-tree search and check
 *  which (if any) of the combs under the current comb are not used
 *  elsewhere.  For those that are not, remove them.
 */
static int
comb_flatten(struct ged *gedp, struct directory *dp)
{
    int result_cnt = 0;
    int obj_cnt = 0;
    struct directory **all_paths;
    char *only_unions_in_tree_plan = "! -bool u";
    char *solids_in_tree_plan = "! -type comb";
    char *combs_in_tree_plan = "-type comb";
    struct bu_ptbl solids = BU_PTBL_INIT_ZERO;
    struct bu_ptbl combs = BU_PTBL_INIT_ZERO;
    struct bu_ptbl combs_outside_of_tree = BU_PTBL_INIT_ZERO;
    struct bu_vls plan_string;
    struct directory **dp_curr;

    /* if there are non-union booleans in this comb's tree, error out */
    result_cnt = db_search(NULL, DB_SEARCH_TREE, only_unions_in_tree_plan, 1, &dp, gedp->dbip, NULL, NULL, NULL);
    if (result_cnt) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s tree contains non-union booleans", dp->d_namep);
	return BRLCAD_ERROR;
    }

    /* Find the solids and combs in the tree */
    (void)db_search(&solids, DB_SEARCH_RETURN_UNIQ_DP, solids_in_tree_plan, 1, &dp, gedp->dbip, NULL, NULL, NULL);
    (void)db_search(&combs, DB_SEARCH_RETURN_UNIQ_DP, combs_in_tree_plan, 1, &dp, gedp->dbip, NULL, NULL, NULL);

    /* If it's all solids already, nothing to do */
    if (!BU_PTBL_LEN(&combs)) {
	db_search_free(&solids);
	db_search_free(&combs);
	return BRLCAD_OK;
    }


    /* Find the combs NOT in the tree */
    obj_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &all_paths);
    bu_vls_init(&plan_string);
    bu_vls_sprintf(&plan_string, "-mindepth 1 ! -below -name %s -type comb", dp->d_namep);
    (void)db_search(&combs_outside_of_tree, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_addr(&plan_string), obj_cnt, all_paths, gedp->dbip, NULL, NULL, NULL);
    bu_vls_free(&plan_string);

    /* Done searching - now we can free the path list and clear the original tree */
    bu_free(all_paths, "free db_tops output");
    if (comb_tree_clear(gedp, dp) == BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s tree clearing failed", dp->d_namep);
	db_search_free(&solids);
	db_search_free(&combs);
	db_search_free(&combs_outside_of_tree);
	return BRLCAD_ERROR;
    }

    /* Sort the solids and union them into a new tree for dp */
    if (BU_PTBL_LEN(&solids)) {
	bu_sort((void *)BU_PTBL_BASEADDR(&solids), BU_PTBL_LEN(&solids), sizeof(struct directory *), name_compare, NULL);
	for (BU_PTBL_FOR(dp_curr, (struct directory **), &solids)) {
	    /* add "child" comb to the newly cleared parent */
	    if (_ged_combadd(gedp, (*dp_curr), dp->d_namep, 0, WMOP_UNION, 0, 0) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Error adding '%s' to '%s'\n", (*dp_curr)->d_namep, dp->d_namep);
		db_search_free(&solids);
		db_search_free(&combs);
		db_search_free(&combs_outside_of_tree);
		return BRLCAD_ERROR;
	    }
	}
    }
    /* Done with solids */
    db_search_free(&solids);

    /* Remove any combs that were in dp and not used elsewhere from the .g file */
    for (BU_PTBL_FOR(dp_curr, (struct directory **), &combs)) {
	if (bu_ptbl_locate(&combs_outside_of_tree, (const long *)(*dp_curr)) == -1 && ((*dp_curr) != dp)) {
	    /* This comb is only part of the flattened tree - remove */
	    bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", (*dp_curr)->d_namep);
	    if (db_delete(gedp->dbip, (*dp_curr)) != 0 || db_dirdelete(gedp->dbip, (*dp_curr)) == 0) {
		bu_vls_trunc(gedp->ged_result_str, 0);
	    } else {
		db_search_free(&combs);
		db_search_free(&combs_outside_of_tree);
		return BRLCAD_ERROR;
	    }
	}
    }

    db_search_free(&combs);
    db_search_free(&combs_outside_of_tree);
    return BRLCAD_OK;
}


/* "Lift a region flag to the specified comb, removing all region
 * flags below in the tree if practical.
 */
static int
comb_lift_region(struct ged *gedp, struct directory *dp)
{
    int j;
    int obj_cnt;
    struct directory **all_paths;
    char *regions_in_tree_plan = "-type region";

    struct bu_ptbl combs_outside_of_tree = BU_PTBL_INIT_ZERO;
    struct bu_ptbl regions = BU_PTBL_INIT_ZERO;
    struct bu_ptbl regions_to_clear = BU_PTBL_INIT_ZERO;
    struct bu_ptbl regions_to_wrap = BU_PTBL_INIT_ZERO;

    struct bu_vls plan_string;
    struct directory **dp_curr;
    int failure_case = 0;

    /* Find the regions - need full paths here, because we'll be checking parents */
    (void)db_search(&regions, DB_SEARCH_TREE, regions_in_tree_plan, 1, &dp, gedp->dbip, NULL, NULL, NULL);

    /* If it's all non-region combs and solids already, nothing to do except possibly set the region flag*/
    if (!BU_PTBL_LEN(&regions)) {
	db_search_free(&regions);
	if (!(dp->d_flags & RT_DIR_REGION))
	    return region_flag_set(gedp, dp);
	return BRLCAD_OK;
    }

    /* Find the combs NOT in the tree */
    obj_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &all_paths);
    bu_vls_init(&plan_string);
    bu_vls_sprintf(&plan_string, "-mindepth 1 ! -below -name %s -type comb", dp->d_namep);
    (void)db_search(&combs_outside_of_tree, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_addr(&plan_string), obj_cnt, all_paths, gedp->dbip, NULL, NULL, NULL);
    bu_vls_free(&plan_string);

    /* release our db_ls path */
    bu_free(all_paths, "free db_ls top paths");

    /* check for entry last node in combs_outside of tree
     *    - if NO, add to quash region flag bu_ptbl list (uniq insert).  If yes, continue.
     * get parent of entry last node
     * check for parent node in combs_outside_of_tree
     * if problem found, append specifics to ged_result_str, set fail flag
     * if no problem found, add to bu_ptbl list of regions to wrap (uniq insert)
     * no point in storing the specific parent, since we'll in-tree mvall in any case to update */
    bu_ptbl_init(&regions_to_clear, 64, "regions to clear");
    bu_ptbl_init(&regions_to_wrap, 64, "regions to wrap");
    for (j = (int)BU_PTBL_LEN(&regions) - 1; j >= 0; j--) {
	struct db_full_path *entry = (struct db_full_path *)BU_PTBL_GET(&regions, j);
	struct directory *dp_curr_dir =  DB_FULL_PATH_CUR_DIR(entry);
	struct directory *dp_parent = DB_FULL_PATH_GET(entry, entry->fp_len-2);
	if (dp_curr_dir != dp) {
	    if (bu_ptbl_locate(&combs_outside_of_tree, (const long *)(dp_curr_dir)) == -1) {
		bu_ptbl_ins_unique(&regions_to_clear, (long *)(dp_curr_dir));
	    } else {
		if (bu_ptbl_locate(&combs_outside_of_tree, (const long *)(dp_parent)) == -1 || (dp_parent == dp)) {
		    bu_ptbl_ins_unique(&regions_to_wrap, (long *)(dp_curr_dir));
		} else {
		    if (!failure_case) {
			bu_vls_sprintf(gedp->ged_result_str,  "Comb region lift failed - the following combs in the tree contain regions and are also used outside the tree of %s:\n\n", dp->d_namep);
			failure_case = 1;
		    }
		    bu_vls_printf(gedp->ged_result_str,  "%s, containing region %s\n", dp_parent->d_namep, dp_curr_dir->d_namep);
		}
	    }
	}
    }
    db_search_free(&regions);
    db_search_free(&combs_outside_of_tree);

    if (failure_case) {
	bu_vls_printf(gedp->ged_result_str,  "\nThe above combs must be reworked before region lifting the tree of %s can succeed.\n", dp->d_namep);
	bu_ptbl_free(&regions_to_clear);
	bu_ptbl_free(&regions_to_wrap);
	return BRLCAD_ERROR;
    }

    /* Easy case first - if we can just clear the region flag, do it. */
    for (BU_PTBL_FOR(dp_curr, (struct directory **), &regions_to_clear)) {
	if ((*dp_curr) != dp) {
	    if (region_flag_clear(gedp, (*dp_curr)) == BRLCAD_ERROR) {
		bu_ptbl_free(&regions_to_clear);
		bu_ptbl_free(&regions_to_wrap);
		return BRLCAD_ERROR;
	    }
	}
    }
    bu_ptbl_free(&regions_to_clear);

    /* Now the tricky case.  We need to wrap regions, then insert their
     * comb definitions into the tree of dp->d_namep and its children.
     * Because we're doing that, the combs *in* the tree will be changing
     * as we go.
     */
    {
	struct bu_vls new_comb_name = BU_VLS_INIT_ZERO;
	struct bu_ptbl stack = BU_PTBL_INIT_ZERO;
	struct directory *new_comb;

	char *combs_in_tree_plan = "-type comb";
	struct bu_ptbl combs_in_tree = BU_PTBL_INIT_ZERO;

	bu_ptbl_init(&stack, 64, "comb mvall working stack");

	(void)db_search(&combs_in_tree, DB_SEARCH_RETURN_UNIQ_DP, combs_in_tree_plan, 1, &dp, gedp->dbip, NULL, NULL, NULL);
	bu_ptbl_ins(&combs_in_tree, (long *)dp);
	for (BU_PTBL_FOR(dp_curr, (struct directory **), &regions_to_wrap)) {
	    if ((*dp_curr) != dp) {
		struct directory **dp_comb_from_tree;
		if (comb_wrap(gedp, (*dp_curr)) == BRLCAD_ERROR) {
		    bu_ptbl_free(&regions_to_wrap);
		    db_search_free(&combs_in_tree);
		    bu_ptbl_free(&stack);
		    return BRLCAD_ERROR;
		} else {
		    bu_vls_sprintf(&new_comb_name, "%s.c", (*dp_curr)->d_namep);
		    new_comb = db_lookup(gedp->dbip, bu_vls_addr(&new_comb_name), LOOKUP_QUIET);
		}
		for (BU_PTBL_FOR(dp_comb_from_tree, (struct directory **), &combs_in_tree)) {
		    bu_ptbl_reset(&stack);
		    if (db_comb_mvall((*dp_comb_from_tree), gedp->dbip, (*dp_curr)->d_namep, bu_vls_addr(&new_comb_name), &stack) == 2) {
			db_search_free(&combs_in_tree);
			bu_ptbl_free(&regions_to_wrap);
			bu_ptbl_free(&stack);
			return BRLCAD_ERROR;
		    }
		}
		bu_ptbl_ins(&combs_in_tree, (long *)new_comb);
		bu_ptbl_rm(&combs_in_tree, (long *)(*dp_curr));
	    }
	}
	bu_ptbl_free(&stack);
	db_search_free(&combs_in_tree);
    }
    bu_ptbl_free(&regions_to_wrap);

    /* Finally, set the region flag on the toplevel comb */
    if (region_flag_set(gedp, dp) == BRLCAD_ERROR)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

static int
comb_decimate(struct ged *gedp, struct directory *dp)
{
    unsigned int i;
    int ret = BRLCAD_OK;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    struct db_i *dbip = gedp->dbip;

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, 1, &dp, gedp->dbip, NULL, NULL, NULL) < 0) {
	bu_log("Problem searching for BoTs - aborting.\n");
	ret = BRLCAD_ERROR;
	goto comb_decimate_memfree;
    }

    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct bn_tol btol = BN_TOL_INIT_TOL;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int not_solid = 0;
	int old_face_cnt;
	int edges_removed;
	point_t obj_min, obj_max;
	fastf_t bbox_diag;
	fastf_t avg_thickness;
	fastf_t fs;
	struct directory *bot_dp = (struct directory *)BU_PTBL_GET(bot_dps, i);
	GED_DB_GET_INTERN(gedp, &intern, bot_dp, bn_mat_identity, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char **)&bot_dp->d_namep, 0, obj_min, obj_max);
	bbox_diag = DIST_PNT_PNT(obj_min, obj_max);

	/* Get avg thickness from raytracer */
	if (rt_gen_obj_pnts(NULL, &avg_thickness, dbip, bot_dp->d_namep, &btol, RT_GEN_OBJ_PNTS_RAND, 100000, 5, 0)) {
	    fs = 0.0005*bbox_diag;
	} else {
	    fs = 0.01*avg_thickness;
	}

	/* do initial decimation */
	old_face_cnt = bot->num_faces;
	edges_removed = rt_bot_decimate_gct(bot, fs);
	if (edges_removed >= 0) {
	    not_solid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
	    bu_log("%s: %d edges removed, %zu faces removed, valid: %d\n", bot_dp->d_namep, edges_removed, old_face_cnt - bot->num_faces, !not_solid);
	} else {
	    bu_log("%s: decimation failure\n", bot_dp->d_namep);
	}
	if (not_solid) {
	    int scnt = 2;
	    fs = 2*fs;
	    while (not_solid && fs < bbox_diag*0.1) {
		old_face_cnt = bot->num_faces;
		edges_removed = rt_bot_decimate_gct(bot, fs);
		if (edges_removed >= 0) {
		    not_solid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
		    bu_log("%s(%d): %d edges removed, %zu faces removed, valid: %d\n", bot_dp->d_namep, scnt, edges_removed, old_face_cnt - bot->num_faces, !not_solid);
		} else {
		    bu_log("%s(%d): decimation failure\n", bot_dp->d_namep, scnt);
		}
		scnt++;
		fs = 2*fs;
	    }
	}

	if (edges_removed >= 0) {
	    if (not_solid) {
		bu_log("Unable to create a valid version of %s via decimation\n", bot_dp->d_namep);
	    } else {
		struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
		if (wdb_put_internal(wdbp, bot_dp->d_namep, &intern, 1.0) < 0) {
		    bu_log("Failed to write decimated version of %s back to database\n", bot_dp->d_namep);
		}
	    }
	}
    }


comb_decimate_memfree:
    bu_ptbl_free(bot_dps);
    bu_free(bot_dps, "bot directory pointer table");

    return ret;
}

enum comb_command {
    COMB_COMMAND_NONE = 0,
    COMB_COMMAND_RM,
    COMB_COMMAND_WRAP,
    COMB_COMMAND_FLATTEN,
    COMB_COMMAND_LIFT,
    COMB_COMMAND_REGION,
    COMB_COMMAND_UNREGION,
    COMB_COMMAND_DECIMATE
};

int ged_comb_core(struct ged *gedp, int argc, const char *argv[]);

static enum comb_command
comb_command_id(const char *command)
{
    if (BU_STR_EQUAL(command, "rm")) return COMB_COMMAND_RM;
    if (BU_STR_EQUAL(command, "wrap")) return COMB_COMMAND_WRAP;
    if (BU_STR_EQUAL(command, "flatten")) return COMB_COMMAND_FLATTEN;
    if (BU_STR_EQUAL(command, "lift")) return COMB_COMMAND_LIFT;
    if (BU_STR_EQUAL(command, "region")) return COMB_COMMAND_REGION;
    if (BU_STR_EQUAL(command, "unregion")) return COMB_COMMAND_UNREGION;
    if (BU_STR_EQUAL(command, "decimate")) return COMB_COMMAND_DECIMATE;
    return COMB_COMMAND_NONE;
}

static const char *
comb_command_name(enum comb_command command)
{
    switch (command) {
	case COMB_COMMAND_RM: return "rm";
	case COMB_COMMAND_WRAP: return "wrap";
	case COMB_COMMAND_FLATTEN: return "flatten";
	case COMB_COMMAND_LIFT: return "lift";
	case COMB_COMMAND_REGION: return "region";
	case COMB_COMMAND_UNREGION: return "unregion";
	case COMB_COMMAND_DECIMATE: return "decimate";
	case COMB_COMMAND_NONE: break;
    }
    return "unknown";
}

static int
comb_remove_members(struct ged *gedp, struct directory *dp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int ret = BRLCAD_OK;

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    for (int i = 0; i < argc; i++) {
	if (db_tree_rm_dbleaf(&(comb->tree), argv[i], 0) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Failure deleting %s/%s\n", dp->d_namep, argv[i]);
	    ret = BRLCAD_ERROR;
	} else {
	    struct bu_vls path = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&path, "%s/%s", dp->d_namep, argv[i]);
	    _dl_eraseAllPathsFromDisplay(gedp, bu_vls_addr(&path), 0);
	    bu_vls_free(&path);
	    bu_vls_printf(gedp->ged_result_str, "deleted %s/%s\n", dp->d_namep, argv[i]);
	}
    }
    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }
    return ret;
}

static int
comb_execute_command(struct ged *gedp, enum comb_command command,
	const char *comb_name, int argc, const char *argv[])
{
    struct directory *dp;
    int ret;

    if (!comb_name) {
	bu_vls_printf(gedp->ged_result_str, "comb: no combination specified\n");
	return BRLCAD_ERROR;
    }
    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL || !(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s is not a combination", comb_name);
	return BRLCAD_ERROR;
    }
    if (command == COMB_COMMAND_RM) {
	if (argc < 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: comb combination rm member(s)");
	    return BRLCAD_ERROR;
	}
	GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
	ret = comb_remove_members(gedp, dp, argc, argv);
	db_update_nref(gedp->dbip);
	return ret;
    }
    if (argc != 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: comb combination %s", comb_command_name(command));
	return BRLCAD_ERROR;
    }
    if (command == COMB_COMMAND_REGION)
	return region_flag_set(gedp, dp);
    if (command == COMB_COMMAND_UNREGION)
	return region_flag_clear(gedp, dp);
    db_update_nref(gedp->dbip);
    switch (command) {
	case COMB_COMMAND_WRAP: ret = comb_wrap(gedp, dp); break;
	case COMB_COMMAND_FLATTEN: ret = comb_flatten(gedp, dp); break;
	case COMB_COMMAND_LIFT: ret = comb_lift_region(gedp, dp); break;
	case COMB_COMMAND_DECIMATE: ret = comb_decimate(gedp, dp); break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "comb: invalid command\n");
	    return BRLCAD_ERROR;
    }
    if (ret == BRLCAD_OK)
	db_update_nref(gedp->dbip);
    return ret;
}

struct comb_tree_info {
    struct ged *gedp;
    const char *comb_name;
};

static int
comb_tree_command(void *bs, enum comb_command command, int argc, const char **argv)
{
    struct comb_tree_info *info = (struct comb_tree_info *)bs;

    if (argc == 2 && (BU_STR_EQUAL(argv[1], HELPFLAG) ||
	BU_STR_EQUAL(argv[1], PURPOSEFLAG))) {
	bu_vls_printf(info->gedp->ged_result_str,
	    "comb %s %s\n", info->comb_name, comb_command_name(command));
	return BRLCAD_OK;
    }
    return comb_execute_command(info->gedp, command, info->comb_name,
	argc - 1, argv + 1);
}

#define COMB_COMMAND_WRAPPER(_name, _command) \
    static int _comb_cmd_ ## _name(void *bs, int argc, const char **argv) \
    { return comb_tree_command(bs, _command, argc, argv); }
COMB_COMMAND_WRAPPER(rm, COMB_COMMAND_RM)
COMB_COMMAND_WRAPPER(wrap, COMB_COMMAND_WRAP)
COMB_COMMAND_WRAPPER(flatten, COMB_COMMAND_FLATTEN)
COMB_COMMAND_WRAPPER(lift, COMB_COMMAND_LIFT)
COMB_COMMAND_WRAPPER(region, COMB_COMMAND_REGION)
COMB_COMMAND_WRAPPER(unregion, COMB_COMMAND_UNREGION)
COMB_COMMAND_WRAPPER(decimate, COMB_COMMAND_DECIMATE)
#undef COMB_COMMAND_WRAPPER

const struct bu_cmdtab _comb_cmds[] = {
    {"rm", _comb_cmd_rm},
    {"wrap", _comb_cmd_wrap},
    {"flatten", _comb_cmd_flatten},
    {"lift", _comb_cmd_lift},
    {"region", _comb_cmd_region},
    {"unregion", _comb_cmd_unregion},
    {"decimate", _comb_cmd_decimate},
    {(char *)NULL, NULL}
};

static int
comb_tree_execute(void *bs, int argc, const char **argv)
{
    int ret = BRLCAD_ERROR;

    if (bu_cmd(_comb_cmds, argc, argv, 0, bs, &ret) == BRLCAD_OK)
	return ret;
    return BRLCAD_ERROR;
}

static int
comb_execute_selector_first_operator(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    const int legacy_argc = argc - 1;
    const char **legacy_argv = (const char **)bu_calloc((size_t)legacy_argc,
	 sizeof(*legacy_argv), "comb legacy argv");
    legacy_argv[0] = argv[0];
    legacy_argv[1] = argv[3];
    legacy_argv[2] = argv[1];
    for (int i = 4; i < argc; i++) legacy_argv[i - 1] = argv[i];
    ret = ged_comb_core(gedp, legacy_argc, legacy_argv);
    bu_free((void *)legacy_argv, "comb legacy argv");
    return ret;
}

int
ged_comb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    char *comb_name;
    int i;
    int operand_index;
    db_op_t oper;
    struct comb_args args = {0, 0, 0, 0, 0, 0, 0};
    static const char *usage = "[-c|-r] [-w|-f|-l] [-d] [-S] comb_name [<operation object>]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc >= 3 && (BU_STR_EQUAL(argv[2], "-C") || BU_STR_EQUAL(argv[2], "--comb"))) {
	enum comb_command command = comb_command_id(argv[1]);
	if (command != COMB_COMMAND_NONE) {
	    if (argc < 4) return BRLCAD_ERROR;
	    return comb_execute_command(gedp, command, argv[3], argc - 4, argv + 4);
	}
	if (db_str2op(argv[1]) != DB_OP_NULL && argc >= 4)
	    return comb_execute_selector_first_operator(gedp, argc, argv);
    }
    if (argc >= 3 && argv[1][0] != '-') {
	if (comb_command_id(argv[2]) != COMB_COMMAND_NONE) {
	    return comb_dispatch_modern(gedp, argc, argv);
	}
    }

    /* must be wanting help */
    if (argc == 1) {
	comb_tree_show_help(gedp);
	bu_vls_printf(gedp->ged_result_str, "\nLegacy syntax: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(comb_schema(), &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;


    /* Get target combination info */
    comb_name = (char *)argv[0];
    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s is not a combination", comb_name);
	    return BRLCAD_ERROR;
	}
	if (args.require_new && !args.do_decimation) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s already exists.", comb_name);
	    return BRLCAD_ERROR;
	}
    }

    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip);

    /* Do decimation, if that's enabled */

    if (args.do_decimation) {
	if (dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s is not an existing combination", comb_name);
	    return BRLCAD_ERROR;
	}
	return comb_decimate(gedp, dp);
    }

    /* If we aren't performing one of the option operations,
     * proceed with the standard comb build */
    if (!args.wrap && !args.flatten && !args.lift_region) {

	/* Now, we're ready to process operation/object pairs, if any */
	/* Check for odd number of arguments */
	if ((argc - 1) & 01) {
	    bu_vls_printf(gedp->ged_result_str, "error in number of args!");
	    return BRLCAD_ERROR;
	}

	/* Get operation and solid name for each solid */
	for (i = 1; i < argc; i += 2) {
	    /* they come in pairs */
	    if (i+1 >= argc) {
		bu_vls_printf(gedp->ged_result_str, "Invalid syntax near '%s', ignored.  Expecting object name after operator.\n", argv[i]);
		return BRLCAD_ERROR;
	    }

	    oper = db_str2op(argv[i]);
	    if (oper == DB_OP_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Unknown operator '%c' (0x%x) encountered, invalid syntax.\n", argv[i][0], argv[i][0]);
		continue;
	    }

	    /* object name comes after op */
	    if ((dp = db_lookup(gedp->dbip,  argv[i+1], LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Object '%s does not exist.\n", argv[i+1]);
		continue;
	    }

	    /* add it to the comb immediately */
	    if (_ged_combadd(gedp, dp, comb_name, 0, oper, 0, 0) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "Error adding '%s' (with op '%c') to '%s'\n", dp->d_namep, (char)oper, comb_name);
		return BRLCAD_ERROR;
	    }
	}
    }

    /* Handle the -w option for "wrapping" the contents of the comb */
    if (args.wrap) {
	if (!dp || dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Combination '%s does not exist.\n", comb_name);
	    return BRLCAD_ERROR;
	}
	if (comb_wrap(gedp, dp) == BRLCAD_ERROR) {
	    return BRLCAD_ERROR;
	} else {
	    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: wrap of %s failed", comb_name);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (args.flatten) {
	if (!dp || dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Combination '%s does not exist.\n", comb_name);
	    return BRLCAD_ERROR;
	}
	if (comb_flatten(gedp, dp) == BRLCAD_ERROR) {
	    return BRLCAD_ERROR;
	} else {
	    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: flattening of %s failed", comb_name);
		return BRLCAD_ERROR;
	    }
	}
    }

    if (args.lift_region) {
	if (!dp || dp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Combination '%s does not exist.\n", comb_name);
	    return BRLCAD_ERROR;
	}
	if (comb_lift_region(gedp, dp) == BRLCAD_ERROR) {
	    return BRLCAD_ERROR;
	} else {
	    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "ERROR: region lift to %s failed", comb_name);
		return BRLCAD_ERROR;
	    }
	}
    }


    /* Make sure the region flag is set appropriately */

    if (args.set_comb || args.set_region) {
	if ((dp = db_lookup(gedp->dbip, comb_name, LOOKUP_NOISY)) != RT_DIR_NULL) {
	    if (args.set_region) {
		if (region_flag_set(gedp, dp) == BRLCAD_ERROR)
		    return BRLCAD_ERROR;
	    }
	    if (args.set_comb) {
		if (region_flag_clear(gedp, dp) == BRLCAD_ERROR)
		    return BRLCAD_ERROR;
	    }
	}
    }

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

static const struct bu_cmd_option comb_schema_options[] = {
    BU_CMD_FLAG("c", NULL, struct comb_args, set_comb, "Clear the region flag"),
    BU_CMD_FLAG("d", NULL, struct comb_args, do_decimation, "Decimate the combination's BoT members"),
    BU_CMD_FLAG("f", NULL, struct comb_args, flatten, "Flatten a union-only combination"),
    BU_CMD_FLAG("l", NULL, struct comb_args, lift_region, "Lift a region flag to the combination"),
    BU_CMD_FLAG("r", NULL, struct comb_args, set_region, "Set the region flag"),
    BU_CMD_FLAG("w", NULL, struct comb_args, wrap, "Wrap the combination contents"),
    BU_CMD_FLAG("S", NULL, struct comb_args, require_new, "Require a new combination name"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand comb_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to create or modify", "ged.db_object"),
    BU_CMD_OPERAND("operation_members", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Repeated boolean-operation/member-object pairs", NULL),
    BU_CMD_OPERAND_NULL
};
static const char * const comb_op_keywords[] = {"u", "-", "+", NULL};

static void
comb_operation_candidates(struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;

    for (size_t i = 0; comb_op_keywords[i]; i++)
	if (!prefix || !prefix[0] || strncmp(comb_op_keywords[i], prefix, strlen(prefix)) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "comb operation candidates");
    for (size_t i = 0, oi = 0; comb_op_keywords[i]; i++)
	if (!prefix || !prefix[0] || strncmp(comb_op_keywords[i], prefix, strlen(prefix)) == 0)
	    result->completion_candidates[oi++] = bu_strdup(comb_op_keywords[i]);
    result->completion_count = count;
}

static int
comb_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}


static int
comb_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    size_t operands = 0;
    size_t previous_operands = 0;
    int set_comb = 0;
    int set_region = 0;
    int wrap = 0;
    int flatten = 0;
    int lift_region = 0;
    int decimate = 0;
    int require_new = 0;
    int ret = 0;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;

    set_comb = bu_cmd_schema_option_present(schema, argc, argv, "c");
    set_region = bu_cmd_schema_option_present(schema, argc, argv, "r");
    wrap = bu_cmd_schema_option_present(schema, argc, argv, "w");
    flatten = bu_cmd_schema_option_present(schema, argc, argv, "f");
    lift_region = bu_cmd_schema_option_present(schema, argc, argv, "l");
    decimate = bu_cmd_schema_option_present(schema, argc, argv, "d");
    require_new = bu_cmd_schema_option_present(schema, argc, argv, "S");
    operands = bu_cmd_schema_operand_count(schema, argc, argv);

    if (decimate && (set_comb || set_region || wrap || flatten || lift_region || require_new)) {
	comb_validation_result(result, BU_CMD_VALIDATE_INVALID,
	    cursor_arg < argc ? cursor_arg : argc, BU_CMD_VALUE_STRING,
	    "-d cannot be combined with another comb action", NULL);
	return 0;
    }

    if (cursor_arg < argc && argv[cursor_arg] && argv[cursor_arg][0] == '-' &&
	argv[cursor_arg][1] && (result->expected & BU_CMD_EXPECT_OPTION))
	return 0;
    if (!operands || wrap || flatten || lift_region || decimate)
	return 0;

    /* Every accepted prefix must already contain complete, valid operation
     * tokens.  The cursor token is handled below so a user may still edit an
     * operation prefix without the preceding pair being misclassified. */
    for (size_t i = 0; i < cursor_arg; i++) {
	size_t before = bu_cmd_schema_operand_count(schema, i, argv);
	size_t after = bu_cmd_schema_operand_count(schema, i + 1, argv);
	if (after == before || before == 0 || (before - 1) % 2)
	    continue;
	if (db_str2op(argv[i]) == DB_OP_NULL) {
	    comb_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_KEYWORD, "invalid boolean operation", NULL);
	    return 0;
	}
    }

    if (operands == 1) {
	if (!set_comb && !set_region && !require_new && cursor_arg >= argc) {
	    comb_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		BU_CMD_VALUE_KEYWORD, "boolean operation expected", NULL);
	    comb_operation_candidates(result, "");
	}
	return 0;
    }

    if (cursor_arg >= argc) {
	if ((operands - 1) % 2) {
	    comb_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc,
		BU_CMD_VALUE_DB_OBJECT, "member object expected", "ged.db_object");
	}
	return 0;
    }

    previous_operands = bu_cmd_schema_operand_count(schema, cursor_arg, argv);
    if (previous_operands < 1)
	return 0;
    if ((previous_operands - 1) % 2 == 0) {
	const char *operation = argv[cursor_arg];
	bu_cmd_validate_state_t state = db_str2op(operation) != DB_OP_NULL ?
	    BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID;
	comb_validation_result(result, state, cursor_arg, BU_CMD_VALUE_KEYWORD,
	    state == BU_CMD_VALIDATE_VALID ? "boolean operation" : "invalid boolean operation", NULL);
	comb_operation_candidates(result, operation);
	return 0;
    }

    comb_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
	BU_CMD_VALUE_DB_OBJECT, "member object", "ged.db_object");
    return 0;
}


static const char * const comb_region_options[] = {"c", "r", NULL};
static const char * const comb_tree_options[] = {"w", "f", "l", NULL};
static const char * const comb_decimate_options[] = {"d", NULL};
static const struct bu_cmd_constraint comb_schema_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(comb_region_options, 0, 1, "-c and -r are mutually exclusive"),
    BU_CMD_CONSTRAINT_OPTIONS(comb_tree_options, 0, 1, "-w, -f, and -l are mutually exclusive"),
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT, comb_tree_options, 1, 1,
	"tree-action options accept only a combination name"),
    BU_CMD_CONSTRAINT_OPERANDS(BU_CMD_CONDITION_ANY_OPTION_PRESENT, comb_decimate_options, 1, 1,
	"-d accepts only an existing combination name"),
    BU_CMD_CONSTRAINT_NULL
};
static const struct bu_cmd_schema comb_cmd_schema = {
    "comb", "Create or modify combinations", comb_schema_options,
    comb_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(comb_schema_validate, comb_schema_constraints)
};

static const struct bu_cmd_option comb_tree_root_options[] = {
    BU_CMD_FLAG_UNBOUND("h", "help", "h", "Print command help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand comb_tree_root_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to operate on", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema comb_tree_root_schema = {
    "comb", "Operate on a combination", comb_tree_root_options,
    comb_tree_root_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand comb_tree_rm_operands[] = {
	BU_CMD_OPERAND("member", BU_CMD_VALUE_RAW, 1,
	BU_CMD_COUNT_UNLIMITED, "Command arguments", NULL),
	BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema comb_tree_rm_schema = {
    "rm", "Remove members from a combination", NULL,
	comb_tree_rm_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
	BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
#define COMB_TREE_RAW_SCHEMA(_id, _name, _help) \
    static const struct bu_cmd_schema _id = { \
	_name, _help, NULL, NULL, \
	BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL) \
    }
COMB_TREE_RAW_SCHEMA(comb_tree_wrap_schema, "wrap", "Wrap a combination");
COMB_TREE_RAW_SCHEMA(comb_tree_flatten_schema, "flatten", "Flatten a combination");
COMB_TREE_RAW_SCHEMA(comb_tree_lift_schema, "lift", "Lift a region to a combination");
COMB_TREE_RAW_SCHEMA(comb_tree_region_schema, "region", "Set the region flag");
COMB_TREE_RAW_SCHEMA(comb_tree_unregion_schema, "unregion", "Clear the region flag");
COMB_TREE_RAW_SCHEMA(comb_tree_decimate_schema, "decimate", "Decimate BoT members");
#undef COMB_TREE_RAW_SCHEMA

static const struct bu_cmd_tree_node comb_tree_subcommands[] = {
    BU_CMD_TREE_NODE(&comb_tree_rm_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_wrap_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_flatten_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_lift_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_region_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_unregion_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE(&comb_tree_decimate_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, comb_tree_execute),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree comb_tree = {
    &comb_tree_root_schema, comb_tree_subcommands,
    BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS
};

static void
comb_tree_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&comb_tree);
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "comb native tree help");
    }
}

static int
comb_dispatch_modern(struct ged *gedp, int argc, const char **argv)
{
    struct comb_tree_info info = {gedp, NULL};
    int operand_index;
    int child_index;
    int root_data = 0;
    int tree_ret = BRLCAD_ERROR;

	operand_index = bu_cmd_schema_parse(&comb_tree_root_schema, &root_data,
	gedp->ged_result_str, argc - 1, argv + 1);
	child_index = operand_index + 1;
	if (child_index < 0 || child_index >= argc - 1)
	return BRLCAD_ERROR;
	info.comb_name = argv[1];
	if (bu_cmd_tree_dispatch(&comb_tree, &info, argc - 1 - child_index,
	argv + 1 + child_index, &tree_ret) == BRLCAD_OK)
	return tree_ret;
    return BRLCAD_ERROR;
}

static int
comb_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &comb_tree, input, cursor_pos, result);
}

static int
comb_grammar_analyze(struct ged *gedp, const char *input,
	struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &comb_tree, input, analysis);
}

static char *
comb_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&comb_tree);
}

static int
comb_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&comb_tree, msgs);
}

static const struct ged_cmd_grammar comb_grammar = {
    "comb", "Operate on a combination", comb_grammar_validate,
    comb_grammar_analyze, comb_grammar_json, comb_grammar_lint
};

static const struct bu_cmd_schema *
comb_schema(void)
{
    return &comb_cmd_schema;
}

#define GED_COMB_COMMANDS(X, XID) \
    X(comb, ged_comb_core, GED_CMD_DEFAULT, &comb_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_COMB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_comb", 1, GED_COMB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
