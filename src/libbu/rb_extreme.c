/*                    R B _ E X T R E M E . C
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
 * _ R B _ E X T R E M E
 *
 * Find the minimum or maximum node in one order of a red-black tree
 *
 * This function has four parameters: the root of the tree, the
 * order, the sense (min or max), and the address to be understood
 * as the nil node pointer. rb_extreme() returns a pointer to the
 * extreme node.
 */
HIDDEN struct bu_rb_node *
_rb_extreme(struct bu_rb_node *root, int order, int sense, struct bu_rb_node *empty_node)
{
    struct bu_rb_node *child;
    struct bu_rb_tree *tree;

    if (root == empty_node)
	return root;

    while (1) {
	BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
	tree = root->rbn_tree;
	RB_CKORDER(tree, order);

	child = (sense == SENSE_MIN) ? rb_left_child(root, order) :
	    rb_right_child(root, order);
	if (child == empty_node)
	    break;
	root = child;
    }

    /* Record the node with which we've been working */
    rb_current(tree) = root;

    return root;
}


void *
bu_rb_extreme(struct bu_rb_tree *tree, int order, int sense)
{
    struct bu_rb_node *node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (UNLIKELY((sense != SENSE_MIN) && (sense != SENSE_MAX))) {
	bu_log("ERROR: bu_rb_extreme(): invalid sense %d, file %s, line %d\n", sense, __FILE__, __LINE__);
	return NULL;
    }

    /* Wade throught the tree */
    node = _rb_extreme(rb_root(tree, order), order, sense, rb_null(tree));

    if (node == rb_null(tree))
	return NULL;
    else
	return rb_data(node, order);
}


struct bu_rb_node *
rb_neighbor(struct bu_rb_node *node, int order, int sense)
{
    struct bu_rb_node *child;
    struct bu_rb_node *parent;
    struct bu_rb_tree *tree;
    struct bu_rb_node *empty_node;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    tree = node->rbn_tree;
    RB_CKORDER(tree, order);

    empty_node = rb_null(tree);

    child = (sense == SENSE_MIN) ? rb_left_child(node, order) :
	rb_right_child(node, order);
    if (child != empty_node)
	return _rb_extreme(child, order, 1 - sense, empty_node);
    parent = rb_parent(node, order);
    while ((parent != empty_node) &&
	   (node == rb_child(parent, order, sense)))
    {
	node = parent;
	parent = rb_parent(parent, order);
    }

    /* Record the node with which we've been working */
    rb_current(tree) = parent;

    return parent;
}


void *
bu_rb_neighbor(struct bu_rb_tree *tree, int order, int sense)
{
    struct bu_rb_node *node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (UNLIKELY((sense != SENSE_MIN) && (sense != SENSE_MAX))) {
	bu_log("ERROR: bu_rb_neighbor(): invalid sense %d, file %s, line %d\n", sense, __FILE__, __LINE__);
	return NULL;
    }

    /* Wade through the tree */
    node = rb_neighbor(rb_current(tree), order, sense);

    if (node == rb_null(tree)) {
	return NULL;
    } else {
	/* Record the node with which we've been working */
	rb_current(tree) = node;
	return rb_data(node, order);
    }
}


void *
bu_rb_curr(struct bu_rb_tree *tree, int order)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (rb_current(tree) == rb_null(tree))
	return NULL;
    else
	return rb_data(rb_current(tree), order);
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
