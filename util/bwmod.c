/*
 *		B W M O D . C
 *
 * Modify intensities in Black and White files.
 *
 * Adds or subtracts an offset and then optionally
 * provides a "gain" around a chosen "zero" level.
 * Keeps track of and reports clipping.
 *
 * Note that this works on PIX files also but there is no
 *  distinction made between colors.
 *
 *  Author -
 *	Phillip Dykstra
 *	31 July 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

double	atof();

static char *Usage = "usage: bwmod offset [gain [\"zero\" (127) [offset]]]\n";

main( argc, argv )
int argc;
char **argv;
{
	int	i, n;
	long	value;
	double	offset, gain, zero, offset2;
	long	clipped_high, clipped_low;
	unsigned char buf[1024], out;

	if( argc < 2 || isatty(fileno(stdin)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	offset = atof( argv[1] );
	gain = (argc > 2) ? atof( argv[2] ) : 0.0;
	zero = (argc > 3) ? atof( argv[3] ) : 127.0;
	offset2 = (argc > 4) ? atof( argv[4] ) : 0.0;

	clipped_high = clipped_low = 0;

	while( (n = fread(buf, sizeof(*buf), 1024, stdin)) > 0 ) {
		for( i = 0; i < n; i++ ) {
			value = offset + buf[i];
			if( gain != 0.0 )
				value = gain * (value-zero) + zero;
			value += offset2;
			if( value > 255.0 ) {
				out = 255;
				clipped_high++;
			} else if( value < 0.0 ) {
				out = 0;
				clipped_low++;
			} else
				out = value;

			fwrite( &out, 1, 1, stdout );
		}
	}

	if( clipped_high != 0 || clipped_low != 0 ) {
		fprintf( stderr, "bwmod: clipped %d high, %d low\n",
			clipped_high, clipped_low );
	}
}
