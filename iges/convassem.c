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


/*	This routine controls the conversion of IGES solid assemblies
	to BRLCAD groups	*/

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "wdb.h"

/* Linked list to hold needed data for the group members */
struct solid_list
{
	int item;	/* Index into "dir" structure for this group member */
	int matrix;	/* Pointer to transformation entity for this member */
	char *name;	/* BRLCAD name for this member */
	mat_t rot;	/* Pointer to BRLCAD matrix */
	struct solid_list *next;
};

Convassem()
{

	int i,j,comblen,conv=0,totass=0;
	struct solid_list *root,*ptr;
	struct wmember head,*wmem;
	fastf_t *flt;

	printf( "\nConverting solid assembly entities:\n" );

	ptr = NULL;
	root = NULL;
	RT_LIST_INIT( &head.l );

	for( i=0 ; i<totentities ; i++ ) /* loop through all entities */
	{
		if( dir[i]->type != 184 )	/* This is not a solid assembly */
			continue;

		/* Increment count of solid assemblies */
		totass++;

		if( dir[i]->param <= pstart )
		{
			printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
					dir[i]->direct , dir[i]->name );
			continue;
		}
		Readrec( dir[i]->param ); /* read first record into buffer */

		Readint( &j , "" );	/* read entity type */
		if( j != 184 )
		{
			fprintf( stderr , "Incorrect entity type in Parameter section for entity %d\n" , i );
			return;
		}

		Readint( &comblen , "" );	/* read number of members in group */

		/* Read pointers to group members */
		for( j=0 ; j<comblen ; j++ )
		{
			if( ptr == NULL )
			{
				root = (struct solid_list *)malloc( sizeof( struct solid_list ) );
				ptr = root;
			}
			else
			{
				ptr->next = (struct solid_list *)malloc( sizeof( struct solid_list ) );
				ptr = ptr->next;
			}
			ptr->next = NULL;

			/* Read pointer to an object */
			Readint( &ptr->item , "" );
			if( ptr->item < 0 )
				ptr->item = (-ptr->item);

			/* Convert pointer to a "dir" index */
			ptr->item = (ptr->item-1)/2;

			/* Save name of object */
			ptr->name = dir[ptr->item]->name;

			/* increment reference count */
			dir[ptr->item]->referenced++;
		}

		/* Read pointer to transformation matrix for each member */
		ptr = root;
		for( j=0 ; j<comblen ; j++ )
		{
			ptr->matrix = 0;

			/* Read pointer to a transformation */
			Readint( &ptr->matrix , "" );

			if( ptr->matrix < 0 )
				ptr->matrix = (-ptr->matrix);

			/* Convert to a "dir" index */
			if( ptr->matrix )
				ptr->matrix = (ptr->matrix-1)/2;
			else
				ptr->matrix = (-1); /* flag to indicate "none" */

			ptr = ptr->next;
		}


		/* Make the members */
		ptr = root;
		while( ptr != NULL )
		{
			/* copy the members original transformation matrix */
			for( j=0 ; j<16 ; j++ )
				ptr->rot[j] = (*dir[ptr->item]->rot)[j];

			/* Apply any matrix indicated for this group member */
			if( ptr->matrix > (-1) )
				Matmult( ptr->rot , *(dir[ptr->matrix]->rot)  , ptr->rot );

			wmem = mk_addmember( ptr->name , &head );
			wmem->wm_op = operator[Union];
			flt = (fastf_t *)ptr->rot;
			for( j=0 ; j<16 ; j++ )
			{
				wmem->wm_mat[j] = (*flt);
				flt++;
			}
			ptr = ptr->next;
		}

		if( dir[i]->colorp != 0 )
			mk_lcomb( fdout , dir[i]->name , &head , 0 , (char *)0 , (char *)0 , dir[i]->rgb , 0 );
		else
			mk_lcomb( fdout , dir[i]->name , &head , 0 , (char *)0 , (char *)0 , (char *)0 , 0 );

		/* Increment the count of successful conversions */
		conv++;

		/* Free some memory */
		ptr = root;
		while( ptr != NULL )
		{
			free( ptr );
			ptr = ptr->next;
		}
	}
	printf( "Converted %d solid assemblies successfully out of %d total assemblies\n" , conv , totass );
}
