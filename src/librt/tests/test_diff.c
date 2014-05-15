/*                    T E S T _ D I F F . C
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

#include "raytrace.h"
#include "rt/db_diff.h"

struct result_container {
    int status;
    const struct directory *dp_orig;
    const struct directory *dp_new;
    struct rt_db_internal *intern_orig;
    struct rt_db_internal *intern_new;
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

void
result_init(struct result_container *result)
{
    result->status = 0;
    result->internal_diff = 0;
    result->attribute_diff = 0;
    result->dp_orig = NULL;
    result->dp_new = NULL;
    result->intern_orig = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_orig");
    result->intern_new = (struct rt_db_internal *)bu_calloc(1, sizeof(struct rt_db_internal), "intern_new");
    RT_DB_INTERNAL_INIT(result->intern_orig);
    RT_DB_INTERNAL_INIT(result->intern_new);
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
result_free(void *result)
{
    struct result_container *curr_result = (struct result_container *)result;
    if (!result) return;

    rt_db_free_internal(curr_result->intern_orig);
    rt_db_free_internal(curr_result->intern_new);
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

void
result_free_ptbl(struct bu_ptbl *results_table)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	struct result_container *result = (struct result_container *)BU_PTBL_GET(results_table, i);
	result_free((void *)result);
    }
    bu_ptbl_free(results_table);
}

struct results {
    struct bu_ptbl added;     /* directory pointers */
    struct bu_ptbl removed;   /* directory pointers */
    struct bu_ptbl unchanged; /* directory pointers */
    struct bu_ptbl changed;   /* result containers */
};

int
diff_added(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *added, void *data)
{
    struct results *results;
    if (!data || !added) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(&results->added)) BU_PTBL_INIT(&results->added);

    bu_ptbl_ins(&results->added, (long *)added);

    return 0;
}

int
diff_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *removed, void *data)
{
    struct results *results;
    if (!data || !removed) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(&results->removed)) BU_PTBL_INIT(&results->removed);

    bu_ptbl_ins(&results->removed, (long *)removed);

    return 0;
}

int
diff_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *unchanged, void *data)
{
    struct results *results;
    if (!data || !unchanged) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(&results->unchanged)) BU_PTBL_INIT(&results->unchanged);

    bu_ptbl_ins(&results->unchanged, (long *)unchanged);

    return 0;
}

int
diff_changed(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data)
{
    struct results *results;
    struct result_container *result;
    struct bn_tol diff_tol = {BN_TOL_MAGIC, 0.0, 0.0, 0.0, 0.0};
    if (!left || !right || !before || !after|| !data) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(&results->changed)) BU_PTBL_INIT(&results->changed);

    result = (struct result_container *)bu_calloc(1, sizeof(struct result_container), "diff result struct");
    result_init(result);
    bu_ptbl_ins(&results->changed, (long *)result);

    result->dp_orig = before;
    result->dp_new = after;

    /* Get the internal objects */
    if (rt_db_get_internal(result->intern_orig, before, left, (fastf_t *)NULL, &rt_uniresource) < 0) {
	result->status = 1;
	return -1;
    }
    if (rt_db_get_internal(result->intern_new, after, right, (fastf_t *)NULL, &rt_uniresource) < 0) {
	result->status = 1;
	return -1;
    }

    result->internal_diff = db_compare(result->intern_orig, result->intern_new, DB_COMPARE_PARAM,
	    &(result->internal_new_only), &(result->internal_orig_only), &(result->internal_orig_diff),
	    &(result->internal_new_diff), &(result->internal_shared), &diff_tol);

    result->attribute_diff = db_compare(result->intern_orig, result->intern_new, DB_COMPARE_ATTRS,
	    &(result->additional_new_only), &(result->additional_orig_only), &(result->additional_orig_diff),
	    &(result->additional_new_diff), &(result->additional_shared), &diff_tol);

    return 0;
}


void
params_summary(struct bu_vls *attr_log, struct result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    struct bu_attribute_value_set *orig_only = &result->internal_orig_only;
    struct bu_attribute_value_set *new_only = &result->internal_new_only;
    struct bu_attribute_value_set *orig_diff = &result->internal_orig_diff;
    struct bu_attribute_value_set *new_diff = &result->internal_new_diff;
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


void
attrs_summary(struct bu_vls *attr_log, struct result_container *result)
{
    struct bu_attribute_value_pair *avpp;
    struct bu_attribute_value_set *orig_only = &result->additional_orig_only;
    struct bu_attribute_value_set *new_only = &result->additional_new_only;
    struct bu_attribute_value_set *orig_diff = &result->additional_orig_diff;
    struct bu_attribute_value_set *new_diff = &result->additional_new_diff;
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

void
diff_summarize(struct bu_vls *diff_log, struct results *results)
{
    int i = 0;

    struct bu_ptbl *added = &results->added;
    struct bu_ptbl *removed = &results->removed;
    struct bu_ptbl *changed = &results->changed;

    for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
	bu_vls_printf(diff_log, "%s was added.\n\n", dp->d_namep);
    }

    for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
	bu_vls_printf(diff_log, "%s was removed.\n\n", dp->d_namep);
    }

    for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
	struct result_container *result = (struct result_container *)BU_PTBL_GET(changed, i);
	const struct directory *dp = result->dp_orig;
	if (!dp) dp = result->dp_new;
	bu_vls_printf(diff_log, "%s was changed:\n", dp->d_namep);
	params_summary(diff_log, result);
	attrs_summary(diff_log, result);
	bu_vls_printf(diff_log, "\n");
    }
}

int
main(int argc, char **argv)
{
    int have_diff = 0;
    struct results results;
    struct db_i *dbip1 = DBI_NULL;
    struct db_i *dbip2 = DBI_NULL;
    struct bu_vls diff_log = BU_VLS_INIT_ZERO;

    BU_PTBL_INIT(&results.added);
    BU_PTBL_INIT(&results.removed);
    BU_PTBL_INIT(&results.changed);
    BU_PTBL_INIT(&results.unchanged);

    if (argc != 3) {
	bu_log("Error - please specify two .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if (!bu_file_exists(argv[2], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[2]);
    }

    if ((dbip1 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip1);
    if (db_dirbuild(dbip1) < 0) {
	db_close(dbip1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }

    if ((dbip2 = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
    }
    RT_CK_DBI(dbip2);
    if (db_dirbuild(dbip2) < 0) {
	db_close(dbip1);
	db_close(dbip2);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
    }


    have_diff = db_diff(dbip1, dbip2, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)&results);
    if (!have_diff) {
	bu_log("No differences found.\n");
	return 0;
    } else {
	diff_summarize(&diff_log, &results);
	bu_log("%s", bu_vls_addr(&diff_log));
    }

    bu_ptbl_free(&results.added);
    bu_ptbl_free(&results.removed);
    bu_ptbl_free(&results.unchanged);
    result_free_ptbl(&results.changed);

    db_close(dbip1);
    db_close(dbip2);
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
