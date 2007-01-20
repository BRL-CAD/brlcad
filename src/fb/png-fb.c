/*                        P N G - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2007 United States Government as represented by
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
/** @file png-fb.c
 *
 *  Program to take PNG (Portable Network Graphics) files and send them to a framebuffer.
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "png.h"
#include "machine.h"
#include "bu.h"
#include "fb.h"

#include "pkg.h"

#ifdef _WIN32
#  include <winsock.h>
#  include <fcntl.h>
#endif

static png_color_16 def_backgrd={ 0,0,0,0,0 };
static unsigned char **scanline;	/* 1 scanline pixel buffer */
static int	scanbytes;		/* # of bytes of scanline */
static int	scanpix;		/* # of pixels of scanline */

static int	multiple_lines = 0;	/* Streamlined operation */

static char	*framebuffer = NULL;
static char	*file_name;
static FILE	*fp_in;

static int	fileinput = 0;		/* file of pipe on input? */

static int	scr_width = 0;		/* screen tracks file if not given */
static int	scr_height = 0;
static int	file_xoff=0, file_yoff=0;
static int	scr_xoff=0, scr_yoff=0;
static int	clear = 0;
static int	zoom = 0;
static int	inverse = 0;		/* Draw upside-down */
static int	one_line_only = 0;	/* insist on 1-line writes */
static int	verbose = 0;
static int	header_only = 0;

static double	def_screen_gamma=1.0;	/* Don't add more gamma, by default */
/* Particularly because on SGI, the system provides gamma correction,
 * so programs like this one don't have to.
 */

