/*			R B _ O R D E R _ S T A T S . C
 *
 *	Routines to support order-statistic operations for a red-black tree
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
static char libbu_rb_order_stats_RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"

/*		        _ R B _ S E L E C T ( )
 *
 *	Retrieve the element of rank k in one order of a red-black tree
 *
 *	This function has three parameters: the root of the tree to search,
 *	the order on which to do the searching, and the rank of interest.
 *	_rb_select() returns the discovered node.  It is an implemenation
 *	of the routine OS-SELECT on p. 282 of Cormen et al.
 */
static struct bu_rb_node *_rb_select (root, order, k)

struct bu_rb_node	*root;
int			order;
int			k;

{
    int		rank;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");

    rank = bu_rb_size(bu_rb_left_child(root, order), order) + 1;
    if (root -> rbn_tree -> rbt_debug & BU_RB_DEBUG_OS)
	bu_log("_rb_select(<%x>, %d, %d): rank=%d\n",
	    root, order, k, rank);
    
    if (rank == k)
	return (root);
    else if (rank > k)
	return (_rb_select(bu_rb_left_child(root, order), order, k));
    else
	return (_rb_select(bu_rb_right_child(root, order), order, k - rank));
}

/*		        B U _ R B _ S E L E C T ( )
 *
 *		Applications interface to _rb_select()
 *
 *	This function has three parameters: the tree in which to search,
 *	the order on which to do the searching, and the rank of interest.
 *	On success, bu_rb_select() returns a pointer to the data block in
 *	the discovered node.  Otherwise, it returns NULL.
 */
void *bu_rb_select (tree, order, k)

bu_rb_tree	*tree;
int		order;
int		k;

{
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((k < 1) || (k > tree -> rbt_nm_nodes))
    {
	if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	    bu_log("bu_rb_select(<%x>, %d, %d): k out of bounds [1, %d]\n",
		tree, order, k, tree -> rbt_nm_nodes);
	bu_rb_current(tree) = bu_rb_null(tree);
	return (NULL);
    }
    if (tree -> rbt_debug & BU_RB_DEBUG_OS)
	bu_log("bu_rb_select(<%x>, %d, %d): root=<%x>\n",
	    tree, order, k, bu_rb_root(tree, order));

    bu_rb_current(tree) = node
			= _rb_select(bu_rb_root(tree, order), order, k);
    return (bu_rb_data(node, order));
}

/*		        B U _ R B _ R A N K ( )
 *
 *	Determines the rank of a node in one order of a red-black tree
 *
 *	This function has two parameters: the tree in which to search
 *	and the order on which to do the searching.  If the current node
 *	is null, bu_rb_rank() returns 0.  Otherwise, it returns the rank
 *	of the current node in the specified order.  bu_rb_rank() is an
 *	implementation of the routine OS-RANK on p. 283 of Cormen et al.
 */
int bu_rb_rank (tree, order)

bu_rb_tree	*tree;
int		order;

{
    int			rank;
    struct bu_rb_node	*node;
    struct bu_rb_node	*parent;
    struct bu_rb_node	*root;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    if ((node = bu_rb_current(tree)) == bu_rb_null(tree))
	return (0);

    root = bu_rb_root(tree, order);
    rank = bu_rb_size(bu_rb_left_child(node, order), order) + 1;
    while (node != root)
    {
	parent = bu_rb_parent(node, order);
	if (node == bu_rb_right_child(parent, order)) 
	    rank += bu_rb_size(bu_rb_left_child(parent, order), order) + 1;
	node = parent;
    }

    return (rank);
}
