/*
 *  			C B A R S - F B . C
 *  
 *  Program to make vertical color scales arranged horizontally on screen.
 *  The lower portion of the screen will contain:
 *	black, white, dark gray (31%), black
 *  The upper portion of the screen will contain:
 *	75% gray, yellow, cyan, green, magenta, red, blue
 *
 *  The reason for this program running directly to the framebuffer, rather
 *  than generating a pix(5) format file (like pixbackgnd(1)) is twofold:
 *	1)  Some framebuffers may have irregular shapes (like the SGI
 *	    3030 is "hi-res" mode.
 *	2)  This program will most often be used to place an image on
 *	    a framebuffer;  other processing of this image is likely
 *	    to be a rare event.
 *  Thus, this bundled version is more efficient, easier to use,
 *  and more likely to have the desired effect.
 *  
 *  Author -
 *	Michael John Muuss
 *
 *  Inspired by "ikcolorbars" by Mike Pique, University of North Carolina.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
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

#define MAX_LINE	2048		/* Max pixels/line */
static RGBpixel scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

static char	*framebuffer = NULL;

static int	scr_width = 0;
static int	scr_height = 0;

static  RGBpixel toppart[7] = {
	/* gray */ 	{190, 190, 190},
	/* yel */	{255, 255, 0},
	/* cyan */	{0, 255, 255},
	/* green */	{0, 255, 0},
	/* magenta */	{255, 0, 255},
	/* red */	{255, 0, 0},
	/* blue */	{0, 0, 255}
};

static RGBpixel botpart[5] = {
	/* black */ {0, 0, 0},
	/* white */ {255, 255, 255},
	/* dark gray */ {80, 80, 80},
	/* black */ {0, 0, 0},
	/* black */ {0, 0, 0}
};

static char usage[] = "\
Usage: cbars-fb [-h] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hF:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'S':
			scr_height = scr_width = atoi(optarg);
			break;
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "cbars-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register int x, y;
	register FBIO *fbp;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(12);

	/* Get the screen size we were actually given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/*
	 *  Operation is bottom-to-top.
	 *  Build bottom line, and send it for 1/4th of the screen,
	 *  then build the top line, and fill the rest of the screen.
	 */
	for( x=0; x<scr_width; x++) {
		/* build bottom part */
		COPYRGB( scanline[x], botpart[x*5/scr_width] );
	}
	for( y=0; y<(scr_height/4); y++)
		fb_write( fbp, 0, y, scanline, scr_width );

	for( x=0; x<scr_width; x++)  {
		/* build top line */
		COPYRGB( scanline[x], toppart[x*7/scr_width] );
	}
	for( ; y<scr_height; y++)
		fb_write( fbp, 0, y, scanline, scr_width );
	fb_close(fbp);
}
