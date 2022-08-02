/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/defines.h"

#define ALPHANUM_IMPL
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

struct fp_cmp_vls {
    struct bu_vls *left;
    struct bu_vls *right;
    struct db_i *dbip;
    int print_verbose_info;
};
static int
fp_name_compare(const void *d1, const void *d2, void *arg)
{
    struct db_full_path *fp1 = *(struct db_full_path **)d1;
    struct db_full_path *fp2 = *(struct db_full_path **)d2;
    struct fp_cmp_vls *data = (struct fp_cmp_vls *)arg;

    BU_ASSERT(data != NULL);

    if (fp1 == fp2)
	return 0;
    else if (!fp1)
	return 1;
    else if (!fp2)
	return -1;

    bu_vls_trunc(data->left, 0);
    bu_vls_trunc(data->right, 0);

    db_fullpath_to_vls(data->left, fp1, data->dbip, data->print_verbose_info);
    db_fullpath_to_vls(data->right, fp2, data->dbip, data->print_verbose_info);

    return alphanum_impl(bu_vls_cstr(data->right), bu_vls_cstr(data->left), arg);
}


struct ged_search {
    struct bu_vls *prefix;
    struct directory **paths;
    size_t path_cnt;
    int search_type;
};


HIDDEN db_search_callback_t
ged_get_interp_eval_callback(struct ged *gedp)
{
    /* FIXME this might need to be more robust? */
    return gedp->ged_interp_eval;
}


HIDDEN int
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


HIDDEN void
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


