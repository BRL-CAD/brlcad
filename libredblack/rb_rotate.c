/*			R B _ R O T A T E . C
 *
 *	    Routines to perform rotations on a red-black tree
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
#include "redblack.h"
#include "./rb_internals.h"

/*		    _ R B _ R O T _ L E F T ( )
 *
 *		Perfrom left rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  _rb_rot_left() is an implementation
 *	of the routine called LEFT-ROTATE on p. 266 of Cormen et al.
 */
void _rb_rot_left (x, order)

struct rb_node	*x;
int		order;

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

    if (tree -> rbt_debug & RB_DEBUG_ROTATE)
	rt_log("_rb_rot_left(<%x>, %d)...\n", x, order);

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

/*		    _ R B _ R O T _ R I G H T ( )
 *
 *		Perfrom right rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  _rb_rot_right() is hacked from
 *	_rb_rot_left() above.
 */
void _rb_rot_right (y, order)

struct rb_node	*y;
int		order;

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

    if (tree -> rbt_debug & RB_DEBUG_ROTATE)
	rt_log("_rb_rot_right(<%x>, %d)...\n", y, order);

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
