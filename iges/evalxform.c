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
#include "./iges_extern.h"


/*	This routine evaluates the transformation matrix list
	for each transformation matrix entity */

struct list /* simple linked list for remembering a matrix sequence */
{
	int index; /* index to "dir" array for a xform matrix */
	struct list *prev; /* link to previous xform matrix */
};

void
Evalxform()
{

	int i,j,xform;
	struct list *ptr,*ptr1,*ptr_root;
	mat_t rot;


	for( i=0 ; i<totentities ; i++ ) /* loop through all entities */
	{
		/* skip non-transformation entities */
		if( dir[i]->type != 124 && dir[i]->type != 700 )
			continue;

		if( dir[i]->trans >= 0 && !dir[i]->referenced )
		{
			/* Make a linked list of the xform matrices
				in reverse order */
			ptr = NULL;
			ptr1 = NULL;
			ptr_root = NULL;
			xform = i;
			while( xform >= 0 )
			{
				if( ptr == NULL )
					ptr = (struct list *)bu_malloc( sizeof( struct list ),
							"Evalxform: ptr" );
				else
				{
					ptr1 = ptr;
					ptr = (struct list *)bu_malloc( sizeof( struct list ),
							"Evalxform: ptr" );
				}
				ptr->prev = ptr1;
				ptr->index = xform;
				xform = dir[xform]->trans;
			}

			for( j=0 ; j<16 ; j++ )
				rot[j] = (*identity)[j];

			ptr_root = ptr;
			while( ptr != NULL )
			{
				if( !dir[ptr->index]->referenced )
				{
					Matmult( rot , *dir[ptr->index]->rot , rot );
					for( j=0 ; j<16 ; j++ )
						(*dir[ptr->index]->rot)[j] = rot[j];
					dir[ptr->index]->referenced++;
				}
				else
				{
					for( j=0 ; j<16 ; j++ )
					rot[j] = (*dir[ptr->index]->rot)[j];
				}
				ptr = ptr->prev;
			}

			/* Free some memory */
			ptr = ptr_root;
			while( ptr )
			{
				ptr1 = ptr;
				ptr = ptr->prev;
				bu_free( (char *)ptr1, "Evalxform: ptr1" );
			}
		}
	}

	/* set matrices for all other entities */
	for( i=0 ; i<totentities ; i++ )
	{
		/* skip xform entities */
		if( dir[i]->type == 124 || dir[i]->type == 700 )
			continue;

		if( dir[i]->trans >= 0 )
			dir[i]->rot = dir[dir[i]->trans]->rot;
		else
			dir[i]->rot = identity;
	}
}
