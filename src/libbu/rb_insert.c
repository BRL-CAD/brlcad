/*                     R B _ I N S E R T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup rb */
/** @{ */
/** @file rb_insert.c
 *
 *		Routines to insert into a red-black tree
 *
 *  @author
 *	Paul J. Tanenbaum
 *
 * @par Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char libbu_rb_insert_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "./rb_internals.h"


/**			_ R B _ I N S E R T ( )
 *
 *	    Insert a node into one linear order of a red-black tree
 *
 *	This function has three parameters: the tree and linear order into
 *	which to insert the new node and the new node itself.  If the node
 *	is equal (modulo the linear order) to a node already in the tree,
 *	_rb_insert() returns 1.  Otherwise, it returns 0.
 */
static int _rb_insert (bu_rb_tree *tree, int order, struct bu_rb_node *new_node)
{
    struct bu_rb_node	*node;
    struct bu_rb_node	*parent;
    struct bu_rb_node	*grand_parent;
    struct bu_rb_node	*y;
    int			(*compare)();
    int			comparison=0xdeadbeef;
    int			direction;
    int			result = 0;


    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);
    BU_CKMAG(new_node, BU_RB_NODE_MAGIC, "red-black node");

    /*
     *	Initialize the new node
     */
    bu_rb_parent(new_node, order) =
    bu_rb_left_child(new_node, order) =
    bu_rb_right_child(new_node, order) = bu_rb_null(tree);
    bu_rb_size(new_node, order) = 1;
    if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	bu_log("_rb_insert(%x): size(%x, %d)=%d\n",
	    new_node, new_node, order, bu_rb_size(new_node, order));

    /*
     *	Perform vanilla-flavored binary-tree insertion
     */
    parent = bu_rb_null(tree);
    node = bu_rb_root(tree, order);
    compare = bu_rb_order_func(tree, order);
    while (node != bu_rb_null(tree))
    {
	parent = node;
	++bu_rb_size(parent, order);
	if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	    bu_log("_rb_insert(%x): size(%x, %d)=%d\n",
		new_node, parent, order, bu_rb_size(parent, order));
	comparison = (*compare)(bu_rb_data(new_node, order),
				bu_rb_data(node, order));
	if (comparison < 0)
	{
	    if (tree -> rbt_debug & BU_RB_DEBUG_INSERT)
		bu_log("_rb_insert(%x): <_%d <%x>, going left\n",
		    new_node, order, node);
	    node = bu_rb_left_child(node, order);
	}
	else
	{
	    if (tree -> rbt_debug & BU_RB_DEBUG_INSERT)
		bu_log("_rb_insert(%x): >=_%d <%x>, going right\n",
		    new_node, order, node);
	    node = bu_rb_right_child(node, order);
	    if (comparison == 0)
		result = 1;
	}
    }
    bu_rb_parent(new_node, order) = parent;
    if (parent == bu_rb_null(tree))
	bu_rb_root(tree, order) = new_node;
    else if ((*compare)(bu_rb_data(new_node, order),
			bu_rb_data(parent, order)) < 0)
	bu_rb_left_child(parent, order) = new_node;
    else
	bu_rb_right_child(parent, order) = new_node;

    /*
     *	Reestablish the red-black properties, as necessary
     */
    bu_rb_set_color(new_node, order, BU_RB_RED);
    node = new_node;
    parent = bu_rb_parent(node, order);
    grand_parent = bu_rb_parent(parent, order);
    while ((node != bu_rb_root(tree, order))
	&& (bu_rb_get_color(parent, order) == BU_RB_RED))
    {
	if (parent == bu_rb_left_child(grand_parent, order))
	    direction = BU_RB_LEFT;
	else
	    direction = BU_RB_RIGHT;

	y = bu_rb_other_child(grand_parent, order, direction);
	if (bu_rb_get_color(y, order) == BU_RB_RED)
	{
	    bu_rb_set_color(parent, order, BU_RB_BLACK);
	    bu_rb_set_color(y, order, BU_RB_BLACK);
	    bu_rb_set_color(grand_parent, order, BU_RB_RED);
	    node = grand_parent;
	    parent = bu_rb_parent(node, order);
	    grand_parent = bu_rb_parent(parent, order);
	}
	else
	{
	    if (node == bu_rb_other_child(parent, order, direction))
	    {
		node = parent;
		bu_rb_rotate(node, order, direction);
		parent = bu_rb_parent(node, order);
		grand_parent = bu_rb_parent(parent, order);
	    }
	    bu_rb_set_color(parent, order, BU_RB_BLACK);
	    bu_rb_set_color(grand_parent, order, BU_RB_RED);
	    bu_rb_other_rotate(grand_parent, order, direction);
	}
    }
    bu_rb_set_color(bu_rb_root(tree, order), order, BU_RB_BLACK);

    if (tree -> rbt_debug & BU_RB_DEBUG_INSERT)
	bu_log("_rb_insert(%x): comparison = %d, returning %d\n",
	    new_node, comparison, result);

    return (result);
}

