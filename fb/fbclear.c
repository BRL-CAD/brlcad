/*
 *			F B C L E A R . C
 *
 *  This program is intended to be used to clear a frame buffer
 *  to black, or to the specified color
 *
 *  Authors -
 *	Michael John Muuss
 *	Gary S. Moss
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
int	fbsize = 0;		/* Use FB's "default" size */

#define u_char	unsigned char

main(argc, argv)
char	**argv;
int	argc;
{
	if( argc > 1 && strcmp (argv[1], "-h") == 0 )
	{
		fbsize = 1024;
		argv++;
		argc--;
	}
	if( argc != 4 && argc > 1 )
	{
		(void) fprintf( stderr, "Usage:  fbclear [r g b]\n" );
		return	1;
	}
	if(	(fbp = fb_open( NULL, fbsize, fbsize )) == NULL
	    ||	fb_wmap( fbp, COLORMAP_NULL ) == -1
	    )
	{
		return	1;
	}
	if( argc == 4 )  { 
		static RGBpixel	pixel;
		pixel[RED] = (u_char) atoi( argv[1] );
		pixel[GRN] = (u_char) atoi( argv[2] );
		pixel[BLU] = (u_char) atoi( argv[3] );
		fb_clear( fbp, pixel );
	} else {
		fb_clear( fbp, PIXEL_NULL );
	}
	return	fb_close( fbp ) == -1;
}
