/*                R B _ O R D E R _ S T A T S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu/rb.h"
#include "./rb_internals.h"


/**
 * Retrieve the element of rank k in one order of a red-black tree
 *
 * This function has three parameters: the root of the tree to search,
 * the order on which to do the searching, and the rank of interest.
 * _rb_select() returns the discovered node.  It is an implementation
 * of the routine OS-SELECT on p. 282 of Cormen et al. (p. 341 in the
 * paperback version of the 2009 edition).
 */
HIDDEN struct bu_rb_node *
_rb_select(struct bu_rb_node *root, int order, int k)
{
    int rank;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");

    rank = RB_SIZE(RB_LEFT_CHILD(root, order), order) + 1;
    if (UNLIKELY(root->rbn_tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("_rb_select(<%p>, %d, %d): rank=%d\n",
	       (void*)root, order, k, rank);

    if (rank == k)
	return root;
    else if (rank > k)
	return _rb_select(RB_LEFT_CHILD(root, order), order, k);
    else
	return _rb_select(RB_RIGHT_CHILD(root, order), order, k - rank);
}


void *
bu_rb_select(struct bu_rb_tree *tree, int order, int k)
{
    struct bu_rb_node *node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((k < 1) || (k > tree->rbt_nm_nodes)) {
	if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	    bu_log("bu_rb_select(<%p>, %d, %d): k out of bounds [1, %d]\n",
		   (void*)tree, order, k, tree->rbt_nm_nodes);
	RB_CURRENT(tree) = RB_NULL(tree);
	return NULL;
    }
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("bu_rb_select(<%p>, %d, %d): root=<%p>\n",
	       (void*)tree, order, k, (void*)RB_ROOT(tree, order));

    RB_CURRENT(tree) = node
	= _rb_select(RB_ROOT(tree, order), order, k);
    return RB_DATA(node, order);
}


int bu_rb_rank(struct bu_rb_tree *tree, int order)
{
    int rank;
    struct bu_rb_node *node;
    struct bu_rb_node *parent;
    struct bu_rb_node *root;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((node = RB_CURRENT(tree)) == RB_NULL(tree))
	return 0;

    root = RB_ROOT(tree, order);
    rank = RB_SIZE(RB_LEFT_CHILD(node, order), order) + 1;
    while (node != root) {
	parent = RB_PARENT(node, order);
	if (node == RB_RIGHT_CHILD(parent, order))
	    rank += RB_SIZE(RB_LEFT_CHILD(parent, order), order) + 1;
	node = parent;
    }

    return rank;
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
