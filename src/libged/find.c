/*                         F I N D . C
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
/** @file libged/find.c
 *
 * The find command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


HIDDEN void
find_ref(struct db_i *dbip,
	 struct rt_comb_internal *comb,
	 union tree *comb_leaf,
	 genptr_t object,
	 genptr_t comb_name_ptr,
	 genptr_t user_ptr3)
{
    char *obj_name;
    char *comb_name;
    struct ged *gedp = (struct ged *)user_ptr3;

    if (dbip) RT_CK_DBI(dbip);
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);

    obj_name = (char *)object;
    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj_name))
	return;

    comb_name = (char *)comb_name_ptr;

    bu_vls_printf(gedp->ged_result_str, "%s ", comb_name);
}


int
ged_find(struct ged *gedp, int argc, const char *argv[])
{
    int i, k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;
    int c;
    int aflag = 0;		/* look at all objects */
    static const char *usage = "<objects>";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "a")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return GED_ERROR;
	}
    }
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* Examine all COMB nodes */
    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (!(dp->d_flags & RT_DIR_COMB) ||
		(!aflag && (dp->d_flags & RT_DIR_HIDDEN)))
		continue;

	    if (rt_db_get_internal(&intern,
				   dp,
				   gedp->ged_wdbp->dbip,
				   (fastf_t *)NULL,
				   &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		return GED_ERROR;
	    }

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    for (k=1; k<argc; k++)
		db_tree_funcleaf(gedp->ged_wdbp->dbip,
				 comb,
				 comb->tree,
				 find_ref,
				 (genptr_t)argv[k],
				 (genptr_t)dp->d_namep,
				 (genptr_t)gedp);

	    rt_db_free_internal(&intern);
	}
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
