/*
 *  			P I X - F B . C
 *  
 *  Program to take bottom-up pixel files and send them to a framebuffer.
 *  
 *  Author -
 *	Michael John Muuss
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

#define MAX_LINE	2048		/* Max pixels/line */
RGBpixel scanline[MAX_LINE];		/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

char *file_name;
FILE *infp;

int inverse = 0;			/* Draw upside-down */
int clear = 0;
int height;				/* input height */
int width;				/* input width */

char usage[] = "\
Usage: pix-fb [-h -i -c] [-s squaresize] [-W width] [-H height] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hics:W:H:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'c':
			clear = 1;
			break;
		case 's':
			/* square size */
			height = width = atoi(optarg);
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'H':
			height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pix-fb: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register int y;
	register FBIO *fbp;

	height = width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		return 1;
	}

	scanbytes = width * sizeof(RGBpixel);

	if( (fbp = fb_open( NULL, width, height )) == NULL )
		exit(12);

	if( clear )  {
		fb_clear( fbp, PIXEL_NULL );
		fb_wmap( fbp, COLORMAP_NULL );
	}
	/* Zoom in, in the center of view */
	fb_zoom( fbp, fb_getwidth(fbp)/width, fb_getheight(fbp)/height );
	fb_window( fbp, width/2, height/2 );

	if( !inverse )  {
		/* Normal way -- bottom to top */
		for( y=0; y < height; y++ )  {
			if( fread( (char *)scanline, scanbytes, 1, infp ) != 1 )
				break;
			fb_write( fbp, 0, y, scanline, width );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y = height-1; y >= 0; y-- )  {
			if( fread( (char *)scanline, scanbytes, 1, infp ) != 1 )
				break;
			fb_write( fbp, 0, y, scanline, width );
		}
	}
	fb_close( fbp );
	exit(0);
}
