/*
 *			F B F R A M E
 *
 *  Overwrite a frame (border) on the current framebuffer.
 *  CCW from the bottom:  Red, Green, Blue, White
 *
 *  Author -
 *	Phil Dykstra
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

static RGBpixel line[1024];

FBIO *fbp;

main(argc, argv)
char **argv;
{
	int	x, y, xsize, ysize;
	static RGBpixel white = { 255, 255, 255 };
	static RGBpixel red = { 255, 0, 0 };
	static RGBpixel green = { 0, 255, 0 };
	static RGBpixel blue = { 0, 0, 255 };

	for( x = 0; x < 512; x++ )
		line[x][RED] = 255;
	for( x = 512; x < 1024; x++ )
		line[x][BLU] = 255;

	if( argc > 1 && strcmp( argv[1], "-h" ) == 0 )
		xsize = ysize = 1024;
	else
		xsize = ysize = 512;
	if( (fbp = fb_open( NULL, xsize, ysize )) == FBIO_NULL )
		exit( 1 );

	/*
	 * Red:		(   0 -> 510,   0	 )
	 * Green:	(        511,   0 -> 510 )
	 * Blue:	( 511 ->   1, 511	 )
	 * White:	(          0, 511 -> 1	 )
	 */
	for( x = 0; x < xsize-1; x++ )
		fb_write( fbp, x, 0, red, 1 );
	for( y = 0; y < ysize-1; y++ )
		fb_write( fbp, xsize-1, y, green, 1 );
	for( x = 1; x < xsize; x++ )
		fb_write( fbp, x, ysize-1, blue, 1 );
	for( y = 1; y < ysize; y++ ) {
		fb_write( fbp, 0, y, white, 1 );
	}
/**	fb_write( fbp, 0, 100, line, 1024 );
	fb_write( fbp, 100, 200, line, 1024 );
	fb_write( fbp, 0, 300, line, 700 );
**/

	fb_close( fbp );
}
