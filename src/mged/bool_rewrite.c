/*                  B O O L _ R E W R I T E . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file bool_rewrite.c
 *
 *	Conversion routines to mangle an arbitrary Boolean tree,
 *	leaving it in GIFT-Boolean form.
 *
 *  Author -
 *	Paul Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./comb_bool.h"
#include "./ged.h"


/*
 *	A Boolean expression tree is in GIFT-Boolean form provided
 *	that
 *	    - every internal node is one of the operators
 *	      UNION, INTERSECTION, and DIFFERENCE;
 *	    - any UNION nodes occur consecutively, starting
 *	      at the root and proceeding leftward; and
 *	    - every INTERSECTION and DIFFERENCE node has
 *	      a leaf for a right child.
 *
 *	The conversion algorithm proceeds by performing a breadth-first
 *	traversal of the tree (actually a DAG), converting each internal
 *	node as necessary.  This conversion consists in distributing
 *	intersections and differences across unions, and generally
 *	adjusting the associativity and precedence of subexpressions.
 *	Specifically, the following nine graph-rewrite rules are applied:
 *
 *		1.  a U (b U c)  -->  (a U b) U c
 *		2.  (a U b) * c  -->  (a * c) U (b * c)
 *		3.  a * (b U c)  -->  (a * b) U (a * c)
 *		4.  (a U b) - c  -->  (a - c) U (b - c)
 *		5.  a - (b U c)  -->  (a - b) - c
 *		6.  a * (b * c)  -->  (a * b) * c
 *		7.  a - (b * c)  -->  (a - b) U (a - c)
 *		8.  a * (b - c)  -->  (a * b) - c
 *		9.  a - (b - c)  -->  (a - b) U (a * c)
 */

/*
 *	    F I N D _ B O O L _ T R E E _ R E W R I T E ( )
 *
 *	Find an applicable rewrite for the root of a Boolean tree
 *
 *	This function has one parameter:  a Boolean-tree node.
 *	Find_bool_tree_rewrite() compares the structure of the
 *	subtree rooted at the specified node to the LHS's of
 *	the rewrite rules and returns the number of the first
 *	match it finds.
 */
static int find_bool_tree_rewrite (struct bool_tree_node *rp)
{
    int		rule_nm;	/* An applicable rule */
    int		lop;		/* Left child's operation */
    int		rop;		/* Right   "        "     */
    
    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    BU_CKMAG(bt_opd(rp, BT_LEFT), BOOL_TREE_NODE_MAGIC, "Boolean tree node");
    BU_CKMAG(bt_opd(rp, BT_RIGHT), BOOL_TREE_NODE_MAGIC, "Boolean tree node");
    lop = bt_opn(bt_opd(rp, BT_LEFT));
    rop = bt_opn(bt_opd(rp, BT_RIGHT));
    rule_nm = 0;

    switch (bt_opn(rp))
    {
	case OPN_UNION:
	    if (rop == OPN_UNION)
		rule_nm = 1;
	    break;
	case OPN_INTERSECTION:
	    if (lop == OPN_UNION)
		rule_nm = 2;
	    else
		switch (rop)
		{
		    case OPN_UNION:		rule_nm = 3; break;
		    case OPN_INTERSECTION:	rule_nm = 6; break;
		    case OPN_DIFFERENCE:	rule_nm = 8; break;
		}
	    break;
	case OPN_DIFFERENCE:
	    if (lop == OPN_UNION)
		rule_nm = 4;
	    else
		switch (rop)
		{
		    case OPN_UNION:		rule_nm = 5; break;
		    case OPN_INTERSECTION:	rule_nm = 7; break;
		    case OPN_DIFFERENCE:	rule_nm = 9; break;
		}
	    break;
	default:
	    bu_log("Reached %s:%d.  This shouldn't happen\n",
		    __FILE__, __LINE__);
	    exit (1);
    }

    return (rule_nm);
}

/*
 *		    D U P _ B O O L _ T R E E ( )
 *
 *			Duplicate a Boolean tree
 *
 *	This function has one parameter:  a Boolean-tree node.
 *	Dup_bool_tree() recursively copies the subtree rooted at
 *	the specified node and returns a pointer to the root of
 *	the copy.
 */
