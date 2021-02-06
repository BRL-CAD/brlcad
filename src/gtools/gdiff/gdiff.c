/*                     G D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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

#include "./gdiff.h"
#include "bu/app.h"
#include "bu/opt.h"

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
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL);

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
	BU_GET(inmem_dbip, struct db_i);
	inmem_dbip->dbi_eof = (b_off_t)-1L;
	inmem_dbip->dbi_fp = NULL;
	inmem_dbip->dbi_mf = NULL;
	inmem_dbip->dbi_read_only = 0;

	/* Initialize fields */
	for (i = 0; i <RT_DBNHASH; i++) {
	    inmem_dbip->dbi_Head[i] = RT_DIR_NULL;
	}

	inmem_dbip->dbi_local2base = 1.0;         /* mm */
	inmem_dbip->dbi_base2local = 1.0;
	inmem_dbip->dbi_title = bu_strdup("Untitled BRL-CAD Database");
	inmem_dbip->dbi_uses = 1;
	inmem_dbip->dbi_filename = NULL;
	inmem_dbip->dbi_filepath = NULL;
	inmem_dbip->dbi_version = 5;

	bu_ptbl_init(&inmem_dbip->dbi_clients, 128, "dbi_clients[]");
	inmem_dbip->dbi_magic = DBI_MAGIC;                /* Now it's valid */

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

	    (void)db_search(&left_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL);
	    (void)db_search(&right_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL);

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

	bu_ptbl_free(&inmem_dbip->dbi_clients);
	for (i = 0; i < (int)BU_PTBL_LEN(&diff3_results); i++) {
	    struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&diff3_results, i);
	    diff_free_result(dr);
	    BU_PUT(dr, struct diff_result);
	}
	bu_ptbl_free(&diff3_results);
	BU_PUT(inmem_dbip, struct db_i);
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
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL);
	(void)db_search(&ancestor_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip, NULL);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip, NULL);

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
gdiff_usage(const char *cmd, struct bu_opt_desc *d) {
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(&str, "Usage: %s [options] left.g [ancestor.g] right.g\n", cmd);
    if (option_help) {
        bu_vls_printf(&str, "Options:\n%s\n", option_help);
        bu_free(option_help, "help str");
    }
    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}

int
main(int argc, const char **argv)
{
    int diff_return = 0;
    int print_help = 0;
    struct diff_state *state;
    struct db_i *left_dbip = DBI_NULL;
    struct db_i *right_dbip = DBI_NULL;
    struct db_i *ancestor_dbip = DBI_NULL;
    const char *diff_prog_name = argv[0];
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    // Name stashed, move on
    argc--; argv++;

    BU_GET(state, struct diff_state);
    diff_state_init(state);

    struct bu_opt_desc d[13];
    BU_OPT(d[0],  "h", "help",       "",        NULL,              &print_help,              "Print help and exit");
    BU_OPT(d[1],  "?", "",           "",        NULL,              &print_help,              "");
    BU_OPT(d[2],  "a", "added",      "",        NULL,              &state->return_added,     "Report added objects");
    BU_OPT(d[3],  "d", "deleted",    "",        NULL,              &state->return_removed,   "Report deleted objects");
    BU_OPT(d[4],  "m", "modified",   "",        NULL,              &state->return_changed,   "Report modified objects");
    BU_OPT(d[5],  "u", "unchanged",  "",        NULL,              &state->return_unchanged, "Report unchanged objects");
    BU_OPT(d[6],  "F", "filter",     "string",  &bu_opt_vls,       state->search_filter,     "Specify filter to use on results");
    BU_OPT(d[7],  "M", "merge-file", "merge.g", &bu_opt_vls,       state->merge_file,        "Specify merge file");
    BU_OPT(d[8],  "t", "tolerance",  "#",       &bu_opt_fastf_t,   &state->diff_tol->dist,   "numerical distance tolerance for same/different decisions (RT_LEN_TOL is default)");
    BU_OPT(d[9],  "v", "verbosity",  "",        &bu_opt_incr_long, &state->verbosity,        "increase output verbosity (multiple specifications of -v increase verbosity more)");
    BU_OPT(d[10], "q", "quiet",      "",        &bu_opt_incr_long, &state->quiet,            "decrease output verbosity (multiple specifications of -q decrease verbosity more)");
    BU_OPT_NULL(d[11]);

    int ret_ac = bu_opt_parse(&msg, argc, argv, d);
    if (ret_ac < 0) {
        bu_log("%s\n", bu_vls_cstr(&msg));
        bu_vls_free(&msg);
        bu_vls_free(state->search_filter);
        bu_vls_free(state->merge_file);
	BU_PUT(state, struct diff_state);
        return BRLCAD_ERROR;
    }
    if (print_help) {
	gdiff_usage(diff_prog_name, d);
        bu_vls_free(&msg);
        bu_vls_free(state->search_filter);
        bu_vls_free(state->merge_file);
	BU_PUT(state, struct diff_state);
        return BRLCAD_OK;
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

    argc = ret_ac;

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
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}
	db_update_nref(left_dbip, &rt_uniresource);

	if ((right_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(right_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(right_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}
	db_update_nref(right_dbip, &rt_uniresource);

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
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}
	db_update_nref(left_dbip, &rt_uniresource);

	if ((ancestor_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(ancestor_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(ancestor_dbip) < 0) {
	    db_close(left_dbip);
	    db_close(ancestor_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}
	db_update_nref(ancestor_dbip, &rt_uniresource);

	if ((right_dbip = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(-1, "Cannot open geometry database file %s\n", argv[2]);
	}
	RT_CK_DBI(right_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(left_dbip);
	    bu_exit(-1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
	}
	db_update_nref(right_dbip, &rt_uniresource);

	diff_return = do_diff3(left_dbip, ancestor_dbip, right_dbip, state);
    }

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


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
