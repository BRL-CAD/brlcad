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

/*	This routine loops through all the directory entries and calls
	appropriate routines to convert solid entities to BRLCAD
	equivalents	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Convsolids()
{

	int i,totsolids=0,conv=0;

	bu_log( "\n\nConverting solid entities:\n" );

	for( i=0 ; i<totentities ; i++ )
	{
		switch( dir[i]->type )
		{
			case 150:
				totsolids++;
				conv += block( i );
				break;
			case 152:
				totsolids++;
				conv += wedge( i );
				break;
			case 154:
				totsolids++;
				conv += cyl( i );
				break;
			case 156:
				totsolids++;
				conv += cone( i );
				break;
			case 158:
				totsolids++;
				conv += sphere( i );
				break;
			case 160:
				totsolids++;
				conv += torus( i );
				break;
			case 162:
				totsolids++;
				conv += revolve( i );
				break;
			case 164:
				totsolids++;
				conv += extrude( i );
				break;
			case 168:
				totsolids++;
				conv += ell( i );
				break;
			case 186:
				totsolids++;
				conv += brep( i );
				break;
		}
	}
	bu_log( "Converted %d solids successfully out of %d total solids\n" , conv , totsolids );
}
