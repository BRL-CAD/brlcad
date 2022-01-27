/*                           R B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/redblack.h"

/**
 * This internal macro has two parameters: a tree and an order number.
 * It ensures that the order number is valid for the tree.
 */
#define RB_CKORDER(t, o)						\
    if (UNLIKELY(((o) < 0) || ((o) >= (t)->rbt_nm_orders))) {		\
	char buf[256] = {0};						\
	snprintf(buf, 256, "ERROR: Order %d outside 0..%d (nm_orders-1), file %s, line %d\n", \
		 (o), (t)->rbt_nm_orders - 1, __FILE__, __LINE__);	\
	bu_bomb(buf);							\
    }

/*
 * Access functions for fields of struct bu_rb_tree
 */
#define RB_COMPARE_FUNC(t, o) (((t)->rbt_compar)[o])
#define RB_PRINT(t, p) (((t)->rbt_print)((p)->rbp_data))
#define RB_ROOT(t, o) (((t)->rbt_root)[o])
#define RB_CURRENT(t) ((t)->rbt_current)
#define RB_NULL(t) ((t)->rbt_empty_node)
#define RB_GET_UNIQUENESS(t, o) ((((t)->rbt_unique)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0)
#define RB_SET_UNIQUENESS(t, o, u) {		\
	int _b = (o) / 8;			\
	int _p = (o) - _b * 8;			\
	((t)->rbt_unique)[_b] &= ~(0x1 << _p);	\
	((t)->rbt_unique)[_b] |= (u) << _p;	\
    }

/*
 * Access functions for fields of (struct bu_rb_node)
 */
#define RB_PARENT(n, o) (((n)->rbn_parent)[o])
#define RB_LEFT_CHILD(n, o) (((n)->rbn_left)[o])
#define RB_RIGHT_CHILD(n, o) (((n)->rbn_right)[o])
#define RB_LEFT 0
#define RB_RIGHT 1
#define RB_CHILD(n, o, d) (((d) == RB_LEFT) ?		\
			   RB_LEFT_CHILD((n), (o)) :	\
			   RB_RIGHT_CHILD((n), (o)))
#define RB_OTHER_CHILD(n, o, d) (((d) == RB_LEFT) ?		\
				 RB_RIGHT_CHILD((n), (o)) :	\
				 RB_LEFT_CHILD((n), (o)))
#define RB_SIZE(n, o) (((n)->rbn_size)[o])
#define RB_GET_COLOR(n, o)					\
    ((((n)->rbn_color)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0)
#define RB_SET_COLOR(n, o, c) {			\
	int _b = (o) / 8;			\
	int _p = (o) - _b * 8;			\
	((n)->rbn_color)[_b] &= ~(0x1 << _p);	\
	((n)->rbn_color)[_b] |= (c) << _p;	\
    }
#define RB_RED 0
#define RB_BLK 1
#define RB_DATA(n, o) (((n)->rbn_package)[o]->rbp_data)

/**
 * Interface to rb_walk()
 * (Valid values for the parameter what_to_walk)
 */
#define WALK_NODES 0
#define WALK_DATA 1

/**
 * This macro has three parameters: the node about which to rotate,
 * the order to be rotated, and the direction of rotation.  They allow
 * indirection in the use of rb_rot_left() and rb_rot_right().
 */
#define RB_ROTATE(n, o, d) (((d) == RB_LEFT) ?		\
			    rb_rot_left((n), (o)) :	\
			    rb_rot_right((n), (o)))

/**
 * This macro has three parameters: the node about which to rotate,
 * the order to be rotated, and the direction of rotation.  They allow
 * indirection in the use of rb_rot_left() and rb_rot_right().
 */
#define RB_OTHER_ROTATE(n, o, d) (((d) == RB_LEFT) ?		\
				  rb_rot_right((n), (o)) :	\
				  rb_rot_left((n), (o)))


