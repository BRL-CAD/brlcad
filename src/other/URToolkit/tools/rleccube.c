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
 * rleccube.c - Make an image of a "color cube".
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Tue Jan 29 1991
 * Copyright (c) 1991, University of Michigan
 */
#ifndef lint
char rcsid[] = "$Header$";
#endif
/*
rleccube()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

/*****************************************************************
 * TAG( main )
 *
 * Usage:
 * 	rleccube [-p] [-w squares-wide] [-o outfile] [cube-side]
 * Inputs:
 * 	squares-wide:	The number of squares per row in the output
 * 			image.  The output image will have
 * 			ceiling(cube-side / squares-wide) rows, each
 * 			with squares-wide blocks.  For best results,
 * 			should divide cube-side.  Default is the
 * 			smallest divisor of cube-side >=
 * 			sqrt(cube-side).
 *	cube-side:	The number of levels of red, green, and blue
 *			on each side of the color cube.  The output
 *			image will have cube-side^3 pixels, arranged
 *			in squares as described above.  Each square
 *			will have a constant value of red, with green
 *			and blue varying along the horizontal and
 *			vertical axes, respectively.  Black will be in
 *			the lower left corner; red increases along
 *			each row from left to right.  Must be <= 256.
 *			Default is 64.
 * Outputs:
 * 	outfile:	If specified, the output image will be written
 * 			to this file.  Otherwise, it will be written
 * 			to the standard output.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Pretty simple.
 */
int
main( argc, argv)
int argc;
char **argv;
{
    int		wflag = 0,
		squares_wide = 8;	/* Default squares/row. */
    int		cube_side = 64;		/* Default size of cube is 64x64x64. */
    double	step = 255. / 63.;	/* Corresponding step size. */
    int		oflag = 0;
    int		pflag = 0;
    int		nbits = 6;		/* 64 = 2^6 */
    char       *ofname = NULL;
    int r = 0, g, b;
    int x, y;
    rle_pixel **rows;

    if ( scanargs( argc, argv,
		   "% p%- w%-squares-wide!d o%-outfile!s cube-side%d",
		   &pflag, &wflag, &squares_wide,
		   &oflag, &ofname, &cube_side ) == 0 )
	exit( 1 );

    if ( cube_side != 64 )
    {
	/* Error check. */
	if ( cube_side < 2 || cube_side > 256 )
	{
	    fprintf( stderr,
		     "%s: cube_side (%d given) must be >=2 and <= 256.\n",
		     cmd_name( argv ), cube_side );
	    exit( 1 );
	}
	/* Peano curve must have power of 2. */
	if ( pflag )
	{
	    register int i;

	    if ( cube_side > 64 )
	    {
		fprintf( stderr,
	 "%s: cube_side (%d given) must be <= 64 for Peano curve display.\n",
			 cmd_name( argv ), cube_side );
		exit( 1 );
	    }
	    for ( nbits = 0, i = 1; i < cube_side; i <<= 1, nbits += 1 )
		;
	    if ( cube_side != i )
		fprintf( stderr,
	 "%s: cube_side (%d given) adjusted to %d for Peano curve display.\n",
			 cmd_name( argv ), cube_side, i );
	    cube_side = i;
	    if ( wflag )
	    {
		fprintf( stderr,
	 "%s: squares_wide (%d given) ignored for Peano curve display.\n",
			 cmd_name( argv ), squares_wide );
		wflag = 0;
	    }
	}
	/* Recompute step and squares_wide. */
	step = 255. / (cube_side - 1);
	if ( !wflag )
	{
	    squares_wide = (int)sqrt((double)cube_side);

	    /* If cube_side isn't an exact square, search for a larger
	     * divisor of cube_side.
	     */
	    while ( cube_side % squares_wide != 0 )
		squares_wide++;
	}
    }

    /* Error check. */
    if ( wflag )
    {
	if ( squares_wide < 1 || squares_wide > cube_side )
	{
	    fprintf( stderr,
		     "%s: squares_wide (%d given) must be >= 1 and <= %d.",
		     cmd_name( argv ), squares_wide, cube_side );
	    exit( 1 );
	}
	if ( pflag )
	{
	    fprintf( stderr,
	     "%s: squares_wide (%d given) ignored for Peano curve display.\n",
		     cmd_name( argv ), squares_wide );
	    /* Note: only get here if cube_side was NOT given, so it's
	     * safe to reset squares_wide to its default.
	     */
	    squares_wide = 8;
	}
    }

    /* Set up the output. */
    rle_dflt_hdr.xmin = rle_dflt_hdr.ymin = 0;
    rle_dflt_hdr.xmax = cube_side * squares_wide - 1;
    rle_dflt_hdr.ymax =
	cube_side * ((cube_side + squares_wide - 1) / squares_wide) - 1;
    rle_addhist( argv, NULL, &rle_dflt_hdr );
    rle_dflt_hdr.rle_file = rle_open_f( cmd_name( argv ), ofname, "w" );
    /* The other default values should be ok. */

    rle_put_setup( &rle_dflt_hdr );

    /* Allocate scanline memory. */
    if ( rle_row_alloc( &rle_dflt_hdr, &rows ) < 0 )
	RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );

    for ( y = 0; y <= rle_dflt_hdr.ymax; y++ )
    {
	if ( !pflag )
	{
	    /* Green is constant per scanline. */
	    g = (int)(step * (y % cube_side));
	    for ( x = 0; x <= rle_dflt_hdr.xmax; x++ )
		rows[1][x] = g;

	    /* If first scanline of a new row, recompute red and blue. */
	    if ( g == 0 )
		for ( x = 0; x <= rle_dflt_hdr.xmax; x++ )
		{
		    b = (int)(step * (x % cube_side));
		    if ( b == 0 )
			r = (int)(step * (x / cube_side +
					  squares_wide * y / cube_side));

		    rows[0][x] = r;
		    rows[2][x] = b;
		}
	}
	else
	{
	    long int r;
	    int a[2], c[3];
	    int two_d_bits = (nbits * 3 / 2),
	    	two_d_max = 1 << two_d_bits,
	    	max_r = 1 << (3 * nbits);

	    /* For each pixel on the scan-line, find its peano index
	     * and get the corresponding color.
	     */
	    a[1] = y;
	    for ( x = 0; x <= rle_dflt_hdr.xmax; x++ )
	    {
		if ( x < two_d_max )
		    a[0] = x;
		else
		    a[0] = 2 * two_d_max - x - 1;
		hilbert_c2i( 2, nbits * 3 / 2, a, &r );
		if ( x >= two_d_max )
		    r = max_r - r - 1;
		hilbert_i2c( 3, nbits, r, c );
		rows[0][x] = c[0] << (8 - nbits);
		rows[1][x] = c[1] << (8 - nbits);
		rows[2][x] = c[2] << (8 - nbits);
	    }
	}

	rle_putrow( rows, squares_wide * cube_side, &rle_dflt_hdr );
    }

    /* All done. */
    rle_puteof( &rle_dflt_hdr );
    exit( 0 );
}
