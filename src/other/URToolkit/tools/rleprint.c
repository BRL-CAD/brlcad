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


#include <stdio.h>
#include "rle.h"

#define MALLOC_ERR RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 )

void
main(argc, argv)
int	argc;
char	*argv[];
{
    char *infname = NULL;
    int         i, j;
    rle_hdr 	in_hdr;
    rle_pixel **rows0;
    rle_pixel  *prev;
    int		chan;
    int		rle_cnt, rle_err;
    int         out_alpha = 0, cur_out_alpha, uniq = 0, first;
    
    in_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% a%- u%- infile%s",
		   &out_alpha, &uniq, &infname ) == 0 )
	exit( 1 );
    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infname, "r" );
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( rle_row_alloc( &in_hdr, &rows0 ) < 0 )
	    MALLOC_ERR;
	prev = (rle_pixel *)malloc( (in_hdr.alpha + in_hdr.ncolors) *
				    sizeof(rle_pixel) );
	prev += in_hdr.alpha;
	RLE_CHECK_ALLOC( in_hdr.cmd, prev, NULL );
	first = 1;

	/* output alpha if it exists and is wanted */
	cur_out_alpha = in_hdr.alpha && out_alpha ;

	for ( j = in_hdr.ymin; j <= in_hdr.ymax ; j++ )
	{
	    rle_getrow(&in_hdr, rows0 );
	
	    for ( i = in_hdr.xmin; i <= in_hdr.xmax; i++ )
	    {
		if ( uniq && !first )
		{
		    for ( chan = -cur_out_alpha;
			  chan < in_hdr.ncolors;
			  chan++ )
			if ( rows0[chan][i] != prev[chan] )
			    break;
		    if ( chan >= in_hdr.ncolors )
			continue;
		}
		first = 0;
		for ( chan = 0; chan < in_hdr.ncolors; chan++ )
		{
		    printf( "%d ", rows0[chan][i] );
		    prev[chan] = rows0[chan][i];
		}
		if ( cur_out_alpha )
		{
		    printf( "%d", rows0[RLE_ALPHA][i] );
		    prev[-1] = rows0[RLE_ALPHA][i];
		}
		printf( "\n" );
	    }
	}
	rle_row_free( &in_hdr, rows0 );
	prev -= in_hdr.alpha;
	free( prev );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
