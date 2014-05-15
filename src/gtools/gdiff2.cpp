/*                    G D I F F 2 . C P P
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

#include "common.h"

extern "C" {
#include <string.h>

#include "tcl.h"

#include "bu/getopt.h"
#include "raytrace.h"
#include "db_diff.h"
}

/*******************************************************************/
/* Structure and memory management for state management - this holds
 * user supplied options and output logs */
/*******************************************************************/
struct diff_state {
    int return_added;
    int return_removed;
    int return_changed;
    int return_unchanged;
    int return_conflicts;
    int have_search_filter;
    int verbosity;
    int output_mode;
    float diff_tolerance;
    struct bu_vls *diff_log;
    struct bu_vls *search_filter;
};

void diff_state_init(struct diff_state *state) {
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

void diff_state_free(struct diff_state *state) {
    bu_vls_free(state->diff_log);
    bu_vls_free(state->search_filter);
    BU_PUT(state->diff_log, struct bu_vls);
    BU_PUT(state->search_filter, struct bu_vls);
}

/*******************************************************************/
/* Structure and memory management for the container used to hold
 * diff results for pairs of differing objects */
/*******************************************************************/
struct diff_result_container {
    int status;
    struct db_i *dbip_ancestor;
    struct db_i *dbip_orig;
    struct db_i *dbip_new;
    const struct directory *dp_ancestor;
    const struct directory *dp_orig;
    const struct directory *dp_new;
    int internal_diff;
    struct bu_attribute_value_set internal_shared;
    struct bu_attribute_value_set internal_orig_only;
    struct bu_attribute_value_set internal_new_only;
    struct bu_attribute_value_set internal_orig_diff;
    struct bu_attribute_value_set internal_new_diff;
    int attribute_diff;
    struct bu_attribute_value_set additional_shared;
    struct bu_attribute_value_set additional_orig_only;
    struct bu_attribute_value_set additional_new_only;
    struct bu_attribute_value_set additional_orig_diff;
    struct bu_attribute_value_set additional_new_diff;
};


static void
diff_result_init(struct diff_result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->dbip_ancestor = DBI_NULL;
    result->dbip_orig = DBI_NULL;
    result->dbip_new = DBI_NULL;
    result->dp_ancestor = RT_DIR_NULL;
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


static void
diff_result_free(void *result)
{
    struct diff_result_container *curr_result = (struct diff_result_container *)result;

    if (!result)
	return;

    bu_avs_free(&curr_result->internal_shared);
    bu_avs_free(&curr_result->internal_orig_only);
    bu_avs_free(&curr_result->internal_new_only);
    bu_avs_free(&curr_result->internal_orig_diff);
    bu_avs_free(&curr_result->internal_new_diff);
    bu_avs_free(&curr_result->additional_shared);
    bu_avs_free(&curr_result->additional_orig_only);
    bu_avs_free(&curr_result->additional_new_only);
    bu_avs_free(&curr_result->additional_orig_diff);
    bu_avs_free(&curr_result->additional_new_diff);
}


static void
diff_result_free_ptbl(struct bu_ptbl *results_table)
{
    int i = 0;

    if (!results_table)
	return;

    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(results_table, i);
	diff_result_free((void *)result);
    }
    bu_ptbl_free(results_table);
}

/*******************************************************************/
/* Structure and memory management for the container used to hold
 * diff results */
/*******************************************************************/

struct diff_results {
    float diff_tolerance;
    struct bu_ptbl *added;     /* directory pointers */
    struct bu_ptbl *removed;   /* directory pointers */
    struct bu_ptbl *unchanged; /* directory pointers */
    struct bu_ptbl *changed;   /* result containers */
    struct bu_ptbl *changed_ancestor_dbip;   /* directory pointers */
    struct bu_ptbl *changed_new_dbip_1;   /* directory pointers */
};

void diff_results_init(struct diff_results *results){

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


void diff_results_free(struct diff_results *results){

    bu_ptbl_free(results->added);
    bu_ptbl_free(results->removed);
    bu_ptbl_free(results->unchanged);
    bu_ptbl_free(results->changed_ancestor_dbip);
    bu_ptbl_free(results->changed_new_dbip_1);
    diff_result_free_ptbl(results->changed);

    BU_PUT(results->added, struct bu_ptbl);
    BU_PUT(results->removed, struct bu_ptbl);
    BU_PUT(results->changed, struct bu_ptbl);
    BU_PUT(results->changed_ancestor_dbip, struct bu_ptbl);
    BU_PUT(results->changed_new_dbip_1, struct bu_ptbl);
    BU_PUT(results->unchanged, struct bu_ptbl);

}


/*******************************************************************/
/* Structures and memory management for the containers specific to
 * diff3 results */
/*******************************************************************/

struct diff3_result_container {
    struct diff_result_container *ancestor_dbip1;
    struct diff_result_container *ancestor_dbip2;
    struct diff_result_container *dbip1_dbip2;
    int internal_diff;
    int attribute_diff;
    struct bu_attribute_value_set *additional_unchanged;
    struct bu_attribute_value_set *additional_added;
    struct bu_attribute_value_set *additional_added_conflicting_dbip1;
    struct bu_attribute_value_set *additional_added_conflicting_dbip2;
    struct bu_attribute_value_set *additional_removed;
    struct bu_attribute_value_set *additional_changed;
    struct bu_attribute_value_set *additional_changed_conflicting_dbip1;
    struct bu_attribute_value_set *additional_changed_conflicting_dbip2;

};

struct diff3_results {
    float diff_tolerance;

    struct bu_ptbl *added_dbip1;     	/* directory pointers */
    struct bu_ptbl *added_dbip2;     	/* directory pointers */
    struct bu_ptbl *added_both;     	/* directory pointers (dbip1 - note that we are assuming objects are either identical or conflict - if we can merge the two added objects *without* conflict, we need to use a container)*/
    struct bu_ptbl *added_conflicts;    /* containers */

    struct bu_ptbl *removed_dbip1;   	/* directory pointers */
    struct bu_ptbl *removed_dbip2;   	/* directory pointers */
    struct bu_ptbl *removed_both;     	/* directory pointers (dbip1) */

    struct bu_ptbl *changed_ancestor;   /* directory pointers */
    struct bu_ptbl *changed_dbip1_new;	/* directory pointers */
    struct bu_ptbl *changed_dbip2_new;	/* directory pointers */
    struct bu_ptbl *changed_both;	/* directory pointers (dbip1 - note that we are assuming objects are either identical or conflict - if we can merge the two changed objects *without* conflict, we need to use a container) */
    struct bu_ptbl *changed_conflicts;  /* containers */

    struct bu_ptbl *unchanged; 		/* directory pointers (ancestor)*/
};

void diff3_results_init(struct diff3_results *results){

    BU_GET(results->added_dbip1, struct bu_ptbl);
    BU_GET(results->added_dbip2, struct bu_ptbl);
    BU_GET(results->added_both, struct bu_ptbl);
    BU_GET(results->added_conflicts, struct bu_ptbl);

    BU_GET(results->removed_dbip1, struct bu_ptbl);
    BU_GET(results->removed_dbip2, struct bu_ptbl);
    BU_GET(results->removed_both, struct bu_ptbl);

    BU_GET(results->changed_ancestor, struct bu_ptbl);
    BU_GET(results->changed_dbip1_new, struct bu_ptbl);
    BU_GET(results->changed_dbip2_new, struct bu_ptbl);
    BU_GET(results->changed_both, struct bu_ptbl);
    BU_GET(results->changed_conflicts, struct bu_ptbl);

    BU_GET(results->unchanged, struct bu_ptbl);

    BU_PTBL_INIT(results->added_dbip1);
    BU_PTBL_INIT(results->added_dbip2);
    BU_PTBL_INIT(results->added_both);
    BU_PTBL_INIT(results->added_conflicts);

    BU_PTBL_INIT(results->removed_dbip1);
    BU_PTBL_INIT(results->removed_dbip2);
    BU_PTBL_INIT(results->removed_both);

    BU_PTBL_INIT(results->changed_ancestor);
    BU_PTBL_INIT(results->changed_dbip1_new);
    BU_PTBL_INIT(results->changed_dbip2_new);
    BU_PTBL_INIT(results->changed_both);
    BU_PTBL_INIT(results->changed_conflicts);

    BU_PTBL_INIT(results->unchanged);
}


void diff3_results_free(struct diff3_results *results){

    bu_ptbl_free(results->added_dbip1);
    bu_ptbl_free(results->added_dbip2);
    bu_ptbl_free(results->added_both);

    bu_ptbl_free(results->removed_dbip1);
    bu_ptbl_free(results->removed_dbip2);
    bu_ptbl_free(results->removed_both);

    bu_ptbl_free(results->changed_ancestor);
    bu_ptbl_free(results->changed_dbip1_new);
    bu_ptbl_free(results->changed_dbip2_new);
    bu_ptbl_free(results->changed_both);

    bu_ptbl_free(results->unchanged);

    /* TODO - properly free the allocated containers with BU_PUT */
    bu_ptbl_free(results->added_conflicts);
    bu_ptbl_free(results->changed_conflicts);


    BU_PUT(results->added_dbip1, struct bu_ptbl);
    BU_PUT(results->added_dbip2, struct bu_ptbl);
    BU_PUT(results->added_both, struct bu_ptbl);
    BU_PUT(results->added_conflicts, struct bu_ptbl);

    BU_PUT(results->removed_dbip1, struct bu_ptbl);
    BU_PUT(results->removed_dbip2, struct bu_ptbl);
    BU_PUT(results->removed_both, struct bu_ptbl);

    BU_PUT(results->changed_ancestor, struct bu_ptbl);
    BU_PUT(results->changed_dbip1_new, struct bu_ptbl);
    BU_PUT(results->changed_dbip2_new, struct bu_ptbl);
    BU_PUT(results->changed_both, struct bu_ptbl);
    BU_PUT(results->changed_conflicts, struct bu_ptbl);

    BU_PUT(results->unchanged, struct bu_ptbl);
}

/* TODO - this function is a duplicate of a private one in librt - probably
 * need to move bu_avs_diff3 to db_diff and expose it in the private header... */
static int
avpp_val_compare(const char *val1, const char *val2, const struct bn_tol *diff_tol)
{
    /* We need to look for numbers to do tolerance based comparisons */
    int num_compare = 1;
    int pnt_compare = 1;
    double dval1, dval2;
    float p1val1, p1val2, p1val3;
    float p2val1, p2val2, p2val3;
    char *endptr;

    /* First, check for individual numbers */
    errno = 0;
    dval1 = strtod(val1, &endptr);
    if (errno == EINVAL || *endptr != '\0') num_compare--;
    errno = 0;
    dval2 = strtod(val2, &endptr);
    if (errno == EINVAL || *endptr != '\0') num_compare--;

    /* If we didn't find numbers, try for points (3 floating point numbers) */
    if (num_compare != 1) {
	if (!sscanf(val1, "%f %f %f", &p1val1, &p1val2, &p1val3)) pnt_compare--;
	if (!sscanf(val2, "%f %f %f", &p2val1, &p2val2, &p2val3)) pnt_compare--;
    }

    if (num_compare == 1) {
	return NEAR_EQUAL(dval1, dval2, diff_tol->dist);
    }
    if (pnt_compare == 1) {
	vect_t v1, v2;
	VSET(v1, p1val1, p1val2, p1val3);
	VSET(v2, p2val1, p2val2, p2val3);
	return VNEAR_EQUAL(v1, v2, diff_tol->dist);
    }
    return BU_STR_EQUAL(val1, val2);
}


/*******************************************************************/
/* diff3 logic for attribute/value sets                            */
/*******************************************************************/
static void
bu_avs_diff3(struct bu_attribute_value_set *unchanged,
	struct bu_attribute_value_set *removed_avs1_only,
	struct bu_attribute_value_set *removed_avs2_only,
	struct bu_attribute_value_set *removed_both,
	struct bu_attribute_value_set *added_avs1_only,
	struct bu_attribute_value_set *added_avs2_only,
	struct bu_attribute_value_set *added_both,
	struct bu_attribute_value_set *added_conflict_avs1,
	struct bu_attribute_value_set *added_conflict_avs2,
	struct bu_attribute_value_set *changed_avs1_only,
	struct bu_attribute_value_set *changed_avs2_only,
	struct bu_attribute_value_set *changed_both,
	struct bu_attribute_value_set *changed_conflict_ancestor,
	struct bu_attribute_value_set *changed_conflict_avs1,
	struct bu_attribute_value_set *changed_conflict_avs2,
	struct bu_attribute_value_set *merged,
	const struct bu_attribute_value_set *ancestor,
	const struct bu_attribute_value_set *avs1,
	const struct bu_attribute_value_set *avs2,
	const struct bn_tol *diff_tol)
{
    struct bu_attribute_value_pair *avp;
    if (!BU_AVS_IS_INITIALIZED(unchanged)) BU_AVS_INIT(unchanged);
    if (!BU_AVS_IS_INITIALIZED(removed_avs1_only)) BU_AVS_INIT(removed_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(removed_avs2_only)) BU_AVS_INIT(removed_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(removed_both)) BU_AVS_INIT(removed_both);
    if (!BU_AVS_IS_INITIALIZED(added_avs1_only)) BU_AVS_INIT(added_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(added_avs2_only)) BU_AVS_INIT(added_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(added_both)) BU_AVS_INIT(added_both);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_avs1)) BU_AVS_INIT(added_conflict_avs1);
    if (!BU_AVS_IS_INITIALIZED(added_conflict_avs2)) BU_AVS_INIT(added_conflict_avs2);
    if (!BU_AVS_IS_INITIALIZED(changed_avs1_only)) BU_AVS_INIT(changed_avs1_only);
    if (!BU_AVS_IS_INITIALIZED(changed_avs2_only)) BU_AVS_INIT(changed_avs2_only);
    if (!BU_AVS_IS_INITIALIZED(changed_both)) BU_AVS_INIT(changed_both);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_ancestor)) BU_AVS_INIT(changed_conflict_ancestor);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_avs1)) BU_AVS_INIT(changed_conflict_avs1);
    if (!BU_AVS_IS_INITIALIZED(changed_conflict_avs2)) BU_AVS_INIT(changed_conflict_avs2);
    if (!BU_AVS_IS_INITIALIZED(merged)) BU_AVS_INIT(merged);

    for (BU_AVS_FOR(avp, ancestor)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	const char *val1 = bu_avs_get(avs1, avp->name);
	const char *val2 = bu_avs_get(avs2, avp->name);
	/* The possibilities are:
	 *
	 * (!val1 && !val2) && val_ancestor
	 * (val1 && !val2) && (val_ancestor == val1)
	 * (val1 && !val2) && (val_ancestor != val1)
	 * (!val1 && val2) && (val_ancestor == val2)
	 * (!val1 && val2) && (val_ancestor != val2)
	 * (val1 == val2) && (val_ancestor == val1)
	 * (val1 == val2) && (val_ancestor != val1)
	 * (val1 != val2) && (val_ancestor == val1)
	 * (val1 != val2) && (val_ancestor == val2)
	 * (val1 != val2) && (val_ancestor != val1 && val_ancestor != val2)
	 */

	/* Removed from both - no conflict, nothing to merge */
	if ((!val1 && !val2) && val_ancestor)
	    bu_avs_add(removed_both, avp->name, val_ancestor);

	/* Removed from avs2 only, avs1 not changed - no conflict,
	 * avs2 removal wins and avs1 is not merged */
	if ((val1 && !val2) && avpp_val_compare(val_ancestor, val1, diff_tol))
	    bu_avs_add(removed_avs2_only, avp->name, val_ancestor);

	/* Removed from avs2 only, avs1 changed - conflict
	 * merge adds conflict a/v pairs */
	if ((val1 && !val2) && !avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    struct bu_vls avval = BU_VLS_INIT_ZERO;
	    bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    bu_avs_add(changed_conflict_avs1, avp->name, val1);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(left):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val1);
	    bu_vls_sprintf(&avname, "CONFLICT(right):%s", avp->name);
	    bu_vls_sprintf(&avval, "%s", "REMOVED");
	    bu_avs_add(merged, bu_vls_addr(&avname), bu_vls_addr(&avval));
	    bu_vls_free(&avname);
	    bu_vls_free(&avval);
	}

	/* Removed from avs1 only, avs2 not changed - no conflict,
	 * avs1 change wins and avs2 not merged */
	if ((!val1 && val2) && avpp_val_compare(val_ancestor, val2, diff_tol))
	    bu_avs_add(removed_avs1_only, avp->name, val_ancestor);

	/* Removed from avs1 only, avs2 changed - conflict,
	 * merge defaults to preserving information */
	if ((!val1 && val2) && !avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    struct bu_vls avval = BU_VLS_INIT_ZERO;
	    bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    bu_avs_add(changed_conflict_avs2, avp->name, val2);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(left):%s", avp->name);
	    bu_vls_sprintf(&avval, "%s", "REMOVED");
	    bu_avs_add(merged, bu_vls_addr(&avname), bu_vls_addr(&avval));
	    bu_vls_sprintf(&avname, "CONFLICT(right):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val2);
	    bu_vls_free(&avname);
	    bu_vls_free(&avval);
	}

	/* All values equal, unchanged and merged */
	if (avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    bu_avs_add(unchanged, avp->name, val_ancestor);
	    bu_avs_add(merged, avp->name, val_ancestor);
	}
	/* Identical change to both - changed and merged */
	if (avpp_val_compare(val1, val2, diff_tol) && !avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    bu_avs_add(changed_both, avp->name, val1);
	    bu_avs_add(merged, avp->name, val1);
	}
	/* val2 changed, val1 not changed - val2 change wins and is merged */
	if (!avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val1, diff_tol)) {
	    bu_avs_add(changed_avs2_only, avp->name, val2);
	    bu_avs_add(merged, avp->name, val2);
	}
	/* val1 changed, val2 not changed - val1 change wins and is merged */
	if (!avpp_val_compare(val1, val2, diff_tol) && avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    bu_avs_add(changed_avs1_only, avp->name, val1);
	    bu_avs_add(merged, avp->name, val1);
	}
	/* val1 and val2 changed and incompatible - conflict,
	 * merge adds conflict a/v pairs */
	if (!avpp_val_compare(val1, val2, diff_tol) && !avpp_val_compare(val_ancestor, val1, diff_tol) && !avpp_val_compare(val_ancestor, val2, diff_tol)) {
	    struct bu_vls avname = BU_VLS_INIT_ZERO;
	    bu_avs_add(changed_conflict_ancestor, avp->name, val_ancestor);
	    bu_avs_add(changed_conflict_avs1, avp->name, val1);
	    bu_avs_add(changed_conflict_avs2, avp->name, val2);
	    bu_vls_sprintf(&avname, "CONFLICT(ANCESTOR):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val_ancestor);
	    bu_vls_sprintf(&avname, "CONFLICT(LEFT):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val1);
	    bu_vls_sprintf(&avname, "CONFLICT(RIGHT):%s", avp->name);
	    bu_avs_add(merged, bu_vls_addr(&avname), val2);
	    bu_vls_free(&avname);
	}
    }

    /* Now do avs1 - anything in ancestor has already been handled */
    for (BU_AVS_FOR(avp, avs1)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	if (!val_ancestor) {
	    const char *val1 = bu_avs_get(avs1, avp->name);
	    const char *val2 = bu_avs_get(avs2, avp->name);
	    /* The possibilities are:
	     *
	     * (val1 && !val2)
	     * (val1 == val2)
	     * (val1 != val2)
	     */

	    /* Added in avs1 only - no conflict */
	    if (val1 && !val2) {
		bu_avs_add(added_avs1_only, avp->name, val1);
		bu_avs_add(merged, avp->name, val1);
	    }
	    /* Added in avs1 and avs2 with the same value - no conflict */
	    if (avpp_val_compare(val1,val2, diff_tol)) {
	       	bu_avs_add(added_both, avp->name, val1);
		bu_avs_add(merged, avp->name, val1);
	    }
	    /* Added in avs1 and avs2 with different values - conflict
	     * merge adds conflict a/v pairs */
	    if (avpp_val_compare(val1,val2, diff_tol)) {
		struct bu_vls avname = BU_VLS_INIT_ZERO;
		bu_avs_add(added_conflict_avs1, avp->name, val1);
		bu_avs_add(added_conflict_avs2, avp->name, val2);
		bu_vls_sprintf(&avname, "CONFLICT(LEFT):%s", avp->name);
		bu_avs_add(merged, bu_vls_addr(&avname), val1);
		bu_vls_sprintf(&avname, "CONFLICT(RIGHT):%s", avp->name);
		bu_avs_add(merged, bu_vls_addr(&avname), val2);
		bu_vls_free(&avname);
	    }
	}
    }

    /* Last but not least, avs2 - anything in ancestor and/or avs1 has already been handled */
    for (BU_AVS_FOR(avp, avs2)) {
	const char *val_ancestor = bu_avs_get(ancestor, avp->name);
	const char *val1 = bu_avs_get(avs1, avp->name);
	if (!val_ancestor && !val1) {
	    const char *val2 = bu_avs_get(avs2, avp->name);
	    bu_avs_add(added_avs2_only, avp->name, val1);
	    bu_avs_add(merged, avp->name, val2);
	}
    }

}


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

