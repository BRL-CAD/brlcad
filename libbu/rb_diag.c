/*		R B _ D I A G N O S T I C S . C
 *
 *	Diagnostic routines for red-black tree maintenance
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
#include "redblack.h"
#include "./rb_internals.h"

static int d_order;	/* Used by describe_node() */

/*		    D E S C R I B E _ N O D E ( )
 *
 *		Print out the contents of a red-black node
 *
 *	This function has two parameters:  the node to describe and
 *	its depth in the tree.  Describe_node() is intended to be
 *	called by rb_diagnose_tree().
 */
static void describe_node (node, depth)

struct rb_node	*node;
int		depth;

{
    rb_tree		*tree;
    struct rb_package	*package;
    void		(*pp)();	/* Pretty print function */

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");
    tree = node -> rbn_tree;
    RB_CKORDER(tree, d_order);

    package = (node -> rbn_package)[d_order];
    pp = tree -> rbt_print;

#if 1
    rt_log("%*snode <%x>...\n", depth * 2, "", node);
    rt_log("%*s  tree:   <%x>\n", depth * 2, "", node -> rbn_tree);
    rt_log("%*s  parent: <%x>\n", depth * 2, "", rb_parent(node, d_order));
    rt_log("%*s  left:   <%x>\n", depth * 2, "", rb_left_child(node, d_order));
    rt_log("%*s  right:  <%x>\n", depth * 2, "", rb_right_child(node, d_order));
    rt_log("%*s  color:  %s\n",
	    depth * 2, "",
	    (rb_get_color(node, d_order) == RB_RED) ? "RED" :
	    (rb_get_color(node, d_order) == RB_BLACK) ? "BLACK" : "Huhh?");
    rt_log("%*s  package: <%x> ", depth * 2, "", package);
#else
    rt_log("%*s", depth * 8, "");
#endif
    if ((pp != 0) && (package != RB_PKG_NULL))
	(*pp)(package -> rbp_data);
    else
	rt_log("\n");
}

/*		    R B _ D I A G N O S E _ T R E E ( )
 *
 *	    Produce a diagnostic printout of a red-black tree
 *
 *	This function has three parameters: the root and order of the tree
 *	to print out and the type of traversal (preorder, inorder, or
 *	postorder).
 */
void rb_diagnose_tree (tree, order, trav_type)

rb_tree	*tree;
int	order;
int	trav_type;

{
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    rt_log("-------- Red-black tree <%x> contents --------\n", tree);
    rt_log("Description: '%s'\n", tree -> rbt_description);
    rt_log("Order:       %d of %d\n", order, tree -> rbt_nm_orders);
    rt_log("Current:     <%x>\n", tree -> rbt_current);
    rt_log("Empty node:  <%x>\n", tree -> rbt_empty_node);
    d_order = order;
    _rb_walk(tree, order, describe_node, WALK_NODES, trav_type);
    rt_log("--------------------------------------------------\n");
}

/*		R B _ S U M M A R I Z E _ T R E E ( )
 *
 *		    Describe a red-black tree
 *
 *	This function has one parameter: a pointer to a red-black
 *	tree.  Rb_describe() prints out the header information
 *	for the tree.  It is intended for diagnostic purposes.
 */
void rb_summarize_tree (tree)

rb_tree	*tree;

{
    int		i;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");

    rt_log("-------- Red-black tree <%x> summary --------\n", tree);
    rt_log("Description:      '%s'\n", tree -> rbt_description);
    rt_log("Current:          <%x>\n", tree -> rbt_current);
    rt_log("Empty node:       <%x>\n", tree -> rbt_empty_node);
    rt_log("Size (in nodes):  <%x>\n", tree -> rbt_nm_nodes);
    if (tree -> rbt_nm_orders <= 0)
	fputs("No orders\n", stderr);
    else
    {
	rt_log("i    Order[i]     Root[i]       Package[i]    Data[i]\n");
	for (i = 0; i < tree -> rbt_nm_orders; ++i)
	{
	    rt_log("%-3d  <%x>    <%x>    <%x>    <%x>\n",
		    i,
		    rb_order_func(tree, i),
		    rb_root(tree, i),
		    (rb_root(tree, i) == RB_NODE_NULL) ? 0 :
			(rb_root(tree, i) -> rbn_package)[i],
		    (rb_root(tree, i) == RB_NODE_NULL) ? 0 :
			rb_data(rb_root(tree, i), i));
	}
    }
    rt_log("-------------------------------------------------\n");
}
