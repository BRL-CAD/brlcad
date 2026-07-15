/*                         M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
#include "bu/app.h"
#include "bu/cmdschema.h"
#include "./gdiff.h"
}


/* Keep command-line storage independent from the long-lived diff and grouping
 * state.  The schema owns this record completely; after parsing, main copies
 * the selected values into the execution records appropriate to the selected
 * mode. */
struct gdiff_args {
    int print_help;
    int return_added;
    int return_removed;
    int return_changed;
    int return_unchanged;
    struct bu_vls search_filter;
    struct bu_vls merge_file;
    fastf_t diff_tolerance;
    long verbosity;
    long quiet;
    int grouping_mode;
    long filename_threshold;
    long threshold;
    int use_names;
    int use_geometry;
    int geom_fast;
    struct bu_vls file_pattern;
    struct bu_vls hash_infile;
    struct bu_vls hash_outfile;
    int path_display_mode;
};


static int
gdiff_path_display_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    int value;

    if (BU_STR_EQUAL(arg, "auto")) {
	value = 0;
    } else if (BU_STR_EQUAL(arg, "relative")) {
	value = GDIFF_PATH_DISPLAY_RELATIVE;
    } else if (BU_STR_EQUAL(arg, "absolute")) {
	value = GDIFF_PATH_DISPLAY_ABSOLUTE;
    } else {
	if (msg)
	    bu_vls_printf(msg, "invalid value '%s' for path display option; expected auto, relative, or absolute\n", arg ? arg : "");
	return -1;
    }

    if (storage)
	*((int *)storage) = value;
    return 0;
}


