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
 * Compare two database instances.
 *
 * All objects in dbip_left are compared against the objects in
 * dbip_right.  Every object results in one of four callback functions
 * getting called.  Any objects in dbip_right but not in dbip_left
 * cause add_func() to get called.  Any objects in dbip_left but not
 * in dbip_right cause del_func() to get called.  Objects existing in
 * both (i.e., with the same name) cause chgd_func() to get called.
 * If the object is unchanged, unch_func() is called.  NULL may be
 * passed to skip any callback.
 *
 * The function returns 0 if there are no differences, or returns the
 * number of differences encountered.  Negative values indicate a
 * traversal failure.
 */
RT_EXPORT extern int
db_diff(const struct db_i *dbip_left,
	const struct db_i *dbip_right,
	int (*add_func)(const struct db_i *left, const struct db_i *right, const struct directory *added, void *data),
	int (*del_func)(const struct db_i *left, const struct db_i *right, const struct directory *removed, void *data),
	int (*chgd_func)(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data),
	int (*unch_func)(const struct db_i *left, const struct db_i *right, const struct directory *unchanged, void *data));

typedef enum {
    DB_COMPARE_ALL=0x00,
    DB_COMPARE_PARAM=0x01,
    DB_COMPARE_ATTRS 0x02
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
 * The same bu_attribute_value_set container may be passed to any of
 * the provided containers to aggregate results.  NULL may be passed
 * to not inspect or record information for that type of comparison.
 *
 * The flags parameter is a bitfield specifying whether to report
 * internal object parameter differences (DB_COMPARE_PARAM), attribute
 * differences (DB_COMPARE_ATTRS), or everything (DB_COMPARE_ALL).
 * The default is to report everything.
 *
 * This function returns 0 if there are no differences and non-0 if
 * there are differences.  Negative values indicate an internal error.
 */
RT_EXPORT extern int
db_compare(const struct rt_db_internal *left_obj,
	   const struct rt_db_internal *right_obj,
	   db_compare_criteria_t flags,
	   struct bu_attribute_value_set *added,
	   struct bu_attribute_value_set *removed,
	   struct bu_attribute_value_set *changed_left,
	   struct bu_attribute_value_set *changed_right,
	   struct bu_attribute_value_set *unchanged);


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
