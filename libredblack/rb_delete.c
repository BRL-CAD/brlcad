/*			R B _ D E L E T E . C
 *
 *	    Routines to delete a node from a red-black tree
 *
 */
#ifndef lint
static char RCSid[] = "@(#) $Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "redblack.h"
#include "rb_internals.h"

/*		    L E F T _ R O T A T E ( )
 *
 *		Perfrom left rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  Left_rotate() is an implementation
 *	of the routine called LEFT-ROTATE on p. 266 of Cormen et al.
 */
static void left_rotate (struct rb_node *x, int order)
{
    struct rb_node	*y;		/* x's child to pivot up */
    struct rb_node	*beta;		/* y's child in direction of rot. */
    struct rb_node	*x_parent;	/* x's parent */
    rb_tree		*tree = x -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set y and check data types of both x and y
     */
    RB_CKMAG(x, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(x -> rbn_tree, order);

    y = rb_right_child(x, order);

    rb_right_child(x, order) = beta = rb_left_child(y, order);
    if (beta != rb_null(tree))
	rb_parent(beta, order) = x;
    rb_parent(y, order) = x_parent = rb_parent(x, order);
    if (x_parent == rb_null(tree))
	rb_root(tree, order) = y;
    else if (x == rb_left_child(x_parent, order))
	rb_left_child(x_parent, order) = y;
    else
	rb_right_child(x_parent, order) = y;
    rb_left_child(y, order) = x;
    rb_parent(x, order) = y;
}

/*		    R I G H T _ R O T A T E ( )
 *
 *		Perfrom right rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  Right_rotate() is hacked from
 *	left_rotate() above.
 */
static void right_rotate (struct rb_node *y, int order)
{
    struct rb_node	*x;		/* y's child to pivot up */
    struct rb_node	*beta;		/* x's child in direction of rot. */
    struct rb_node	*y_parent;	/* y's parent */
    rb_tree		*tree = y -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set x and check data types of both x and y
     */
    RB_CKMAG(y, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(y -> rbn_tree, order);

    x = rb_left_child(y, order);

    rb_left_child(y, order) = beta = rb_right_child(x, order);
    if (beta != rb_null(tree))
	rb_parent(beta, order) = y;
    rb_parent(x, order) = y_parent = rb_parent(y, order);
    if (y_parent == rb_null(tree))
	rb_root(tree, order) = x;
    else if (y == rb_left_child(y_parent, order))
	rb_left_child(y_parent, order) = x;
    else
	rb_right_child(y_parent, order) = x;
    rb_right_child(x, order) = y;
    rb_parent(y, order) = x;
}

/*		    R B _ R O T A T E ( )
 *			    and
 *		R B _ O T H E R _ R O T A T E ( )
 *
 *	These macros have three parameters: the node about which
 *	to rotate, the order to be rotated, and the direction of
 *	rotation.  They allow indirection in the use of left_rotate()
 *	and right_rotate().  Intended for use only by rb_fixup()
 *	below.
 */
#define	rb_rotate(n, o, d)	(((d) == RB_LEFT)		? 	\
				    left_rotate((n), (o))	:	\
				    right_rotate((n), (o)))
#define	rb_other_rotate(n, o, d) (((d) == RB_LEFT)		? 	\
				    right_rotate((n), (o))	:	\
				    left_rotate((n), (o)))

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
static void rb_fixup (rb_tree *tree, struct rb_node *node, int order)
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

/*		    R B _ F R E E _ N O D E ( )
 *
 *	    Relinquish memory occupied by a red-black node
 *
 *	This function has one parameter: a node to free.  rb_free_node()
 *	frees the memory allocated for the various members of the node
 *	and then frees the memory allocated for the node itself.
 */
static void rb_free_node (struct rb_node *node)
{
    rb_tree	*tree;

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");

    tree = node -> rbn_tree;
    if (rb_current(tree) == node)
	rb_current(tree) = rb_null(tree);

    rt_free((char *) node -> rbn_parent, "red-black parents");
    rt_free((char *) node -> rbn_left, "red-black left children");
    rt_free((char *) node -> rbn_right, "red-black right children");
    rt_free((char *) node -> rbn_color, "red-black colors");
    rt_free((char *) node -> rbn_package, "red-black packages");
    rt_free((char *) node, "red-black node");
}

/*		    R B _ F R E E _ P A C K A G E ( )
 *
 *	    Relinquish memory occupied by a red-black package
 *
 *	This function has one parameter: a package to free.
 *	rb_free_package() frees the memory allocated to point to the
 *	nodes that contained the package and then frees the memory
 *	allocated for the package itself.
 */
static void rb_free_package (struct rb_package *package)
{
    RB_CKMAG(package, RB_PKG_MAGIC, "red-black package");

    rt_free((char *) package -> rbp_node, "red-black package nodes");
    rt_free((char *) package, "red-black package");
}

/*		        _ R B _ D E L E T E ( )
 *
 *	        Delete a node from one order of a red-black tree
 *
 *	This function has three parameters: a tree, the node to delete,
 *	and the order from which to delete it.  _rb_delete() is an
 *	implementation of the routine RB-DELETE on p. 273 of Cormen et al.
 */
static void _rb_delete (rb_tree *tree, struct rb_node *node, int order)
{
    struct rb_node	*y;		/* The node to splice out */
    struct rb_node	*parent;
    struct rb_node	*only_child;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(tree, order);

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
    if (--(y -> rbn_pkg_refs) == 0)
	rb_free_node(y);
    
    if (rb_get_color(y, order) == RB_BLACK)
	rb_fixup(tree, only_child, order);
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
void rb_delete (rb_tree *tree, int order)
{
    int			nm_orders;
    struct rb_node	**node;		/* Nodes containing data */
    struct rb_package	*package;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    nm_orders = tree -> rbt_nm_orders;
    package = (rb_current(tree) -> rbn_package)[order];

    node = (struct rb_node **)
	    rt_malloc(nm_orders * sizeof(struct rb_node *), "node list");
	
    for (order = 0; order < nm_orders; ++order)
	node[order] = (package -> rbp_node)[order];

    /*
     *	Do the deletion from each order
     */
    for (order = 0; order < nm_orders; ++order)
	_rb_delete(tree, node[order], order);

    rb_free_package(package);
    rt_free((char *) node, "node list");
}
