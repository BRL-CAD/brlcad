/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * rlequant.c - Quantize an image to a given number of colors.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Tue Jun 12 1990
 * Copyright (c) 1990, University of Michigan
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "externs.h"		/* For malloc and free */
#include "rle.h"

#define Quantize(x)	(x >> shift)

/* Handle malloc errors. */
#define CHECK_MALLOC(type, ptr, size, desc)	\
    if ( ((ptr) = (type *)malloc( size )) == NULL ) \
    {  \
	fprintf( stderr, "%s: Can't allocate memory for %s.\n", \
		 MY_NAME, desc ); \
	exit( RLE_NO_SPACE ); \
    }

/* An enumerated type for the state machine.
 * If in NORMAL state, stays in normal state for all images.
 * To do -m, start in INIT_HIST for first image, move to USE_HIST for
 * subsequent images (only compute histogram at this point).  Switch
 * to PROCESS_HIST after finishing with input images.  Then rewind
 * file and output images.
 */
typedef enum { NORMAL=0, INIT_HIST, USE_HIST, PROCESS_HIST, OUTPUT } state_t;

static void mem_alloc(rle_hdr *hdr, long unsigned int entries, int dflag, rle_pixel ***rows, rle_pixel **red, rle_pixel **green, rle_pixel **blue, rle_pixel **alpha, rle_pixel **img_red, rle_pixel **img_green, rle_pixel **img_blue);
static void read_input(rle_hdr *hdr, int width, int shift, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel **rows);
static void copy_hdr(char **argv, rle_hdr *in_hdr, rle_hdr *out_hdr, FILE *outfile, int cflag);
static void setup_output(rle_hdr *hdr, int colors, rle_pixel **colormap);
static void write_output(rle_hdr *hdr, int width, int height, int bits, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel *rgbmap, rle_pixel **colormap, rle_pixel **outrows);
static void free_mem(rle_hdr *hdr, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel **rows, rle_pixel **outrows);

static const char *MY_NAME = "rlequant";

/*****************************************************************
 * TAG( main )
 *
 * Using Craig Kolb's colorquant code, quantize an RLE image to a
 * specified number of colors.
 * Usage:
 * 	rlequant [-b bits] [-d] [-f] [-n colors] [-o outfile] [infile]
 * Inputs:
 * 	-b bits:	The number of bits to which the image will be
 * 			"prequantized".  The default is 5.  Normally
 * 			this is fine, but some images have a very
 * 			limited number of colors.  The program will
 * 			require more memory if a larger value is
 * 			given: an array of size 2^(3*bits) is
 * 			allocated.  Must be <= 8.
 * 	-c:		Just generate a color map.  The output file will
 * 			be a 0x0 image with a color map.  The
 * 			rledither program can be used to quantize any
 * 			input image to this color map.
 *	-d:		Floyd Steinberg Dither the output.
 * 	-f:		"Fast" mode.  The quantization accuracy will
 * 			be a little worse, but it is usually acceptable.
 * 	-m:		Multiple image mode: create a color map that
 * 			is "optimal" for the concatenation of all the
 * 			input images.  This is useful for creating
 * 			quantized "movies" for 8-bit displays.  If the
 * 			input comes from a pipe, the -c flag must be
 * 			specified.
 * 	-n colors:	The number of colors to quantize the image to.
 * 			The output image will contain no more than
 * 			this number of colors.  Must be <= 256.
 * 	infile:		The input RLE file.  Default stdin.
 * 			"-" means stdin.
 * Outputs:
 * 	-o outfile:	The output RLE file.  Default stdout.
 * 			"-" means stdout.  The output image will be a
 * 			single channel image with a color map.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Read image, call colorquant, write image with new colormap.
 */
