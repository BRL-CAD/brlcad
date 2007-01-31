/*                    C O N V S O L I D S . C
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
/** @file convsolids.c
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

/*	This routine loops through all the directory entries and calls
	appropriate routines to convert solid entities to BRL-CAD
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
