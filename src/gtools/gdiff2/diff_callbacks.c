/*                D I F F _ C A L L B A C K S . C
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
/* Callback functions for db_compare */
/*******************************************************************/
int
diff_added(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *added, void *data)
{
    struct diff_results *results;

    if (!data || !added)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->added)) BU_PTBL_INIT(results->added);

    bu_ptbl_ins(results->added, (long *)added);

    return 0;
}


int
diff_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *removed, void *data)
{
    struct diff_results *results;

    if (!data || !removed)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->removed)) BU_PTBL_INIT(results->removed);

    bu_ptbl_ins(results->removed, (long *)removed);

    return 0;
}


int
diff_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *unchanged, void *data)
{
    struct diff_results *results;

    if (!data || !unchanged)
	return -1;

    results = (struct diff_results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);

    bu_ptbl_ins(results->unchanged, (long *)unchanged);

    return 0;
}

int
diff_changed(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data)
{
    struct diff_results *results;
    struct diff_result_container *result;
    struct bn_tol diff_tol = BN_TOL_INIT_ZERO;
    struct rt_db_internal intern_orig;
    struct rt_db_internal intern_new;

    if (!left || !right || !before || !after|| !data)
	return -1;

    results = (struct diff_results *)data;
    diff_tol.dist = results->diff_tolerance;

    if (!BU_PTBL_IS_INITIALIZED(results->changed))
	BU_PTBL_INIT(results->changed);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_ancestor_dbip))
	BU_PTBL_INIT(results->changed_ancestor_dbip);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_new_dbip_1))
	BU_PTBL_INIT(results->changed_new_dbip_1);

    bu_ptbl_ins(results->changed_ancestor_dbip, (long *)before);
    bu_ptbl_ins(results->changed_new_dbip_1, (long *)after);

    result = (struct diff_result_container *)bu_calloc(1, sizeof(struct diff_result_container), "diff result struct");
    diff_result_init(result);

    result->dp_orig = before;
    result->dp_new = after;

    if (before->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY || after->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	if (!(before->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && after->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY)) {
	    RT_DB_INTERNAL_INIT(&intern_orig);
	    RT_DB_INTERNAL_INIT(&intern_new);
	    if (before->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
		if (rt_db_get_internal(&intern_new, after, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    result->status = 1;
		    return -1;
		}
	    }
	    if (after->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
		if (rt_db_get_internal(&intern_orig, before, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    result->status = 1;
		    return -1;
		}
	    }
	} else {
	    int attr_diff_status = attr_obj_diff(result, left, right, before, after, &diff_tol);
	    if (attr_diff_status < 0) {return -1;}
	    if (result->attribute_diff) {
		bu_ptbl_ins(results->changed, (long *)result);
	    } else {
		if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);
		bu_ptbl_ins(results->unchanged, (long *)before);
		diff_result_free(result);
	    }
	    return 0;
	}
    } else {
	RT_DB_INTERNAL_INIT(&intern_orig);
	RT_DB_INTERNAL_INIT(&intern_new);

	/* Get the internal objects */
	if (rt_db_get_internal(&intern_orig, before, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    result->status = 1;
	    return -1;
	}
	if (rt_db_get_internal(&intern_new, after, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    rt_db_free_internal(&intern_orig);
	    result->status = 1;
	    return -1;
	}
    }

    result->internal_diff = db_compare(&(result->internal_new_only), &(result->internal_orig_only),
	    &(result->internal_orig_diff), &(result->internal_new_diff), &(result->internal_shared),
	    &intern_orig, &intern_new, DB_COMPARE_PARAM, &diff_tol);

    result->attribute_diff = db_compare(&(result->additional_new_only), &(result->additional_orig_only),
	    &(result->additional_orig_diff), &(result->additional_new_diff), &(result->additional_shared),
	    &intern_orig, &intern_new, DB_COMPARE_ATTRS, &diff_tol);

    if (result->internal_diff || result->attribute_diff) {
	bu_ptbl_ins(results->changed, (long *)result);
    } else {
	if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);
	bu_ptbl_ins(results->unchanged, (long *)before);
	diff_result_free(result);
    }

    rt_db_free_internal(&intern_orig);
    rt_db_free_internal(&intern_new);

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
