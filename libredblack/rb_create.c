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
#include "vmath.h"
#include "raytrace.h"
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
    tree = (rb_tree *) rt_malloc(sizeof(rb_tree), "red-black tree");
    tree -> rbt_root = (struct rb_node **)
		    rt_malloc(nm_orders * sizeof(struct rb_node),
			"red-black roots");
    tree -> rbt_empty_node = (struct rb_node *)
		    rt_malloc(sizeof(struct rb_node), "red-black empty node");
    rb_null(tree) -> rbn_parent = (struct rb_node **)
		rt_malloc(nm_orders * sizeof(struct rb_node *),
			    "red-black parents");
    rb_null(tree) -> rbn_color = (char *)
		rt_malloc((size_t) ceil((double) (nm_orders / 8.0)),
			    "red-black colors");
    /*
     *	Fill in the tree
     */
    tree -> rbt_magic = RB_TREE_MAGIC;
    tree -> rbt_description = description;
    tree -> rbt_nm_orders = nm_orders;
    tree -> rbt_order = order_funcs;
    tree -> rbt_print = 0;
    tree -> rbt_current = tree -> rbt_empty_node;
    for (order = 0; order < nm_orders; ++order)
	rb_root(tree, order) = rb_null(tree);

    /*
     *	Initialize the nil sentinel
     */
    rb_null(tree) -> rbn_magic = RB_NODE_MAGIC;
    rb_null(tree) -> rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order)
    {
	rb_parent(rb_null(tree), order) = RB_NODE_NULL;
	rb_set_color(rb_null(tree), order, RB_BLACK);
    }
    rb_null(tree) -> rbn_left = 0;
    rb_null(tree) -> rbn_right = 0;

    return (tree);
}

/*		    R B _ C R E A T E 1 ( )
 *
 *		Create a single-order red-black tree
 *
 *	This function has two parameters: a comment describing the
 *	tree to create and a comparison function.  Rb_create1() builds
 *	an array of one function pointer and passes it to rb_create().
 *	rb_create1() returns a pointer to the red-black tree header
 *	record.
 */
rb_tree *rb_create1 (description, order_func)

char	*description;
int	(*order_func)();

{
    int		(**ofp)();

    ofp = (int (**)())
		rt_malloc(sizeof(int (*)()), "red-black function table");
    *ofp = order_func;
    return (rb_create(description, 1, ofp));
}