static int
compare_dps(struct diff_result_container *result, const struct directory *dp1, const struct db_i *dbip1, const struct directory *dp2, const struct db_i *dbip2, struct bn_tol *diff_tol)
{
    /* Compare the two objects */
    struct rt_db_internal *intern_1 = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_1");
    struct rt_db_internal *intern_2 = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_2");
    struct bu_attribute_value_set *ino = NULL;
    struct bu_attribute_value_set *ioo = NULL;
    struct bu_attribute_value_set *iod = NULL;
    struct bu_attribute_value_set *ind = NULL;
    struct bu_attribute_value_set *ins = NULL;
    struct bu_attribute_value_set *ano = NULL;
    struct bu_attribute_value_set *aoo = NULL;
    struct bu_attribute_value_set *aod = NULL;
    struct bu_attribute_value_set *avsnd = NULL;
    struct bu_attribute_value_set *ans = NULL;

    int internal_diff = 1;
    int attr_diff = 1;
    int bad_internal_1 = 1;
    int bad_internal_2 = 1;
    RT_DB_INTERNAL_INIT(intern_1);
    RT_DB_INTERNAL_INIT(intern_2);

    if (result) {
	result->dp_orig = dp1;
	result->dp_new = dp2;
    }

    /* Get the internal objects */
    if (rt_db_get_internal(intern_1, dp1, dbip1, (fastf_t *)NULL, &rt_uniresource) >= 0) {
	bad_internal_1--;
    }
    if (rt_db_get_internal(intern_2, dp2, dbip2, (fastf_t *)NULL, &rt_uniresource) >= 0) {
	bad_internal_2--;
    }

    /* If we have a result structure, uses its bu_avs containers */
    if (result) {
	ino = &(result->internal_new_only);
	ioo = &(result->internal_orig_only);
	iod = &(result->internal_orig_diff);
	ind = &(result->internal_new_diff);
	ins = &(result->internal_shared);
	ano = &(result->additional_new_only);
	aoo = &(result->additional_orig_only);
	aod = &(result->additional_orig_diff);
	avsnd = &(result->additional_new_diff);
	ans = &(result->additional_shared);
    }
    if (bad_internal_1 || bad_internal_2) {
	if (!bad_internal_1) rt_db_free_internal(intern_1);
	if (!bad_internal_2) rt_db_free_internal(intern_2);
	if (result) result->status = 1;
	return -1;
    }

    internal_diff = db_compare(intern_1, intern_2, DB_COMPARE_PARAM, ino, ioo, iod, ind, ins, diff_tol);
    attr_diff = db_compare(intern_1, intern_2, DB_COMPARE_ATTRS, ano, aoo, aod, avsnd, ans, diff_tol);

    if (result) {
	result->internal_diff = internal_diff;
	result->attribute_diff = attr_diff;
    }

    rt_db_free_internal(intern_1);
    rt_db_free_internal(intern_2);

    if (!internal_diff && !attr_diff) return 0;
    if (!internal_diff && attr_diff)  return 1;
    if (internal_diff && !attr_diff)  return 2;
    if (internal_diff && attr_diff)   return 3;
    return -1;  /* Shouldn't get here */
}


