/*                     R B _ S E A R C H . C
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
 * Search for a node in a red-black tree
 *
 * This function has four parameters: the root and order of the tree
 * in which to search, the comparison function, and a data block
 * containing the desired value of the key.  On success, _rb_search()
 * returns a pointer to the discovered node.  Otherwise, it returns
 * (tree->rbt_empty_node).
 */
HIDDEN struct bu_rb_node *
_rb_search(struct bu_rb_node *root, int order_nm, int (*compare)(const void *, const void *), void *data)
{
    int result;
    struct bu_rb_tree *tree;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    tree = root->rbn_tree;
    RB_CKORDER(tree, order_nm);

    while (1) {
	if (root == RB_NULL(root->rbn_tree))
	    break;
	if ((result = compare(data, RB_DATA(root, order_nm))) == 0)
	    break;
	else if (result < 0)
	    root = RB_LEFT_CHILD(root, order_nm);
	else	/* result > 0 */
	    root = RB_RIGHT_CHILD(root, order_nm);
	BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    }
    RB_CURRENT(tree) = root;
    return root;
}


void *
bu_rb_search (struct bu_rb_tree *tree, int order, void *data)
{

    int (*compare)(const void *, const void *);
    struct bu_rb_node *node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    compare = RB_COMPARE_FUNC(tree, order);
    node = _rb_search(RB_ROOT(tree, order), order, compare, data);
    if (node == RB_NULL(tree))
	return NULL;
    else
	return RB_DATA(node, order);
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
