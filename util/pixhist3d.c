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

FBIO	*fbp;

long	rxb[256][256], rxg[256][256], bxg[256][256];

unsigned char ibuf[8*1024*3];

static char *Usage = "usage: pixhist3d < file.pix\n";

main()
{
	int	n;
	
	if( isatty(fileno(stdin)) ) {
		fprintf( stderr, Usage );
		exit( 1 );
	}

	if( (fbp = fb_open( NULL, 512, 512 )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	while( (n = fread(&ibuf[0], sizeof(*ibuf), sizeof(ibuf), stdin)) > 0 ) {
		register unsigned char *bp;
		register int i;

		bp = &ibuf[0];
		for( i = n/3; i > 0; i--, bp += 3 )  {
			rxb[ bp[0] ][ bp[2] ]++;
			rxg[ bp[0] ][ bp[1] ]++;
			bxg[ bp[2] ][ bp[1] ]++;
		}
	}

	disp_array( rxg, 0, 0 );
	disp_array( rxb, 256, 0 );
	disp_array( bxg, 0, 256 );

	fb_close( fbp );
}

/*
 * Display the array v[Y][X] at screen location xoff, yoff.
 */
disp_array( v, xoff, yoff )
long v[256][256];
int xoff, yoff;
{
	register int	x, y;
	static long	max;
	static double scale;
	RGBpixel	obuf[256];

	/* Find max value */
	max = 0;
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			if( v[y][x] > max )
				max = v[y][x];
		}
	}
	scale = 255.0 / ((double)max);

	/* plot them */
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			register int value;

			value = v[y][x] * scale;
			if( value < 20 && v[y][x] != 0 )
				value = 20;
			obuf[x][RED] = obuf[x][GRN] = obuf[x][BLU] = value;
		}
		fb_write( fbp, xoff, yoff+y, obuf, 256 );
	}
}
