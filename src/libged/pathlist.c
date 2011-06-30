/*                         P A T H L I S T . C
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
/** @file libged/pathlist.c
 *
 * The pathlist command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


static int pathListNoLeaf = 0;


static union tree *
pathlist_leaf_func(struct db_tree_state *UNUSED(tsp), const struct db_full_path *pathp, struct rt_db_internal *ip, genptr_t client_data)
{
    struct ged *gedp = (struct ged *)client_data;
    char *str;

    RT_CK_FULL_PATH(pathp);
    RT_CK_DB_INTERNAL(ip);

    if (pathListNoLeaf) {
	struct db_full_path pp;
	db_full_path_init(&pp);
	db_dup_full_path(&pp, pathp);
	--pp.fp_len;
	str = db_path_to_string(&pp);
	bu_vls_printf(&gedp->ged_result_str, " %s", str);
	db_free_full_path(&pp);
    } else {
	str = db_path_to_string(pathp);
	bu_vls_printf(&gedp->ged_result_str, " %s", str);
    }

    bu_free((genptr_t)str, "path string");
    return TREE_NULL;
}


int
ged_pathlist(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "name";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    pathListNoLeaf = 0;

    if (argc == 3) {
	if (BU_STR_EQUAL(argv[1], "-noleaf"))
	    pathListNoLeaf = 1;

	++argv;
	--argc;
    }

    if (db_walk_tree(gedp->ged_wdbp->dbip, argc-1, (const char **)argv+1, 1,
		     &gedp->ged_wdbp->wdb_initial_tree_state,
		     0, 0, pathlist_leaf_func, (genptr_t)gedp) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "ged_pathlist: db_walk_tree() error");
	return GED_ERROR;
    }

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
