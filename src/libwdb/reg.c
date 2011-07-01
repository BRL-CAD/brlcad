/*                           R E G . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2011 United States Government as represented by
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
/** @file libwdb/reg.c
 *
 * Library for writing MGED databases from arbitrary procedures.
 *
 * This module contains routines to create combinations, and regions.
 *
 * It is expected that this library will grow as experience is gained.
 *
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

/**
 * M K _ T R E E _ P U R E
 *
 * Given a list of wmember structures, build a tree that performs the
 * boolean operations in the given sequence.  No GIFT semantics or
 * precedence is provided.  For that, use mk_tree_gift().
 */
void
mk_tree_pure(struct rt_comb_internal *comb, struct bu_list *member_hd)
{
    struct wmember *wp;

    for (BU_LIST_FOR(wp, wmember, member_hd)) {
	union tree *leafp, *nodep;

	WDB_CK_WMEMBER(wp);

	BU_GETUNION(leafp, tree);
	RT_TREE_INIT(leafp);
	leafp->tr_l.tl_op = OP_DB_LEAF;
	leafp->tr_l.tl_name = bu_strdup(wp->wm_name);
	if (!bn_mat_is_identity(wp->wm_mat)) {
	    leafp->tr_l.tl_mat = bn_mat_dup(wp->wm_mat);
	}

	if (!comb->tree) {
	    comb->tree = leafp;
	    continue;
	}
	/* Build a left-heavy tree */
	BU_GETUNION(nodep, tree);
	RT_TREE_INIT(nodep);
	switch (wp->wm_op) {
	    case WMOP_UNION:
		nodep->tr_b.tb_op = OP_UNION;
		break;
	    case WMOP_INTERSECT:
		nodep->tr_b.tb_op = OP_INTERSECT;
		break;
	    case WMOP_SUBTRACT:
		nodep->tr_b.tb_op = OP_SUBTRACT;
		break;
	    default:
		bu_bomb("mk_tree_pure() bad wm_op");
	}
	nodep->tr_b.tb_left = comb->tree;
	nodep->tr_b.tb_right = leafp;
	comb->tree = nodep;
    }
}


/**
 * M K _ T R E E _ G I F T
 *
 * Add some nodes to a new or existing combination's tree, with GIFT
 * precedence and semantics.
 *
 * NON-PARALLEL due to rt_uniresource
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
int
mk_tree_gift(struct rt_comb_internal *comb, struct bu_list *member_hd)
{
    struct wmember *wp;
    union tree *tp;
    struct rt_tree_array *tree_list;
    size_t node_count;
    size_t actual_count;
    int new_nodes;

    new_nodes = bu_list_len(member_hd);
    if (new_nodes <= 0)
	return 0;	/* OK, nothing to do */

    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_log("mk_tree_gift() Cannot flatten tree for editing\n");
	    return -1;
	}
    }

    /* make space for an extra leaf */
    node_count = db_tree_nleaves(comb->tree);
    tree_list = (struct rt_tree_array *)bu_calloc((size_t)node_count + (size_t)new_nodes,
						  sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    if (comb->tree) {
	/* Release storage for non-leaf nodes, steal leaves */
	actual_count = (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1, &rt_uniresource) - tree_list;
	BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	comb->tree = TREE_NULL;
    } else {
	actual_count = 0;
    }

    /* Add new members to the array */
    for (BU_LIST_FOR(wp, wmember, member_hd)) {
	WDB_CK_WMEMBER(wp);

	switch (wp->wm_op) {
	    case WMOP_INTERSECT:
		tree_list[node_count].tl_op = OP_INTERSECT;
		break;
	    case WMOP_SUBTRACT:
		tree_list[node_count].tl_op = OP_SUBTRACT;
		break;
	    default:
		bu_log("mk_tree_gift() unrecognized relation %c (assuming UNION)\n", wp->wm_op);
		/* Fall through */
	    case WMOP_UNION:
		tree_list[node_count].tl_op = OP_UNION;
		break;
	}

	/* make new leaf node, and insert at end of array */
	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	tree_list[node_count++].tl_tree = tp;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(wp->wm_name);
	if (!bn_mat_is_identity(wp->wm_mat)) {
	    tp->tr_l.tl_mat = bn_mat_dup(wp->wm_mat);
	} else {
	    tp->tr_l.tl_mat = (matp_t)NULL;
	}
    }
    BU_ASSERT_SIZE_T(node_count, ==, actual_count + (size_t)new_nodes);

    /* rebuild the tree with GIFT semantics */
    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count, &rt_uniresource);

    bu_free((char *)tree_list, "mk_tree_gift: tree_list");

    return 0;	/* OK */
}


/**
 * M K _ A D D M E M B E R
 *
 * Obtain dynamic storage for a new wmember structure, fill in the
 * name, default the operation and matrix, and add to doubly linked
 * list.  In typical use, a one-line call is sufficient.  To change
 * the defaults, catch the pointer that is returned, and adjust the
 * structure to taste.
 *
 * The caller is responsible for initializing the header structures
 * forward and backward links.
 */
