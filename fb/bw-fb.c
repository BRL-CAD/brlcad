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

extern int	getopt();
extern char	*optarg;
extern int	optind;

#define	MAXSCAN	(16*1024)	/* Largest input file scan line length */

FBIO	*fbp;

char	ibuf[MAXSCAN];		/* Allow us to see parts of big files */
Pixel	obuf[MAXSCAN];

int	height;				/* input height */
int	width;				/* input width */
int	scr_xoff, scr_yoff;
int	file_xoff, file_yoff;
int	clear = 0;
int	inverse = 0;
int	redflag   = 0;
int	greenflag = 0;
int	blueflag  = 0;

char *file_name;
FILE *infp;

char	usage[] = "\
Usage: bw-fb [-h -i -c -r -g -b]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-s squaresize] [-W width] [-H height] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hicrgbx:y:X:Y:s:W:H:" )) != EOF )  {
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
		case 'r':
			redflag = 1;
			break;
		case 'g':
			greenflag = 1;
			break;
		case 'b':
			blueflag = 1;
			break;
		case 'x':
			file_xoff = atoi(optarg);
			break;
		case 'y':
			file_yoff = atoi(optarg);
			break;
		case 'X':
			scr_xoff = atoi(optarg);
			break;
		case 'Y':
			scr_yoff = atoi(optarg);
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

main( argc, argv )
int argc; char **argv;
{
	register int	x, y, n;

	height = width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		return 1;
	}

	/* If no color planes were selected, load them all */
	if( redflag == 0 && greenflag == 0 && blueflag == 0 )
		redflag = greenflag = blueflag = 1;

	/* Check the scan line size */
	if( width-file_xoff > MAXSCAN ) {
		fprintf( stderr, "bw-fb: not compiled for files that wide.\n" );
		exit( 2 );
	}

	/* Open Display Device */
	if ((fbp = fb_open( NULL, width, height )) == NULL ) {
		fprintf( stderr, "fb_open failed\n");
		exit( 3 );
	}

	if( clear ) fb_clear(fbp, PIXEL_NULL);

	if( file_yoff != 0 ) fseek( infp, file_yoff*width*height, 1 );

	for( y = scr_yoff; y < height; y++ )  {
		if( file_xoff != 0 )
			fseek( infp, file_xoff, 1 );
		n = fread( &ibuf[0], sizeof( char ), width-file_xoff, infp );
		if( n <= 0 ) break;
		/*
		 * If we are not loading all color planes, we have
		 * to do a pre-read.
		 */
		if( redflag == 0 || greenflag == 0 || blueflag == 0 ) {
			if( inverse )
				n = fb_read( fbp, scr_xoff, fb_getheight(fbp)-1-y,
					&obuf[0], width );
			else
				n = fb_read( fbp, scr_xoff, y, &obuf[0], width );
			if( n < 0 )  break;
		}
		for( x = 0; x < width; x++ ) {
			if( redflag )
				obuf[x].red   = ibuf[x];
			if( greenflag )
				obuf[x].green = ibuf[x];
			if( blueflag )
				obuf[x].blue  = ibuf[x];
		}
		if( inverse )
			fb_write( fbp, scr_xoff, fb_getheight(fbp)-1-y,
				&obuf[0], width );
		else
			fb_write( fbp, scr_xoff, y, &obuf[0], width );
	}

	fb_close( fbp );
}
