/*                     R B _ D E L E T E . C
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

#include "bu.h"
#include "./rb_internals.h"


/**
 * Restore the red-black properties of a red-black tree after the
 * splicing out of a node
 *
 * This function has three parameters: the tree to be fixed up, the
 * node where the trouble occurs, and the order.  _rb_fixup() is an
 * implementation of the routine RB-DELETE-FIXUP on p. 274 of Cormen
 * et al. (p. 326 in the paperback version of the 2009 edition).
 */
HIDDEN void
_rb_fixup(struct bu_rb_tree *tree, struct bu_rb_node *node, int order)
{
    int direction;
    struct bu_rb_node *parent;
    struct bu_rb_node *w;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(tree, order);

    while ((node != RB_ROOT(tree, order))
	   && (RB_GET_COLOR(node, order) == RB_BLK))
    {
	parent = RB_PARENT(node, order);
	if (node == RB_LEFT_CHILD(parent, order))
	    direction = RB_LEFT;
	else
	    direction = RB_RIGHT;

	w = RB_OTHER_CHILD(parent, order, direction);
	if (RB_GET_COLOR(w, order) == RB_RED) {
	    RB_SET_COLOR(w, order, RB_BLK);
	    RB_SET_COLOR(parent, order, RB_RED);
	    RB_ROTATE(parent, order, direction);
	    w = RB_OTHER_CHILD(parent, order, direction);
	}
	if ((RB_GET_COLOR(RB_CHILD(w, order, direction), order) == RB_BLK)
	    && (RB_GET_COLOR(RB_OTHER_CHILD(w, order, direction), order) == RB_BLK))
	{
	    RB_SET_COLOR(w, order, RB_RED);
	    node = parent;
	} else {
	    if (RB_GET_COLOR(RB_OTHER_CHILD(w, order, direction), order) == RB_BLK) {
		RB_SET_COLOR(RB_CHILD(w, order, direction), order, RB_BLK);
		RB_SET_COLOR(w, order, RB_RED);
		RB_OTHER_ROTATE(w, order, direction);
		w = RB_OTHER_CHILD(parent, order, direction);
	    }
	    RB_SET_COLOR(w, order, RB_GET_COLOR(parent, order));
	    RB_SET_COLOR(parent, order, RB_BLK);
	    RB_SET_COLOR(RB_OTHER_CHILD(w, order, direction),
			 order, RB_BLK);
	    RB_ROTATE(parent, order, direction);
	    node = RB_ROOT(tree, order);
	}
    }
    RB_SET_COLOR(node, order, RB_BLK);
}

/**
 * _R B _ D E L E T E ()
 *
 * Delete a node from one order of a red-black tree
 *
 * This function has three parameters: a tree, the node to delete, and
 * the order from which to delete it.  _rb_delete() is an
 * implementation of the routine RB-DELETE on p. 273 of Cormen et
 * al. (p. 324 in the paperback version of the 2009 edition).
 */
HIDDEN void
_rb_delete(struct bu_rb_tree *tree, struct bu_rb_node *node, int order)
{
    struct bu_rb_node *y;		/* The node to splice out */
    struct bu_rb_node *parent;
    struct bu_rb_node *only_child;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(tree, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_DELETE))
	bu_log("_rb_delete(%p, %p, %d): data=%p\n",
	       (void*)tree, (void*)node, order, RB_DATA(node, order));

    if ((RB_LEFT_CHILD(node, order) == RB_NULL(tree))
	|| (RB_RIGHT_CHILD(node, order) == RB_NULL(tree)))
	y = node;
    else
	y = rb_neighbor(node, order, SENSE_MAX);

    if (RB_LEFT_CHILD(y, order) == RB_NULL(tree))
	only_child = RB_RIGHT_CHILD(y, order);
    else
	only_child = RB_LEFT_CHILD(y, order);

    parent = RB_PARENT(only_child, order) = RB_PARENT(y, order);
    if (parent == RB_NULL(tree))
	RB_ROOT(tree, order) = only_child;
    else if (y == RB_LEFT_CHILD(parent, order))
	RB_LEFT_CHILD(parent, order) = only_child;
    else
	RB_RIGHT_CHILD(parent, order) = only_child;

    /*
     * Splice y out if it's not node
     */
    if (y != node) {
	(node->rbn_package)[order] = (y->rbn_package)[order];
	((node->rbn_package)[order]->rbp_node)[order] = node;
    }

    if (RB_GET_COLOR(y, order) == RB_BLK)
	_rb_fixup(tree, only_child, order);

    if (--(y->rbn_pkg_refs) == 0)
	rb_free_node(y);
}


void
bu_rb_delete(struct bu_rb_tree *tree, int order)
{
    int nm_orders;
    struct bu_rb_node **node;		/* Nodes containing data */
    struct bu_rb_package *package;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (UNLIKELY(tree->rbt_nm_nodes <= 0)) {
	bu_log("ERROR: Attempt to delete from tree with %d nodes\n",
	       tree->rbt_nm_nodes);
	bu_bomb("");
    }
    if (UNLIKELY(RB_CURRENT(tree) == RB_NULL(tree))) {
	bu_log("Warning: bu_rb_delete(): current node is undefined\n");
	return;
    }

    nm_orders = tree->rbt_nm_orders;
    package = (RB_CURRENT(tree)->rbn_package)[order];

    node = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *), "node list");

    for (order = 0; order < nm_orders; ++order)
	node[order] = (package->rbp_node)[order];

    /*
     * Do the deletion from each order
     */
    for (order = 0; order < nm_orders; ++order)
	_rb_delete(tree, node[order], order);

    --(tree->rbt_nm_nodes);
    rb_free_package(package);
    bu_free((genptr_t) node, "node list");
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
