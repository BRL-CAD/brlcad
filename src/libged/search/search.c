/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/search.c
 *
 * GED wrapper around librt search functions
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/defines.h"

#include "../alphanum.h"
#include "../ged_private.h"

static int
dp_name_compare(const void *d1, const void *d2, void *arg)
{
    struct directory *dp1 = *(struct directory **)d1;
    struct directory *dp2 = *(struct directory **)d2;
    if (dp1 == dp2)
	return 0;
    else if (!dp1)
	return 1;
    else if (!dp2)
	return -1;
    return alphanum_impl((const char *)dp2->d_namep, (const char *)dp1->d_namep, arg);
}

struct fp_sort_entry {
    struct db_full_path *path;
    size_t name_offset;
};

struct fp_sort_data {
    const char *names;
};

/* Estimate enough storage to keep the common path rendering case from
 * repeatedly growing the shared VLS.  Type labels are allowed extra room;
 * bu_vls will still grow normally if an unusual label exceeds the estimate. */
static size_t
fp_name_estimate(const struct db_full_path *fp, int fp_flags)
{
    size_t estimate = 1; /* record-separating NUL */
    size_t i;

    if (!fp || fp->fp_len == 0)
	return estimate;
    if (!fp->fp_names)
	return estimate + strlen("**NULL**");

    for (i = 0; i < fp->fp_len; i++) {
	size_t component = strlen(fp->fp_names[i]->d_namep) + 1; /* slash */

	if (fp_flags & DB_FP_PRINT_BOOL)
	    component += 2;
	if ((fp_flags & DB_FP_PRINT_COMB_INDEX) && fp->fp_cinst[i])
	    component += 16;
	if (fp_flags & DB_FP_PRINT_TYPE)
	    component += 32;

	if (estimate > SIZE_MAX - component)
	    return 0;
	estimate += component;
    }

    return estimate;
}

static int
fp_name_compare(const void *d1, const void *d2, void *arg)
{
    const struct fp_sort_entry *e1 = (const struct fp_sort_entry *)d1;
    const struct fp_sort_entry *e2 = (const struct fp_sort_entry *)d2;
    const struct fp_sort_data *data = (const struct fp_sort_data *)arg;

    BU_ASSERT(data != NULL);

    if (e1 == e2 || e1->path == e2->path)
	return 0;
    else if (!e1->path)
	return 1;
    else if (!e2->path)
	return -1;

    return alphanum_impl(data->names + e2->name_offset,
			 data->names + e1->name_offset, arg);
}


struct ged_search {
    struct bu_vls *prefix;
    struct directory **paths;
    size_t path_cnt;
    int search_type;
};

static int
_path_scrub(struct bu_vls *prefix, struct bu_vls *path)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    const char *normalized;
    int islocal = 1;

    if (!path)
	return 0;

    if (bu_vls_addr(path)[0] == '/')
	islocal = 0;

    normalized = bu_path_normalize(bu_vls_addr(path));

    if (normalized && !BU_STR_EQUAL(normalized, "/")) {

	// Search will use the basename
	char *tbasename = bu_path_basename(normalized, NULL);
	bu_vls_sprintf(&tmp, "%s", tbasename);
	bu_free(tbasename, "free bu_path_basename string (caller's responsibility per bu/path.h)");
	bu_vls_sprintf(path, "%s", bu_vls_cstr(&tmp));

	// Stash the rest of the path, if any, in prefix
	char *tdirname = bu_path_dirname(normalized);
	bu_vls_sprintf(&tmp, "%s", tdirname);
	bu_free(tdirname, "free bu_path_dirname string (caller's responsibility per bu/path.h)");
	bu_vls_trunc(prefix, 0);
	if (bu_vls_strlen(&tmp) > 1) {
	    bu_vls_sprintf(prefix, "%s", bu_vls_cstr(&tmp));
	}

	bu_vls_free(&tmp);
    } else {
	bu_vls_sprintf(path, "%s", "/");
    }

    return islocal;
}


static void
_ged_free_search_set(struct bu_ptbl *search_set)
{
    size_t i;

    if (!search_set)
	return;

    for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);

	if (search) {
	    if (search->paths) {
		bu_free(search->paths, "free search paths");
	    }
	    if (search->prefix) {
		bu_vls_free(search->prefix);
		BU_PUT(search->prefix, struct bu_vls);
	    }
	    bu_free(search, "free search");
	}
    }

    bu_ptbl_free(search_set);
    bu_free(search_set, "free search container");
}


