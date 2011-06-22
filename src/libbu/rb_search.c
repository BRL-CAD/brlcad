/*                     R B _ S E A R C H . C
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
 * _ R B _ S E A R C H ()
 *
 * Search for a node in a red-black tree
 *
 * This function has four parameters: the root and order of the tree
 * in which to search, the comparison function, and a data block
 * containing the desired value of the key.  On success, _rb_search()
 * returns a pointer to the discovered node.  Otherwise, it returns
 * (tree->rbt_empty_node).
 */
HIDDEN struct bu_rb_node *
_rb_search(struct bu_rb_node *root, int order_nm, int (*order) (/* ??? */), void *data)
{
    int result;
    struct bu_rb_tree *tree;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    tree = root->rbn_tree;
    RB_CKORDER(tree, order_nm);

    while (1) {
	if (root == rb_null(root->rbn_tree))
	    break;
	if ((result = (*order)(data, rb_data(root, order_nm))) == 0)
	    break;
	else if (result < 0)
	    root = rb_left_child(root, order_nm);
	else	/* result > 0 */
	    root = rb_right_child(root, order_nm);
	BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    }
    rb_current(tree) = root;
    return root;
}


void *bu_rb_search (struct bu_rb_tree *tree, int order, void *data)
{

    int (*compare)();
    struct bu_rb_node *node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    compare = rb_order_func(tree, order);
    node = _rb_search(rb_root(tree, order), order, compare, data);
    if (node == rb_null(tree))
	return NULL;
    else
	return rb_data(node, order);
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
