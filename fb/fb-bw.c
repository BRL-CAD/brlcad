/*
 *		F B - B W . C
 *
 * Read a Black and White image from the framebuffer and output
 *  it in 8-bit black and white form in pix order,
 *  i.e. Bottom UP, left to right.
 *
 *  Author -
 *	Phillip Dykstra
 *	15 Aug 1985
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

Pixel	inbuf[1024];
unsigned char	obuf[1024];
int	size, initx, inity;

char	Usage[] = "usage: fb-bw [-h -i] [size] [xorig] [yorig] > file.bw\n";

main( argc, argv )
int argc; char **argv;
{
	int	x, y;
	int	default_size;
	int	inverted = 0;

	default_size = 512;

	/* Check for flags */
	while( argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'h':
			default_size = 1024;
			break;
		case 'i':
			inverted++;
			break;
		}
		argc--;
		argv++;
	}

	if( argc > 4 || isatty(fileno(stdout)) ) {
		fputs( Usage, stderr );
		exit( 0 );
	}

	size = ( argc > 1 ) ? atoi( argv[1] ) : default_size;
	initx = ( argc > 2 ) ? atoi( argv[2] ) : 0;
	inity = ( argc > 3 ) ? (default_size-1 - atoi( argv[3] )) : default_size-1;

	/* Open Display Device */
	if ((fbp = fb_open( NULL, default_size, default_size )) == NULL ) {
		fprintf( stderr, "fb_open failed\n");
		exit( 1 );
	}

	if( !inverted ) {
		for( y = inity; y > (inity-size); y-- ) {
			fb_read( fbp, initx, y, &inbuf[0], size );
			for( x = 0; x < size; x++ ) {
				obuf[x] = (inbuf[x].red + inbuf[x].green
					+ inbuf[x].blue) / 3;
			}
			fwrite( &obuf[0], sizeof( char ), size, stdout );
		}
	} else {
		for( y = (inity-size-1); y <= inity; y++ ) {
			fb_read( fbp, initx, y, &inbuf[0], size );
			for( x = 0; x < size; x++ ) {
				obuf[x] = (inbuf[x].red + inbuf[x].green
					+ inbuf[x].blue) / 3;
			}
			fwrite( &obuf[0], sizeof( char ), size, stdout );
		}
	}

	fb_close( fbp );
}