struct bu_rb_tree *
bu_rb_create(const char *description, int nm_orders, bu_rb_cmp_t *compare_funcs)
{
    int order;
    struct bu_rb_tree *tree;

    /*
     * Allocate memory for the tree
     */
    BU_ALLOC(tree, struct bu_rb_tree);

    tree->rbt_root = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node),
		  "red-black roots");
    tree->rbt_unique = (char *)
	bu_malloc((size_t) lrint(ceil((double) (nm_orders / 8.0))),
		  "red-black uniqueness flags");
    RB_NULL(tree) = (struct bu_rb_node *)
	bu_malloc(sizeof(struct bu_rb_node),
		  "red-black empty node");
    RB_NULL(tree)->rbn_parent = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black parents");
    RB_NULL(tree)->rbn_left = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black left children");
    RB_NULL(tree)->rbn_right = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black right children");
    RB_NULL(tree)->rbn_color = (char *)
	bu_malloc((size_t) lrint(ceil((double) (nm_orders / 8.0))),
		  "red-black colors");
    RB_NULL(tree)->rbn_size = (int *)
	bu_malloc(nm_orders * sizeof(int),
		  "red-black subtree sizes");
    RB_NULL(tree)->rbn_package = (struct bu_rb_package **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
		  "red-black packages");
    /*
     * Fill in the tree
     */
    tree->rbt_magic = BU_RB_TREE_MAGIC;
    tree->rbt_description = description;
    tree->rbt_nm_orders = nm_orders;
    tree->rbt_compar = compare_funcs;
    tree->rbt_print = 0;
    bu_rb_uniq_all_off(tree);
    tree->rbt_debug = 0x0;
    tree->rbt_current = RB_NULL(tree);
    for (order = 0; order < nm_orders; ++order)
	RB_ROOT(tree, order) = RB_NULL(tree);
    BU_RB_LIST_INIT(tree->rbt_nodes, rbl_node);
    BU_RB_LIST_INIT(tree->rbt_packages, rbl_node);

    /*
     * Initialize the nil sentinel
     */
    RB_NULL(tree)->rbn_magic = BU_RB_NODE_MAGIC;
    RB_NULL(tree)->rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order) {
	RB_PARENT(RB_NULL(tree), order) = BU_RB_NODE_NULL;
	RB_SET_COLOR(RB_NULL(tree), order, RB_BLK);
	RB_LEFT_CHILD(RB_NULL(tree), order) = BU_RB_NODE_NULL;
	RB_RIGHT_CHILD(RB_NULL(tree), order) = BU_RB_NODE_NULL;
	RB_SIZE(RB_NULL(tree), order) = 0;
	(RB_NULL(tree)->rbn_package)[order] = BU_RB_PKG_NULL;
    }

    return tree;
}

void
rb_free_node(struct bu_rb_node *node)
{
    struct bu_rb_tree *tree;
    size_t rbn_list_pos = node->rbn_list_pos;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    tree = node->rbn_tree;
    if (RB_CURRENT(tree) == node)
	RB_CURRENT(tree) = RB_NULL(tree);
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    /*
     * Remove node from the list of all nodes
     */

    bu_free((void *) node->rbn_parent, "red-black parents");
    bu_free((void *) node->rbn_left, "red-black left children");
    bu_free((void *) node->rbn_right, "red-black right children");
    bu_free((void *) node->rbn_color, "red-black colors");
    bu_free((void *) node->rbn_size, "red-black size");
    bu_free((void *) node->rbn_package, "red-black packages");
    bu_free((void *) node, "red-black node");

    memmove(tree->rbt_nodes.rbl_node + rbn_list_pos,
	    tree->rbt_nodes.rbl_node + rbn_list_pos + 1,
	    sizeof tree->rbt_nodes.rbl_node[0] * (tree->rbt_nodes.size - rbn_list_pos));
    tree->rbt_nodes.size--;
}


void
rb_free_package(struct bu_rb_package *package)
{
    struct bu_rb_tree *tree;
    size_t rbp_list_pos = package->rbp_list_pos;

    BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");

    tree = (*package->rbp_node)->rbn_tree;

    /*
     * Remove node from the list of all packages
     */
    bu_free((void *) package->rbp_node, "red-black package nodes");
    bu_free((void *) package, "red-black package");

    memmove(tree->rbt_nodes.rbl_package + rbp_list_pos,
	    tree->rbt_nodes.rbl_package + rbp_list_pos + 1,
	    sizeof tree->rbt_nodes.rbl_package[0] * (tree->rbt_nodes.size - rbp_list_pos));
    tree->rbt_nodes.size--;
}

