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
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <sys/stat.h>
#include "machine.h"
#include "externs.h"			/* For getopt() */
#include "fb.h"

static unsigned char	*scanline;		/* 1 scanline pixel buffer */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */
static ColorMap	cmap;			/* libfb color map */

char	*framebuffer = NULL;
char	*file_name;
FILE	*outfp;

static int	crunch = 0;		/* Color map crunch? */
static int	inverse = 0;		/* Draw upside-down */
int	screen_height;			/* input height */
int	screen_width;			/* input width */

extern void	cmap_crunch(register RGBpixel (*scan_buf), register int pixel_ct, ColorMap *cmap);

char usage[] = "\
Usage: fb-pix [-h -i -c] [-F framebuffer]\n\
	[-s squaresize] [-w width] [-n height] [file.pix]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "chiF:s:w:n:" )) != EOF )  {
		switch( c )  {
		case 'c':
			crunch = 1;
			break;
		case 'h':
			/* high-res */
			screen_height = screen_width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
			/* square size */
			screen_height = screen_width = atoi(optarg);
			break;
		case 'w':
			screen_width = atoi(optarg);
			break;
		case 'n':
			screen_height = atoi(optarg);
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

int
main(int argc, char **argv)
{
	register FBIO *fbp;
	register int y;

	screen_height = screen_width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	scanpix = screen_width;
	scanbytes = scanpix * sizeof(RGBpixel);
	if( (scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL )  {
		fprintf(stderr,
			"fb-pix:  malloc(%d) failure\n", scanbytes );
		exit(2);
	}

	if( (fbp = fb_open( framebuffer, screen_width, screen_height )) == NULL )
		exit(12);

	if( screen_height > fb_getheight(fbp) )
		screen_height = fb_getheight(fbp);
	if( screen_width > fb_getwidth(fbp) )
		screen_width = fb_getwidth(fbp);

	if( crunch )  {
		if( fb_rmap( fbp, &cmap ) == -1 )  {
			crunch = 0;
		} else if( fb_is_linear_cmap( &cmap ) ) {
			crunch = 0;
		}
	}

	if( !inverse )  {
		/*  Regular -- read bottom to top */
		for( y=0; y < screen_height; y++ )  {
			fb_read( fbp, 0, y, scanline, screen_width );
			if( crunch )
				cmap_crunch( (RGBpixel *)scanline, scanpix, &cmap );
			if( fwrite( (char *)scanline, scanbytes, 1, outfp ) != 1 )  {
				perror("fwrite");
				break;
			}
		}
	}  else  {
		/*  Inverse -- read top to bottom */
		for( y = screen_height-1; y >= 0; y-- )  {
			fb_read( fbp, 0, y, scanline, screen_width );
			if( crunch )
				cmap_crunch( (RGBpixel *)scanline, scanpix, &cmap );
			if( fwrite( (char *)scanline, scanbytes, 1, outfp ) != 1 )  {
				perror("fwrite");
				break;
			}
		}
	}
	fb_close( fbp );
	exit(0);
}
