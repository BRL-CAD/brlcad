/*			R B _ S E A R C H . C
 *
 *	Written by:	Paul Tanenbaum
 *
 *  $Header$
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "redblack.h"
#include "rb_internals.h"

/*		        _ R B _ S E A R C H ( )
 *
 *	   	Search for a node in a red-black tree
 *
 *	This function has four parameters: the root of the tree
 *	in which to search, the order number and the comparison function,
 *	and a data block containing the desired value of the key.  On
 *	success, _rb_search() returns a pointer to the data block
 *	in the discovered node.  Otherwise, it returns NULL.
 */
static void *_rb_search
	(struct rb_node *root, int order_nm, int (*order)(), void *datum)
{
    int	result;

    while (1)
    {
	if (root == RB_NODE_NULL)
	    return (NULL);
	RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
	if ((result = (*order)(datum, root -> rbn_data)) == 0)
	    break;
	else if (result < 0)
	    root = rb_left_child(root, order_nm);
	else	/* result > 0 */
	    root = rb_right_child(root, order_nm);
    }
    current_node = root;
    return (root -> rbn_data);
}

/*		        R B _ S E A R C H ( )
 *
 *	        Applications interface to _rb_search()
 *
 *	This function has three parameters: the tree in which to search,
 *	the order on which to do the searching, and a data block containing
 *	the desired value of the key.  On success, rb_search() returns a
 *	pointer to the data block in the discovered node.  Otherwise,
 *	it returns NULL.
 */
void *rb_search (rb_tree *tree, int order, void *datum)
{

    int			(*compare)();

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    compare = rb_order_func(tree, order);
    return(_rb_search(rb_root(tree, order), order, compare, datum));
}
