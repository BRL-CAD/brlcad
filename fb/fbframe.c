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


main(argc, argv)
char **argv;
{
	register int	x, y;
	FBIO		*fbp;
	int	xsize, ysize;
	static RGBpixel white = { 255, 255, 255 };
	static RGBpixel red = { 255, 0, 0 };
	static RGBpixel green = { 0, 255, 0 };
	static RGBpixel blue = { 0, 0, 255 };
	int		len;
	register RGBpixel *line;

	if( argc > 1 && strcmp( argv[1], "-h" ) == 0 )
		xsize = ysize = 1024;
	else
		xsize = ysize = 0;
	if( (fbp = fb_open( NULL, xsize, ysize )) == FBIO_NULL )
		exit( 1 );
	xsize = fb_getwidth(fbp);
	ysize = fb_getheight(fbp);
	len = (xsize > ysize) ? xsize : ysize;
	if( (line = (RGBpixel *)malloc(len*sizeof(RGBpixel))) == (RGBpixel *)0 )  {
		fprintf("fbframe:  malloc failure\n");
		return(1);
	}

#define FLOOD(col)	{ for( x=len-1; x >= 0; x-- ) {COPYRGB(line[x], col);} }

	/*
	 * Red:		(   0 -> 510,   0	 )
	 * Green:	(        511,   0 -> 510 )
	 * Blue:	( 511 ->   1, 511	 )
	 * White:	(          0, 511 -> 1	 )
	 */
	FLOOD( red );
	fb_writerect( fbp, 0, 0, xsize-1, 1, line );
	FLOOD( green );
	fb_writerect( fbp, xsize-1, 0, 1, ysize-1, line );
	FLOOD( blue );
	fb_writerect( fbp, 1, ysize-1, xsize-1, 1, line );
	FLOOD( white );
	fb_writerect( fbp, 0, 1, 1, ysize-1, line );

	fb_close( fbp );
	return(0);
}
