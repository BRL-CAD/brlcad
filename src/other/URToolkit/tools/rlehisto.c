/*
 * rlehisto.c - Create histogram image of an RLE file.
 *
 * Author:	Gregg Townsend
 *		Department of Computer Science
 *		University of Arizona
 * Date:	June 23, 1990
 *
 * Original version:
 * Author:	Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Thu Nov  6 1986
 * Copyright (c) 1986 Rod Bogart
 *
 * Flags:
 *    -b	Don't count background color values for scaling.
 *    		Ineffective when -c specified.
 *    -c	Output cumulative values instead of discrete values.
 *    -t	Print totals of each value in each channel.
 *    -h npix	Set height of image.
 *    -o fname	Direct output to file.
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

#define MAXCHAN 10

int
main(argc, argv)
int	argc;
char	*argv[];
{
    FILE *outfile = stdout;
    int i, j, bflag=0, cflag=0, tflag=0, oflag=0;
    int hist_height = 256;
    rle_hdr 	in_hdr, out_hdr;
    rle_pixel ** rows, ** rowsout;
    rle_pixel *pixptr;
    long *pixelcount[256];
    long maxcount;
    long n;
    int	chan, nchan;
    int rle_cnt, rle_err;
    char *infname = NULL, *outfname = NULL;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
		   "% b%- c%- t%- h%-height!d o%-outfile!s infile%s",
		   &bflag, &cflag, &tflag,
		   &i, &hist_height,
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    in_hdr.rle_file  = rle_open_f(cmd_name( argv ), infname, "r");
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, outfname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(cmd_name( argv ), outfname, "w");

	/* Only pay attention to bflag if background color is defined. */
	bflag = (bflag && in_hdr.bg_color != NULL);

	in_hdr.xmax -= in_hdr.xmin;
	in_hdr.xmin = 0;

	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.cmap = (rle_map *)NULL;
	out_hdr.ncmap = 0;
	out_hdr.cmaplen = 0;
	out_hdr.background = 2;
	out_hdr.bg_color = (int *)calloc( in_hdr.ncolors, sizeof(int) );
	RLE_CHECK_ALLOC( cmd_name( argv ), out_hdr.bg_color,
			 "background" );
	out_hdr.alpha = 0;
	out_hdr.xmin = 0;
	out_hdr.xmax = 255;
	out_hdr.ymin = 0;
	out_hdr.ymax = hist_height - 1;
	out_hdr.rle_file = outfile;

	nchan = in_hdr.ncolors;

	if (!tflag)
	{
	    rle_addhist( argv, &in_hdr, &out_hdr );
	    rle_put_setup( &out_hdr );
	}

	if ( rle_row_alloc( &out_hdr, &rowsout ) < 0 ||
	     rle_row_alloc( &in_hdr, &rows ) < 0 )
	    RLE_CHECK_ALLOC( cmd_name( argv ), 0, 0 );

	for ( j = 0; j < 256; j++)
	{
	    if ( rle_cnt == 0 )
	    {
		pixelcount[j] = (long *) malloc(sizeof(long) * nchan);
		RLE_CHECK_ALLOC( cmd_name( argv ), pixelcount[j], 0 );
	    }
	    for (chan=0; chan < nchan; chan++)
	    {
		pixelcount[j][chan] = 0;
	    }
	}
	maxcount = 0;

	for (j=in_hdr.ymin; j <= in_hdr.ymax; j++)
	{
	    rle_getrow(&in_hdr, rows);
	    for (chan=0; chan < nchan; chan++)
	    {
		pixptr = rows[chan];
		for (i=0; i < in_hdr.xmax + 1; i++)
		    pixelcount[ *pixptr++ ][ chan ] += 1;
	    }
	}

	/* create cumulative figures if those are wanted. */
	if (cflag)
	{
	    for (chan = 0; chan < nchan; chan++)
	    {
		for (j = 1; j < 256; j++)
		    pixelcount[j][chan] += pixelcount[j-1][chan];
		if (pixelcount[255][chan] > maxcount)
		    maxcount = pixelcount[255][chan];
	    }
	}
	else
	    for ( chan = 0; chan < nchan; chan++ )
		for ( j = 0; j < 256; j++ )
		    if ( bflag && j == in_hdr.bg_color[chan] )
			continue;
		    else
			if ( pixelcount[j][chan] > maxcount )
			    maxcount = pixelcount[j][chan];

	/* after entire image has been read in, output the histogram */

	if (tflag)
	{
	    if ( rle_cnt > 0 )
		fprintf( outfile, "\n\n" );
	    for (j = 0; j < 256; j++)
	    {
		for (chan = 0; chan < nchan; chan++)
		    if (j > 0 && cflag) {
			if (pixelcount[j][chan] != pixelcount[j-1][chan])
			    break;
		    } else {
			if (pixelcount[j][chan] != 0)
			    break;
		    }
		if (chan == nchan)     /* if all entries zero, suppress line */
		    continue;
		fprintf(outfile, "%3d.", j);
		for (chan = 0; chan < nchan; chan++)
		    fprintf(outfile, "\t%ld", pixelcount[j][chan]);
		fprintf(outfile, "\n");
	    }
	}
	else
	{
	    for (i = 0; i < hist_height; i++)
	    {
		n = (maxcount * i) / (hist_height - 2);
		for (chan = 0; chan < nchan; chan++)
		{
		    for (j = 0; j < 256; j++)
		    {
			if (pixelcount[j][chan] > n)
			    rowsout[chan][j] = 255;
			else
			    rowsout[chan][j] = 0;
		    }
		}
		rle_putrow( rowsout, 256, &out_hdr);
	    }
	    rle_puteof( &out_hdr );
	}

	/* Free memory. */
	rle_row_free( &out_hdr, rowsout );
	rle_row_free( &in_hdr, rows );
    }
    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	 rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
