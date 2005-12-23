/*                        F B - P N G . C
 * BRL-CAD
 *
 * Copyright (C) 1998-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fb-png.c
 *
 *  Program to take a frame buffer image and write a PNG (Portable Network Graphics) format file.
 *
 *  Authors -
 *	John R. Anderson
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <sys/stat.h>

#include "png.h"
#include "machine.h"
#include "bu.h"
#include "fb.h"

#include "pkg.h"

#ifdef _WIN32
#  include <winsock.h>
#  include <fcntl.h>
#endif

static unsigned char	*scanline;	/* scanline pixel buffers */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */
static ColorMap	cmap;			/* libfb color map */

char	*framebuffer = NULL;
char	*file_name;
FILE	*outfp;

static int	crunch = 0;		/* Color map crunch? */
static int	inverse = 0;		/* Draw upside-down */
static int	pixbytes = 3;		/* Default is 3 bytes/pixel */
int	screen_height;			/* input height */
int	screen_width;			/* input width */

double	out_gamma = 1.0;		/* Gamma the image was created at */

extern void	cmap_crunch(register RGBpixel (*scan_buf), register int pixel_ct, ColorMap *cmap);

char usage[] = "\
Usage: fb-png [-h -i -c] [-# nbytes/pixel] [-F framebuffer] [-g gamma]\n\
	[-s squaresize] [-w width] [-n height] [file.png]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "chiF:s:w:n:g:#:" )) != EOF )  {
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
			framebuffer = bu_optarg;
			break;
		case 's':
			/* square size */
			screen_height = screen_width = atoi(bu_optarg);
			break;
		case 'w':
			screen_width = atoi(bu_optarg);
			break;
		case 'n':
			screen_height = atoi(bu_optarg);
			break;
		case 'g':
			out_gamma = atof(bu_optarg);
			break;
		case '#':
			pixbytes = atoi(bu_optarg);
			if( pixbytes != 1 && pixbytes != 3 )
				bu_bomb("fb-png: Only able to handle 1 and 3 byte pixels\n");
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		if( isatty(fileno(stdout)) )
			return(0);
		file_name = "-";
		outfp = stdout;
	} else {
		file_name = argv[bu_optind];
#ifdef _WIN32
		if( (outfp = fopen(file_name, "wb")) == NULL )  {
#else
		if( (outfp = fopen(file_name, "w")) == NULL )  {
#endif
			(void)fprintf( stderr,
				"fb-png: cannot open \"%s\" for writing\n",
				file_name );
			return(0);
		}
		(void)chmod(file_name, 0444);
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "fb-png: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register FBIO *fbp;
	register int y;
	int got;
	png_structp png_p;
	png_infop info_p;

	screen_height = screen_width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if (pkg_init() != 0)
	    exit(1);

	png_p = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if (!png_p) {
		pkg_terminate();
		bu_bomb( "Could not create PNG write structure\n" );
	}

	info_p = png_create_info_struct( png_p );
	if (!info_p) {
		pkg_terminate();
		bu_bomb( "Could not create PNG info structure\n" );
	}

	if ((fbp = fb_open(framebuffer, screen_width, screen_height)) == NULL) {
		pkg_terminate();
		exit(12);
	}

	/* If actual screen is smaller than requested size, trim down */
	if( screen_height > fb_getheight(fbp) )
		screen_height = fb_getheight(fbp);
	if( screen_width > fb_getwidth(fbp) )
		screen_width = fb_getwidth(fbp);

	scanpix = screen_width;
	scanbytes = scanpix * sizeof(RGBpixel);
	scanline = (unsigned char *)bu_malloc( scanbytes, "scanline" );

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
	png_set_IHDR( png_p, info_p,
		screen_width, screen_height, 8,
		pixbytes == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT );

	png_set_gAMA( png_p, info_p, out_gamma );

	png_write_info( png_p, info_p );

	if( inverse )
	{
		/*  Read bottom to top */
		for( y=0; y < screen_height; y++ )
		{
			if( pixbytes == 3 )
				got = fb_read( fbp, 0, y,
					scanline, screen_width );
			else
				got = fb_bwreadrect( fbp, 0, y,
					screen_width, 1,
					scanline );

			if( got != screen_width )  {
				fprintf(stderr,"fb-png: Read of scanline %d returned %d, expected %d, aborting.\n",
					y, got, screen_width);
				break;
			}
			if( crunch )
				cmap_crunch( (RGBpixel *)scanline, scanpix, &cmap );
			png_write_row( png_p, scanline );
		}
	}
	else
	{
		/*  Read top to bottom */
		for( y = screen_height-1; y >= 0; y-- )
		{
			if( pixbytes == 3 )
				got = fb_read( fbp, 0, y,
					scanline, screen_width );
			else
				got = fb_bwreadrect( fbp, 0, y,
					screen_width, 1,
					scanline );

			if( got != screen_width )  {
				fprintf(stderr,"fb-png: Read of scanline %d returned %d, expected %d, aborting.\n",
					y, got, screen_width);
				break;
			}
			if( crunch )
				cmap_crunch( (RGBpixel *)scanline, scanpix, &cmap );
			png_write_row( png_p, scanline );
		}
	}
	fb_close( fbp );
	png_write_end( png_p, NULL );
	pkg_terminate();
	exit(0);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