static const struct bu_cmd_option gdiff_options[] = {
    BU_CMD_FLAG("h", "help", gdiff_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("a", "added", gdiff_args, return_added, "Report added objects"),
    BU_CMD_FLAG("d", "deleted", gdiff_args, return_removed, "Report deleted objects"),
    BU_CMD_FLAG("m", "modified", gdiff_args, return_changed, "Report modified objects"),
    BU_CMD_FLAG("u", "unchanged", gdiff_args, return_unchanged, "Report unchanged objects"),
    BU_CMD_VLS_APPEND("F", "filter", gdiff_args, search_filter, "string", "Specify filter to use on results"),
    BU_CMD_VLS_APPEND("M", "merge-file", gdiff_args, merge_file, "merge.g", "Specify merge file"),
    BU_CMD_NUMBER("t", "tolerance", gdiff_args, diff_tolerance, "distance", "Numerical distance tolerance for same/different decisions (RT_LEN_TOL is default)"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbosity", gdiff_args, verbosity, "Increase output verbosity; repeat for more detail"),
    BU_CMD_COUNTING_LONG_FLAG("q", "quiet", gdiff_args, quiet, "Decrease output verbosity; repeat for less detail"),
    BU_CMD_FLAG("G", "group", gdiff_args, grouping_mode, "Group files into similar bins; grouping options refine the comparison"),
    BU_CMD_LONG(NULL, "filename-threshold", gdiff_args, filename_threshold, "distance", "Use filenames within this edit distance to establish top-level groupings"),
    BU_CMD_LONG(NULL, "threshold", gdiff_args, threshold, "distance", "Grouping tolerance (larger values produce looser groups; default is 30)"),
    BU_CMD_FLAG(NULL, "use-objnames", gdiff_args, use_names, "Incorporate object names into similarity checking"),
    BU_CMD_FLAG(NULL, "use-geometry", gdiff_args, use_geometry, "Incorporate object geometry into similarity checking"),
    BU_CMD_FLAG(NULL, "fast-geometry", gdiff_args, geom_fast, "Use a faster, more sensitive geometry difference test"),
    BU_CMD_VLS_APPEND("P", "file-pattern", gdiff_args, file_pattern, "pattern", "Pattern for files below a root directory (default: *.g)"),
    BU_CMD_VLS_APPEND(NULL, "read-hashes", gdiff_args, hash_infile, "file", "Read reusable precomputed hashes (grouping mode only)"),
    BU_CMD_VLS_APPEND(NULL, "write-hashes", gdiff_args, hash_outfile, "file", "Write computed hashes (grouping mode only)"),
    BU_CMD_CUSTOM(NULL, "display-paths", gdiff_args, path_display_mode, gdiff_path_display_parse, "auto|relative|absolute", "Display report paths as automatic, relative, or absolute"),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_operand gdiff_operands[] = {
    BU_CMD_OPERAND("input", BU_CMD_VALUE_FILE, 0, BU_CMD_COUNT_UNLIMITED,
	"Two or three database files, or grouping roots/patterns with --group", NULL),
    BU_CMD_OPERAND_NULL
};


static const struct bu_cmd_schema gdiff_schema = {
    "gdiff",
    "Compare geometry databases or group geometry files by similarity.",
    gdiff_options,
    gdiff_operands,
    BU_CMD_PARSE_INTERSPERSED,
    NULL
};


static void
gdiff_args_init(struct gdiff_args *args, const struct diff_state *state)
{
    *args = {};
    args->return_added = state->return_added;
    args->return_removed = state->return_removed;
    args->return_changed = state->return_changed;
    args->return_unchanged = state->return_unchanged;
    args->diff_tolerance = state->diff_tol->dist;
    args->verbosity = state->verbosity;
    args->filename_threshold = -1;
    args->threshold = -1;
    bu_vls_init(&args->search_filter);
    bu_vls_init(&args->merge_file);
    bu_vls_init(&args->file_pattern);
    bu_vls_init(&args->hash_infile);
    bu_vls_init(&args->hash_outfile);
}


static void
gdiff_args_free(struct gdiff_args *args)
{
    bu_vls_free(&args->search_filter);
    bu_vls_free(&args->merge_file);
    bu_vls_free(&args->file_pattern);
    bu_vls_free(&args->hash_infile);
    bu_vls_free(&args->hash_outfile);
}

/*******************************************************************/
/* Primary function for basic diff operation on two .g files */
/*******************************************************************/
static int
do_diff(struct db_i *left_dbip, struct db_i *right_dbip, struct diff_state *state) {
    int i = 0;
    int diff_state = DIFF_EMPTY;
    struct bu_ptbl results;
    BU_PTBL_INIT(&results);

    diff_state = db_diff(left_dbip, right_dbip, state->diff_tol, DB_COMPARE_ALL, &results);

    /* Now we have our diff results, time to filter (if applicable) and report them */
    if (state->have_search_filter) {
	int s_flags = 0;
	int filtered_diff_state = DIFF_EMPTY;
	struct bu_ptbl results_filtered;

	/* In order to respect search filters involving depth, we have to build "allowed"
	 * sets of objects from both databases based on straight-up search filtering of
	 * the original databases.  We then check the filtered diff results against the
	 * search-filtered original databases to make sure they are valid for the final
	 * results-> */
	struct bu_ptbl left_dbip_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl right_dbip_filtered = BU_PTBL_INIT_ZERO;
	s_flags |= DB_SEARCH_HIDDEN;
	s_flags |= DB_SEARCH_RETURN_UNIQ_DP;
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL, NULL, NULL);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL, NULL, NULL);

	BU_PTBL_INIT(&results_filtered);
	for (i = 0; i < (int)BU_PTBL_LEN(&results); i++) {
	    struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&results, i);
	    // Default to pass - if an object doesn't exist on one side, we want to pass/fail
	    // based on the object we have.
	    int lpass = 1;
	    int rpass = 1;
	    // If both sides pass the filters, it's in - otherwise it's out
	    if (dr->dp_left != RT_DIR_NULL) {
		lpass = (bu_ptbl_locate(&left_dbip_filtered,  (long *)dr->dp_left ) != -1);
	    }
	    if (dr->dp_right != RT_DIR_NULL) {
		rpass = (bu_ptbl_locate(&right_dbip_filtered, (long *)dr->dp_right) != -1);
	    }
	    if (lpass && rpass) {
		bu_ptbl_ins(&results_filtered, (long *)dr);
		filtered_diff_state |= dr->param_state;
		filtered_diff_state |= dr->attr_state;
	    } else {
		diff_free_result(dr);
		BU_PUT(dr, struct diff_result);
	    }
	}

	bu_ptbl_reset(&results);
	bu_ptbl_cat(&results, &results_filtered);
	diff_state = filtered_diff_state;
	bu_ptbl_free(&results_filtered);
    }

    if (state->verbosity) {
	if (diff_state == DIFF_UNCHANGED || diff_state == DIFF_EMPTY) {
	    bu_log("No differences found.\n");
	} else {
	    diff_summarize(state->diff_log, &results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }

    if (state->merge) {
	struct bu_ptbl diff3_results;
	struct db_i *inmem_dbip;
	BU_PTBL_INIT(&diff3_results);
	inmem_dbip = db_open_inmem();

	(void)db_diff3(left_dbip, inmem_dbip, right_dbip, state->diff_tol, DB_COMPARE_ALL, &diff3_results);

	/* Now we have our diff results, time to filter (if applicable) and report them */
	if (state->have_search_filter) {
	    int s_flags = 0;
	    struct bu_ptbl diff3_results_filtered;
	    /* In order to respect search filters involving depth, we have to build "allowed"
	     * sets of objects from both databases based on straight-up search filtering of
	     * the original databases.  We then check the filtered diff results against the
	     * search-filtered original databases to make sure they are valid for the final
	     * results-> */
	    struct bu_ptbl left_dbip3_filtered = BU_PTBL_INIT_ZERO;
	    struct bu_ptbl right_dbip3_filtered = BU_PTBL_INIT_ZERO;
	    s_flags |= DB_SEARCH_HIDDEN;
	    s_flags |= DB_SEARCH_RETURN_UNIQ_DP;

	    (void)db_search(&left_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL, NULL, NULL);
	    (void)db_search(&right_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL, NULL, NULL);

	    BU_PTBL_INIT(&diff3_results_filtered);
	    for (i = 0; i < (int)BU_PTBL_LEN(&diff3_results); i++) {
		struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&diff3_results, i);
		if ((dr->dp_left  != RT_DIR_NULL && (bu_ptbl_locate(&left_dbip3_filtered,  (long *)dr->dp_left)  != -1)) ||
	            (dr->dp_right != RT_DIR_NULL && (bu_ptbl_locate(&right_dbip3_filtered, (long *)dr->dp_right) != -1))) {
		    bu_ptbl_ins(&diff3_results_filtered, (long *)dr);
		} else {
		    diff_free_result(dr);
		    BU_PUT(dr, struct diff_result);
		}
	    }

	    bu_ptbl_reset(&diff3_results);
	    bu_ptbl_cat(&diff3_results, &diff3_results_filtered);
	    bu_ptbl_free(&diff3_results_filtered);
	}

	(void)diff3_merge(left_dbip, inmem_dbip, right_dbip, state, &diff3_results);

	db_close(inmem_dbip);
    }

    for (i = 0; i < (int)BU_PTBL_LEN(&results); i++) {
	struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&results, i);
	diff_free_result(dr);
	BU_PUT(dr, struct diff_result);
    }
    bu_ptbl_free(&results);

    return diff_state;
}

