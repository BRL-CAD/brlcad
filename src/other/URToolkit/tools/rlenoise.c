/* 
 * rlenoise.c - Add random (uniform) noise to an image.
 * 
 * Copyright 1988
 * Center for Information Technology Integration (CITI)
 * Information Technology Division
 * University of Michigan
 * Ann Arbor, Michigan
 *
 * Spencer W. Thomas
 *
 * Usage:
 * 	rlenoise [-a amount] [file]
 *
 * Adds random noise to an image.  May be useful when the image will
 * be output to a quantized display, and the display program doesn't
 * understand how to compensate.  (That's why I wrote it, anyway.)
 * Optionally, can specify the amount of noise (peak-to-peak) to add.
 * Default is 4 (because that's what I needed.)
 */

#include <stdio.h>
#include "rle.h"

/* Change this according the value on your system.  This is a crock. */
#define	RANDOM_MAX	((double)(int)(((unsigned int)~0)>>1))

#define MALLOC_ERR RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 )

void
main( argc, argv )
int argc;
char ** argv;
{
    int x, y, i;
    int oflag = 0, noise_amount = 4;
    char * in_fname = NULL, * out_fname = NULL;
    FILE *outfile = stdout;
    rle_hdr 	in_hdr, out_hdr;
    rle_pixel **inrows, **outrows;
    double rand_mult, newpix;
    int rle_err, rle_cnt;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

#ifdef USE_RANDOM
    /* Use the BSD random() function */
    {
	long seed;
	(void)time( &seed );
	srandom( *(int *)&seed );
    }
#else
    /* use the standard Unix rand function if nothing better */
    {
	long seed;
	(void)time( &seed );
	srand( *(int *)&seed );
    }
#define random rand
#endif

    if ( scanargs( argc, argv, "% n%-amount!d o%-outfile!s infile%s",
		   &noise_amount, &noise_amount, &oflag, &out_fname, &in_fname) == 0 )
	exit( 1 );

    rand_mult = noise_amount / RANDOM_MAX;
    noise_amount /= 2;

    in_hdr.rle_file = rle_open_f(cmd_name( argv ), in_fname, "r");
    rle_names( &in_hdr, cmd_name( argv ), in_fname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );
   
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Always write to stdout, well almost always */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	/* Init output file */
	rle_put_setup( &out_hdr );

	/* Get some memory */
	if ( rle_row_alloc( &in_hdr, &inrows ) < 0 )
	    MALLOC_ERR;
	/* Set up the output pointers.  This is a pain. */
	if ( (outrows = ((rle_pixel **) malloc(sizeof(rle_pixel *)
					       * out_hdr.ncolors + 1 ) + 1))
	     == NULL )
	    MALLOC_ERR;
	for ( i = -out_hdr.alpha; i < out_hdr.ncolors; i++ )
	    if ( inrows[i] )
		outrows[i] = inrows[i] + out_hdr.xmin;

	for ( y = in_hdr.ymin; y <= in_hdr.ymax; y++ )
	{
	    rle_getrow( &in_hdr, inrows );
	    for ( i = 0; i < in_hdr.ncolors; i++ )
		if ( inrows[i] != NULL )
		    for ( x = in_hdr.xmin; x <= in_hdr.xmax; x++ )
		    {
			newpix = inrows[i][x] + random() * rand_mult -
			    noise_amount + 0.5;
			if ( newpix < 0 )
			    newpix = 0;
			if ( newpix > 255 )
			    newpix = 255;
			inrows[i][x] = newpix;
		    }
	    rle_putrow( outrows, out_hdr.xmax - out_hdr.xmin + 1,
			&out_hdr );
	}

	rle_puteof( &out_hdr );

	/* Free memory. */
	rle_row_free( &in_hdr, inrows );
	free( outrows - 1 );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), in_fname );

    exit( 0 );
}
