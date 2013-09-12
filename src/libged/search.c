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


HIDDEN void
_add_toplevel(struct db_full_path_list *path_list, int local)
{
    struct db_full_path_list *new_entry;
    BU_ALLOC(new_entry, struct db_full_path_list);
    BU_ALLOC(new_entry->path, struct db_full_path);
    db_full_path_init(new_entry->path);
    new_entry->path->fp_maxlen = 0;
    new_entry->local = local;
    BU_LIST_INSERT(&(path_list->l), &(new_entry->l));
}

/* TODO - another instance of the "build a toplevel search list" function - need
 * to consolidate these and get them into librt.  There's at least one more in comb.c*/
HIDDEN void
_gen_toplevel(struct db_i *dbip, struct db_full_path_list *path_list, struct db_full_path *dfp, int local, int aflag)
{
    int i;
    struct directory *dp;
    struct db_full_path_list *new_entry;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_nref == 0 && (!(dp->d_flags & RT_DIR_HIDDEN) || aflag) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
	      if (!db_string_to_path(dfp, dbip, dp->d_namep)) {
		BU_ALLOC(new_entry, struct db_full_path_list);
		BU_ALLOC(new_entry->path, struct db_full_path);
		db_full_path_init(new_entry->path);
		db_dup_full_path(new_entry->path, (const struct db_full_path *)dfp);
		new_entry->local = local;
		BU_LIST_INSERT(&(path_list->l), &(new_entry->l));
	      }
	    }
	}
    }
}