static struct bool_tree_node *dup_bool_tree (struct bool_tree_node *rp)
{
    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    if (bt_is_leaf(rp))
	return (rp);
    else
	return (bt_create_internal(bt_opn(rp),
		    dup_bool_tree(bt_opd(rp, BT_LEFT)),
		    dup_bool_tree(bt_opd(rp, BT_RIGHT))));
}

/*
 *		D O _ B O O L _ T R E E _ R E W R I T E ( )
 *
 *	Perform one rewrite step on the root of a Boolean tree
 *
 *	This function has two parameters:  a Boolean-tree node and
 *	a rule number.  Do_bool_tree_rewrite() applies the specified
 *	rewrite rule to the subtree rooted at the specified node.
 */
static void do_bool_tree_rewrite (struct bool_tree_node *rp, int rule_nm)
{
    struct bool_tree_node	*left;		/* Left child of the root */
    struct bool_tree_node	*right;		/* Right  "   "   "   "   */
    struct bool_tree_node	*a, *b, *c;	/* Subtrees unchanged */

    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    left = bt_opd(rp, BT_LEFT);
    right = bt_opd(rp, BT_RIGHT);
    BU_CKMAG(left, BOOL_TREE_NODE_MAGIC, "Boolean tree node");
    BU_CKMAG(right, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    switch (rule_nm)
    {
	case 0:
	    return;
	case 1:		/*  a U (b U c)  :  (a U b) U c  */
	case 5:		/*  a - (b U c)  :  (a - b) - c  */
	case 6:		/*  a * (b * c)  :  (a * b) * c  */
	case 8:		/*  a * (b - c)  :  (a * b) - c  */
	    a = left;
	    b = bt_opd(right, BT_LEFT);
	    c = bt_opd(right, BT_RIGHT);
	    bt_opd(rp, BT_LEFT) = right;
	    bt_opd(bt_opd(rp, BT_LEFT), BT_LEFT) = a;
	    bt_opd(bt_opd(rp, BT_LEFT), BT_RIGHT) = b;
	    bt_opd(rp, BT_RIGHT) = c;
	    bt_opn(bt_opd(rp, BT_LEFT)) = bt_opn(rp);
	    if ((rule_nm == 5) || (rule_nm == 8))
	       bt_opn(rp) = OPN_DIFFERENCE;
	    break;
	case 2:		/*  (a U b) * c  :  (a * c) U (b * c)  */
	case 4:		/*  (a U b) - c  :  (a - c) U (b - c)  */
	    a = bt_opd(left, BT_LEFT);
	    b = bt_opd(left, BT_RIGHT);
	    c = right;
	    bt_opn(left) = bt_opn(rp);
	    bt_opd(left, BT_RIGHT) = dup_bool_tree(c);
	    bt_opn(rp) = OPN_UNION;
	    bt_opd(rp, BT_RIGHT) = bt_create_internal(bt_opn(left), b, c);
	    break;
	case 3:		/*  a * (b U c)  :  (a * b) U (a * c)  */
	case 7:		/*  a - (b * c)  :  (a - b) U (a - c)  */
	case 9:		/*  a - (b - c)  :  (a - b) U (a * c)  */
	    a = left;
	    b = bt_opd(right, BT_LEFT);
	    c = bt_opd(right, BT_RIGHT);
	    bt_opd(rp, BT_LEFT) = bt_create_internal(bt_opn(rp), a, b);
	    bt_opn(rp) = OPN_UNION;
	    bt_opn(right) = (rule_nm == 7) ? OPN_DIFFERENCE
					   : OPN_INTERSECTION;
	    bt_opd(right, BT_LEFT) = dup_bool_tree(a);
	    break;
	default:
	    bu_log("Reached %s:%d.  This shouldn't happen\n",
		    __FILE__, __LINE__);
	    exit (1);
    }
}

/*
 *		C O N V E R T _ O N E _ N O D E ( )
 *
 *	    Successively rewrite the root of a Boolean tree
 *
 *	This function has one parameter:  a Boolean-tree node.
 *	Convert_one_node() iteratively rewrites the subtree rooted
 *	at the specified node until it no longer matches the LHS
 *	of any of the rewrite rules.  It returns the number of
 *	times a rewrite rule was applied.
 */
static int convert_one_node (struct bool_tree_node *rp)
{
    int		lisp = 1;
    int		rule_nm;
    int		nm_rewrites;

    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    for (nm_rewrites = 0; rule_nm = find_bool_tree_rewrite(rp); ++nm_rewrites)
	do_bool_tree_rewrite(rp, rule_nm);
    
    return (nm_rewrites);
}

/*
 *		_ C V T _ T O _ G I F T _ B O O L ( )
 *
 *	    Make one conversion pass through a Boolean tree
 *
 *	This function has one parameter:  a Boolean-tree node.
 *	_cvt_to_gift_bool() recursively rewrites the subtree rooted
 *	at the specified node.  It returns the number of times a
 *	rewrite rule was applied.
 */
static int _cvt_to_gift_bool (struct bool_tree_node *rp)
{
    int		nm_rewrites;

    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

    if (bt_is_leaf(rp))
	return (0);

    nm_rewrites = convert_one_node(rp);
    nm_rewrites += _cvt_to_gift_bool(bt_opd(rp, BT_LEFT));
    nm_rewrites += _cvt_to_gift_bool(bt_opd(rp, BT_RIGHT));

    return (nm_rewrites);
}

/*
 *		    C V T _ T O _ G I F T _ B O O L ( )
 *
 *		Convert a Boolean tree to GIFT-Boolean form.
 *
 *	This function has one parameter:  a Boolean-tree node.
 *	Cvt_to_gift_bool() recursively rewrites the subtree rooted
 *	at the specified node.  It returns the number of times a
 *	rewrite rule was applied.
 */
int cvt_to_gift_bool (struct bool_tree_node *rp)
{
    int		cnr;		/* Cumulative number of rewrites */
    int		nr;		/* Number of rewrites in this pass */

    BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");


    for (cnr = 0; nr = _cvt_to_gift_bool(rp); cnr += nr) {
	;
#if 0
	show_tree (rp, 1);
#endif
    }
    
    return (cnr);
}

/*
 *		    S H O W _ G I F T _ B O O L ( )
 *
 *		    Pretty print a GIFT-Boolean tree.
 *
 *	This function has two parameters:  a Boolean-tree node and
 *	a flag.  Show_gift_bool() prints the expression in GIFT format
 *	by performing an inorder traversal of the subtree rooted at
 *	the specified node.  If the flag is nonzero, it prints a
 *	final newline.
 */
void
show_gift_bool (struct bool_tree_node *rp, int new_line)
{
  BU_CKMAG(rp, BOOL_TREE_NODE_MAGIC, "Boolean tree node");

  if (bt_is_leaf(rp))
    Tcl_AppendResult(interp, bt_leaf_name(rp), (char *)NULL);
  else{
    show_gift_bool(bt_opd(rp, BT_LEFT), 0);
    switch (bt_opn(rp)){
    case OPN_UNION:
      Tcl_AppendResult(interp, " u ", (char *)NULL);
      break;
    case OPN_DIFFERENCE:
      Tcl_AppendResult(interp, " - ", (char *)NULL);
      break;
    case OPN_INTERSECTION:
      Tcl_AppendResult(interp, " + ", (char *)NULL);
      break;
    default:
      {
	struct bu_vls tmp_vls;

	bu_vls_init(&tmp_vls);
	bu_vls_printf(&tmp_vls, "%s:%d: Illegal operation type: %d\n",
		      __FILE__, __LINE__, bt_opn(rp));
	Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	bu_vls_free(&tmp_vls);

	exit(1);
      }
    }

    show_gift_bool(bt_opd(rp, BT_RIGHT), 0);
  }

  if (new_line)
    Tcl_AppendResult(interp, "\n", (char *)NULL);
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
