/*               E U C L I D _ U N F O R M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file euclid_unformat.c
 *
 * Converts from Euclid decoded format to no CR format
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "bu.h"


#define MAX_PTS	500

int
main(int argc, char *argv[])
{
    char str[10000] = {0};
    int ident, face_type, npts, face_no;
    plane_t pl;
    int i;
    float a, b, c, d;
    int old_id=(-1), e;

    printf( "$03" );

    if (argc != 1) {
	if (argc > 1)
	    bu_log("Unexpected paramter [%s]\n", argv[1]);

	bu_log("Usage: %s < inputfile]\n  Primary input lines are as follows:\n    face# #npts face_type e ident a\n", argv[0]);
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    while ( bu_fgets( str, sizeof(str), stdin ) )
    {
	sscanf( str, "%d %d %d %d %d %f", &face_no, &npts, &face_type, &e, &ident, &a );

	if ( ident > 0 )
	    old_id = ident;

	if ( !bu_fgets( str, sizeof(str), stdin ) )
	    break;

	sscanf( str, "%f %f %f %f", &a, &b, &c, &d );
	QSET( pl, a, b, c, d )

	    if ( npts > 2 )
		printf( "%10d%3d%7.0f%5d%5d", old_id, face_type, 0.0, 1, npts );

	for ( i=0; i<npts; i++ )
	{
	    if ( i >= MAX_PTS )
	    {
		fprintf( stderr, "Too many points, MAX is %d\n", MAX_PTS );
		return 1;
	    }
	    if ( !bu_fgets( str, sizeof(str), stdin ) )
	    {
		fprintf( stderr, "Unexpected EOF\n" );
		break;
	    }
	    sscanf( str, "%f %f %f", &a, &b, &c );
	    if ( npts > 2 )
		printf( "%10d%8.1f%8.1f%8.1f", i+1, a, b, c );
	}
	if ( npts > 2 )
	    printf( "         1%15.5f%15.5f%15.5f%15.5f", V4ARGS( pl ) );
    }
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
