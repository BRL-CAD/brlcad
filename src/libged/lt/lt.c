/*                         L T . C
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
/** @file libged/lt.c
 *
 * The lt command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/getopt.h"
#include "bu/cmd.h"
#include "../ged_private.h"


static int
list_children(struct ged *gedp, struct directory *dp, int c_sep)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return BRLCAD_OK;

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for listing");
		return BRLCAD_ERROR;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count,
							      sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(
		rt_tree_array, comb->tree, OP_UNION,
		1, &rt_uniresource) - rt_tree_array;
	    BU_ASSERT(actual_count == node_count);
	    comb->tree = TREE_NULL;
	} else {
	    actual_count = 0;
	    rt_tree_array = NULL;
	}

	for (i = 0; i < actual_count; i++) {
	    char op;

	    switch (rt_tree_array[i].tl_op) {
		case OP_UNION:
		    op = DB_OP_UNION;
		    break;
		case OP_INTERSECT:
		    op = DB_OP_INTERSECT;
		    break;
		case OP_SUBTRACT:
		    op = DB_OP_SUBTRACT;
		    break;
		default:
		    op = '?';
		    break;
	    }

	    if (c_sep == -1)
		bu_vls_printf(gedp->ged_result_str, "{%c %s} ", op, rt_tree_array[i].tl_tree->tr_l.tl_name);
	    else {
		if (i == 0)
		    bu_vls_printf(gedp->ged_result_str, "%s", rt_tree_array[i].tl_tree->tr_l.tl_name);
		else
		    bu_vls_printf(gedp->ged_result_str, "%c%s", (char)c_sep, rt_tree_array[i].tl_tree->tr_l.tl_name);
	    }

	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	bu_vls_free(&vls);

	if (rt_tree_array)
	    bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
}


int
ged_lt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "[-c sep_char] object";
    int opt;
    int c_sep = -1;
    const char *cmd_name = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_HELP;
    }

    bu_optind = 1;      /* re-init bu_getopt() */
    while ((opt = bu_getopt(argc, (char * const *)argv, "c:")) != -1) {
	switch (opt) {
	    case 'c':
		c_sep = (int)bu_optarg[0];
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", opt);
		return BRLCAD_ERROR;
	}
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

    return list_children(gedp, dp, c_sep);
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl lt_cmd_impl = {
    "lt",
    ged_lt_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd lt_cmd = { &lt_cmd_impl };
const struct ged_cmd *lt_cmds[] = { &lt_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  lt_cmds, 1 };

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
