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
 * Brief description
 *
 */

/* OpenBSD:
 *
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NetBSD:
 *
 * Copyright (c) 1990, 1993, 1994
 * The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
    int plan_argv, plan_found;
    struct directory *dp;
    struct db_full_path dfp;
    struct db_full_path_list *entry;
    struct db_full_path_list *new_entry;
    struct db_full_path_list *path_list;
    struct db_full_path_list *search_results;
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

	plan_argv = 1;
	plan_found = 0;
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

	if (BU_LIST_IS_EMPTY(&(path_list->l))) {
		/* We found a plan, but have nothing to search - in that case, search all top level objects */
		for (i = 0; i < RT_DBNHASH; i++) {
			for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
				if (dp->d_nref == 0 && !(dp->d_flags & RT_DIR_HIDDEN) && (dp->d_addr != RT_DIR_PHONY_ADDR)) {
					db_string_to_path(&dfp, gedp->ged_wdbp->dbip, dp->d_namep);
					BU_GETSTRUCT(new_entry, db_full_path_list);
					new_entry->path = (struct db_full_path *) bu_malloc(sizeof(struct db_full_path), "new full path");
					db_full_path_init(new_entry->path);
					db_dup_full_path(new_entry->path, (const struct db_full_path *)&dfp);
					BU_LIST_PUSH(&(path_list->l), &(new_entry->l));
				}
			}
		}
	}

	if (BU_LIST_IS_EMPTY(&(path_list->l))) {
		/* If list is STILL empty, we have nothing to search - just return empty */
		return TCL_OK;
	} else {
	    dbplan = db_search_formplan(&argv[plan_argv], gedp->ged_wdbp->dbip, gedp->ged_wdbp, search_results);
	    if (!dbplan) {
		bu_vls_printf(&gedp->ged_result_str,  "Failed to build find plan.\n");
		db_free_full_path(&dfp);
		bu_free_argv(argc, argv);
		while (BU_LIST_WHILE(entry, db_full_path_list, &(path_list->l))) {
			BU_LIST_DEQUEUE(&(entry->l));
			db_free_full_path(entry->path);
			bu_free(entry, "free db_full_path_list entry");
		}
		bu_free(path_list, "free path_list");
		return GED_ERROR;
	    } else {
		search_results = db_search_execute(dbplan, path_list, gedp->ged_wdbp->dbip, gedp->ged_wdbp);
	    }
	}

	/* Assign results to string */
	while (BU_LIST_WHILE(entry, db_full_path_list, &(search_results->l))) {
		 bu_vls_printf(&gedp->ged_result_str, "%s\n", db_path_to_string(entry->path));
		 BU_LIST_DEQUEUE(&(entry->l));
		 db_free_full_path(entry->path);
		 bu_free(entry, "free db_full_path_list entry");
	}
	db_free_full_path(&dfp);
	bu_free_argv(argc, argv);	
	bu_free(search_results, "free search_results");
    }
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
