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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"
#include "fb.h"

char *Usage="[-h] [-F framebuffer]\n\
	[-s squareframesize] [-w frame_width] [-n frame_height]\n";

#define USAGE_EXIT(p) { fprintf(stderr, "Usage: %s %s\n", (p), Usage); \
			exit(-1); }

int
main(argc, argv)
char **argv;
{
	register int c;
	register int	x;
	FBIO		*fbp;
	int	xsize, ysize;
	int		 len;
	char	*framebuffer = (char *)NULL;
	register unsigned char *line;
	static RGBpixel white = { 255, 255, 255 };
	static RGBpixel red = { 255, 0, 0 };
	static RGBpixel green = { 0, 255, 0 };
	static RGBpixel blue = { 0, 0, 255 };

	xsize = ysize = 0;
	while ( (c = getopt( argc, argv, "ahF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			xsize = ysize = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
		case 'S':
			/* square file size */
			if ((len=atoi(optarg)) > 0)
				xsize = ysize = len;
			else
				USAGE_EXIT(*argv);

			break;
		case 'w':
		case 'W':
			if ((len=atoi(optarg)) > 0)
				xsize = len;
			else
				USAGE_EXIT(*argv);
			break;
		case 'n':
		case 'N':
			if ((len=atoi(optarg)) > 0)
				ysize = len;
			else
				USAGE_EXIT(*argv);
			break;
		default:	/* '?' */
			USAGE_EXIT(*argv);
			break;
		}
	}

	if( (fbp = fb_open( framebuffer, xsize, ysize )) == FBIO_NULL )
		exit( 1 );

	if (xsize <= 0)
		xsize = fb_getwidth(fbp);
	if (ysize <= 0)
		ysize = fb_getheight(fbp);

	/* malloc buffer for pixel lines */
	len = (xsize > ysize) ? xsize : ysize;
	if( (line = (unsigned char *)malloc(len*sizeof(RGBpixel))) == RGBPIXEL_NULL )  {
		fprintf(stderr, "fbframe:  malloc failure\n");
		return(1);
	}

#define FLOOD(col)	{ for( x=len-1; x >= 0; x-- ) {COPYRGB(&line[3*x], col);} }

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
