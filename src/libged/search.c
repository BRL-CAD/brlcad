/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2013 United States Government as represented by
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

#include "cmd.h"

#include "./ged_private.h"

HIDDEN int
_path_scrub(struct bu_vls *path)
{
    struct bu_vls tmp = BU_VLS_INIT_ZERO;
    const char *normalized;
    int islocal = 1;

    if (bu_vls_addr(path)[0] == '/') islocal = 0;
    normalized = db_normalize(bu_vls_addr(path));
    if (normalized && !BU_STR_EQUAL(normalized, "/")) {
	char *basename = bu_basename(normalized);
	bu_vls_sprintf(&tmp, "%s", basename);
	bu_free(basename, "free bu_basename string (caller's responsibility per bu.h)");
	bu_vls_sprintf(path, "%s", bu_vls_addr(&tmp));
    } else {
	bu_vls_sprintf(path, "%s", "/");
    }
    bu_vls_free(&tmp);
    return islocal;
}


/* TODO - another instance of the "build a toplevel search list" function - need
 * to consolidate these and get them into librt.  There's at least one more in comb.c*/
char **
db_tops(struct db_i *dbip, int aflag, int flat)
{
    int i;
    int objcount = 0;
    struct directory *dp;
    char ** path_list = NULL;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (((!flat && dp->d_nref == 0) || flat) && (!(dp->d_flags & RT_DIR_HIDDEN) || aflag) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
		objcount++;
	    }
	}
    }
    path_list = (char **)bu_malloc(sizeof(char *) * (objcount + 1), "tops path array");
    objcount = 0;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (((!flat && dp->d_nref == 0) || flat) && (!(dp->d_flags & RT_DIR_HIDDEN) || aflag) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
		path_list[objcount] = dp->d_namep;
		objcount++;
	    }
	}
    }
    path_list[objcount] = '\0';

    return path_list;
}

struct ged_search {
    const char **paths;
    int search_type;
};

void _ged_free_search_set(struct bu_ptbl *search_set) {
    for(int i = (int)BU_PTBL_LEN(search_set) - 1; i >= 0; i--){
	struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	bu_free(search->paths, "free search paths");
	bu_free(search, "free search");
    }
    bu_ptbl_free(search_set);
    bu_free(search_set, "free search container");
}


