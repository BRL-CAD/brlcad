/*			R B _ D E L E T E . C
 *
 *	    Routines to delete a node from a red-black tree
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
#include "bu.h"
#include "redblack.h"
#include "./rb_internals.h"

/*			R B _ F I X U P ( )
 *
 *	    Restore the red-black properties of a red-black tree
 *		    after the splicing out of a node
 *
 *	This function has three parameters: the tree to be fixed up,
 *	the node where the trouble occurs, and the order.  rb_fixup()
 *	is an implementation of the routine RB-DELETE-FIXUP on p. 274
 *	of Cormen et al.
 */
static void rb_fixup (tree, node, order)

rb_tree		*tree;
struct rb_node	*node;
int		order;

{
    int			direction;
    struct rb_node	*parent;
    struct rb_node	*w;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(tree, order);

    while ((node != rb_root(tree, order))
        && (rb_get_color(node, order) == RB_BLACK))
    {
	parent = rb_parent(node, order);
	if (node == rb_left_child(parent, order))
	    direction = RB_LEFT;
	else
	    direction = RB_RIGHT;

	w = rb_other_child(parent, order, direction);
	if (rb_get_color(w, order) == RB_RED)
	{
	    rb_set_color(w, order, RB_BLACK);
	    rb_set_color(parent, order, RB_RED);
	    rb_rotate(parent, order, direction);
	    w = rb_other_child(parent, order, direction);
	}
	if ((rb_get_color(rb_child(w, order, direction), order) == RB_BLACK)
	 && (rb_get_color(rb_other_child(w, order, direction), order)
		 == RB_BLACK))
	{
	    rb_set_color(w, order, RB_RED);
	    node = parent;
	}
	else
	{
	    if (rb_get_color(rb_other_child(w, order, direction), order)
		    == RB_BLACK)
	    {
		rb_set_color(rb_child(w, order, direction), order, RB_BLACK);
		rb_set_color(w, order, RB_RED);
		rb_other_rotate(w, order, direction);
		w = rb_other_child(parent, order, direction);
	    }
	    rb_set_color(w, order, rb_get_color(parent, order));
	    rb_set_color(parent, order, RB_BLACK);
	    rb_set_color(rb_other_child(w, order, direction), order, RB_BLACK);
	    rb_rotate(parent, order, direction);
	    node = rb_root(tree, order);
	}
    }
    rb_set_color(node, order, RB_BLACK);
}

/*		        _ R B _ D E L E T E ( )
 *
 *	        Delete a node from one order of a red-black tree
 *
 *	This function has three parameters: a tree, the node to delete,
 *	and the order from which to delete it.  _rb_delete() is an
 *	implementation of the routine RB-DELETE on p. 273 of Cormen et al.
 */
static void _rb_delete (tree, node, order)

rb_tree		*tree;
struct rb_node	*node;
int		order;

{
    struct rb_node	*y;		/* The node to splice out */
    struct rb_node	*parent;
    struct rb_node	*only_child;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(tree, order);

    if (tree -> rbt_debug & RB_DEBUG_DELETE)
	bu_log("_rb_delete(%x,%x,%d): data=x%x\n",
	    tree, node, order, rb_data(node, order));

    if ((rb_left_child(node, order) == rb_null(tree))
     || (rb_right_child(node, order) == rb_null(tree)))
	y = node;
    else
	y = _rb_neighbor(node, order, SENSE_MAX);
    
    if (rb_left_child(y, order) == rb_null(tree))
	only_child = rb_right_child(y, order);
    else
	only_child = rb_left_child(y, order);
    
    parent = rb_parent(only_child, order) = rb_parent(y, order);
    if (parent == rb_null(tree))
	rb_root(tree, order) = only_child;
    else if (y == rb_left_child(parent, order))
	rb_left_child(parent, order) = only_child;
    else
	rb_right_child(parent, order) = only_child;
    
    /*
     *	Splice y out if it's not node
     */
    if (y != node)
    {
	(node -> rbn_package)[order] = (y -> rbn_package)[order];
	((node -> rbn_package)[order] -> rbp_node)[order] = node;
    }
    
    if (rb_get_color(y, order) == RB_BLACK)
	rb_fixup(tree, only_child, order);

    if (--(y -> rbn_pkg_refs) == 0)
	rb_free_node(y);
}

/*		        R B _ D E L E T E ( )
 *
 *	        Applications interface to _rb_delete()
 *
 *	This function has two parameters: the tree and order from which
 *	to do the deleting.  rb_delete() removes the data block stored
 *	in the current node (in the position of the specified order)
 *	from every order in the tree.
 */
void rb_delete (tree, order)

rb_tree	*tree;
int	order;

{
    int			nm_orders;
    struct rb_node	**node;		/* Nodes containing data */
    struct rb_package	*package;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (tree -> rbt_nm_nodes <= 0)
    {
	bu_log("Error: Attempt to delete from tree with %d nodes\n",
		tree -> rbt_nm_nodes);
	exit (0);
    }
    if (rb_current(tree) == rb_null(tree))
    {
	bu_log("Warning: rb_delete(): current node is undefined\n");
	return;
    }

    nm_orders = tree -> rbt_nm_orders;
    package = (rb_current(tree) -> rbn_package)[order];

    node = (struct rb_node **)
	    bu_malloc(nm_orders * sizeof(struct rb_node *), "node list");
	
    for (order = 0; order < nm_orders; ++order)
	node[order] = (package -> rbp_node)[order];

    /*
     *	Do the deletion from each order
     */
    for (order = 0; order < nm_orders; ++order)
	_rb_delete(tree, node[order], order);

    --(tree -> rbt_nm_nodes);
    rb_free_package(package);
    bu_free((genptr_t) node, "node list");
}
