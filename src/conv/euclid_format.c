/*                 E U C L I D _ F O R M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file euclid_format.c
 *
 * Converts from no CR format to Euclid decoded format
 *
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

int
main(void)
{
	point_t *pts = (point_t *)NULL;
	int ident,face_type,npts,face_no;
	plane_t pl;
	int tmp_i;
	float tmp_a;
	float a,b,c,d;
	int old_id=(-1);
	int face_count=0;

	while( scanf( "%d %d %f %d %d" , &ident , &face_type , &tmp_a , &tmp_i , &npts ) != EOF )
	{
		int i;

		if( npts > 0 )
			pts = (point_t *)bu_calloc( npts , sizeof( point_t ) , "pts" );

		for( i=0 ; i<npts ; i++ )
		{
			int j;

			if( scanf( "%d %f %f %f" , &j , &a , &b , &c ) == EOF )
				rt_bomb( "Unexpected EOF\n" );

			if( j != i+1 )
			{
				rt_bomb( "Points out of order\n" );
			}

			VSET( pts[i] , a , b , c );
		}

		if( scanf( "%d %f %f %f %f" , &face_no , &a , &b , &c , &d ) == EOF )
			rt_bomb( "Unexpected EOF\n" );
		VSET( pl , a , b , c );
		pl[3] = d;

		if( ident != old_id )
		{
			if( npts > 2 )
			{
				face_count = 1;
				printf( "%5d%5d%5d    0    %8d      %5.2f               \n" ,
					face_count , npts , face_type , ident , 0.0 );
			}
			old_id = ident;
		}
		else
		{
			if( npts > 2 )
			{
				face_count++;
				printf( "%5d%5d%5d    0                                              \n" ,
					face_count , npts , face_type );
			}
		}

		if( npts > 2 )
		{
			printf( "%11.6f%11.6f%11.6f%13.6f\n" , V4ARGS( pl ) );

			for( i=0 ; i<npts ; i++ )
				printf( "%8f  %8f  %8f  \n" , V3ARGS( pts[i] ) );
		}

		if( npts > 0 )
			bu_free( (char *)pts , "pts" );
	}
	return 0;
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
