/*                     R B _ C R E A T E . C
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

#include <stdio.h>
#include <math.h>
#include "bu.h"
#include "./rb_internals.h"


struct bu_rb_tree *
bu_rb_create(char *description, int nm_orders, int (**order_funcs)())
{
    int order;
    struct bu_rb_tree *tree;

    /*
     * Allocate memory for the tree
     */
    tree = (struct bu_rb_tree *) bu_malloc(sizeof(struct bu_rb_tree), "red-black tree");
    tree->rbt_root = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node),
		  "red-black roots");
    tree->rbt_unique = (char *)
	bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
		  "red-black uniqueness flags");
    rb_null(tree) = (struct bu_rb_node *)
	bu_malloc(sizeof(struct bu_rb_node),
		  "red-black empty node");
    rb_null(tree)->rbn_parent = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black parents");
    rb_null(tree)->rbn_left = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black left children");
    rb_null(tree)->rbn_right = (struct bu_rb_node **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_node *),
		  "red-black right children");
    rb_null(tree)->rbn_color = (char *)
	bu_malloc((size_t) ceil((double) (nm_orders / 8.0)),
		  "red-black colors");
    rb_null(tree)->rbn_size = (int *)
	bu_malloc(nm_orders * sizeof(int),
		  "red-black subtree sizes");
    rb_null(tree)->rbn_package = (struct bu_rb_package **)
	bu_malloc(nm_orders * sizeof(struct bu_rb_package *),
		  "red-black packages");
    /*
     * Fill in the tree
     */
    tree->rbt_magic = BU_RB_TREE_MAGIC;
    tree->rbt_description = description;
    tree->rbt_nm_orders = nm_orders;
    tree->rbt_order = order_funcs;
    tree->rbt_print = 0;
    bu_rb_uniq_all_off(tree);
    tree->rbt_debug = 0x0;
    tree->rbt_current = rb_null(tree);
    for (order = 0; order < nm_orders; ++order)
	rb_root(tree, order) = rb_null(tree);
    BU_LIST_INIT(&(tree->rbt_nodes.l));
    BU_LIST_INIT(&(tree->rbt_packages.l));

    /*
     * Initialize the nil sentinel
     */
    rb_null(tree)->rbn_magic = BU_RB_NODE_MAGIC;
    rb_null(tree)->rbn_tree = tree;
    for (order = 0; order < nm_orders; ++order) {
	rb_parent(rb_null(tree), order) = BU_RB_NODE_NULL;
	rb_set_color(rb_null(tree), order, RB_BLK);
	rb_left_child(rb_null(tree), order) = BU_RB_NODE_NULL;
	rb_right_child(rb_null(tree), order) = BU_RB_NODE_NULL;
	rb_size(rb_null(tree), order) = 0;
	(rb_null(tree)->rbn_package)[order] = BU_RB_PKG_NULL;
    }

    return tree;
}


struct bu_rb_tree *
bu_rb_create1(char *description, int (*order_func) (/* ??? */))
{
    int (**ofp)();

    ofp = (int (**)())
	bu_malloc(sizeof(int (*)()), "red-black function table");
    *ofp = order_func;
    return bu_rb_create(description, 1, ofp);
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
