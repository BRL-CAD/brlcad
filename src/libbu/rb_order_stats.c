/*                R B _ O R D E R _ S T A T S . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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

#include "bu.h"

#include "./rb_internals.h"


/**
 * _ R B _ S E L E C T
 *
 * Retrieve the element of rank k in one order of a red-black tree
 *
 * This function has three parameters: the root of the tree to search,
 * the order on which to do the searching, and the rank of interest.
 * _rb_select() returns the discovered node.  It is an implemenation
 * of the routine OS-SELECT on p. 282 of Cormen et al.
 */
HIDDEN struct bu_rb_node *
_rb_select(struct bu_rb_node *root, int order, int k)
{
    int rank;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");

    rank = rb_size(rb_left_child(root, order), order) + 1;
    if (UNLIKELY(root->rbn_tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("_rb_select(<%p>, %d, %d): rank=%d\n",
	       root, order, k, rank);

    if (rank == k)
	return root;
    else if (rank > k)
	return _rb_select(rb_left_child(root, order), order, k);
    else
	return _rb_select(rb_right_child(root, order), order, k - rank);
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
		   tree, order, k, tree->rbt_nm_nodes);
	rb_current(tree) = rb_null(tree);
	return NULL;
    }
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("bu_rb_select(<%p>, %d, %d): root=<%p>\n",
	       tree, order, k, rb_root(tree, order));

    rb_current(tree) = node
	= _rb_select(rb_root(tree, order), order, k);
    return rb_data(node, order);
}


int bu_rb_rank(struct bu_rb_tree *tree, int order)
{
    int rank;
    struct bu_rb_node *node;
    struct bu_rb_node *parent;
    struct bu_rb_node *root;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((node = rb_current(tree)) == rb_null(tree))
	return 0;

    root = rb_root(tree, order);
    rank = rb_size(rb_left_child(node, order), order) + 1;
    while (node != root) {
	parent = rb_parent(node, order);
	if (node == rb_right_child(parent, order))
	    rank += rb_size(rb_left_child(parent, order), order) + 1;
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
