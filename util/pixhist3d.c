/*
 *		P I X H I S T 3 D . C
 *
 * Display R x G, R x B, B x G on a framebuffer, where the
 *  intensity is the relative frequency for that plane.
 *
 *  Author -
 *	Phillip Dykstra
 *	20 June 1986
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
#include "fb.h"

long	rxb[256][256], rxg[256][256], bxg[256][256];

struct	pix_element {
	unsigned char red, green, blue;
};

static char *Usage = "usage: pixhist3d < file.pix\n";

main()
{
	int	n, x;
	struct	pix_element scan[512];
	
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, Usage );
		exit( 1 );
	}

	while( (n = fread(&scan[0], sizeof(*scan), 512, stdin)) > 0 ) {
		for( x = 0; x < n; x++ ) {
			rxb[ scan[x].red ][ scan[x].blue ]++;
			rxg[ scan[x].red ][ scan[x].green ]++;
			bxg[ scan[x].blue ][ scan[x].green ]++;
		}
	}

	fbopen( 0, 0 );

	disp_array( rxg, 0, 0 );
	disp_array( rxb, 256, 0 );
	disp_array( bxg, 0, 256 );
}

/*
 * Display the array v[Y][X] at screen location xoff, yoff.
 */
disp_array( v, xoff, yoff )
long v[256][256];
int xoff, yoff;
{
	int	x, y, value;
	int	max;
	Pixel	obuf[256];

	/* Find max value */
	max = 0;
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			if( v[y][x] > max )
				max = v[y][x];
		}
	}

	/* plot them */
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			value = v[y][x] / (double)max * 255;
			if( value < 20 && v[y][x] != 0 )
				value = 20;
			obuf[x].red = obuf[x].green = obuf[x].blue = value;
		}
		fbwrite( xoff, 511-(yoff+y), &obuf[0], 256 );
	}
}
