/*                    R E A D M A T R I X . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file readmatrix.c
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

/* Routine to read a transformation matrix from the IGES file.
	xform is the pointer to the parameter entry for the matrix,
	rot is where the matrix is to be placed. */

#include "./iges_struct.h"
#include "./iges_extern.h"

#define	CR	'\015'

void
Readmatrix( xform , rot )
int xform;
mat_t rot;
{
	int i,j;

	/* read the actual transformation matrix and store */

	Readrec( xform );
	Readint( &i , "" );
	if( i != 124 && i != 700  )
	{
		bu_log( "Error in transformation parameter data at P%d\n" , xform );
		for( j=0 ; j<16 ; j++ )
			rot[j] = (*identity)[j];
		return;
	}
	else if( i == 124 )
	{
		for( i=0 ; i<12 ; i++ )
		{
			if( !((i+1)%4) ) /* convert translation */
				Readcnv( &rot[i] , "" );
			else	/* Don't convert rotations */
				Readflt( &rot[i] , "" );
		}
		for( i=12 ; i<15 ; i++ )
			rot[i] = 0.0;
		rot[15] = 1.0;

	}
	else
	{
		for( i=0 ; i<15 ; i++ )
		{
			if( !((i+1)%4) ) /* convert translation */
				Readcnv( &rot[i] , "" );
			else	/* Don't convert rotations */
				Readflt( &rot[i] , "" );
		}
		Readflt( &rot[15] , "" ); /* Don't convert the scale */
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
