/*
 *  			F B - P I X . C
 *  
 *  Program to take a frame buffer image and write a .pix image.
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
RGBpixel scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

char	*framebuffer = NULL;
char	*file_name;
FILE	*outfp;

int inverse = 0;			/* Draw upside-down */
int height;				/* input height */
int width;				/* input width */

char usage[] = "\
Usage: fb-pix [-h -i] [-F framebuffer]\n\
	[-s squaresize] [-w width] [-n height] [file.pix]\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hics:w:n:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
			/* square size */
			height = width = atoi(optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdout)) )
			return(0);
		file_name = "-";
		outfp = stdout;
	} else {
		file_name = argv[optind];
		if( (outfp = fopen(file_name, "w")) == NULL )  {
			(void)fprintf( stderr,
				"fb-pix: cannot open \"%s\" for writing\n",
				file_name );
			return(0);
		}
		(void)chmod(file_name, 0444);
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "fb-pix: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register FBIO *fbp;
	register int y;

	height = width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	scanbytes = width * sizeof(RGBpixel);

	if( (fbp = fb_open( framebuffer, width, height )) == NULL )
		exit(12);

	if( !inverse )  {
		/*  Regular -- draw bottom to top */
		for( y=0; y < height; y++ )  {
			fb_read( fbp, 0, y, scanline, width );
			if( fwrite( (char *)scanline, scanbytes, 1, outfp ) != 1 )  {
				perror("fwrite");
				break;
			}
		}
	}  else  {
		/*  Inverse -- draw top to bottom */
		for( y = height-1; y >= 0; y-- )  {
			fb_read( fbp, 0, y, scanline, width );
			if( fwrite( (char *)scanline, scanbytes, 1, outfp ) != 1 )  {
				perror("fwrite");
				break;
			}
		}
	}
	fb_close( fbp );
	exit(0);
}
