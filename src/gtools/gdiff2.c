/*                    G D I F F 2 . C
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
#include "db_diff.h"

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
    if (!results_table) return;
    for (i = 0; i < (int)BU_PTBL_LEN(results_table); i++) {
	struct result_container *result = (struct result_container *)BU_PTBL_GET(results_table, i);
	result_free((void *)result);
    }
    bu_ptbl_free(results_table);
}

struct results {
    float diff_tolerance;
    struct bu_ptbl *added;     /* directory pointers */
    struct bu_ptbl *removed;   /* directory pointers */
    struct bu_ptbl *unchanged; /* directory pointers */
    struct bu_ptbl *changed;   /* result containers */
    struct bu_ptbl *changed_dbip1;   /* directory pointers */
    struct bu_ptbl *changed_dbip2;   /* directory pointers */
};

int
diff_added(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *added, void *data)
{
    struct results *results;
    if (!data || !added) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->added)) BU_PTBL_INIT(results->added);

    bu_ptbl_ins(results->added, (long *)added);

    return 0;
}

int
diff_removed(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *removed, void *data)
{
    struct results *results;
    if (!data || !removed) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->removed)) BU_PTBL_INIT(results->removed);

    bu_ptbl_ins(results->removed, (long *)removed);

    return 0;
}

int
diff_unchanged(const struct db_i *UNUSED(left), const struct db_i *UNUSED(right), const struct directory *unchanged, void *data)
{
    struct results *results;
    if (!data || !unchanged) return -1;
    results = (struct results *)data;
    if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);

    bu_ptbl_ins(results->unchanged, (long *)unchanged);

    return 0;
}

int
diff_changed(const struct db_i *left, const struct db_i *right, const struct directory *before, const struct directory *after, void *data)
{
    struct results *results;
    struct result_container *result;
    struct bn_tol diff_tol = {BN_TOL_MAGIC, VUNITIZE_TOL, 0.0, 0.0, 0.0};
    if (!left || !right || !before || !after|| !data) return -1;
    results = (struct results *)data;
    diff_tol.dist = results->diff_tolerance;
    if (!BU_PTBL_IS_INITIALIZED(results->changed)) BU_PTBL_INIT(results->changed);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_dbip1)) BU_PTBL_INIT(results->changed_dbip1);
    if (!BU_PTBL_IS_INITIALIZED(results->changed_dbip2)) BU_PTBL_INIT(results->changed_dbip2);
    bu_ptbl_ins(results->changed_dbip1, (long *)before);
    bu_ptbl_ins(results->changed_dbip2, (long *)after);

    result = (struct result_container *)bu_calloc(1, sizeof(struct result_container), "diff result struct");
    result_init(result);

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

    if (result->internal_diff || result->attribute_diff) {
	bu_ptbl_ins(results->changed, (long *)result);
    } else {
	if (!BU_PTBL_IS_INITIALIZED(results->unchanged)) BU_PTBL_INIT(results->unchanged);
	bu_ptbl_ins(results->unchanged, (long *)before);
	result_free(result);
    }
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

int
print_dp(struct bu_vls *diff_log, struct bu_ptbl *dptable, int cnt, const struct directory *dp, int line_len)
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

void
diff_summarize(struct bu_vls *diff_log, struct results *results, int verbosity, int r_added, int r_removed, int r_changed, int r_unchanged)
{
    if(verbosity >= 1) {
	int i = 0;
	struct bu_ptbl *added = results->added;
	struct bu_ptbl *removed = results->removed;
	struct bu_ptbl *changed = results->changed;
	struct bu_ptbl *unchanged = results->unchanged;

	if(verbosity == 1) {
	    int line_len = 0;
	    if (r_added > 0) {
		if ((int)BU_PTBL_LEN(added) > 0) {
		    bu_vls_printf(diff_log, "\nObjects added:\n");
		}
		for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		    line_len = print_dp(diff_log, added, i, dp, line_len);
		}
	    }
	    line_len = 0;
	    if (r_removed > 0) {
		if ((int)BU_PTBL_LEN(removed) > 0) {
		    bu_vls_printf(diff_log, "\nObjects removed:\n");
		}
		for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		    line_len = print_dp(diff_log, removed, i, dp, line_len);
		}
	    }
	    line_len = 0;
	    if (r_changed > 0) {
		if ((int)BU_PTBL_LEN(changed) > 0) {
		    bu_vls_printf(diff_log, "\nObjects changed:\n");
		}
		for (i = 0; i < (int)BU_PTBL_LEN(changed); i++) {
		    struct result_container *result = (struct result_container *)BU_PTBL_GET(changed, i);
		    const struct directory *dp = result->dp_orig;
		    if (!dp) dp = result->dp_new;
		    line_len = print_dp(diff_log, changed, i, dp, line_len);
		}
	    }
	    line_len = 0;
	    if (r_unchanged > 0) {
		if ((int)BU_PTBL_LEN(unchanged) > 0) {
		    bu_vls_printf(diff_log, "\nObjects unchanged:\n");
		}
		for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		    line_len = print_dp(diff_log, unchanged, i, dp, line_len);
		}
	    }

	}
	if(verbosity > 1) {
	    if (r_added > 0) {
		for (i = 0; i < (int)BU_PTBL_LEN(added); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(added, i);
		    bu_vls_printf(diff_log, "%s was added.\n\n", dp->d_namep);
		}
	    }

	    if (r_removed > 0) {
		for (i = 0; i < (int)BU_PTBL_LEN(removed); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(removed, i);
		    bu_vls_printf(diff_log, "%s was removed.\n\n", dp->d_namep);
		}
	    }

	    if (r_changed > 0) {
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
	    if (r_unchanged > 0) {
		for (i = 0; i < (int)BU_PTBL_LEN(unchanged); i++) {
		    struct directory *dp = (struct directory *)BU_PTBL_GET(unchanged, i);
		    bu_vls_printf(diff_log, "%s was unchanged.\n\n", dp->d_namep);
		}
	    }


	}
	bu_vls_printf(diff_log, "\n");
    }
}

