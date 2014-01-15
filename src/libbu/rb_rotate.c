/*                     R B _ R O T A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
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

    y = RB_RIGHT_CHILD(x, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_left(<%p>, %d)...\n", (void*)x, order);

    RB_RIGHT_CHILD(x, order) = beta = RB_LEFT_CHILD(y, order);
    if (beta != RB_NULL(tree))
	RB_PARENT(beta, order) = x;
    RB_PARENT(y, order) = x_parent = RB_PARENT(x, order);
    if (x_parent == RB_NULL(tree))
	RB_ROOT(tree, order) = y;
    else if (x == RB_LEFT_CHILD(x_parent, order))
	RB_LEFT_CHILD(x_parent, order) = y;
    else
	RB_RIGHT_CHILD(x_parent, order) = y;
    RB_LEFT_CHILD(y, order) = x;
    RB_PARENT(x, order) = y;

    RB_SIZE(y, order) = RB_SIZE(x, order);
    RB_SIZE(x, order) =
	RB_SIZE(RB_LEFT_CHILD(x, order), order) +
	RB_SIZE(RB_RIGHT_CHILD(x, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       (void*)x, order, RB_SIZE(x, order), (void*)y, order, RB_SIZE(y, order));
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

    x = RB_LEFT_CHILD(y, order);

    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_ROTATE))
	bu_log("rb_rot_right(<%p>, %d)...\n", (void*)y, order);

    RB_LEFT_CHILD(y, order) = beta = RB_RIGHT_CHILD(x, order);
    if (beta != RB_NULL(tree))
	RB_PARENT(beta, order) = y;
    RB_PARENT(x, order) = y_parent = RB_PARENT(y, order);
    if (y_parent == RB_NULL(tree))
	RB_ROOT(tree, order) = x;
    else if (y == RB_LEFT_CHILD(y_parent, order))
	RB_LEFT_CHILD(y_parent, order) = x;
    else
	RB_RIGHT_CHILD(y_parent, order) = x;
    RB_RIGHT_CHILD(x, order) = y;
    RB_PARENT(y, order) = x;

    RB_SIZE(x, order) = RB_SIZE(y, order);
    RB_SIZE(y, order) =
	RB_SIZE(RB_LEFT_CHILD(y, order), order) +
	RB_SIZE(RB_RIGHT_CHILD(y, order), order) + 1;
    if (UNLIKELY(tree->rbt_debug & BU_RB_DEBUG_OS))
	bu_log("After rotation, size(%p, %d)=%d, size(%p, %d)=%d\n",
	       (void*)x, order, RB_SIZE(x, order), (void*)y, order, RB_SIZE(y, order));
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
