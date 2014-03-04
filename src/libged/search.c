/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "bio.h"
#include "bu/cmd.h"

#include "./ged_private.h"


struct ged_search {
    struct directory **paths;
    int path_cnt;
    int search_type;
};


HIDDEN int
_path_scrub(struct bu_vls *path)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    const char *normalized;
    int islocal = 1;

    if (!path)
	return 0;

    if (bu_vls_addr(path)[0] == '/')
	islocal = 0;

    normalized = db_normalize(bu_vls_addr(path));

    if (normalized && !BU_STR_EQUAL(normalized, "/")) {
	char *tbasename = (char *)bu_calloc(strlen(normalized), sizeof(char), "_path_scrub tbasename");
	bu_basename(tbasename, normalized);
	bu_vls_sprintf(&tmp, "%s", tbasename);
	bu_free(tbasename, "free bu_basename string (caller's responsibility per bu.h)");
	bu_vls_sprintf(path, "%s", bu_vls_addr(&tmp));
	bu_vls_free(&tmp);
    } else {
	bu_vls_sprintf(path, "%s", "/");
    }

    return islocal;
}


HIDDEN void
_ged_free_search_set(struct bu_ptbl *search_set)
{
    int i;

    if (!search_set)
	return;

    for (i = (int)BU_PTBL_LEN(search_set) - 1; i >= 0; i--) {
	struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);

	if (search && search->paths)
	    bu_free(search->paths, "free search paths");

	if (search)
	    bu_free(search, "free search");
    }

    if (search_set) {
	bu_ptbl_free(search_set);
	bu_free(search_set, "free search container");
    }
}


/* Returns 1 if char string seems to be part of a plan,
 * else return 0 */
HIDDEN int
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
HIDDEN int
_ged_search_characterize_path(struct ged *gedp, const char *orig, struct bu_vls *normalized, int *is_specific, int *is_local, int *is_flat, int *flat_only)
{
    struct directory *path_dp = NULL;
    (*is_flat) = 0;
    (*flat_only) = 0;
    (*is_specific) = 0;
    (*is_local) = 0;
    if (!orig || !normalized) return 0;
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
    (*is_local) = _path_scrub(normalized);
    if (!bu_vls_strlen(normalized)) return 0;
    if (BU_STR_EQUAL(bu_vls_addr(normalized), "/")) {
	return 1;
    }
    /* We've handled the toplevel special cases - now the only question
     * is is the path valid */
    (*is_specific) = 1;
    path_dp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(normalized), LOOKUP_QUIET);
    if (path_dp == RT_DIR_NULL) {
	bu_vls_printf(normalized, " not found in database!");
	return 0;
    }
    return 1;
}


HIDDEN int
_ged_search_localized_obj_list(struct ged *gedp, struct directory *path, struct directory ***path_list)
{
    int path_cnt;
    int j;
    const char *comb_str = "-name *";
    struct bu_ptbl *tmp_search;
    BU_ALLOC(tmp_search, struct bu_ptbl);
    (void)db_search(tmp_search, comb_str, 1, &path, gedp->ged_wdbp, DB_SEARCH_RETURN_UNIQ_DP);
    path_cnt = (int)BU_PTBL_LEN(tmp_search);
    (*path_list) = (struct directory **)bu_malloc(sizeof(char *) * (path_cnt+1), "object path array");
    for (j = 0; j < path_cnt; j++) {
	(*path_list)[j] = (struct directory *)BU_PTBL_GET(tmp_search, j);
    }
    (*path_list)[path_cnt] = RT_DIR_NULL;
    bu_ptbl_free(tmp_search);
    bu_free(tmp_search, "Free search table container");
    return path_cnt;
}


