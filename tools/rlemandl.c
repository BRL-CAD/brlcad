/* 
 * rlemandl.c - Compute images of the Mandelbrot set in RLE format
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Nov  9 1987
 * Copyright (c) 1987, University of Utah
 */

#include "conf.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdlib.h>

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "rle.h"

int
main(int argc, char **argv)
{
    char *out_fname = NULL;
    float real, imaginary, width, step;
    float ypos, xpos;
    rle_pixel ** rows;
    int xsize = 256, ysize = 256;
    int x_pixel, y_pixel;
    int oflag = 0, junk, verbose = 0;
    register int iter;
    /* May want to use floats...whatever's fastest */
    register double z_r, z_i, z_rs, z_is;
    
    if (! scanargs(argc, argv,
	         "% v%- s%-xsize!dysize!d o%-outfile!s real!f image!f width!f",
		   &verbose, 
		   &junk, &xsize, &ysize, &oflag, &out_fname,
		   &real, &imaginary, &width ))
	exit( -2 );

    step = width / (float) xsize;

    xpos = real - width / 2.0;
    ypos = imaginary - (step / 2.0) * (float) ysize;

    /* Re-use real as left margin */
    real = xpos;

    /* Change the default rle_dflt_hdr struct to match what we need */
    rle_dflt_hdr.rle_file = rle_open_f("rlemandl", out_fname, "w");
   
    rle_dflt_hdr.xmax = xsize - 1;
    rle_dflt_hdr.ymax = ysize - 1;
    rle_dflt_hdr.ncolors = 1;	/* One output channel */
    rle_dflt_hdr.alpha = 0;	/* No coverage (alpha) */

    rle_addhist( argv, (rle_hdr *)0, &rle_dflt_hdr );

    /* Allocate storage for the output row */
    if (rle_row_alloc( &rle_dflt_hdr, &rows ))
    {
	fprintf( stderr, "rlemandl: malloc failed\n" );
	exit( -2 );
    }

    /* Create the header in the output file */
    rle_put_setup( &rle_dflt_hdr );

    for (y_pixel = 0; y_pixel < ysize; y_pixel++)
    {
	xpos = real;
	for (x_pixel = 0; x_pixel < xsize; x_pixel++)
	{
	    z_r = 0;
	    z_i = 0;
	    iter = 0;
	    while (iter < 255 && ((z_rs = z_r * z_r) + (z_is = z_i * z_i)) < 4)
	    {
		z_i = 2 * z_r*z_i + ypos;
		z_r = z_rs - z_is + xpos;
		iter++;
	    }
	    rows[0][x_pixel] = (rle_pixel) iter == 1 ? 0 : iter;
	    xpos += step;
	}
	rle_putrow( rows, xsize + 1, &rle_dflt_hdr );
	if (verbose)
	    if ((y_pixel % 50) == 0)
		fprintf(stderr, "line %d...\n", y_pixel);
	ypos += step;
    }
    rle_puteof( &rle_dflt_hdr );
    exit( 0 );
}
