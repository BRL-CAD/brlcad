/*			R B _ F R E E . C
 *
 *		Routine to free a red-black tree
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

/*			R B _ F R E E ( )
 *
 *		      Free a red-black tree
 *
 *	This function has two parameters: the tree to free and a function
 *	to handle the application data.  rb_free() traverses tree's lists
 *	of nodes and packages, freeing each one in turn, and then frees tree
 *	itself.  If free_data is non-NULL, then rb_free() calls it just
 *	before freeing each package , passing it the package's rbp_data member.
 *	Otherwise, the application data is left untouched.
 */
void rb_free (tree, free_data)

rb_tree	*tree;
void	(*free_data)();

{
    struct rb_list	*rblp;
    struct rb_node	*node;
    struct rb_package	*package;

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");

    /*
     *	Free all the nodes
     */
    while (RT_LIST_WHILE(rblp, rb_list, &(tree -> rbt_nodes.l)))
    {
	RB_CKMAG(rblp, RB_LIST_MAGIC, "red-black list element");
	rb_free_node(rblp -> rbl_node);
    }

    /*
     *	Free all the packages
     */
    while (RT_LIST_WHILE(rblp, rb_list, &(tree -> rbt_packages.l)))
    {
	RB_CKMAG(rblp, RB_LIST_MAGIC, "red-black list element");
	package = rblp -> rbl_package;
	RB_CKMAG(package, RB_PKG_MAGIC, "red-black package");
	if (free_data)
	    (*free_data)(package -> rbp_data);
	rb_free_package(package);
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

/*		    R B _ F R E E _ N O D E ( )
 *
 *	    Relinquish memory occupied by a red-black node
 *
 *	This function has one parameter: a node to free.  rb_free_node()
 *	frees the memory allocated for the various members of the node
 *	and then frees the memory allocated for the node itself.
 */
void rb_free_node (node)

struct rb_node	*node;

{
    rb_tree	*tree;

    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");

    tree = node -> rbn_tree;
    if (rb_current(tree) == node)
	rb_current(tree) = rb_null(tree);
    RB_CKMAG(node, RB_NODE_MAGIC, "red-black node");

    /*
     *	Remove node from the list of all nodes
     */
    RB_CKMAG(node -> rbn_list_pos, RB_LIST_MAGIC, "red-black list element");
    RT_LIST_DEQUEUE(&(node -> rbn_list_pos -> l));

    bu_free((genptr_t) node -> rbn_parent, "red-black parents");
    bu_free((genptr_t) node -> rbn_left, "red-black left children");
    bu_free((genptr_t) node -> rbn_right, "red-black right children");
    bu_free((genptr_t) node -> rbn_color, "red-black colors");
    bu_free((genptr_t) node -> rbn_package, "red-black packages");
    bu_free((genptr_t) node -> rbn_list_pos, "red-black list element");
    bu_free((genptr_t) node, "red-black node");
}

/*		    R B _ F R E E _ P A C K A G E ( )
 *
 *	    Relinquish memory occupied by a red-black package
 *
 *	This function has one parameter: a package to free.
 *	rb_free_package() frees the memory allocated to point to the
 *	nodes that contained the package and then frees the memory
 *	allocated for the package itself.
 */
void rb_free_package (package)

struct rb_package	*package;

{
    RB_CKMAG(package, RB_PKG_MAGIC, "red-black package");

    /*
     *	Remove node from the list of all packages
     */
    RB_CKMAG(package -> rbp_list_pos, RB_LIST_MAGIC, "red-black list element");
    RT_LIST_DEQUEUE(&(package -> rbp_list_pos -> l));

    bu_free((genptr_t) package -> rbp_node, "red-black package nodes");
    bu_free((genptr_t) package -> rbp_list_pos, "red-black list element");
    bu_free((genptr_t) package, "red-black package");
}
