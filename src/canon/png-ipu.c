/*                       P N G - I P U . C
 * BRL-CAD
 *
 * Copyright (C) 1992-2005 United States Government as represented by
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
/** @file png-ipu.c
 *
 *  Display a PNG file on the Canon CLC-500 Color Laser printer.
 *  This program is based upon pix-ipu.c
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *
 *	Options
 *	a	autosize image file
 *	c	clear framebuffer first
 *	d	SCSI device
 *	g	gamma
 *	h	1Kx1K
 *	m	mosaic
 *	n	scanlines (image)
 *	s	squaresize (image)
 *	w	width (image)
 *	x	file_xoffset
 *	y	file_yoffset
 *	z	zoom image display
 *	A	Autoscale
 *	M	Mag_factor
 *	R	Resolution
 *	C	# copies
 *	D	Divisor
 *	N	scr_height
 *	S	scr_height
 *	U	units ( i | m )
 *	W	scr_width
 *	X	scr_xoffset
 *	Y	scr_yoffset
 *	v	verbose;
 *	V	verbose;
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "png.h"

static int
mread(fd, bufp, n)
     int	fd;
     register char	*bufp;
     int	n;
{
    register int	count = 0;
    register int	nread;

    do {
	nread = read(fd, bufp, (unsigned)n-count);
	if(nread < 0)  {
	    return nread;
	}
	if(nread == 0)
	    return((int)count);
	count += (unsigned)nread;
	bufp += nread;
    } while(count < n);

    return((int)count);
}

#if defined(IRIX) && (IRIX == 4 || IRIX == 5 || IRIX == 6)
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include "./canon.h"

static png_color_16 def_backgrd={ 255,255,255,255,255 };	/* white */
static double	def_screen_gamma=1.0;	/* Don't add more gamma, by default */

static unsigned char **scanline;	/* 1 scanline pixel buffer */


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
     int ac;
     char *av[];
{
    int arg_index;
    struct dsreq *dsp;
    int i;
    /**/
    png_structp png_p;
    png_infop info_p;
    char header[8];
    int bit_depth;
    int color_type;
    png_color_16p input_backgrd;
    double gamma=1.0;
    int file_width, file_height;
    unsigned char *image;


    if ((arg_index = parse_args(ac, av)) >= ac) {
	if (isatty(fileno(stdin)))
	    usage("Specify image on cmd line or redirect from standard input\n");

	if (autosize) fprintf(stderr, "Cannot autosize stdin\n");

    } else if (arg_index+1 < ac)
	(void)fprintf(stderr,
		      "%s: Excess command line arguments ignored\n", *av);
    else if (freopen(av[arg_index], "r", stdin) == NULL) {
	perror(av[arg_index]);
	return(-1);
    }


    /* open the printer SCSI device */
    if ((dsp = dsopen(scsi_device, O_RDWR)) == NULL) {
	perror(scsi_device);
	usage("Cannot open SCSI device\n");
    }

    /* Read the image */
    if( fread( header, 8, 1, stdin ) != 1 )
	bu_bomb( "png-ipu: ERROR: Failed while reading file header!!!\n" );

    if( !png_check_sig( (png_bytep)header, 8 ) )
	bu_bomb( "png-ipu: This is not a PNG file!!!\n" );

    png_p = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
    if( !png_p )
	bu_bomb( "png_create_read_struct() failed!!\n" );

    info_p = png_create_info_struct( png_p );
    if( !info_p )
	bu_bomb( "png_create_info_struct() failed!!\n" );

    png_init_io( png_p, stdin );

    png_set_sig_bytes( png_p, 8 );

    png_read_info( png_p, info_p );

    color_type = png_get_color_type( png_p, info_p );

    png_set_expand( png_p );
    bit_depth = png_get_bit_depth( png_p, info_p );
    if( bit_depth == 16 )
	png_set_strip_16( png_p );

    file_width = png_get_image_width( png_p, info_p );
    file_height = png_get_image_height( png_p, info_p );

    if( ipu_debug )
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

    if( png_get_bKGD( png_p, info_p, &input_backgrd ) )
	{
	    if( ipu_debug && (color_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
			      color_type == PNG_COLOR_TYPE_RGB_ALPHA ) )
		bu_log( "background color: %d %d %d\n", input_backgrd->red, input_backgrd->green, input_backgrd->blue );
	    png_set_background( png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0 );
	}
    else
	png_set_background( png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0 );

    if( !png_get_gAMA( png_p, info_p, &gamma ) )
	gamma = 0.5;
    png_set_gamma( png_p, def_screen_gamma, gamma );
    if( ipu_debug )
	bu_log( "file gamma: %f, additional screen gamma: %f\n",
		gamma, def_screen_gamma );

    if( ipu_debug )
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

    /* Change order top-to-bottom */
    for( i=0 ; i<file_height ; i++ )
	scanline[file_height-1-i] = image+(i*file_width*3);

    png_read_image( png_p, scanline );

    if( ipu_debug )
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

    if (ipu_debug)
	fprintf(stderr, "Image is %dx%d\n", file_width, file_height);

    if (conv == IPU_RESOLUTION) {
	if (scr_width)
	    scr_width *= 400.0 / (double)param.i;
	else
	    scr_width = file_width * 400.0 / (double)param.i;
	if (scr_height)
	    scr_height *= 400.0 / (double)param.i;
	else
	    scr_height = file_height * 400.0 / (double)param.i;
    } else if (conv == IPU_MAG_FACTOR) {
	if (scr_width)
	    scr_width *= 400.0 / (double)param.i;
	else
	    scr_width = file_width * 400.0 / (double)param.i;
	if (scr_height)
	    scr_height *= 400.0 / (double)param.i;
	else
	    scr_height = file_height * 400.0 / (double)param.i;
    }

    /* Wait for printer to finish what it was doing */
    ipu_acquire(dsp, 120);

    ipu_delete_file(dsp, 1);
    ipu_create_file(dsp, (char)1, ipu_filetype, file_width, file_height, 0);
    ipu_put_image(dsp, (char)1, file_width, file_height, image);

    ipu_print_config(dsp, units, divisor, conv,
		     mosaic, ipu_gamma, tray);

    if( ipu_filetype == IPU_PALETTE_FILE )
	ipu_set_palette(dsp, NULL);

    ipu_print_file(dsp, (char)1, copies, 0/*wait*/,
		   scr_xoff, scr_yoff, scr_width, scr_height, &param);

    /* Wait for print operation to complete */
    ipu_acquire(dsp, 30 * copies);

    dsclose(dsp);
    return(0);
}

#else
int
main(ac, av)
     int ac;
     char *av[];
{
    fprintf(stderr,
	    "%s only works on SGI(tm) systems with dslib support\n", *av);
    return(-1);
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
