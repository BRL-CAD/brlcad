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

#include "./iges_struct.h"

#define	Union		1
#define	Intersect	2
#define	Subtract	3

Treecheck( root )
struct node *root;
{
	struct node *ptr,*Pop();

	ptr = root;
	while( 1 )
	{
		while( ptr != NULL )
		{
			Push( ptr );
			ptr = ptr->left;
		}
		ptr = Pop();

		if( ptr->op == Intersect || ptr->op == Subtract )
			if( ptr->right->op == Union )	/* Not a legal BRLCAD tree */
				return( 0 );
		if( ptr->parent == NULL )
			return( 1 );

		if( ptr != ptr->parent->right )
			ptr = ptr->parent->right;
		else
			ptr = NULL;

	}
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