void
bu_rb_free(struct bu_rb_tree *tree, void (*free_data)(void *))
{
    struct bu_rb_node *node;
    struct bu_rb_package *package;
    size_t i;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    /*
     * Free all the nodes
     */
    for (i = 0; i < tree->rbt_nodes.size; i++) {
	node = tree->rbt_nodes.rbl_node[i];
	rb_free_node(node);
    }

    /*
     * Free all the packages
     */
    for (i = 0; i < tree->rbt_packages.size; i++) {
	package = tree->rbt_packages.rbl_package[i];
	BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");
	if (free_data)
	    free_data(package->rbp_data);
	rb_free_package(package);
    }

    /*
     * Free the tree's NIL sentinel
     */
    node = tree->rbt_empty_node;
    bu_free((void *) node->rbn_left, "red-black left children");
    bu_free((void *) node->rbn_right, "red-black right children");
    bu_free((void *) node->rbn_parent, "red-black parents");
    bu_free((void *) node->rbn_color, "red-black colors");
    bu_free((void *) node->rbn_size, "red-black size");
    bu_free((void *) node->rbn_package, "red-black packages");
    bu_free((void *) node, "red-black empty node");

    /*
     * Free the tree itself
     */
    bu_free((void *) tree->rbt_root, "red-black roots");
    bu_free((void *) tree->rbt_unique, "red-black uniqueness flags");
    bu_free((void *) tree, "red-black tree");
}


