/*                  R B _ I N T E R N A L S . H
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

#include "bu.h"

#ifndef BU_RB_INTERNALS_H
#define BU_RB_INTERNALS_H seen


/**
 * R B _ C K O R D E R
 *
 * This internal macro has two parameters: a tree and an order number.
 * It ensures that the order number is valid for the tree.
 */
#define RB_CKORDER(t, o)						\
    if (UNLIKELY(((o) < 0) || ((o) >= (t)->rbt_nm_orders))) {		\
	char buf[128] = {0};						\
	snprintf(buf, 128, "ERROR: Order %d outside 0..%d (nm_orders-1), file %s, line %d\n", \
		 (o), (t)->rbt_nm_orders - 1, __FILE__, __LINE__);	\
	bu_bomb(buf);							\
    }

/*
 * Access functions for fields of struct bu_rb_tree
 */
#define rb_order_func(t, o) (((t)->rbt_order)[o])
#define rb_print(t, p) (((t)->rbt_print)((p)->rbp_data))
#define rb_root(t, o) (((t)->rbt_root)[o])
#define rb_current(t) ((t)->rbt_current)
#define rb_null(t) ((t)->rbt_empty_node)
#define rb_get_uniqueness(t, o) ((((t)->rbt_unique)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0)
#define rb_set_uniqueness(t, o, u) {		\
	int _b = (o) / 8;			\
	int _p = (o) - _b * 8;			\
	((t)->rbt_unique)[_b] &= ~(0x1 << _p);	\
	((t)->rbt_unique)[_b] |= (u) << _p;	\
    }

/*
 * Access functions for fields of (struct bu_rb_node)
 */
#define rb_parent(n, o) (((n)->rbn_parent)[o])
#define rb_left_child(n, o) (((n)->rbn_left)[o])
#define rb_right_child(n, o) (((n)->rbn_right)[o])
#define RB_LEFT 0
#define RB_RIGHT 1
#define rb_child(n, o, d) (((d) == RB_LEFT) ?		\
			   rb_left_child((n), (o)) :	\
			   rb_right_child((n), (o)))
#define rb_other_child(n, o, d) (((d) == RB_LEFT) ?		\
				 rb_right_child((n), (o)) :	\
				 rb_left_child((n), (o)))
#define rb_size(n, o) (((n)->rbn_size)[o])
#define rb_get_color(n, o)					\
    ((((n)->rbn_color)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0)
#define rb_set_color(n, o, c) {			\
	int _b = (o) / 8;			\
	int _p = (o) - _b * 8;			\
	((n)->rbn_color)[_b] &= ~(0x1 << _p);	\
	((n)->rbn_color)[_b] |= (c) << _p;	\
    }
#define RB_RED 0
#define RB_BLK 1
#define rb_data(n, o) (((n)->rbn_package)[o]->rbp_data)

/**
 * Interface to rb_walk()
 * (Valid values for the parameter what_to_walk)
 */
#define WALK_NODES 0
#define WALK_DATA 1

/**
 * R B _ R O T A T E
 *
 * This macro has three parameters: the node about which to rotate,
 * the order to be rotated, and the direction of rotation.  They allow
 * indirection in the use of rb_rot_left() and rb_rot_right().
 */
#define rb_rotate(n, o, d) (((d) == RB_LEFT) ?		\
			    rb_rot_left((n), (o)) :	\
			    rb_rot_right((n), (o)))

/**
 * B U _ R B _ O T H E R _ R O T A T E
 *
 * This macro has three parameters: the node about which to rotate,
 * the order to be rotated, and the direction of rotation.  They allow
 * indirection in the use of rb_rot_left() and rb_rot_right().
 */
#define rb_other_rotate(n, o, d) (((d) == RB_LEFT) ?		\
				  rb_rot_right((n), (o)) :	\
				  rb_rot_left((n), (o)))

/*
 * Functions internal to LIBREDBLACK
 */

/**
 * R B _ N E I G H B O R ()
 *
 * Return a node adjacent to a given red-black node
 *
 * This function has three parameters: the node of interest, the order
 * on which to do the search, and the sense (min or max, which is to
 * say predecessor or successor).  rb_neighbor() returns a pointer to
 * the adjacent node.  This function is modeled after the routine
 * TREE-SUCCESSOR on p. 249 of Cormen et al.
 */
extern struct bu_rb_node *rb_neighbor(struct bu_rb_node *node, int order, int sense);

/** @file rb_rotate.c
 *
 * Routines to perform rotations on a red-black tree
 *
 */

/**
 * R B _ R O T _ L E F T
 *
 * Perfrom left rotation on a red-black tree
 *
 * This function has two parameters: the node about which to rotate
 * and the order to be rotated.  rb_rot_left() is an implementation of
 * the routine called LEFT-ROTATE on p. 266 of Cormen et al, with
 * modification on p. 285.
 */
extern void rb_rot_left(struct bu_rb_node *x, int order);

/**
 * R B _ R O T _ R I G H T
 *
 * Perfrom right rotation on a red-black tree
 *
 * This function has two parameters: the node about which to rotate
 * and the order to be rotated.  rb_rot_right() is hacked from
 * rb_rot_left() above.
 */
extern void rb_rot_right(struct bu_rb_node *y, int order);

/**
 * R B _ W A L K
 *
 * Traverse a red-black tree
 *
 * This function has five parameters: the tree to traverse, the order
 * on which to do the walking, the function to apply to each node,
 * whether to apply the function to the entire node (or just to its
 * data), and the type of traversal (preorder, inorder, or postorder).
 *
 * N.B. rb_walk() is not declared static because it is called by
 * bu_rb_diagnose_tree() in rb_diag.c.
 */
extern void rb_walk(struct bu_rb_tree *tree, int order, void (*visit) (/* ??? */), int what_to_visit, int trav_type);


/**
 * R B _ F R E E _ N O D E
 *
 * Relinquish memory occupied by a red-black node
 *
 * This function has one parameter: a node to free.  rb_free_node()
 * frees the memory allocated for the various members of the node and
 * then frees the memory allocated for the node itself.
 */
extern void rb_free_node(struct bu_rb_node *node);

/**
 * R B _ F R E E _ P A C K A G E
 *
 * Relinquish memory occupied by a red-black package
 *
 * This function has one parameter: a package to free.
 * rb_free_package() frees the memory allocated to point to the
 * nodes that contained the package and then frees the memory
 * allocated for the package itself.
 */
extern void rb_free_package(struct bu_rb_package *package);

#endif /* BU_RB_INTERNALS_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
