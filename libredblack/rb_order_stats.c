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

/*		        _ R B _ S E L E C T ( )
 *
 *	Retrieve the element of rank k in one order of a red-black tree
 *
 *	This function has three parameters: the root of the tree to search,
 *	the order on which to do the searching, and the rank of interest.
 *	On success, _rb_select() returns a pointer to the data block in
 *	the discovered node.  Otherwise, it returns NULL.  _rb_select() is
 *	an implementation of the routine OS-SELECT on p. 282 of Cormen et al.
 */
static struct rb_node *_rb_select (root, order, k)

struct rb_node	*root;
int		order;
int		k;

{
    int		rank;

    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");

    rank = rb_size(rb_left_child(root, order), order) + 1;
    if (root -> rbn_tree -> rbt_debug & RB_DEBUG_OS)
	rt_log("_rb_select(<%x>, %d, %d): rank=%d\n",
	    root, order, k, rank);
    
    if (rank == k)
	return (root);
    else if (rank > k)
	return (_rb_select(rb_left_child(root, order), order, k));
    else
	return (_rb_select(rb_right_child(root, order), order, k - rank));
}

/*		        R B _ S E L E C T ( )
 *
 *		Applications interface to _rb_select()
 *
 *	This function has three parameters: the tree in which to search,
 *	the order on which to do the searching, and the rank of interest.
 *	On success, rb_select() returns a pointer to the data block in
 *	the discovered node.  Otherwise, it returns NULL.
 */
void *rb_select (tree, order, k)

rb_tree	*tree;
int	order;
int	k;

{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((k < 1) || (k > tree -> rbt_nm_nodes))
    {
	if (tree -> rbt_debug & RB_DEBUG_OS)
	    rt_log("rb_select(<%x>, %d, %d): k out of bounds [1, %d]\n",
		tree, order, k, tree -> rbt_nm_nodes);
	rb_current(tree) = rb_null(tree);
	return (NULL);
    }
    if (tree -> rbt_debug & RB_DEBUG_OS)
	rt_log("rb_select(<%x>, %d, %d): root=<%x>\n",
	    tree, order, k, rb_root(tree, order));

    rb_current(tree) = node = _rb_select(rb_root(tree, order), order, k);
    return (rb_data(node, order));
}

/*		        R B _ R A N K ( )
 *
 *	Determines the rank of a node in one order of a red-black tree
 *
 *	This function has two parameters: the tree in which to search
 *	and the order on which to do the searching.  If the current node
 *	is null, rb_rank() returns 0.  Otherwise, it returns the rank
 *	of the current node in the specified order.  rb_rank() is an
 *	implementation of the routine OS-RANK on p. 283 of Cormen et al.
 */
int rb_rank (tree, order)

rb_tree	*tree;
int	order;

{
    int			rank;
    struct rb_node	*node;
    struct rb_node	*parent;
    struct rb_node	*root;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((node = rb_current(tree)) == rb_null(tree))
	return (0);

    root = rb_root(tree, order);
    rank = rb_size(rb_left_child(node, order), order) + 1;
    while (node != root)
    {
	parent = rb_parent(node, order);
	if (node == rb_right_child(parent, order)) 
	    rank += rb_size(rb_left_child(parent, order), order) + 1;
	node = parent;
    }

    return (rank);
}
