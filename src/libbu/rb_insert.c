/*                     R B _ I N S E R T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2013 United States Government as represented by
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
 * _ R B _ I N S E R T ()
 *
 * Insert a node into one linear order of a red-black tree
 *
 * This function has three parameters: the tree and linear order into
 * which to insert the new node and the new node itself.  If the node
 * is equal (modulo the linear order) to a node already in the tree,
 * _rb_insert() returns 1.  Otherwise, it returns 0.
 */
HIDDEN int
_rb_insert(struct bu_rb_tree *tree, int order, struct bu_rb_node *new_node)
{
    struct bu_rb_node *node;
    struct bu_rb_node *parent;
    struct bu_rb_node *grand_parent;
    struct bu_rb_node *y;
    int (*compare)(const void *, const void *);
    int comparison = 0xdeadbeef;
    int direction;
    int result = 0;


    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    BU_CKMAG(new_node, BU_RB_NODE_MAGIC, "red-black node");

    /*
     * Initialize the new node
     */
    RB_PARENT(new_node, order) =
	RB_LEFT_CHILD(new_node, order) =
	RB_RIGHT_CHILD(new_node, order) = RB_NULL(tree);
    RB_SIZE(new_node, order) = 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("_rb_insert(%p): size(%p, %d)=%d\n",
	       (void*)new_node, (void*)new_node, order, RB_SIZE(new_node, order));

    /*
     * Perform vanilla-flavored binary-tree insertion
     */
    parent = RB_NULL(tree);
    node = RB_ROOT(tree, order);
    compare = RB_COMPARE_FUNC(tree, order);
    while (node != RB_NULL(tree)) {
	parent = node;
	++RB_SIZE(parent, order);
	if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	    bu_log("_rb_insert(%p): size(%p, %d)=%d\n",
		   (void*)new_node, (void*)parent, order, RB_SIZE(parent, order));
	comparison = compare(RB_DATA(new_node, order),
			     RB_DATA(node, order));
	if (comparison < 0) {
	    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_INSERT))
		bu_log("_rb_insert(%p): <_%d <%p>, going left\n",
		       (void*)new_node, order, (void*)node);
	    node = RB_LEFT_CHILD(node, order);
	} else {
	    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_INSERT))
		bu_log("_rb_insert(%p): >=_%d <%p>, going right\n",
		       (void*)new_node, order, (void*)node);
	    node = RB_RIGHT_CHILD(node, order);
	    if (comparison == 0)
		result = 1;
	}
    }
    RB_PARENT(new_node, order) = parent;
    if (parent == RB_NULL(tree))
	RB_ROOT(tree, order) = new_node;
    else if ((compare(RB_DATA(new_node, order),
		      RB_DATA(parent, order))) < 0)
	RB_LEFT_CHILD(parent, order) = new_node;
    else
	RB_RIGHT_CHILD(parent, order) = new_node;

    /*
     * Reestablish the red-black properties, as necessary
     */
    RB_SET_COLOR(new_node, order, RB_RED);
    node = new_node;
    parent = RB_PARENT(node, order);
    grand_parent = RB_PARENT(parent, order);
    while ((node != RB_ROOT(tree, order))
	   && (RB_GET_COLOR(parent, order) == RB_RED))
    {
	if (parent == RB_LEFT_CHILD(grand_parent, order))
	    direction = RB_LEFT;
	else
	    direction = RB_RIGHT;

	y = RB_OTHER_CHILD(grand_parent, order, direction);
	if (RB_GET_COLOR(y, order) == RB_RED) {
	    RB_SET_COLOR(parent, order, RB_BLK);
	    RB_SET_COLOR(y, order, RB_BLK);
	    RB_SET_COLOR(grand_parent, order, RB_RED);
	    node = grand_parent;
	    parent = RB_PARENT(node, order);
	    grand_parent = RB_PARENT(parent, order);
	} else {
	    if (node == RB_OTHER_CHILD(parent, order, direction)) {
		node = parent;
		RB_ROTATE(node, order, direction);
		parent = RB_PARENT(node, order);
		grand_parent = RB_PARENT(parent, order);
	    }
	    RB_SET_COLOR(parent, order, RB_BLK);
	    RB_SET_COLOR(grand_parent, order, RB_RED);
	    RB_OTHER_ROTATE(grand_parent, order, direction);
	}
    }
    RB_SET_COLOR(RB_ROOT(tree, order), order, RB_BLK);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_INSERT))
	bu_log("_rb_insert(%p): comparison = %d, returning %d\n",
	       (void*)new_node, comparison, result);

    return result;
}


