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
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

char *file_name;
FILE *infp;

Pixel outline[MAX_LINE];

int inverse = 0;			/* Draw upside-down */
int clear = 0;
int height;				/* input height */
int width;				/* input width */

FBIO *fbp;

char usage[] = "\
Usage: pix-fb [-h -i -c] [-s squaresize] [-H height] [-W width] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hics:H:W:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 's':
			/* square size */
			height = width = atoi(optarg);
			break;
		case 'H':
			height = atoi(optarg);
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'i':
			inverse = 1;
			break;
		case 'c':
			clear = 1;
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
	static int y;

	height = width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		return 1;
	}

	scanbytes = width * 3;

	if( (fbp = fb_open( NULL, height, width )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}
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
			register char *in;
			register Pixel *out;
			register int i;

			if( fread( (char *)scanline, scanbytes, 1, infp ) != 1 )
				break;

			in = scanline;
			out = outline;
			for( i=0; i<width; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				out++;
			}
			fb_write( fbp, 0, y, outline, width );
		}
	}  else  {
		/* Inverse -- top to bottom */
		for( y = height-1; y >= 0; y-- )  {
			register char *in;
			register Pixel *out;
			register int i;

			if( fread( (char *)scanline, scanbytes, 1, infp ) != 1 )
				break;

			in = scanline;
			out = outline;
			for( i=0; i<width; i++ )  {
				out->red = *in++;
				out->green = *in++;
				out->blue = *in++;
				(out++)->spare = 0;
			}
			fb_write( fbp, 0, y, outline, width );
		}
	}
	fb_close( fbp );
	exit(0);
}
