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