/* Returns 1 if char string seems to be part of a plan,
 * else return 0 */
static int
_ged_plan_item(char *arg)
{
    if (!arg)
	return 0;

    if (arg[0] == '-') return 1;
    if (arg[0] == '!') return 1;
    if (arg[0] == '(') return 1;
    return 0;
}


/* Wrapper to identify the various types of searches being requested.  Returns:
 *
 * 0: path is invalid - db_lookup failed or null input or path normalized to nothing
 * 1: valid search path
 *
 * The variables is_specific, is_local, and is_flat convey specifics about the search.
 */
static int
_ged_search_characterize_path(struct ged *gedp, const char *orig, struct bu_vls *prefix, struct bu_vls *normalized, int *is_specific, int *is_local, int *is_flat, int *flat_only)
{
    struct directory *path_dp = NULL;
    (*is_flat) = 0;
    (*flat_only) = 0;
    (*is_specific) = 0;
    (*is_local) = 0;

    if (!orig || !normalized)
	return 0;

    if (BU_STR_EQUAL(orig, "/")) {
	return 1;
    }
    if (BU_STR_EQUAL(orig, ".")) {
	(*is_local) = 1;
	return 1;
    }
    if (BU_STR_EQUAL(orig, "|")) {
	(*flat_only) = 1;
	return 1;
    }

    bu_vls_sprintf(normalized, "%s", orig);
    if (bu_vls_addr(normalized)[0] == '|') {
	(*is_flat) = 1;
	bu_vls_nibble(normalized, 1);
    }
    if (BU_STR_EQUAL(bu_vls_addr(normalized), "/")) {
	return 1;
    }
    if (BU_STR_EQUAL(bu_vls_addr(normalized), ".")) {
	(*is_local) = 1;
	return 1;
    }

    (*is_local) = _path_scrub(prefix, normalized);
    if (!bu_vls_strlen(normalized))
	return 0;
    if (BU_STR_EQUAL(bu_vls_addr(normalized), "/")) {
	return 1;
    }

    /* We've handled the toplevel special cases - now the only question
     * is is the path valid */
    (*is_specific) = 1;
    path_dp = db_lookup(gedp->dbip, bu_vls_addr(normalized), LOOKUP_QUIET);
    if (path_dp == RT_DIR_NULL) {
	bu_vls_printf(normalized, " not found in database!");
	return 0;
    }
    return 1;
}


static int
_ged_search_localized_obj_list(struct ged *gedp, struct directory *path, struct directory ***path_list, bu_clbk_t clbk, void *u1, void *u2)
{
    size_t path_cnt;
    size_t j;
    const char *comb_str = "-name *";
    struct bu_ptbl *tmp_search;

    BU_ALLOC(tmp_search, struct bu_ptbl);

    (void)db_search(tmp_search, DB_SEARCH_RETURN_UNIQ_DP, comb_str, 1, &path, gedp->dbip, clbk, u1, u2);
    path_cnt = BU_PTBL_LEN(tmp_search);
    (*path_list) = (struct directory **)bu_malloc(sizeof(char *) * (path_cnt+1), "object path array");

    for (j = 0; j < path_cnt; j++) {
	(*path_list)[j] = (struct directory *)BU_PTBL_GET(tmp_search, j);
    }

    (*path_list)[path_cnt] = RT_DIR_NULL;
    bu_ptbl_free(tmp_search);
    bu_free(tmp_search, "Free search table container");

    return path_cnt;
}


static int
search_print_objs_to_vls(const struct bu_ptbl *objs, struct bu_vls *out)
{
    size_t len;

    if (!objs || !out)
	return 0;

    len = BU_PTBL_LEN(objs);
    if (len > 0) {
	bu_sort((void *)BU_PTBL_BASEADDR(objs), len, sizeof(struct directory *), dp_name_compare, NULL);

	while (len-- > 0) {
	    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(objs, len);
	    bu_vls_printf(out, "%s\n", uniq_dp->d_namep);
	}
    }

    return BU_PTBL_LEN(objs);
}

/* -v can be supplied multiple times for growing verbosity
 * first v: will print search totals
 * next  v: will print boolean operations
 * next  v: will print types
 */
