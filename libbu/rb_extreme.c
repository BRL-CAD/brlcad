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

/*		        _ R B _ E X T R E M E ( )
 *
 *	    Return the minimum or maximum node in a red-black tree
 *
 *	This function has four parameters: the root of the tree, the
 *	order, the sense (min or max), and the address to be understood
 *	as the nil node pointer. _rb_extreme() returns a pointer to the
 *	extreme node.
 */
static struct rb_node *_rb_extreme (struct rb_node *root, int order,
				    int sense, struct rb_node *empty_node)
{
    struct rb_node	*child;

    while (1)
    {
	RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
	RB_CKORDER(root -> rbn_tree, order);

	child = (sense == SENSE_MIN) ? rb_left_child(root, order) :
				       rb_right_child(root, order);
	if (child == empty_node)
	    break;
	root = child;
    }

    /* Record the node with which we've been working */
    current_node = root;

    return (root);
}

/*		        R B _ E X T R E M E ( )
 *
 *		Applications interface to _rb_extreme()
 *
 *	This function has three parameters: the tree in which to find an
 *	extreme node, the order on which to do the search, and the sense
 *	(min or max).  On success, rb_extreme() returns a pointer to the
 *	data in the extreme node.
 */
void *rb_extreme (rb_tree *tree, int order, int sense)
{
    struct rb_node	*node;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	fprintf(stderr,
	    "Error: rb_extreme(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    /* Wade throught the tree */
    node = _rb_extreme(rb_root(tree, order), order, sense, rb_null(tree));

    if (node == rb_null(tree))
	return (NULL);
    else
	return (rb_data(node, order));
}

/*		    _ R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to a given red-black node
 *
 *	This function has three parameters: the node of interest, the
 *	order on which to do the search, and the sense (min or max,
 *	which is to say predecessor or successor).  _rb_neighbor()
 *	returns a pointer to the adjacent node.  The bulk of this code
 *	is from T. H. Cormen, C. E. Leiserson, and R. L. Rivest.
 *	_Introduction to Algorithms_.  Cambridge, MA: MIT Press, 1990.
 *	p. 249.
 */
struct rb_node *_rb_neighbor (struct rb_node *node, int order, int sense)
{
    struct rb_node	*child;
    struct rb_node	*parent;
    struct rb_node	*empty_node = rb_null(node -> rbn_tree);

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(node -> rbn_tree, order);

    fprintf(stderr,
	"_rb_neighbor(<%x>, %d, %d)...\n", (int) node, order, sense);
    child = (sense == SENSE_MIN) ? rb_left_child(node, order) :
				   rb_right_child(node, order);
    if (child != empty_node)
	return (_rb_extreme(child, order, 1 - sense, empty_node));
    parent = rb_parent(node, order);
    while ((parent != empty_node) &&
	   (node == rb_child(parent, order, sense)))
    {
	node = parent;
	parent = rb_parent(parent, order);
    }

    /* Record the node with which we've been working */
    current_node = parent;

    return (parent);
}

/*		        R B _ N E I G H B O R ( )
 *
 *	    Return a node adjacent to the current red-black node
 *
 *	This function has two parameters: the order on which to do the
 *	search and the sense (min or max, which is to say predecessor or
 *	successor).  Rb_neighbor() returns a pointer to the data in the
 *	adjacent node, if that node exists.  Otherwise, it returns NULL.
 */
void *rb_neighbor (int order, int sense)
{
    rb_tree		*tree;
    struct rb_node	*node;

    RB_CKMAG(current_node, RB_NODE_MAGIC, "red-black node");
    tree = current_node -> rbn_tree;
    RB_CKORDER(tree, order);

    if ((sense != SENSE_MIN) && (sense != SENSE_MAX))
    {
	fprintf(stderr,
	    "Error: rb_neighbor(): invalid sense %d, file %s, line %s\n",
	    sense, __FILE__, __LINE__);
	exit (0);
    }

    /* Wade throught the tree */
    node = _rb_neighbor(current_node, order, sense);

    if (node == rb_null(tree))
	return (NULL);
    else
    {
	/* Record the node with which we've been working */
	current_node = node;
	return (rb_data(node, order));
    }
}
