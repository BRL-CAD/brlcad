/*			R B _ W A L K . C
 *
 *	    Routines for traversal of red-black trees
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
 *
 *
 *	The function bu_rb_walk() is defined in terms of the function
 *	_rb_walk(), which, in turn, calls any of the six functions
 *
 *		static void prewalknodes()
 *		static void inwalknodes()
 *		static void postwalknodes()
 *		static void prewalkdata()
 *		static void inwalkdata()
 *		static void postwalkdata()
 *
 *	depending on the type of traversal desired and the objects
 *	to be visited (nodes themselves, or merely the data stored
 *	in them).  Each of these last six functions has four parameters:
 *	the root of the tree to traverse, the order on which to do the
 *	walking, the function to apply at each visit, and the current
 *	depth in the tree.
 */
#ifndef lint
static const char libbu_rb_walk_RCSid[] = "@(#) $Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"

/*		        P R E W A L K N O D E S ( )
 *
 *	    Perform a preorder traversal of a red-black tree
 */
static void prewalknodes (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    visit(root, depth);
    prewalknodes (bu_rb_left_child(root, order), order, visit, depth + 1);
    prewalknodes (bu_rb_right_child(root, order), order, visit, depth + 1);
}

/*		        I N W A L K N O D E S ( )
 *
 *	    Perform an inorder traversal of a red-black tree
 */
static void inwalknodes (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    inwalknodes (bu_rb_left_child(root, order), order, visit, depth + 1);
    visit(root, depth);
    inwalknodes (bu_rb_right_child(root, order), order, visit, depth + 1);
}

/*		        P O S T W A L K N O D E S ( )
 *
 *	    Perform a postorder traversal of a red-black tree
 */
static void postwalknodes (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    postwalknodes (bu_rb_left_child(root, order), order, visit, depth + 1);
    postwalknodes (bu_rb_right_child(root, order), order, visit, depth + 1);
    visit(root, depth);
}

/*		        P R E W A L K D A T A ( )
 *
 *	    Perform a preorder traversal of a red-black tree
 */
static void prewalkdata (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    visit(bu_rb_data(root, order), depth);
    prewalkdata (bu_rb_left_child(root, order), order, visit, depth + 1);
    prewalkdata (bu_rb_right_child(root, order), order, visit, depth + 1);
}

/*		        I N W A L K D A T A ( )
 *
 *	    Perform an inorder traversal of a red-black tree
 */
static void inwalkdata (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    inwalkdata (bu_rb_left_child(root, order), order, visit, depth + 1);
    visit(bu_rb_data(root, order), depth);
    inwalkdata (bu_rb_right_child(root, order), order, visit, depth + 1);
}

/*		        P O S T W A L K D A T A ( )
 *
 *	    Perform a postorder traversal of a red-black tree
 */
static void postwalkdata (struct bu_rb_node *root, int order, void (*visit) (/* ??? */), int depth)
{
    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    BU_RB_CKORDER(root -> rbn_tree, order);

    if (root == bu_rb_null(root -> rbn_tree))
	return;
    postwalkdata (bu_rb_left_child(root, order), order, visit, depth + 1);
    postwalkdata (bu_rb_right_child(root, order), order, visit, depth + 1);
    visit(bu_rb_data(root, order), depth);
}

/*		        _ R B _ W A L K ( )
 *
 *		    Traverse a red-black tree
 *
 *	This function has five parameters: the tree to traverse,
 *	the order on which to do the walking, the function to apply
 *	to each node, whether to apply the function to the entire node
 *	(or just to its data), and the type of traversal (preorder,
 *	inorder, or postorder).
 *
 *	N.B. _rb_walk() is not declared static because it is called
 *	by bu_rb_diagnose_tree() in rb_diag.c.
 */
void _rb_walk (bu_rb_tree *tree, int order, void (*visit) (/* ??? */), int what_to_visit, int trav_type)
{
    static void (*walk[][3])() =
		{
		    { prewalknodes, inwalknodes, postwalknodes },
		    { prewalkdata, inwalkdata, postwalkdata }
		};

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);
    switch (trav_type)
    {
	case PREORDER:
	case INORDER:
	case POSTORDER:
	    switch (what_to_visit)
	    {
		case WALK_NODES:
		case WALK_DATA:
		    (*walk[what_to_visit][trav_type])
			(bu_rb_root(tree, order), order, visit, 0);
		    break;
		default:
		    bu_log("FATAL: _rb_walk(): Illegal visitation object: %d\n",
			what_to_visit);
		    exit (1);
	    }
	    break;
	default:
	    bu_log("FATAL: _rb_walk(): Illegal traversal type: %d\n",
		trav_type);
	    exit (1);
    }
}

/*		        B U _ R B _ W A L K ( )
 *
 *		Applications interface to _rb_walk()
 *
 *	This function has four parameters: the tree to traverse,
 *	the order on which to do the walking, the function to apply
 *	to each node, and the type of traversal (preorder, inorder,
 *	or postorder).
 */
void bu_rb_walk (bu_rb_tree *tree, int order, void (*visit) (/* ??? */), int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    _rb_walk(tree, order, visit, WALK_DATA, trav_type);
}
