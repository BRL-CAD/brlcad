/*                       R B _ D I A G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup rb */
/*@{*/
/** @file rb_diag.c
 *	Diagnostic routines for red-black tree maintenance
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char libbu_rb_diag_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"


static int d_order;	/* Used by describe_node() */

/*		    D E S C R I B E _ N O D E ( )
 *
 *		Print out the contents of a red-black node
 *
 *	This function has two parameters:  the node to describe and
 *	its depth in the tree.  Describe_node() is intended to be
 *	called by bu_rb_diagnose_tree().
 */
static void describe_node (struct bu_rb_node *node, int depth)
{
    bu_rb_tree			*tree;
    struct bu_rb_package	*package;
    void			(*pp)();	/* Pretty print function */

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");
    tree = node -> rbn_tree;
    BU_RB_CKORDER(tree, d_order);

    package = (node -> rbn_package)[d_order];
    pp = tree -> rbt_print;

    bu_log("%*snode <%x>...\n", depth * 2, "", node);
    bu_log("%*s  tree:   <%x>\n", depth * 2, "", node -> rbn_tree);
    bu_log("%*s  parent: <%x>\n", depth * 2, "",
	bu_rb_parent(node, d_order));
    bu_log("%*s  left:   <%x>\n", depth * 2, "",
	bu_rb_left_child(node, d_order));
    bu_log("%*s  right:  <%x>\n", depth * 2, "",
	bu_rb_right_child(node, d_order));
    bu_log("%*s  color:  %s\n", depth * 2, "",
	    (bu_rb_get_color(node, d_order) == BU_RB_RED) ? "RED" :
	    (bu_rb_get_color(node, d_order) == BU_RB_BLACK) ? "BLACK" :
								"Huhh?");
    bu_log("%*s  package: <%x> ", depth * 2, "", package);

    if ((pp != 0) && (package != BU_RB_PKG_NULL))
	(*pp)(package -> rbp_data);
    else
	bu_log("\n");
}

/*		    B U _ R B _ D I A G N O S E _ T R E E ( )
 *
 *	    Produce a diagnostic printout of a red-black tree
 *
 *	This function has three parameters: the root and order of the tree
 *	to print out and the type of traversal (preorder, inorder, or
 *	postorder).
 */
void bu_rb_diagnose_tree (bu_rb_tree *tree, int order, int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    bu_log("-------- Red-black tree <%x> contents --------\n", tree);
    bu_log("Description: '%s'\n", tree -> rbt_description);
    bu_log("Order:       %d of %d\n", order, tree -> rbt_nm_orders);
    bu_log("Current:     <%x>\n", tree -> rbt_current);
    bu_log("Empty node:  <%x>\n", tree -> rbt_empty_node);
    bu_log("Uniqueness:  %d\n", bu_rb_get_uniqueness(tree, order));
    d_order = order;
    _rb_walk(tree, order, describe_node, WALK_NODES, trav_type);
    bu_log("--------------------------------------------------\n");
}

/*		B U _ R B _ S U M M A R I Z E _ T R E E ( )
 *
 *		    Describe a red-black tree
 *
 *	This function has one parameter: a pointer to a red-black
 *	tree.  bu_rb_summarize_tree() prints out the header information
 *	for the tree.  It is intended for diagnostic purposes.
 */
void bu_rb_summarize_tree (bu_rb_tree *tree)
{
    int		i;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    bu_log("-------- Red-black tree <%x> summary --------\n", tree);
    bu_log("Description:      '%s'\n", tree -> rbt_description);
    bu_log("Current:          <%x>\n", tree -> rbt_current);
    bu_log("Empty node:       <%x>\n", tree -> rbt_empty_node);
    bu_log("Size (in nodes):  %d\n", tree -> rbt_nm_nodes);
    bu_log("Number of orders: %d\n", tree -> rbt_nm_orders);
    bu_log("Debug bits:       <%x>\n", tree -> rbt_debug);
    if ((tree -> rbt_nm_orders > 0) && (tree -> rbt_nm_nodes > 0))
    {
	bu_log("i    Order[i]   Uniq[i]  Root[i]      Package[i]     Data[i]\n");
	for (i = 0; i < tree -> rbt_nm_orders; ++i)
	{
	    bu_log("%-3d  <%x>    %c      <%x>    <%x>    <%x>\n",
		    i,
		    bu_rb_order_func(tree, i),
		    bu_rb_get_uniqueness(tree, i) ? 'Y' : 'N',
		    bu_rb_root(tree, i),
		    (bu_rb_root(tree, i) == BU_RB_NODE_NULL) ? 0 :
			(bu_rb_root(tree, i) -> rbn_package)[i],
		    (bu_rb_root(tree, i) == BU_RB_NODE_NULL) ? 0 :
			bu_rb_data(bu_rb_root(tree, i), i));
	}
    }
    bu_log("-------------------------------------------------\n");
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