static void
_handle_verbosity(int *search_flags, int *fp_flags)
{
    /* Handle separate -v instances and compact -vv... forms identically. */
    if (!(*search_flags & DB_SEARCH_PRINT_TOTAL)) {
	*search_flags |= DB_SEARCH_PRINT_TOTAL;
	return;					// first time, enable totals
    }

    if (!(*fp_flags & DB_FP_PRINT_BOOL)) {
	*fp_flags |= DB_FP_PRINT_BOOL;
	return;					// second time, enable bools
    }

    *fp_flags |= DB_FP_PRINT_TYPE;		// max verbosity, enable types (everything else is already on)
}

/* Search accepts its few leading flags only before a path or expression.  This
 * parser intentionally models the command grammar directly instead of using
 * global getopt state: -v may repeat as -v -v or as -vv, while the other
 * flags remain single-token forms. */
static int
_ged_search_parse_options(struct ged *gedp, int argc, const char *argv[], int *consumed,
	int *aflag, int *wflag, int *flags, int *want_help, int *print_verbose_info)
{
    int index = 1;

    if (!gedp || !argv || !consumed || !aflag || !wflag || !flags || !want_help || !print_verbose_info)
	return -1;

    while (index < argc) {
	const char *arg = argv[index];
	size_t len = arg ? strlen(arg) : 0;
	enum ged_search_top_option_kind kind = GED_SEARCH_TOP_OPTION_UNKNOWN;
	size_t occurrences = 0;

	if (!arg || arg[0] != '-')
	    break;
	if (ged_search_top_option_parse(arg, &kind, &occurrences)) {
	    switch (kind) {
		case GED_SEARCH_TOP_OPTION_HIDDEN:
		    *aflag = 1;
		    *flags |= DB_SEARCH_HIDDEN;
		    break;
		case GED_SEARCH_TOP_OPTION_QUIET:
		    *wflag = 1;
		    *flags |= DB_SEARCH_QUIET;
		    break;
		case GED_SEARCH_TOP_OPTION_HELP:
		    *want_help = 1;
		    break;
		case GED_SEARCH_TOP_OPTION_VERBOSE:
		    for (size_t i = 0; i < occurrences; i++)
			_handle_verbosity(flags, print_verbose_info);
		    break;
		case GED_SEARCH_TOP_OPTION_UNKNOWN:
		default:
		    break;
	    }
	    index++;
	    continue;
	}

	/* Preserve the established options-first grammar: a longer word that is
	 * not a compact verbosity form begins the path/expression phase. */
	if (len != 2)
	    break;

	bu_vls_printf(gedp->ged_result_str,
		"Error: option %s is unknown.\nUsage: [-a] [-v[v..]] [-Q] [-h] [path] [expressions...]\n",
		arg);
	return -1;
    }

    *consumed = index - 1;
    return 0;
}


