/*                       R B _ D I A G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu.h"

#include "./rb_internals.h"


static int d_order;	/* Used by describe_node() */


/**
 * _ R B _ D E S C R I B E _ N O D E
 *
 * Print out the contents of a red-black node
 *
 * This function has two parameters: the node to describe and its
 * depth in the tree.  _rb_describe_node() is intended to be called by
 * bu_rb_diagnose_tree().
 */
HIDDEN void
_rb_describe_node(struct bu_rb_node *node, int depth)
{
    struct bu_rb_tree *tree;
    struct bu_rb_package *package;
    void (*pp)(void *);	/* Pretty print function */

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    tree = node->rbn_tree;
    RB_CKORDER(tree, d_order);

    package = (node->rbn_package)[d_order];
    pp = tree->rbt_print;

    bu_log("%*snode <%p>...\n", depth * 2, "", node);
    bu_log("%*s  tree:   <%p>\n", depth * 2, "", node->rbn_tree);
    bu_log("%*s  parent: <%p>\n", depth * 2, "", rb_parent(node, d_order));
    bu_log("%*s  left:   <%p>\n", depth * 2, "", rb_left_child(node, d_order));
    bu_log("%*s  right:  <%p>\n", depth * 2, "", rb_right_child(node, d_order));
    bu_log("%*s  color:  %s\n", depth * 2, "", (rb_get_color(node, d_order) == RB_RED) ? "RED" : (rb_get_color(node, d_order) == RB_BLK) ? "BLACK" : "Huh?");
    bu_log("%*s  package: <%p> ", depth * 2, "", package);

    if ((pp != 0) && (package != BU_RB_PKG_NULL))
	(*pp)(package->rbp_data);
    else
	bu_log("\n");
}


void
bu_rb_diagnose_tree(struct bu_rb_tree *tree, int order, int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    bu_log("-------- Red-black tree <%p> contents --------\n", tree);
    bu_log("Description: '%s'\n", tree->rbt_description);
    bu_log("Order:       %d of %d\n", order, tree->rbt_nm_orders);
    bu_log("Current:     <%p>\n", tree->rbt_current);
    bu_log("Empty node:  <%p>\n", tree->rbt_empty_node);
    bu_log("Uniqueness:  %d\n", rb_get_uniqueness(tree, order));
    d_order = order;
    rb_walk(tree, order, _rb_describe_node, WALK_NODES, trav_type);
    bu_log("--------------------------------------------------\n");
}

void
bu_rb_summarize_tree(struct bu_rb_tree *tree)
{
    int i;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    bu_log("-------- Red-black tree <%p> summary --------\n", tree);
    bu_log("Description:      '%s'\n", tree->rbt_description);
    bu_log("Current:          <%p>\n", tree->rbt_current);
    bu_log("Empty node:       <%p>\n", tree->rbt_empty_node);
    bu_log("Size (in nodes):  %d\n", tree->rbt_nm_nodes);
    bu_log("Number of orders: %d\n", tree->rbt_nm_orders);
    bu_log("Debug bits:       <0x%X>\n", tree->rbt_debug);
    if ((tree->rbt_nm_orders > 0) && (tree->rbt_nm_nodes > 0)) {
	bu_log("i    Order[i]   Uniq[i]  Root[i]      Package[i]     Data[i]\n");
	for (i = 0; i < tree->rbt_nm_orders; ++i) {
	    bu_log("%-3d  <%p>    %c      <%p>    <%p>    <%p>\n",
		   i,
		   rb_order_func(tree, i),
		   rb_get_uniqueness(tree, i) ? 'Y' : 'N',
		   rb_root(tree, i),
		   (rb_root(tree, i) == BU_RB_NODE_NULL) ? 0 : (rb_root(tree, i)->rbn_package)[i],
		   (rb_root(tree, i) == BU_RB_NODE_NULL) ? 0 : rb_data(rb_root(tree, i), i));
	}
    }
    bu_log("-------------------------------------------------\n");
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
