/*
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1990-2004 by the United States Army.
 *	All rights reserved.
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
