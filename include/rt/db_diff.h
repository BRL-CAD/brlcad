/*                       D B _ D I F F . H
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
/** @file db_diff.h
 *
 * Diff interface for comparing geometry.
 *
 */

/**
 * DIFF bit flags to select various types of results
 */
#define DIFF_UNCHANGED	0      /* (left == right) */
#define DIFF_REMOVED	1      /* (right == NULL) */
#define DIFF_ADDED	2      /* (left == NULL)  */
#define DIFF_CHANGED	4      /* (left != right) */

/**
 * Compare two database instances.
 *
 * All objects in dbip_left are compared against the objects in
 * dbip_right.  Every object results in one of four callback functions
 * getting called.  Any objects in dbip_right but not in dbip_left
 * cause add_func() to get called.  Any objects in dbip_left but not
 * in dbip_right cause del_func() to get called.  Objects existing in
 * both (i.e., with the same name) but differing in some fashion
 * cause chgd_func() to get called. If the object exists in both
 * but is unchanged, unch_func() is called.  NULL may be
 * passed to skip any callback.
 *
 * Returns an int with bit flags set according to the above
 * four diff categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_diff(const struct db_i *dbip_left,
	const struct db_i *dbip_right,
	int (*add_func)(const struct db_i *left, const struct db_i *right, const struct directory *added, void *data),
	int (*del_func)(const struct db_i *left, const struct db_i *right, const struct directory *removed, void *data),
	int (*chgd_func)(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data),
	int (*unch_func)(const struct db_i *left, const struct db_i *right, const struct directory *unchanged, void *data),
	void *client_data);

/**
 * DIFF3 bit flags to select various types of results
 */
#define DIFF3_UNCHANGED				0         /* (ancestor == left) && (ancestor == right)                    */
#define DIFF3_REMOVED_BOTH_IDENTICALLY 		1         /* (ancestor) && (right == NULL) && (left == NULL)              */
#define DIFF3_REMOVED_LEFT_ONLY 		2         /* (ancestor == right) && (left == NULL)                        */
#define DIFF3_REMOVED_RIGHT_ONLY 		4         /* (ancestor == left) && (right == NULL)                        */
#define DIFF3_ADDED_BOTH_IDENTICALLY 		8         /* (ancestor == NULL) && (left == right)                        */
#define DIFF3_ADDED_LEFT_ONLY 			16        /* (ancestor == NULL) && (right == NULL) && (left)              */
#define DIFF3_ADDED_RIGHT_ONLY 			32        /* (ancestor == NULL) && (left == NULL) && (right)              */
#define DIFF3_CHANGED_BOTH_IDENTICALLY   	64        /* (ancestor != left) && (ancestor != right) && (left == right) */
#define DIFF3_CHANGED_LEFT_ONLY 		128       /* (ancestor != left) && (ancestor == right)                    */
#define DIFF3_CHANGED_RIGHT_ONLY 		256       /* (ancestor == left) && (ancestor != right)                    */
#define DIFF3_CHANGED_CLEAN_MERGE 		1024      /* ((ancestor == NULL) || ((ancestor != left) && (ancestor != right))) && (left != right) && (clean_merge)  */
#define DIFF3_CONFLICT_LEFT_CHANGE_RIGHT_DEL 	2048      /* (ancestor != left) && (right== NULL)                         */
#define DIFF3_CONFLICT_RIGHT_CHANGE_LEFT_DEL 	4096      /* (ancestor != right) && (left == NULL)                        */
#define DIFF3_CONFLICT_BOTH_CHANGED	 	8192      /* ((ancestor == NULL) || ((ancestor != left) && (ancestor != right))) && (left != right) && (!clean_merge)  */

/**
 * Compare three database instances.
 *
 * This does a "3-way" diff to identify changes in the left and
 * right databases relative to the ancestor database, and provides
 * functional hooks for the various cases.
 *
 * Returns an int with bit flags set according to the above
 * diff3 categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_diff3(const struct db_i *dbip_left,
	const struct db_i *dbip_ancestor,
	const struct db_i *dbip_right,
	int (*add_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*del_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*chgd_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	int (*unchgd_func)(const struct db_i *left_dbip, const struct db_i *ancestor_dbip, const struct db_i *right_dbip, const struct directory *left, const struct directory *ancestor, const struct directory *right, void *data),
	void *client_data);


/**
 * The flags parameter is a bitfield is used with db_compare() to
 * specify whether to report internal object parameter differences
 * (DB_COMPARE_PARAM), attribute differences (DB_COMPARE_ATTRS), or
 * everything (DB_COMPARE_ALL).
 */
