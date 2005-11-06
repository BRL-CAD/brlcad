/* 
 * rlemandl.c - Compute images of the Mandelbrot set in RLE format
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Nov  9 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

void
main( argc, argv )
int argc;
char **argv;
{
    char *out_fname = NULL;
    rle_hdr out_hdr;
    char comment[128];
    float real, imaginary, width, step;
    float ypos, xpos;
    rle_pixel ** rows;
    int xsize = 256, ysize = 256;
    int x_pixel, y_pixel;
    int oflag = 0, offset = 1, junk, verbose = 0;
    double scale = 1;
    register int iter;
    int stop;
    /* May want to use floats...whatever's fastest */
    register double z_r, z_i, z_rs, z_is;
    
    out_hdr = *rle_hdr_init( NULL );

    if (! scanargs(argc, argv,
		   "% v%- s%-xsize!dysize!d b%-band-scale!Fband-offset%d \n\
\to%-outfile!s real!f imag!f width!f",
		   &verbose, 
		   &junk, &xsize, &ysize,
		   &junk, &scale, &offset,
		   &oflag, &out_fname,
		   &real, &imaginary, &width ))
	exit( -2 );

    step = width / (double) xsize;

    sprintf( comment,
    "Mandelbrot=set centered at (%g %g), width %g.  Bands %g wide, offset %d.",
	     real, imaginary, width, scale, offset );

    xpos = real - width / 2.0;
    ypos = imaginary - (step / 2.0) * (double) ysize;

    /* Re-use real as left margin */
    real = xpos;

    /* Change the default rle_dflt_hdr struct to match what we need */
    out_hdr.rle_file = rle_open_f(cmd_name( argv ), out_fname, "w");
    rle_names( &out_hdr, cmd_name( argv ), out_fname, 0 );
   
    out_hdr.xmax = xsize - 1;
    out_hdr.ymax = ysize - 1;
    out_hdr.ncolors = 1;	/* One output channel */
    out_hdr.alpha = 0;		/* No coverage (alpha) */

    rle_putcom( comment, &out_hdr );
    rle_addhist( argv, (rle_hdr *)0, &out_hdr );

    /* Allocate storage for the output row */
    if (rle_row_alloc( &out_hdr, &rows ) < 0)
	RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );

    /* Create the header in the output file */
    rle_put_setup( &out_hdr );

    for (y_pixel = 0; y_pixel < ysize; y_pixel++)
    {
	xpos = real;
	for (x_pixel = 0; x_pixel < xsize; x_pixel++)
	{
	    z_r = 0;
	    z_i = 0;
	    iter = 0;
	    stop = scale * 255 + offset;
	    while (iter < stop &&
		   ((z_rs = z_r * z_r) + (z_is = z_i * z_i)) < 4)
	    {
		z_i = 2 * z_r*z_i + ypos;
		z_r = z_rs - z_is + xpos;
		iter++;
	    }
	    if ( iter == stop )
		iter = 255;
	    else
		iter = (iter - offset) / scale;
	    rows[0][x_pixel] = (rle_pixel) (iter <= 0 ? 0 : iter);
	    xpos += step;
	}
	rle_putrow( rows, xsize + 1, &out_hdr );
	if (verbose)
	    if ((y_pixel % 50) == 0)
		fprintf(stderr, "line %d...\n", y_pixel);
	ypos += step;
    }
    rle_puteof( &out_hdr );
    exit( 0 );
}
