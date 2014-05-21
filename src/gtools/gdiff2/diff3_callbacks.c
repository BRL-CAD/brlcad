/*              G D I F F 3 _ C A L L B A C K S . C
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


/*******************************************************************/
/* Callback functions for db_compare3 */
/*******************************************************************/
int
diff3_added(const struct db_i *left, const struct db_i *ancestor, const struct db_i *right, const struct directory *dp_left, const struct directory *dp_ancestor, const struct directory *dp_right, void *data)
{
    /* The cases are:
     *
     * added in left only
     * added in right only
     * added in left and right, identically
     * added in left and right, mergeably
     * added in left and right, conflict
     */
    struct diff3_results *results;
    if (!data || dp_ancestor)
	return -1;

    results = (struct diff3_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->added_left_only)) BU_PTBL_INIT(results->added_left_only);
    if (!BU_PTBL_IS_INITIALIZED(results->added_right_only)) BU_PTBL_INIT(results->added_right_only);
    if (!BU_PTBL_IS_INITIALIZED(results->added_both)) BU_PTBL_INIT(results->added_both);
    if (!BU_PTBL_IS_INITIALIZED(results->changed)) BU_PTBL_INIT(results->changed);
    if (!BU_PTBL_IS_INITIALIZED(results->conflict)) BU_PTBL_INIT(results->conflict);

    if (dp_right == RT_DIR_NULL && dp_left == RT_DIR_NULL) return -1;
    if (dp_left == RT_DIR_NULL) {
	bu_ptbl_ins(results->added_right_only, (long *)dp_right);
    }
    if (dp_right == RT_DIR_NULL) {
	bu_ptbl_ins(results->added_left_only, (long *)dp_left);
    }

    if ((dp_right != RT_DIR_NULL) && (dp_left != RT_DIR_NULL)) {
	struct rt_db_internal intern_left;
	struct rt_db_internal intern_right;
	struct bn_tol diff_tol = BN_TOL_INIT_ZERO;
	struct diff3_result_container *result3 = (struct diff3_result_container *)bu_calloc(1, sizeof(struct diff3_result_container), "diff3 result struct");
	diff3_result_init(result3);
	RT_DB_INTERNAL_INIT(&intern_left);
	RT_DB_INTERNAL_INIT(&intern_right);
	diff_tol.dist = results->diff_tolerance;

	result3->ancestor_dbip = ancestor;
	result3->left_dbip = left;
	result3->right_dbip = right;
	result3->ancestor_dp = dp_ancestor;
	result3->left_dp = dp_left;
	result3->right_dp = dp_right;

	/* Get the internal objects */
	if (rt_db_get_internal(&intern_left, dp_left, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    return -1;
	}
	if (rt_db_get_internal(&intern_right, dp_right, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    rt_db_free_internal(&intern_left);
	    return -1;
	}

	result3->internal_diff = db_compare3(&(result3->param_unchanged),
		&(result3->param_removed_left_only), &(result3->param_removed_right_only), &(result3->param_removed_both),
		&(result3->param_added_left_only), &(result3->param_added_right_only), &(result3->param_added_both),
		&(result3->param_added_conflict_left), &(result3->param_added_conflict_right),
		&(result3->param_changed_left_only), &(result3->param_changed_right_only),
		&(result3->param_changed_both), &(result3->param_changed_conflict_ancestor),
		&(result3->param_changed_conflict_left),&(result3->param_changed_conflict_right),
		&(result3->param_merged), &intern_left, NULL, &intern_right, DB_COMPARE_PARAM, &diff_tol);

	result3->attribute_diff = db_compare3(&(result3->attribute_unchanged),
		&(result3->attribute_removed_left_only), &(result3->attribute_removed_right_only), &(result3->attribute_removed_both),
		&(result3->attribute_added_left_only), &(result3->attribute_added_right_only), &(result3->attribute_added_both),
		&(result3->attribute_added_conflict_left), &(result3->attribute_added_conflict_right),
		&(result3->attribute_changed_left_only), &(result3->attribute_changed_right_only),
		&(result3->attribute_changed_both), &(result3->attribute_changed_conflict_ancestor),
		&(result3->attribute_changed_conflict_left),&(result3->attribute_changed_conflict_right),
		&(result3->attribute_merged), &intern_left, NULL, &intern_right, DB_COMPARE_ATTRS, &diff_tol);

	result3->status = (result3->internal_diff > result3->attribute_diff) ? result3->internal_diff : result3->attribute_diff;

	switch (result3->status) {
	    case 0:
		/* dp_left == dp_right */
		bu_ptbl_ins(results->added_both, (long *)dp_left);
		diff3_result_free(result3);
		break;
	    case 1:
		/* dp_left != dp_right, but they can be merged */
		bu_ptbl_ins(results->added_merged, (long *)result3);
		break;
	    case 2:
	    case 3:
		/* dp_left != dp_right */
		bu_ptbl_ins(results->conflict, (long *)result3);
		break;
	    default:
		return -1;
		break;
	}
    }

    return 0;
}


int
diff3_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *UNUSED(dp_ancestor), const struct directory *UNUSED(dp_right), void *UNUSED(data))
{
    /* The cases are:
     *
     * removed in left only
     * removed in right only
     * removed in both
     *
     * Cases where a change conflicts with a removal are handled by diff3_changed
     */
    return 0;
}


int
diff3_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *dp_ancestor, const struct directory *UNUSED(dp_right), void *data)
{
    struct diff3_results *results;

    if (!data || !dp_ancestor)
	return -1;

    results = (struct diff3_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);

    bu_ptbl_ins(results->unchanged, (long *)dp_ancestor);

    return 0;
}

int
diff3_changed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(ancestor), const struct db_i *UNUSED(right), const struct directory *UNUSED(dp_left), const struct directory *UNUSED(dp_ancestor), const struct directory *UNUSED(dp_right), void *UNUSED(data))
{
    /* The cases are:
     *
     * OK:
     * changed in left, right unchanged
     * changed in right, left unchanged
     * changed in left and right, identically
     * changed in left and right, mergeably
     *
     *
     * Conflicts:
     * removed in left, changed in right
     * removed in right, changed in left
     * changed in left, changed in right, conflict
     *
     *
     * Cases where additions conflict with each other are handled by diff3_added
     */

    return 0;
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
