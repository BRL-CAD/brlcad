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
 * rleskel.c - Skeleton RLE tool.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Tue Jun 12 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "rle.h"

/*****************************************************************
 * TAG( main )
 * 
 * A skeleton RLE tool.  Demonstrates argument parsing, opening,
 * reading, and writing RLE files.  Includes support for files
 * consisting of concatenated images.
 * Usage:
 * 	rleskel [-o outfile] [infile]
 * Inputs:
 * 	infile:		The input RLE file.  Default stdin.
 * 			"-" means stdin.
 * Outputs:
 * 	-o outfile:	The output RLE file.  Default stdout.
 * 			"-" means stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Repeatedly read from the input until the file EOF or an
 * 	error is encountered.
 */
main(int argc, char **argv)
{
    char       *infname = NULL,
    	       *outfname = NULL;
    int 	oflag = 0;
    int		rle_cnt, rle_err, width, y;
    FILE       *outfile;
    rle_hdr in_hdr, out_hdr;	/* Headers for input and output files. */
    rle_pixel **rows;		/* Will be used for scanline storage. */
    
    if ( scanargs( argc, argv, "% o%-outfile!s infile%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    /* Open the input file.
     * The output file won't be opened until the first image header
     * has been read.  This avoids unnecessarily wiping out a
     * pre-existing file if the input is garbage.
     */
    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infname, "r" );

    /* Read images from the input file until the end of file is
     * encountered or an error occurs.
     */
    rle_cnt = 0;
    while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
    {
	/* Open the output file when the first header is successfully read. */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );

	/* Count the input images. */
	rle_cnt++;

	/* The output header is a copy of the input header.  The only
	 * difference is the FILE pointer.
	 */
	out_hdr = in_hdr;
	out_hdr.rle_file = outfile;

	/* Add to the history comment. */
	rle_addhist( argv, &in_hdr, &out_hdr );

	/* Write the output image header. */
	rle_put_setup( &out_hdr );

	/* Since rle_getrow and rle_putrow use different array origins,
	 * we will compensate by adjusting the xmin and xmax values in
	 * the input header.  [rle_getrow assumes that the scanline
	 * array starts at pixel 0, while rle_putrow assumes that the
	 * scanline array starts at pixel xmin.  This is a botch, but
	 * it's too late to change it now.]
	 */
	in_hdr.xmax -= in_hdr.xmin;
	in_hdr.xmin = 0;
	width = in_hdr.xmax + 1;	/* Width of a scanline. */

	/* Allocate memory into which the image scanlines can be read.
	 * This should happen after the above adjustment, to minimize
	 * the amount of memory allocated.
	 */
	if ( rle_row_alloc( &in_hdr, &rows ) < 0 )
	{
	    fprintf( stderr, "rleskel: Unable to allocate image memory.\n" );
	    exit( RLE_NO_SPACE );
	}
	
	/* Read the input image and copy it to the output file. */
	for ( y = in_hdr.ymin; y <= in_hdr.ymax; y++ )
	{
	    /* Read a scanline. */
	    rle_getrow( &in_hdr, rows );
	    
	    /* Process the scanline as desired here. */

	    /* Write the processed scanline. */
	    rle_putrow( rows, width, &out_hdr );
	}

	/* Protect from bad input. */
	while ( rle_getskip( &in_hdr ) != 32768 )
	    ;

	/* Free memory. */
	rle_row_free( &in_hdr, rows );

	/* Write an end-of-image code. */
	rle_puteof( &out_hdr );
    }
    
    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
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
