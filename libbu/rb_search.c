/*			R B _ S E A R C H . C
 *
 *	Routines to search for a node in a red-black tree
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

/*		        _ R B _ S E A R C H ( )
 *
 *	   	Search for a node in a red-black tree
 *
 *	This function has four parameters: the root and order of the tree
 *	in which to search, the comparison function, and a data block
 *	containing the desired value of the key.  On success, _rb_search()
 *	returns a pointer to the discovered node.  Otherwise, it returns
 *	(tree -> rbt_empty_node).
 */
static struct rb_node *_rb_search (root, order_nm, order, data)

struct rb_node	*root;
int		order_nm;
int		(*order)();
void		*data;

{
    int		result;
    rb_tree	*tree;

    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    tree = root -> rbn_tree;
    RB_CKORDER(tree, order_nm);

    while (1)
    {
	if (root == rb_null(root -> rbn_tree))
	    return (root);
	if ((result = (*order)(data, rb_data(root, order_nm))) == 0)
	    break;
	else if (result < 0)
	    root = rb_left_child(root, order_nm);
	else	/* result > 0 */
	    root = rb_right_child(root, order_nm);
	RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    }
    rb_current(tree) = root;
    return (root);
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
void *rb_search (tree, order, data)

rb_tree	*tree;
int	order;
void	*data;

{

    int			(*compare)();
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    compare = rb_order_func(tree, order);
    node = _rb_search(rb_root(tree, order), order, compare, data);
    if (node == rb_null(tree))
	return (NULL);
    else
	return (rb_data(node, order));
}