/**			B U _ R B _ I N S E R T ( )
 *
 *		Applications interface to _rb_insert()
 *
 *	This function has two parameters: the tree into which to insert
 *	the new node and the contents of the node.  If a uniqueness
 *	requirement would be violated, bu_rb_insert() does nothing but return
 *	a number from the set {-1, -2, ..., -nm_orders} of which the
 *	absolute value is the first order for which a violation exists.
 *	Otherwise, it returns the number of orders for which the new node
 *	was equal to a node already in the tree.
 */
int bu_rb_insert (bu_rb_tree *tree, void *data)
{
    int				nm_orders;
    int				order;
    int				result = 0;
    struct bu_rb_node		*node;
    struct bu_rb_package	*package;
    struct bu_rb_list		*rblp;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree -> rbt_nm_orders;

    /*
     *	Enforce uniqueness
     *
     *	NOTE: The approach is that for each order that requires uniqueness,
     *	    we look for a match.  This is not the most efficient way to do
     *	    things, since _rb_insert() is just going to turn around and
     *	    search the tree all over again.
     */
    for (order = 0; order < nm_orders; ++order)
	if (bu_rb_get_uniqueness(tree, order) &&
	    (bu_rb_search(tree, order, data) != NULL))
	{
	    if (tree -> rbt_debug & BU_RB_DEBUG_UNIQ)
		bu_log("bu_rb_insert(<%x>, <%x>, TBD) will return %d\n",
		    tree, data, -(order + 1));
	    return (-(order + 1));
	}

    /*
     *	Make a new package
     *	and add it to the list of all packages.
     */
    package = (struct bu_rb_package *)
		bu_malloc(sizeof(struct bu_rb_package), "red-black package");
    package -> rbp_node = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black package nodes");
    rblp = (struct bu_rb_list *)
		bu_malloc(sizeof(struct bu_rb_list),
			    "red-black list element");
    rblp -> rbl_magic = BU_RB_LIST_MAGIC;
    rblp -> rbl_package = package;
    BU_LIST_PUSH(&(tree -> rbt_packages.l), rblp);
    package -> rbp_list_pos = rblp;

    /*
     *	Make a new node
     *	and add it to the list of all nodes.
     */
    node = (struct bu_rb_node *)
		bu_malloc(sizeof(struct bu_rb_node), "red-black node");
    node -> rbn_parent = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black parents");
    node -> rbn_left = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black left children");
    node -> rbn_right = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black right children");
    node -> rbn_color = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black colors");
    node -> rbn_size = (int *)
		bu_malloc(nm_orders * sizeof(int),
			    "red-black subtree sizes");
    node -> rbn_package = (struct bu_rb_package **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
			    "red-black packages");
    rblp = (struct bu_rb_list *)
		bu_malloc(sizeof(struct bu_rb_list),
			    "red-black list element");
    rblp -> rbl_magic = BU_RB_LIST_MAGIC;
    rblp -> rbl_node = node;
    BU_LIST_PUSH(&(tree -> rbt_nodes.l), rblp);
    node -> rbn_list_pos = rblp;

    /*
     *	Fill in the package
     */
    package -> rbp_magic = BU_RB_PKG_MAGIC;
    package -> rbp_data = data;
    for (order = 0; order < nm_orders; ++order)
	(package -> rbp_node)[order] = node;

    /*
     *	Fill in the node
     */
    node -> rbn_magic = BU_RB_NODE_MAGIC;
    node -> rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
	(node -> rbn_package)[order] = package;
    node -> rbn_pkg_refs = nm_orders;

    /*
     *	If the tree was empty, install this node as the root
     *	and give it a null parent and null children
     */
    if (bu_rb_root(tree, 0) == bu_rb_null(tree))
	for (order = 0; order < nm_orders; ++order)
	{
	    bu_rb_root(tree, order) = node;
	    bu_rb_parent(node, order) =
	    bu_rb_left_child(node, order) =
	    bu_rb_right_child(node, order) = bu_rb_null(tree);
	    bu_rb_set_color(node, order, BU_RB_BLACK);
	    bu_rb_size(node, order) = 1;
	    if (tree -> rbt_debug & BU_RB_DEBUG_OS)
		bu_log("bu_rb_insert(<%x>, <%x>, <%x>): size(%x, %d)=%d\n",
		    tree, data, node, node, order, bu_rb_size(node, order));
	}
    /*	Otherwise, insert the node into the tree */
    else
    {
	for (order = 0; order < nm_orders; ++order)
	    result += _rb_insert(tree, order, node);
	if (tree -> rbt_debug & BU_RB_DEBUG_UNIQ)
	    bu_log("bu_rb_insert(<%x>, <%x>, <%x>) will return %d\n",
		tree, data, node, result);
    }

    ++(tree -> rbt_nm_nodes);
    bu_rb_current(tree) = node;
    return (result);
}