int
bu_rb_insert(struct bu_rb_tree *tree, void *data)
{
    int nm_orders;
    int order;
    int result = 0;
    struct bu_rb_node *node;
    struct bu_rb_package *package;
    struct bu_rb_list *rblp;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree->rbt_nm_orders;

    /*
     * Enforce uniqueness
     *
     * NOTE: The approach is that for each order that requires
     * uniqueness, we look for a match.  This is not the most
     * efficient way to do things, since _rb_insert() is just going to
     * turn around and search the tree all over again.
     */
    for (order = 0; order < nm_orders; ++order) {
	if (RB_GET_UNIQUENESS(tree, order) &&
	    (bu_rb_search(tree, order, data) != NULL))
	{
	    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_UNIQ))
		bu_log("bu_rb_insert(<%p>, <%p>, TBD) will return %d\n",
		       (void*)tree, (void*)data, -(order + 1));
	    return -(order + 1);
	}
    }

    /*
     * Make a new package and add it to the list of all packages.
     */
    BU_ALLOC(package, struct bu_rb_package);
    package->rbp_node = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black package nodes");

    BU_ALLOC(rblp, struct bu_rb_list);
    rblp->rbl_magic = BU_RB_LIST_MAGIC;
    rblp->rbl_package = package;

    BU_LIST_PUSH(&(tree->rbt_packages.l), rblp);
    package->rbp_list_pos = rblp;

    /*
     * Make a new node and add it to the list of all nodes.
     */
    node = (struct bu_rb_node *)
	bu_malloc(sizeof(struct bu_rb_node), "red-black node");
    node->rbn_parent = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black parents");
    node->rbn_left = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black left children");
    node->rbn_right = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black right children");
    node->rbn_color = (char *)
	bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
		  "red-black colors");
    node->rbn_size = (int *)
	bu_malloc(nm_orders * sizeof(int),
		  "red-black subtree sizes");
    node->rbn_package = (struct bu_rb_package **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
		  "red-black packages");

    BU_ALLOC(rblp, struct bu_rb_list);
    rblp->rbl_magic = BU_RB_LIST_MAGIC;
    rblp->rbl_node = node;

    BU_LIST_PUSH(&(tree->rbt_nodes.l), rblp);
    node->rbn_list_pos = rblp;

    /*
     * Fill in the package
     */
    package->rbp_magic = BU_RB_PKG_MAGIC;
    package->rbp_data = data;
    for (order = 0; order < nm_orders; ++order)
	(package->rbp_node)[order] = node;

    /*
     * Fill in the node
     */
    node->rbn_magic = BU_RB_NODE_MAGIC;
    node->rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
	(node->rbn_package)[order] = package;
    node->rbn_pkg_refs = nm_orders;

    /*
     * If the tree was empty, install this node as the root and give
     * it a null parent and null children
     */
    if (RB_ROOT(tree, 0) == RB_NULL(tree)) {
	for (order = 0; order < nm_orders; ++order) {
	    RB_ROOT(tree, order) = node;
	    RB_PARENT(node, order) =
		RB_LEFT_CHILD(node, order) =
		RB_RIGHT_CHILD(node, order) = RB_NULL(tree);
	    RB_SET_COLOR(node, order, RB_BLK);
	    RB_SIZE(node, order) = 1;
	    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
		bu_log("bu_rb_insert(<%p>, <%p>, <%p>): size(%p, %d)=%d\n",
		       (void*)tree, (void*)data, (void*)node, (void*)node, order, RB_SIZE(node, order));
	}
    } else {
	/* Otherwise, insert the node into the tree */
	for (order = 0; order < nm_orders; ++order)
	    result += _rb_insert(tree, order, node);
	if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_UNIQ))
	    bu_log("bu_rb_insert(<%p>, <%p>, <%p>) will return %d\n",
		   (void*)tree, (void*)data, (void*)node, result);
    }

    ++(tree->rbt_nm_nodes);
    RB_CURRENT(tree) = node;
    return result;
}


/**
 * _ R B _ S E T _ U N I Q ()
 *
 * Raise or lower the uniqueness flag for one linear order of a
 * red-black tree
 *
 * This function has three parameters: the tree, the order for which
 * to modify the flag, and the new value for the flag.  _rb_set_uniq()
 * sets the specified flag to the specified value and returns the
 * previous value of the flag.
 */
HIDDEN int
_rb_set_uniq(struct bu_rb_tree *tree, int order, int new_value)
{
    int prev_value;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    new_value = (new_value != 0);

    prev_value = RB_GET_UNIQUENESS(tree, order);
    RB_SET_UNIQUENESS(tree, order, new_value);
    return prev_value;
}


int
bu_rb_uniq_on(struct bu_rb_tree *tree, int order)
{
    return _rb_set_uniq(tree, order, 1);
}

int
bu_rb_uniq_off(struct bu_rb_tree *tree, int order)
{
    return _rb_set_uniq(tree, order, 0);
}


int
bu_rb_is_uniq(struct bu_rb_tree *tree, int order)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    return RB_GET_UNIQUENESS(tree, order);
}


void
bu_rb_set_uniqv(struct bu_rb_tree *tree, bitv_t flag_rep)
{
    int nm_orders;
    int order;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree->rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	RB_SET_UNIQUENESS(tree, order, 0);

    for (order = 0; (flag_rep != 0) && (order < nm_orders); flag_rep >>= 1,
	     ++order)
	if (flag_rep & 0x1)
	    RB_SET_UNIQUENESS(tree, order, 1);

    if (UNLIKELY(flag_rep != 0))
	bu_log("bu_rb_set_uniqv(): Ignoring bits beyond rightmost %d\n", nm_orders);
}


/**
 * _ R B _ S E T _ U N I Q _ A L L
 *
 * Raise or lower the uniqueness flags for all the linear orders of a
 * red-black tree
 *
 * This function has two parameters: the tree, and the new value for
 * all the flags.
 */
HIDDEN void
_rb_set_uniq_all(struct bu_rb_tree *tree, int new_value)
{
    int nm_orders;
    int order;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    new_value = (new_value != 0);

    nm_orders = tree->rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	RB_SET_UNIQUENESS(tree, order, new_value);
}


void bu_rb_uniq_all_on(struct bu_rb_tree *tree)
{
    _rb_set_uniq_all(tree, 1);
}


void bu_rb_uniq_all_off(struct bu_rb_tree *tree)
{
    _rb_set_uniq_all(tree, 0);
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
