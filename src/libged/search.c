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
/** @file search.c
 *
 * GED wrapper around librt search functions
 *
 */

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"

int
ged_search(struct ged *gedp, int argc, const char *argv_orig[])
{
    void *dbplan;
    int i;
    int plan_argv = 1;
    int plan_found = 0;
    int build_uniq_list = 0;
    struct directory *dp;
    struct db_full_path dfp;
    struct db_full_path_list *entry;
    struct db_full_path_list *new_entry;
    struct db_full_path_list *path_list = NULL;
    struct db_full_path_list *search_results = NULL;
    struct bu_ptbl *uniq_db_objs;
    /* COPY argv_orig to argv; */
    char **argv = bu_dup_argv(argc, argv_orig);

    if (argc < 2) {
	bu_vls_printf(&gedp->ged_result_str, " [path] [expressions...]\n");
    } else {
	/* initialize list of search paths */
	BU_GETSTRUCT(path_list, db_full_path_list);
	BU_LIST_INIT(&(path_list->l));

	db_full_path_init(&dfp);
	db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

	/* initialize result */
	bu_vls_trunc(&gedp->ged_result_str, 0);

	/* Check if we're doing a unique object list */
	if ((BU_STR_EQUAL(argv[plan_argv], "."))) {
		build_uniq_list = 1;
		plan_argv++;
	}

	while (!plan_found) {
		if (!((argv[plan_argv][0] == '-') || (argv[plan_argv][0] == '!')  || (argv[plan_argv][0] == '(')) && (!BU_STR_EQUAL(argv[plan_argv], "/")) && (!BU_STR_EQUAL(argv[plan_argv], "."))) {
			/* We seem to have a path - make sure it's valid */
			if (db_string_to_path(&dfp, gedp->ged_wdbp->dbip, argv[plan_argv]) == -1) {
				bu_vls_printf(&gedp->ged_result_str,  " Search path not found in database.\n");
				db_free_full_path(&dfp);
				bu_free_argv(argc, argv);	
				return GED_ERROR;
			} else {
				BU_GETSTRUCT(new_entry, db_full_path_list);
				new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
				db_full_path_init(new_entry->path);
				db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
				BU_LIST_PUSH(&(path_list->l), &(new_entry->l));
				plan_argv++;
			}
		} else {
			plan_found = 1;
		}
	}

	dbplan = db_search_formplan(&argv[plan_argv], gedp->ged_wdbp->dbip, gedp->ged_wdbp);
	if (!dbplan) {
		bu_vls_printf(&gedp->ged_result_str,  "Failed to build find plan.\n");
		db_free_full_path(&dfp);
		bu_free_argv(argc, argv);
		db_free_full_path_list(path_list);
		return GED_ERROR;
	} else {
		if (build_uniq_list) {
			uniq_db_objs = db_search_unique_objects(dbplan, path_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
		} else {
			search_results = db_search_full_paths(dbplan, path_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
		}
	}

	/* Assign results to string - if we're doing a list, process the results for unique objects - otherwise
	 * just assemble the full path list and return it */
	if (build_uniq_list) {
		for (i=0; i < (int)BU_PTBL_LEN(uniq_db_objs); i++) {
			dp = (struct directory *)BU_PTBL_GET(uniq_db_objs, i);
			bu_vls_printf(&gedp->ged_result_str, "%s\n", dp->d_namep);
		}
		bu_ptbl_free(uniq_db_objs);
	} else {
		for(BU_LIST_FOR(entry, db_full_path_list, &(search_results->l))) {
			bu_vls_printf(&gedp->ged_result_str, "%s\n", db_path_to_string(entry->path));
		}
		db_free_full_path_list(search_results);
	}
    }
    db_free_full_path_list(path_list);
    db_free_full_path(&dfp);
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