void
gdiff_usage(const char *str) {
    bu_log("Usage: %s [-acmrv] file1.g file2.g\n", str);
    bu_exit(1, NULL);
}

int
main(int argc, char **argv)
{
    int c;
    int return_added = -1;
    int return_removed = -1;
    int return_changed = -1;
    int return_unchanged = 0;
    int diff_return = 0;
    int verbosity = 2;
    int output_mode = 0;
    int have_diff = 0;
    int have_search_filter = 0;
    float diff_tolerance = VUNITIZE_TOL;
    struct results results;
    struct db_i *dbip1 = DBI_NULL;
    struct db_i *dbip2 = DBI_NULL;
    struct bu_vls diff_log = BU_VLS_INIT_ZERO;
    const char *diff_prog_name = argv[0];
    struct bu_vls search_filter = BU_VLS_INIT_ZERO;

    while ((c = bu_getopt(argc, argv, "acF:mrt:uv:h?")) != -1) {
	switch (c) {
	    case 'a':
		return_added = 1;
		break;
	    case 'r':
		return_removed = 1;
		break;
	    case 'c':
		return_changed = 1;
		break;
	    case 'u':
		return_unchanged = 1;
		break;
	    case 'F':
		have_search_filter = 1;
		bu_vls_sprintf(&search_filter, "%s", bu_optarg);
		break;
	    case 'm':   /* mged readable */
		output_mode = 109;  /* use ascii decimal value for 'm' to signify mged mode */
		break;
	    case 't':   /* distance tolerance for same/different decisions (VUNITIZE_TOL is default) */
		if(sscanf(bu_optarg, "%f", &diff_tolerance) != 1) {
		    bu_log("Invalid distance tolerance specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    case 'v':   /* verbosity (2 is default) */
		if(sscanf(bu_optarg, "%d", &verbosity) != 1) {
		    bu_log("Invalid verbosity specification: '%s'\n", bu_optarg);
		    gdiff_usage(diff_prog_name);
		    bu_exit (1, NULL);
		}
		break;
	    default:
		gdiff_usage(diff_prog_name);
	}
    }

    if (output_mode == 109) {
	bu_log("Error - mged script generation not yet implemented\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (return_added == -1 && return_removed == -1 && return_changed == -1 && return_unchanged == 0) {
	return_added = 1; return_removed = 1; return_changed = 1;
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc != 2) {
	bu_log("Error - please specify two .g files\n");
	bu_exit(EXIT_FAILURE, NULL);
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[0]);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "Cannot stat file %s\n", argv[1]);
    }

    if ((dbip1 = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[0]);
    }
    RT_CK_DBI(dbip1);
    if (db_dirbuild(dbip1) < 0) {
	db_close(dbip1);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }

    if ((dbip2 = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
    }
    RT_CK_DBI(dbip2);
    if (db_dirbuild(dbip2) < 0) {
	db_close(dbip1);
	db_close(dbip2);
	bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
    }

    results.diff_tolerance = diff_tolerance;

    BU_GET(results.added, struct bu_ptbl);
    BU_GET(results.removed, struct bu_ptbl);
    BU_GET(results.changed, struct bu_ptbl);
    BU_GET(results.changed_dbip1, struct bu_ptbl);
    BU_GET(results.changed_dbip2, struct bu_ptbl);
    BU_GET(results.unchanged, struct bu_ptbl);

    BU_PTBL_INIT(results.added);
    BU_PTBL_INIT(results.removed);
    BU_PTBL_INIT(results.changed);
    BU_PTBL_INIT(results.changed_dbip1);
    BU_PTBL_INIT(results.changed_dbip2);
    BU_PTBL_INIT(results.unchanged);

    have_diff = db_diff(dbip1, dbip2, &diff_added, &diff_removed, &diff_changed, &diff_unchanged, (void *)&results);

    /* Now we have our diff results, time to filter (if applicable) and report them */
    if (have_search_filter) {
	int i = 0;
	struct bu_ptbl added_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl removed_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl unchanged_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl changed_filtered_dbip1 = BU_PTBL_INIT_ZERO;
	struct bu_ptbl changed_filtered_dbip2 = BU_PTBL_INIT_ZERO;
	struct bu_ptbl changed_filtered = BU_PTBL_INIT_ZERO;

	/* Filter added objects */
	if (BU_PTBL_LEN(results.added) > 0) {
	    (void)db_search(&added_filtered, DB_SEARCH_FLAT, (const char *)bu_vls_addr(&search_filter), BU_PTBL_LEN(results.added), (struct directory **)results.added->buffer, dbip2);
	    bu_ptbl_reset(results.added);
	    bu_ptbl_cat(results.added, &added_filtered);
	    db_search_free(&added_filtered);
	}

	/* Filter removed objects */
	if (BU_PTBL_LEN(results.removed) > 0) {
	    (void)db_search(&removed_filtered, DB_SEARCH_FLAT, (const char *)bu_vls_addr(&search_filter), BU_PTBL_LEN(results.removed), (struct directory **)results.removed->buffer, dbip1);
	    bu_ptbl_reset(results.removed);
	    bu_ptbl_cat(results.removed, &removed_filtered);
	    db_search_free(&removed_filtered);
	}

	/* Filter changed objects */
	if (BU_PTBL_LEN(results.changed_dbip1) > 0) {
	    (void)db_search(&changed_filtered_dbip1, DB_SEARCH_FLAT, (const char *)bu_vls_addr(&search_filter), BU_PTBL_LEN(results.changed_dbip1), (struct directory **)results.changed_dbip1->buffer, dbip1);
	}
	if (BU_PTBL_LEN(results.changed_dbip2) > 0) {
	    (void)db_search(&changed_filtered_dbip2, DB_SEARCH_FLAT, (const char *)bu_vls_addr(&search_filter), BU_PTBL_LEN(results.changed_dbip2), (struct directory **)results.changed_dbip2->buffer, dbip2);
	}
	if (BU_PTBL_LEN(&changed_filtered_dbip1) > 0 || BU_PTBL_LEN(&changed_filtered_dbip2) > 0) {
	    for (i = 0; i < (int)BU_PTBL_LEN(results.changed); i++) {
		struct result_container *result = (struct result_container *)BU_PTBL_GET(results.changed, i);
		if ((result->dp_orig && bu_ptbl_locate(&changed_filtered_dbip1, (long *)result->dp_orig) != -1) ||
			(result->dp_new && bu_ptbl_locate(&changed_filtered_dbip2, (long *)result->dp_new) != -1)) {
		    bu_ptbl_ins(&changed_filtered, (long *)result);
		} else {
		    result_free((void *)result);
		}
	    }
	    bu_ptbl_reset(results.changed);
	    bu_ptbl_cat(results.changed, &changed_filtered);
	} else {
	    for (i = 0; i < (int)BU_PTBL_LEN(results.changed); i++) {
		struct result_container *result = (struct result_container *)BU_PTBL_GET(results.changed, i);
		result_free((void *)result);
	    }
	    bu_ptbl_reset(results.changed);
	}
	if (BU_PTBL_LEN(results.changed_dbip1) > 0) {
	    db_search_free(&changed_filtered_dbip1);
	}
	if (BU_PTBL_LEN(results.changed_dbip2) > 0) {
	    db_search_free(&changed_filtered_dbip2);
	}

	/* Filter unchanged objects */
	if (BU_PTBL_LEN(results.unchanged) > 0) {
	    (void)db_search(&unchanged_filtered, DB_SEARCH_FLAT, (const char *)bu_vls_addr(&search_filter), BU_PTBL_LEN(results.unchanged), (struct directory **)results.unchanged->buffer, dbip1);
	    bu_ptbl_reset(results.unchanged);
	    bu_ptbl_cat(results.unchanged, &unchanged_filtered);
	    db_search_free(&unchanged_filtered);
	}
    }

    if (verbosity) {
	if (!have_diff) {
	    bu_log("No differences found.\n");
	} else {
	    diff_summarize(&diff_log, &results, verbosity, return_added, return_removed, return_changed, return_unchanged);
	    bu_log("%s", bu_vls_addr(&diff_log));
	}
    }
    if (have_diff) {
	if (return_added > 0) diff_return += BU_PTBL_LEN(results.added);
	if (return_removed > 0) diff_return += BU_PTBL_LEN(results.removed);
	if (return_changed > 0) diff_return += BU_PTBL_LEN(results.changed);
	if (return_unchanged > 0) diff_return += BU_PTBL_LEN(results.unchanged);
    }

    bu_ptbl_free(results.added);
    bu_ptbl_free(results.removed);
    bu_ptbl_free(results.unchanged);
    bu_ptbl_free(results.changed_dbip1);
    bu_ptbl_free(results.changed_dbip2);
    bu_free(results.added, "free table");
    bu_free(results.removed, "free table");
    bu_free(results.unchanged, "free table");
    result_free_ptbl(results.changed);
    bu_free(results.changed, "free table");
    bu_free(results.changed_dbip1, "free_table");
    bu_free(results.changed_dbip2, "free_table");
    bu_vls_free(&search_filter);

    db_close(dbip1);
    db_close(dbip2);
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
