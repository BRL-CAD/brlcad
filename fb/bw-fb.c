/*
 *		B W - F B . C
 *
 * Write a black and white (.bw) image to the framebuffer.
 * From an 8-bit/pixel, pix order file (i.e. Bottom UP, left to right).
 *
 * This allows an offset into both the display and source file.
 * The color planes to be loaded are also selectable.
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

#define	MAXSCAN	(16*1024)	/* Largest input file scan line length */

int	fb_fd;
char	*framebuffer = NULL;	/* Default Framebuffer */

char	ibuf[MAXSCAN];		/* Allow us to see parts of big files */
Pixel	obuf[1024];
int	size, initx, inity;
int	outsize;
int	offx, offy;
int	clear = 0;
int	redflag   = 0;
int	greenflag = 0;
int	blueflag  = 0;

char	*Usage = "usage: bw-fb [-h -i -c -r -g -b] [-o xoff yoff] [width] [xorig] [yorig] < file.bw\n";

main( argc, argv )
int argc; char **argv;
{
	int	x, y, n;
	int	default_size;
	int	inverted = 0;

	default_size = 512;
	offx = offy = 0;

	/* Check for flags */
	while( argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'h':
			default_size = 1024;
			fbsetsize(1024);
			break;
		case 'i':
			inverted++;
			break;
		case 'c':
			clear++;
			break;
		case 'r':
			redflag = 1;
			break;
		case 'g':
			greenflag = 1;
			break;
		case 'b':
			blueflag = 1;
			break;
		case 'o':
			offx = atoi( argv[2] );
			offy = atoi( argv[3] );
			argv += 2;
			argc -= 2;
			break;
		}
		argc--;
		argv++;
	}

	/* If no color planes were selected, load them all */
	if( redflag == 0 && greenflag == 0 && blueflag == 0 )
		redflag = greenflag = blueflag = 1;

	if( argc > 4 || isatty(fileno(stdin)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	size = ( argc > 1 ) ? atoi( argv[1] ) : default_size;
	initx = ( argc > 2 ) ? atoi( argv[2] ) : 0;
	inity = ( argc > 3 ) ? (default_size-1 - atoi( argv[3] )) : default_size-1;

	outsize = (size > default_size) ? default_size : size;

	/* Check the scan line size */
	if( size-offx > MAXSCAN ) {
		fprintf( stderr, "bw-fb: not compiled for files that large.\n" );
		exit( 2 );
	}

	/* Open Display Device */
	if ((fb_fd = fbopen(framebuffer, APPEND)) < 0) {
		perror (framebuffer == NULL ? "$FB_FILE" : framebuffer);
		exit( 3 );
	}

	if( clear ) fbclear();

	if( offy != 0 ) fseek( stdin, offy*size, 1 );
	for( y = inity; y > (inity-outsize); y-- ) {
		if( offx != 0 ) fseek( stdin, offx, 1 );
		n = fread( &ibuf[0], sizeof( char ), size-offx, stdin );
		if( n <= 0 ) exit( 0 );
		/*
		 * If we are not loading all color planes, we have
		 * to do a pre-read.
		 */
		if( redflag == 0 || greenflag == 0 || blueflag == 0 ) {
			if( inverted )
				fbread( initx, 511-y, &obuf[0], outsize );
			else
				fbread( initx, y, &obuf[0], outsize );
		}
		for( x = 0; x < outsize; x++ ) {
			if( redflag )
				obuf[x].red   = ibuf[x];
			if( greenflag )
				obuf[x].green = ibuf[x];
			if( blueflag )
				obuf[x].blue  = ibuf[x];
		}
		if( inverted )
			fbwrite( initx, default_size-1-y, &obuf[0], outsize );
		else
			fbwrite( initx, y, &obuf[0], outsize );
	}
}