static int
do_diff3(struct db_i *left_dbip, struct db_i *ancestor_dbip, struct db_i *right_dbip, struct diff_state *state) {
    int diff3_state = DIFF_EMPTY;
    struct bu_ptbl results;
    BU_PTBL_INIT(&results);

    diff3_state = db_diff3(left_dbip, ancestor_dbip, right_dbip, state->diff_tol, DB_COMPARE_ALL, &results);

    /* Now we have our diff results, time to filter (if applicable) and report them */
    if (state->have_search_filter) {
	int i = 0;
	int s_flags = 0;
	int filtered_diff_state = DIFF_EMPTY;
	struct bu_ptbl results_filtered;

	/* In order to respect search filters involving depth, we have to build "allowed"
	 * sets of objects from both databases based on straight-up search filtering of
	 * the original databases.  We then check the filtered diff results against the
	 * search-filtered original databases to make sure they are valid for the final
	 * results-> */
	struct bu_ptbl left_dbip_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl ancestor_dbip_filtered = BU_PTBL_INIT_ZERO;
	struct bu_ptbl right_dbip_filtered = BU_PTBL_INIT_ZERO;
	s_flags |= DB_SEARCH_HIDDEN;
	s_flags |= DB_SEARCH_RETURN_UNIQ_DP;
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL, NULL, NULL);
	(void)db_search(&ancestor_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL, NULL, NULL);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL, NULL, NULL);

	BU_PTBL_INIT(&results_filtered);
	for (i = 0; i < (int)BU_PTBL_LEN(&results); i++) {
	    struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&results, i);
	    if ((dr->dp_left     != RT_DIR_NULL && (bu_ptbl_locate(&left_dbip_filtered,     (long *)dr->dp_left)     != -1)) ||
		(dr->dp_ancestor != RT_DIR_NULL && (bu_ptbl_locate(&ancestor_dbip_filtered, (long *)dr->dp_ancestor) != -1)) ||
		(dr->dp_right    != RT_DIR_NULL && (bu_ptbl_locate(&right_dbip_filtered,    (long *)dr->dp_right)    != -1))) {
		bu_ptbl_ins(&results_filtered, (long *)dr);
		filtered_diff_state |= dr->param_state;
		filtered_diff_state |= dr->attr_state;
	    } else {
		diff_free_result(dr);
		BU_PUT(dr, struct diff_result);
	    }
	}

	bu_ptbl_reset(&results);
	bu_ptbl_cat(&results, &results_filtered);
	diff3_state = filtered_diff_state;
	bu_ptbl_free(&results_filtered);
    }

    if (state->verbosity) {
	if (diff3_state == DIFF_UNCHANGED || diff3_state == DIFF_EMPTY) {
	    bu_log("No differences found.\n");
	} else {
	    diff3_summarize(state->diff_log, &results, state);
	    bu_log("%s", bu_vls_addr(state->diff_log));
	}
    }

    if (state->merge) {
	(void)diff3_merge(left_dbip, ancestor_dbip, right_dbip, state, &results);
    }

    bu_ptbl_free(&results);

    return diff3_state;
}

