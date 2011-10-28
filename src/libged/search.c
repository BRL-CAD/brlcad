/*                        S E A R C H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
    struct bu_vls tmp;
    const char *normalized;
    int islocal = 1;
    bu_vls_init(&tmp);
    if (bu_vls_addr(path)[0] == '/') islocal = 0;
    normalized = db_normalize(bu_vls_addr(path));
    if (normalized && !BU_STR_EQUAL(normalized, "/")) {
	bu_vls_sprintf(&tmp, "%s", bu_basename(normalized));
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
    BU_GETSTRUCT(new_entry, db_full_path_list);
    new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
    db_full_path_init(new_entry->path);
    new_entry->path->fp_maxlen = 0;
    new_entry->local = local;
    BU_LIST_INSERT(&(path_list->l), &(new_entry->l));
}


HIDDEN void
_gen_toplevel(struct db_i *dbip, struct db_full_path_list *path_list, struct db_full_path *dfp, int local)
{
    int i;
    struct directory *dp;
    struct db_full_path_list *new_entry;
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_nref == 0 && !(dp->d_flags & RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
		db_string_to_path(dfp, dbip, dp->d_namep);
		BU_GETSTRUCT(new_entry, db_full_path_list);
		new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
		db_full_path_init(new_entry->path);
		db_dup_full_path(new_entry->path, (const struct db_full_path *)dfp);
		new_entry->local = local;
		BU_LIST_INSERT(&(path_list->l), &(new_entry->l));
	    }
	}
    }
}


int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    void *dbplan;
    int i, islocal;
    int plan_argv = 1;
    int plan_found = 0;
    int path_found = 0;
    struct bu_vls argvls;
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
    /* COPY argv_orig to argv; */
    char **argv = bu_dup_argv(argc, argv_orig);

    bu_vls_init(&argvls);

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, " [path] [expressions...]\n");
	return TCL_OK;
    }

    /* initialize list of search paths */
    BU_GETSTRUCT(path_list, db_full_path_list);
    BU_LIST_INIT(&(path_list->l));
    BU_GETSTRUCT(dispatch_list, db_full_path_list);
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
			    BU_GETSTRUCT(new_entry, db_full_path_list);
			    new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
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
		BU_GETSTRUCT(local_list, db_full_path_list);
		BU_LIST_INIT(&(local_list->l));
		_gen_toplevel(gedp->ged_wdbp->dbip, local_list, &dfp, 1);
		uniq_db_objs = db_search_unique_objects(dbplan, local_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
		db_free_full_path_list(local_list);
	    } else {
		uniq_db_objs = db_search_unique_objects(dbplan, path_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
	    }
	    for (i=(int)BU_PTBL_LEN(uniq_db_objs) - 1; i >=0 ; i--) {
		dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
		bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
	    }
	    bu_ptbl_free(uniq_db_objs);
	} else {
	    for (BU_LIST_FOR_BACKWARDS(entry, db_full_path_list, &(path_list->l))) {
		if (entry->path->fp_maxlen == 0) {
		    BU_GETSTRUCT(local_list, db_full_path_list);
		    BU_LIST_INIT(&(local_list->l));
		    _gen_toplevel(gedp->ged_wdbp->dbip, local_list, &dfp, entry->local);
		    if (entry->local) {
			uniq_db_objs = db_search_unique_objects(dbplan, local_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (i=(int)BU_PTBL_LEN(uniq_db_objs) - 1; i >=0 ; i--) {
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
		    BU_GETSTRUCT(new_entry, db_full_path_list);
		    new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
		    db_full_path_init(new_entry->path);
		    db_dup_full_path(new_entry->path, entry->path);
		    BU_LIST_PUSH(&(dispatch_list->l), &(new_entry->l));
		    if (entry->local) {
			uniq_db_objs = db_search_unique_objects(dbplan, dispatch_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (i=(int)BU_PTBL_LEN(uniq_db_objs) - 1; i >=0 ; i--) {
			    dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
			    bu_vls_printf(gedp->ged_result_str, "%s\n", dp->d_namep);
			}
			bu_ptbl_free(uniq_db_objs);
		    } else {
			search_results = db_search_full_paths(dbplan, dispatch_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
			for (BU_LIST_FOR_BACKWARDS(result, db_full_path_list, &(search_results->l))) {
			    bu_vls_printf(gedp->ged_result_str, "%s\n", db_path_to_string(result->path));
			}
			db_free_full_path_list(search_results);
		    }
		    db_free_full_path(new_entry->path);
		    BU_LIST_DEQUEUE(&(new_entry->l));
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
    bu_vls_free(&argvls);
    bu_free_argv(argc, argv);
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