static char usage[] = "\
Usage: png-fb [-H -h -i -c -v -z -1] [-m #lines] [-F framebuffer]\n\
	[-g screen_gamma]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [file.png]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "1m:g:HhicvzF:s:x:y:X:Y:S:W:N:" )) != EOF )  {
		switch( c )  {
		case '1':
			one_line_only = 1;
			break;
		case 'm':
			multiple_lines = atoi(bu_optarg);
			break;
		case 'g':
			def_screen_gamma = atof(bu_optarg);
			break;
		case 'H':
			header_only = 1;
			break;
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'c':
			clear = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'z':
			zoom = 1;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'x':
			file_xoff = atoi(bu_optarg);
			break;
		case 'y':
			file_yoff = atoi(bu_optarg);
			break;
		case 'X':
			scr_xoff = atoi(bu_optarg);
			break;
		case 'Y':
			scr_yoff = atoi(bu_optarg);
			break;
		case 'S':
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'N':
			scr_height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		fp_in = stdin;
	} else {
		file_name = argv[bu_optind];
#ifdef _WIN32
		if( (fp_in = fopen(file_name, "rb")) == NULL )  {
#else
		if( (fp_in = fopen(file_name, "r")) == NULL )  {
#endif
			perror(file_name);
			(void)fprintf( stderr,
				"png-fb: cannot open \"%s\" for reading\n",
				file_name );
			exit(1);
		}
		fileinput++;
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "png-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int y;
	register FBIO *fbp;
	int i;
	int	xout, yout, m, xstart;
	png_structp png_p;
	png_infop info_p;
	char header[8];
	int bit_depth;
	int color_type;
	png_color_16p input_backgrd;
	double gamma=1.0;
	int file_width, file_height;
	unsigned char *image;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if (pkg_init() != 0)
	    exit(1);

	if (fread(header, 8, 1, fp_in) != 1) {
		pkg_terminate();
		bu_bomb( "ERROR: Failed while reading file header!!!\n" );
	}

	if (!png_check_sig((png_bytep)header, 8)) {
		pkg_terminate();
		bu_bomb( "This is not a PNG file!!!\n" );
	}

	png_p = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if (!png_p) {
		pkg_terminate();
		bu_bomb( "png_create_read_struct() failed!!\n" );
	}

	info_p = png_create_info_struct( png_p );
	if (!info_p) {
		pkg_terminate();
		bu_bomb( "png_create_info_struct() failed!!\n" );
	}

	png_init_io( png_p, fp_in );

	png_set_sig_bytes( png_p, 8 );

	png_read_info( png_p, info_p );

	color_type = png_get_color_type( png_p, info_p );

	png_set_expand( png_p );
	bit_depth = png_get_bit_depth( png_p, info_p );
	if( bit_depth == 16 )
		png_set_strip_16( png_p );

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_p);

	file_width = png_get_image_width( png_p, info_p );
	file_height = png_get_image_height( png_p, info_p );

	if( verbose )
	{
		switch (color_type)
		{
			case PNG_COLOR_TYPE_GRAY:
				bu_log( "color type: b/w (bit depth=%d)\n", bit_depth );
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
				bu_log( "color type: b/w with alpha channel (bit depth=%d)\n", bit_depth );
				break;
			case PNG_COLOR_TYPE_PALETTE:
				bu_log( "color type: color palette (bit depth=%d)\n", bit_depth );
				break;
			case PNG_COLOR_TYPE_RGB:
				bu_log( "color type: RGB (bit depth=%d)\n", bit_depth );
				break;
			case PNG_COLOR_TYPE_RGB_ALPHA:
				bu_log( "color type: RGB with alpha channel (bit depth=%d)\n", bit_depth );
				break;
			default:
				bu_log( "Unrecognized color type (bit depth=%d)\n", bit_depth );
				break;
		}
		bu_log( "Image size: %d X %d\n", file_width, file_height );
	}

	if( header_only )  {
		fprintf(stdout, "WIDTH=%d HEIGHT=%d\n", file_width, file_height);
		pkg_terminate();
		exit(0);
	}

	if( png_get_bKGD( png_p, info_p, &input_backgrd ) )
	{
		if( verbose && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
				color_type == PNG_COLOR_TYPE_RGB_ALPHA ) )
			bu_log( "background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue );
		png_set_background( png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0 );
	}
	else
		png_set_background( png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0 );

	if( !png_get_gAMA( png_p, info_p, &gamma ) )
		gamma = 0.5;
	png_set_gamma( png_p, def_screen_gamma, gamma );
	if( verbose )
		bu_log( "file gamma: %f, additional screen gamma: %f\n",
			gamma, def_screen_gamma );

	if( verbose )
	{
		if( png_get_interlace_type( png_p, info_p ) == PNG_INTERLACE_NONE )
			bu_log( "not interlaced\n" );
		else
			bu_log( "interlaced\n" );
	}

	png_read_update_info( png_p, info_p );

	/* allocate memory for image */
	image = (unsigned char *)bu_calloc( 1, file_width*file_height*3, "image" );

	/* create rows array */
	scanline = (unsigned char **)bu_calloc( file_height, sizeof( unsigned char *), "scanline" );
	for( i=0 ; i<file_height ; i++ )
		scanline[i] = image+(i*file_width*3);

	png_read_image( png_p, scanline );

	if( verbose )
	{
		png_timep mod_time;
		png_textp text;
		int num_text;

		png_read_end(png_p, info_p );
		if( png_get_text( png_p, info_p, &text, &num_text ) )
		{
			int i;

			for( i=0 ; i<num_text ; i++ )
				bu_log( "%s: %s\n", text[i].key, text[i].text );
		}
		if( png_get_tIME( png_p, info_p, &mod_time ) )
			bu_log( "Last modified: %d/%d/%d %d:%d:%d\n", mod_time->month, mod_time->day,
				mod_time->year, mod_time->hour, mod_time->minute, mod_time->second );
	}

	/* If screen size was not set, track the file size */
	if( scr_width == 0 )
		scr_width = file_width;
	if( scr_height == 0 )
		scr_height = file_height;

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(12);

	/* Get the screen size we were given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/* compute number of pixels to be output to screen */
	if( scr_xoff < 0 )
	{
		xout = scr_width + scr_xoff;
		xstart = 0;
	}
	else
	{
		xout = scr_width - scr_xoff;
		xstart = scr_xoff;
	}

	if( xout < 0 )
		exit(0);			/* off screen */
	if( xout > (file_width-file_xoff) )
		xout = (file_width-file_xoff);
	scanpix = xout;				/* # pixels on scanline */

	if( inverse )
		scr_yoff = (-scr_yoff);

	yout = scr_height - scr_yoff;
	if( yout < 0 )
		exit(0);			/* off screen */
	if( yout > (file_height-file_yoff) )
		yout = (file_height-file_yoff);

	/* Only in the simplest case use multi-line writes */
	if( !one_line_only && multiple_lines > 0 && !inverse && !zoom &&
	    xout == file_width && file_xoff == 0 &&
	    file_width <= scr_width )  {
	    	scanpix *= multiple_lines;
	}

	scanbytes = scanpix * sizeof(RGBpixel);

	if( clear )  {
		fb_clear( fbp, PIXEL_NULL );
	}
	if( zoom ) {
		/* Zoom in, and center the display.  Use square zoom. */
		int	zoom;
		zoom = scr_width/xout;
		if( scr_height/yout < zoom )  zoom = scr_height/yout;
		if( inverse )  {
			fb_view( fbp,
				scr_xoff+xout/2, scr_height-1-(scr_yoff+yout/2),
				zoom, zoom );
		} else {
			fb_view( fbp,
				scr_xoff+xout/2, scr_yoff+yout/2,
				zoom, zoom );
		}
	}

	if( multiple_lines )  {
		/* Bottom to top with multi-line reads & writes */
		int	height=file_height;
		for( y = scr_yoff; y < scr_yoff + yout; y += multiple_lines )  {
			/* Don't over-write */
			if( y + height > scr_yoff + yout )
				height = scr_yoff + yout - y;
			if( height <= 0 )  break;
			m = fb_writerect( fbp, scr_xoff, y,
				file_width, height,
				scanline[file_yoff++] );
			if( m != file_width*height )  {
				fprintf(stderr,
					"png-fb: fb_writerect(x=%d, y=%d, w=%d, h=%d) failure, ret=%d, s/b=%d\n",
					scr_xoff, y,
					file_width, height, m, scanbytes );
			}
		}
	} else if( !inverse )  {
		/* Normal way -- bottom to top */
		int line=file_height-file_yoff-1;
		for( y = scr_yoff; y < scr_yoff + yout; y++ )  {
			m = fb_write( fbp, xstart, y, scanline[line--]+(3*file_xoff), xout );
			if( m != xout )  {
				fprintf(stderr,
					"png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
					scr_xoff, y, xout,
					m, xout );
			}
		}
	}  else  {
		/* Inverse -- top to bottom */
		int line=file_height-file_yoff-1;
		for( y = scr_height-1-scr_yoff; y >= scr_height-scr_yoff-yout; y-- )  {
			m = fb_write( fbp, xstart, y, scanline[line--]+(3*file_xoff), xout );
			if( m != xout )  {
				fprintf(stderr,
					"png-fb: fb_write(x=%d, y=%d, npix=%d) ret=%d, s/b=%d\n",
					scr_xoff, y, xout,
					m, xout );
			}
		}
	}
	if( fb_close( fbp ) < 0 )  {
		fprintf(stderr, "png-fb: Warning: fb_close() error\n");
	}
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