int
main(int argc, char **argv)
{
    char       *infname = NULL,
    	       *outfname = NULL;
    int 	oflag = 0;
    int		bflag = 0,
    		bits = 5;
    int		cflag = 0,
    		mflag = 0,
    		nflag = 0,
    		colors_in = 256;
    int		fflag = 0;
    int		dflag = 0;
    int		rle_cnt, rle_err, width, height, shift;
    int		colors = -1;
    long	entries;
    FILE       *outfile = stdout;
    rle_hdr in_hdr, out_hdr;	/* Headers for input and output files. */
    rle_pixel **rows;		/* Will be used for scanline storage. */
    rle_pixel *outrows[2];	/* For the output scanlines. */
    rle_pixel *alpha = NULL, *red, *green, *blue;	/* Image storage. */
    rle_pixel *img_red, *img_green, *img_blue; /* Image storage. */
    rle_pixel *colormap[3];	/* The quantized colormap. */
    rle_pixel *rgbmap;		/* Used for quantization. */
    state_t	state;

    MY_NAME = cmd_name( argv );

    if ( scanargs( argc, argv,
	   "% b%-bits!d c%- d%- f%- m%- n%-colors!d o%-outfile!s infile%s",
		   &bflag, &bits, &cflag, &dflag, &fflag,
		   &mflag, &nflag, &colors_in,
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    if ( bits <= 0 || bits > 8 )
    {
	fprintf( stderr, "%s: The bits argument must be >0 and <= 8.\n",
		 MY_NAME );
	exit( 1 );
    }
    if ( colors_in <= 0 || colors_in > 256 )
    {
	fprintf( stderr, "%s: The colors argument must be >0 and <= 256.\n",
		 MY_NAME );
	exit( 1 );
    }

    /* Open the input file.
     * The output file won't be opened until the first image header
     * has been read.  This avoids unnecessarily wiping out a
     * pre-existing file if the input is garbage.
     */
    in_hdr.rle_file = rle_open_f( MY_NAME, infname, "r" );

    /* Check for seekability in multiple image mode. */
    if ( mflag && (!cflag) && ftell( in_hdr.rle_file ) < 0 )
    {
	fprintf( stderr,
	 "%s: Piped input incompatible with -m unless -c also specified.\n",
		 MY_NAME );
	exit( 1 );
    }

    /* Allocate the new colormap. */
    CHECK_MALLOC( rle_pixel, colormap[0], colors_in, "colormap" );
    CHECK_MALLOC( rle_pixel, colormap[1], colors_in, "colormap" );
    CHECK_MALLOC( rle_pixel, colormap[2], colors_in, "colormap" );

    /* Allocate memory for the rgbmap. */
    CHECK_MALLOC( rle_pixel, rgbmap, 1L << (3*bits), "RGB map" );

    /* Amount to shift incoming pixels by for prequantization. */
    shift = 8 - bits;

    /* Repeat loop for -m flag.
     * Exits when state == NORMAL or state == OUTPUT.
     */
    if ( mflag )
	state = INIT_HIST;
    else
	state = NORMAL;

    do
    {
	/* Advance state machine.
	 * Rewind input if necessary.
	 */
	if ( state == USE_HIST )
	{
	    state = OUTPUT;
	    rewind( in_hdr.rle_file );
	}

	/* Read images from the input file until the end of file is
	 * encountered or an error occurs.
	 */
	rle_cnt = 0;
	while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
	{
	    /* Open output file when the first header is successfully read. */
	    if ( rle_cnt == 0 )
		outfile = rle_open_f( MY_NAME, outfname, "w" );

	    /* Count the input images. */
	    rle_cnt++;

	    /* Verify that the input image has at least 3 color channels. */
	    if ( in_hdr.ncolors < 3 )
	    {
		fprintf( stderr,
		    "%s: Input image %d has only %d colors (must have 3).\n",
			 MY_NAME, rle_cnt, in_hdr.ncolors );
		exit ( 1 );
	    }

	    /* Copy input header to output.
	     * Only do it if it will be used:
	     *     state == NORMAL or state == OUTPUT
	     * 		An image will be output normally.
	     * 	 state == INIT_HIST && cflag In multiple
	     * 		image/color-map-only mode, we will copy the
	     * 		comments from the first image header.
	     */
	    if ( state == NORMAL || state == OUTPUT ||
		 (cflag && state == INIT_HIST) )
		copy_hdr( argv, &in_hdr, &out_hdr, outfile, cflag );

	    width = in_hdr.xmax + 1;	/* Width of a scanline. */
	    height = in_hdr.ymax + 1;	/* Height of image. */

	    /* Total number of pixels in image. */
	    entries = width * height;

	    /* Allocate memory for the imput image. */
	    mem_alloc( &in_hdr, entries, dflag, &rows,
		       &red, &green, &blue, &alpha,
		       &img_red, &img_green, &img_blue );

	    /* Read and quantize the input image. */
	    if ( !(cflag && state == OUTPUT) )
		read_input( &in_hdr, width, shift, dflag,
			    red, green, blue, alpha,
			    img_red, img_green, img_blue, rows );

	    /* Compute the color quantization map. */
	    if ( state != OUTPUT )
		colors = colorquant( red, green, blue, entries,
				     colormap, colors_in, bits,
				     rgbmap, fflag, state );

	    /* Advance state machine. */
	    if ( state == INIT_HIST )
		state = USE_HIST;

	    if ( state == NORMAL || state == OUTPUT )
	    {
		/* Compute and write setup information to the output file. */
		setup_output( &out_hdr, colors, colormap );

		/* Quantize (and maybe dither) the input image and write the
		 * output image.
		 */
		write_output( &out_hdr, width, height, bits, dflag,
			      red, green, blue, alpha,
			      img_red, img_green, img_blue, rgbmap, colormap,
			      outrows );
	    }

	    /* Free all the memory we allocated. */
	    free_mem( &in_hdr, dflag, red, green, blue, alpha,
		      img_red, img_green, img_blue, rows, outrows );
	}
    
	/* Check for an error.  EOF or EMPTY is ok if at least one image
	 * has been read.  Otherwise, print an error message.
	 */
	if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	{
	    rle_get_error( rle_err, MY_NAME, infname );
	    break;
	}

	/* In multiple image mode, finish quantization processing. */
	if ( state == USE_HIST )
	{
	    colors = colorquant( NULL, NULL, NULL, 0,
				 colormap, colors_in, bits,
				 rgbmap, fflag, PROCESS_HIST );

	    /* If multiple images and only outputting color map, do it now. */
	    if ( cflag )
	    {
		/* Write the setup information, only. */
		setup_output( &out_hdr, colors, colormap );
		rle_puteof( &out_hdr );
		/* Done. */
		state = OUTPUT;
	    }
	}
    } while ( state != OUTPUT && state != NORMAL );

    exit( 0 );
}


/*
 * copy_hdr -- copy information from the input header to the output.
 *
 * Inputs:
 * 	argv:		Command line, used to update history comment.
 * 	in_hdr:		Input image header.
 * 	outfile:	The output file pointer.
 * 	cflag:		If non-zero, output image will be 0x0.
 * Outputs:
 * 	out_hdr:	Updated from in_hdr.
 */
static void
copy_hdr(char **argv, rle_hdr *in_hdr, rle_hdr *out_hdr, FILE *outfile, int cflag)
{
    static int		zero = 0;

    /* The output header starts out as a copy of the input header.
     * The FILE pointer is different, of course.
     * Also, this output image will have only one channel.
     */
    *out_hdr = *in_hdr;
    out_hdr->rle_file = outfile;
    out_hdr->ncolors = 1;
    out_hdr->bg_color = &zero;
    out_hdr->background = 2;

    if ( cflag )
	out_hdr->xmin = out_hdr->xmax = out_hdr->ymin = out_hdr->ymax = 0;

    rle_addhist( argv, in_hdr, out_hdr );

    /* Since rle_getrow and rle_putrow use different array origins,
     * we will compensate by adjusting the xmin and xmax values in
     * the input header.  [rle_getrow assumes that the scanline
     * array starts at pixel 0, while rle_putrow assumes that the
     * scanline array starts at pixel xmin.  This is a botch, but
     * it's too late to change it now.]
     */
    in_hdr->xmax -= in_hdr->xmin;
    in_hdr->xmin = 0;

    /* For convenience, do the same to the Y values. */
    in_hdr->ymax -= in_hdr->ymin;
    in_hdr->ymin = 0;
}

static void
mem_alloc(rle_hdr *hdr, long unsigned int entries, int dflag, rle_pixel ***rows, rle_pixel **red, rle_pixel **green, rle_pixel **blue, rle_pixel **alpha, rle_pixel **img_red, rle_pixel **img_green, rle_pixel **img_blue)
{
    /* Allocate memory into which the image scanlines can be read.
     * This should happen after the above adjustment, to minimize
     * the amount of memory allocated.
     */
    if ( rle_row_alloc( hdr, rows ) < 0 )
    {
	fprintf( stderr, "%s: Unable to allocate scanline memory.\n",
		 MY_NAME );
	exit( RLE_NO_SPACE );
    }
	
    /* Allocate image memory for prequantized image. */
    CHECK_MALLOC( rle_pixel, *red, entries, "input image" );
    CHECK_MALLOC( rle_pixel, *green, entries, "input image" );
    CHECK_MALLOC( rle_pixel, *blue, entries, "input image" );

    if ( hdr->alpha )
	CHECK_MALLOC( rle_pixel, *alpha, entries, "input image" );

    if (dflag) {
	/* Allocate image memory for original image to dither on. */
	CHECK_MALLOC( rle_pixel, *img_red, entries, "dithered image" );
	CHECK_MALLOC( rle_pixel, *img_green, entries, "dithered image" );
	CHECK_MALLOC( rle_pixel, *img_blue, entries, "dithered image" );
    }
}

static void
read_input(rle_hdr *hdr, int width, int shift, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel **rows)
{
    register rle_pixel *rp, *gp, *bp, *ap;
    register rle_pixel *irp = NULL, *igp = NULL, *ibp = NULL;
    int x, y;

    /* Set traversal pointers. */
    rp = red;
    gp = green;
    bp = blue;
    ap = alpha;
    if (dflag) {
	irp = img_red;
	igp = img_green;
	ibp = img_blue;
    }

    /* Read the input image and copy it to the output file. */
    for ( y = hdr->ymin; y <= hdr->ymax; y++ )
    {
	/* Read a scanline. */
	rle_getrow( hdr, rows );
	    
	/* Prequantize the pixels. */
	for ( x = 0; x < width; x++ )
	{
	    *rp++ = Quantize( rows[0][x] );
	    *gp++ = Quantize( rows[1][x] );
	    *bp++ = Quantize( rows[2][x] );
	    if ( hdr->alpha )
		*ap++ = rows[-1][x];
	    if (dflag) {
		*irp++ = rows[0][x];
		*igp++ = rows[1][x];
		*ibp++ = rows[2][x];
	    }
	}
    }
}

static void
setup_output(rle_hdr *hdr, int colors, rle_pixel **colormap)
{
    char *buf;
    int x, y;

    CHECK_MALLOC( char, buf, 80, "comment buffer" );

    /* Give the output image a colormap. */
    CHECK_MALLOC( rle_map, hdr->cmap, 3 * 256 * sizeof( rle_map ),
		  "output colormap" );
    hdr->ncmap = 3;
    hdr->cmaplen = 8;
    for ( y = 0; y < 3; y++ )
    {
	for ( x = 0; x < colors; x++ )
	    hdr->cmap[y * 256 + x] = colormap[y][x] << 8;
	for ( ; x < 256; x++ )
	    hdr->cmap[y * 256 + x] = 0;
    }

    /* Add a comment to the output image with the true colormap length. */
    sprintf( buf, "color_map_length=%d", colors );
    rle_putcom( buf, hdr );

    /* Write the output image header. */
    rle_put_setup( hdr );

}

static void
write_output(rle_hdr *hdr, int width, int height, int bits, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel *rgbmap, rle_pixel **colormap, rle_pixel **outrows)
{
    int shift = 8 - bits;
    register rle_pixel *rp, *gp, *bp, *ap;
    register rle_pixel *irp, *igp, *ibp;
    int x, y;

    /* Allocate memory for the output scanline. */
    CHECK_MALLOC( rle_pixel, outrows[1], width, "output scanline" );

    ap = alpha;
    if (!dflag)  {
	/* Set traversal pointers. */
	rp = red;
	gp = green;
	bp = blue;

	/* Write the output image. */
	for ( y = 0; y < height; y++ )
	{
	    /* Point to the correct data. */
	    if ( hdr->alpha )
	    {
		outrows[0] = ap;
		ap += width;
	    }
	    for ( x = 0; x < width; x++, rp++, gp++, bp++ )
		outrows[1][x] = rgbmap[(((*rp<<bits)|*gp)<<bits)|*bp];

	    rle_putrow( &outrows[1], width, hdr );
	}
    }
    else {
	register short *thisptr, *nextptr ;
	int	lastline, lastpixel ;
	short *thisline, *nextline, *tmpptr;

	/* Set traversal pointers. */
	irp = img_red;
	igp = img_green;
	ibp = img_blue;

	CHECK_MALLOC( short, thisline, width * 3 * sizeof(short),
		      "dither scanline" );
	CHECK_MALLOC( short, nextline, width * 3 * sizeof(short),
		      "dither scanline" );
	nextptr = nextline;
	for (x=0; x < width; x++)
	{
	    *nextptr++ = *irp++ ;
	    *nextptr++ = *igp++ ;
	    *nextptr++ = *ibp++ ;
	}
	for (y=0; y < height; y++)
	{
	    /* swap nextline into thisline and copy new nextline */
	    tmpptr = thisline ;
	    thisline = nextline ;
	    nextline = tmpptr ;
	    lastline = (y == height - 1) ;
	    if (!lastline)
	    {
		nextptr = nextline;
		for (x=0; x < width; x++)
		{
		    *nextptr++ = *irp++ ;
		    *nextptr++ = *igp++ ;
		    *nextptr++ = *ibp++ ;
		}
	    }

	    thisptr = thisline ;
	    nextptr = nextline ;

	    for(x=0; x < width ; x++)
	    {
		int	rval, gval, bval, color ;
		rle_pixel r2,g2,b2 ;

		lastpixel = (x == width - 1) ;

		rval = *thisptr++ ;
		gval = *thisptr++ ;
		bval = *thisptr++ ;

		/* Current pixel has been accumulating error, it could be
		 * out of range.
		 */
		if( rval < 0 ) rval = 0 ;
		else if( rval > 255 ) rval = 255 ;
		if( gval < 0 ) gval = 0 ;
		else if( gval > 255 ) gval = 255 ;
		if( bval < 0 ) bval = 0 ;
		else if( bval > 255 ) bval = 255 ;

		r2 = Quantize(rval);
		g2 = Quantize(gval);
		b2 = Quantize(bval);

		color = rgbmap[(((r2<<bits)|g2)<<bits)|b2];
		outrows[1][x] = color;

		rval -= colormap[0][color];
		gval -= colormap[1][color];
		bval -= colormap[2][color];

		if( !lastpixel )
		{
		    thisptr[0] += rval * 7 / 16 ;
		    thisptr[1] += gval * 7 / 16 ;
		    thisptr[2] += bval * 7 / 16 ;
		}
		if( !lastline )
		{
		    if( x != 0 )
		    {
			nextptr[-3] += rval * 3 / 16 ;
			nextptr[-2] += gval * 3 / 16 ;
			nextptr[-1] += bval * 3 / 16 ;
		    }
		    nextptr[0] += rval * 5 / 16 ;
		    nextptr[1] += gval * 5 / 16 ;
		    nextptr[2] += bval * 5 / 16 ;
		    if( !lastpixel )
		    {
			nextptr[3] += rval / 16 ;
			nextptr[4] += gval / 16 ;
			nextptr[5] += bval / 16 ;
		    }
		    nextptr += 3 ;
		}
	    }

	    if ( hdr->alpha )
	    {
		outrows[0] = ap;
		ap += width;
	    }
	    rle_putrow( &outrows[1], width, hdr );
	}
    }

    /* Write an end-of-image code. */
    rle_puteof( hdr );
}


static void
free_mem(rle_hdr *hdr, int dflag, rle_pixel *red, rle_pixel *green, rle_pixel *blue, rle_pixel *alpha, rle_pixel *img_red, rle_pixel *img_green, rle_pixel *img_blue, rle_pixel **rows, rle_pixel **outrows)
{
    /* Free image store. */
    free( red );
    free( green );
    free( blue );
    if ( hdr->alpha )
	free( alpha );
    if (dflag) {
	/* Free image store. */
	free( img_red );
	free( img_green );
	free( img_blue );
    }
    /* Free the scanline memory. */
    rle_row_free( hdr, rows );
    free( outrows[1] );
}
