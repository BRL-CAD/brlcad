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
			depth * 2, "", node -> rbn_data);
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
    if (root != 0)
    {
	RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
	visit(root, depth, order);
	internal_walk (rb_left_child(root, order), depth + 1, order, visit);
	internal_walk (rb_right_child(root, order), depth + 1, order, visit);
    }
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
    if (tree -> rbt_nm_orders <= 0)
	fputs("No orders\n", stderr);
    else
	for (i = 0; i < tree -> rbt_nm_orders; ++i)
	{
	    fprintf(stderr,
		    "Order[%d]:   <%x>\n", i, rb_order_func(tree, i));
	    fprintf(stderr,
		    "Root[%d]:    <%x>\n", i, rb_root(tree, i));
	    if (rb_root(tree, 0) != RB_NODE_NULL)
		fprintf(stderr,
		    "Data[%d]:    <%x>\n", i, rb_root(tree, i) -> rbn_data);
	}
    fprintf(stderr, "-------------------------------------------------\n");
}
