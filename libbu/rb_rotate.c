/*			R B _ R O T A T E . C
 *
 *	    Routines to perform rotations on a red-black tree
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
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char libbu_rb_rotate_RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"

/*		    _ R B _ R O T _ L E F T ( )
 *
 *		Perfrom left rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  _rb_rot_left() is an implementation
 *	of the routine called LEFT-ROTATE on p. 266 of Cormen et al,
 *	with modification on p. 285.
 */
void _rb_rot_left (struct bu_rb_node *x, int order)
{
    struct bu_rb_node	*y;		/* x's child to pivot up */
    struct bu_rb_node	*beta;		/* y's child in direction of rot. */
    struct bu_rb_node	*x_parent;	/* x's parent */
    bu_rb_tree		*tree = x -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set y and check data types of both x and y
     */
    BU_CKMAG(x, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(x -> rbn_tree, order);

    y = bu_rb_right_child(x, order);

    if (tree -> rbt_debug & BU_RB_DEBUG_ROTATE)
	bu_log("_rb_rot_left(<%x>, %d)...\n", x, order);

    bu_rb_right_child(x, order) = beta = bu_rb_left_child(y, order);
    if (beta != bu_rb_null(tree))
	bu_rb_parent(beta, order) = x;
    bu_rb_parent(y, order) = x_parent = bu_rb_parent(x, order);
    if (x_parent == bu_rb_null(tree))
	bu_rb_root(tree, order) = y;
    else if (x == bu_rb_left_child(x_parent, order))
	bu_rb_left_child(x_parent, order) = y;
    else
	bu_rb_right_child(x_parent, order) = y;
    bu_rb_left_child(y, order) = x;
    bu_rb_parent(x, order) = y;

    bu_rb_size(y, order) = bu_rb_size(x, order);
    bu_rb_size(x, order) =
	bu_rb_size(bu_rb_left_child(x, order), order) +
	bu_rb_size(bu_rb_right_child(x, order), order) + 1;
    if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	bu_log("After rotation, size(%x, %d)=%d, size(%x, %d)=%d\n",
	    x, order, bu_rb_size(x, order), y, order, bu_rb_size(y, order));
}

/*		    _ R B _ R O T _ R I G H T ( )
 *
 *		Perfrom right rotation on a red-black tree
 *
 *	This function has two parameters: the node about which to rotate
 *	and the order to be rotated.  _rb_rot_right() is hacked from
 *	_rb_rot_left() above.
 */
void _rb_rot_right (struct bu_rb_node *y, int order)
{
    struct bu_rb_node	*x;		/* y's child to pivot up */
    struct bu_rb_node	*beta;		/* x's child in direction of rot. */
    struct bu_rb_node	*y_parent;	/* y's parent */
    bu_rb_tree		*tree = y -> rbn_tree;	/* Tree where it all happens */

    /*
     *	Set x and check data types of both x and y
     */
    BU_CKMAG(y, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(y -> rbn_tree, order);

    x = bu_rb_left_child(y, order);

    if (tree -> rbt_debug & BU_RB_DEBUG_ROTATE)
	bu_log("_rb_rot_right(<%x>, %d)...\n", y, order);

    bu_rb_left_child(y, order) = beta = bu_rb_right_child(x, order);
    if (beta != bu_rb_null(tree))
	bu_rb_parent(beta, order) = y;
    bu_rb_parent(x, order) = y_parent = bu_rb_parent(y, order);
    if (y_parent == bu_rb_null(tree))
	bu_rb_root(tree, order) = x;
    else if (y == bu_rb_left_child(y_parent, order))
	bu_rb_left_child(y_parent, order) = x;
    else
	bu_rb_right_child(y_parent, order) = x;
    bu_rb_right_child(x, order) = y;
    bu_rb_parent(y, order) = x;

    bu_rb_size(x, order) = bu_rb_size(y, order);
    bu_rb_size(y, order) =
	bu_rb_size(bu_rb_left_child(y, order), order) +
	bu_rb_size(bu_rb_right_child(y, order), order) + 1;
    if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	bu_log("After rotation, size(%x, %d)=%d, size(%x, %d)=%d\n",
	    x, order, bu_rb_size(x, order), y, order, bu_rb_size(y, order));
}
