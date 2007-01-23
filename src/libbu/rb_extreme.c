/*                    R B _ E X T R E M E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @addtogroup rb */
/** @{ */
/** @file rb_extreme.c
 *
 *	Routines to extract mins, maxes, adjacent, and current nodes
 *			from a red-black tree
 *
 *  @author
 *	Paul J. Tanenbaum
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char libbu_rb_extreme_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"


/**		        _ R B _ E X T R E M E ( )
 *
 *	Find the minimum or maximum node in one order of a red-black tree
 *
 *	This function has four parameters: the root of the tree, the
 *	order, the sense (min or max), and the address to be understood
 *	as the nil node pointer. _rb_extreme() returns a pointer to the
 *	extreme node.
 */
static struct bu_rb_node *_rb_extreme (struct bu_rb_node *root, int order, int sense, struct bu_rb_node *empty_node)
{
    struct bu_rb_node	*child;
    bu_rb_tree		*tree;

    if (root == empty_node)
	return (root);

    while (1)
    {
	BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
	tree = root -> rbn_tree;
	BU_RB_CKORDER(tree, order);

	child = (sense == SENSE_MIN) ? bu_rb_left_child(root, order) :
				       bu_rb_right_child(root, order);
	if (child == empty_node)
	    break;
	root = child;
    }

    /* Record the node with which we've been working */
    bu_rb_current(tree) = root;

    return (root);
}

/**		        B U _ R B _ E X T R E M E ( )
 *
 *		Applications interface to _rb_extreme()
 *
 *	This function has three parameters: the tree in which to find an
 *	extreme node, the order on which to do the search, and the sense
 *	(min or max).  On success, bu_rb_extreme() returns a pointer to the
 *	data in the extreme node.  Otherwise it returns NULL.
 */
void *bu_rb_extreme (bu_rb_tree *tree, int order, int sense)
{
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	bu_log("FATAL: bu_rb_extreme(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	bu_bomb("");
    }

    /* Wade throught the tree */
    node = _rb_extreme(bu_rb_root(tree, order), order, sense,
			bu_rb_null(tree));

    if (node == bu_rb_null(tree))
	return (NULL);
    else
	return (bu_rb_data(node, order));
}

/**		    _ R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to a given red-black node
 *
 *	This function has three parameters: the node of interest, the
 *	order on which to do the search, and the sense (min or max,
 *	which is to say predecessor or successor).  _rb_neighbor()
 *	returns a pointer to the adjacent node.  This function is
 *	modeled after the routine TREE-SUCCESSOR on p. 249 of Cormen et al.
 */
struct bu_rb_node *_rb_neighbor (struct bu_rb_node *node, int order, int sense)
{
    struct bu_rb_node	*child;
    struct bu_rb_node	*parent;
    bu_rb_tree		*tree;
    struct bu_rb_node	*empty_node;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    tree = node -> rbn_tree;
    BU_RB_CKORDER(tree, order);

    empty_node = bu_rb_null(tree);

    child = (sense == SENSE_MIN) ? bu_rb_left_child(node, order) :
				   bu_rb_right_child(node, order);
    if (child != empty_node)
	return (_rb_extreme(child, order, 1 - sense, empty_node));
    parent = bu_rb_parent(node, order);
    while ((parent != empty_node) &&
	   (node == bu_rb_child(parent, order, sense)))
    {
	node = parent;
	parent = bu_rb_parent(parent, order);
    }

    /* Record the node with which we've been working */
    bu_rb_current(tree) = parent;

    return (parent);
}

/**		        B U _ R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to the current red-black node
 *
 *	This function has three parameters: the tree and order on which
 *	to do the search and the sense (min or max, which is to say
 *	predecessor or successor) of the search.  bu_rb_neighbor() returns
 *	a pointer to the data in the node adjacent to the current node
 *	in the specified direction, if that node exists.  Otherwise,
 *	it returns NULL.
 */
void *bu_rb_neighbor (bu_rb_tree *tree, int order, int sense)
{
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	bu_log("FATAL: bu_rb_neighbor(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	bu_bomb("");
    }

    /* Wade through the tree */
    node = _rb_neighbor(bu_rb_current(tree), order, sense);

    if (node == bu_rb_null(tree))
	return (NULL);
    else
    {
	/* Record the node with which we've been working */
	bu_rb_current(tree) = node;
	return (bu_rb_data(node, order));
    }
}

/**		            B U _ R B _ C U R R ( )
 *
 *	    Return the current red-black node
 *
 *	This function has two parameters: the tree and order in which
 *	to find the current node.  bu_rb_curr() returns a pointer to
 *	the data in the current node, if it exists.  Otherwise,
 *	it returns NULL.
 */
void *bu_rb_curr (bu_rb_tree *tree, int order)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if (bu_rb_current(tree) == bu_rb_null(tree))
	return (NULL);
    else
	return (bu_rb_data(bu_rb_current(tree), order));
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
