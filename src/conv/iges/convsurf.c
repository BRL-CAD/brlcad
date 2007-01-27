/*                      C O N V S U R F . C
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
/** @file convsurf.c
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
	appropriate routines to convert solid entities to BRLCAD
	equivalents	*/

#include "./iges_struct.h"
#include "./iges_extern.h"

void
Convsurfs()
{

	int i,totsurfs=0,convsurf=0;
	struct face_g_snurb **surfs;
	struct face_g_snurb *srf;

	bu_log( "\n\nConverting NURB entities:\n" );

	/* First count the number of surfaces */
	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 128 )
			totsurfs ++;
	}

	surfs = (struct face_g_snurb **)bu_calloc( totsurfs+1, sizeof( struct face_g_snurb *), "surfs" );

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 128 ) {
			if( spline( i, &srf ) )
				surfs[convsurf++] = srf;
		}
	}

	if( totsurfs )
	{
		if( curr_file->obj_name )
			mk_bspline( fdout , curr_file->obj_name , surfs );
		else
			mk_bspline( fdout , "nurb.s" , surfs );
	}

	bu_log( "Converted %d NURBS successfully out of %d total NURBS\n" , convsurf , totsurfs );
	if( convsurf )
		bu_log( "\tCaution: All NURBS are assumed to be part of the same solid\n" );
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
