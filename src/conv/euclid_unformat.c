/*               E U C L I D _ U N F O R M A T . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file euclid_unformat.c
 *
 * Converts from Euclid decoded format to no CR format
 *
 */

#ifndef lint
static const char RCSid[] = "$Header$";
#endif

#include "common.h"



#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

#define MAX_PTS	500

int
main(void)
{
	char str[1024];
	int ident,face_type,npts,face_no;
	plane_t pl;
	int i;
	float a,b,c,d;
	int old_id=(-1),e;

	printf( "$03" );

	while( fgets( str, sizeof(str), stdin ) )
	{
		sscanf( str, "%d %d %d %d %d %f", &face_no, &npts, &face_type, &e, &ident, &a );

		if( ident > 0 )
			old_id = ident;

		if( !fgets( str, sizeof(str), stdin ) )
			break;

		sscanf( str, "%f %f %f %f", &a, &b, &c, &d );
		QSET( pl, a, b, c, d )

		if( npts > 2 )
			printf( "%10d%3d%7.0f%5d%5d", old_id, face_type, 0.0, 1, npts );

		for( i=0 ; i<npts ; i++ )
		{
			if( i >= MAX_PTS )
			{
				fprintf( stderr , "Too many points, MAX is %d\n" , MAX_PTS );
				exit( 1 );
			}
			if( !fgets( str, sizeof(str), stdin ) )
			{
				fprintf( stderr, "Unexpected EOF\n" );
				break;
			}
			sscanf( str, "%f %f %f", &a, &b, &c );
			if( npts > 2 )
				printf( "%10d%8.1f%8.1f%8.1f", i+1, a, b, c );
		}
		if( npts > 2 )
			printf( "         1%15.5f%15.5f%15.5f%15.5f", V4ARGS( pl ) );
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
