/*			R B _ E X T R E M E . C
 *
 *	Routines to extract mins, maxes, adjacent, and current nodes
 *			from a red-black tree
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char libbu_rb_extreme_RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"

/*		        _ R B _ E X T R E M E ( )
 *
 *	Find the minimum or maximum node in one order of a red-black tree
 *
 *	This function has four parameters: the root of the tree, the
 *	order, the sense (min or max), and the address to be understood
 *	as the nil node pointer. _rb_extreme() returns a pointer to the
 *	extreme node.
 */
static struct bu_rb_node *_rb_extreme (root, order, sense, empty_node)

struct bu_rb_node	*root;
int			order;
int			sense;
struct bu_rb_node	*empty_node;

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

/*		        B U _ R B _ E X T R E M E ( )
 *
 *		Applications interface to _rb_extreme()
 *
 *	This function has three parameters: the tree in which to find an
 *	extreme node, the order on which to do the search, and the sense
 *	(min or max).  On success, bu_rb_extreme() returns a pointer to the
 *	data in the extreme node.  Otherwise it returns NULL.
 */
void *bu_rb_extreme (tree, order, sense)

bu_rb_tree	*tree;
int		order;
int		sense;

{
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	bu_log("FATAL: bu_rb_extreme(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    /* Wade throught the tree */
    node = _rb_extreme(bu_rb_root(tree, order), order, sense,
			bu_rb_null(tree));

    if (node == bu_rb_null(tree))
	return (NULL);
    else
	return (bu_rb_data(node, order));
}

/*		    _ R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to a given red-black node
 *
 *	This function has three parameters: the node of interest, the
 *	order on which to do the search, and the sense (min or max,
 *	which is to say predecessor or successor).  _rb_neighbor()
 *	returns a pointer to the adjacent node.  This function is
 *	modeled after the routine TREE-SUCCESSOR on p. 249 of Cormen et al.
 */
struct bu_rb_node *_rb_neighbor (node, order, sense)

struct bu_rb_node	*node;
int			order;
int			sense;

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

/*		        B U _ R B _ N E I G H B O R ( )
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
void *bu_rb_neighbor (tree, order, sense)

bu_rb_tree	*tree;
int		order;
int		sense;

{
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	bu_log("FATAL: bu_rb_neighbor(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
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

/*		            B U _ R B _ C U R R ( )
 *
 *	    Return the current red-black node
 *
 *	This function has two parameters: the tree and order in which
 *	to find the current node.  bu_rb_curr() returns a pointer to
 *	the data in the current node, if it exists.  Otherwise,
 *	it returns NULL.
 */
void *bu_rb_curr (tree, order)

bu_rb_tree	*tree;
int		order;

{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if (bu_rb_current(tree) == bu_rb_null(tree))
	return (NULL);
    else
	return (bu_rb_data(bu_rb_current(tree), order));
}
