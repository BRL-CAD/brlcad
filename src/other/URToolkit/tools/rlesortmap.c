/*
 * rlesortmap.c - Sort the colormap using a n-d peano curve.
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Fri Feb 15 1991
 * Copyright (c) 1991, University of Michigan
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

#define	map_pixel( pix, cmaplen, cmap )	((pix) > cmaplen ? (pix) : \
					 cmap[pix])

/* This is global so that the pcompar function can see it. */
static rle_map ** cmap = NULL;
static int ncolor;

/* pcompar compares two colormap entries using the Peano ordering. */
extern int pcompar();

/*****************************************************************
 * TAG( main )
 *
 * Usage:
 * 	rlesortmap [-o outfile] [infile]
 * Inputs:
 * 	infile:		Input file to have its colormap sorted.
 * 			Defaults to stdin.
 * Outputs:
 * 	outfile:	Result of sorting the colormap.
 * 			Defaults to stdout.
 * Assumptions:
 *	[None]
 * Algorithm:
 * 	Uses a Peano curve ordering to sort the elements in the input
 * 	colormap.  Keeps track of where each original element went,
 * 	and then remaps the input data accordingly.  This makes most
 * 	sense for pseudo-color input files, but the program will work
 * 	on any input file.
 */
int
main( argc, argv )
int argc;
char **argv;
{
    register int i, c, j;
    char * infname = NULL, * outfname = NULL;
    FILE *outfile = stdout;
    int oflag = 0, y, nskip, nrow;
    int	rle_cnt, rle_err;
    rle_hdr 	in_hdr, out_hdr;
    rle_op ** scan, ** outscan;
    int * nraw;
    int cmaplen;
    rle_map *mapp, **scmap;
    int *sortindex, *isort;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% o%-outfile!s infile%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );
    /* Open input file and read RLE header */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Copy header data from the input file */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	rle_addhist( argv, &in_hdr, &out_hdr );

	/* Open the output file */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), outfname, "w" );
	out_hdr.rle_file = outfile;

	nrow = in_hdr.xmax - in_hdr.xmin + 1;	/* how wide is it? */

	if ( in_hdr.ncmap > 0 )
	{
	    char *map_cmt;

	    cmaplen = 1 << in_hdr.cmaplen;	/* length of the color map */
	    /* Get pointers to the individual rows of the color map */
	    cmap = (rle_map **) malloc( in_hdr.ncmap * sizeof(rle_map *) );
	    RLE_CHECK_ALLOC( in_hdr.cmd, cmap, "color map" );
	    for ( c = 0; c < in_hdr.ncmap; c++ )
		cmap[c] = &in_hdr.cmap[c * cmaplen];

	    /* Pointers for storing sorted colormap. */
	    scmap = (rle_map **) malloc( in_hdr.ncmap * sizeof(rle_map *) );
	    mapp = (rle_map *)malloc( in_hdr.ncmap * cmaplen * sizeof(rle_map) );
	    RLE_CHECK_ALLOC( in_hdr.cmd, scmap && mapp, "color map" );
	    for ( c = 0; c < in_hdr.ncmap; c++ )
		scmap[c] = &mapp[c * cmaplen];

	    /* If usable colormap length given, use it. */
	    if ( (map_cmt = rle_getcom( "color_map_length", &in_hdr )) &&
		 atoi( map_cmt ) > 0 )
		cmaplen = atoi( map_cmt );
	}
	else
	    cmaplen = 0;

	if ( cmaplen > 0 )
	{
	    /* Allocate space for the sort indices. */
	    sortindex = (int *)malloc( sizeof(int) * cmaplen );
	    RLE_CHECK_ALLOC( in_hdr.cmd, sortindex, 0 );

	    /* And initialize them to the identity mapping. */
	    for ( i = 0; i < cmaplen; i++ )
		sortindex[i] = i;

	    ncolor = in_hdr.ncmap;
	    /* Do the sort. */
	    qsort( sortindex, cmaplen, sizeof(int), pcompar );

	    /* Compute the inverse sort map. */
	    isort = (int *)malloc( sizeof(int) * cmaplen );
	    RLE_CHECK_ALLOC( in_hdr.cmd, isort, 0 );
	    for ( i = 0; i < cmaplen; i++ )
		isort[sortindex[i]] = i;

#if 0
	    /* Collapse identical entries. */
	    for ( i = 1, j = 0; i < cmaplen; i++ )
	    {
		int xi, xi1;
		xi = sortindex[i];
		xi1 = sortindex[i-1];

		for ( c = 0; c < ncolor; c++ )
		    if ( cmap[c][xi] != cmap[c][xi1] )
			break;
		/* If loop broke, colors are different. */
		if ( c != ncolor )
		    j++;
		if ( i != j )
		    sortindex[j] = sortindex[i];
	    }
	    cmaplen = j + 1;
#endif

	    /* Rearrange the physical colormap. */
	    for ( i = 0; i < cmaplen; i++ )
	    {
		int xi = sortindex[i];

		for ( c = 0; c < ncolor; c++ )
		    scmap[c][i] = cmap[c][xi];
	    }

	    /* And put the sorted colormap into the output file. */
	    out_hdr.cmap = mapp;
	    if ( cmaplen < (1 << out_hdr.cmaplen ) )
	    {
		char buf[80];

		sprintf( buf, "color_map_length=%d", cmaplen );
		rle_putcom( buf, &out_hdr );
	    }
	}

	/* Allocate space for the rle opcode information */
	if ( rle_raw_alloc( &in_hdr, &scan, &nraw ) < 0 )
	    RLE_CHECK_ALLOC( in_hdr.cmd, 0, "image data" );

	if ( in_hdr.ncmap > 0 && in_hdr.background )
	    for ( i = 0; i < out_hdr.ncolors; i++ )
		in_hdr.bg_color[i] = map_pixel( in_hdr.bg_color[i],
						cmaplen, isort );

	/* Start the output file */
	rle_put_setup( &out_hdr );

	y = in_hdr.ymin - 1;
	while ( (nskip = rle_getraw( &in_hdr, scan, nraw )) != 32768 )
	{
	    nskip -= y;		/* figure out difference from previous line */
	    y += nskip;
	    if ( nskip > 1 )
		rle_skiprow( &out_hdr, nskip - 1 );
	    if ( cmaplen > 0 )
	    {
		for ( c = 0; c < in_hdr.ncolors; c++ )
		    if ( c < in_hdr.ncmap )
			for ( i = 0; i < nraw[c]; i++ )
			    switch( scan[c][i].opcode )
			    {
			    case RRunDataOp:
				scan[c][i].u.run_val =
				    map_pixel( scan[c][i].u.run_val,
					       cmaplen, isort );
				break;
			    case RByteDataOp:
				for ( j = 0; j < scan[c][i].length;
				      j++ )
				    scan[c][i].u.pixels[j] =
					map_pixel( scan[c][i].u.pixels[j],
						   cmaplen, isort);
				break;
			    }
	    }
	    rle_putraw( scan, nraw, &out_hdr );
	    rle_freeraw( &in_hdr, scan, nraw );
	}
	rle_puteof( &out_hdr );

	if ( in_hdr.ncmap > 0 )
	{
	    free( cmap );
	    free( scmap );
	    free( mapp );
	}

	rle_raw_free( &in_hdr, scan, nraw );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}