int
ged_search_core(struct ged *gedp, int argc, const char *argv_orig[])
{
    size_t i;
    int aflag = 0; /* flag controlling whether hidden objects are examined */
    int wflag = 0; /* flag controlling whether to fail quietly or not */
    int flags = 0;
    int want_help = 0;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    int all_local = 1;
    int print_verbose_info = DB_FP_PRINT_COMB_INDEX;
    int search_cnt = 0; /* used to keep a running total of all items printed from search */
    int search_error = 0;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    struct bu_vls search_string = BU_VLS_INIT_ZERO;
    struct bu_ptbl *search_set;
    const char *usage = "[-a] [-v[v..]] [-Q] [-h] [path] [expressions...]\n";
    /* COPY argv_orig to argv; */
    char **argv = NULL;


    bu_clbk_t clbk = NULL;
    void *u1 = (void *)gedp;
    void *u2 = NULL;

    if (ged_clbk_get(&clbk, &u2, gedp, argv_orig[0], BU_CLBK_DURING) == BRLCAD_ERROR) {
	bu_log("search cmd callback retrieval error\n");
	return BRLCAD_ERROR;
    }

    int consumed = 0;
    if (_ged_search_parse_options(gedp, argc, argv_orig, &consumed, &aflag, &wflag,
		&flags, &want_help, &print_verbose_info) != 0)
	return BRLCAD_ERROR;
    argc -= consumed;
    argv_orig += consumed;

    if (want_help || argc == 1) {
	if (!wflag) {
	    bu_vls_sprintf(gedp->ged_result_str, "Usage: %s", usage);
	} else {
	    bu_vls_trunc(gedp->ged_result_str, 0);
	}
	return BRLCAD_OK;
    }

    /* COPY argv_orig to argv; */
    argv = bu_argv_dup(argc, argv_orig);

    /* initialize search set */
    BU_ALLOC(search_set, struct bu_ptbl);
    bu_ptbl_init(search_set, 8, "initialize search set table");


    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* If any specific paths are specified before the plan, we need to identify
     * them and construct search structs. */
    while (!plan_found) {

	if (!argv[plan_argv]) {
	    /* OK, no plan - will use default behavior */
	    plan_found = 1;

	} else {

	    if (!(_ged_plan_item(argv[plan_argv]))) {
		/* We seem to have a path - figure out what type of search it specifies */
		int is_specific, is_local, is_flat, flat_only;
		struct ged_search *new_search;
		int search_path_type;

		search_path_type = _ged_search_characterize_path(gedp, argv[plan_argv], &prefix, &bname, &is_specific, &is_local, &is_flat, &flat_only);
		path_found = 1;

		if (search_path_type) {
		    BU_ALLOC(new_search, struct ged_search);
		    BU_GET(new_search->prefix, struct bu_vls);
		    bu_vls_init(new_search->prefix);
		    bu_vls_sprintf(new_search->prefix, "%s", bu_vls_cstr(&prefix));
		} else {
		    /* FIXME: confirm 'bname' is the desired string to print */
		    if (!wflag) {
			bu_vls_printf(gedp->ged_result_str,  "Search path error:\n input: '%s' normalized: '%s' \n",
				argv[plan_argv], bu_vls_addr(&bname));
		    } else {
			bu_vls_trunc(gedp->ged_result_str, 0);
		    }

		    bu_vls_free(&bname);
		    bu_vls_free(&prefix);
		    bu_argv_free(argc, argv);
		    _ged_free_search_set(search_set);
		    return (wflag) ? BRLCAD_OK : BRLCAD_ERROR;
		}

		if (!is_specific) {
		    if (!is_flat && !aflag && !flat_only)
			new_search->path_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &(new_search->paths));
		    if (!is_flat && aflag && !flat_only)
			new_search->path_cnt = db_ls(gedp->dbip, DB_LS_TOPS | DB_LS_HIDDEN, NULL, &(new_search->paths));
		    if (is_flat && !aflag && !flat_only)
			new_search->path_cnt = db_ls(gedp->dbip, 0, NULL, &(new_search->paths));
		    if (is_flat && aflag && !flat_only)
			new_search->path_cnt = db_ls(gedp->dbip, DB_LS_HIDDEN, NULL, &(new_search->paths));
		} else {
		    /* _ged_search_characterize_path verified that the db_lookup will succeed */
		    struct directory *local_dp = db_lookup(gedp->dbip, bu_vls_addr(&bname), LOOKUP_QUIET);
		    if (is_flat) {
			new_search->path_cnt = _ged_search_localized_obj_list(gedp, local_dp, &(new_search->paths), clbk, u1, u2);
		    } else {
			new_search->paths = (struct directory **)bu_malloc(sizeof(struct directory *) * 2, "object path array");
			new_search->paths[0] = local_dp;
			new_search->paths[1] = RT_DIR_NULL;
			new_search->path_cnt = 1;
		    }
		}

		new_search->search_type = is_local;
		if (flat_only)
		    new_search->search_type = 2;
		bu_ptbl_ins(search_set, (long *)new_search);
		plan_argv++;

	    } else {
		plan_found = 1;

		if (!path_found) {
		    /* We have a plan but not path - in that case, do a non-full-path tops search */
		    struct ged_search *new_search;

		    BU_ALLOC(new_search, struct ged_search);
		    new_search->prefix = NULL;

		    if (!aflag)
			new_search->path_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &(new_search->paths));
		    if (aflag)
			new_search->path_cnt = db_ls(gedp->dbip, DB_LS_TOPS | DB_LS_HIDDEN, NULL, &(new_search->paths));

		    new_search->search_type = 1;
		    bu_ptbl_ins(search_set, (long *)new_search);
		}
	    }
	}
    }

    /* re-assemble search plan into a string - the db search functions break it out themselves */
    bu_vls_trunc(&search_string, 0);
    while (argv[plan_argv]) {
	const char *cp = argv[plan_argv];
	/* Preserve each original argv element through db_search's
	 * bu_argv_from_string pass.  Its grammar recognizes only double
	 * quotes and backslash escapes, so quote every argument and escape
	 * precisely those two characters.  In particular, do not apply Tcl
	 * escaping to braces: search -exec uses literal {} placeholders. */
	if (bu_vls_strlen(&search_string))
	    bu_vls_putc(&search_string, ' ');
	bu_vls_putc(&search_string, '"');
	for (; *cp; cp++) {
	    if (*cp == '"' || *cp == '\\')
		bu_vls_putc(&search_string, '\\');
	    bu_vls_putc(&search_string, *cp);
	}
	bu_vls_putc(&search_string, '"');
	plan_argv++;
    }

    /* Validate once before running any roots.  A NULL dbip returns -2 for a
     * valid plan and -1 for an invalid plan.  Quiet mode suppresses the
     * diagnostic and treats only the invalid-plan case as a quiet success. */
    if (db_search(NULL, flags, bu_vls_addr(&search_string), 0, NULL, NULL,
		clbk, u1, u2) == -1) {
	bu_vls_free(&bname);
	bu_vls_free(&prefix);
	bu_vls_free(&search_string);
	bu_argv_free(argc, argv);
	_ged_free_search_set(search_set);
	bu_vls_trunc(gedp->ged_result_str, 0);
	return wflag ? BRLCAD_OK : BRLCAD_ERROR;
    }

    /* Check if all of our searches are local or not */
    for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	if (search && search->search_type != 1) {
	    all_local = 0;
	}
    }

    /* If all searches are local, use all supplied paths in the search to
     * return one unique list of objects.  If one or more paths are non-local,
     * each path is treated as its own search */
    if (all_local) {
	struct bu_ptbl *uniq_db_objs;
	struct directory **batch_paths = NULL;
	size_t batch_cnt = 0;
	size_t batch_i = 0;

	BU_ALLOC(uniq_db_objs, struct bu_ptbl);
	BU_PTBL_INIT(uniq_db_objs);

	for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    if (search && search->paths)
		batch_cnt += search->path_cnt;
	}
	if (batch_cnt > INT_MAX) {
	    bu_log("search: too many local paths to evaluate in one search\n");
	    search_error = 1;
	    batch_cnt = 0;
	}
	if (batch_cnt)
	    batch_paths = (struct directory **)bu_malloc(
		    batch_cnt * sizeof(struct directory *), "batched local search paths");

	for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	    struct ged_search *search;

	    search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    if (!search || !search->paths || !batch_paths)
		continue;
	    for (size_t path_i = 0; path_i < search->path_cnt; path_i++)
		batch_paths[batch_i++] = search->paths[path_i];
	}

	flags |= DB_SEARCH_RETURN_UNIQ_DP;
	if (batch_cnt && db_search(uniq_db_objs, flags,
		bu_vls_addr(&search_string), (int)batch_cnt, batch_paths,
		gedp->dbip, clbk, u1, u2) < 0) {
	    search_error = 1;
	}
	if (batch_paths)
	    bu_free(batch_paths, "batched local search paths");

	search_cnt += search_print_objs_to_vls(uniq_db_objs, gedp->ged_result_str);

	bu_ptbl_free(uniq_db_objs);
	bu_free(uniq_db_objs, "free unique object container");

    } else {

	/* Search types are either mixed or all full path, so use the standard calls and print
	 * the full output of each search */

	for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	    size_t j;
	    size_t path_cnt = 0;
	    size_t sr_len;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);

	    if (search && (search->path_cnt > 0 || search->search_type == 2)) {
		if (search->search_type == 2) {
		    struct bu_ptbl *search_results;

		    flags |= DB_SEARCH_FLAT;
		    BU_ALLOC(search_results, struct bu_ptbl);
		    bu_ptbl_init(search_results, 8, "initialize search result table");

		    if (db_search(search_results, flags,
			    bu_vls_addr(&search_string), 0, NULL, gedp->dbip,
			    clbk, u1, u2) < 0)
			search_error = 1;

		    search_cnt += search_print_objs_to_vls(search_results, gedp->ged_result_str);

		    db_search_free(search_results);
		    bu_free(search_results, "free search container");

		    /* Make sure to clear the flag in case of subsequent searches of different types */
		    flags = flags & ~(DB_SEARCH_FLAT);

		} else if (search->path_cnt > 0) {
		    struct directory *curr_path = search->paths[0];

		    while (path_cnt < search->path_cnt) {
			struct bu_ptbl *search_results;

			BU_ALLOC(search_results, struct bu_ptbl);
			bu_ptbl_init(search_results, 8, "initialize search result table");

			switch (search->search_type) {
			    case 0:
				flags &= ~DB_SEARCH_RETURN_UNIQ_DP;
				if (db_search(search_results, flags, bu_vls_addr(&search_string),
					1, &curr_path, gedp->dbip, clbk, u1, u2) < 0)
				    search_error = 1;

				sr_len = BU_PTBL_LEN(search_results);
				if (sr_len > 0) {
				    struct fp_sort_entry *sorted_results;
				    struct fp_sort_data sdata;
				    struct bu_vls sort_names = BU_VLS_INIT_ZERO;
				    size_t sort_names_estimate = 1; /* final VLS terminator */

				    sorted_results = (struct fp_sort_entry *)bu_calloc(
					sr_len, sizeof(struct fp_sort_entry), "cached search sort records");

				    for (j = 0; j < sr_len; j++) {
					struct db_full_path *dfptr =
					    (struct db_full_path *)BU_PTBL_GET(search_results, j);
					size_t path_estimate = fp_name_estimate(dfptr, print_verbose_info);
					if (!path_estimate || sort_names_estimate > SIZE_MAX - path_estimate) {
					    sort_names_estimate = 0;
					    break;
					}
					sort_names_estimate += path_estimate;
				    }
				    if (sort_names_estimate)
					bu_vls_extend(&sort_names, sort_names_estimate);

				    /* Render every path once.  Store all names in one VLS so
				     * sorting needs no per-result allocations and the same
				     * cached strings can be reused for final output. */
				    for (j = 0; j < sr_len; j++) {
					struct db_full_path *dfptr =
					    (struct db_full_path *)BU_PTBL_GET(search_results, j);
					sorted_results[j].path = dfptr;
					sorted_results[j].name_offset = bu_vls_strlen(&sort_names);
					db_fullpath_to_vls(&sort_names, dfptr, gedp->dbip, print_verbose_info);
					bu_vls_putc(&sort_names, '\0');
				    }

				    sdata.names = bu_vls_cstr(&sort_names);
				    bu_sort((void *)sorted_results, sr_len,
					    sizeof(struct fp_sort_entry), fp_name_compare, (void *)&sdata);

				    j = sr_len;
				    while (j-- > 0) {
					const char *sorted_name = sdata.names + sorted_results[j].name_offset;
					if (search->prefix && bu_vls_strlen(search->prefix)) {
					    bu_vls_printf(gedp->ged_result_str, "%s%s\n",
						    bu_vls_cstr(search->prefix), sorted_name);
					} else {
					    bu_vls_printf(gedp->ged_result_str, "%s\n", sorted_name);
					}
				    }

				    search_cnt += sr_len;
				    bu_vls_free(&sort_names);
				    bu_free(sorted_results, "cached search sort records");
				}
				break;
			    case 1:
				flags |= DB_SEARCH_RETURN_UNIQ_DP;
				if (db_search(search_results, flags, bu_vls_addr(&search_string),
					1, &curr_path, gedp->dbip, clbk, u1, u2) < 0)
				    search_error = 1;

				search_cnt += search_print_objs_to_vls(search_results, gedp->ged_result_str);

				break;
			    default:
				bu_log("Warning - ignoring unknown search type %d\n", search->search_type);
				break;
			}

			db_search_free(search_results);
			bu_free(search_results, "free search container");
			path_cnt++;
			if (path_cnt < search->path_cnt)
			    curr_path = search->paths[path_cnt];
		    }
		}
	    }
	}

    }

    if (flags & DB_SEARCH_PRINT_TOTAL)
	bu_vls_printf(gedp->ged_result_str, "[%d] items found in search\n", search_cnt);

    /* Done - free memory */
    bu_vls_free(&bname);
    bu_vls_free(&prefix);
    bu_vls_free(&search_string);
    bu_argv_free(argc, argv);
    _ged_free_search_set(search_set);
    return search_error ? BRLCAD_ERROR : BRLCAD_OK;
}


#include "../include/plugin.h"

extern GED_EXPORT const struct ged_cmd_grammar ged_search_grammar;

#define GED_SEARCH_COMMANDS(X, XID) \
    X(search, ged_search_core, GED_CMD_DEFAULT, &ged_search_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_SEARCH_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_search", 1, GED_SEARCH_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
