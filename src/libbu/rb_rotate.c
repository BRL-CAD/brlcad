/*                     R B _ R O T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu.h"

#include "./rb_internals.h"


void
rb_rot_left(struct bu_rb_node *x, int order)
{
    struct bu_rb_node *y;		/* x's child to pivot up */
    struct bu_rb_node *beta;		/* y's child in direction of rot. */
    struct bu_rb_node *x_parent;	/* x's parent */
    struct bu_rb_tree *tree = x->rbn_tree;	/* Tree where it all happens */

    /*
     * Set y and check data types of both x and y
     */
    BU_CKMAG(x, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(x->rbn_tree, order);

    y = rb_right_child(x, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_left(<%p>, %d)...\n", x, order);

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

    rb_size(y, order) = rb_size(x, order);
    rb_size(x, order) =
	rb_size(rb_left_child(x, order), order) +
	rb_size(rb_right_child(x, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       x, order, rb_size(x, order), y, order, rb_size(y, order));
}


void rb_rot_right (struct bu_rb_node *y, int order)
{
    struct bu_rb_node *x;		/* y's child to pivot up */
    struct bu_rb_node *beta;		/* x's child in direction of rot. */
    struct bu_rb_node *y_parent;	/* y's parent */
    struct bu_rb_tree *tree = y->rbn_tree;	/* Tree where it all happens */

    /*
     * Set x and check data types of both x and y
     */
    BU_CKMAG(y, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(y->rbn_tree, order);

    x = rb_left_child(y, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_right(<%p>, %d)...\n", y, order);

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

    rb_size(x, order) = rb_size(y, order);
    rb_size(y, order) =
	rb_size(rb_left_child(y, order), order) +
	rb_size(rb_right_child(y, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       x, order, rb_size(x, order), y, order, rb_size(y, order));
}

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
