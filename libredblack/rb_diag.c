/*		R B _ D I A G N O S T I C S . C
 *
 *	Written by:	Paul Tanenbaum
 *
 */
#ifndef lint
static char RCSid[] = "@(#) $Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "redblack.h"
#include "rb_internals.h"

static int d_order;	/* Used by describe_node() */

/*		    D E S C R I B E _ N O D E ( )
 *
 *		Print out the contents of a red-black node
 *
 *	This function has one parameter:  the node to describe.
 *	Describe_node() is intended to be called by rb_diagnose_tree().
 */
static void describe_node (struct rb_node *node, int depth)
{
    rb_tree		*tree;
    struct rb_package	*package;
    void		(*pp)();	/* Pretty print function */

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    tree = node -> rbn_tree;
    RB_CKORDER(tree, d_order);

    package = (node -> rbn_package)[d_order];
    pp = tree -> rbt_print;

#if 0
    fprintf(stderr, "%*snode <%x>...\n",
			depth * 2, "", node);
    fprintf(stderr, "%*s  tree:   <%x>\n",
			depth * 2, "", node -> rbn_tree);
    fprintf(stderr, "%*s  parent: <%x>\n",
			depth * 2, "", rb_parent(node, d_order));
    fprintf(stderr, "%*s  left:   <%x>\n",
			depth * 2, "", rb_left_child(node, d_order));
    fprintf(stderr, "%*s  right:  <%x>\n",
			depth * 2, "", rb_right_child(node, d_order));
    fprintf(stderr, "%*s  color:  %s\n",
			depth * 2, "",
			(rb_get_color(node, d_order) == RB_RED) ? "RED"
							      : "BLACK");
    fprintf(stderr, "%*s  package: <%x> ",
			depth * 2, "", package);
#else
    fprintf(stderr, "%*s", depth * 8, "");
#endif
    if ((pp != 0) && (package != RB_PKG_NULL))
	(*pp)(package -> rbp_data);
    else
	fprintf(stderr, "\n");
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
    fprintf(stderr, "Current:     <%x>\n", tree -> rbt_current);
    fprintf(stderr, "Empty node:  <%x>\n", tree -> rbt_empty_node);
    if (rb_root(tree, order) == rb_null(tree))
	fprintf(stderr, "Empty tree!  %x %x\n",
	    rb_root(tree, order), rb_null(tree));
    d_order = order;
    _rb_walk(tree, order, describe_node, WALK_NODES, INORDER);
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
    fprintf(stderr, "Current:     <%x>\n", tree -> rbt_current);
    fprintf(stderr, "Empty node:  <%x>\n", tree -> rbt_empty_node);
    if (tree -> rbt_nm_orders <= 0)
	fputs("No orders\n", stderr);
    else
    {
	fprintf(stderr,
	    "i    Order[i]     Root[i]       Package[i]    Data[i]\n");
	for (i = 0; i < tree -> rbt_nm_orders; ++i)
	{
	    fprintf(stderr, "%-3d  <%x>    <%x>    <%x>    <%x>\n",
		    i,
		    rb_order_func(tree, i),
		    rb_root(tree, i),
		    (rb_root(tree, i) == RB_NODE_NULL) ? 0 :
			(rb_root(tree, i) -> rbn_package)[i],
		    (rb_root(tree, i) == RB_NODE_NULL) ? 0 :
			rb_data(rb_root(tree, i), i));
	}
    }
    fprintf(stderr, "-------------------------------------------------\n");
}
