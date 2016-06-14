/*                       R B _ F R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2014 United States Government as represented by
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
#include "bu/rb.h"
#include "./rb_internals.h"


void
bu_rb_free(struct bu_rb_tree *tree, void (*free_data)(void *))
{
    struct bu_rb_list *rblp;
    struct bu_rb_node *node;
    struct bu_rb_package *package;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    /*
     * Free all the nodes
     */
    while (BU_LIST_WHILE(rblp, bu_rb_list, &(tree->rbt_nodes.l))) {
	BU_CKMAG(rblp, BU_RB_LIST_MAGIC, "red-black list element");
	rb_free_node(rblp->rbl_node);
    }

    /*
     * Free all the packages
     */
    while (BU_LIST_WHILE(rblp, bu_rb_list, &(tree->rbt_packages.l))) {
	BU_CKMAG(rblp, BU_RB_LIST_MAGIC, "red-black list element");
	package = rblp->rbl_package;
	BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");
	if (free_data)
	    free_data(package->rbp_data);
	rb_free_package(package);
    }

    /*
     * Free the tree's NIL sentinel
     */
    node = tree->rbt_empty_node;
    bu_free((void *) node->rbn_left, "red-black left children");
    bu_free((void *) node->rbn_right, "red-black right children");
    bu_free((void *) node->rbn_parent, "red-black parents");
    bu_free((void *) node->rbn_color, "red-black colors");
    if (node->rbn_size)
	bu_free((void *) node->rbn_size, "red-black size");
    bu_free((void *) node->rbn_package, "red-black packages");
    bu_free((void *) node, "red-black empty node");

    /*
     * Free the tree itself
     */
    bu_free((void *) tree->rbt_root, "red-black roots");
    bu_free((void *) tree->rbt_unique, "red-black uniqueness flags");
    bu_free((void *) tree, "red-black tree");
}


void
rb_free_node(struct bu_rb_node *node)
{
    struct bu_rb_tree *tree;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    tree = node->rbn_tree;
    if (RB_CURRENT(tree) == node)
	RB_CURRENT(tree) = RB_NULL(tree);
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    /*
     * Remove node from the list of all nodes
     */
    BU_CKMAG(node->rbn_list_pos, BU_RB_LIST_MAGIC, "red-black list element");
    BU_LIST_DEQUEUE(&(node->rbn_list_pos->l));

    bu_free((void *) node->rbn_parent, "red-black parents");
    bu_free((void *) node->rbn_left, "red-black left children");
    bu_free((void *) node->rbn_right, "red-black right children");
    bu_free((void *) node->rbn_color, "red-black colors");
    if (node->rbn_size)
	bu_free((void *) node->rbn_size, "red-black size");
    bu_free((void *) node->rbn_package, "red-black packages");
    bu_free((void *) node->rbn_list_pos, "red-black list element");
    bu_free((void *) node, "red-black node");
}


void
rb_free_package(struct bu_rb_package *package)
{
    BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");

    /*
     * Remove node from the list of all packages
     */
    BU_CKMAG(package->rbp_list_pos, BU_RB_LIST_MAGIC,
	     "red-black list element");
    BU_LIST_DEQUEUE(&(package->rbp_list_pos->l));

    bu_free((void *) package->rbp_node, "red-black package nodes");
    bu_free((void *) package->rbp_list_pos, "red-black list element");
    bu_free((void *) package, "red-black package");
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