int
diff_changed(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data)
{
    struct diff_results *results;
    struct diff_result_container *result;
    struct bn_tol diff_tol = BN_TOL_INIT_ZERO;
    int changed_status = -1;

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

    changed_status = compare_dps(result, before, left, after, right, &diff_tol);
    switch (changed_status) {
	case -1:
	    result->status = 1;
	    return -1;
	    break;
	case 0:
	    result->internal_diff = 0;
	    result->attribute_diff = 0;
	    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);
	    bu_ptbl_ins(results->unchanged, (long *)before);
	    diff_result_free(result);
	    break;
	case 1:
	    result->internal_diff = 0;
	    result->attribute_diff = 1;
	    bu_ptbl_ins(results->changed, (long *)result);
	    break;
	case 2:
	    result->internal_diff = 1;
	    result->attribute_diff = 0;
	    bu_ptbl_ins(results->changed, (long *)result);
	    break;
	case 3:
	    result->internal_diff = 1;
	    result->attribute_diff = 1;
	    bu_ptbl_ins(results->changed, (long *)result);
	    break;
	default:
	    bu_log("error - unknown return code %d from directory object comparison\n", changed_status);
	    diff_result_free(result);
	    result->status = 1;
	    return -1;
	    break;
    }

    return 0;
}

/*******************************************************************/
/* Output generators for diff log */
/*******************************************************************/
static void
params_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->internal_orig_only;
    const struct bu_attribute_value_set *new_only = &result->internal_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->internal_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->internal_new_diff;

    if (orig_only->count > 0) {
	bu_vls_printf(attr_log, "   Parameters removed:\n");
	for (BU_AVS_FOR(avpp, orig_only)) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Parameters added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}


