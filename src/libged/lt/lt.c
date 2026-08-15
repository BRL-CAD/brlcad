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

#include "bu/opt.h"
#include "../ged_private.h"


struct lt_args {
    char separator;
};

static int
lt_separator_parse(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    char *separator = (char *)storage;

    if (argc && argv && argv[0] && argv[0][0] && !argv[0][1]) {
	if (separator)
	    *separator = argv[0][0];
	return 1;
    }
    if (msg)
	bu_vls_printf(msg, "separator must be one character\n");
    return -1;
}

static int
lt_separator_validate(const struct bu_opt_desc *UNUSED(option), size_t argc,
	const char **argv, size_t cursor_arg, void *UNUSED(context),
	void *UNUSED(data), struct bu_opt_validate_result *result)
{
    if (!argv || cursor_arg >= argc || !argv[cursor_arg] ||
	!argv[cursor_arg][0] || argv[cursor_arg][1]) {
	result->state = BU_OPT_VALIDATE_INVALID;
	result->hint = "one-character separator";
    }
    return 0;
}

#define LT_OPTIONS(args) \
    BU_OPT_CUSTOM(args, "c", NULL, separator, "separator", lt_separator_parse, \
	"Output separator character"),

BU_OPT_DESC_BUILDER(lt_options, struct lt_args, LT_OPTIONS);
static const ged_opt_rule lt_opt_rules[] = {
    GED_RULE_VALUE_VALIDATE("c", BU_OPT_VALUE_CHAR, "one-character separator",
	lt_separator_validate, NULL),
    GED_RULE_NULL
};
static const ged_opt_spec lt_opt_spec =
    GED_OPT_WITH("lt", "List combination children", lt_options,
	"options-first object:object", lt_opt_rules);


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
    struct lt_args args = {0};
    int c_sep = -1;
    const char *cmd_name = argv[0];
    int operand_count = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, cmd_name, cmd_name);
	return GED_HELP;
    }

    argc--; argv++;
    operand_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, lt_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (operand_count != 1) {
	ged_cmd_help_append(gedp->ged_result_str, cmd_name, cmd_name);
	return BRLCAD_ERROR;
    }

    if (args.separator)
	c_sep = (int)args.separator;

    if ((dp = db_lookup(gedp->dbip, argv[0], LOOKUP_NOISY)) == RT_DIR_NULL) {
	ged_cmd_help_append(gedp->ged_result_str, cmd_name, cmd_name);
	return BRLCAD_ERROR;
    }

    return list_children(gedp, dp, c_sep);
}

#include "../include/plugin.h"

#define GED_LT_COMMANDS(X, XID) \
    X(lt, ged_lt_core, GED_CMD_DEFAULT, &lt_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_LT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_lt", 1, GED_LT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