int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    void *dbplan;
    int i, c, islocal, optcnt;
    int aflag = 0; /* flag controlling whether hidden objects are examined */
    int want_help = 0;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    struct bu_vls argvls = BU_VLS_INIT_ZERO;
    struct directory *dp;
    struct db_full_path dfp;
    struct db_full_path_list *entry;
    struct db_full_path_list *result;
    struct db_full_path_list *new_entry;
    struct db_full_path_list *path_list = NULL;
    struct db_full_path_list *dispatch_list = NULL;
    struct db_full_path_list *local_list = NULL;
    struct db_full_path_list *search_results = NULL;
    struct bu_ptbl *uniq_db_objs;
    const char *usage = "[-a] [-h] [path] [expressions...]\n";
    /* COPY argv_orig to argv; */
    char **argv = NULL;

    /* Find how many options we have */

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


    /* initialize list of search paths */
    BU_ALLOC(path_list, struct db_full_path_list);
    BU_LIST_INIT(&(path_list->l));
    BU_ALLOC(dispatch_list, struct db_full_path_list);
    BU_LIST_INIT(&(dispatch_list->l));


    db_full_path_init(&dfp);
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
		path_found = 1;
		if (BU_STR_EQUAL(argv[plan_argv], "/")) {
		    /* if we have nothing but a slash, add all toplevel objects to the list as
		     * full path searches */
		    _add_toplevel(path_list, 0);
		    plan_argv++;
		} else if (BU_STR_EQUAL(argv[plan_argv], ".")) {
		    /* if we have nothing but a dot, add all toplevel objects to the list as
		     * local searches */
		    _add_toplevel(path_list, 1);
		    plan_argv++;
		} else {
		    bu_vls_sprintf(&argvls, "%s", argv[plan_argv]);
		    islocal = _path_scrub(&argvls);
		    if (BU_STR_EQUAL(bu_vls_addr(&argvls), "/")) {
			/* if we have nothing but a slash, normalize resolved to the toplevel. Add
			 * a toplevel search with the islocal flag */
			_add_toplevel(path_list, islocal);
			plan_argv++;
		    } else {
			if (!bu_vls_strlen(&argvls)) {
			    bu_vls_printf(gedp->ged_result_str,  "Search path %s not found in database.\n", argv[plan_argv]);
			    db_free_full_path(&dfp);
			    bu_vls_free(&argvls);
			    bu_free_argv(argc, argv);
			    return GED_ERROR;
			}
			if (db_string_to_path(&dfp, gedp->ged_wdbp->dbip, bu_vls_addr(&argvls)) == -1) {
			    bu_vls_printf(gedp->ged_result_str,  "Search path %s not found in database.\n", bu_vls_addr(&argvls));
			    db_free_full_path(&dfp);
			    bu_vls_free(&argvls);
			    bu_free_argv(argc, argv);
			    return GED_ERROR;
			} else {
			    BU_ALLOC(new_entry, struct db_full_path_list);
			    BU_ALLOC(new_entry->path, struct db_full_path);
			    db_full_path_init(new_entry->path);
			    db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
			    if (argv[plan_argv][0] == '/' && !islocal) {
				new_entry->local = 0;
			    } else {
				new_entry->local = 1;
			    }
			    BU_LIST_PUSH(&(path_list->l), &(new_entry->l));
			    plan_argv++;
			}
		    }
		}
	    } else {
		plan_found = 1;
		if (!path_found) {
		    /* We have a plan but not path - in that case, do a non-full-path tops search */
		    _add_toplevel(path_list, 1);
		}
	    }
	}
    }

    dbplan = db_search_formplan(&argv[plan_argv], gedp->ged_wdbp->dbip, gedp->ged_wdbp);
    if (!dbplan) {
	bu_vls_printf(gedp->ged_result_str,  "Failed to build find plan.\n");
	db_free_full_path(&dfp);
	bu_vls_free(&argvls);
	bu_free_argv(argc, argv);
	db_free_full_path_list(path_list);
	return GED_ERROR;
    } else {
	islocal = 1;
	for (BU_LIST_FOR_BACKWARDS(entry, db_full_path_list, &(path_list->l))) {
	    if (!entry->local) islocal = 0;
	}
	/* If all searches are local, use all supplied paths in the search to
	 * return one unique list of objects.  If one or more paths are non-local,
	 * each path is treated as its own search */
	if (islocal) {
	    int search_all = 0;
	    for (BU_LIST_FOR_BACKWARDS(entry, db_full_path_list, &(path_list->l))) {
		if (entry->path->fp_maxlen == 0) {
		    search_all = 1;
		}
	    }
	    if (search_all) {
		BU_ALLOC(local_list, struct db_full_path_list);
		BU_LIST_INIT(&(local_list->l));
		_gen_toplevel(gedp->ged_wdbp->dbip, local_list, &dfp, 1, aflag);
		uniq_db_objs = db_search_unique_objects(dbplan, local_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
		db_free_full_path_list(local_list);
	    } else {
		uniq_db_objs = db_search_unique_objects(dbplan, path_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
	    }
	    for (i = (int)BU_PTBL_LEN(uniq_db_objs) - 1; i >= 0 ; i--) {
		dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
		bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
	    }
	    bu_ptbl_free(uniq_db_objs);
	} else {
	    for (BU_LIST_FOR_BACKWARDS(entry, db_full_path_list, &(path_list->l))) {
		if (entry->path->fp_maxlen == 0) {
		    BU_ALLOC(local_list, struct db_full_path_list);
		    BU_LIST_INIT(&(local_list->l));
		    _gen_toplevel(gedp->ged_wdbp->dbip, local_list, &dfp, entry->local, aflag);
		    if (entry->local) {
			uniq_db_objs = db_search_unique_objects(dbplan, local_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (i = (int)BU_PTBL_LEN(uniq_db_objs) - 1; i >= 0 ; i--) {
			    dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
			}
			bu_ptbl_free(uniq_db_objs);
		    } else {
			search_results = db_search_full_paths(dbplan, local_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (BU_LIST_FOR_BACKWARDS(result, db_full_path_list, &(search_results->l))) {
			    bu_vls_printf(gedp->ged_result_str, "%s\n", db_path_to_string(result->path));
			}
			db_free_full_path_list(search_results);
		    }
		    db_free_full_path_list(local_list);
		} else {
		    BU_ALLOC(new_entry, struct db_full_path_list);
		    BU_ALLOC(new_entry->path, struct db_full_path);
		    db_full_path_init(new_entry->path);
		    db_dup_full_path(new_entry->path, entry->path);
		    BU_LIST_PUSH(&(dispatch_list->l), &(new_entry->l));
		    if (entry->local) {
			uniq_db_objs = db_search_unique_objects(dbplan, dispatch_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for ( i = (int)BU_PTBL_LEN(uniq_db_objs) - 1; i >= 0 ; i--) {
			    dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
			}
			bu_ptbl_free(uniq_db_objs);
		    } else {
			search_results = db_search_full_paths(dbplan, dispatch_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (BU_LIST_FOR_BACKWARDS(result, db_full_path_list, &(search_results->l))) {
			    char *path_string = db_path_to_string(result->path);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", path_string);
			    bu_free(path_string, "free db_path_to_string output, per raytrace.h");
			}
			db_free_full_path_list(search_results);
		    }
		    db_free_full_path(new_entry->path);
		    BU_LIST_DEQUEUE(&(new_entry->l));
		    bu_free(new_entry->path, "free new_entry path");
		    bu_free(new_entry, "free new_entry");
		}
	    }
	}
	db_search_freeplan(&dbplan);
    }

    /* Assign results to string - if we're doing a list, process the results for unique objects - otherwise
     * just assemble the full path list and return it */
    db_free_full_path(&dfp);
    db_free_full_path_list(path_list);
    db_free_full_path_list(dispatch_list);
    bu_vls_free(&argvls);
    bu_free_argv(argc, argv);
    return TCL_OK;
}

#if 0
    struct bu_ptbl *
db_search(const char *plan_string,
	const char *path,
	struct rt_wdb *wdbp,
	int search_type)
{
    struct bu_ptbl *search_results = NULL;
    char **plan_argv = NULL;
    void *dbplan;
    struct bu_vls plan_string_vls;

    if (!path)return NULL;
    /* get the plan string into an argv array */
    plan_argv = (char **)bu_calloc(strlen(plan_string) + 1, sizeof(char *), "plan argv");
    bu_vls_init(&plan_string_vls);
    bu_vls_sprintf(&plan_string_vls, "%s", plan_string);
    bu_argv_from_string(&plan_argv[0], strlen(plan_string), bu_vls_addr(&plan_string_vls));
    dbplan = db_search_formplan(plan_argv, wdbp->dbip, wdbp);

    /* Based on the type of search, execute the plan and build the appropriate
     *      * final results table */
    BU_ALLOC(search_results, struct bu_ptbl);
    bu_ptbl_init(search_results, 8, "initialize searchresults table");
    switch (search_type) {
	case DB_SEARCH_STANDARD:
	    struct directory *dp = db_lookup(wdbp->dbip, path, LOOKUP_QUIET);
	    if (dp != RT_DIR_NULL) {
		struct db_node_t curr_node;
		db_add_node_to_full_path(curr_node.path, dp);
		/* by convention, the top level node is "unioned" into the global database */
		DB_FULL_PATH_SET_CUR_BOOL(curr_node.path, 2);
		db_fullpath_traverse(wdbp->dbip, wdbp, search_results, &curr_node, find_execute_plans, find_execute_plans, wdbp->wdb_resp, (struct db_plan_t *)dbplan);
	    }
	    break;
	case DB_SEARCH_UNIQ_OBJ:
	    struct directory *dp = db_lookup(wdbp->dbip, path, LOOKUP_QUIET);
	    if (dp != RT_DIR_NULL) {
		struct db_node_t curr_node;
		struct bu_ptbl *initial_search_results = NULL;
		BU_ALLOC(initial_search_results, struct bu_ptbl);
		bu_ptbl_init(initial_search_results, 8, "initialize search results table");
		db_add_node_to_full_path(curr_node.path, dp);
		/* by convention, the top level node is "unioned" into the global database */
		DB_FULL_PATH_SET_CUR_BOOL(curr_node.path, 2);
		db_fullpath_traverse(wdbp->dbip, wdbp, initial_search_results, &curr_node, find_execute_plans, find_execute_plans, wdbp->wdb_resp, (struct db_plan_t *)dbplan);
		/* For this return, we want a list of all unique leaf objects */
		if ((int)BU_PTBL_LEN(initial_search_results) > 0) {
		    struct bu_ptbl *uniq_db_objs = NULL;
		    BU_ALLOC(uniq_db_objs, struct bu_ptbl);
		    bu_ptbl_init(uniq_db_objs, 8, "initialize unique objects table");
		    for(int i = (int)BU_PTBL_LEN(initial_search_results) - 1; i >= 0; i--){
			dfptr = (struct db_full_path *)BU_PTBL_GET(initial_search_results, i);
			if (bu_ptbl_ins_unique(uniq_db_objs, (long *)entry->path->fp_names[entry->path->fp_len - 1]) == -1) {
			    struct db_full_path *new_entry;
			    BU_ALLOC(new_entry, struct db_full_path);
			    db_full_path_init(new_entry);
			    db_add_node_to_full_path(new_entry, entry->path->fp_names[entry->path->fp_len - 1]);
			    bu_ptbl_ins(search_results, (long *)new_entry);
			}
		    }
		    bu_ptbl_free(uniq_db_objs);
		}
		bu_ptbl_free(initial_search_results);
	    }
	    break;
	default:
	    break;
    }
    bu_vls_free(&plan_string_vls);
    bu_free((char *)plan_argv, "free plan argv");
    db_search_freeplan(&dbplan);
    return search_results;
}
#endif



/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