static void
attrs_summary(struct bu_vls *attr_log, const struct diff_result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    const struct bu_attribute_value_set *orig_only = &result->additional_orig_only;
    const struct bu_attribute_value_set *new_only = &result->additional_new_only;
    const struct bu_attribute_value_set *orig_diff = &result->additional_orig_diff;
    const struct bu_attribute_value_set *new_diff = &result->additional_new_diff;

    if (orig_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes removed:\n");
	for (BU_AVS_FOR(avpp, orig_only)) {
	    bu_vls_printf(attr_log, "      %s\n", avpp->name);
	}
    }
    if (new_only->count > 0) {
	bu_vls_printf(attr_log, "   Attributes added:\n");
	for (BU_AVS_FOR(avpp, new_only)) {
	    bu_vls_printf(attr_log, "      %s: %s\n", avpp->name, avpp->value);
	}
    }
    if (orig_diff->count > 0) {
	bu_vls_printf(attr_log, "   Attribute parameters changed:\n");
	for (BU_AVS_FOR(avpp, orig_diff)) {
	    bu_vls_printf(attr_log, "      %s: \"%s\" -> \"%s\"\n", avpp->name, avpp->value, bu_avs_get(new_diff, avpp->name));
	}
    }
}


static int
print_dp(struct bu_vls *diff_log, const struct bu_ptbl *dptable, const int cnt, const struct directory *dp, const int line_len)
{
    int local_line_len = line_len;

    if (local_line_len + strlen(dp->d_namep) > 80) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    }

    bu_vls_printf(diff_log, "%s", dp->d_namep);
    local_line_len += strlen(dp->d_namep);

    if (local_line_len > 79 || (cnt+1) == (int)BU_PTBL_LEN(dptable)) {
	bu_vls_printf(diff_log, "\n");
	local_line_len = 0;
    } else {
	bu_vls_printf(diff_log, ", ");
	local_line_len += 2;
    }

    return local_line_len;
}


