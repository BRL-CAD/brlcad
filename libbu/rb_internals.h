/*			R B _ I N T E R N A L S . H
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#ifndef REDBLACK_H
#include "redblack.h"
#endif

#ifndef RB_INTERNALS_H
#define RB_INTERNALS_H seen

/*			R B _ C K M A G ( )
 *	    Check and validate a structure pointer
 *
 *	This macro has three parameters: a pointer, the magic number
 *	expected at that location, and a string describing the expected
 *	structure type.
 */
#define	RB_CKMAG(p, m, _s)						\
    if ((p) == 0)							\
    {									\
	fprintf(stderr, "Error: Null %s pointer, file %s, line %d\n",	\
	    (_s), __FILE__, __LINE__);					\
	exit (0);							\
    }									\
    else if (*((long *)(p)) != (m))					\
    {									\
	fprintf(stderr,							\
	    "Error: Bad %s pointer x%x s/b x%x was x%x, file %s, line %d\n", \
	    (_s), (p), (m), *((long *)(p)), __FILE__, __LINE__);	\
	exit (0);							\
    }
#define	RB_TREE_MAGIC		0x72627472
#define	RB_NODE_MAGIC		0x72626e6f

/*			R B _ C K O R D E R ( )
 *
 *	This macro has two parameters: a tree and an order number.
 *	It ensures that the order number is valid for the tree.
 */
#define RB_CKORDER(t, o)						\
    if (((o) < 0) || ((o) >= (t) -> rbt_nm_orders))			\
    {									\
	fprintf(stderr,							\
	    "Error: Order %d outside 0..%d (nm_orders-1), file %s, line %s\n", \
	    (o), (t) -> rbt_nm_orders - 1, __FILE__, __LINE__);		\
	exit (0);							\
    }

/*
 *	Access functions for fields of rb_tree and (struct rb_node)
 */
#define	rb_order_func(t, o)	(((t) -> rbt_order)[o])
#define	rb_root(t, o)		(((t) -> rbt_root)[o])
#define	rb_parent(n, o)		(((n) -> rbn_parent)[o])
#define	rb_left_child(n, o)	(((n) -> rbn_left)[o])
#define	rb_right_child(n, o)	(((n) -> rbn_right)[o])
#define	RB_LEFT			0
#define	RB_RIGHT		1
#define	rb_child(n, o, d)	(((d) == RB_LEFT)		? 	\
				    rb_left_child((n), (o))	:	\
				    rb_right_child((n), (o)))
#define	rb_other_child(n, o, d)	(((d) == RB_LEFT)		?	\
				    rb_right_child((n), (o))	:	\
				    rb_left_child((n), (o)))
#define	rb_get_color(n, o)						\
(									\
    (((n) -> rbn_color)[(o)/8] & (0x1 << ((o) % 8))) ? 1 : 0		\
)

#define	rb_set_color(n, o, c)						\
{									\
    int	_b = (o) / 8;							\
    int _p = (o) - _b * 8;						\
									\
    RB_CKMAG((n), RB_NODE_MAGIC, "red-black node");			\
    ((n) -> rbn_color)[_b] &= 0x1 << _p;				\
    ((n) -> rbn_color)[_b] |= (c) << _p;				\
}
#define	RB_RED			0
#define	RB_BLACK		1

/*
 *	Global variables within LIBREDBLACK
 */
#ifdef RB_CREATE
    struct rb_node		*current_node = RB_NODE_NULL;
#else
    extern struct rb_node	*current_node;
#endif

/*
 *	Functions internal to LIBREDBLACK
 */
void left_rotate	(
			    struct rb_node	*node,
			    int			order
			);
void right_rotate	(
			    struct rb_node	*node,
			    int			order
			);

#endif /* RB_INTERNALS_H */
