/*			R B _ C R E A T E . C
 *
 *		Routines to create a red-black tree
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 */
#ifndef lint
static const char libbu_rb_create_RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "./rb_internals.h"

/*		    B U _ R B _ C R E A T E ( )
 *
 *		    Create a red-black tree
 *
 *	This function has three parameters: a comment describing the
 *	tree to create, the number of linear orders to maintain
 *	simultaneously, and the comparison functions (one per order).
 *	bu_rb_create() returns a pointer to the red-black tree header
 *	record.
 */
bu_rb_tree *bu_rb_create (description, nm_orders, order_funcs)

char	*description;
int	nm_orders;
int	(**order_funcs)();

{
    int		order;
    bu_rb_tree	*tree;

    /*
     *	Allocate memory for the tree
     */
    tree = (bu_rb_tree *) bu_malloc(sizeof(bu_rb_tree), "red-black tree");
    tree -> rbt_root = (struct bu_rb_node **)
		    bu_malloc(nm_orders * sizeof(struct bu_rb_node),
			"red-black roots");
    tree -> rbt_unique = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black uniqueness flags");
    bu_rb_null(tree) = (struct bu_rb_node *)
		    bu_malloc(sizeof(struct bu_rb_node),
				"red-black empty node");
    bu_rb_null(tree) -> rbn_parent = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black parents");
    bu_rb_null(tree) -> rbn_left = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black left children");
    bu_rb_null(tree) -> rbn_right = (struct bu_rb_node **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
			    "red-black right children");
    bu_rb_null(tree) -> rbn_color = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black colors");
    bu_rb_null(tree) -> rbn_size = (int *)
		bu_malloc(nm_orders * sizeof(int),
			    "red-black subtree sizes");
    bu_rb_null(tree) -> rbn_package = (struct bu_rb_package **)
		bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
			    "red-black packages");
    /*
     *	Fill in the tree
     */
    tree -> rbt_magic = BU_RB_TREE_MAGIC;
    tree -> rbt_description = description;
    tree -> rbt_nm_orders = nm_orders;
    tree -> rbt_order = order_funcs;
    tree -> rbt_print = 0;
    bu_rb_uniq_all_off(tree);
    tree -> rbt_debug = 0x0;
    tree -> rbt_current = bu_rb_null(tree);
    for (order = 0; order < nm_orders; ++order)
	bu_rb_root(tree, order) = bu_rb_null(tree);
    BU_LIST_INIT(&(tree -> rbt_nodes.l));
    BU_LIST_INIT(&(tree -> rbt_packages.l));

    /*
     *	Initialize the nil sentinel
     */
    bu_rb_null(tree) -> rbn_magic = BU_RB_NODE_MAGIC;
    bu_rb_null(tree) -> rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
    {
	bu_rb_parent(bu_rb_null(tree), order) = BU_RB_NODE_NULL;
	bu_rb_set_color(bu_rb_null(tree), order, BU_RB_BLACK);
	bu_rb_left_child(bu_rb_null(tree), order) = BU_RB_NODE_NULL;
	bu_rb_right_child(bu_rb_null(tree), order) = BU_RB_NODE_NULL;
	bu_rb_size(bu_rb_null(tree), order) = 0;
	(bu_rb_null(tree) -> rbn_package)[order] = BU_RB_PKG_NULL;
    }

    return (tree);
}

/*		    B U _ R B _ C R E A T E 1 ( )
 *
 *		Create a single-order red-black tree
 *
 *	This function has two parameters: a comment describing the
 *	tree to create and a comparison function.  bu_rb_create1() builds
 *	an array of one function pointer and passes it to bu_rb_create().
 *	bu_rb_create1() returns a pointer to the red-black tree header
 *	record.
 *
 *	N.B. - Since this function allocates storage for the array of
 *	function pointers, in order to avoid memory leaks on freeing
 *	the tree, applications should call bu_rb_free1(), NOT bu_rb_free().
 */
bu_rb_tree *bu_rb_create1 (description, order_func)

char	*description;
int	(*order_func)();

{
    int		(**ofp)();

    ofp = (int (**)())
		bu_malloc(sizeof(int (*)()), "red-black function table");
    *ofp = order_func;
    return (bu_rb_create(description, 1, ofp));
}
