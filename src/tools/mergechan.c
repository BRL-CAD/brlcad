/* 
 * mergechan.c - Merge channels from multiple RLE images
 * 
 * Author:	John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon Nov  9 1987
 * Copyright (c) 1987, University of Utah
 *
 * Warning: this code does not intelligently deal with color maps!
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include "machine.h"
#include "rle.h"
#include "rle_code.h"
#include "rle_raw.h"

#define CHECK_MALLOC( ptr ) \
  { if (! (ptr)) {fprintf( stderr, "%s: Malloc failed\n", cmd_name( argv ) ); \
		      exit(-2);}}
#ifndef MIN
#define MIN(i,j)   ( (i) < (j) ? (i) : (j) )
#endif

#define RLE_END 32768		/* This should be in rle.h */

int
main(int argc, char **argv)
{
    int nfiles;
    rle_hdr * in_hdr;
    rle_hdr out_hdr;
    int alpha_flag = 0, oflag = 0;
    rle_op *** in_rows;		/* A Carl Sagan pointer. */
    int ** in_counts;		/* nraw */
    rle_op ** out_rows;
    int * out_count;
    int * skips;
    const char **filenames, *out_fname = NULL;
    FILE *outfile = stdout;
    int minskip = 0, y;
    register int i;
    int rle_cnt, rle_err, stdin_used = -1;

    if (! scanargs( argc, argv, "% a%- o%-outfile!s files%*s",
		    &alpha_flag, &oflag, &out_fname, &nfiles, &filenames ))
	exit( -1 );

    if (alpha_flag) alpha_flag = 1; /* So indexing's right. */

    in_hdr = (rle_hdr *) malloc( sizeof( rle_hdr )
					    * nfiles );
    
    in_rows = (rle_op ***) malloc( sizeof( rle_op ** ) * nfiles );
    CHECK_MALLOC( in_rows );
    in_counts = (int **) malloc( sizeof( int * ) * nfiles );
    CHECK_MALLOC( in_counts );
    skips = (int *) malloc( sizeof( int ) * nfiles );
    CHECK_MALLOC( skips );

    out_rows = (rle_op **) malloc( sizeof( rle_op * ) * nfiles );
    CHECK_MALLOC( out_rows );
    out_count = (int *) malloc( sizeof( int ) * nfiles );
    CHECK_MALLOC( out_count );
				  
    /* Open all the files, and check consistancy */

    for (i = 0; i < nfiles; i++)
    {
        in_hdr[i].rle_file = rle_open_f("mergechan", filenames[i], "r");
	if ( in_hdr[i].rle_file == stdin )
        {
	    if ( stdin_used < 0 )
	    {
		filenames[i] = "Standard Input";
		stdin_used = i;
	    }
	    else
	    {
		fprintf( stderr,
		 "%s: Images %d and %d are both from the standard input\n",
			 argv[0], stdin_used, i );
		exit( -1 );
	    }
        }
    }

    /* Note: the only way out of this loop is via one of the two exit
     * calls below.
     */
    for ( rle_cnt = 0; ; rle_cnt++ )
    {
	for (i = 0; i < nfiles; i++)
	{
	    /* Check for an error.  EOF or EMPTY is ok if at least one image
	     * has been read.  Otherwise, print an error message.  EOF
	     * or EMPTY after first image means end of input.
	     */
	    if ( (rle_err = rle_get_setup( &in_hdr[i])) != RLE_SUCCESS )
	    {
		if ( rle_cnt == 0 || (rle_err != RLE_EOF &&
				      rle_err != RLE_EMPTY) )
		{
		    rle_get_error( rle_err, cmd_name( argv ), filenames[i] );
		    exit( 1 );
		}
		else if ( rle_err == RLE_EOF || rle_err == RLE_EMPTY )
		    goto out;
	    }

	    /* Check that the channel's really there */
	    if (((in_hdr[i].ncolors-1) < (i-alpha_flag)) ||
		(! RLE_BIT( in_hdr[i], i-alpha_flag )))
	    {
		fprintf(stderr, "mergechan: channel %d not in file %s\n",
			i, filenames[i] );
		exit( -2 );
	    }

	    /* Check to make sure all images have the same size */
	    if (i > 0)
	    {
		if (! ((in_hdr[0].xmin == in_hdr[i].xmin) &&
		       (in_hdr[0].xmax == in_hdr[i].xmax) &&
		       (in_hdr[0].ymin == in_hdr[i].ymin) &&
		       (in_hdr[0].ymax == in_hdr[i].ymax)) )
		{
		    fprintf(stderr,
		    "mergechan: image %s is not the same size as image %s\n",
			    filenames[i], filenames[0] );
		    exit( -2 );
		}
	    }

	    if( rle_raw_alloc( &(in_hdr[i]), &(in_rows[i]), &(in_counts[i]) ))
	    {
		fprintf( stderr, "mergechan: can't allocate raw storage\n" );
		exit(-2);
	    }
	}

	/* Setup output stuff */

	out_hdr = in_hdr[0];
	rle_addhist( argv, &in_hdr[0], &out_hdr );

	if ( rle_cnt == 0 )
	    outfile = rle_open_f("mergechan", out_fname, "w");
	out_hdr.rle_file = outfile;

	out_hdr.ncolors = nfiles - alpha_flag;
	out_hdr.alpha = alpha_flag;
    
	/* Enable all output channels. */
	for (i = -alpha_flag; i < out_hdr.ncolors; i++)
	    RLE_SET_BIT( out_hdr, i );

	rle_put_setup( &out_hdr );


	if (alpha_flag)		/* So indexing's right (alpha == -1) */
	{
	    in_hdr++;
	    in_rows++;
	    in_counts++;

	    out_rows++;
	    out_count++;
	    skips++;
	}

	/* Initialize counters */
	for (i = -alpha_flag; i < out_hdr.ncolors; i++)
	{
	    skips[i] = 0;
	    out_rows[i] = in_rows[i][i];
	}
	y = out_hdr.ymin - 1;	/* -1 'cuz we haven't read data yet */

	/*
	 * Do the actual work.  Since rle_getraw may "skip" several lines
	 * ahead, we need to keep track of the Y position of each channel
	 * independently with skips[].  The output moves ahead by the
	 * minimum of these skip values (minskip).
	 */
	while (1)		/* Stops at EOF on all files */
	{
	    for (i = -alpha_flag; i < out_hdr.ncolors; i++)
	    {
		if (! skips[i])
		{
		    skips[i] = rle_getraw( &(in_hdr[i]),
					   in_rows[i], in_counts[i] );
		    if (skips[i] != RLE_END)
			skips[i] -= y;	/* Store delta to next data */
		}

		/* Find smallest skip distance until a channel has data again */
		if (i == -alpha_flag)
		    minskip = skips[i];
		else
		    minskip = MIN( skips[i], minskip );
	    }

	    if (minskip == RLE_END)
		break;		/* Hit the end of all input files */

	    if (minskip > 1)
		rle_skiprow( &out_hdr, minskip-1 );

	    y += minskip;

	    for (i = -alpha_flag; i < out_hdr.ncolors; i++)
	    {
		if (skips[i] != RLE_END) skips[i] -= minskip;

		if (skips[i] == 0)	/* Has data to go out */
		{
		    out_count[i] = in_counts[i][i];
		}
		else
		{
		    out_count[i] = 0;
		}
	    }
	
	    rle_putraw( out_rows, out_count, &out_hdr );

	    for (i = -alpha_flag; i < out_hdr.ncolors; i++)
	    {
		if (skips[i] == 0)	/* Data is written, so free the raws */
		{
		    rle_freeraw( &(in_hdr[i]), in_rows[i], in_counts[i] );
		}
	    }
	}

	rle_puteof( &out_hdr );

	/* Free storage. */
	if (alpha_flag)
	{
	    in_hdr--;
	    in_rows--;
	    in_counts--;

	    out_rows--;
	    out_count--;
	    skips--;
	}
	for ( i = 0; i < nfiles; i++ )
	    rle_raw_free( &in_hdr[i], in_rows[i], in_counts[i] );
    }
out:
    exit(0);
}
