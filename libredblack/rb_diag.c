/*		R B _ D I A G N O S T I C S . C
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

/*		    D E S C R I B E _ N O D E ( )
 *
 *		Print out the contents of a red-black node
 *
 *	This function has three parameters:  the node to describe,
 *	its depth from an ancestor, and the order to describe.
 *	Describe_node() is intended to be called by rb_diagnose_tree().
 */
static void describe_node (struct rb_node *node, int depth, int order)
{
    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    fprintf(stderr, "%*snode <%x>...\n",
			depth * 2, "", node);
    fprintf(stderr, "%*s  tree:   <%x>\n",
			depth * 2, "", node -> rbn_tree);
    fprintf(stderr, "%*s  parent: <%x>\n",
			depth * 2, "", rb_parent(node, order));
    fprintf(stderr, "%*s  left:   <%x>\n",
			depth * 2, "", rb_left_child(node, order));
    fprintf(stderr, "%*s  right:  <%x>\n",
			depth * 2, "", rb_right_child(node, order));
    fprintf(stderr, "%*s  color:  %s\n",
			depth * 2, "",
			(rb_get_color(node, order) == RB_RED) ? "RED"
							      : "BLACK");
    fprintf(stderr, "%*s  data:   <%x>\n",
			depth * 2, "", rb_data(node, order));
}

/*		    I N T E R N A L _ W A L K ( )
 *
 *	    Perform an inorder tree walk on a red-black tree
 *
 *	This function has four parameters: the root of the tree
 *	to traverse, the depth of this root from the ancestor at
 *	which we began, the order on which to do the walking, and the
 *	function to apply to each node.  Internal_walk() is entirely
 *	analogous to _rb_walk(), except that instead of visiting
 *	rbn_data in each node, it visits the node itself.
 */
static void internal_walk (struct rb_node *root, int depth,
			   int order, void (*visit)())
{

    /* Check data type of the parameter "root" */
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    if (root == rb_null(root -> rbn_tree))
	return;
    visit(root, depth, order);
    internal_walk (rb_left_child(root, order), depth + 1, order, visit);
    internal_walk (rb_right_child(root, order), depth + 1, order, visit);
}

/*		    R B _ D I A G N O S E _ T R E E ( )
 *
 *	Produce an inorder diagnostic printout of a red-black tree
 *
 *	This function has two parameters: the root and order of the tree
 *	to print out.
 */
void rb_diagnose_tree (rb_tree *tree, int order)
{
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    fprintf(stderr, "-------- Red-black tree <%x> contents --------\n", tree);
    fprintf(stderr, "Description: '%s'\n", tree -> rbt_description);
    fprintf(stderr, "Order:       %d of %d\n", order, tree -> rbt_nm_orders);
    if (rb_root(tree, order) == rb_null(tree))
	fprintf(stderr, "Empty tree!  %x %x\n",
	    rb_root(tree, order), rb_null(tree));
    else
	fprintf(stderr, "Non-empty tree!  %x %x\n",
	    rb_root(tree, order), rb_null(tree));
    internal_walk(rb_root(tree, order), 0, order, describe_node);
    fprintf(stderr, "--------------------------------------------------\n");
}

/*		R B _ S U M M A R I Z E _ T R E E ( )
 *
 *		    Describe a red-black tree
 *
 *	This function has one parameter: a pointer to a red-black
 *	tree.  Rb_describe() prints out the header information
 *	for the tree.  It is intended for diagnostic purposes.
 */
void rb_summarize_tree (rb_tree *tree)
{
    int		i;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    fprintf(stderr, "-------- Red-black tree <%x> summary --------\n", tree);
    fprintf(stderr, "Description: '%s'\n", tree -> rbt_description);
    fprintf(stderr, "Empty node:  <%x>\n", tree -> rbt_empty_node);
    if (tree -> rbt_nm_orders <= 0)
	fputs("No orders\n", stderr);
    else
    {
	fprintf(stderr, "i    Order[i]     Root[i]       Data[i]\n");
	for (i = 0; i < tree -> rbt_nm_orders; ++i)
	{
	    fprintf(stderr, "%-3d  <%x>    <%x>    <%x>\n",
		    i,
		    rb_order_func(tree, i),
		    rb_root(tree, i),
		    (rb_root(tree, i) == RB_NODE_NULL) ? 0 :
			rb_root(tree, i) -> rbn_data[i]);
	}
    }
    fprintf(stderr, "-------------------------------------------------\n");
}
