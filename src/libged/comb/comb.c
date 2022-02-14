/*                  C O M B . C
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
/** @file libged/comb.c
 *
 * The comb command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/sort.h"
#include "bg/trimesh.h"
#include "wdb.h"
#include "analyze.h"

#include "../ged_private.h"

HIDDEN int
region_flag_set(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    db5_standardize_avs(&avs);
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

HIDDEN int
region_flag_clear(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    db5_standardize_avs(&avs);
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

HIDDEN int
color_shader_clear(struct ged *gedp, struct directory *dp) {
    struct bu_attribute_value_set avs;
    bu_avs_init_empty(&avs);
    if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    db5_standardize_avs(&avs);
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

HIDDEN int
comb_tree_clear(struct ged *gedp, struct directory *dp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    /* Clear the tree from the original object */
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, BRLCAD_ERROR);
    RT_CK_DB_INTERNAL(&intern);
    comb = (struct rt_comb_internal *)(&intern)->idb_ptr;
    RT_CK_COMB(comb);
    db_free_tree(comb->tree, &rt_uniresource);
    comb->tree = TREE_NULL;
    db5_sync_comb_to_attr(&((&intern)->idb_avs), comb);
    db5_standardize_avs(&((&intern)->idb_avs));
    if (wdb_put_internal(gedp->ged_wdbp, dp->d_namep, &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "wdb_export(%s) failure", dp->d_namep);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


HIDDEN int
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
    if (wdb_export_external(gedp->ged_wdbp, &external, bu_vls_addr(&comb_child_name), dp->d_flags, dp->d_minor_type) < 0) {
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
HIDDEN int
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
    result_cnt = db_search(NULL, DB_SEARCH_TREE, only_unions_in_tree_plan, 1, &dp, gedp->dbip, NULL);
    if (result_cnt) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s tree contains non-union booleans", dp->d_namep);
	return BRLCAD_ERROR;
    }

    /* Find the solids and combs in the tree */
    (void)db_search(&solids, DB_SEARCH_RETURN_UNIQ_DP, solids_in_tree_plan, 1, &dp, gedp->dbip, NULL);
    (void)db_search(&combs, DB_SEARCH_RETURN_UNIQ_DP, combs_in_tree_plan, 1, &dp, gedp->dbip, NULL);

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
    (void)db_search(&combs_outside_of_tree, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_addr(&plan_string), obj_cnt, all_paths, gedp->dbip, NULL);
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
HIDDEN int
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
    (void)db_search(&regions, DB_SEARCH_TREE, regions_in_tree_plan, 1, &dp, gedp->dbip, NULL);

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
    (void)db_search(&combs_outside_of_tree, DB_SEARCH_RETURN_UNIQ_DP, bu_vls_addr(&plan_string), obj_cnt, all_paths, gedp->dbip, NULL);
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

	(void)db_search(&combs_in_tree, DB_SEARCH_RETURN_UNIQ_DP, combs_in_tree_plan, 1, &dp, gedp->dbip, NULL);
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

HIDDEN int
comb_decimate(struct ged *gedp, struct directory *dp)
{
    unsigned int i;
    int ret = BRLCAD_OK;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    struct db_i *dbip = gedp->dbip;

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, 1, &dp, gedp->dbip, NULL) < 0) {
	bu_log("Problem searching for BoTs - aborting.\n");
	ret = BRLCAD_ERROR;
	goto comb_decimate_memfree;
    }

    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct bn_tol btol = BG_TOL_INIT;
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
	GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	ged_get_obj_bounds(gedp, 1, (const char **)&bot_dp->d_namep, 0, obj_min, obj_max);
	bbox_diag = DIST_PNT_PNT(obj_min, obj_max);

	/* Get avg thickness from raytracer */
	if (analyze_obj_to_pnts(NULL, &avg_thickness, dbip, bot_dp->d_namep, &btol, ANALYZE_OBJ_TO_PNTS_RAND, 100000, 5, 0)) {
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
		if (wdb_put_internal(gedp->ged_wdbp, bot_dp->d_namep, &intern, 1.0) < 0) {
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

int
ged_comb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const char *cmd_name;
    char *comb_name;
    int i,c, sum;
    db_op_t oper;
    int do_decimation = 0;
    int set_region = 0;
    int set_comb = 0;
    int standard_comb_build = 1;
    int wrap_comb = 0;
    int flatten_comb = 0;
    int lift_region_comb = 0;
    int alter_existing = 1;
    static const char *usage = "[-c|-r] [-w|-f|-l] [-d] [-S] comb_name [<operation object>]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd_name = argv[0];

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* First, handle options, if any */

    bu_optind = 1;
    /* Grab any arguments off of the argv list */
    while ((c = bu_getopt(argc, (char **)argv, "cdflrswFS")) != -1) {
	switch (c) {
	    case 'c' :
		set_comb = 1;
		break;
	    case 'd' :
		do_decimation = 1;
		break;
	    case 'r' :
		set_region = 1;
		break;
	    case 'w' :
		wrap_comb = 1;
		standard_comb_build = 0;
		break;
	    case 'f' :
		flatten_comb = 1;
		standard_comb_build = 0;
		break;
	    case 'l' :
		lift_region_comb = 1;
		standard_comb_build = 0;
		break;
	    case 'S' :
		alter_existing = 0;
		break;
	    default :
		break;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;

    if (set_comb && set_region) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], cmd_name);
	return BRLCAD_ERROR;
    }

    sum = wrap_comb + flatten_comb + lift_region_comb;
    if (sum > 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if ((wrap_comb || flatten_comb || lift_region_comb) && argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    /* Get target combination info */
    comb_name = (char *)argv[1];
    dp = db_lookup(gedp->dbip, comb_name, LOOKUP_QUIET);
    if (dp != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s is not a combination", comb_name);
	    return BRLCAD_ERROR;
	}
	if (!alter_existing && !do_decimation) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s already exists.", comb_name);
	    return BRLCAD_ERROR;
	}
    }

    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    /* Do decimation, if that's enabled */
    if (do_decimation) {
	return comb_decimate(gedp, dp);
    }

    /* If we aren't performing one of the option operations,
     * proceed with the standard comb build */
    if (standard_comb_build) {

	/* Now, we're ready to process operation/object pairs, if any */
	/* Check for odd number of arguments */
	if (argc & 01) {
	    bu_vls_printf(gedp->ged_result_str, "error in number of args!");
	    return BRLCAD_ERROR;
	}

	/* Get operation and solid name for each solid */
	for (i = 2; i < argc; i += 2) {
	    /* they come in pairs */
	    if (i+1 >= argc) {
		bu_vls_printf(gedp->ged_result_str, "Invalid syntax near '%s', ignored.  Expecting object name after operator.\n", argv[i+1]);
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
    if (wrap_comb) {
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

    if (flatten_comb) {
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

    if (lift_region_comb) {
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
    if (set_comb || set_region) {
	if ((dp = db_lookup(gedp->dbip, comb_name, LOOKUP_NOISY)) != RT_DIR_NULL) {
	    if (set_region) {
		if (region_flag_set(gedp, dp) == BRLCAD_ERROR)
		    return BRLCAD_ERROR;
	    }
	    if (set_comb) {
		if (region_flag_clear(gedp, dp) == BRLCAD_ERROR)
		    return BRLCAD_ERROR;
	    }
	}
    }

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl comb_cmd_impl = {"comb", ged_comb_core, GED_CMD_DEFAULT};

const struct ged_cmd comb_cmd = { &comb_cmd_impl };
const struct ged_cmd *comb_cmds[] = {
    &comb_cmd,
    NULL
};

static const struct ged_plugin pinfo = { GED_API,  comb_cmds, 1 };

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
