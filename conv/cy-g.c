#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
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

RT_EXTERN( fastf_t nmg_loop_plane_area , (struct loopuse *lu , plane_t pl ) );

static char *usage="Usage:\n";

main( argc, argv )
int argc;
char *argv[];
{
	FILE *infp;
	FILE *outfp;
	char line[LINE_LEN];
	char *cptr;
	int rshift=5;
	int nlg=512,nlt=256;
	int x,y;
	fastf_t delta_z;
	fastf_t delta_angle;
	fastf_t angle=0.0;
	fastf_t *sins, *coss;
	fastf_t **curves;
	fastf_t *ptr;
	int first_non_zero=30000;
	int last_non_zero=(-1);

	if( (infp=fopen( argv[1], "r" )) == NULL )
	{
		bu_log( "Cannot open file %s\n", argv[1] );
		bu_log( "%s", usage );
		exit( 1 );
	}

	while( 1 )
	{
		if( fgets( line, LINE_LEN, infp ) == NULL )
		{
			bu_log( "Unexpected EOF while loking for data\n" );
			exit( 1 );
		}
		printf( "%s\n", line );
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
	delta_angle = bn_twopi/(fastf_t)nlg;

	curves = (fastf_t **)rt_malloc( (nlt+2)*sizeof( fastf_t ** ), "ars curve pointers" );
	for( y=0 ; y<nlt+2 ; y++ )
		curves[y] = (fastf_t *)rt_calloc( (nlg+1)*ELEMENTS_PER_VECT,
			sizeof(fastf_t), "ars curve" );

	sins = (fastf_t *)rt_calloc( nlg+1, sizeof( fastf_t ), "sines" );
	coss = (fastf_t *)rt_calloc( nlg+1, sizeof( fastf_t ), "cosines" );

	for( x=0 ; x<nlg ; x++ )
	{
		angle = delta_angle * (fastf_t)x;
		sins[x] = sin(angle);
		coss[x] = cos(angle);
	}
	sins[nlg] = sins[0];
	coss[nlg] = coss[0];

	for( x=0 ; x<nlg ; x++ )
	{
		fastf_t z=0.0;

		for( y=0 ; y<nlt ; y++ )
		{
			point_t pt;
			short r;
			long radius;
			fastf_t rad;

			ptr = &curves[y+1][x*ELEMENTS_PER_VECT];

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
			bu_log( "%d %d: %g (%d) (%g %g %g)\n", x, y, rad, r, V3ARGS( ptr ) );
			if( x == 0 )
			{
				ptr = &curves[y+1][nlg*ELEMENTS_PER_VECT];
				*ptr = rad * coss[x];
				*(ptr+1) = rad * sins[x];
				*(ptr+2) = z;
			}
			z += delta_z;
		}
	}
	fclose( infp );

	outfp = fopen( "out.g", "w" );

	mk_id( outfp, "Laser Scan" );
	mk_ars( outfp, "laser_scan", last_non_zero - first_non_zero + 2, nlg, &curves[first_non_zero-1] );
	fclose( outfp );
}
