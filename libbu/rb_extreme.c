/*			R B _ E X T R E M E . C
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#include <stdio.h>
#include <math.h>
#include "redblack.h"
#include "rb_internals.h"

/*		        R B _ E X T R E M E ( )
 *
 *	    Return the minimum or maximum node in a red-black tree
 *
 *	This function has three parameters: the tree in which to find an
 *	extreme node, the order on which to do the search, and the sense
 *	(min or max).  On success, _rb_extreme() returns a pointer to
 *	the extreme node.  Otherwise, it returns RB_NODE_NULL.
 */
void *rb_extreme (rb_tree *tree, int order_nm, int sense)
{
    int			(*order)();  /* Comparison functions */
    struct rb_node	*child;
    struct rb_node	*node;

    /* Check data type of the parameter "tree" */
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");

    /* Ensure other two parameters are within range */
    RB_CKORDER(tree, order_nm);
    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	fprintf(stderr,
	    "Error: _rb_extreme(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    node = rb_root(tree, order_nm);
    while (1)
    {
	RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
	child = (sense == SENSE_MIN) ? rb_left_child(node, order_nm) :
				       rb_right_child(node, order_nm);
	if (child == RB_NODE_NULL)
	    break;
	node = child;
    }

    /* Record the node with which we've been working */
    if (node != RB_NODE_NULL)
	current_node = node;
    return (node -> rbn_data);
}
