/*                     G D I F F 2 . C
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
/* Primary function for basic diff operation on two .g files */
/*******************************************************************/
static int
do_diff(struct db_i *left_dbip, struct db_i *right_dbip, struct diff_state *state) {
    int diff_state = DIFF_EMPTY;
    struct bu_ptbl results;
    BU_PTBL_INIT(&results);

    diff_state = db_diff(left_dbip, right_dbip, state->diff_tol, DB_COMPARE_ALL, &results);

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
	struct bu_ptbl right_dbip_filtered = BU_PTBL_INIT_ZERO;
	s_flags |= DB_SEARCH_HIDDEN;
	s_flags |= DB_SEARCH_RETURN_UNIQ_DP;
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip);

	BU_PTBL_INIT(&results_filtered);
	for (i = 0; i < (int)BU_PTBL_LEN(&results); i++) {
	    struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&results, i);
	    if ((dr->dp_left  != RT_DIR_NULL && (bu_ptbl_locate(&left_dbip_filtered,  (long *)dr->dp_left ) != -1)) ||
		(dr->dp_right != RT_DIR_NULL && (bu_ptbl_locate(&right_dbip_filtered, (long *)dr->dp_right) != -1))) {
		bu_ptbl_ins(&results_filtered, (long *)dr);
		filtered_diff_state |= dr->param_state;
		filtered_diff_state |= dr->attr_state;
	    } else {
		diff_free_result(dr);
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
	int diff3_state = DIFF_EMPTY;
	struct bu_ptbl diff3_results;
	struct db_i *inmem_dbip;
	int i;
	BU_PTBL_INIT(&diff3_results);
	BU_GET(inmem_dbip, struct db_i);
	inmem_dbip->dbi_eof = (off_t)-1L;
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

	diff3_state = db_diff3(left_dbip, inmem_dbip, right_dbip, state->diff_tol, DB_COMPARE_ALL, &diff3_results);

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

	    (void)db_search(&left_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip);
	    (void)db_search(&right_dbip3_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip);

	    BU_PTBL_INIT(&diff3_results_filtered);
	    for (i = 0; i < (int)BU_PTBL_LEN(&diff3_results); i++) {
		struct diff_result *dr = (struct diff_result *)BU_PTBL_GET(&diff3_results, i);
		if ((dr->dp_left  != RT_DIR_NULL && (bu_ptbl_locate(&left_dbip3_filtered,  (long *)dr->dp_left)  != -1)) ||
	            (dr->dp_right != RT_DIR_NULL && (bu_ptbl_locate(&right_dbip3_filtered, (long *)dr->dp_right) != -1))) {
		    bu_ptbl_ins(&diff3_results_filtered, (long *)dr);
		} else {
		    diff_free_result(dr);
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
	}
	bu_ptbl_free(&diff3_results);
	BU_PUT(inmem_dbip, struct db_i);
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
	(void)db_search(&left_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip);
	(void)db_search(&ancestor_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, left_dbip);
	(void)db_search(&right_dbip_filtered, s_flags, (const char *)bu_vls_addr(state->search_filter), 0, NULL, right_dbip);

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
    struct db_i *left_dbip = DBI_NULL;
    struct db_i *right_dbip = DBI_NULL;
    struct db_i *ancestor_dbip = DBI_NULL;
    const char *diff_prog_name = argv[0];

    BU_GET(state, struct diff_state);
    diff_state_init(state);

    while ((c = bu_getopt(argc, argv, "aC:cF:M:rt:uv:xh?")) != -1) {
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
	    case 'M':
		state->merge = 1;
		bu_vls_sprintf(state->merge_file, "%s", bu_optarg);
		break;
	    case 't':   /* distance tolerance for same/different decisions (RT_LEN_TOL is default) */
		if (sscanf(bu_optarg, "%lf", &(state->diff_tol->dist)) != 1) {
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

    if (state->merge && bu_file_exists(bu_vls_addr(state->merge_file), NULL)) {
	bu_exit(1, "File %s already exists.\n", bu_vls_addr(state->merge_file));
    }

    /* diff case */
    if (argc == 2) {
	if ((left_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[0]);
	}
	RT_CK_DBI(left_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}

	if ((right_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(right_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(right_dbip);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}

	diff_return = do_diff(left_dbip, right_dbip, state);
    }

    /* diff3 case */
    if (argc == 3) {
	if (!bu_file_exists(argv[2], NULL)) {
	    bu_exit(1, "Cannot stat file %s\n", argv[2]);
	}

	if ((left_dbip = db_open(argv[0], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[0]);
	}
	RT_CK_DBI(left_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(left_dbip) < 0) {
	    db_close(left_dbip);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[0]);
	}

	if ((ancestor_dbip = db_open(argv[1], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[1]);
	}
	RT_CK_DBI(ancestor_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(ancestor_dbip) < 0) {
	    db_close(left_dbip);
	    db_close(ancestor_dbip);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[1]);
	}

	if ((right_dbip = db_open(argv[2], DB_OPEN_READONLY)) == DBI_NULL) {
	    bu_exit(1, "Cannot open geometry database file %s\n", argv[2]);
	}
	RT_CK_DBI(right_dbip);
	/* Reset the material head so we don't get warnings when the global
	 * is overwritten.  This will go away when material_head ceases to
	 * be a librt global.*/
	rt_new_material_head(MATER_NULL);
	if (db_dirbuild(right_dbip) < 0) {
	    db_close(ancestor_dbip);
	    db_close(left_dbip);
	    bu_exit(1, "db_dirbuild failed on geometry database file %s\n", argv[2]);
	}

	diff_return = do_diff3(left_dbip, ancestor_dbip, right_dbip, state);
    }

    diff_state_free(state);
    BU_PUT(state, struct diff_state);

    db_close(left_dbip);
    db_close(right_dbip);
    if (argc == 3) db_close(ancestor_dbip);
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
