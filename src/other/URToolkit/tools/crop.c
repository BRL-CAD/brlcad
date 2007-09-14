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
 * crop.c - Crop an image to the given size.
 *
 * Author:	Rod Bogart & John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Jun 23 1986
 * Copyright (c) 1986, University of Utah
 *
 */
#ifndef lint
static char rcs_ident[] = "$Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

extern void rle_box();
int pos_box_vals();

int
main(argc, argv)
int	argc;
char	*argv[];
{
    rle_pixel **scanline, **rows, **outrows;
    int xlen, i, j;
    int xmin = -1, ymin = -1, xmax = -1, ymax = -1;
    char *infilename = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    int oflag = 0, bflag = 0, bottom_row;
    rle_hdr 	in_hdr, out_hdr;
    int rle_cnt, rle_err;
    long start;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if (scanargs( argc, argv,
		  "% b%- xmin%d ymin%d xmax%d ymax%d o%-outfile!s infile%s",
		  &bflag, &xmin, &ymin, &xmax, &ymax, &oflag,  &out_fname,
		  &infilename ) == 0)
    {
	exit(-1);
    }

    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infilename, "r" );
    rle_names( &in_hdr, cmd_name( argv ), infilename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );
    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	start = ftell( in_hdr.rle_file );
	if ( (rle_err = rle_get_setup( &in_hdr )) != RLE_SUCCESS )
	    break;

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	rle_addhist( argv, &in_hdr, &out_hdr );

	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	/* If bflag, then pre-scan file to get bounding box. */
	if (bflag != 0)
	{
	    if (xmin == -1 && start >= 0)
	    {
		/* Common code with rlebox program. */
		rle_box( &in_hdr, &xmin, &xmax, &ymin, &ymax );

		/* Rewind and start over. */
		fseek( in_hdr.rle_file, start, 0 );
		rle_get_setup( &in_hdr );	/* Should work fine this time. */
	    }
	    else
	    {
		/* error message and exit: -b flag and box values or piped input */
		if (xmin != -1)
		    fprintf( stderr,
			     "%s: You cannot specify the -b flag and box values together\n",
			     cmd_name( argv ) );
		else
		    fprintf( stderr,
			     "%s: No piped input allowed with the -b flag\n",
			     cmd_name( argv ) );
		exit(-1);
	    }
	}
	else if ( pos_box_vals( xmin, ymin, xmax, ymax ) != 0 )
	{
	    fprintf( stderr,
		     "%s: You must specify either all the box coordinates or the -b flag\n",
		     cmd_name( argv ) );
	    exit(-1);
	}

	xlen = xmax - xmin + 1;

	if ( (xmin > xmax) || (ymin > ymax) )
	{
	    fprintf( stderr, "%s: Illegal size: %d, %d to %d, %d\n",
		     cmd_name( argv ), xmin, ymin, xmax, ymax );
	    exit(-1);
	}

	/* should check for disjoint regions */

	out_hdr.xmin = xmin;
	out_hdr.ymin = ymin;
	out_hdr.xmax = xmax;
	out_hdr.ymax = ymax;

	rle_put_setup( &out_hdr );

	if (out_hdr.xmax > in_hdr.xmax)
	    RLE_CHECK_ALLOC( cmd_name( argv ),
			     rle_row_alloc( &out_hdr, &scanline ) == 0,
			     "image data" );
	else
	    RLE_CHECK_ALLOC( cmd_name( argv ),
			     rle_row_alloc( &in_hdr, &scanline ) == 0,
			     "image data" );

	rows = (rle_pixel **)
	    malloc((in_hdr.ncolors + in_hdr.alpha) * sizeof( rle_pixel * ));
	RLE_CHECK_ALLOC( cmd_name( argv ), rows, "input image data" );

	outrows = (rle_pixel **)
	    malloc((in_hdr.ncolors + in_hdr.alpha) * sizeof( rle_pixel * ));
	RLE_CHECK_ALLOC( cmd_name( argv ), outrows, "output image data" );

	if ( in_hdr.alpha )
	{
	    rows++;		/* So alpha is rows[-1] */
	    outrows++;
	}

	for( i = -in_hdr.alpha; i < in_hdr.ncolors; i++ )
	    rows[i] = scanline[i];

	for( i = -in_hdr.alpha; i < in_hdr.ncolors; i++ )
	    outrows[i] = &scanline[i][xmin];

	/* Fill scanline with background color in case output is
	 * larger than input.
	 */
	for ( j = -in_hdr.alpha; j < in_hdr.ncolors; j++ )
	    if ( in_hdr.bg_color && j >= 0 && in_hdr.bg_color[j] != 0 )
	    {
		int c = in_hdr.bg_color[j];
		register rle_pixel *pix;
		for ( i = out_hdr.xmin, pix = &scanline[j][i];
		      i <= out_hdr.xmax;
		      i++, pix++ )
		    *pix = c;
	    }
	    else
		bzero( (char *)&scanline[j][out_hdr.xmin],
		       out_hdr.xmax - out_hdr.xmin + 1 );

	bottom_row = in_hdr.ymin;
	if (in_hdr.ymin > out_hdr.ymin)
	{
	    /* Output blank scanlines if crop region is larger than data */
	    rle_skiprow(&out_hdr, in_hdr.ymin - out_hdr.ymin);
	    bottom_row = in_hdr.ymin;
	}
	else
	    if (in_hdr.ymin < out_hdr.ymin)
	    {
		/* Read past extra lower scanlines */
		for (i = in_hdr.ymin; i < out_hdr.ymin; i++)
		    rle_getrow(&in_hdr, rows );
		bottom_row = out_hdr.ymin;
	    }

	/* Read in image */
	for (j = bottom_row; j <= out_hdr.ymax; j++)
	{
	    rle_getrow(&in_hdr, rows );

	    rle_putrow( outrows, xlen, &out_hdr );
	}
	rle_puteof( &out_hdr );

	/* Skip extra upper scanlines. */
	while ( rle_getskip( &in_hdr ) != 32768 )
	    ;

	/* Clean up for next time around. */
	rle_row_free( (out_hdr.xmax > in_hdr.xmax) ? &out_hdr : &in_hdr,
		      scanline );
	free( rows - in_hdr.alpha );
	free( outrows - in_hdr.alpha );

	if ( bflag )
	    xmin = xmax = ymin = ymax = -1;
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infilename );


    exit( 0 );
}

/* Return 0 if all parameters >= 0, else -1. */
int
pos_box_vals(x1, y1, x2, y2)
int x1, y1, x2, y2;
{
  if ((x1 < 0) || (y1 < 0) || (x2 < 0) || (y2 < 0))
    return -1;
  else
    return 0;
}
