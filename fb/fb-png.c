/*
 *  			F B - P N G . C
 *  
 *  Program to take a frame buffer image and write a PNG (Portable Network Graphics) format file.
 *  
 *  Author -
 *	John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1998 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "externs.h"			/* For getopt() */
#include "fb.h"
#include "png.h"

static unsigned char	*scanline;	/* scanline pixel buffers */
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

extern void	cmap_crunch();

char usage[] = "\
Usage: fb-png [-h -i -c] [-F framebuffer]\n\
	[-s squaresize] [-w width] [-n height] [file.png]\n";

get_args( argc, argv )
register char **argv;
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
				"fb-png: cannot open \"%s\" for writing\n",
				file_name );
			return(0);
		}
		(void)chmod(file_name, 0444);
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "fb-png: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

main(argc, argv)
int argc;
char **argv;
{
	register FBIO *fbp;
	register int y;
	int i;
	png_structp png_p;
	png_infop info_p;

	screen_height = screen_width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	png_p = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_p )
		bu_bomb( "Could not create PNG write structure\n" );

	info_p = png_create_info_struct( png_p );
	if( !info_p )
		bu_bomb( "Could not create PNG info structure\n" );

	scanpix = screen_width;
	scanbytes = scanpix * sizeof(RGBpixel);
	scanline = (unsigned char *)bu_malloc( scanbytes, "scanline" );

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

	png_init_io( png_p, outfp );
	png_set_filter( png_p, 0, PNG_FILTER_NONE );
	png_set_compression_level( png_p, Z_BEST_COMPRESSION );
	png_set_IHDR( png_p, info_p, screen_width, screen_height, 8,
		PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

	png_set_gAMA( png_p, info_p, 1.0 );

	png_write_info( png_p, info_p );

	if( inverse )
	{
		/*  Regular -- read bottom to top */
		for( y=0; y < screen_height; y++ )
		{
			fb_read( fbp, 0, y, scanline, screen_width );
			if( crunch )
				cmap_crunch( scanline, scanpix, &cmap );
			png_write_row( png_p, scanline );
		}
	}
	else
	{
		/*  Inverse -- read top to bottom */
		for( y = screen_height-1; y >= 0; y-- )
		{
			fb_read( fbp, 0, y, scanline, screen_width );
			if( crunch )
				cmap_crunch( scanline, scanpix, &cmap );
			png_write_row( png_p, scanline );
		}
	}
	fb_close( fbp );
	png_write_end( png_p, NULL );
	exit(0);
}
