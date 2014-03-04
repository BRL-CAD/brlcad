/*                       R B _ W A L K . C
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu/rb.h"
#include "./rb_internals.h"


/**
 * Perform a preorder traversal of a red-black tree
 */
HIDDEN void
prewalknodes(struct bu_rb_node *root,
	     int order,
	     void (*visit)(void),
	     int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    _visit(root, depth);

    prewalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    prewalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform an inorder traversal of a red-black tree
 */
HIDDEN void
inwalknodes(struct bu_rb_node *root,
	    int order,
	    void (*visit)(void),
	    int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    inwalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);

    _visit(root, depth);

    inwalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform a postorder traversal of a red-black tree
 */
HIDDEN void
postwalknodes(struct bu_rb_node *root,
	      int order,
	      void (*visit)(void),
	      int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_NODE_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_NODE_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    postwalknodes(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    postwalknodes(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);

    _visit(root, depth);
}


/**
 * Perform a preorder traversal of a red-black tree
 */
HIDDEN void
prewalkdata(struct bu_rb_node *root,
	    int order,
	    void (*visit)(void),
	    int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;

    _visit(RB_DATA(root, order), depth);

    prewalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    prewalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform an inorder traversal of a red-black tree
 */
HIDDEN void
inwalkdata(struct bu_rb_node *root,
	   int order,
	   void (*visit)(void),
	   int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    inwalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);

    _visit(RB_DATA(root, order), depth);

    inwalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);
}


/**
 * Perform a postorder traversal of a red-black tree
 */
HIDDEN void
postwalkdata(struct bu_rb_node *root,
	     int order,
	     void (*visit)(void),
	     int depth)
{
    /* need function pointer for recasting visit for actual use as a function */
    BU_RB_WALK_FUNC_DATA_DECL(_visit);
    /* make the cast */
    _visit = BU_RB_WALK_FUNC_CAST_AS_DATA_FUNC(visit);

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root->rbn_tree, order);

    if (root == RB_NULL(root->rbn_tree))
	return;
    postwalkdata(RB_LEFT_CHILD(root, order), order, visit, depth + 1);
    postwalkdata(RB_RIGHT_CHILD(root, order), order, visit, depth + 1);

    _visit(RB_DATA(root, order), depth);
}


void
rb_walk(struct bu_rb_tree *tree,
	int order,
	void (*visit)(void),
	int what_to_visit,
	int trav_type)
{
    static void (*walk[][3])(void) = {
	{ BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(prewalknodes),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(inwalknodes),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(postwalknodes)
	},
	{ BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(prewalkdata),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(inwalkdata),
	  BU_RB_WALK_FUNC_CAST_AS_FUNC_ARG(postwalkdata)
	}
    };

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
    switch (trav_type) {
	case BU_RB_WALK_PREORDER:
	case BU_RB_WALK_INORDER:
	case BU_RB_WALK_POSTORDER:
	    switch (what_to_visit) {
		case WALK_NODES:
		case WALK_DATA: {
		    BU_RB_WALK_FUNC_FUNC_DECL(_walk_func);
		    _walk_func = BU_RB_WALK_FUNC_CAST_AS_FUNC_FUNC(walk[what_to_visit][trav_type]);
		    _walk_func(RB_ROOT(tree, order), order, visit, 0);
		}
		    break;
		default:
		    bu_log("ERROR: rb_walk(): Illegal visitation object: %d\n",
			   what_to_visit);
		    bu_bomb("");
	    }
	    break;
	default:
	    bu_log("ERROR: rb_walk(): Illegal traversal type: %d\n",
		   trav_type);
	    bu_bomb("");
    }
}

void
bu_rb_walk(struct bu_rb_tree *tree,
	   int order,
	   void (*visit)(void),
	   int trav_type)
{
    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    rb_walk(tree, order, visit, WALK_DATA, trav_type);
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
