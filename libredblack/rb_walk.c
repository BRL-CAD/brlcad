/*			R B _ W A L K . C
 *
 *	    Routines for traversal of red-black trees
 *
 *	The function rb_walk() is defined in terms of the function
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
#include "redblack.h"
#include "./rb_internals.h"

/*		        P R E W A L K N O D E S ( )
 *
 *	    Perform a preorder traversal of a red-black tree
 */
static void prewalknodes (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    visit(root, depth);
    prewalknodes (rb_left_child(root, order), order, visit, depth + 1);
    prewalknodes (rb_right_child(root, order), order, visit, depth + 1);
}

/*		        I N W A L K N O D E S ( )
 *
 *	    Perform an inorder traversal of a red-black tree
 */
static void inwalknodes (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    inwalknodes (rb_left_child(root, order), order, visit, depth + 1);
    visit(root, depth);
    inwalknodes (rb_right_child(root, order), order, visit, depth + 1);
}

/*		        P O S T W A L K N O D E S ( )
 *
 *	    Perform a postorder traversal of a red-black tree
 */
static void postwalknodes (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    postwalknodes (rb_left_child(root, order), order, visit, depth + 1);
    postwalknodes (rb_right_child(root, order), order, visit, depth + 1);
    visit(root, depth);
}

/*		        P R E W A L K D A T A ( )
 *
 *	    Perform a preorder traversal of a red-black tree
 */
static void prewalkdata (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    visit(rb_data(root, order), depth);
    prewalkdata (rb_left_child(root, order), order, visit, depth + 1);
    prewalkdata (rb_right_child(root, order), order, visit, depth + 1);
}

/*		        I N W A L K D A T A ( )
 *
 *	    Perform an inorder traversal of a red-black tree
 */
static void inwalkdata (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    inwalkdata (rb_left_child(root, order), order, visit, depth + 1);
    visit(rb_data(root, order), depth);
    inwalkdata (rb_right_child(root, order), order, visit, depth + 1);
}

/*		        P O S T W A L K D A T A ( )
 *
 *	    Perform a postorder traversal of a red-black tree
 */
static void postwalkdata (root, order, visit, depth)

struct rb_node	*root;
int		order;
void		(*visit)();
int		depth;

{
    RB_CKMAG(root, RB_NODE_MAGIC, "red-black node");
    RB_CKORDER(root -> rbn_tree, order);

    if (root == rb_null(root -> rbn_tree))
	return;
    postwalkdata (rb_left_child(root, order), order, visit, depth + 1);
    postwalkdata (rb_right_child(root, order), order, visit, depth + 1);
    visit(rb_data(root, order), depth);
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
 */
void _rb_walk (tree, order, visit, what_to_visit, trav_type)

rb_tree	*tree;
int	order;
void	(*visit)();
int	what_to_visit;
int	trav_type;

{
    static void (*walk[][3])() =
		{
		    { prewalknodes, inwalknodes, postwalknodes },
		    { prewalkdata, inwalkdata, postwalkdata }
		};

    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);
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
			(rb_root(tree, order), order, visit, 0);
		    break;
		default:
		    rt_log("FATAL: _rb_walk(): Illegal visitation object: %d\n",
			what_to_visit);
		    exit (1);
	    }
	    break;
	default:
	    rt_log("FATAL: _rb_walk(): Illegal traversal type: %d\n",
		trav_type);
	    exit (1);
    }
}

/*		        R B _ W A L K ( )
 *
 *		Applications interface to _rb_walk()
 *
 *	This function has four parameters: the tree to traverse,
 *	the order on which to do the walking, the function to apply
 *	to each node, and the type of traversal (preorder, inorder,
 *	or postorder).
 */
void rb_walk (tree, order, visit, trav_type)

rb_tree	*tree;
int	order;
void	(*visit)();
int	trav_type;

{
    RB_CKMAG(tree, RB_TREE_MAGIC, "red-black tree");
    RB_CKORDER(tree, order);

    _rb_walk(tree, order, visit, WALK_DATA, trav_type);
}
