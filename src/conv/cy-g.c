/*                          C Y - G . C
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
/** @file cy-g.c
 *	This routine converts Cyberware Digitizer Data (laser scan data)
 *	to a single BRL-CAD ARS solid. The data must be in cylindrical scan
 *	format.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <errno.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt/debug.h"

#define LINE_LEN 256

static char *usage="Usage:\n\tcy-g input_laser_scan_file output_brlcad_file.g\n";

int
main(int argc, char **argv)
{
	FILE *infp;
	struct rt_wdb *outfp;
	char line[LINE_LEN];
	char *cptr;
	int rshift=5;
	int nlg=512,nlt=256;
	int x,y;
	fastf_t delta_z=0;
	fastf_t delta_angle;
	fastf_t angle=0.0;
	fastf_t *sins, *coss;
	fastf_t **curves;
	fastf_t *ptr;
	int first_non_zero=30000;
	int last_non_zero=(-1);
	int new_last=0;

	if( argc != 3 )
	{
		bu_log( "%s", usage );
		exit( 1 );
	}

	if( (infp=fopen( argv[1], "r" )) == NULL )
	{
		bu_log( "Cannot open input file (%s)\n", argv[1] );
		bu_log( "%s", usage );
		exit( 1 );
	}

	if( (outfp = wdb_fopen( argv[2] )) == NULL )
	{
		bu_log( "Cannot open output file (%s)\n", argv[2] );
		bu_log( "%s", usage );
		exit( 1 );
	}


	/* read ASCII header section */
	while( 1 )
	{
		if( fgets( line, LINE_LEN, infp ) == NULL )
		{
			bu_log( "Unexpected EOF while loking for data\n" );
			exit( 1 );
		}
		printf( "%s", line );
		if( !strncmp( "DATA", line, 4 ) )
		{
			bu_log( "Found DATA\n" );
			break;
		}
		else if( !strncmp( "SPACE", line, 5 ) )
		{
			if( !strstr( line, "CYLINDRICAL" ) )
			{
				bu_log( "Can only handle cylindrical scans right now!\n" );
				exit( 1 );
			}
		}
		else if( !strncmp( "NLG", line, 3 ) )
		{
			cptr = strchr( line, '=' );
			if( !cptr )
			{
				bu_log( "Error in setting NLG\n" );
				exit( 1 );
			}
			nlg = atoi( ++cptr );
		}
		else if( !strncmp( "NLT", line, 3 ) )
		{
			cptr = strchr( line, '=' );
			if( !cptr )
			{
				bu_log( "Error in setting NLT\n" );
				exit( 1 );
			}
			nlt = atoi( ++cptr );
		}
		else if( !strncmp( "LTINCR", line, 6 ) )
		{
			int tmp;

			cptr = strchr( line, '=' );
			if( !cptr )
			{
				bu_log( "Error in setting LTINCR\n" );
				exit( 1 );
			}
			tmp = atoi( ++cptr );
			delta_z = (fastf_t)(tmp)/1000.0;
		}
		else if( !strncmp( "RSHIFT", line, 6 ) )
		{
			cptr = strchr( line, '=' );
			if( !cptr )
			{
				bu_log( "Error in setting RSHIFT\n" );
				exit( 1 );
			}
			rshift = atoi( ++cptr );
		}
	}

	/* calculate angle between longitudinal measurements */
	delta_angle = bn_twopi/(fastf_t)nlg;

	/* allocate memory to hold vertices */
	curves = (fastf_t **)bu_malloc( (nlt+2)*sizeof( fastf_t ** ), "ars curve pointers" );
	for( y=0 ; y<nlt+2 ; y++ )
		curves[y] = (fastf_t *)bu_calloc( (nlg+1)*3,
			sizeof(fastf_t), "ars curve" );

	/* allocate memory for a table os sines and cosines */
	sins = (fastf_t *)bu_calloc( nlg+1, sizeof( fastf_t ), "sines" );
	coss = (fastf_t *)bu_calloc( nlg+1, sizeof( fastf_t ), "cosines" );

	/* fill in the sines and cosines table */
	for( x=0 ; x<nlg ; x++ )
	{
		angle = delta_angle * (fastf_t)x;
		sins[x] = sin(angle);
		coss[x] = cos(angle);
	}
	sins[nlg] = sins[0];
	coss[nlg] = coss[0];

	/* read the actual data */
	for( x=0 ; x<nlg ; x++ )
	{
		fastf_t z=0.0;

		for( y=0 ; y<nlt ; y++ )
		{
			short r;
			long radius;
			fastf_t rad;

			ptr = &curves[y+1][x*3];

			if( fread( &r, 2, 1, infp ) != 1 )
				bu_bomb( "Unexpected EOF\n" );
			if( r < 0 )
				rad = 0.0;
			else
			{
				if( y < first_non_zero )
					first_non_zero = y;
				radius = (long)(r) << rshift;
				rad = (fastf_t)radius/1000.0;
				if( y > last_non_zero )
					last_non_zero = y;
			}
			*ptr = rad * coss[x];
			*(ptr+1) = rad * sins[x];
			*(ptr+2) = z;
/*			bu_log( "%d %d: %g (%d) (%g %g %g)\n", x, y, rad, r, V3ARGS( ptr ) ); */

			/* duplicate the first point at the end of the curve */
			if( x == 0 )
			{
				ptr = &curves[y+1][nlg*3];
				*ptr = rad * coss[x];
				*(ptr+1) = rad * sins[x];
				*(ptr+2) = z;
			}
			z += delta_z;
		}
	}

	/* finished with input file */
	fclose( infp );

	/* eliminate single vertex spikes on each curve */
	for( y=first_non_zero ; y<=last_non_zero ; y++ )
	{
		int is_zero=1;

		for( x=0 ; x<nlg ; x++ )
		{
			fastf_t *next, *prev;

			ptr = &curves[y][x*3];
			if( x == 0 )
				prev = &curves[y][nlg*3];
			else
				prev = ptr - 3;
			next = ptr + 3;

			if( ptr[0] != 0.0 || ptr[1] != 0.0 )
			{
				if( prev[0] == 0.0 && prev[1] == 0.0 &&
				    next[0] == 0.0 && next[1] == 0.0 )
				{
					ptr[0] = 0.0;
					ptr[1] = 0.0;
				}
				else
					is_zero = 0;
			}
		}
		if( is_zero && first_non_zero == y )
			first_non_zero = y + 1;
		else
			new_last = y;
	}

	last_non_zero = new_last;

	/* write out ARS solid
	 * First curve is all zeros (first_non_zero - 1)
	 * Last curve is all zeros (last_non_zero + 1 )
	 * Number of curves is (last_non_zero - first_non_zero + 2)
	 */
	mk_id( outfp, "Laser Scan" );
	mk_ars( outfp, "laser_scan", last_non_zero - first_non_zero + 2, nlg, &curves[first_non_zero-1] );
	wdb_close( outfp );
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
