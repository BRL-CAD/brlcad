/*                           R E G . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2026 United States Government as represented by
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

#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

/**
 * Given a list of wmember structures, build a tree that performs the
 * boolean operations in the given sequence.  No GIFT semantics or
 * precedence is provided.  For that, use mk_tree_gift().
 */
static union tree *
mk_tree_pure_leaf(const struct wmember *wp)
{
    union tree *leafp;

    WDB_CK_WMEMBER(wp);
    BU_ALLOC(leafp, union tree);
    RT_TREE_INIT(leafp);
    leafp->tr_l.tl_op = OP_DB_LEAF;
    leafp->tr_l.tl_name = bu_strdup(wp->wm_name);
    if (!bn_mat_is_identity(wp->wm_mat)) {
	leafp->tr_l.tl_mat = bn_mat_dup(wp->wm_mat);
    }

    return leafp;
}


static union tree *
mk_tree_pure_node(int op, union tree *left, union tree *right)
{
    union tree *nodep;

    BU_ALLOC(nodep, union tree);
    RT_TREE_INIT(nodep);
    nodep->tr_b.tb_op = op;
    nodep->tr_b.tb_left = left;
    nodep->tr_b.tb_right = right;

    return nodep;
}


static union tree *
mk_tree_pure_balanced(union tree **trees, size_t count, int op)
{
    size_t middle;

    BU_ASSERT(count > 0);
    if (count == 1) {
	return trees[0];
    }

    middle = count / 2;
    return mk_tree_pure_node(op,
	mk_tree_pure_balanced(trees, middle, op),
	mk_tree_pure_balanced(trees + middle, count - middle, op));
}


void
mk_tree_pure(struct rt_comb_internal *comb, struct bu_list *member_hd)
{
    struct wmember *wp;
    union tree **trees;
    size_t tree_capacity;

    tree_capacity = (size_t)bu_list_len(member_hd);
    if (tree_capacity == 0) {
	return;
    }
    trees = (union tree **)bu_calloc(tree_capacity, sizeof(*trees), "pure tree leaves");

    wp = BU_LIST_FIRST(wmember, member_hd);
    while (BU_LIST_NOT_HEAD(wp, member_hd)) {
	int op;
	size_t tree_count = 0;

	WDB_CK_WMEMBER(wp);
	if (!comb->tree) {
	    comb->tree = mk_tree_pure_leaf(wp);
	    wp = BU_LIST_NEXT(wmember, &wp->l);
	    continue;
	}

	switch (wp->wm_op) {
	    case WMOP_UNION:
		op = OP_UNION;
		break;
	    case WMOP_INTERSECT:
		op = OP_INTERSECT;
		break;
	    case WMOP_SUBTRACT:
		op = OP_SUBTRACT;
		break;
	    default:
		bu_bomb("mk_tree_pure() bad wm_op");
	}

	/*
	 * Union and intersection are associative.  Grouping each run keeps
	 * the specified left-to-right evaluation while avoiding an O(n)-deep
	 * tree for the common case of a large union combination.
	 */
	if (op == OP_UNION || op == OP_INTERSECT) {
	    do {
		trees[tree_count++] = mk_tree_pure_leaf(wp);
		wp = BU_LIST_NEXT(wmember, &wp->l);
	    } while (BU_LIST_NOT_HEAD(wp, member_hd) &&
		     ((op == OP_UNION && wp->wm_op == WMOP_UNION) ||
		      (op == OP_INTERSECT && wp->wm_op == WMOP_INTERSECT)));

	    comb->tree = mk_tree_pure_node(op, comb->tree,
		mk_tree_pure_balanced(trees, tree_count, op));
	    continue;
	}

	comb->tree = mk_tree_pure_node(op, comb->tree, mk_tree_pure_leaf(wp));
	wp = BU_LIST_NEXT(wmember, &wp->l);
    }

    bu_free(trees, "pure tree leaves");
}


/**
 * Add some nodes to a new or existing combination's tree, with GIFT
 * precedence and semantics.
 *
 * NON-PARALLEL due to rt_uniresource.  TODO - is this still true?
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
	db_non_union_push(comb->tree);
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
	actual_count = (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1) - tree_list;
	BU_ASSERT(actual_count == node_count);
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
	BU_ALLOC(tp, union tree);
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
    BU_ASSERT(node_count == actual_count + (size_t)new_nodes);

    /* rebuild the tree with GIFT semantics */
    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count);

    bu_free((char *)tree_list, "mk_tree_gift: tree_list");

    return 0;	/* OK */
}


struct wmember *
mk_addmember(
    const char *name,
    struct bu_list *headp,
    mat_t mat,
    int op)
{
    struct wmember *wp = WMEMBER_NULL;

    /* If we can't append it to anything, we can't add it. */
    if (!headp) return WMEMBER_NULL;

    /* Empty names aren't very useful and can produce lots of weird errors. */
    if (!name || strlen(name) == 0) {
	bu_log("mk_addmember() cannot make a member with an empty name\n");
	return WMEMBER_NULL;
    }

    BU_ALLOC(wp, struct wmember);
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
	BU_ALLOC(comb, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_ptr = (void *)comb;
	intern.idb_meth = &OBJ[ID_COMBINATION];

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
	    if (strchr(shaderargs, '=') != NULL
		&& strchr(shaderargs, '{') == NULL)
	    {
		struct bu_vls old = BU_VLS_INIT_ZERO;

		bu_vls_vlscatzap(&old, &comb->shader);
		if (rt_shader_to_list(bu_vls_addr(&old), &comb->shader))
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
