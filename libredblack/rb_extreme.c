/*			R B _ M I N . C
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#include <stdio.h>
#include <math.h>
#include "redblack.h"
#include "rb_internals.h"

/*		        R B _ M I N ( )
 *
 *		Return the minimum node in a red-black tree
 *
 *	This function has two parameters: the tree in which to find the
 *	the minimum node and the order on which to do the search.
 *	is from T. H. Cormen, C. E. Leiserson, and R. L. Rivest.  _Intro-
 *	duction to Algorithms_.  Cambridge, MA: MIT Press, 1990. p. 268.
 *	On success, rb_min() returns a pointer to the minimum node.
 *	Otherwise, it returns RB_NODE_NULL.
 */
void *rb_min (rb_tree *tree, int order_nm)
{
    int			(*order)();  /* Comparison functions */
    struct rb_node	*node;

    /* Check data type of the parameter "tree" */
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    order = (tree -> rbt_order)[order_nm];

    node = tree -> rbt_root;
    while (1)
    {
	RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
	if (rb_left_child(node, order_nm) == RB_NODE_NULL)
	    break;
	node = rb_left_child(node, order_nm);
    }

    /* Record the node with which we've been working */
    current_node = node;
    return (node -> rbn_data);
}