typedef enum {
    DB_COMPARE_ALL=0x00,
    DB_COMPARE_PARAM=0x01,
    DB_COMPARE_ATTRS=0x02
} db_compare_criteria_t;

/**
 * Compare two database objects.
 *
 * This function is useful for comparing two geometry objects and
 * inspecting their differences.  Differences are recorded in
 * key=value form based on whether there is a difference added in the
 * right, removed in the right, changed going from left to right, or
 * unchanged.
 *
 * The flags parameter is a bitfield specifying what type of
 * comparisons to perform.  The default is to compare everything and
 * report on any differences encountered.
 *
 * The same bu_attribute_value_set container may be passed to any of
 * the provided containers to aggregate results.  NULL may be passed
 * to not inspect or record information for that type of comparison.
 *
 * Returns an int with bit flags set according to the above
 * four diff categories.
 *
 * Negative returns indicate an error.
 */
RT_EXPORT extern int
db_compare(struct bu_attribute_value_set *added,
	   struct bu_attribute_value_set *removed,
	   struct bu_attribute_value_set *changed_left,
	   struct bu_attribute_value_set *changed_right,
	   struct bu_attribute_value_set *unchanged,
	   const struct rt_db_internal *left_obj,
	   const struct rt_db_internal *right_obj,
	   db_compare_criteria_t flags,
	   const struct bn_tol *diff_tol);

/**
 * Compare the attribute sets.
 *
 * This function is useful for comparing the contents
 * of two attribute/value sets. Used by db_compare, this
 * function is also directly avaiable for processing sets
 * and attribute only objects that don't have an internal
 * representation */
RT_EXPORT extern int
db_avs_diff(struct bu_attribute_value_set *added,
            struct bu_attribute_value_set *removed,
            struct bu_attribute_value_set *changed_left,
            struct bu_attribute_value_set *changed_right,
            struct bu_attribute_value_set *unchanged,
	    const struct bu_attribute_value_set *left_set,
	    const struct bu_attribute_value_set *right_set,
            const struct bn_tol *diff_tol);


/**
 * Compare three database objects.
 *
 * The flags parameter is a bitfield specifying what type of
 * comparisons to perform.  The default is to compare everything and
 * report on any differences encountered.
 *
 * The same bu_attribute_value_set container may be passed to any of
 * the provided containers to aggregate results.  NULL may be passed
 * to not inspect or record information for that type of comparison.
 *
 * Returns an int with bit flags set according to the above
 * diff3 categories.
 *
 * Negative returns indicate an error.
 *
 * The various attribute/value sets contain the categorized
 * parameters.  The "merged" set contains the combined attributes
 * of all objects, with conflicts encoded according to the templates:
 *
 * CONFLICT(ANCESTOR):<avs name> , avs_value_ancestor
 * CONFLICT(LEFT):<avs name> , avs_value_left
 * CONFLICT(RIGHT):<avs name> , avs_value_right
 *
 * For cases where a value didn't exist, avs_value_* is replaced with
 * REMOVED.
 *
 */


RT_EXPORT extern int
db_compare3(struct bu_attribute_value_set *unchanged,
	struct bu_attribute_value_set *removed_left_only,
	struct bu_attribute_value_set *removed_right_only,
	struct bu_attribute_value_set *removed_both,
	struct bu_attribute_value_set *added_left_only,
	struct bu_attribute_value_set *added_right_only,
	struct bu_attribute_value_set *added_both,
	struct bu_attribute_value_set *added_conflict_left,
	struct bu_attribute_value_set *added_conflict_right,
	struct bu_attribute_value_set *changed_left_only,
	struct bu_attribute_value_set *changed_right_only,
	struct bu_attribute_value_set *changed_both,
	struct bu_attribute_value_set *changed_conflict_ancestor,
	struct bu_attribute_value_set *changed_conflict_left,
	struct bu_attribute_value_set *changed_conflict_right,
	struct bu_attribute_value_set *merged,
	const struct rt_db_internal *left,
	const struct rt_db_internal *ancestor,
	const struct rt_db_internal *right,
	db_compare_criteria_t flags,
	struct bn_tol *diff_tol);

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
