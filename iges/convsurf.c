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

/*	This routine loops through all the directory entries and calls
	appropriate routines to convert solid entities to BRLCAD
	equivalents	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

Convsurfs()
{

	int i,totsurfs=0,convsurf=0;

	printf( "\n\nConverting NURB entities:\n" );

	/* First count the number of surfaces */
	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 128 )
			totsurfs ++;
	}

	if( totsurfs )
	{
		/* Make the spline solid header record */
		if( curr_file->obj_name )
			mk_bsolid( fdout , curr_file->obj_name , totsurfs , (double)1.0 );
		else
			mk_bsolid( fdout , "nurb.s" , totsurfs , (double)1.0 );
	}

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 128 )
			convsurf += spline( i );
	}
	printf( "Converted %d NURBS successfully out of %d total NURBS\n" , convsurf , totsurfs );
	if( convsurf )
		printf( "\tCaution: All NURBS are assumed to be part of the same solid\n" );
}
