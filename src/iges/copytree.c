/*                      C O P Y T R E E . C
 * BRL-CAD
 *
 * Copyright (C) 1990-2005 United States Government as represented by
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
/** @file copytree.c
 *  Authors -
 *	John R. Anderson
 *	Susanne L. Muuss
 *	Earl P. Weaver
 *
 *  Source -
 *	VLD/ASB Building 1065
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 */

/*	This routine copies a tree rooted at "root" by recursion
	the "parent" field of the root of the new tree is filed by
	the "parent" argument	*/

#include "./iges_struct.h"

struct node *Copytree( root , parent )
struct node *root,*parent;
{

	struct node *ptr;

	if( root == NULL )
		return( (struct node *)NULL );


	ptr = (struct node *)bu_malloc( sizeof( struct node ), "Copytree: ptr" );

	*ptr = (*root);
	ptr->parent = parent;

	if( root->left != NULL )
		ptr->left = Copytree( root->left , ptr );

	if( root->right != NULL )
		ptr->right = Copytree( root->right , ptr );

	return( ptr );
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
