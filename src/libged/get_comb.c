/*                         G E T _ C O M B . C
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
/** @file libged/get_comb.c
 *
 * The skel command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"


int
ged_get_comb(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t actual_count;
    struct bu_vls vls;
    static const char *usage = "comb";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
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

    bu_vls_init(&vls);

    dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_QUIET);

    if (dp != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a combination, so cannot be edited this way\n", argv[1]);
	    return GED_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	    return GED_ERROR;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for editing\n");
		return GED_ERROR;
	    }
	}

	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array),
							      "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(rt_tree_array,
								   comb->tree,
								   OP_UNION,
								   1,
								   &rt_uniresource) - rt_tree_array;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	    comb->tree = TREE_NULL;
	} else {
	    rt_tree_array = (struct rt_tree_array *)NULL;
	    actual_count = 0;
	}

	bu_vls_printf(gedp->ged_result_str, "%s", dp->d_namep);
	if (comb->region_flag) {
	    bu_vls_printf(gedp->ged_result_str, " Yes %ld %ld %ld %ld",
			  comb->region_id, comb->aircode, comb->GIFTmater, comb->los);
	} else {
	    bu_vls_printf(gedp->ged_result_str, " No");
	}

	if (comb->rgb_valid) {
	    bu_vls_printf(gedp->ged_result_str, " {%d %d %d}", V3ARGS(comb->rgb));
	} else
	    bu_vls_printf(gedp->ged_result_str, " {}");

	bu_vls_printf(gedp->ged_result_str, " {%s}", bu_vls_addr(&comb->shader));

	if (comb->inherit)
	    bu_vls_printf(gedp->ged_result_str, " Yes");
	else
	    bu_vls_printf(gedp->ged_result_str, " No");


	bu_vls_printf(gedp->ged_result_str, " {");
	for (i = 0; i < actual_count; i++) {
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
		    bu_vls_printf(gedp->ged_result_str, "\nIllegal op code in tree\n");
		    return GED_ERROR;
	    }

	    bu_vls_printf(gedp->ged_result_str, " %c %s\t", op, rt_tree_array[i].tl_tree->tr_l.tl_name);
	    _ged_vls_print_matrix(gedp->ged_result_str, rt_tree_array[i].tl_tree->tr_l.tl_mat);
	    bu_vls_printf(gedp->ged_result_str, "\n");
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}

	bu_vls_printf(gedp->ged_result_str, "}");
    } else {
	bu_vls_printf(gedp->ged_result_str, "%s Yes %d %d %d %d {} {} No {}",
		      argv[1],
		      gedp->ged_wdbp->wdb_item_default,
		      gedp->ged_wdbp->wdb_air_default,
		      gedp->ged_wdbp->wdb_mat_default,
		      gedp->ged_wdbp->wdb_los_default);
    }

    return GED_OK;
}


void
_ged_vls_print_matrix(struct bu_vls *vls, matp_t matrix)
{
    int k;
    char buf[64];
    fastf_t tmp;

    if (!matrix)
	return;

    if (bn_mat_is_identity(matrix))
	return;

    for (k=0; k<16; k++) {
	sprintf(buf, "%g", matrix[k]);
	tmp = atof(buf);
	if (ZERO(tmp - matrix[k]))
	    bu_vls_printf(vls, " %g", matrix[k]);
	else
	    bu_vls_printf(vls, " %.12e", matrix[k]);
	if ((k&3)==3) bu_vls_printf(vls, " ");
    }
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
