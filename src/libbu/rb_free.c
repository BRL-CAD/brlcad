/*                       R B _ F R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @addtogroup rb */
/** @{ */
/** @file rb_free.c
 *
 *		Routine to free a red-black tree
 *
 * @author
 *	Paul J. Tanenbaum
 *
 * @par Source -
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/** @} */

#ifndef lint
static const char libbu_rb_free_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "./rb_internals.h"

/**			B U _ R B _ F R E E ( )
 *
 *		      Free a red-black tree
 *
 *	This function has two parameters: the tree to free and a function
 *	to handle the application data.  bu_rb_free() traverses tree's lists
 *	of nodes and packages, freeing each one in turn, and then frees tree
 *	itself.  If free_data is non-NULL, then bu_rb_free() calls it just
 *	before freeing each package , passing it the package's rbp_data
 *	member.  Otherwise, the application data is left untouched.
 */
void bu_rb_free (bu_rb_tree *tree, void (*free_data) (/* ??? */))
{
    struct bu_rb_list		*rblp;
    struct bu_rb_node		*node;
    struct bu_rb_package	*package;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");

    /*
     *	Free all the nodes
     */
    while (BU_LIST_WHILE(rblp, bu_rb_list, &(tree -> rbt_nodes.l)))
    {
	BU_CKMAG(rblp, BU_RB_LIST_MAGIC, "red-black list element");
	bu_rb_free_node(rblp -> rbl_node);
    }

    /*
     *	Free all the packages
     */
    while (BU_LIST_WHILE(rblp, bu_rb_list, &(tree -> rbt_packages.l)))
    {
	BU_CKMAG(rblp, BU_RB_LIST_MAGIC, "red-black list element");
	package = rblp -> rbl_package;
	BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");
	if (free_data)
	    (*free_data)(package -> rbp_data);
	bu_rb_free_package(package);
    }

    /*
     *	Free the tree's NIL sentinel
     */
    node = tree -> rbt_empty_node;
    bu_free((genptr_t) node -> rbn_left, "red-black left children");
    bu_free((genptr_t) node -> rbn_right, "red-black right children");
    bu_free((genptr_t) node -> rbn_parent, "red-black parents");
    bu_free((genptr_t) node -> rbn_color, "red-black colors");
    bu_free((genptr_t) node -> rbn_package, "red-black packages");
    bu_free((genptr_t) node, "red-black empty node");

    /*
     *	Free the tree itself
     */
    bu_free((genptr_t) tree -> rbt_root, "red-black roots");
    bu_free((genptr_t) tree -> rbt_unique, "red-black uniqueness flags");
    bu_free((genptr_t) tree, "red-black tree");
}

/**		    B U _ R B _ F R E E _ N O D E ( )
 *
 *	    Relinquish memory occupied by a red-black node
 *
 *	This function has one parameter: a node to free.  bu_rb_free_node()
 *	frees the memory allocated for the various members of the node
 *	and then frees the memory allocated for the node itself.
 */
void bu_rb_free_node (struct bu_rb_node *node)
{
    bu_rb_tree	*tree;

    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    tree = node -> rbn_tree;
    if (bu_rb_current(tree) == node)
	bu_rb_current(tree) = bu_rb_null(tree);
    BU_CKMAG(node, BU_RB_NODE_MAGIC, "red-black node");

    /*
     *	Remove node from the list of all nodes
     */
    BU_CKMAG(node -> rbn_list_pos, BU_RB_LIST_MAGIC, "red-black list element");
    BU_LIST_DEQUEUE(&(node -> rbn_list_pos -> l));

    bu_free((genptr_t) node -> rbn_parent, "red-black parents");
    bu_free((genptr_t) node -> rbn_left, "red-black left children");
    bu_free((genptr_t) node -> rbn_right, "red-black right children");
    bu_free((genptr_t) node -> rbn_color, "red-black colors");
    bu_free((genptr_t) node -> rbn_package, "red-black packages");
    bu_free((genptr_t) node -> rbn_list_pos, "red-black list element");
    bu_free((genptr_t) node, "red-black node");
}

/**		    B U _ R B _ F R E E _ P A C K A G E ( )
 *
 *	    Relinquish memory occupied by a red-black package
 *
 *	This function has one parameter: a package to free.
 *	bu_rb_free_package() frees the memory allocated to point to the
 *	nodes that contained the package and then frees the memory
 *	allocated for the package itself.
 */
void bu_rb_free_package (struct bu_rb_package *package)
{
    BU_CKMAG(package, BU_RB_PKG_MAGIC, "red-black package");

    /*
     *	Remove node from the list of all packages
     */
    BU_CKMAG(package -> rbp_list_pos, BU_RB_LIST_MAGIC,
	"red-black list element");
    BU_LIST_DEQUEUE(&(package -> rbp_list_pos -> l));

    bu_free((genptr_t) package -> rbp_node, "red-black package nodes");
    bu_free((genptr_t) package -> rbp_list_pos, "red-black list element");
    bu_free((genptr_t) package, "red-black package");
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
