/*                  C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2013 United States Government as represented by
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
/** @file libged/comb.c
 *
 * The comb command.
 *
 */

#include "common.h"

#include "bio.h"

#include "cmd.h"
#include "wdb.h"

#include "./ged_private.h"


int
ged_comb(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    char *comb_name;
    int i;
    char oper;
    static const char *usage = "comb_name <operation solid>";

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

    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* Check for odd number of arguments */
    if (argc & 01) {
	bu_vls_printf(gedp->ged_result_str, "error in number of args!");
	return GED_ERROR;
    }

    /* Save combination name, for use inside loop */
    comb_name = (char *)argv[1];
    if ((dp=db_lookup(gedp->ged_wdbp->dbip, comb_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	if (!(dp->d_flags & RT_DIR_COMB)) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: %s is not a combination", comb_name);
	    return GED_ERROR;
	}
    }

    /* Get operation and solid name for each solid */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][1] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "bad operation: %s skip member: %s\n", argv[i], argv[i+1]);
	    continue;
	}
	oper = argv[i][0];
	if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[i+1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "skipping %s\n", argv[i+1]);
	    continue;
	}

	if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
	    bu_vls_printf(gedp->ged_result_str, "bad operation: %c skip member: %s\n",
			  oper, dp->d_namep);
	    continue;
	}

	if (_ged_combadd(gedp, dp, comb_name, 0, oper, 0, 0) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "error in combadd");
	    return GED_ERROR;
	}
    }

    return GED_OK;
}


/*
 * G E D _ C O M B A D D
 *
 * Add an instance of object 'objp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 * Preserves the GIFT semantics.
 */
struct directory *
_ged_combadd(struct ged *gedp,
	     struct directory *objp,
	     char *combname,
	     int region_flag,	/* true if adding region */
	     int relation,	/* = UNION, SUBTRACT, INTERSECT */
	     int ident,		/* "Region ID" */
	     int air		/* Air code */)
{
    int ac = 1;
    const char *av[2];

    av[0] = objp->d_namep;
    av[1] = NULL;

    if (_ged_combadd2(gedp, combname, ac, av, region_flag, relation, ident, air) == GED_ERROR)
	return RT_DIR_NULL;

    return db_lookup(gedp->ged_wdbp->dbip, combname, LOOKUP_QUIET);
}


/*
 * G E D _ C O M B A D D
 *
 * Add an instance of object 'objp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 * Preserves the GIFT semantics.
 */
int
_ged_combadd2(struct ged *gedp,
	      char *combname,
	      int argc,
	      const char *argv[],
	      int region_flag,	/* true if adding region */
	      int relation,	/* = UNION, SUBTRACT, INTERSECT */
	      int ident,	/* "Region ID" */
	      int air		/* Air code */)
{
    struct directory *dp;
    struct directory *objp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct rt_tree_array *tree_list;
    size_t node_count;
    size_t actual_count;
    size_t curr_count;
    int i = 0;

    if (argc < 1)
	return GED_ERROR;

    /*
     * Check to see if we have to create a new combination
     */
    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  combname, LOOKUP_QUIET)) == RT_DIR_NULL) {
	int flags;

	if (region_flag)
	    flags = RT_DIR_REGION | RT_DIR_COMB;
	else
	    flags = RT_DIR_COMB;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &rt_functab[ID_COMBINATION];

	GED_DB_DIRADD(gedp, dp, combname, -1, 0, flags, (genptr_t)&intern.idb_type, 0);

	BU_GET(comb, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	intern.idb_ptr = (genptr_t)comb;

	if (region_flag) {
	    comb->region_flag = 1;
	    comb->region_id = ident;
	    comb->aircode = air;
	    comb->los = gedp->ged_wdbp->wdb_los_default;
	    comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;
	    bu_vls_printf(gedp->ged_result_str,
			  "Creating region id=%d, air=%d, GIFTmaterial=%d, los=%d\n",
			  ident, air,
			  gedp->ged_wdbp->wdb_mat_default,
			  gedp->ged_wdbp->wdb_los_default);
	} else {
	    comb->region_flag = 0;
	}

	for (; i < argc; ++i) {
	    if ((objp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "skip member %s\n", argv[i]);
		continue;
	    }

	    RT_GET_TREE(tp, &rt_uniresource);
	    tp->tr_l.tl_op = OP_DB_LEAF;
	    tp->tr_l.tl_name = bu_strdup(objp->d_namep);
	    tp->tr_l.tl_mat = (matp_t)NULL;
	    comb->tree = tp;

	    ++i;
	    goto addmembers;
	}

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, 0);
	return GED_OK;
    } else if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s exists, but is not a combination\n", dp->d_namep);
	return GED_ERROR;
    }

    /* combination exists, add a new member */
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, 0);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (region_flag && !comb->region_flag) {
	bu_vls_printf(gedp->ged_result_str, "%s: not a region\n", dp->d_namep);
	return GED_ERROR;
    }

addmembers:
    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for editing\n");
	    rt_db_free_internal(&intern);
	    return GED_ERROR;
	}
    }

    /* make space for an extra leaf */
    curr_count = db_tree_nleaves(comb->tree) + 1;
    node_count = curr_count + argc - 1;
    tree_list = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    if (comb->tree) {
	actual_count = argc + (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1, &rt_uniresource) - tree_list;
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	comb->tree = TREE_NULL;
    }

    for (; i < argc; ++i) {
	if ((objp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "skip member %s\n", argv[i]);
	    continue;
	}

	/* insert new member at end */
	switch (relation) {
	case '+':
	    tree_list[curr_count - 1].tl_op = OP_INTERSECT;
	    break;
	case '-':
	    tree_list[curr_count - 1].tl_op = OP_SUBTRACT;
	    break;
	default:
	    if (relation != 'u') {
		bu_vls_printf(gedp->ged_result_str, "unrecognized relation (assume UNION)\n");
	    }
	    tree_list[curr_count - 1].tl_op = OP_UNION;
	    break;
	}

	/* make new leaf node, and insert at end of list */
	RT_GET_TREE(tp, &rt_uniresource);
	tree_list[curr_count-1].tl_tree = tp;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(objp->d_namep);
	tp->tr_l.tl_mat = (matp_t)NULL;

	++curr_count;
    }

    /* rebuild the tree */
    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count, &rt_uniresource);

    /* and finally, write it out */
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, 0);

    bu_free((char *)tree_list, "combadd: tree_list");

    return GED_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