int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    int i, c, optcnt;
    int aflag = 0; /* flag controlling whether hidden objects are examined */
    int wflag = 0; /* flag controlling whether to fail quietly or not */
    int flags = 0;
    int want_help = 0;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    int all_local = 1;
    int print_verbose_info = 0;
    struct bu_vls argvls = BU_VLS_INIT_ZERO;
    struct bu_vls search_string = BU_VLS_INIT_ZERO;
    struct bu_ptbl *search_set;
    const char *usage = "[-a] [-D] [-Q] [-h] [path] [expressions...]\n";
    /* COPY argv_orig to argv; */
    char **argv = NULL;

    /* Find how many options we have. Once we get support
     * for long options, this logic will have to get more sophisticated
     * (do db_lookup on things to see if they are paths, recognize
     * toplevel path specifiers, etc. */
    optcnt = 0;
    for (i = 1; i < argc; i++) {
	if ((argv_orig[i][0] == '-') && (strlen(argv_orig[i]) == 2)) {
	    optcnt++;
	} else {
	    break;
	}
    }

    /* Options have to come before paths and search expressions, so don't look
     * any further than the max possible option count */
    bu_optind = 1;
    while ((bu_optind < (optcnt + 1)) && ((c = bu_getopt(argc, (char * const *)argv_orig, "?aQhv")) != -1)) {
	switch(c) {
	    case 'a':
		aflag = 1;
		flags |= DB_SEARCH_HIDDEN;
		break;
	    case 'v':
		print_verbose_info |= DB_FP_PRINT_BOOL;
		print_verbose_info |= DB_FP_PRINT_TYPE;
		break;

	    case 'Q':
		wflag = 1;
		flags |= DB_SEARCH_QUIET;
		break;
	    case 'h':
	    case '?':
		want_help = 1;
		break;
	    default:
		bu_vls_sprintf(gedp->ged_result_str, "Error: option %s is unknown.\nUsage: %s", argv_orig[0], usage);
		return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv_orig += (bu_optind - 1);

    if (want_help || argc == 1) {
	if (!wflag) {
	    bu_vls_sprintf(gedp->ged_result_str, "Usage: %s", usage);
	} else {
	    bu_vls_trunc(gedp->ged_result_str, 0);
	}
	return GED_OK;
    }

    /* COPY argv_orig to argv; */
    argv = bu_dup_argv(argc, argv_orig);

    /* initialize search set */
    BU_ALLOC(search_set, struct bu_ptbl);
    bu_ptbl_init(search_set, 8, "initialize search set table");


    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

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
		int search_path_type = _ged_search_characterize_path(gedp, argv[plan_argv], &argvls, &is_specific, &is_local, &is_flat, &flat_only);
		path_found = 1;
		if (search_path_type) {
		    BU_ALLOC(new_search, struct ged_search);
		} else {
		    /* FIXME: confirm 'argvls' is the desired string to print */
		    if (!wflag) {
			bu_vls_printf(gedp->ged_result_str,  "Search path error:\n input: '%s' normalized: '%s' \n",
				argv[plan_argv], bu_vls_addr(&argvls));
		    } else {
			bu_vls_trunc(gedp->ged_result_str, 0);
		    }
		    bu_vls_free(&argvls);
		    bu_free_argv(argc, argv);
		    _ged_free_search_set(search_set);
		    return (wflag) ? GED_OK : GED_ERROR;
		}
		if (!is_specific) {
		    if (!is_flat && !aflag && !flat_only) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_TOPS, &(new_search->paths));
		    if (!is_flat && aflag && !flat_only) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_TOPS | DB_LS_HIDDEN, &(new_search->paths));
		    if (is_flat && !aflag && !flat_only) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, 0, &(new_search->paths));
		    if (is_flat && aflag && !flat_only) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_HIDDEN, &(new_search->paths));
		} else {
		    /* _ged_search_characterize_path verified that the db_lookup will succeed */
		    struct directory *local_dp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&argvls), LOOKUP_QUIET);
		    if (is_flat) {
			new_search->path_cnt = _ged_search_localized_obj_list(gedp, local_dp, &(new_search->paths));
		    } else {
			new_search->paths = (struct directory **)bu_malloc(sizeof(struct directory *) * 2, "object path array");
			new_search->paths[0] = local_dp;
			new_search->paths[1] = RT_DIR_NULL;
			new_search->path_cnt = 1;
		    }
		}
		new_search->search_type = is_local;
		if (flat_only) new_search->search_type = 2;
		bu_ptbl_ins(search_set, (long *)new_search);
		plan_argv++;
	    } else {
		plan_found = 1;
		if (!path_found) {
		    /* We have a plan but not path - in that case, do a non-full-path tops search */
		    struct ged_search *new_search;
		    BU_ALLOC(new_search, struct ged_search);
		    if (!aflag) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_TOPS, &(new_search->paths));
		    if (aflag) new_search->path_cnt = db_ls(gedp->ged_wdbp->dbip, DB_LS_TOPS | DB_LS_HIDDEN, &(new_search->paths));
		    new_search->search_type = 1;
		    bu_ptbl_ins(search_set, (long *)new_search);
		}
	    }
	}
    }

    /* re-assemble search plan into a string - the db search functions break it out themselves */
    bu_vls_trunc(&search_string, 0);
    while (argv[plan_argv]) {
	bu_vls_printf(&search_string, " %s", argv[plan_argv]);
	plan_argv++;
    }

    /* If we have the quiet flag set, check now whether we have a valid plan.  Search will handle
     * an invalid plan string, but it will report why it is invalid.  So in quiet mode,
     * we need to identify the bad string and return now. */
    if (wflag && !db_search(NULL, bu_vls_addr(&search_string), 0, NULL, NULL, flags)) {
	bu_vls_free(&argvls);
	bu_vls_free(&search_string);
	bu_free_argv(argc, argv);
	_ged_free_search_set(search_set);
	bu_vls_trunc(gedp->ged_result_str, 0);
	return (wflag) ? GED_OK : GED_ERROR;
    }

    /* Check if all of our searches are local or not */
    for (i = (int)BU_PTBL_LEN(search_set) - 1; i >= 0; i--) {
	struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	if (search->search_type != 1) all_local = 0;
    }

    /* If all searches are local, use all supplied paths in the search to
     * return one unique list of objects.  If one or more paths are non-local,
     * each path is treated as its own search */
    if (all_local) {
	struct bu_ptbl *uniq_db_objs;
	BU_ALLOC(uniq_db_objs, struct bu_ptbl);
	BU_PTBL_INIT(uniq_db_objs);
	for (i = (int)BU_PTBL_LEN(search_set) - 1; i >= 0; i--) {
	    int path_cnt = 0;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    struct directory *curr_path = search->paths[path_cnt];
	    while (path_cnt < search->path_cnt) {
		flags |= DB_SEARCH_RETURN_UNIQ_DP;
		(void)db_search(uniq_db_objs, bu_vls_addr(&search_string), 1, &curr_path, gedp->ged_wdbp, flags);
		path_cnt++;
		curr_path = search->paths[path_cnt];
	    }
	}
	/* For this return, we want a list of all unique leaf objects */
	for (i = (int)BU_PTBL_LEN(uniq_db_objs) - 1; i >= 0; i--) {
	    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", uniq_dp->d_namep);
	}
	bu_ptbl_free(uniq_db_objs);
	bu_free(uniq_db_objs, "free unique object container");
    } else {
	/* Search types are either mixed or all full path, so use the standard calls and print
	 * the full output of each search */
	for (i = 0; i < (int)BU_PTBL_LEN(search_set); i++) {
	    int path_cnt = 0;
	    int j;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    if (search && (search->path_cnt > 0 || search->search_type == 2)) {
		if (search->search_type == 2) {
		    int k;
		    struct bu_ptbl *search_results;
		    flags |= DB_SEARCH_FLAT;
		    BU_ALLOC(search_results, struct bu_ptbl);
		    bu_ptbl_init(search_results, 8, "initialize search result table");
		    for (k = 0; k < RT_DBNHASH; k++) {
			struct directory *dp;
			for (dp = gedp->ged_wdbp->dbip->dbi_Head[k]; dp != RT_DIR_NULL; dp = dp->d_forw) {
			    if (dp->d_addr != RT_DIR_PHONY_ADDR) {
				(void)db_search(search_results, bu_vls_addr(&search_string), 1, &dp, gedp->ged_wdbp, flags);
			    }
			}
		    }
		    if (BU_PTBL_LEN(search_results) > 0) {
			for (j = (int)BU_PTBL_LEN(search_results) - 1; j >= 0; j--) {
			    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(search_results, j);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", uniq_dp->d_namep);
			}
		    }
		    bu_ptbl_free(search_results);
		    bu_free(search_results, "free flat search container");
		} else {
		    struct directory *curr_path = search->paths[path_cnt];
		    while (path_cnt < search->path_cnt) {
			struct bu_ptbl *search_results;
			struct bu_vls fullpath_string = BU_VLS_INIT_ZERO;
			BU_ALLOC(search_results, struct bu_ptbl);
			bu_ptbl_init(search_results, 8, "initialize search result table");
			switch(search->search_type) {
			    case 0:
				(void)db_search(search_results, bu_vls_addr(&search_string), 1, &curr_path, gedp->ged_wdbp, flags);
				if (BU_PTBL_LEN(search_results) > 0) {
				    for (j = (int)BU_PTBL_LEN(search_results) - 1; j >= 0; j--) {
					struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(search_results, j);
					bu_vls_trunc(&fullpath_string, 0);
					db_fullpath_to_vls(&fullpath_string, dfptr, gedp->ged_wdbp->dbip, &(gedp->ged_wdbp->wdb_tol), print_verbose_info);
					bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_addr(&fullpath_string));
				    }
				}
				break;
			    case 1:
				flags |= DB_SEARCH_RETURN_UNIQ_DP;
				(void)db_search(search_results, bu_vls_addr(&search_string), 1, &curr_path, gedp->ged_wdbp, flags);
				for (j = (int)BU_PTBL_LEN(search_results) - 1; j >= 0; j--) {
				    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(search_results, j);
				    bu_vls_printf(gedp->ged_result_str, "%s\n", uniq_dp->d_namep);
				}
				break;
			    default:
				bu_log("Warning - ignoring unknown search type %d\n", search->search_type);
				break;
			}
			db_free_search_tbl(search_results);
			bu_vls_free(&fullpath_string);
			path_cnt++;
			curr_path = search->paths[path_cnt];
		    }
		}
	    }
	}
    }

    /* Done - free memory */
    bu_vls_free(&argvls);
    bu_vls_free(&search_string);
    bu_free_argv(argc, argv);
    _ged_free_search_set(search_set);
    return GED_OK;
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
