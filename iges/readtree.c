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

/*		Read and construct a boolean tree		*/

#include "./iges_struct.h"

struct node *Readtree()
{
	int length,i,op;
	struct node *ptr,*Pop();

	Readint( &i , "" );
	if( i != 180 )
	{
		fprintf( stderr , "Expecting a Boolean Tree, found type %d\n" , i );
		return( (struct node *)NULL );
	}

	Readint( &length , "" );
	for( i=0 ; i<length ; i++ )
	{
		Readint( &op , "" );
		if( op < 0 )	/* This is an operand */
		{
			ptr = (struct node *)rt_malloc( sizeof( struct node ),
				"Readtree: ptr" );
			ptr->op = op;
			ptr->left = NULL;
			ptr->right = NULL;
			ptr->parent = NULL;
			Push( ptr );
		}
		else	/* This is an operator */
		{
			ptr = (struct node *)rt_malloc( sizeof( struct node ),
				"Readtree: ptr" );
			ptr->op = op;
			ptr->right = Pop();
			ptr->left = Pop();
			ptr->parent = NULL;
			ptr->left->parent = ptr;
			ptr->right->parent = ptr;
			Push( ptr );
		}
	}

	return( Pop() );
}

