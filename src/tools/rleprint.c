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
 * rleprint.c - Print all the pixel values in an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Tue Jun  5 1990
 * Copyright (c) 1990, University of Michigan
 */

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "rle.h"

#define MALLOC_ERR {fprintf(stderr, "%s: ran out of heap space\n", \
                            cmd_name( argv ));exit(-2);}

int
main(int argc, char **argv)
{
    char *infname = NULL;
    int         i, j;
    rle_hdr in_hdr;
    rle_pixel **rows0;
    int		chan;
    int		rle_cnt, rle_err;
    
    if ( scanargs( argc, argv, "% infile%s", &infname ) == 0 )
	exit( 1 );
    in_hdr.rle_file = rle_open_f( "rleprint", infname, "r" );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if (rle_row_alloc( &in_hdr, &rows0 ))
	    MALLOC_ERR;

	for ( j = in_hdr.ymin; j <= in_hdr.ymax ; j++ )
	{
	    rle_getrow(&in_hdr, rows0 );
	
	    for ( i = in_hdr.xmin; i <= in_hdr.xmax; i++ )
	    {
		for ( chan = 0; chan < in_hdr.ncolors; chan++ )
		    printf( "%d ", rows0[chan][i] );
		printf( "\n" );
	    }
	}
	rle_row_free( &in_hdr, rows0 );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
