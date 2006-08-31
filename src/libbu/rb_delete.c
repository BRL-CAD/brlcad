/*                     R B _ D E L E T E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
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
/** \addtogroup rb  */
/*@{*/
/** @file rb_delete.c
 *	    Routines to delete a node from a red-black tree
 *
 *
 *  @author	Paul J. Tanenbaum
 *
 * @par  Source -
 *	The U. S. Army Research Laboratory
 *@n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static char const libbu_rb_delete_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "./rb_internals.h"


/**			B U _ R B _ F I X U P ( )
 *
 *	    Restore the red-black properties of a red-black tree
 *		    after the splicing out of a node
 *
 *	This function has three parameters: the tree to be fixed up,
 *	the node where the trouble occurs, and the order.  bu_rb_fixup()
 *	is an implementation of the routine RB-DELETE-FIXUP on p. 274
 *	of Cormen et al.
 */
static void bu_rb_fixup (bu_rb_tree *tree, struct bu_rb_node *node, int order)
{
    int			direction;
    struct bu_rb_node	*parent;
    struct bu_rb_node	*w;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(tree, order);

    while ((node != bu_rb_root(tree, order))
        && (bu_rb_get_color(node, order) == BU_RB_BLACK))
    {
	parent = bu_rb_parent(node, order);
	if (node == bu_rb_left_child(parent, order))
	    direction = BU_RB_LEFT;
	else
	    direction = BU_RB_RIGHT;

	w = bu_rb_other_child(parent, order, direction);
	if (bu_rb_get_color(w, order) == BU_RB_RED)
	{
	    bu_rb_set_color(w, order, BU_RB_BLACK);
	    bu_rb_set_color(parent, order, BU_RB_RED);
	    bu_rb_rotate(parent, order, direction);
	    w = bu_rb_other_child(parent, order, direction);
	}
	if ((bu_rb_get_color(bu_rb_child(w, order, direction), order)
		== BU_RB_BLACK)
	 && (bu_rb_get_color(bu_rb_other_child(w, order, direction), order)
		 == BU_RB_BLACK))
	{
	    bu_rb_set_color(w, order, BU_RB_RED);
	    node = parent;
	}
	else
	{
	    if (bu_rb_get_color(bu_rb_other_child(w, order, direction),
		order)
		    == BU_RB_BLACK)
	    {
		bu_rb_set_color(bu_rb_child(w, order, direction), order,
		    BU_RB_BLACK);
		bu_rb_set_color(w, order, BU_RB_RED);
		bu_rb_other_rotate(w, order, direction);
		w = bu_rb_other_child(parent, order, direction);
	    }
	    bu_rb_set_color(w, order, bu_rb_get_color(parent, order));
	    bu_rb_set_color(parent, order, BU_RB_BLACK);
	    bu_rb_set_color(bu_rb_other_child(w, order, direction),
				order, BU_RB_BLACK);
	    bu_rb_rotate(parent, order, direction);
	    node = bu_rb_root(tree, order);
	}
    }
    bu_rb_set_color(node, order, BU_RB_BLACK);
}

/**		        _ R B _ D E L E T E ( )
 *
 *	        Delete a node from one order of a red-black tree
 *
 *	This function has three parameters: a tree, the node to delete,
 *	and the order from which to delete it.  _rb_delete() is an
 *	implementation of the routine RB-DELETE on p. 273 of Cormen et al.
 */
static void _rb_delete (bu_rb_tree *tree, struct bu_rb_node *node, int order)
{
    struct bu_rb_node	*y;		/* The node to splice out */
    struct bu_rb_node	*parent;
    struct bu_rb_node	*only_child;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(tree, order);

    if (tree -> rbt_debug & BU_RB_DEBUG_DELETE)
	bu_log("_rb_delete(%x,%x,%d): data=x%x\n",
	    tree, node, order, bu_rb_data(node, order));

    if ((bu_rb_left_child(node, order) == bu_rb_null(tree))
     || (bu_rb_right_child(node, order) == bu_rb_null(tree)))
	y = node;
    else
	y = _rb_neighbor(node, order, SENSE_MAX);

    if (bu_rb_left_child(y, order) == bu_rb_null(tree))
	only_child = bu_rb_right_child(y, order);
    else
	only_child = bu_rb_left_child(y, order);

    parent = bu_rb_parent(only_child, order) = bu_rb_parent(y, order);
    if (parent == bu_rb_null(tree))
	bu_rb_root(tree, order) = only_child;
    else if (y == bu_rb_left_child(parent, order))
	bu_rb_left_child(parent, order) = only_child;
    else
	bu_rb_right_child(parent, order) = only_child;

    /*
     *	Splice y out if it's not node
     */
    if (y != node)
    {
	(node -> rbn_package)[order] = (y -> rbn_package)[order];
	((node -> rbn_package)[order] -> rbp_node)[order] = node;
    }

    if (bu_rb_get_color(y, order) == BU_RB_BLACK)
	bu_rb_fixup(tree, only_child, order);

    if (--(y -> rbn_pkg_refs) == 0)
	bu_rb_free_node(y);
}

/**		        B U _ R B _ D E L E T E ( )
 *
 *	        Applications interface to _rb_delete()
 *
 *	This function has two parameters: the tree and order from which
 *	to do the deleting.  bu_rb_delete() removes the data block stored
 *	in the current node (in the position of the specified order)
 *	from every order in the tree.
 */
void bu_rb_delete (bu_rb_tree *tree, int order)
{
    int				nm_orders;
    struct bu_rb_node		**node;		/* Nodes containing data */
    struct bu_rb_package	*package;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if (tree -> rbt_nm_nodes <= 0)
    {
	bu_log("Error: Attempt to delete from tree with %d nodes\n",
		tree -> rbt_nm_nodes);
	bu_bomb("");
    }
    if (bu_rb_current(tree) == bu_rb_null(tree))
    {
	bu_log("Warning: bu_rb_delete(): current node is undefined\n");
	return;
    }

    nm_orders = tree -> rbt_nm_orders;
    package = (bu_rb_current(tree) -> rbn_package)[order];

    node = (struct bu_rb_node **)
	    bu_malloc(nm_orders * sizeof(struct bu_rb_node *), "node list");

    for (order = 0; order < nm_orders; ++order)
	node[order] = (package -> rbp_node)[order];

    /*
     *	Do the deletion from each order
     */
    for (order = 0; order < nm_orders; ++order)
	_rb_delete(tree, node[order], order);

    --(tree -> rbt_nm_nodes);
    bu_rb_free_package(package);
    bu_free((genptr_t) node, "node list");
}
/*@}*/
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
