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
 * applymap.c - Apply the color map in an RLE file to the pixel data.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Jul  8 1986
 * Copyright (c) 1986, Spencer W. Thomas
 */

#include <stdio.h>
#include <rle.h>
#include <rle_raw.h>
#ifdef USE_STDLIB_H
#include <stdlib.h>
#else

#ifdef VOID_STAR
extern void *malloc();
#else
extern char *malloc();
#endif
extern void free();

#endif /* USE_STDLIB_H */

#define	map_pixel( pix, cmaplen, cmap )	((pix) > cmaplen ? (pix) : \
					 (cmap[pix]) >> 8)

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *  	applymap [-l] [-o outfile] [rlefile]
 * Inputs:
 *	-l:		If specified, a linear map will be placed in the
 *			output file.
 * 	rlefile:    	Input file to have its map applied to its pixels.
 *  	    	    	Defaults to stdin.
 * Outputs:
 * 	outfile:    	Result of applying the map in the input file.
 *  	    	    	Defaults to stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
void
main( argc, argv )
int argc;
char **argv;
{
    register int i, c, j;
    char * infname = NULL, * outfname = NULL;
    FILE *outfile = stdout;
    int oflag = 0, y, nskip, nrow, one_to_many = 0, lflag = 0;
    int	rle_cnt, rle_err;
    rle_hdr in_hdr, out_hdr;
    rle_op ** scan, ** outscan;
    int * nraw;
    int cmaplen;
    rle_map ** cmap = NULL, *linmap = NULL, *mapp;

    if ( scanargs( argc, argv, "% l%- o%-outfile!s infile%s",
		   &lflag, &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
    /* Open input file and read RLE header */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Copy header data from the input file */
	out_hdr = in_hdr;
	rle_addhist( argv, &in_hdr, &out_hdr );

	if ( rle_cnt == 0 )
	    /* Open the output file */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );
	out_hdr.rle_file = outfile;

	nrow = in_hdr.xmax - in_hdr.xmin + 1;	/* how wide is it? */

	if ( in_hdr.ncmap > 0 )
	{
	    cmaplen = 1 << in_hdr.cmaplen;	/* length of the color map */
	    /* Get pointers to the individual rows of the color map */
	    cmap = (rle_map **) malloc( in_hdr.ncmap * sizeof(rle_map *) );
	    for ( c = 0; c < in_hdr.ncmap; c++ )
		cmap[c] = &in_hdr.cmap[c * cmaplen];
	}
	else
	    cmaplen = 0;

	/* If the input file has only one channel, and has more than one
	 * color map channel, then do a one to many mapping.
	 */
	if ( in_hdr.ncolors == 1 && in_hdr.ncmap > 1 )
	{
	    one_to_many = 1;
	    out_hdr.ncolors = in_hdr.ncmap;
	    for ( i = 1; i < out_hdr.ncolors; i++ )
		RLE_SET_BIT( out_hdr, i );	/* save all these colors */
	}

	/* If -l, create a linear color mapfor the output file, otherwise,
	 * it gets no map.
	 */
	if ( lflag )
	{
	    linmap = (rle_map *)malloc( cmaplen * out_hdr.ncolors *
					sizeof(rle_map) );
	    out_hdr.ncmap = out_hdr.ncolors;
	    out_hdr.cmap = linmap;
	    for ( c = 0, mapp = linmap; c < out_hdr.ncolors; c++ )
		for ( i = 0; i < cmaplen; i++ )
		    *mapp++ = i << (16 - in_hdr.cmaplen); /* right justify */
	}
	else
	{
	    out_hdr.ncmap = 0;	/* output file won't have a map */
	    out_hdr.cmap = NULL;
	}

	/* Allocate space for the rle opcode information */
	scan = (rle_op **) malloc( (in_hdr.ncolors + in_hdr.alpha) *
				   sizeof( rle_op * ) );
	for ( i = in_hdr.ncolors + in_hdr.alpha - 1;
	      i >= 0;
	      i-- )
	    scan[i] = (rle_op *)malloc( (nrow / 3 + 1) * sizeof( rle_op ) );

	outscan = (rle_op **) malloc( (out_hdr.ncolors + out_hdr.alpha) *
				      sizeof( rle_op * ) );

	if ( one_to_many )
	{
	    for ( i = out_hdr.ncolors + out_hdr.alpha - 1;
		  i >= out_hdr.alpha;
		  i-- )
		outscan[i] = (rle_op *)malloc( (nrow / 3 + 1) *
					       sizeof( rle_op ) );
	    if ( out_hdr.alpha )
		outscan[0] = scan[0];
	    /* Map background color */
	    if ( in_hdr.ncmap > 0 && in_hdr.background )
	    {
		out_hdr.bg_color = (int *)malloc( out_hdr.ncolors *
						  sizeof(int) );
		for ( i = 0; i < out_hdr.ncolors; i++ )
		    out_hdr.bg_color[i] = map_pixel( in_hdr.bg_color[0],
						     cmaplen, cmap[i] );
	    }
	}
	else
	{
	    for ( i = out_hdr.ncolors + out_hdr.alpha - 1;
		  i >= 0;
		  i-- )
		outscan[i] = scan[i];
	    if ( in_hdr.ncmap > 0 && in_hdr.background )
		for ( i = 0; i < out_hdr.ncolors; i++ )
		    in_hdr.bg_color[i] = map_pixel( in_hdr.bg_color[i],
						    cmaplen, cmap[i] );
	}

	nraw = (int *) malloc( (out_hdr.ncolors + out_hdr.alpha) *
			       sizeof( int ) );

	if ( in_hdr.alpha )
	{
	    scan++;		/* [-1] points to the alpha channel */
	    outscan++;
	    nraw++;
	}

	/* Start the output file */
	rle_put_setup( &out_hdr );

	y = in_hdr.ymin - 1;
	while ( (nskip = rle_getraw( &in_hdr, scan, nraw )) != 32768 )
	{
	    nskip -= y;		/* figure out difference from previous line */
	    y += nskip;
	    if ( nskip > 1 )
		rle_skiprow( &out_hdr, nskip - 1 );
	    if ( in_hdr.ncmap > 0 )
		if ( one_to_many )
		{
		    for ( c = 1; c < out_hdr.ncolors; c++ )
			nraw[c] = nraw[0];	/* all the same length */

		    for ( i = 0; i < nraw[0]; i++ )
			switch( scan[0][i].opcode )
			{
			case RRunDataOp:
			    for ( c = 0; c < out_hdr.ncolors;
				  c++ )
			    {
				outscan[c][i] = scan[0][i];
				outscan[c][i].u.run_val =
				    map_pixel( scan[0][i].u.run_val,
					       cmaplen, cmap[c] );
			    }
			    break;
			case RByteDataOp:
			    for ( c = 0; c < out_hdr.ncolors;
				  c++ )
			    {
				outscan[c][i] = scan[0][i];
				outscan[c][i].u.pixels =
				    (rle_pixel *)malloc( outscan[c][i].length *
							 sizeof (rle_pixel) );
				for ( j = 0; j < outscan[c][i].length;
				      j++ )
				    outscan[c][i].u.pixels[j] = 
					map_pixel( scan[0][i].u.pixels[j],
						   cmaplen, cmap[c] );
			    }
			    break;
			}
		}
		else
		{
		    for ( c = 0; c < in_hdr.ncolors; c++ )
			if ( c < in_hdr.ncmap )
			    for ( i = 0; i < nraw[c]; i++ )
				switch( scan[c][i].opcode )
				{
				case RRunDataOp:
				    scan[c][i].u.run_val =
					map_pixel( scan[c][i].u.run_val,
						   cmaplen, cmap[c] );
				    break;
				case RByteDataOp:
				    for ( j = 0; j < scan[c][i].length;
					  j++ )
					scan[c][i].u.pixels[j] = 
					    map_pixel( scan[c][i].u.pixels[j],
						       cmaplen, cmap[c]);
				    break;
				}
		}
	    rle_putraw( outscan, nraw, &out_hdr );
	    if ( one_to_many )
		rle_freeraw( &out_hdr, outscan, nraw );
	    rle_freeraw( &in_hdr, scan, nraw );
	}
	rle_puteof( &out_hdr );

	if ( in_hdr.ncmap > 0 )
	    free( cmap );
	if ( lflag )
	    free( linmap );
	if ( in_hdr.alpha )
	{
	    scan--;		/* [-1] points to the alpha channel */
	    outscan--;
	    nraw--;
	}
	for ( i = in_hdr.ncolors + in_hdr.alpha - 1; i >= 0; i-- )
	    free( scan[i] );
	free( scan );
	if ( one_to_many )
	{
	    for ( i = out_hdr.ncolors + out_hdr.alpha - 1;
		  i >= out_hdr.alpha;
		  i-- )
		free( outscan[i] );
	    free( out_hdr.bg_color );
	}
	free( outscan );
	free( nraw );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