int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    int i, c, islocal, optcnt;
    int aflag = 0; /* flag controlling whether hidden objects are examined */
    int want_help = 0;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    int all_local = 1;
    struct bu_vls argvls = BU_VLS_INIT_ZERO;
    struct bu_vls search_string = BU_VLS_INIT_ZERO;
    struct bu_ptbl *search_set;
    const char *usage = "[-a] [-h] [path] [expressions...]\n";
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
    while ((bu_optind < (optcnt + 1)) && ((c = bu_getopt(argc, (char * const *)argv_orig, "ah?")) != -1)) {
	switch(c) {
	    case 'a':
		aflag = 1;
		break;
	    case 'h':
	    case '?':
		want_help = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv_orig[0], usage);
		return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv_orig += (bu_optind - 1);

    if (want_help || argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv_orig[0], usage);
	return TCL_OK;
    }

    /* COPY argv_orig to argv; */
    argv = bu_dup_argv(argc, argv_orig);

    /* initialize search set */
    BU_ALLOC(search_set, struct bu_ptbl);
    bu_ptbl_init(search_set, 8, "initialize search set table");


    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    while (!plan_found) {
	if (!argv[plan_argv]) {
	    /* OK, no plan - will use default behavior */
	    plan_found = 1;
	} else {
	    if (!((argv[plan_argv][0] == '-') || (argv[plan_argv][0] == '!')  || (argv[plan_argv][0] == '('))) {
		/* We seem to have a path - make sure it's valid */
		struct ged_search *new_search;
		path_found = 1;
		bu_vls_sprintf(&argvls, "%s", argv[plan_argv]);
		islocal = _path_scrub(&argvls);
		if (BU_STR_EQUAL(argv[plan_argv], ".") || BU_STR_EQUAL(argv[plan_argv], "/") || BU_STR_EQUAL(bu_vls_addr(&argvls), "/")) {
		    BU_ALLOC(new_search, struct ged_search);
		    new_search->paths = (const char **)db_tops(gedp->ged_wdbp->dbip, aflag, 0);
		    new_search->search_type = islocal;
		    if (!islocal) all_local = 0;
		    bu_ptbl_ins(search_set, (long *)new_search);
		    plan_argv++;
		} else {
		    struct directory *path_dp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&argvls), LOOKUP_QUIET);
		    if (!bu_vls_strlen(&argvls) || path_dp == RT_DIR_NULL) {
			bu_vls_printf(gedp->ged_result_str,  "Search path %s not found in database.\n", argv[plan_argv]);
			bu_vls_free(&argvls);
			bu_free_argv(argc, argv);
			_ged_free_search_set(search_set);
			return GED_ERROR;
		    } else {
			char **path_list = (char **)bu_malloc(sizeof(char *) * 2, "object path array");
			path_list[0] = path_dp->d_namep;
			path_list[1] = '\0';
			BU_ALLOC(new_search, struct ged_search);
			new_search->paths = (const char **)path_list;
			new_search->search_type = islocal;
			if (!islocal) all_local = 0;
			bu_ptbl_ins(search_set, (long *)new_search);
			plan_argv++;
		    }
		}
	    } else {
		plan_found = 1;
		if (!path_found) {
		    /* We have a plan but not path - in that case, do a non-full-path tops search */
		    struct ged_search *new_search;
		    BU_ALLOC(new_search, struct ged_search);
		    new_search->paths = (const char **)db_tops(gedp->ged_wdbp->dbip, aflag, 0);
		    new_search->search_type = 1;
		    bu_ptbl_ins(search_set, (long *)new_search);
		}
	    }
	}
    }

    bu_vls_trunc(&search_string, 0);
    while (argv[plan_argv]) {
	bu_vls_printf(&search_string, " %s", argv[plan_argv]);
	plan_argv++;
    }

    /* If all searches are local, use all supplied paths in the search to
     * return one unique list of objects.  If one or more paths are non-local,
     * each path is treated as its own search */
    if (all_local) {
	int j;
	struct bu_ptbl *uniq_db_objs;
	BU_ALLOC(uniq_db_objs, struct bu_ptbl);
	BU_PTBL_INIT(uniq_db_objs);
	for (i = (int)BU_PTBL_LEN(search_set) - 1; i >= 0; i--){
	    int path_cnt = 0;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    const char *curr_path = search->paths[path_cnt];
	    while (curr_path) {
		struct bu_ptbl *initial_search_results = db_search(bu_vls_addr(&search_string), curr_path, gedp->ged_wdbp);
		if (initial_search_results) {
		    for(j = (int)BU_PTBL_LEN(initial_search_results) - 1; j >= 0; j--){
			struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(initial_search_results, j);
			bu_ptbl_ins_unique(uniq_db_objs, (long *)dfptr->fp_names[dfptr->fp_len - 1]);
		    }
		    db_free_search_tbl(initial_search_results);
		}
		path_cnt++;
		curr_path = search->paths[path_cnt];
	    }
	}
	/* For this return, we want a list of all unique leaf objects */
	for (i = (int)BU_PTBL_LEN(uniq_db_objs) - 1; i >= 0; i--){
	    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
	    bu_vls_printf(gedp->ged_result_str, "%s\n", uniq_dp->d_namep);
	}
	bu_ptbl_free(uniq_db_objs);
	bu_free(uniq_db_objs, "free unique object container");
    } else {
	/* Search types are either mixed or all full path, so use the standard calls and print
	 * the full output of each search */
	for (i = 0; i < (int)BU_PTBL_LEN(search_set); i++){
	    int path_cnt = 0;
	    int j;
	    struct ged_search *search = (struct ged_search *)BU_PTBL_GET(search_set, i);
	    const char *curr_path = search->paths[path_cnt];
	    while (curr_path) {
		struct bu_ptbl *uniq_db_objs;
		struct bu_ptbl *search_results;
		switch(search->search_type) {
		    case 0:
			search_results = db_search(bu_vls_addr(&search_string), curr_path, gedp->ged_wdbp);
			if (search_results) {
			    for (j = (int)BU_PTBL_LEN(search_results) - 1; j >= 0; j--){
				struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(search_results, j);
				char *full_path = db_path_to_string(dfptr);
				bu_vls_printf(gedp->ged_result_str, "%s\n", full_path);
				bu_free(full_path, "free string from db_path_to_string");
			    }
			    db_free_search_tbl(search_results);
			}
			break;
		    case 1:
			uniq_db_objs = db_search_obj(bu_vls_addr(&search_string), curr_path, gedp->ged_wdbp);
			for (j = (int)BU_PTBL_LEN(uniq_db_objs) - 1; j >= 0; j--){
			    struct directory *uniq_dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, j);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", uniq_dp->d_namep);
			}
			bu_ptbl_free(uniq_db_objs);
			bu_free(uniq_db_objs, "free unique object container");
			break;
		    default:
			bu_log("Warning - ignoring unknown search type %d\n", search->search_type);
			break;
		}
		path_cnt++;
		curr_path = search->paths[path_cnt];
	    }
	}
    }

    /* Done - free memory */
    bu_vls_free(&argvls);
    bu_vls_free(&search_string);
    bu_free_argv(argc, argv);
    _ged_free_search_set(search_set);
    return TCL_OK;
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
