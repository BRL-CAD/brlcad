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
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */

#include "./iges_struct.h"

void
Freetree( root )
struct node *root;
{
	struct node *ptr;

	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Push( (union tree *)ptr );
			ptr = ptr->left;
		}
		ptr = (struct node *)Pop();
		bu_free( (char *)ptr, "Freetree: ptr" );

		if( ptr->parent == NULL )
			return;

		if( ptr != ptr->parent->right )
			ptr = ptr->parent->right;
		else
			ptr = NULL;

	}
}
