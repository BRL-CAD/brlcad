/*                     G D I F F 2 . H
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

#include <string.h>

#include "tcl.h"

#include "bu/getopt.h"
#include "raytrace.h"
#include "rt/db_diff.h"

#ifndef _GDIFF2_H
#define _GDIFF2_H

/*******************************************************************/
/*     Containers for holding various forms of diff information    */
/*******************************************************************/

/* Reporting options, search filters, and other user specified state */
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

extern void diff_state_init(struct diff_state *state);
extern void diff_state_free(struct diff_state *state);

/* diff results container for differing objects */
struct diff_result_container {
    int status;
    struct db_i *dbip_orig;
    struct db_i *dbip_new;
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

extern void diff_result_init(struct diff_result_container *result);
extern void diff_result_free(struct diff_result_container *result);

/* diff3 results container for differing objects */
struct diff3_result_container {
    int status;
    const struct db_i *ancestor_dbip;
    const struct db_i *left_dbip;
    const struct db_i *right_dbip;
    const struct directory *ancestor_dp;
    const struct directory *left_dp;
    const struct directory *right_dp;
    int internal_diff;
    struct bu_attribute_value_set param_unchanged;
    struct bu_attribute_value_set param_removed_left_only;
    struct bu_attribute_value_set param_removed_right_only;
    struct bu_attribute_value_set param_removed_both;
    struct bu_attribute_value_set param_added_left_only;
    struct bu_attribute_value_set param_added_right_only;
    struct bu_attribute_value_set param_added_both;
    struct bu_attribute_value_set param_added_conflict_left;
    struct bu_attribute_value_set param_added_conflict_right;
    struct bu_attribute_value_set param_changed_left_only;
    struct bu_attribute_value_set param_changed_right_only;
    struct bu_attribute_value_set param_changed_both;
    struct bu_attribute_value_set param_changed_conflict_ancestor;
    struct bu_attribute_value_set param_changed_conflict_left;
    struct bu_attribute_value_set param_changed_conflict_right;
    struct bu_attribute_value_set param_merged;
    int attribute_diff;
    struct bu_attribute_value_set attribute_unchanged;
    struct bu_attribute_value_set attribute_removed_left_only;
    struct bu_attribute_value_set attribute_removed_right_only;
    struct bu_attribute_value_set attribute_removed_both;
    struct bu_attribute_value_set attribute_added_left_only;
    struct bu_attribute_value_set attribute_added_right_only;
    struct bu_attribute_value_set attribute_added_both;
    struct bu_attribute_value_set attribute_added_conflict_left;
    struct bu_attribute_value_set attribute_added_conflict_right;
    struct bu_attribute_value_set attribute_changed_left_only;
    struct bu_attribute_value_set attribute_changed_right_only;
    struct bu_attribute_value_set attribute_changed_both;
    struct bu_attribute_value_set attribute_changed_conflict_ancestor;
    struct bu_attribute_value_set attribute_changed_conflict_left;
    struct bu_attribute_value_set attribute_changed_conflict_right;
    struct bu_attribute_value_set attribute_merged;
};

extern void diff3_result_init(struct diff3_result_container *result);
extern void diff3_result_free(struct diff3_result_container *result);

/* Higher level diff results for databases - holds information for
 * multiple objects */
struct diff_results {
    float diff_tolerance;
    struct bu_ptbl *added;     /* directory pointers */
    struct bu_ptbl *removed;   /* directory pointers */
    struct bu_ptbl *unchanged; /* directory pointers */
    struct bu_ptbl *changed;   /* result containers */
    struct bu_ptbl *changed_ancestor_dbip;   /* directory pointers */
    struct bu_ptbl *changed_new_dbip_1;   /* directory pointers */
};

extern void diff_results_init(struct diff_results *results);
extern void diff_results_free(struct diff_results *results);

/* Higher level diff3 results for databases - holds information for
 * multiple objects */
struct diff3_results {
    float diff_tolerance;
    struct db_i *merged_db;
    struct bu_ptbl *unchanged; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_left_only; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_right_only; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *removed_both; /* directory pointers (ancestor dbip) */
    struct bu_ptbl *added_left_only; /* directory pointers (left dbip) */
    struct bu_ptbl *added_right_only; /* directory pointers (right dbip) */
    struct bu_ptbl *added_both; /* directory pointers (left dbip) */
    struct bu_ptbl *added_merged; /* containers */
    struct bu_ptbl *changed; /* containers */
    struct bu_ptbl *conflict; /* containers */
};

extern void diff3_results_init(struct diff3_results *results);
extern void diff3_results_free(struct diff3_results *results);


/*******************************************************************/
/*              callback functions for diff processing             */
/*******************************************************************/
extern int diff_added(const struct db_i *left,
	const struct db_i *right,
	const struct directory *added,
	void *data);

extern int diff_removed(const struct db_i *left,
	const struct db_i *right,
	const struct directory *removed,
	void *data);

extern int diff_unchanged(const struct db_i *left,
	const struct db_i *right,
	const struct directory *unchanged,
	void *data);

extern int diff_changed(const struct db_i *left,
	const struct db_i *right,
	const struct directory *before,
	const struct directory *after,
	void *data);

/*******************************************************************/
/*             callback functions for diff3 processing             */
/*******************************************************************/
extern int diff3_added(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	const struct directory *dp_left, 
	const struct directory *dp_ancestor, 
	const struct directory *dp_right, 
	void *data);

extern int diff3_removed(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	const struct directory *dp_left, 
	const struct directory *dp_ancestor, 
	const struct directory *dp_right, 
	void *data);


extern int diff3_unchanged(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	const struct directory *dp_left, 
	const struct directory *dp_ancestor, 
	const struct directory *dp_right, 
	void *data);

extern int diff3_changed(const struct db_i *left,
	const struct db_i *ancestor,
	const struct db_i *right,
	const struct directory *dp_left, 
	const struct directory *dp_ancestor, 
	const struct directory *dp_right, 
	void *data);


#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
