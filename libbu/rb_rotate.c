/*			R B _ R O T A T E . C
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "redblack.h"
#include "rb_internals.h"

/*		    L E F T _ R O T A T E ( )
 *
 *		Perfrom left rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  The bulk of this code is from
 *	T. H. Cormen, C. E. Leiserson, and R. L. Rivest.  _Introduction
 *	to Algorithms_.  Cambridge, MA: MIT Press, 1990. p. 266.
 */
void left_rotate (struct rb_node *x, int order)
{
    struct rb_node	*y;		/* x's child to pivot up */
    struct rb_node	*beta;		/* y's child in direction of rot. */
    struct rb_node	*x_parent;	/* x's parent */
    rb_tree		*tree = x -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set y and check data types of both x and y
     */
    RB_CKMAG(x, RB_NODE_MAGIC, "red-black node");
    y = rb_right_child(x, order);
    RB_CKMAG(y, RB_NODE_MAGIC, "red-black node");

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
 *	and the order to be rotated.  This function is hacked from
 *	left_rotate() above.
 */
void right_rotate (struct rb_node *y, int order)
{
    struct rb_node	*x;		/* y's child to pivot up */
    struct rb_node	*beta;		/* x's child in direction of rot. */
    struct rb_node	*y_parent;	/* y's parent */
    rb_tree		*tree = y -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set x and check data types of both x and y
     */
    RB_CKMAG(y, RB_NODE_MAGIC, "red-black node");
    x = rb_left_child(y, order);
    RB_CKMAG(x, RB_NODE_MAGIC, "red-black node");

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
