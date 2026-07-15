/*                         T R E E . C
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
/** @file libged/tree.c
 *
 * The tree command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/sort.h"

#include "../ged_private.h"


struct tree_args {
    int include_attributes;
    int combinations_only;
    int indent_size;
    int display_depth;
    int verbosity;
    const char *output_file;
};


static int
tree_nonnegative_integer(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(arg, &end, 10);
    if (errno || !arg[0] || (end && *end) || value < 0) {
	if (msg)
	    bu_vls_printf(msg, "Depth must be a non-negative integer");
	return -1;
    }

    return 0;
}


static const struct bu_cmd_option tree_schema_options[] = {
    BU_CMD_FLAG("a", NULL, struct tree_args, include_attributes,
	"Include object attributes"),
    BU_CMD_FLAG("c", NULL, struct tree_args, combinations_only,
	"Print combinations only"),
    BU_CMD_FILE("o", NULL, struct tree_args, output_file, "file",
	"Write output to a file"),
    BU_CMD_INTEGER("i", NULL, struct tree_args, indent_size, "spaces",
	"Set indentation size"),
    BU_CMD_INTEGER_VALIDATE("d", NULL, struct tree_args, display_depth,
	tree_nonnegative_integer, "depth", "Set maximum display depth"),
    BU_CMD_COUNTING_FLAG("v", NULL, struct tree_args, verbosity,
	"Increase output verbosity"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand tree_schema_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_DB_OBJECT, 0, BU_CMD_COUNT_UNLIMITED,
	"Database objects whose trees should be printed", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema tree_cmd_schema = {
    "tree", "Print database combination trees", tree_schema_options,
    tree_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


static void
tree_show_help(struct ged *gedp, const char *command)
{
    char *option_help = bu_cmd_schema_describe(&tree_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-a] [-c] [-v ...] [-d depth] [-i spaces] [-o file] [object ...]",
	command);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "tree native option help");
    }
}


static int
_tree_cmp_attr(const void *p1, const void *p2, void *UNUSED(arg))
{
    return bu_strcmp(((struct bu_attribute_value_pair *)p1)->name,
		     ((struct bu_attribute_value_pair *)p2)->name);
}

static void
_tree_print_node(struct ged *gedp,
		struct directory *dp,
		size_t pathpos,
		int indentSize,
		char opprefix,
		unsigned flags,
		int displayDepth,
		int currdisplayDepth,
		int verbosity,
		int mprefix)
{
    size_t i;
    const char *mlabel = "[M]";
    struct directory *nextdp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    unsigned aflag = (flags & _GED_TREE_AFLAG);
    unsigned cflag = (flags & _GED_TREE_CFLAG);
    struct bu_vls tmp_str = BU_VLS_INIT_ZERO;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    /* cflag = don't show shapes, so return if this is not a combination */
    if (cflag && !(dp->d_flags & RT_DIR_COMB)) {
	return;
    }

    /* set up spacing from the left margin */
    for (i = 0; i < pathpos; i++) {
	if (indentSize < 0) {
	    bu_vls_printf(gedp->ged_result_str, "\t");
	    if (aflag)
		bu_vls_printf(&tmp_str, "\t");

	} else {
	    int j;
	    for (j = 0; j < indentSize; j++) {
		bu_vls_printf(gedp->ged_result_str, " ");
		if (aflag)
		    bu_vls_printf(&tmp_str, " ");
	    }
	}
    }

    /* add the prefix if desired */
    if (mprefix) {
	bu_vls_printf(gedp->ged_result_str, "%s ", mlabel);
    }
    if (opprefix) {
	bu_vls_printf(gedp->ged_result_str, "%c ", opprefix);
	if (aflag)
	    bu_vls_printf(&tmp_str, " ");
    }

    /* now the object name */
    bu_vls_printf(gedp->ged_result_str, "%s", dp->d_namep);

    /* suffix name if appropriate */
    /* Output Comb and Region flags (-F?) */
    if (dp->d_flags & RT_DIR_COMB)
	bu_vls_printf(gedp->ged_result_str, "/");
    if (dp->d_flags & RT_DIR_REGION)
	bu_vls_printf(gedp->ged_result_str, "R");

    bu_vls_printf(gedp->ged_result_str, "\n");

    /* output attributes if any and if desired */
    if (aflag) {
	struct bu_attribute_value_set avs;
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for object %s\n", dp->d_namep);
	    /* need a bombing macro or set an error code here: return BRLCAD_ERROR; */
	    bu_vls_free(&tmp_str);
	    return;
	}

	/* FIXME: manually list all the attributes, if any.  should be
	 * calling ged_exec_attr(show) so output formatting is consistent.
	 */
	if (avs.count) {
	    struct bu_attribute_value_pair *avpp;
	    int max_attr_name_len = 0;

	    /* sort attribute-value set array by attribute name */
	    bu_sort(&avs.avp[0], avs.count, sizeof(struct bu_attribute_value_pair), _tree_cmp_attr, NULL);

	    for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
		int len = (int)strlen(avpp->name);
		if (len > max_attr_name_len) {
		    max_attr_name_len = len;
		}
	    }
	    for (i = 0, avpp = avs.avp; i < avs.count; i++, avpp++) {
		bu_vls_printf(gedp->ged_result_str, "%s       @ %-*.*s    %s\n",
			      tmp_str.vls_str,
			      max_attr_name_len, max_attr_name_len,
			      avpp->name, avpp->value);
	    }
	}
	bu_vls_free(&tmp_str);
    }

    if (!(dp->d_flags & RT_DIR_COMB))
	return;

    /*
     * This node is a combination (e.g., a directory).
     * Process all the arcs (e.g., directory members).
     */

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for listing");
		return;
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

	    if ((nextdp = db_lookup(gedp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		size_t j;

		for (j = 0; j < pathpos+1; j++)
		    bu_vls_printf(gedp->ged_result_str, "\t");

		if (verbosity && rt_tree_array[i].tl_tree->tr_l.tl_mat && !bn_mat_is_equal(rt_tree_array[i].tl_tree->tr_l.tl_mat, bn_mat_identity, &wdbp->wdb_tol))
		    bu_vls_printf(gedp->ged_result_str, "%s ", mlabel);
		bu_vls_printf(gedp->ged_result_str, "%c ", op);
		bu_vls_printf(gedp->ged_result_str, "%s\n", rt_tree_array[i].tl_tree->tr_l.tl_name);
	    } else {

		int domprefix = 0;
		if (verbosity && rt_tree_array[i].tl_tree->tr_l.tl_mat && !bn_mat_is_equal(rt_tree_array[i].tl_tree->tr_l.tl_mat, bn_mat_identity, &wdbp->wdb_tol)) {
		    domprefix = 1;
		}

		if (currdisplayDepth < displayDepth) {
		    _tree_print_node(gedp, nextdp, pathpos+1, indentSize, op, flags, displayDepth, currdisplayDepth+1, verbosity, domprefix);
		}
	    }
	    db_free_tree(rt_tree_array[i].tl_tree);
	}
	if (rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);
}


