/*                         T R E E . C
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
#include "bu/getopt.h"
#include "bu/sort.h"

#include "../ged_private.h"

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
	 * calling ged_exec(attr show) so output formatting is consistent.
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

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    if (comb->tree) {
	size_t node_count;
	size_t actual_count;
	struct rt_tree_array *rt_tree_array;

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
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

	    if ((nextdp = db_lookup(gedp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL) {
		size_t j;

		for (j = 0; j < pathpos+1; j++)
		    bu_vls_printf(gedp->ged_result_str, "\t");

		if (verbosity && rt_tree_array[i].tl_tree->tr_l.tl_mat && !bn_mat_is_equal(rt_tree_array[i].tl_tree->tr_l.tl_mat, bn_mat_identity, &gedp->ged_wdbp->wdb_tol))
		    bu_vls_printf(gedp->ged_result_str, "%s ", mlabel);
		bu_vls_printf(gedp->ged_result_str, "%c ", op);
		bu_vls_printf(gedp->ged_result_str, "%s\n", rt_tree_array[i].tl_tree->tr_l.tl_name);
	    } else {

		int domprefix = 0;
		if (verbosity && rt_tree_array[i].tl_tree->tr_l.tl_mat && !bn_mat_is_equal(rt_tree_array[i].tl_tree->tr_l.tl_mat, bn_mat_identity, &gedp->ged_wdbp->wdb_tol)) {
		    domprefix = 1;
		}

		if (currdisplayDepth < displayDepth) {
		    _tree_print_node(gedp, nextdp, pathpos+1, indentSize, op, flags, displayDepth, currdisplayDepth+1, verbosity, domprefix);
		}
	    }
	    db_free_tree(rt_tree_array[i].tl_tree, &rt_uniresource);
	}
	if (rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
    }
    rt_db_free_internal(&intern);
}



/*
 * Return the object hierarchy for all object(s) specified or for all currently displayed
 *
 * Usage:
 * tree [-a] [-c] [-o outfile] [-i indentSize] [-d displayDepth] [object(s)]
 *
 */
int
ged_tree_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int j;
    unsigned flags = 0;
    int indentSize = -1;
    int displayDepth = INT_MAX;
    int c;
    int verbosity = 0;
    FILE *fdout = NULL;
    char *buffer = NULL;
#define WHOARGVMAX 256
    char *whoargv[WHOARGVMAX+1] = {0};
    static const char *usage = "[-a] [-c] [-o outfile] [-i indentSize] [-d displayDepth] [object(s)]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "d:i:o:cav")) != -1) {
	switch (c) {
	    case 'i':
		indentSize = atoi(bu_optarg);
		break;
	    case 'a':
		flags |= _GED_TREE_AFLAG;
		break;
	    case 'c':
		flags |= _GED_TREE_CFLAG;
		break;
	    case 'o':
		if (fdout)
		    fclose(fdout);
		fdout = fopen(bu_optarg, "w+b");
		if (fdout == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "Failed to open output file, %d", errno);
		    return BRLCAD_ERROR;
		}
		break;
	    case 'd':
		displayDepth = atoi(bu_optarg);
		if (displayDepth < 0) {
		    bu_vls_printf(gedp->ged_result_str, "Negative number supplied as depth - unsupported.");
		    if (fdout != NULL)
		      fclose(fdout);
		    return BRLCAD_ERROR;
		}
		break;
	    case 'v':
		verbosity++;
		break;
	    case '?':
	    default:
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
		if (fdout != NULL)
		  fclose(fdout);
		return BRLCAD_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    /* tree of all displayed objects */
    if (argc == 1) {
	char *whocmd[2] = {"who", NULL};
	if (ged_exec(gedp, 1, (const char **)whocmd) == BRLCAD_OK) {
	    buffer = bu_strdup(bu_vls_addr(gedp->ged_result_str));
	    bu_vls_trunc(gedp->ged_result_str, 0);

	    argc += bu_argv_from_string(whoargv, WHOARGVMAX, buffer);
	}
    }

    for (j = 1; j < argc; j++) {
	const char *next = argv[j];
	if (buffer) {
	    next = whoargv[j-1];
	}

	if (j > 1)
	    bu_vls_printf(gedp->ged_result_str, "\n");
	if ((dp = db_lookup(gedp->dbip, next, LOOKUP_NOISY)) == RT_DIR_NULL)
	    continue;
	_tree_print_node(gedp, dp, 0, indentSize, 0, flags, displayDepth, 0, verbosity, 0);
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


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl tree_cmd_impl = {
    "tree",
    ged_tree_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd tree_cmd = { &tree_cmd_impl };
const struct ged_cmd *tree_cmds[] = { &tree_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  tree_cmds, 1 };

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
