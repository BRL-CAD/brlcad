/*
 *	Convert PNG (Portable Network Graphics) format to pix
 *
 *	Author -
 *		John R. Anderson
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
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "png.h"
#include "zlib.h"

static png_color_16 def_backgrd={ 0,0,0,0,0 };
static double def_gamma=1.0;

main( argc, argv )
int argc;
char *argv[];
{
	int i;
	FILE *fp_in;
	png_structp png_p;
	png_infop info_p;
	char header[8];
	int bit_depth;
	int color_type;
	png_color_16p input_backgrd;
	double gamma;
	int file_width, file_height;
	unsigned char *image;
	unsigned char **rows;

	if( argc == 2 )
	{
		if( (fp_in=fopen( argv[1], "rb" )) == NULL )
		{
			perror( argv[0] );
			bu_log( "Cannot open %s\n", argv[1] );
			bu_bomb( "Cannot open PNG file\n" );
		}
	}
	else
		fp_in = stdin;

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

	if( color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
	{
		bu_log( "Warning: bw image being converted to RGB!!!\n" );
		png_set_gray_to_rgb( png_p );
	}

	if( png_get_bKGD( png_p, info_p, &input_backgrd ) )
		png_set_background( png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0 );
	else
		png_set_background( png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0 );

	if( png_get_gAMA( png_p, info_p, &gamma ) )
		png_set_gamma( png_p, 1.0, gamma );
	else
		png_set_gamma( png_p, 1.0, def_gamma );

	bit_depth = png_get_bit_depth( png_p, info_p );
	if( bit_depth < 8 )
		png_set_packing( png_p );
	else if( bit_depth == 16 )
		png_set_strip_16( png_p );

	file_width = png_get_image_width( png_p, info_p );
	file_height = png_get_image_height( png_p, info_p );

	png_read_update_info( png_p, info_p );

	/* allocate memory for image */
	image = (unsigned char *)bu_calloc( 1, file_width*file_height*3, "image" );

	/* create rows array */
	rows = (unsigned char **)bu_calloc( file_height, sizeof( unsigned char *), "rows" );
	for( i=0 ; i<file_height ; i++ )
		rows[file_height-1-i] = image+(i*file_width*3);

	png_read_image( png_p, rows );

	fwrite( image, file_width*file_height*3, 1,stdout );
}