static void
gdiff_usage(const char *cmd)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_cmd_schema_describe(&gdiff_schema);
    bu_vls_sprintf(&str, "Usage: %s [options] left.g [ancestor.g] right.g\n", cmd);
    bu_vls_printf(&str,  "Usage: %s -G [options] [pattern1 pattern2 ...]\n", cmd);
    if (option_help) {
        bu_vls_printf(&str, "Options:\n%s\n", option_help);
        bu_free(option_help, "help str");
    }
    bu_vls_printf(&str, "\nWhen in grouping (-G) mode, by default both object names and\n");
    bu_vls_printf(&str, "object geometry are used unless an option is explicitly specified\n");
    bu_vls_printf(&str, "to enable one of them - in that case, only what is specified is used.\n");
    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}

int
main(int argc, const char **argv)
{
    int diff_return = 0;
    struct diff_state *state;
    struct db_i *left_dbip = DBI_NULL;
    struct db_i *right_dbip = DBI_NULL;
    struct db_i *ancestor_dbip = DBI_NULL;
    const char *diff_prog_name = argv[0];
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct gdiff_group_opts gopts = GDIFF_GROUP_OPTS_DEFAULT;
    struct gdiff_args args = {};
    int parsed = 0;
    int operand_count = 0;
    const char **operands = NULL;

    bu_setprogname(argv[0]);

    // Name stashed, move on
    argc--; argv++;

    BU_GET(state, struct diff_state);
    diff_state_init(state);
    gdiff_args_init(&args, state);

    parsed = bu_cmd_schema_parse(&gdiff_schema, &args, &msg, argc, argv);
    if (parsed < 0) {
        bu_log("%s\n", bu_vls_cstr(&msg));
        bu_vls_free(&msg);
	gdiff_args_free(&args);
        bu_vls_free(state->search_filter);
        bu_vls_free(state->merge_file);
	BU_PUT(state, struct diff_state);
        return BRLCAD_ERROR;
    }
    if (args.print_help) {
	gdiff_usage(diff_prog_name);
        bu_vls_free(&msg);
	gdiff_args_free(&args);
        bu_vls_free(state->search_filter);
        bu_vls_free(state->merge_file);
	BU_PUT(state, struct diff_state);
        return BRLCAD_OK;
    }

    operand_count = argc - parsed;
    operands = argv + parsed;
    state->return_added = args.return_added;
    state->return_removed = args.return_removed;
    state->return_changed = args.return_changed;
    state->return_unchanged = args.return_unchanged;
    state->diff_tol->dist = args.diff_tolerance;
    state->verbosity = args.verbosity;
    state->quiet = args.quiet;
    bu_vls_sprintf(state->search_filter, "%s", bu_vls_cstr(&args.search_filter));
    bu_vls_sprintf(state->merge_file, "%s", bu_vls_cstr(&args.merge_file));

    // If we have a non-negative group_threshold, we're doing a different
    // comparison.
    if (args.grouping_mode) {

	/* Carry verbosity setting into grouping mode */
	gopts.verbosity = state->verbosity;
	gopts.filename_threshold = args.filename_threshold;
	gopts.threshold = args.threshold;
	gopts.use_names = args.use_names;
	gopts.use_geometry = args.use_geometry;
	gopts.geom_fast = args.geom_fast;
	gopts.path_display_mode = args.path_display_mode;
	bu_vls_sprintf(&gopts.fpattern, "%s", bu_vls_cstr(&args.file_pattern));
	bu_vls_sprintf(&gopts.hash_infile, "%s", bu_vls_cstr(&args.hash_infile));
	bu_vls_sprintf(&gopts.hash_outfile, "%s", bu_vls_cstr(&args.hash_outfile));

	// Clean up standard gdiff processing structures
	bu_vls_free(&msg);
	bu_vls_free(state->search_filter);
	bu_vls_free(state->merge_file);
	gdiff_args_free(&args);
	BU_PUT(state, struct diff_state);

	// Run grouping mode
	int group_ret = gdiff_group(operand_count, operands, &gopts);
	bu_vls_free(&gopts.fpattern);
	bu_vls_free(&gopts.hash_infile);
	bu_vls_free(&gopts.hash_outfile);
	return group_ret;
    }

    if (bu_vls_strlen(state->search_filter)) {
	state->have_search_filter = 1;
    }
    if (bu_vls_strlen(state->merge_file)) {
	state->merge = 1;
    }

    state->verbosity = state->verbosity - state->quiet;

    if (state->verbosity > 4) {
	state->verbosity = 4;
    }

    state->return_conflicts = 1;

    if (state->return_added == -1 && state->return_removed == -1 && state->return_changed == -1 && state->return_unchanged == 0) {
	state->return_added = 1; state->return_removed = 1; state->return_changed = 1;
    }

    argc = operand_count;
    argv = operands;

    if (argc != 2 && argc != 3) {
	bu_log("Error - please specify either two or three .g files\n");
	bu_exit(-1, NULL);
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(-1, "Cannot stat file %s\n", argv[0]);
    }

    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(-1, "Cannot stat file %s\n", argv[1]);
    }

    if (argc == 3 && !bu_file_exists(argv[2], NULL)) {
	bu_exit(-1, "Cannot stat file %s\n", argv[2]);
    }

    if (state->merge && bu_file_exists(bu_vls_addr(state->merge_file), NULL)) {
	bu_exit(-1, "File %s already exists.\n", bu_vls_addr(state->merge_file));
    }

    /* diff case */
    if (argc == 2) {

	if (bu_file_same(argv[0], argv[1])) {
	    bu_exit(-1, "Same database file specified as both left and right diff inputs: %s\n", argv[0]);
	}

	if ((left_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[0]);
	}
	RT_CK_DBI(left_dbip);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}
	db_update_nref(left_dbip);

	if ((right_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(right_dbip);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(right_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}
	db_update_nref(right_dbip);

	diff_return = do_diff(left_dbip, right_dbip, state);
    }

    /* diff3 case */
    if (argc == 3) {

	if (bu_file_same(argv[0], argv[1]) && bu_file_same(argv[2], argv[1])) {
	    bu_exit(-1, "Same database file specified for all three inputs: %s\n", argv[0]);
	}

	if (bu_file_same(argv[0], argv[1])) {
	    bu_exit(-1, "Same database file specified as both ancestor and left diff inputs: %s\n", argv[0]);
	}

	if (bu_file_same(argv[1], argv[2])) {
	    bu_exit(-1, "Same database file specified as both ancestor and right diff inputs: %s\n", argv[1]);
	}

	if (bu_file_same(argv[0], argv[2])) {
	    bu_exit(-1, "Same database file specified as both left and right diff inputs: %s\n", argv[0]);
	}

	if (!bu_file_exists(argv[2], NULL)) {
	    bu_exit(-1, "Cannot stat file %s\n", argv[2]);
	}

	if ((left_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[0]);
	}
	RT_CK_DBI(left_dbip);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}
	db_update_nref(left_dbip);

	if ((ancestor_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(ancestor_dbip);
	if (db_dirbuild(ancestor_dbip) < 0) {
	    db_close(left_dbip);
	    db_close(ancestor_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}
	db_update_nref(ancestor_dbip);

	if ((right_dbip = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[2]);
	}
	RT_CK_DBI(right_dbip);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
	}
	db_update_nref(right_dbip);

	diff_return = do_diff3(left_dbip, ancestor_dbip, right_dbip, state);
    }

    gdiff_args_free(&args);
    diff_state_free(state);
    BU_PUT(state, struct diff_state);

    db_close(left_dbip);
    db_close(right_dbip);
    if (argc == 3) db_close(ancestor_dbip);
    if (diff_return == DIFF_UNCHANGED || diff_return == DIFF_EMPTY) {
	return 0;
    } else {
	return diff_return;
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
