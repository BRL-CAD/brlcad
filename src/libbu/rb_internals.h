/*                  R B _ I N T E R N A L S . H
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @addtogroup rb */
/** @{ */
/** @file rb_internals.h
 *
 *	The constants, macro functions, etc. need within LIBBU(3)
 *	to handle the red-black tree utilities.
 *
 *  @author
 *	Paul J. Tanenbaum
 *
 *  @par Source -
 *	The U. S. Army Research Laboratory
 *  @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  $Header$
 */

#include "compat4.h"

#ifndef SEEN_BU_H
#include "bu.h"
#endif

#ifndef BU_RB_INTERNALS_H
#define BU_RB_INTERNALS_H seen

/**			B U _ R B _ C K M A G ( )
 *	    Check and validate a structure pointer
 *
 *	This macro has three parameters: a pointer, the magic number
 *	expected at that location, and a string describing the expected
 *	structure type.
 */
#define	BU_RB_CKMAG		BU_CKMAG
#define	BU_RB_NODE_MAGIC	0x72626e6f
#define	BU_RB_PKG_MAGIC		0x7262706b
#define	BU_RB_LIST_MAGIC	0x72626c73

/**			B U _ R B _ C K O R D E R ( )
 *
 *	This macro has two parameters: a tree and an order number.
 *	It ensures that the order number is valid for the tree.
 */
#define BU_RB_CKORDER(t, o)					\
    if (((o) < 0) || ((o) >= (t) -> rbt_nm_orders))		\
    {								\
	bu_log(							\
	    "Error: Order %d outside 0..%d (nm_orders-1), file %s, line %d\n", \
	    (o), (t) -> rbt_nm_orders - 1, __FILE__, __LINE__);	\
	exit (0);						\
    }

/*
 *	Access functions for fields of bu_rb_tree
 */
#define	bu_rb_order_func(t, o)	(((t) -> rbt_order)[o])
#define	bu_rb_print(t, p)	(((t) -> rbt_print)((p) -> rbp_data))
#define	bu_rb_root(t, o)	(((t) -> rbt_root)[o])
#define bu_rb_current(t)	((t) -> rbt_current)
#define bu_rb_null(t)		((t) -> rbt_empty_node)
#define	bu_rb_get_uniqueness(t, o)					\
(									\
    (((t) -> rbt_unique)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0		\
)
#define	bu_rb_set_uniqueness(t, o, u)					\
{									\
    int	_b = (o) / 8;							\
    int _p = (o) - _b * 8;						\
									\
    ((t) -> rbt_unique)[_b] &= ~(0x1 << _p);				\
    ((t) -> rbt_unique)[_b] |= (u) << _p;				\
}

/*
 *	Access functions for fields of (struct bu_rb_node)
 */
#define	bu_rb_parent(n, o)	(((n) -> rbn_parent)[o])
#define	bu_rb_left_child(n, o)	(((n) -> rbn_left)[o])
#define	bu_rb_right_child(n, o)	(((n) -> rbn_right)[o])
#define	BU_RB_LEFT		0
#define	BU_RB_RIGHT		1
#define	bu_rb_child(n, o, d)	(((d) == BU_RB_LEFT)		? 	\
				    bu_rb_left_child((n), (o))	:	\
				    bu_rb_right_child((n), (o)))
#define	bu_rb_other_child(n, o, d) (((d) == BU_RB_LEFT)		?	\
				    bu_rb_right_child((n), (o))	:	\
				    bu_rb_left_child((n), (o)))
#define	bu_rb_size(n, o)	(((n) -> rbn_size)[o])
#define	bu_rb_get_color(n, o)						\
(									\
    (((n) -> rbn_color)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0		\
)
#define	bu_rb_set_color(n, o, c)					\
{									\
    int	_b = (o) / 8;							\
    int _p = (o) - _b * 8;						\
									\
    ((n) -> rbn_color)[_b] &= ~(0x1 << _p);				\
    ((n) -> rbn_color)[_b] |= (c) << _p;				\
}
#define	BU_RB_RED		0
#define	BU_RB_BLACK		1
#define	bu_rb_data(n, o)	(((n) -> rbn_package)[o] -> rbp_data)

/**
 *	Interface to _rb_walk()
 *	(Valid values for the parameter what_to_walk)
 */
#define	WALK_NODES		0
#define	WALK_DATA		1

/**		    B U _ R B _ R O T A T E ( )
 *			    and
 *		B U _ R B _ O T H E R _ R O T A T E ( )
 *
 *	These macros have three parameters: the node about which
 *	to rotate, the order to be rotated, and the direction of
 *	rotation.  They allow indirection in the use of _rb_rot_left()
 *	and _rb_rot_right().
 */
#define	bu_rb_rotate(n, o, d)	(((d) == BU_RB_LEFT)		? 	\
				    _rb_rot_left((n), (o))	:	\
				    _rb_rot_right((n), (o)))
#define	bu_rb_other_rotate(n, o, d) (((d) == BU_RB_LEFT)	? 	\
				    _rb_rot_right((n), (o))	:	\
				    _rb_rot_left((n), (o)))

/*
 *	Functions internal to LIBREDBLACK
 */
BU_EXTERN(struct bu_rb_node *_rb_neighbor, (struct bu_rb_node *node,
					 int		order,
					 int		sense
					));
BU_EXTERN(void _rb_rot_left,		(struct bu_rb_node	*x,
					 int		order
					));
BU_EXTERN(void _rb_rot_right,		(struct bu_rb_node	*y,
					 int		order
					));
BU_EXTERN(void _rb_walk,		(bu_rb_tree	*tree,
			    		 int		order,
					 void		(*visit)(),
					 int		what_to_visit,
					 int		trav_type
					));
BU_EXTERN(void bu_rb_free_node,		(struct bu_rb_node *node));
BU_EXTERN(void bu_rb_free_package,	(struct bu_rb_package *package));

#endif /* BU_RB_INTERNALS_H */
/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