/*
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 *
 * Usage:
 * tree [-a] [-c] [-v ...] [-d depth] [-i spaces] [-o file] [object ...]
 *
 */
int
ged_tree_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int j;
    unsigned flags = 0;
    int operand_index;
    struct tree_args args = {0, 0, -1, INT_MAX, 0, NULL};
    FILE *fdout = NULL;
    char *buffer = NULL;
#define WHOARGVMAX 256
    char *whoargv[WHOARGVMAX+1] = {0};

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&tree_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	tree_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }

    argc -= operand_index + 1;
    argv += operand_index + 1;

    if (args.include_attributes)
	flags |= _GED_TREE_AFLAG;
    if (args.combinations_only)
	flags |= _GED_TREE_CFLAG;

    if (args.output_file) {
	fdout = fopen(args.output_file, "w+b");
	if (fdout == NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Failed to open output file, %d", errno);
	    return BRLCAD_ERROR;
	}
    }

    /* tree of all displayed objects */
    if (argc == 0) {
	const char *whocmd[1] = {"who"};
	if (ged_exec_who(gedp, 1, (const char **)whocmd) == BRLCAD_OK) {
	    buffer = bu_strdup(bu_vls_addr(gedp->ged_result_str));
	    bu_vls_trunc(gedp->ged_result_str, 0);

	    argc = (int)bu_argv_from_string(whoargv, WHOARGVMAX, buffer);
	}
    }

    for (j = 0; j < argc; j++) {
	const char *next = argv[j];
	if (buffer) {
	    next = whoargv[j];
	}

	if (j > 0)
	    bu_vls_printf(gedp->ged_result_str, "\n");
	if ((dp = db_lookup(gedp->dbip, next, LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	_tree_print_node(gedp, dp, 0, args.indent_size, 0, flags,
		args.display_depth, 0, args.verbosity, 0);
    }

    if (buffer) {
	bu_free(buffer, "free who buffer");
	buffer = NULL;
    }

    if (fdout != NULL) {
	fprintf(fdout, "%s", bu_vls_addr(gedp->ged_result_str));
	fclose(fdout);
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_TREE_COMMANDS(X, XID) \
    X(tree, ged_tree_core, GED_CMD_DEFAULT, &tree_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_TREE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_tree", 1, GED_TREE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
