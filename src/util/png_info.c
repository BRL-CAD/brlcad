/*                      P N G _ I N F O . C
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
 */
/** @file png_info.c
 *	Display info about a PNG (Portable Network Graphics) format file
 *
 *	Author -
 *		John R. Anderson
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
#include <math.h>
#include "png.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "zlib.h"

static char *usage="Usage:\n\t%s png__file\n";

int
main(int argc, char **argv)
{
	int i;
	FILE *fp_in = NULL;
	png_structp png_p;
	png_infop info_p;
	char header[8];
	int bit_depth;
	int color_type;
	png_color_16p input_backgrd;
	double gamma;
	int file_width, file_height;
	png_int_32 xoff, yoff;
	png_uint_32 xres, yres;
	int unit_type;
	int rgb_intent;
	double white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y;
	png_timep mod_time;
	png_textp text;
	int num_text;
	unsigned char *image;
	unsigned char **rows;

	if( argc != 2 )
	{
		bu_log( usage, argv[0] );
		bu_bomb( "Incorrect numer of arguments!!\n" );
	} else {
		if( (fp_in = fopen(argv[1], "rb")) == NULL )  {
			perror(argv[1]);
			bu_log(	"png_onfo: cannot open \"%s\" for reading\n",
				argv[1] );
			bu_bomb( "Cannot open input file\n" );
		}
	}

	if( fread( header, 8, 1, fp_in ) != 1 )
		bu_bomb( "ERROR: Failed while reading file header!!!\n" );

	if( !png_check_sig( (png_bytep)header, 8 ) )
		bu_bomb( "This is not a PNG file!!!\n" );

	png_p = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( !png_p )
		bu_bomb( "png_create_read_struct() failed!!\n" );

	info_p = png_create_info_struct( png_p );
	if( !info_p )
		bu_bomb( "png_create_info_struct() failed!!\n" );

	png_init_io( png_p, fp_in );

	png_set_sig_bytes( png_p, 8 );

	png_read_info( png_p, info_p );

	color_type = png_get_color_type( png_p, info_p );

	bit_depth = png_get_bit_depth( png_p, info_p );

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

	file_width = png_get_image_width( png_p, info_p );
	file_height = png_get_image_height( png_p, info_p );

	bu_log( "Image size: %d X %d\n", file_width, file_height );

	/* allocate memory for image */
	image = (unsigned char *)bu_calloc( 1, file_width*file_height*3, "image" );

	/* create rows array */
	rows = (unsigned char **)bu_calloc( file_height, sizeof( unsigned char *), "rows" );
	for( i=0 ; i<file_height ; i++ )
		rows[file_height-1-i] = image+(i*file_width*3);

	png_read_image( png_p, rows );

	if( png_get_oFFs( png_p, info_p, &xoff, &yoff, &unit_type ) )
	{
		if( unit_type == PNG_OFFSET_PIXEL )
			bu_log( "X Offset: %d pixels\nY Offset: %d pixels\n", xoff, yoff );
		else if( unit_type == PNG_OFFSET_MICROMETER )
			bu_log( "X Offset: %d um\nY Offset: %d um\n", xoff, yoff );
	}

	if( png_get_pHYs( png_p, info_p, &xres, &yres, &unit_type ) )
	{
		if( unit_type == PNG_RESOLUTION_UNKNOWN )
			bu_log( "Aspect ratio: %g (width/height)\n", (double)xres/(double)yres );
		else if( unit_type == PNG_RESOLUTION_METER )
			bu_log( "pixel density:\n\t%d pixels/m hroizontal\n\t%d pixels/m vertical\n",
					xres, yres );
	}

	if( png_get_interlace_type( png_p, info_p ) == PNG_INTERLACE_NONE )
		bu_log( "not interlaced\n" );
	else
		bu_log( "interlaced\n" );

	if( color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
			color_type == PNG_COLOR_TYPE_RGB_ALPHA )
		if( png_get_bKGD( png_p, info_p, &input_backgrd ) )
			bu_log( "background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue );

	if( png_get_sRGB( png_p, info_p, &rgb_intent ) )
	{
		bu_log( "rendering intent: " );
		switch( rgb_intent )
		{
			case PNG_sRGB_INTENT_SATURATION:
				bu_log( "saturation\n" );
				break;
			case PNG_sRGB_INTENT_PERCEPTUAL:
				bu_log( "perceptual\n" );
				break;
			case PNG_sRGB_INTENT_ABSOLUTE:
				bu_log( "absolute\n" );
				break;
			case PNG_sRGB_INTENT_RELATIVE:
				bu_log( "relative\n" );
				break;
		}
	}

	if( png_get_gAMA( png_p, info_p, &gamma ) )
		bu_log( "gamma: %g\n", gamma );

#if defined(PNG_READ_cHRM_SUPPORTED)
	if( png_get_cHRM( png_p, info_p, &white_x, &white_y, &red_x, &red_y, &green_x, &green_y, &blue_x, &blue_y ) )
	{
		bu_log( "Chromaticity:\n" );
		bu_log( "\twhite point\t(%g %g )\n\tred\t(%g %g)\n\tgreen\t(%g %g)\n\tblue\t(%g %g)\n",
			white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y );
	}
#endif

	if( png_get_text( png_p, info_p, &text, &num_text ) )
		for( i=0 ; i<num_text ; i++ )
			bu_log( "%s: %s\n", text[i].key, text[i].text );

	if( png_get_tIME( png_p, info_p, &mod_time ) )
		bu_log( "Last modified: %d/%d/%d %d:%d:%d\n", mod_time->month, mod_time->day,
			mod_time->year, mod_time->hour, mod_time->minute, mod_time->second );
	return 0;
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
