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

extern int	getopt();
extern char	*optarg;
extern int	optind;

static char	*framebuffer = NULL;
static FBIO	*fbp;
static int	scr_width = 0;		/* use default size */
static int	scr_height = 0;

#define u_char	unsigned char


static char usage[] = "\
Usage: fbclear [-h] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [r g b]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
		case 'S':
			scr_height = scr_width = atoi(optarg);
			break;
		case 'w':
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'n':
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}
	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(2);

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	if( fb_wmap( fbp, COLORMAP_NULL ) == -1 )
		exit(3);

	if( optind+2 >= argc )  {
		fb_clear( fbp, PIXEL_NULL );
	} else {
		static RGBpixel	pixel;
		pixel[RED] = (u_char) atoi( argv[optind+0] );
		pixel[GRN] = (u_char) atoi( argv[optind+1] );
		pixel[BLU] = (u_char) atoi( argv[optind+2] );
		fb_clear( fbp, pixel );
	}
	(void)fb_close( fbp );
	return(0);
}
