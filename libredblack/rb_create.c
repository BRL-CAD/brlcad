/*			R B _ C R E A T E . C
 *
 *		Routines to create a red-black tree
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
#include "bu.h"
#include "redblack.h"
#include "./rb_internals.h"

/*		    R B _ C R E A T E ( )
 *
 *		    Create a red-black tree
 *
 *	This function has three parameters: a comment describing the
 *	tree to create, the number of linear orders to maintain
 *	simultaneously, and the comparison functions (one per order).
 *	rb_create() returns a pointer to the red-black tree header
 *	record.
 */
rb_tree *rb_create (description, nm_orders, order_funcs)

char	*description;
int	nm_orders;
int	(**order_funcs)();

{
    int		order;
    rb_tree	*tree;

    /*
     *	Allocate memory for the tree
     */
    tree = (rb_tree *) bu_malloc(sizeof(rb_tree), "red-black tree");
    tree -> rbt_root = (struct rb_node **)
		    bu_malloc(nm_orders * sizeof(struct rb_node),
			"red-black roots");
    tree -> rbt_unique = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black uniqueness flags");
    rb_null(tree) = (struct rb_node *)
		    bu_malloc(sizeof(struct rb_node), "red-black empty node");
    rb_null(tree) -> rbn_parent = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black parents");
    rb_null(tree) -> rbn_left = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black left children");
    rb_null(tree) -> rbn_right = (struct rb_node **)
		bu_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black right children");
    rb_null(tree) -> rbn_color = (char *)
		bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black colors");
    rb_null(tree) -> rbn_size = (int *)
		bu_malloc(nm_orders * sizeof(int),
			    "red-black subtree sizes");
    rb_null(tree) -> rbn_package = (struct rb_package **)
		bu_malloc(nm_orders * sizeof(struct rb_package *),
			    "red-black packages");
    /*
     *	Fill in the tree
     */
    tree -> rbt_magic = RB_TREE_MAGIC;
    tree -> rbt_description = description;
    tree -> rbt_nm_orders = nm_orders;
    tree -> rbt_order = order_funcs;
    tree -> rbt_print = 0;
    rb_uniq_all_off(tree);
    tree -> rbt_debug = 0x0;
    tree -> rbt_current = rb_null(tree);
    for (order = 0; order < nm_orders; ++order)
	rb_root(tree, order) = rb_null(tree);
    RT_LIST_INIT(&(tree -> rbt_nodes.l));
    RT_LIST_INIT(&(tree -> rbt_packages.l));

    /*
     *	Initialize the nil sentinel
     */
    rb_null(tree) -> rbn_magic = RB_NODE_MAGIC;
    rb_null(tree) -> rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
    {
	rb_parent(rb_null(tree), order) = RB_NODE_NULL;
	rb_set_color(rb_null(tree), order, RB_BLACK);
	rb_left_child(rb_null(tree), order) = RB_NODE_NULL;
	rb_right_child(rb_null(tree), order) = RB_NODE_NULL;
	rb_size(rb_null(tree), order) = 0;
	(rb_null(tree) -> rbn_package)[order] = RB_PKG_NULL;
    }

    return (tree);
}

/*		    R B _ C R E A T E 1 ( )
 *
 *		Create a single-order red-black tree
 *
 *	This function has two parameters: a comment describing the
 *	tree to create and a comparison function.  rb_create1() builds
 *	an array of one function pointer and passes it to rb_create().
 *	rb_create1() returns a pointer to the red-black tree header
 *	record.
 *
 *	N.B. - Since this function allocates storage for the array of
 *	function pointers, in order to avoid memory leaks on freeing
 *	the tree, applications should call rb_free1(), NOT rb_free().
 */
rb_tree *rb_create1 (description, order_func)

char	*description;
int	(*order_func)();

{
    int		(**ofp)();

    ofp = (int (**)())
		bu_malloc(sizeof(int (*)()), "red-black function table");
    *ofp = order_func;
    return (rb_create(description, 1, ofp));
}