void
rb_rot_left(struct bu_rb_node *x, int order)
{
    struct bu_rb_node *y;		/* x's child to pivot up */
    struct bu_rb_node *beta;		/* y's child in direction of rot. */
    struct bu_rb_node *x_parent;	/* x's parent */
    struct bu_rb_tree *tree = x->rbn_tree;	/* Tree where it all happens */

    /*
     * Set y and check data types of both x and y
     */
    BU_CKMAG(x, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(x->rbn_tree, order);

    y = RB_RIGHT_CHILD(x, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_left(<%p>, %d)...\n", (void*)x, order);

    RB_RIGHT_CHILD(x, order) = beta = RB_LEFT_CHILD(y, order);
    if (beta != RB_NULL(tree))
	RB_PARENT(beta, order) = x;
    RB_PARENT(y, order) = x_parent = RB_PARENT(x, order);
    if (x_parent == RB_NULL(tree))
	RB_ROOT(tree, order) = y;
    else if (x == RB_LEFT_CHILD(x_parent, order))
	RB_LEFT_CHILD(x_parent, order) = y;
    else
	RB_RIGHT_CHILD(x_parent, order) = y;
    RB_LEFT_CHILD(y, order) = x;
    RB_PARENT(x, order) = y;

    RB_SIZE(y, order) = RB_SIZE(x, order);
    RB_SIZE(x, order) =
	RB_SIZE(RB_LEFT_CHILD(x, order), order) +
	RB_SIZE(RB_RIGHT_CHILD(x, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       (void*)x, order, RB_SIZE(x, order), (void*)y, order, RB_SIZE(y, order));
}


void rb_rot_right (struct bu_rb_node *y, int order)
{
    struct bu_rb_node *x;		/* y's child to pivot up */
    struct bu_rb_node *beta;		/* x's child in direction of rot. */
    struct bu_rb_node *y_parent;	/* y's parent */
    struct bu_rb_tree *tree = y->rbn_tree;	/* Tree where it all happens */

    /*
     * Set x and check data types of both x and y
     */
    BU_CKMAG(y, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(y->rbn_tree, order);

    x = RB_LEFT_CHILD(y, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_right(<%p>, %d)...\n", (void*)y, order);

    RB_LEFT_CHILD(y, order) = beta = RB_RIGHT_CHILD(x, order);
    if (beta != RB_NULL(tree))
	RB_PARENT(beta, order) = y;
    RB_PARENT(x, order) = y_parent = RB_PARENT(y, order);
    if (y_parent == RB_NULL(tree))
	RB_ROOT(tree, order) = x;
    else if (y == RB_LEFT_CHILD(y_parent, order))
	RB_LEFT_CHILD(y_parent, order) = x;
    else
	RB_RIGHT_CHILD(y_parent, order) = x;
    RB_RIGHT_CHILD(x, order) = y;
    RB_PARENT(y, order) = x;

    RB_SIZE(x, order) = RB_SIZE(y, order);
    RB_SIZE(y, order) =
	RB_SIZE(RB_LEFT_CHILD(y, order), order) +
	RB_SIZE(RB_RIGHT_CHILD(y, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       (void*)x, order, RB_SIZE(x, order), (void*)y, order, RB_SIZE(y, order));
}


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

	child = (sense == SENSE_MIN) ? RB_LEFT_CHILD(root, order) :
	    RB_RIGHT_CHILD(root, order);
	if (child == empty_node)
	    break;
	root = child;
    }

    /* Record the node with which we've been working */
    RB_CURRENT(tree) = root;

    return root;
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

    empty_node = RB_NULL(tree);

    child = (sense == SENSE_MIN) ? RB_LEFT_CHILD(node, order) :
	RB_RIGHT_CHILD(node, order);
    if (child != empty_node)
	return _rb_extreme(child, order, 1 - sense, empty_node);
    parent = RB_PARENT(node, order);
    while ((parent != empty_node) &&
	   (node == RB_CHILD(parent, order, sense)))
    {
	node = parent;
	parent = RB_PARENT(parent, order);
    }

    /* Record the node with which we've been working */
    RB_CURRENT(tree) = parent;

    return parent;
}


/**
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
    bu_free((void *) node, "node list");
}

static int d_order;	/* Used by describe_node() */


/**
 * Print out the contents of a red-black node
 *
 * This function has two parameters: the node to describe and its
 * depth in the tree.  _rb_describe_node() is intended to be called by
 * bu_rb_diagnose_tree().
 */
HIDDEN void
_rb_describe_node(struct bu_rb_node *node, int depth)
{
    struct bu_rb_tree *tree;
    struct bu_rb_package *package;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    tree = node->rbn_tree;
    RB_CKORDER(tree, d_order);

    package = (node->rbn_package)[d_order];

    bu_log("%*snode <%p>...\n", depth * 2, "", (void*)node);
    bu_log("%*s  tree:    <%p>\n", depth * 2, "", (void*)node->rbn_tree);
    bu_log("%*s  parent:  <%p>\n", depth * 2, "", (void*)RB_PARENT(node, d_order));
    bu_log("%*s  left:    <%p>\n", depth * 2, "", (void*)RB_LEFT_CHILD(node, d_order));
    bu_log("%*s  right:   <%p>\n", depth * 2, "", (void*)RB_RIGHT_CHILD(node, d_order));
    bu_log("%*s  color:   %s\n", depth * 2, "", (RB_GET_COLOR(node, d_order) == RB_RED) ? "RED" : (RB_GET_COLOR(node, d_order) == RB_BLK) ? "BLACK" : "Huh?");
    bu_log("%*s  package: <%p>\n", depth * 2, "", (void*)package);

    if ((tree->rbt_print != 0) && (package != BU_RB_PKG_NULL))
	(*tree->rbt_print)(package->rbp_data);
    else
	bu_log("\n");
}

/**
 * Perform a preorder traversal of a red-black tree
 */
HIDDEN void
prewalknodes(struct bu_rb_node *root,
	     int order,
	     void (*visit)(void),
	     int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    _visit(root, depth);

    prewalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    prewalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform an inorder traversal of a red-black tree
 */
HIDDEN void
inwalknodes(struct bu_rb_node *root,
	    int order,
	    void (*visit)(void),
	    int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    inwalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);

    _visit(root, depth);

    inwalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform a postorder traversal of a red-black tree
 */
HIDDEN void
postwalknodes(struct bu_rb_node *root,
	      int order,
	      void (*visit)(void),
	      int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    postwalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    postwalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);

    _visit(root, depth);
}


/**
 * Perform a preorder traversal of a red-black tree
 */
HIDDEN void
prewalkdata(struct bu_rb_node *root,
	    int order,
	    void (*visit)(void),
	    int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    _visit(RB_DATA(root, order), depth);

    prewalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    prewalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform an inorder traversal of a red-black tree
 */
HIDDEN void
inwalkdata(struct bu_rb_node *root,
	   int order,
	   void (*visit)(void),
	   int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    inwalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);

    _visit(RB_DATA(root, order), depth);

    inwalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform a postorder traversal of a red-black tree
 */
HIDDEN void
postwalkdata(struct bu_rb_node *root,
	     int order,
	     void (*visit)(void),
	     int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    postwalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    postwalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);

    _visit(RB_DATA(root, order), depth);
}


void
rb_walk(struct bu_rb_tree *tree,
	int order,
	void (*visit)(void),
	int what_to_visit,
	int trav_type)
{
    static void (*walk[][3])(void) = {
	{ BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(prewalknodes),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(inwalknodes),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(postwalknodes)
	},
	{ BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(prewalkdata),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(inwalkdata),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(postwalkdata)
	}
    };

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    switch (trav_type) {
	case BU_RB_WALK_PREORDER:
	case BU_RB_WALK_INORDER:
	case BU_RB_WALK_POSTORDER:
	    switch (what_to_visit) {
		case WALK_NODES:
		case WALK_DATA: {
		    BU_RB_WALK_FUNC_FUNC_DECL(_walk_func);
		    _walk_func = BU_RB_WALK_FUNC_CAST_AS_FUNC_FUNC(walk[what_to_visit][trav_type]);
		    _walk_func(RB_ROOT(tree, order), order, visit, 0);
		}
		    break;
		default:
		    bu_log("ERROR: rb_walk(): Illegal visitation object: %d\n",
			   what_to_visit);
		    bu_bomb("");
	    }
	    break;
	default:
	    bu_log("ERROR: rb_walk(): Illegal traversal type: %d\n",
		   trav_type);
	    bu_bomb("");
    }
}


void
bu_rb_diagnose_tree(struct bu_rb_tree *tree, int order, int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    bu_log("-------- Red-black tree <%p> contents --------\n", (void*)tree);
    bu_log("Description: '%s'\n", tree->rbt_description);
    bu_log("Order:       %d of %d\n", order, tree->rbt_nm_orders);
    bu_log("Current:     <%p>\n", (void*)tree->rbt_current);
    bu_log("Empty node:  <%p>\n", (void*)tree->rbt_empty_node);
    bu_log("Uniqueness?:  %s\n", RB_GET_UNIQUENESS(tree, order) ? "Yes" : "No");
    d_order = order;
    rb_walk(tree, order, BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(_rb_describe_node),
	    WALK_NODES, trav_type);
    bu_log("--------------------------------------------------\n");
}

void
bu_rb_summarize_tree(struct bu_rb_tree *tree)
{
    int i;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    bu_log("-------- Red-black tree <%p> summary --------\n", (void*)tree);
    bu_log("Description:      '%s'\n", tree->rbt_description);
    bu_log("Current:          <%p>\n", (void*)tree->rbt_current);
    bu_log("Empty node:       <%p>\n", (void*)tree->rbt_empty_node);
    bu_log("Size (in nodes):  %d\n", tree->rbt_nm_nodes);
    bu_log("Number of orders: %d\n", tree->rbt_nm_orders);
    bu_log("Debug bits:       <0x%X>\n", tree->rbt_debug);
    if ((tree->rbt_nm_orders > 0) && (tree->rbt_nm_nodes > 0)) {
	bu_log("\n");
	bu_log("                                 Order Attributes\n");
	bu_log("\n");
	bu_log("+-------+------------------+-----------+--------------+--------------+--------------+\n");
	bu_log("| Order | Compare Function |  Unique?  |     Root     |   Package    |     Data     |\n");
	bu_log("+-------+------------------+-----------+--------------+--------------+--------------+\n");
	for (i = 0; i < tree->rbt_nm_orders; ++i) {
	    bu_log("| %3d   |   <%p>   |    %-3.3s    | <%p> | <%p> | <%p> |\n",
		   i,
		   (void *)(uintptr_t)RB_COMPARE_FUNC(tree, i),
		   RB_GET_UNIQUENESS(tree, i) ? "Yes" : "No",
		   (void *)RB_ROOT(tree, i),
		   (RB_ROOT(tree, i) == BU_RB_NODE_NULL) ? NULL : (void *)(RB_ROOT(tree, i)->rbn_package)[i],
		   (RB_ROOT(tree, i) == BU_RB_NODE_NULL) ? NULL : RB_DATA(RB_ROOT(tree, i), i));
	}
	bu_log("+-------+------------------+-----------+--------------+--------------+--------------+\n");
    }
    bu_log("\n");
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

    /* Wade through the tree */
    node = _rb_extreme(RB_ROOT(tree, order), order, sense, RB_NULL(tree));

    if (node == RB_NULL(tree))
	return NULL;
    else
	return RB_DATA(node, order);
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
    node = rb_neighbor(RB_CURRENT(tree), order, sense);

    if (node == RB_NULL(tree)) {
	return NULL;
    } else {
	/* Record the node with which we've been working */
	RB_CURRENT(tree) = node;
	return RB_DATA(node, order);
    }
}


void *
bu_rb_curr(struct bu_rb_tree *tree, int order)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (RB_CURRENT(tree) == RB_NULL(tree))
	return NULL;
    else
	return RB_DATA(RB_CURRENT(tree), order);
}

/**
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
    if (tree->rbt_packages.capacity == 0) {
	tree->rbt_packages.capacity = BU_RB_LIST_INIT_CAPACITY;
	tree->rbt_packages.rbl_package = (struct bu_rb_package **)bu_malloc(
	    sizeof tree->rbt_packages.rbl_package[0] * tree->rbt_packages.capacity,
	    "initial rb list init");
    } else if (tree->rbt_packages.size == tree->rbt_packages.capacity) {
	tree->rbt_packages.capacity *= 2;
	tree->rbt_packages.rbl_package = (struct bu_rb_package **)bu_realloc(
	    tree->rbt_packages.rbl_package,
	    sizeof tree->rbt_packages.rbl_package[0] * tree->rbt_packages.capacity,
	    "initial rb list init");
    }

    BU_ALLOC(package, struct bu_rb_package);
    package->rbp_node = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black package nodes");

    package->rbp_list_pos = tree->rbt_packages.size;
    tree->rbt_packages.rbl_package[tree->rbt_packages.size] = package;
    tree->rbt_packages.size++;

    /*
     * Make a new node and add it to the list of all nodes.
     */
    if (tree->rbt_nodes.capacity == 0) {
	tree->rbt_nodes.capacity = BU_RB_LIST_INIT_CAPACITY;
	tree->rbt_nodes.rbl_node = (struct bu_rb_node **)bu_malloc(
	    sizeof tree->rbt_nodes.rbl_node[0] * tree->rbt_nodes.capacity,
	    "initial rb list init");
    } else if (tree->rbt_nodes.size == tree->rbt_nodes.capacity) {
	tree->rbt_nodes.capacity *= 2;
	tree->rbt_nodes.rbl_node = (struct bu_rb_node **)bu_realloc(
	    tree->rbt_nodes.rbl_node,
	    sizeof tree->rbt_nodes.rbl_node[0] * tree->rbt_nodes.capacity,
	    "initial rb list init");
    }

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
	bu_malloc((size_t) lrint(ceil((double) (nm_orders / 8.0))),
		  "red-black colors");
    node->rbn_size = (int *)
	bu_malloc(nm_orders * sizeof(int),
		  "red-black subtree sizes");
    node->rbn_package = (struct bu_rb_package **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
		  "red-black packages");

    node->rbn_list_pos = tree->rbt_nodes.size;
    tree->rbt_nodes.rbl_node[tree->rbt_nodes.size] = node;
    tree->rbt_nodes.size++;

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

void
bu_rb_walk(struct bu_rb_tree *tree,
	   int order,
	   void (*visit)(void),
	   int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    rb_walk(tree, order, visit, WALK_DATA, trav_type);
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