static void
diff_summarize(struct bu_vls *diff_log, const struct diff_results *results, struct diff_state *state)
{
    int i = 0;

    struct bu_ptbl *added;
    struct bu_ptbl *removed;
    struct bu_ptbl *changed;
    struct bu_ptbl *unchanged;

    if (state->verbosity < 1 || !results)
	return;

    added = results->added;
    removed = results->removed;
    changed = results->changed;
    unchanged = results->unchanged;

    if (state->verbosity == 1) {
	int line_len = 0;
	if (state->return_added > 0) {
	    if ((int)BU_PTBL_LEN(added) > 0) {
		bu_vls_printf(diff_log, "\nObjects added:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		line_len = print_dp(diff_log, added, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_removed > 0) {
	    if ((int)BU_PTBL_LEN(removed) > 0) {
		bu_vls_printf(diff_log, "\nObjects removed:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		line_len = print_dp(diff_log, removed, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_changed > 0) {
	    if ((int)BU_PTBL_LEN(changed) > 0) {
		bu_vls_printf(diff_log, "\nObjects changed:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(changed, i);
		const struct directory *dp = result->dp_orig;
		if (!dp) dp = result->dp_new;
		line_len = print_dp(diff_log, changed, i, dp, line_len);
	    }
	}
	line_len = 0;
	if (state->return_unchanged > 0) {
	    if ((int)BU_PTBL_LEN(unchanged) > 0) {
		bu_vls_printf(diff_log, "\nObjects unchanged:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		line_len = print_dp(diff_log, unchanged, i, dp, line_len);
	    }
	}

    }
    if (state->verbosity > 1) {
	if (state->return_added > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		bu_vls_printf(diff_log, "%s was added.\n\n", dp->d_namep);
	    }
	}

	if (state->return_removed > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		bu_vls_printf(diff_log, "%s was removed.\n\n", dp->d_namep);
	    }
	}

	if (state->return_changed > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(changed, i);
		const struct directory *dp = result->dp_orig;
		if (!dp) dp = result->dp_new;
		bu_vls_printf(diff_log, "%s was changed:\n", dp->d_namep);
		params_summary(diff_log, result);
		attrs_summary(diff_log, result);
		bu_vls_printf(diff_log, "\n");
	    }
	}
	if (state->return_unchanged > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		bu_vls_printf(diff_log, "%s was unchanged.\n\n", dp->d_namep);
	    }
	}


    }
    bu_vls_printf(diff_log, "\n");
}

static void
diff3_summarize(struct bu_vls *diff_log, const struct diff3_results *results, struct diff_state *state)
{
    int i = 0;

    if (state->verbosity < 1 || !results)
	return;

    /* Temporarily, only do verbosity == 1 */
    if (state->verbosity >= 1) {
	int line_len = 0;
	if (state->return_added > 0) {
	    if ((int)BU_PTBL_LEN(results->added_dbip1) > 0) {
		bu_vls_printf(diff_log, "\nObjects added in dbip1:\n"); /* TODO - capture input filenames for this */
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->added_dbip1); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->added_dbip1, i);
		line_len = print_dp(diff_log, results->added_dbip1, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	    if ((int)BU_PTBL_LEN(results->added_dbip2) > 0) {
		bu_vls_printf(diff_log, "\nObjects added in dbip2:\n"); /* TODO - capture input filenames for this */
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->added_dbip2); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->added_dbip2, i);
		line_len = print_dp(diff_log, results->added_dbip2, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	    if ((int)BU_PTBL_LEN(results->added_both) > 0) {
		bu_vls_printf(diff_log, "\nObjects added in both:\n"); /* TODO - capture input filenames for this */
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->added_both); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->added_both, i);
		line_len = print_dp(diff_log, results->added_both, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	}
	line_len = 0;
	if (state->return_removed > 0) {
	    if ((int)BU_PTBL_LEN(results->removed_dbip1) > 0) {
		bu_vls_printf(diff_log, "\nObjects removed from dbip1 only:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->removed_dbip1); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->removed_dbip1, i);
		line_len = print_dp(diff_log, results->removed_dbip1, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	    if ((int)BU_PTBL_LEN(results->removed_dbip2) > 0) {
		bu_vls_printf(diff_log, "\nObjects removed from dbip2 only:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->removed_dbip2); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->removed_dbip2, i);
		line_len = print_dp(diff_log, results->removed_dbip2, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	    if ((int)BU_PTBL_LEN(results->removed_both) > 0) {
		bu_vls_printf(diff_log, "\nObjects removed from both:\n");
	    }
	    for (i = 0; i < (int)BU_PTBL_LEN(results->removed_both); i++) {
		struct directory *dp = (struct directory *)BU_PTBL_GET(results->removed_both, i);
		line_len = print_dp(diff_log, results->removed_both, i, dp, line_len);
	    }
	    bu_vls_printf(diff_log, "\n");
	}
	line_len = 0;
	if (state->return_changed > 0) {
	    if ((int)BU_PTBL_LEN(results->changed_dbip1_new) > 0) {
		bu_vls_printf(diff_log, "\nObjects changed in dbip1 only: %d\n", (int)BU_PTBL_LEN(results->changed_dbip1_new));
	    }
	    if ((int)BU_PTBL_LEN(results->changed_dbip2_new) > 0) {
		bu_vls_printf(diff_log, "\nObjects changed in dbip2 only: %d\n", (int)BU_PTBL_LEN(results->changed_dbip2_new));
	    }
	    if ((int)BU_PTBL_LEN(results->changed_both) > 0) {
		bu_vls_printf(diff_log, "\nObjects changed in both: %d\n", (int)BU_PTBL_LEN(results->changed_both));
	    }
	}
	line_len = 0;
	if (state->return_conflicts > 0) {
	    if ((int)BU_PTBL_LEN(results->added_conflicts) > 0) {
		bu_vls_printf(diff_log, "\n%d conflicts in objects added to dbip1 and dbip2\n", (int)BU_PTBL_LEN(results->added_conflicts));
	    }
	    if ((int)BU_PTBL_LEN(results->changed_conflicts) > 0) {
		bu_vls_printf(diff_log, "\n%d conflicts in objects changed in both dbip1 and dbip2\n", (int)BU_PTBL_LEN(results->changed_conflicts));
	    }

	}
	line_len = 0;
	if (state->return_unchanged > 0) {
	    if ((int)BU_PTBL_LEN(results->unchanged) > 0) {
		bu_vls_printf(diff_log, "\nObjects unchanged: %d\n", (int)BU_PTBL_LEN(results->unchanged));
	    }
	}

    }
    bu_vls_printf(diff_log, "\n");
}



/*******************************************************************/
/* Primary function for basic diff operation on two .g files */
/*******************************************************************/
static int
do_diff(struct db_i *ancestor_dbip, struct db_i *new_dbip_1, struct diff_state *state) {
    int have_diff = 0;
    int diff_return = 0;
    struct diff_results *results;

    BU_GET(results, struct diff_results);
    diff_results_init(results);
    results->diff_tolerance = state->diff_tolerance;

    have_diff = db_diff(ancestor_dbip, new_dbip_1, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)results);

    /* Now we have our diff results, time to filter (if applicable) and report them */
    if (state->have_search_filter) {
	int i = 0;
	struct bu_ptbl added_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl removed_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl unchanged_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl changed_filtered = BU_PTBL_INIT_ZERO;

	/* In order to respect search filters involving depth, we have to build "allowed"
	 * sets of objects from both databases based on straight-up search filtering of
	 * the original databases.  We then check the filtered diff results against the
	 * search-filtered original databases to make sure they are valid for the final
	 * results-> */
	struct bu_ptbl ancestor_dbip_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl new_dbip_1_filtered = BU_PTBL_INIT_ZERO;
	(void)db_search(&ancestor_dbip_filtered, DB_SEARCH_RETURN_UNIQ_DP, (const char *)bu_vls_addr(state->search_filter), 0, NULL, ancestor_dbip);
	(void)db_search(&new_dbip_1_filtered, DB_SEARCH_RETURN_UNIQ_DP, (const char *)bu_vls_addr(state->search_filter), 0, NULL, new_dbip_1);

	/* Filter added objects */
	if (BU_PTBL_LEN(results->added) > 0) {
	    bu_ptbl_cat(&added_filtered, results->removed);
	    bu_ptbl_reset(results->added);
	    for (i = 0; i < (int)BU_PTBL_LEN(&added_filtered); i++) {
		if (bu_ptbl_locate(&new_dbip_1_filtered, (long *)BU_PTBL_GET(&added_filtered, i)) != -1) {
		    bu_ptbl_ins(results->added, (long *)BU_PTBL_GET(&added_filtered, i));
		}
	    }
	    bu_ptbl_free(&added_filtered);
	}

	/* Filter removed objects */
	if (BU_PTBL_LEN(results->removed) > 0) {
	    bu_ptbl_cat(&removed_filtered, results->removed);
	    bu_ptbl_reset(results->removed);
	    for (i = 0; i < (int)BU_PTBL_LEN(&removed_filtered); i++) {
		if (bu_ptbl_locate(&ancestor_dbip_filtered, (long *)BU_PTBL_GET(&removed_filtered, i)) != -1) {
		    bu_ptbl_ins(results->removed, (long *)BU_PTBL_GET(&removed_filtered, i));
		}
	    }
	    bu_ptbl_free(&removed_filtered);
	}

	/* Filter changed objects */
	if (BU_PTBL_LEN(results->changed_ancestor_dbip) > 0 || BU_PTBL_LEN(results->changed_new_dbip_1) > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(results->changed, i);
		if ((result->dp_orig && bu_ptbl_locate(&ancestor_dbip_filtered, (long *)result->dp_orig) != -1) ||
			(result->dp_new && bu_ptbl_locate(&new_dbip_1_filtered, (long *)result->dp_orig) != -1)) {
		    bu_ptbl_ins(&changed_filtered, (long *)result);
		} else {
		    diff_result_free((void *)result);
		}
	    }
	    bu_ptbl_reset(results->changed);
	    bu_ptbl_cat(results->changed, &changed_filtered);
	    bu_ptbl_free(&changed_filtered);
	} else {
	    for (i = 0; i < (int)BU_PTBL_LEN(results->changed); i++) {
		struct diff_result_container *result = (struct diff_result_container *)BU_PTBL_GET(results->changed, i);
		diff_result_free((void *)result);
	    }
	    bu_ptbl_reset(results->changed);
	}

	/* Filter unchanged objects */
	if (BU_PTBL_LEN(results->unchanged) > 0) {
	    bu_ptbl_cat(&unchanged_filtered, results->unchanged);
	    bu_ptbl_reset(results->unchanged);
	    for (i = 0; i < (int)BU_PTBL_LEN(&unchanged_filtered); i++) {
		if (bu_ptbl_locate(&ancestor_dbip_filtered, (long *)BU_PTBL_GET(&unchanged_filtered, i)) != -1) {
		    bu_ptbl_ins(results->unchanged, (long *)BU_PTBL_GET(&unchanged_filtered, i));
		}
	    }
	    bu_ptbl_cat(results->unchanged, &unchanged_filtered);
	}
	db_search_free(&ancestor_dbip_filtered);
	db_search_free(&new_dbip_1_filtered);
    }

    if (state->verbosity) {
	if (!have_diff) {
	    bu_log("No differences found.\n");
	} else {
	    diff_summarize(state->diff_log, results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }
    if (have_diff) {
	if (state->return_added > 0) diff_return += BU_PTBL_LEN(results->added);
	if (state->return_removed > 0) diff_return += BU_PTBL_LEN(results->removed);
	if (state->return_changed > 0) diff_return += BU_PTBL_LEN(results->changed);
	if (state->return_unchanged > 0) diff_return += BU_PTBL_LEN(results->unchanged);
    }

    diff_results_free(results);
    BU_PUT(results, struct diff_results);

    return diff_return;
}


/*******************************************************************/
/* Functions for 3-way diff3 set building (supports 3-way merge)  */
/*******************************************************************/

/* TODO - may need to bu_sort the ptbl contents by name
 * so we can do a binary lookup for these two functions instead
 * of looking at every entry every time - potentially expensive in
 * the worst cases. */

static const struct directory *
dp_ptbl_find(struct bu_ptbl *table, const char *name)
{
    int i = 0;
    const struct directory *found = RT_DIR_NULL;
    for (i = 0; i < (int)BU_PTBL_LEN(table); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(table, i);
	if (BU_STR_EQUAL(name, dp->d_namep)) found = dp;
    }

    return found;
}

static const struct directory *
rc_ptbl_find(struct bu_ptbl *table, const char *name)
{
    int i = 0;
    const struct directory *found = RT_DIR_NULL;
    for (i = 0; i < (int)BU_PTBL_LEN(table); i++) {
	struct diff_result_container *rc = (struct diff_result_container *)BU_PTBL_GET(table, i);
	const struct directory *dp = rc->dp_orig;
	if (BU_STR_EQUAL(name, dp->d_namep)) found = dp;
    }

    return found;
}

static int
do_diff3(struct db_i *ancestor_dbip, struct db_i *new_dbip_1, struct db_i *new_dbip_2, struct diff_state *state) {
    int i = 0;
    int have_diff = 0;
    struct diff_results *dbip1_results;
    struct diff_results *dbip2_results;
    int diff_return_1 = 0;
    int diff_return_2 = 0;
    int diff_return = 0;
    struct diff3_results *results;
    struct bn_tol diff_tol = BN_TOL_INIT_ZERO;

    diff_tol.dist = state->diff_tolerance;

    BU_GET(results, struct diff3_results);

    diff3_results_init(results);

    BU_GET(dbip1_results, struct diff_results);
    BU_GET(dbip2_results, struct diff_results);

    diff_results_init(dbip1_results);
    diff_return_1 = db_diff(ancestor_dbip, new_dbip_1, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)dbip1_results);
    diff_results_init(dbip2_results);
    diff_return_2 = db_diff(ancestor_dbip, new_dbip_2, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)dbip2_results);
    if (diff_return_1 < 0 && diff_return_2 < 0) {
	return diff_return_1;
    } else {
	have_diff = 1;
    }

    /* Now we know what changed from the ancestor file to each of the two new files.
     * We now need to categorize the results according to three-way merging needs. The
     * directory pointers in the tables aren't adequate for this, as the same object
     * will have different pointers in each table. */

    /* To get a true "unchanged" set, one of the unchanged sets needs
     * to have any of the objects present in the other diff's results
     * removed, since they are not unchanged in the final merged output */
    BU_PTBL_INIT(results->unchanged);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip1_results->unchanged); i++) {
	int changed = 0;
	struct directory *dp = (struct directory *)BU_PTBL_GET(dbip1_results->unchanged, i);
	if (dp_ptbl_find(dbip2_results->added, dp->d_namep)) changed++;
	if (dp_ptbl_find(dbip2_results->removed, dp->d_namep)) changed++;
	if (rc_ptbl_find(dbip2_results->changed, dp->d_namep)) changed++;
	if (!changed) {
	    bu_ptbl_ins(results->unchanged, (long *)dp);
	} else {
	    bu_log("unchanged in one, but changed in the other: %s\n", dp->d_namep);
	}
    }

    /* If the same object was added in both files, we have a conflict
     * unless the objects are identical. (Maybe not strictly true -
     * could attrs be merged if the sets are fully compatible?) */
    BU_PTBL_INIT(results->added_dbip1);
    BU_PTBL_INIT(results->added_both);
    BU_PTBL_INIT(results->added_conflicts);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip1_results->added); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip1_results->added, i);
	const struct directory *dp2 = dp_ptbl_find(dbip2_results->added, dp->d_namep);
	if (dp2 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->added_dbip1, (long *)dp);
	} else {
	    int c = compare_dps(NULL, dp, new_dbip_1, dp2, new_dbip_2, &diff_tol);
	    switch (c) {
		case -1:
		    break;
		case 0:
		    bu_ptbl_ins(results->added_both, (long *)dp);
		    break;
		case 1:
		case 2:
		case 3:
		    struct diff_result_container *new_conflict;
		    BU_GET(new_conflict, diff_result_container);
		    new_conflict->dbip_ancestor = DBI_NULL;
		    new_conflict->dbip_orig = new_dbip_1;
		    new_conflict->dbip_new = new_dbip_2;
		    new_conflict->dp_ancestor = RT_DIR_NULL;
		    new_conflict->dp_orig = dp;
		    new_conflict->dp_new = dp2;
		    bu_ptbl_ins(results->added_conflicts, (long *)new_conflict);
		    break;
		default:
		    bu_log("error - unknown return code %d from directory object comparison\n", c);
	    }

	}
    }
    BU_PTBL_INIT(results->added_dbip2);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip2_results->added); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip2_results->added, i);
	const struct directory *dp1 = dp_ptbl_find(dbip1_results->added, dp->d_namep);
	if (dp1 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->added_dbip2, (long *)dp);
	}
	/* We already got all the common objects when we iterated through dbip1_results->added */
    }

    /* If the same object was removed in both files, we make a note
     * of that to allow for more informative reporting */
    BU_PTBL_INIT(results->removed_dbip1);
    BU_PTBL_INIT(results->removed_both);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip1_results->removed); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip1_results->removed, i);
	const struct directory *dp2 = dp_ptbl_find(dbip2_results->removed, dp->d_namep);
	if (dp2 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->removed_dbip1, (long *)dp);
	} else {
	    bu_ptbl_ins(results->removed_both, (long *)dp);
	}
    }
    BU_PTBL_INIT(results->removed_dbip2);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip2_results->removed); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip2_results->removed, i);
	const struct directory *dp1 = dp_ptbl_find(dbip1_results->removed, dp->d_namep);
	if (dp1 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->removed_dbip2, (long *)dp);
	}
	/* We already got all the common objects when we iterated through dbip1_results->removed */
    }


    /* If the same object was changed in both files, we have a *potential* conflict
     * unless the changes are identical. Whether it is always a conflict revolves
     * around whether we try to merge changes below the object level. A possible
     * middle ground is to try and resolve attributes but not tangle with primitive
     * parameters, which avoids thorny issues such as when are comb trees mergeable? */
    BU_PTBL_INIT(results->changed_ancestor);
    BU_PTBL_INIT(results->changed_dbip1_new);
    BU_PTBL_INIT(results->changed_dbip2_new);
    BU_PTBL_INIT(results->changed_both);
    for (i = 0; i < (int)BU_PTBL_LEN(dbip1_results->changed_ancestor_dbip); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip1_results->changed_new_dbip_1, i);
	const struct directory *dp2 = (struct directory *)BU_PTBL_GET(dbip2_results->changed_new_dbip_1, i);
	bu_ptbl_ins_unique(results->changed_ancestor, (long *)BU_PTBL_GET(dbip1_results->changed_ancestor_dbip, i));
	if (dp2 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->changed_dbip1_new, (long *)dp);
	} else {
	    /* Same situation as adding, in some respects - did we end up in the same
	     * place or not?  This should be more sophisticated, but for now do the
	     * simple thing and don't try to merge attrs. */
	    int c = compare_dps(NULL, dp, new_dbip_1, dp2, new_dbip_2, &diff_tol);
	    switch (c) {
		case -1:
		    break;
		case 0:
		    bu_ptbl_ins(results->changed_both, (long *)dp);
		    break;
		case 1:
		case 2:
		case 3:
		    struct bu_attribute_value_set unchanged;
		    struct bu_attribute_value_set removed_avs1_only;
		    struct bu_attribute_value_set removed_avs2_only;
		    struct bu_attribute_value_set removed_both;
		    struct bu_attribute_value_set added_avs1_only;
		    struct bu_attribute_value_set added_avs2_only;
		    struct bu_attribute_value_set added_both;
		    struct bu_attribute_value_set added_conflict_avs1;
		    struct bu_attribute_value_set added_conflict_avs2;
		    struct bu_attribute_value_set changed_avs1_only;
		    struct bu_attribute_value_set changed_avs2_only;
		    struct bu_attribute_value_set changed_both;
		    struct bu_attribute_value_set changed_conflict_ancestor;
		    struct bu_attribute_value_set changed_conflict_avs1;
		    struct bu_attribute_value_set changed_conflict_avs2;
		    struct bu_attribute_value_set merged;
		    struct diff_result_container *new_conflict;
		    BU_GET(new_conflict, struct diff_result_container);
		    new_conflict->dbip_ancestor = ancestor_dbip;
		    new_conflict->dbip_orig = new_dbip_1;
		    new_conflict->dbip_new = new_dbip_2;
		    new_conflict->dp_ancestor = (struct directory *)BU_PTBL_GET(dbip1_results->changed_ancestor_dbip, i);
		    new_conflict->dp_orig = dp;
		    new_conflict->dp_new = dp2;
		    bu_ptbl_ins(results->changed_conflicts, (long *)new_conflict);

		    bu_avs_diff3(&unchanged, &removed_avs1_only, &removed_avs2_only, &removed_both,
			    &added_avs1_only, &added_avs2_only, &added_both, &added_conflict_avs1,
			    &added_conflict_avs2, &changed_avs1_only, &changed_avs2_only, &changed_both,
			    &changed_conflict_ancestor, &changed_conflict_avs1, &changed_conflict_avs2,
			    &merged, NULL, NULL, NULL, &diff_tol);

		    break;
		default:
		    bu_log("error - unknown return code %d from directory object comparison\n", c);
	    }
	}
    }
    for (i = 0; i < (int)BU_PTBL_LEN(dbip2_results->changed_ancestor_dbip); i++) {
	const struct directory *dp = (struct directory *)BU_PTBL_GET(dbip2_results->changed_new_dbip_1, i);
	const struct directory *dp1 = (struct directory *)BU_PTBL_GET(dbip1_results->changed_new_dbip_1, i);
	bu_ptbl_ins_unique(results->changed_ancestor, (long *)BU_PTBL_GET(dbip2_results->changed_ancestor_dbip, i));
	if (dp1 == RT_DIR_NULL) {
	    bu_ptbl_ins(results->changed_dbip2_new, (long *)dp);
	} else {
	    /* The real work begins when we have two changed objects with different changes -
	     * are the changes such that they can be combined (i.e. two different attributes changed/added)
	     * or are they incompatible changes to the same object attribute? */
	}
    }

    if (state->verbosity) {
	if (!have_diff) {
	    bu_log("No differences found.\n");
	} else {
	    diff3_summarize(state->diff_log, results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }
    if (have_diff) {
	if (state->return_added > 0) diff_return += BU_PTBL_LEN(results->added_dbip1);
	if (state->return_added > 0) diff_return += BU_PTBL_LEN(results->added_dbip2);
	if (state->return_added > 0) diff_return += BU_PTBL_LEN(results->added_both);
	if (state->return_removed > 0) diff_return += BU_PTBL_LEN(results->removed_dbip1);
	if (state->return_removed > 0) diff_return += BU_PTBL_LEN(results->removed_dbip2);
	if (state->return_removed > 0) diff_return += BU_PTBL_LEN(results->removed_both);
	if (state->return_changed > 0) diff_return += BU_PTBL_LEN(results->changed_dbip1_new);
	if (state->return_changed > 0) diff_return += BU_PTBL_LEN(results->changed_dbip2_new);
	if (state->return_changed > 0) diff_return += BU_PTBL_LEN(results->changed_both);
	if (state->return_conflicts > 0) diff_return += BU_PTBL_LEN(results->added_conflicts);
	if (state->return_conflicts > 0) diff_return += BU_PTBL_LEN(results->changed_conflicts);
	if (state->return_unchanged > 0) diff_return += BU_PTBL_LEN(results->unchanged);
    }

    diff3_results_free(results);
    BU_PUT(results, struct diff3_results);
    BU_PUT(dbip1_results, struct diff_results);
    BU_PUT(dbip2_results, struct diff_results);

    return 0;
}



static void
gdiff_usage(const char *str) {
    bu_log("Usage: %s [-acmrv] file1.g file2.g\n", str);
    bu_exit(1, NULL);
}


int
main(int argc, char **argv)
{
    int c;
    int diff_return = 0;
    struct diff_state *state;
    struct db_i *ancestor_dbip = DBI_NULL;
    struct db_i *new_dbip_1 = DBI_NULL;
    struct db_i *new_dbip_2 = DBI_NULL;
    const char *diff_prog_name = argv[0];

    BU_GET(state, struct diff_state);
    diff_state_init(state);

    while ((c = bu_getopt(argc, argv, "aC:cF:mrt:uv:xh?")) != -1) {
	switch (c) {
	    case 'a':
		state->return_added = 1;
		break;
	    case 'r':
		state->return_removed = 1;
		break;
	    case 'c':
		state->return_changed = 1;
		break;
	    case 'u':
		state->return_unchanged = 1;
		break;
	    case 'F':
		state->have_search_filter = 1;
		bu_vls_sprintf(state->search_filter, "%s", bu_optarg);
		break;
	    case 'm':   /* mged readable */
		state->output_mode = 109;  /* use ascii decimal value for 'm' to signify mged mode */
		break;
	    case 't':   /* distance tolerance for same/different decisions (RT_LEN_TOL is default) */
		if (sscanf(bu_optarg, "%f", &(state->diff_tolerance)) != 1) {
		    bu_log("Invalid distance tolerance specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    case 'v':   /* verbosity (2 is default) */
		if (sscanf(bu_optarg, "%d", &(state->verbosity)) != 1) {
		    bu_log("Invalid verbosity specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    case 'C':   /* conflict reporting (1 is default) */
		if (sscanf(bu_optarg, "%d", &(state->return_conflicts)) != 1) {
		    bu_log("Invalid verbosity specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    default:
		gdiff_usage(diff_prog_name);
	}
    }

    if (state->output_mode == 109) {
	bu_log("Error - mged script generation not yet implemented\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (state->return_added == -1 && state->return_removed == -1 && state->return_changed == -1 && state->return_unchanged == 0) {
	state->return_added = 1; state->return_removed = 1; state->return_changed = 1;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc != 2 && argc != 3) {
	bu_log("Error - please specify either two or three .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[0]);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (argc == 3 && !bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    if ((ancestor_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[0]);
    }
    RT_CK_DBI(ancestor_dbip);
    if (db_dirbuild(ancestor_dbip) < 0) {
	db_close(ancestor_dbip);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }

    if ((new_dbip_1 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(new_dbip_1);
    if (db_dirbuild(new_dbip_1) < 0) {
	db_close(ancestor_dbip);
	db_close(new_dbip_1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    /* If we have three files, we're doing a 3 way diff and maybe a 3 way merge */
    if (argc == 3) {
	if ((new_dbip_2 = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
	}
	RT_CK_DBI(new_dbip_2);
	if (db_dirbuild(new_dbip_2) < 0) {
	    db_close(ancestor_dbip);
	    db_close(new_dbip_2);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
	}
    }


    if (argc == 2)
	diff_return = do_diff(ancestor_dbip, new_dbip_1, state);

    if (argc == 3)
	diff_return = do_diff3(ancestor_dbip, new_dbip_1, new_dbip_2, state);

    diff_state_free(state);
    BU_PUT(state, struct diff_state);

    db_close(ancestor_dbip);
    db_close(new_dbip_1);
    if (argc == 3) db_close(new_dbip_2);
    return diff_return;
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
