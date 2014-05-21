/*                     C O N T A I N E R S . C
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "./gdiff2.h"

void
diff_state_init(struct diff_state *state)
{
    state->return_added = -1;
    state->return_removed = -1;
    state->return_changed = -1;
    state->return_conflicts = 1;
    state->return_unchanged = 0;
    state->have_search_filter = 0;
    state->verbosity = 2;
    state->output_mode = 0;
    state->diff_tolerance = RT_LEN_TOL;
    BU_GET(state->diff_log, struct bu_vls);
    BU_GET(state->search_filter, struct bu_vls);
    bu_vls_init(state->diff_log);
    bu_vls_init(state->search_filter);
}

void
diff_state_free(struct diff_state *state)
{
    bu_vls_free(state->diff_log);
    bu_vls_free(state->search_filter);
    BU_PUT(state->diff_log, struct bu_vls);
    BU_PUT(state->search_filter, struct bu_vls);
}

void
diff_result_init(struct diff_result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->dbip_orig = DBI_NULL;
    result->dbip_new = DBI_NULL;
    result->dp_orig = RT_DIR_NULL;
    result->dp_new = RT_DIR_NULL;
    BU_AVS_INIT(&result->internal_shared);
    BU_AVS_INIT(&result->internal_orig_only);
    BU_AVS_INIT(&result->internal_new_only);
    BU_AVS_INIT(&result->internal_orig_diff);
    BU_AVS_INIT(&result->internal_new_diff);
    BU_AVS_INIT(&result->additional_shared);
    BU_AVS_INIT(&result->additional_orig_only);
    BU_AVS_INIT(&result->additional_new_only);
    BU_AVS_INIT(&result->additional_orig_diff);
    BU_AVS_INIT(&result->additional_new_diff);
}


void
diff_result_free(struct diff_result_container *result)
{
    if (!result)
	return;

    bu_avs_free(&result->internal_shared);
    bu_avs_free(&result->internal_orig_only);
    bu_avs_free(&result->internal_new_only);
    bu_avs_free(&result->internal_orig_diff);
    bu_avs_free(&result->internal_new_diff);
    bu_avs_free(&result->additional_shared);
    bu_avs_free(&result->additional_orig_only);
    bu_avs_free(&result->additional_new_only);
    bu_avs_free(&result->additional_orig_diff);
    bu_avs_free(&result->additional_new_diff);
}


void
diff3_result_init(struct diff3_result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->ancestor_dbip = DBI_NULL;
    result->left_dbip = DBI_NULL;
    result->right_dbip = DBI_NULL;
    result->ancestor_dp = RT_DIR_NULL;
    result->left_dp = RT_DIR_NULL;
    result->right_dp = RT_DIR_NULL;
    BU_AVS_INIT(&result->param_unchanged);
    BU_AVS_INIT(&result->param_removed_left_only);
    BU_AVS_INIT(&result->param_removed_right_only);
    BU_AVS_INIT(&result->param_removed_both);
    BU_AVS_INIT(&result->param_added_left_only);
    BU_AVS_INIT(&result->param_added_right_only);
    BU_AVS_INIT(&result->param_added_both);
    BU_AVS_INIT(&result->param_added_conflict_left);
    BU_AVS_INIT(&result->param_added_conflict_right);
    BU_AVS_INIT(&result->param_changed_left_only);
    BU_AVS_INIT(&result->param_changed_right_only);
    BU_AVS_INIT(&result->param_changed_both);
    BU_AVS_INIT(&result->param_changed_conflict_ancestor);
    BU_AVS_INIT(&result->param_changed_conflict_left);
    BU_AVS_INIT(&result->param_changed_conflict_right);
    BU_AVS_INIT(&result->param_merged);
    BU_AVS_INIT(&result->attribute_unchanged);
    BU_AVS_INIT(&result->attribute_removed_left_only);
    BU_AVS_INIT(&result->attribute_removed_right_only);
    BU_AVS_INIT(&result->attribute_removed_both);
    BU_AVS_INIT(&result->attribute_added_left_only);
    BU_AVS_INIT(&result->attribute_added_right_only);
    BU_AVS_INIT(&result->attribute_added_both);
    BU_AVS_INIT(&result->attribute_added_conflict_left);
    BU_AVS_INIT(&result->attribute_added_conflict_right);
    BU_AVS_INIT(&result->attribute_changed_left_only);
    BU_AVS_INIT(&result->attribute_changed_right_only);
    BU_AVS_INIT(&result->attribute_changed_both);
    BU_AVS_INIT(&result->attribute_changed_conflict_ancestor);
    BU_AVS_INIT(&result->attribute_changed_conflict_left);
    BU_AVS_INIT(&result->attribute_changed_conflict_right);
    BU_AVS_INIT(&result->attribute_merged);

}

void
diff3_result_free(struct diff3_result_container *result)
{
    if (!result)
	return;

    bu_avs_free(&result->param_unchanged);
    bu_avs_free(&result->param_removed_left_only);
    bu_avs_free(&result->param_removed_right_only);
    bu_avs_free(&result->param_removed_both);
    bu_avs_free(&result->param_added_left_only);
    bu_avs_free(&result->param_added_right_only);
    bu_avs_free(&result->param_added_both);
    bu_avs_free(&result->param_added_conflict_left);
    bu_avs_free(&result->param_added_conflict_right);
    bu_avs_free(&result->param_changed_left_only);
    bu_avs_free(&result->param_changed_right_only);
    bu_avs_free(&result->param_changed_both);
    bu_avs_free(&result->param_changed_conflict_ancestor);
    bu_avs_free(&result->param_changed_conflict_left);
    bu_avs_free(&result->param_changed_conflict_right);
    bu_avs_free(&result->param_merged);
    bu_avs_free(&result->attribute_unchanged);
    bu_avs_free(&result->attribute_removed_left_only);
    bu_avs_free(&result->attribute_removed_right_only);
    bu_avs_free(&result->attribute_removed_both);
    bu_avs_free(&result->attribute_added_left_only);
    bu_avs_free(&result->attribute_added_right_only);
    bu_avs_free(&result->attribute_added_both);
    bu_avs_free(&result->attribute_added_conflict_left);
    bu_avs_free(&result->attribute_added_conflict_right);
    bu_avs_free(&result->attribute_changed_left_only);
    bu_avs_free(&result->attribute_changed_right_only);
    bu_avs_free(&result->attribute_changed_both);
    bu_avs_free(&result->attribute_changed_conflict_ancestor);
    bu_avs_free(&result->attribute_changed_conflict_left);
    bu_avs_free(&result->attribute_changed_conflict_right);
    bu_avs_free(&result->attribute_merged);

}


void
diff_results_init(struct diff_results *results)
{

    BU_GET(results->added, struct bu_ptbl);
    BU_GET(results->removed, struct bu_ptbl);
    BU_GET(results->changed, struct bu_ptbl);
    BU_GET(results->changed_ancestor_dbip, struct bu_ptbl);
    BU_GET(results->changed_new_dbip_1, struct bu_ptbl);
    BU_GET(results->unchanged, struct bu_ptbl);

    BU_PTBL_INIT(results->added);
    BU_PTBL_INIT(results->removed);
    BU_PTBL_INIT(results->changed);
    BU_PTBL_INIT(results->changed_ancestor_dbip);
    BU_PTBL_INIT(results->changed_new_dbip_1);
    BU_PTBL_INIT(results->unchanged);

}


void
diff_results_free(struct diff_results *results)
{
    int i = 0;
    bu_ptbl_free(results->added);
    bu_ptbl_free(results->removed);
    bu_ptbl_free(results->unchanged);
    bu_ptbl_free(results->changed_ancestor_dbip);
    bu_ptbl_free(results->changed_new_dbip_1);
    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
	struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(results->changed, i);
	diff_result_free(result);
    }
    bu_ptbl_free(results->changed);

    BU_PUT(results->added, struct bu_ptbl);
    BU_PUT(results->removed, struct bu_ptbl);
    BU_PUT(results->changed, struct bu_ptbl);
    BU_PUT(results->changed_ancestor_dbip, struct bu_ptbl);
    BU_PUT(results->changed_new_dbip_1, struct bu_ptbl);
    BU_PUT(results->unchanged, struct bu_ptbl);

}


void
diff3_results_init(struct diff3_results *results)
{

    results->merged_db = DBI_NULL;
    results->diff_tolerance = RT_LEN_TOL;
    BU_GET(results->unchanged, struct bu_ptbl);
    BU_GET(results->removed_left_only, struct bu_ptbl);
    BU_GET(results->removed_right_only, struct bu_ptbl);
    BU_GET(results->removed_both, struct bu_ptbl);
    BU_GET(results->added_left_only, struct bu_ptbl);
    BU_GET(results->added_right_only, struct bu_ptbl);
    BU_GET(results->added_both, struct bu_ptbl);
    BU_GET(results->added_merged, struct bu_ptbl);
    BU_GET(results->changed, struct bu_ptbl);
    BU_GET(results->conflict, struct bu_ptbl);

    BU_PTBL_INIT(results->unchanged);
    BU_PTBL_INIT(results->removed_left_only);
    BU_PTBL_INIT(results->removed_right_only);
    BU_PTBL_INIT(results->removed_both);
    BU_PTBL_INIT(results->added_left_only);
    BU_PTBL_INIT(results->added_right_only);
    BU_PTBL_INIT(results->added_both);
    BU_PTBL_INIT(results->added_merged);
    BU_PTBL_INIT(results->changed);
    BU_PTBL_INIT(results->conflict);

}

void
diff3_results_free(struct diff3_results *results)
{
    int i = 0;

    bu_ptbl_free(results->unchanged);
    bu_ptbl_free(results->removed_left_only);
    bu_ptbl_free(results->removed_right_only);
    bu_ptbl_free(results->removed_both);
    bu_ptbl_free(results->added_left_only);
    bu_ptbl_free(results->added_right_only);
    bu_ptbl_free(results->added_both);
    for (i = 0; i < (int)BU_PTBL_LEN(results->added_merged); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->added_merged, i);
	diff3_result_free(result);
    }
    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->changed, i);
	diff3_result_free(result);
    }
    bu_ptbl_free(results->changed);
    for (i = 0; i < (int)BU_PTBL_LEN(results->conflict); i++) {
	struct diff3_result_container *result = (struct diff3_result_container *)BU_PTBL_GET(results->conflict, i);
	diff3_result_free(result);
    }
    bu_ptbl_free(results->conflict);

    BU_PUT(results->unchanged, struct bu_ptbl);
    BU_PUT(results->removed_left_only, struct bu_ptbl);
    BU_PUT(results->removed_right_only, struct bu_ptbl);
    BU_PUT(results->removed_both, struct bu_ptbl);
    BU_PUT(results->added_left_only, struct bu_ptbl);
    BU_PUT(results->added_right_only, struct bu_ptbl);
    BU_PUT(results->added_both, struct bu_ptbl);
    BU_PUT(results->changed, struct bu_ptbl);
    BU_PUT(results->conflict, struct bu_ptbl);

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
