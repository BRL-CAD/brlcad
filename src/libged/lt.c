/*                         L T . C
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
/** @file libged/lt.c
 *
 * The lt command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


static int
list_children(struct ged *gedp, struct directory *dp)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return GED_OK;

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	struct bu_vls vls;
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for listing");
		return GED_ERROR;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	bu_vls_init(&vls);
	for (i=0; i<actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = 'u';
		    break;
		case OP_INTERSECT:
		    op = '+';
		    break;
		case OP_SUBTRACT:
		    op = '-';
		    break;
		default:
		    op = '?';
		    break;
	    }

	    bu_vls_printf(gedp->ged_result_str, "{%c %s} ", op, rt_tree_array[i].tl_tree->tr_l.tl_name);
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}

	if (rt_tree_array)
	    bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);

    return GED_OK;
}


int
ged_lt(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "object";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    return list_children(gedp, dp);
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
