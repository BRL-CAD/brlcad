/*                         L T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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

#include "bu/cmdschema.h"
#include "../ged_private.h"


struct lt_args {
    const char *separator;
};

static int
lt_separator_valid(struct bu_vls *msg, const char *value)
{
    if (value && value[0] && !value[1])
	return 0;
    if (msg)
	bu_vls_printf(msg, "separator must be one character\n");
    return -1;
}

static const struct bu_cmd_option lt_options[] = {
    BU_CMD_STRING_VALIDATE("c", NULL, struct lt_args, separator,
	lt_separator_valid, "separator", "Output separator character"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand lt_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to list", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema lt_cmd_schema = {
    "lt", "List combination children", lt_options, lt_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


static int
list_children(struct ged *gedp, struct directory *dp, int c_sep)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    if (!(dp->d_flags & RT_DIR_COMB))
	return BRLCAD_OK;

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
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
	    db_non_union_push(comb->tree);
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
		rt_tree_array, comb->tree, OP_UNION, 1) - rt_tree_array;
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

	    db_free_tree(rt_tree_array[i].tl_tree);
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
    struct lt_args args = {0};
    int c_sep = -1;
    const char *cmd_name = argv[0];
    int operand_index = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(&lt_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

	if (args.separator)
	c_sep = (int)args.separator[0];

    if ((dp = db_lookup(gedp->dbip, argv[1 + operand_index], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return BRLCAD_ERROR;
    }

    return list_children(gedp, dp, c_sep);
}

#include "../include/plugin.h"

#define GED_LT_COMMANDS(X, XID) \
    X(lt, ged_lt_core, GED_CMD_DEFAULT, &lt_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_lt", 1, GED_LT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
