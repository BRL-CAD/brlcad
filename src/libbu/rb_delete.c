/*                     R B _ D E L E T E . C
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
 * _ R B _ F I X U P ()
 *
 * Restore the red-black properties of a red-black tree after the
 * splicing out of a node
 *
 * This function has three parameters: the tree to be fixed up, the
 * node where the trouble occurs, and the order.  _rb_fixup() is an
 * implementation of the routine RB-DELETE-FIXUP on p. 274 of Cormen
 * et al.
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

    while ((node != rb_root(tree, order))
	   && (rb_get_color(node, order) == RB_BLK))
    {
	parent = rb_parent(node, order);
	if (node == rb_left_child(parent, order))
	    direction = RB_LEFT;
	else
	    direction = RB_RIGHT;

	w = rb_other_child(parent, order, direction);
	if (rb_get_color(w, order) == RB_RED) {
	    rb_set_color(w, order, RB_BLK);
	    rb_set_color(parent, order, RB_RED);
	    rb_rotate(parent, order, direction);
	    w = rb_other_child(parent, order, direction);
	}
	if ((rb_get_color(rb_child(w, order, direction), order) == RB_BLK)
	    && (rb_get_color(rb_other_child(w, order, direction), order) == RB_BLK))
	{
	    rb_set_color(w, order, RB_RED);
	    node = parent;
	} else {
	    if (rb_get_color(rb_other_child(w, order, direction), order) == RB_BLK) {
		rb_set_color(rb_child(w, order, direction), order, RB_BLK);
		rb_set_color(w, order, RB_RED);
		rb_other_rotate(w, order, direction);
		w = rb_other_child(parent, order, direction);
	    }
	    rb_set_color(w, order, rb_get_color(parent, order));
	    rb_set_color(parent, order, RB_BLK);
	    rb_set_color(rb_other_child(w, order, direction),
			 order, RB_BLK);
	    rb_rotate(parent, order, direction);
	    node = rb_root(tree, order);
	}
    }
    rb_set_color(node, order, RB_BLK);
}

/**
 * _R B _ D E L E T E ()
 *
 * Delete a node from one order of a red-black tree
 *
 * This function has three parameters: a tree, the node to delete,
 * and the order from which to delete it.  _rb_delete() is an
 * implementation of the routine RB-DELETE on p. 273 of Cormen et al.
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
	       tree, node, order, rb_data(node, order));

    if ((rb_left_child(node, order) == rb_null(tree))
	|| (rb_right_child(node, order) == rb_null(tree)))
	y = node;
    else
	y = rb_neighbor(node, order, SENSE_MAX);

    if (rb_left_child(y, order) == rb_null(tree))
	only_child = rb_right_child(y, order);
    else
	only_child = rb_left_child(y, order);

    parent = rb_parent(only_child, order) = rb_parent(y, order);
    if (parent == rb_null(tree))
	rb_root(tree, order) = only_child;
    else if (y == rb_left_child(parent, order))
	rb_left_child(parent, order) = only_child;
    else
	rb_right_child(parent, order) = only_child;

    /*
     * Splice y out if it's not node
     */
    if (y != node) {
	(node->rbn_package)[order] = (y->rbn_package)[order];
	((node->rbn_package)[order]->rbp_node)[order] = node;
    }

    if (rb_get_color(y, order) == RB_BLK)
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
    if (UNLIKELY(rb_current(tree) == rb_null(tree))) {
	bu_log("Warning: bu_rb_delete(): current node is undefined\n");
	return;
    }

    nm_orders = tree->rbt_nm_orders;
    package = (rb_current(tree)->rbn_package)[order];

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
