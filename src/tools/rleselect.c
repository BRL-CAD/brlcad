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
 * rleselect.c - Select images from an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Wed Jul 11 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "rle.h"


static void insert(int **sorted_list_p, int i, int n);

static const char *my_name = "rleselect";

/*****************************************************************
 * TAG( rleselect )
 *
 * Select images from an RLE file.
 * Usage:
 * 	rleselect [-i infile] [-o outfile] image-numbers ...
 * Inputs:
 * 	-i infile:	The input RLE file.  Default stdin.
 * 			"-" means stdin.
 * 	image-numbers:	A space-separated list of image numbers.
 * 			Images are numbered from 1 in the input file.
 * 			Images in this list will be included in the
 * 			output. 
 * 			A negative number in the list means that all
 * 			images from the previous number to the
 * 			absolute value of this number should be
 * 			included.  A number of 0 is taken to mean
 * 			'-infinity' (i.e., all images from the
 * 			previous through the end).
 * Examples:
 * 	rleselect 1 4 5
 * 			Selects images 1, 4, and 5.
 * 	rleselect 4 1 5
 * 			Does the same.
 * 	rleselect 1 -4 5
 * 			Selects images 1 through 5.
 * 	rleselect 3 0
 * 			Selects images 3 through the last.
 * 	rleselect -4
 * 			Selects images 1 through 4.
 * Outputs:
 * 	-o outfile:	The output RLE file.  Default stdout.
 * 			"-" means stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Sort the list of desired image numbers.  (Treat N - infinity
 * 	specially.)  Read images, skipping if they are not in the
 * 	list, and copying if they are.
 */
int
main(int argc, char **argv)
{
    char       *infname = NULL,
    	       *outfname = NULL;
    int        *image_list = NULL;
    int 	oflag = 0,
    		iflag = 0,
		verbose = 0;
    int		nimage = 0;
    int		nsorted = 0;
    int		thrulast = -1;	/* If > 0, take this through last. */
    int		rle_cnt, rle_err, width, y;
    FILE       *outfile = stdout;
    rle_hdr in_hdr, out_hdr;	/* Headers for input and output files. */
    rle_pixel **rows;		/* Will be used for scanline storage. */
    int	       *sorted_list;
    register int i, j;
    
    my_name = cmd_name( argv );

    if ( scanargs( argc, argv,
		   "% i%-infile%s o%-outfile!s v%- image-numbers!*d",
		   &iflag, &infname, &oflag, &outfname, &verbose,
		   &nimage, &image_list ) == 0 )
	exit( 1 );

    if ( nimage == 0 )
    {
	fprintf( stderr, "%s: No images selected.\n", my_name );
	exit( 0 );
    }

    /*
     * Scan the image list, expanding "through" specifications.  Then
     * sort the result.
     */
    for ( i = 0; i < nimage; i++ )
    {
	if ( image_list[i] < 0 )
	{
	    image_list[i] = abs( image_list[i] );
	    if ( i == 0 )
		j = 1;
	    else
		j = image_list[i-1] + 1;
	    for ( ; j <= image_list[i]; j++ )
		insert( &sorted_list, j, nsorted++ );
	}
	else if ( image_list[i] == 0 )
	{
	    if ( i == 0 )
		thrulast = 1;
	    else
		thrulast = image_list[i-1];
	}
	else
	    insert( &sorted_list, image_list[i], nsorted++ );
    }
    /* Optimize list. */
    if ( thrulast >= 0 )
    {
	for ( i = nsorted - 1; i >= 0 && sorted_list[i] >= thrulast; i-- )
	    ;
	nsorted = i + 1;
    }

    if ( verbose )
    {
	fprintf( stderr, "%s: Selecting images", my_name );
	for ( i = 0; i < nsorted; i++ )
	{
	    if ( i % 14 == 0 )
		fprintf( stderr, "\n\t" );
	    fprintf( stderr, "%d%s", sorted_list[i],
		     ((i < nsorted - 1) || thrulast >= 0)  ? ", " : "" );
	}
	if ( thrulast > 0 )
	    fprintf( stderr, "%s%d - last", i % 14 == 0 ? "\n\t" : "",
		     thrulast );
	putc( '\n', stderr );
	fflush( stderr );
    }

	
    /* Open the input file.
     * The output file won't be opened until the first image header
     * has been read.  This avoids unnecessarily wiping out a
     * pre-existing file if the input is garbage.
     */
    in_hdr.rle_file = rle_open_f( my_name, infname, "r" );

    /* Read images from the input file until the end of file is
     * encountered or an error occurs.
     */
    rle_cnt = 0;
    i = 0;
    while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
    {
	/* Open the output file when the first header is successfully read. */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( my_name, outfname, "w" );

	/* Count the input images. */
	rle_cnt++;

	/* If it's not in the list, skip it. */
	while ( rle_cnt > sorted_list[i] && i < nsorted )
	    i++;
	if ( ! (thrulast >= 0 && rle_cnt >= thrulast) &&
	     (i >= nsorted || rle_cnt < sorted_list[i]) )
	{
	    while ( rle_getskip( &in_hdr ) != 32768 )
		;
	    continue;
	}

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
	if ( rle_row_alloc( &out_hdr, &rows ) < 0 )
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

	/* Free memory. */
	rle_row_free( &in_hdr, rows );

	/* Write an end-of-image code. */
	rle_puteof( &out_hdr );
    }
    
    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, my_name, infname );

    exit( 0 );
}


static void 
insert(int **sorted_list_p, int i, int n)
{
    register int *sorted_list = *sorted_list_p;
    register int j;

    if ( n == 0 )
    {
	sorted_list = (int *)malloc( sizeof(int) );
	if ( sorted_list == NULL )
	{
	    fprintf( stderr, "%s: Out of memory\n", my_name );
	    exit( RLE_NO_SPACE );
	}
	*sorted_list = i;
    }
    else
    {
	sorted_list = (int *)realloc( sorted_list, (n + 1) * sizeof(int) );
	if ( sorted_list == NULL )
	{
	    fprintf( stderr, "%s: Out of memory\n", my_name );
	    exit( RLE_NO_SPACE );
	}

	for ( j = n - 1; j >= 0 && sorted_list[j] > i; j-- )
	    sorted_list[j+1] = sorted_list[j];
	sorted_list[j+1] = i;
    }

    *sorted_list_p = sorted_list;
}

