/*			R B _ E X T R E M E . C
 *
 *	Routines to extract mins, maxes, adjacent, and current nodes
 *			from a red-black tree
 *
 *	Author:	Paul Tanenbaum
 *
 */
#ifndef lint
static char RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "redblack.h"
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
static struct rb_node *_rb_extreme (root, order, sense, empty_node)

struct rb_node	*root;
int		order;
int		sense;
struct rb_node	*empty_node;

{
    struct rb_node	*child;
    rb_tree		*tree;

    if (root == empty_node)
	return (root);

    while (1)
    {
	RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
	tree = root -> rbn_tree;
	RB_CKORDER(tree, order);

	child = (sense == SENSE_MIN) ? rb_left_child(root, order) :
				       rb_right_child(root, order);
	if (child == empty_node)
	    break;
	root = child;
    }

    /* Record the node with which we've been working */
    rb_current(tree) = root;

    return (root);
}

/*		        R B _ E X T R E M E ( )
 *
 *		Applications interface to _rb_extreme()
 *
 *	This function has three parameters: the tree in which to find an
 *	extreme node, the order on which to do the search, and the sense
 *	(min or max).  On success, rb_extreme() returns a pointer to the
 *	data in the extreme node.  Otherwise it returns NULL.
 */
void *rb_extreme (tree, order, sense)

rb_tree	*tree;
int	order;
int	sense;

{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	rt_log("FATAL: rb_extreme(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    /* Wade throught the tree */
    node = _rb_extreme(rb_root(tree, order), order, sense, rb_null(tree));

    if (node == rb_null(tree))
	return (NULL);
    else
	return (rb_data(node, order));
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
struct rb_node *_rb_neighbor (node, order, sense)

struct rb_node	*node;
int		order;
int		sense;

{
    struct rb_node	*child;
    struct rb_node	*parent;
    rb_tree		*tree;
    struct rb_node	*empty_node;

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    tree = node -> rbn_tree;
    RB_CKORDER(tree, order);

    empty_node = rb_null(tree);

    child = (sense == SENSE_MIN) ? rb_left_child(node, order) :
				   rb_right_child(node, order);
    if (child != empty_node)
	return (_rb_extreme(child, order, 1 - sense, empty_node));
    parent = rb_parent(node, order);
    while ((parent != empty_node) &&
	   (node == rb_child(parent, order, sense)))
    {
	node = parent;
	parent = rb_parent(parent, order);
    }

    /* Record the node with which we've been working */
    rb_current(tree) = parent;

    return (parent);
}

/*		        R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to the current red-black node
 *
 *	This function has three parameters: the tree and order on which
 *	to do the search and the sense (min or max, which is to say
 *	predecessor or successor) of the search.  rb_neighbor() returns
 *	a pointer to the data in the node adjacent to the current node
 *	in the specified direction, if that node exists.  Otherwise,
 *	it returns NULL.
 */
void *rb_neighbor (tree, order, sense)

rb_tree	*tree;
int	order;
int	sense;

{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	rt_log("FATAL: rb_neighbor(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    /* Wade through the tree */
    node = _rb_neighbor(rb_current(tree), order, sense);

    if (node == rb_null(tree))
	return (NULL);
    else
    {
	/* Record the node with which we've been working */
	rb_current(tree) = node;
	return (rb_data(node, order));
    }
}

/*		            R B _ C U R R ( )
 *
 *	    Return the current red-black node
 *
 *	This function has two parameters: the tree and order in which
 *	to find the current node.  rb_curr() returns a pointer to
 *	the data in the current node, if it exists.  Otherwise,
 *	it returns NULL.
 */
void *rb_curr (tree, order)

rb_tree	*tree;
int	order;

{
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (rb_current(tree) == rb_null(tree))
	return (NULL);
    else
	return (rb_data(rb_current(tree), order));
}
