/*			R B _ O R D E R _ S T A T S . C
 *
 *	Routines to support order-statistic operations for a red-black tree
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
#include "rtlist.h"
#include "redblack.h"
#include "./rb_internals.h"

/*		        _ R B _ G E T _ K T H ( )
 *
 *	Retrieve the element of rank k in one order of a red-black tree
 *
 *	This function has three parameters: the root of the tree to search,
 *	the order on which to do the searching, and the rank of interest.
 *	On success, _rb_get_kth() returns a pointer to the data block in
 *	the discovered node.  Otherwise, it returns NULL.
 */
static void *_rb_get_kth (root, order, k)

struct rb_node	*root;
int		order;
int		k;

{
    int			rank;

    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");

    rank = rb_size(rb_left_child(root, order), order) + 1;
    if (root -> rbn_tree -> rbt_debug & RB_DEBUG_OS)
	rt_log("_rb_get_kth(<%x>, %d, %d): rank=%d\n",
	    root, order, k, rank);
    
    if (rank == k)
	return (rb_data(root, order));
    else if (rank > k)
	return (_rb_get_kth(rb_left_child(root, order), order, k));
    else
	return (_rb_get_kth(rb_right_child(root, order), order, k - rank));
}

/*		        R B _ G E T _ K T H ( )
 *
 *		Applications interface to _rb_get_kth()
 *
 *	This function has three parameters: the tree in which to search,
 *	the order on which to do the searching, and the rank of interest.
 *	On success, rb_get_kth() returns a pointer to the data block in
 *	the discovered node.  Otherwise, it returns NULL.
 */
void *rb_get_kth (tree, order, k)

rb_tree	*tree;
int	order;
int	k;

{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (tree -> rbt_debug & RB_DEBUG_OS)
	rt_log("rb_get_kth(<%x>, %d, %d): root=<%x>\n",
	    tree, order, k, rb_root(tree, order));

    return (_rb_get_kth(rb_root(tree, order), order, k));
}

/*		        R B _ R A N K ( )
 *
 *	Determines the rank of a node in one order of a red-black tree
 *
 *	This function has two parameters: the tree in which to search
 *	and the order on which to do the searching.  Rb_rank() returns
 *	the rank of the current node in the specified order.
 */
int rb_rank (tree, order)

rb_tree	*tree;
int	order;

{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if (node == rb_null(tree))
	return (0);
    else
	return (1);
}