struct wmember *
mk_addmember(
    const char *name,
    struct bu_list *headp,
    mat_t mat,
    int op)
{
    struct wmember *wp;

    BU_GETSTRUCT(wp, wmember);
    wp->l.magic = WMEMBER_MAGIC;
    wp->wm_name = bu_strdup(name);
    switch (op) {
	case WMOP_UNION:
	case WMOP_INTERSECT:
	case WMOP_SUBTRACT:
	    wp->wm_op = op;
	    break;
	default:
	    bu_log("mk_addmember() op=x%x is bad\n", op);
	    return WMEMBER_NULL;
    }

    /* if the user gave a matrix, use it.  otherwise use identity matrix*/
    if (mat) {
	MAT_COPY(wp->wm_mat, mat);
    } else {
	MAT_IDN(wp->wm_mat);
    }

    /* Append to end of doubly linked list */
    BU_LIST_INSERT(headp, &wp->l);
    return wp;
}


/**
 * M K _ F R E E M E M B E R S
 */
void
mk_freemembers(struct bu_list *headp)
{
    struct wmember *wp;

    while (BU_LIST_WHILE(wp, wmember, headp)) {
	WDB_CK_WMEMBER(wp);
	BU_LIST_DEQUEUE(&wp->l);
	bu_free((char *)wp->wm_name, "wm_name");
	bu_free((char *)wp, "wmember");
    }
}


/**
 * M K _ C O M B
 *
 * Make a combination, where the members are described by a linked
 * list of wmember structs.
 *
 * The linked list is freed when it has been output.
 *
 * Has many operating modes.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
int
mk_comb(
    struct rt_wdb *wdbp,
    const char *combname,
    struct bu_list *headp,
    int region_kind,
    const char *shadername,
    const char *shaderargs,
    const unsigned char *rgb,
    int id,
    int air,
    int material,
    int los,
    int inherit,
    int append_ok,
    int gift_semantics)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int fresh_combination;

    RT_CK_WDB(wdbp);

    RT_DB_INTERNAL_INIT(&intern);

    if (append_ok &&
	wdb_import(wdbp, &intern, combname, (matp_t)NULL) >= 0) {
	/* We retrieved an existing object, append to it */
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	fresh_combination = 0;
    } else {
	/* Create a fresh new object for export */
	BU_GETSTRUCT(comb, rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_ptr = (genptr_t)comb;
	intern.idb_meth = &rt_functab[ID_COMBINATION];

	fresh_combination = 1;
    }

    if (gift_semantics)
	mk_tree_gift(comb, headp);
    else
	mk_tree_pure(comb, headp);

    /* Release the wmember list dynamic storage */
    mk_freemembers(headp);

    /* Don't change these things when appending to existing combination */
    if (fresh_combination) {
	if (region_kind) {
	    comb->region_flag = 1;
	    switch (region_kind) {
		case 'P':
		    comb->is_fastgen = REGION_FASTGEN_PLATE;
		    break;
		case 'V':
		    comb->is_fastgen = REGION_FASTGEN_VOLUME;
		    break;
		case 'R':
		case 1:
		    /* Regular non-FASTGEN Region */
		    break;
		default:
		    bu_log("mk_comb(%s) unknown region_kind=%d (%c), assuming normal non-FASTGEN\n",
			   combname, region_kind, region_kind);
	    }
	}
	if (shadername) bu_vls_strcat(&comb->shader, shadername);
	if (shaderargs) {
	    bu_vls_strcat(&comb->shader, " ");
	    bu_vls_strcat(&comb->shader, shaderargs);
	    /* Convert to Tcl form if necessary.  Use heuristics */
	    if (strchr(shaderargs, '=') != NULL &&
		strchr(shaderargs, '{') == NULL) {
		struct bu_vls old;
		bu_vls_init(&old);
		bu_vls_vlscatzap(&old, &comb->shader);
		if (bu_shader_to_tcl_list(bu_vls_addr(&old), &comb->shader))
		    bu_log("Unable to convert shader string '%s %s'\n", shadername, shaderargs);
		bu_vls_free(&old);
	    }
	}

	if (rgb) {
	    comb->rgb_valid = 1;
	    comb->rgb[0] = rgb[0];
	    comb->rgb[1] = rgb[1];
	    comb->rgb[2] = rgb[2];
	}

	comb->region_id = id;
	comb->aircode = air;
	comb->GIFTmater = material;
	comb->los = los;

	comb->inherit = inherit;
    }

    /* The internal representation will be freed */
    return wdb_put_internal(wdbp, combname, &intern, mk_conv2mm);
}


/**
 * M K _ C O M B 1
 *
 * Convenience interface to make a combination with a single member.
 */
int
mk_comb1(struct rt_wdb *wdbp,
	 const char *combname,
	 const char *membname,
	 int regflag)
{
    struct bu_list head;

    BU_LIST_INIT(&head);
    if (mk_addmember(membname, &head, NULL, WMOP_UNION) == WMEMBER_NULL)
	return -2;
    return mk_comb(wdbp, combname, &head, regflag,
		   (char *)NULL, (char *)NULL, (unsigned char *)NULL,
		   0, 0, 0, 0,
		   0, 0, 0);
}


/**
 * M K _ R E G I O N 1
 *
 * Convenience routine to make a region with shader and rgb possibly
 * set.
 */
int
mk_region1(
    struct rt_wdb *wdbp,
    const char *combname,
    const char *membname,
    const char *shadername,
    const char *shaderargs,
    const unsigned char *rgb)
{
    struct bu_list head;

    BU_LIST_INIT(&head);
    if (mk_addmember(membname, &head, NULL, WMOP_UNION) == WMEMBER_NULL)
	return -2;
    return mk_comb(wdbp, combname, &head, 1, shadername, shaderargs,
		   rgb, 0, 0, 0, 0, 0, 0, 0);
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