HIDDEN int
_ged_search_localized_obj_list(struct ged *gedp, struct directory *path, struct directory ***path_list, struct db_search_context *ctx)
{
    size_t path_cnt;
    size_t j;
    const char *comb_str = "-name *";
    struct bu_ptbl *tmp_search;

    BU_ALLOC(tmp_search, struct bu_ptbl);

    (void)db_search(tmp_search, DB_SEARCH_RETURN_UNIQ_DP, comb_str, 1, &path, gedp->dbip, ctx);
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


HIDDEN void
search_print_objs_to_vls(const struct bu_ptbl *objs, struct bu_vls *out)
{
    size_t len;

    if (!objs || !out)
	return;

    len = BU_PTBL_LEN(objs);
    if (len > 0) {
	bu_sort((void *)BU_PTBL_BASEADDR(objs), len, sizeof(struct directory *), dp_name_compare, NULL);

	while (len-- > 0) {
	    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(objs, len);
	    bu_vls_printf(out, "%s\n", uniq_dp->d_namep);
	}
    }
}


int
ged_search_core(struct ged *gedp, int argc, const char *argv_orig[])
{
    size_t i;
    int c;
    int optcnt;
    int aflag = 0; /* flag controlling whether hidden objects are examined */
    int wflag = 0; /* flag controlling whether to fail quietly or not */
    int flags = 0;
    int want_help = 0;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    int all_local = 1;
    int print_verbose_info = DB_FP_PRINT_COMB_INDEX;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    struct bu_vls bname = BU_VLS_INIT_ZERO;
    struct bu_vls search_string = BU_VLS_INIT_ZERO;
    struct bu_ptbl *search_set;
    const char *usage = "[-a] [-v] [-Q] [-h] [path] [expressions...]\n";
    /* COPY argv_orig to argv; */
    char **argv = NULL;
    struct db_search_context *ctx = db_search_context_create();

    db_search_register_data(ctx, (void *)gedp->ged_interp);
    db_search_register_exec(ctx, ged_get_interp_eval_callback(gedp));


    /* Find how many options we have. Once we get support
     * for long options, this logic will have to get more sophisticated
     * (do db_lookup on things to see if they are paths, recognize
     * toplevel path specifiers, etc. */
    optcnt = 0;
    for (i = 1; i < (size_t)argc; i++) {
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
	if (bu_optopt == '?') c='h';
	switch (c) {
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
		want_help = 1;
		break;
	    default:
		bu_vls_sprintf(gedp->ged_result_str, "Error: option %s is unknown.\nUsage: %s", argv_orig[0], usage);
		return BRLCAD_ERROR;
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
	return BRLCAD_OK;
    }

    /* COPY argv_orig to argv; */
    argv = bu_argv_dup(argc, argv_orig);

    /* initialize search set */
    BU_ALLOC(search_set, struct bu_ptbl);
    bu_ptbl_init(search_set, 8, "initialize search set table");


    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip, &rt_uniresource);

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
			new_search->path_cnt = _ged_search_localized_obj_list(gedp, local_dp, &(new_search->paths), ctx);
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
	bu_vls_printf(&search_string, " %s", argv[plan_argv]);
	plan_argv++;
    }

    /* If we have the quiet flag set, check now whether we have a valid plan.  Search will handle
     * an invalid plan string, but it will report why it is invalid.  So in quiet mode,
     * we need to identify the bad string and return now. */
    if (wflag && db_search(NULL, flags, bu_vls_addr(&search_string), 0, NULL, NULL, ctx) != -1) {
	bu_vls_free(&bname);
	bu_vls_free(&prefix);
	bu_vls_free(&search_string);
	bu_argv_free(argc, argv);
	_ged_free_search_set(search_set);
	bu_vls_trunc(gedp->ged_result_str, 0);
	return (wflag) ? BRLCAD_OK : BRLCAD_ERROR;
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

	BU_ALLOC(uniq_db_objs, struct bu_ptbl);
	BU_PTBL_INIT(uniq_db_objs);

	for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	    size_t path_cnt = 0;
	    struct ged_search *search;
	    struct directory *curr_path;

	    search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    if (!search || !search->paths)
		continue;
	    curr_path = search->paths[path_cnt];

	    while (path_cnt < search->path_cnt) {
		flags |= DB_SEARCH_RETURN_UNIQ_DP;
		(void)db_search(uniq_db_objs, flags, bu_vls_addr(&search_string), 1, &curr_path, gedp->dbip, ctx);
		path_cnt++;
		curr_path = search->paths[path_cnt];
	    }
	}

	search_print_objs_to_vls(uniq_db_objs, gedp->ged_result_str);

	bu_ptbl_free(uniq_db_objs);
	bu_free(uniq_db_objs, "free unique object container");

    } else {

	/* Search types are either mixed or all full path, so use the standard calls and print
	 * the full output of each search */

	struct fp_cmp_vls *sdata;

	BU_GET(sdata, struct fp_cmp_vls);
	BU_GET(sdata->left, struct bu_vls);
	BU_GET(sdata->right, struct bu_vls);
	bu_vls_init(sdata->left);
	bu_vls_init(sdata->right);
	sdata->dbip = gedp->dbip;
	sdata->print_verbose_info = print_verbose_info;

	for (i = 0; i < BU_PTBL_LEN(search_set); i++) {
	    size_t j;
	    size_t path_cnt = 0;
	    size_t sr_len;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);

	    if (search && (search->path_cnt > 0 || search->search_type == 2)) {
		if (search->search_type == 2) {
		    size_t k;
		    struct bu_ptbl *search_results;

		    flags |= DB_SEARCH_FLAT;
		    BU_ALLOC(search_results, struct bu_ptbl);
		    bu_ptbl_init(search_results, 8, "initialize search result table");

		    for (k = 0; k < RT_DBNHASH; k++) {
			struct directory *dp;
			for (dp = gedp->dbip->dbi_Head[k]; dp != RT_DIR_NULL; dp = dp->d_forw) {
			    if (dp->d_addr != RT_DIR_PHONY_ADDR) {
				(void)db_search(search_results, flags, bu_vls_addr(&search_string), 1, &dp, gedp->dbip, ctx);
			    }
			}
		    }

		    search_print_objs_to_vls(search_results, gedp->ged_result_str);

		    db_search_free(search_results);
		    bu_free(search_results, "free search container");

		    /* Make sure to clear the flag in case of subsequent searches of different types */
		    flags = flags & ~(DB_SEARCH_FLAT);

		} else {
		    struct directory *curr_path = search->paths[path_cnt];

		    while (path_cnt < search->path_cnt) {
			struct bu_ptbl *search_results;
			struct bu_vls fullpath_string = BU_VLS_INIT_ZERO;

			BU_ALLOC(search_results, struct bu_ptbl);
			bu_ptbl_init(search_results, 8, "initialize search result table");

			switch (search->search_type) {
			    case 0:
				flags &= ~DB_SEARCH_RETURN_UNIQ_DP;
				(void)db_search(search_results, flags, bu_vls_addr(&search_string), 1, &curr_path, gedp->dbip, ctx);

				sr_len = j = BU_PTBL_LEN(search_results);
				if (sr_len > 0) {
				    bu_sort((void *)BU_PTBL_BASEADDR(search_results), sr_len, sizeof(struct directory *), fp_name_compare, (void *)sdata);

				    while (j-- > 0) {
					struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(search_results, j);
					bu_vls_trunc(&fullpath_string, 0);
					db_fullpath_to_vls(&fullpath_string, dfptr, gedp->dbip, print_verbose_info);
					if (search->prefix && bu_vls_strlen(search->prefix)) {
					    bu_vls_printf(gedp->ged_result_str, "%s%s\n", bu_vls_cstr(search->prefix), bu_vls_cstr(&fullpath_string));
					} else {
					    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&fullpath_string));
					}
				    }
				}
				break;
			    case 1:
				flags |= DB_SEARCH_RETURN_UNIQ_DP;
				(void)db_search(search_results, flags, bu_vls_addr(&search_string), 1, &curr_path, gedp->dbip, ctx);

				search_print_objs_to_vls(search_results, gedp->ged_result_str);

				break;
			    default:
				bu_log("Warning - ignoring unknown search type %d\n", search->search_type);
				break;
			}

			db_search_free(search_results);
			bu_free(search_results, "free search container");
			bu_vls_free(&fullpath_string);
			path_cnt++;
			curr_path = search->paths[path_cnt];
		    }
		}
	    }
	}

	bu_vls_free(sdata->left);
	bu_vls_free(sdata->right);
	BU_PUT(sdata->left, struct bu_vls);
	BU_PUT(sdata->right, struct bu_vls);
	BU_PUT(sdata, struct fp_cmp_vls);
    }

    /* Done - free memory */
    bu_vls_free(&bname);
    bu_vls_free(&prefix);
    bu_vls_free(&search_string);
    bu_argv_free(argc, argv);
    db_search_context_destroy(ctx);
    _ged_free_search_set(search_set);
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl search_cmd_impl = {
    "search",
    ged_search_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd search_cmd = { &search_cmd_impl };
const struct ged_cmd *search_cmds[] = { &search_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  search_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