/**		        _ R B _ S E T _ U N I Q ( )
 *
 *	    Raise or lower the uniqueness flag for one linear order
 *			    of a red-black tree
 *
 *	This function has three parameters: the tree, the order for which
 *	to modify the flag, and the new value for the flag.  _rb_set_uniq()
 *	sets the specified flag to the specified value and returns the
 *	previous value of the flag.
 */
static int _rb_set_uniq (bu_rb_tree *tree, int order, int new_value)
{
    int	prev_value;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);
    new_value = (new_value != 0);

    prev_value = bu_rb_get_uniqueness(tree, order);
    bu_rb_set_uniqueness(tree, order, new_value);
    return (prev_value);
}

/*		         B U _ R B _ U N I Q _ O N ( )
 *		        B U _ R B _ U N I Q _ O F F ( )
 *
 *		Applications interface to _rb_set_uniq()
 *
 *	These functions have two parameters: the tree and the order for
 *	which to require uniqueness/permit nonuniqueness.  Each sets the
 *	specified flag to the specified value and returns the previous
 *	value of the flag.
 */
int bu_rb_uniq_on (bu_rb_tree *tree, int order)
{
    return (_rb_set_uniq(tree, order, 1));
}

int bu_rb_uniq_off (bu_rb_tree *tree, int order)
{
    return (_rb_set_uniq(tree, order, 0));
}

/**		         B U _ R B _ I S _ U N I Q ( )
 *
 *	  Query the uniqueness flag for one order of a red-black tree
 *
 *	This function has two parameters: the tree and the order for
 *	which to query uniqueness.
 */
int bu_rb_is_uniq (bu_rb_tree *tree, int order)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    return(bu_rb_get_uniqueness(tree, order));
}

/**		        B U _ R B _ S E T _ U N I Q V ( )
 *
 *	    Set the uniqueness flags for all the linear orders
 *			    of a red-black tree
 *
 *	This function has two parameters: the tree and a bitv_t
 *	encoding the flag values.  bu_rb_set_uniqv() sets the flags
 *	according to the bits in flag_rep.  For example, if
 *	flag_rep = 1011_2, then the first, second, and fourth
 *	orders are specified unique, and the third is specified
 *	not-necessarily unique.
 */
void bu_rb_set_uniqv (bu_rb_tree *tree, bitv_t flag_rep)
{
    int	nm_orders;
    int	order;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    nm_orders = tree -> rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	bu_rb_set_uniqueness(tree, order, 0);

    for (order = 0; (flag_rep != 0) && (order < nm_orders); flag_rep >>= 1,
							    ++order)
	if (flag_rep & 0x1)
	    bu_rb_set_uniqueness(tree, order, 1);

    if (flag_rep != 0)
	bu_log("bu_rb_set_uniqv(): Ignoring bits beyond rightmost %d\n",
	    nm_orders);
}

/**		    _ R B _ S E T _ U N I Q _ A L L ( )
 *
 *	    Raise or lower the uniqueness flags for all the linear orders
 *			    of a red-black tree
 *
 *	This function has two parameters: the tree, and the new value
 *	for all the flags.
 */
static void _rb_set_uniq_all (bu_rb_tree *tree, int new_value)
{
    int	nm_orders;
    int	order;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    new_value = (new_value != 0);

    nm_orders = tree -> rbt_nm_orders;
    for (order = 0; order < nm_orders; ++order)
	bu_rb_set_uniqueness(tree, order, new_value);
}

/**		     B U _ R B _ U N I Q _ A L L _ O N ( )
 *		    B U _ R B _ U N I Q _ A L L _ O F F ( )
 *
 *	      Applications interface to _rb_set_uniq_all()
 *
 *	These functions have one parameter: the tree for which to
 *	require uniqueness/permit nonuniqueness.
 */
void bu_rb_uniq_all_on (bu_rb_tree *tree)
{
    _rb_set_uniq_all(tree, 1);
}

void bu_rb_uniq_all_off (bu_rb_tree *tree)
{
    _rb_set_uniq_all(tree, 0);
}
/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