/*****************************************************************
 * TAG( pcompar )
 *
 * Compares the indicated colormap entries using the Peano ordering.
 * Inputs:
 * 	a, b:	Indices into the sortindex array (which indexes the
 * 		colormap itself.)
 * Outputs:
 * 	Returns -1 if cmap[sortindex[a]] < cmap[sortindex[b]],
 * 		0 if they are equal, and
 * 		1 if a ">" b.
 * Assumptions:
 *	(global) ncolors <= 4.
 * Algorithm:
 *	Uses the Peano curve ordering for n=ncolors and m=8 to get a
 *	linear ordering for the n-dimensional colors.
 */
int
pcompar( ap, bp )
int *ap, *bp;
{
    int a = *ap, b = *bp;
    long int ai, bi;
    int c[4];

    switch( ncolor )
    {
    case 4:	c[3] = cmap[3][a] >> 8;
    case 3:	c[2] = cmap[2][a] >> 8;
    case 2:	c[1] = cmap[1][a] >> 8;
    case 1:	c[0] = cmap[0][a] >> 8;
    }
    hilbert_c2i( ncolor, 8, c, &ai );

    switch( ncolor )
    {
    case 4:	c[3] = cmap[3][b] >> 8;
    case 3:	c[2] = cmap[2][b] >> 8;
    case 2:	c[1] = cmap[1][b] >> 8;
    case 1:	c[0] = cmap[0][b] >> 8;
    }
    hilbert_c2i( ncolor, 8, c, &bi );

    if ( ai < bi )
	return -1;
    else if ( ai == bi )
	return 0;
    else
	return 1;
}
