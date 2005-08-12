/*                     R B _ S E A R C H . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup rb */
/*@{*/
/** @file rb_search.c
 *	Routines to search for a node in a red-black tree
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char libbu_rb_search_RCSid[] = "@(#) $Header$";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "rtlist.h"
#include "bu.h"
#include "compat4.h"
#include "./rb_internals.h"

/*		        _ R B _ S E A R C H ( )
 *
 *	   	Search for a node in a red-black tree
 *
 *	This function has four parameters: the root and order of the tree
 *	in which to search, the comparison function, and a data block
 *	containing the desired value of the key.  On success, _rb_search()
 *	returns a pointer to the discovered node.  Otherwise, it returns
 *	(tree -> rbt_empty_node).
 */
static struct bu_rb_node *_rb_search (struct bu_rb_node *root, int order_nm, int (*order) (/* ??? */), void *data)
{
    int		result;
    bu_rb_tree	*tree;

    BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    tree = root -> rbn_tree;
    BU_RB_CKORDER(tree, order_nm);

    while (1)
    {
	if (root == bu_rb_null(root -> rbn_tree))
	    break;
	if ((result = (*order)(data, bu_rb_data(root, order_nm))) == 0)
	    break;
	else if (result < 0)
	    root = bu_rb_left_child(root, order_nm);
	else	/* result > 0 */
	    root = bu_rb_right_child(root, order_nm);
	BU_CKMAG(root, BU_RB_NODE_MAGIC, "red-black node");
    }
    bu_rb_current(tree) = root;
    return (root);
}

/*		        B U _ R B _ S E A R C H ( )
 *
 *	        Applications interface to _rb_search()
 *
 *	This function has three parameters: the tree in which to search,
 *	the order on which to do the searching, and a data block containing
 *	the desired value of the key.  On success, bu_rb_search() returns a
 *	pointer to the data block in the discovered node.  Otherwise,
 *	it returns NULL.
 */
void *bu_rb_search (bu_rb_tree *tree, int order, void *data)
{

    int			(*compare)();
    struct bu_rb_node	*node;

    BU_CKMAG(tree, BU_RB_TREE_MAGIC, "red-black tree");
    BU_RB_CKORDER(tree, order);

    compare = bu_rb_order_func(tree, order);
    node = _rb_search(bu_rb_root(tree, order), order, compare, data);
    if (node == bu_rb_null(tree))
	return (NULL);
    else
	return (bu_rb_data(node, order));
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
