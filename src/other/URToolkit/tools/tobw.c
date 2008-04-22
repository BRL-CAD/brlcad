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
 * tobw.c - Filter to change an RLE file to B&W.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Tue Feb 25 1986
 * Copyright (c) 1986, University of Utah
 *
 */
#ifndef lint
static char rcs_ident[] = "$Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "rle.h"

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

/*****************************************************************
 * TAG( main )
 *
 * Usage: tobw [-t] [-o outfile] [infile]
 *
 * Inputs:
 *	-t:		If specified, output a 3-channel image (all three
 *			the same).
 * 	infile:		Input (color) RLE file.  Stdin used if not
 *			specified.
 * Outputs:
 * 	outfile:	Output (black & white) RLE file.  Stdout used
 *			if not specified.
 * Assumptions:
 *	[None]
 * Algorithm:
 *	[None]
 */
int
main( argc, argv )
int argc;
char **argv;
{
    char * infname = NULL, * outfname = NULL;
    FILE * outfile = stdout;
    int oflag = 0, tflag = 0, nrow;
    rle_hdr out_hdr;
    unsigned char ** scan, *outscan[4];
    unsigned char * buffer;
    int rle_cnt, rle_err;

    if ( scanargs( argc, argv, "% t%- o%-outfile!s infile%s",
		    &tflag, &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    rle_dflt_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &rle_dflt_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( rle_dflt_hdr.ncolors == 1 )
	{
	    fprintf( stderr, "%s: %s is already black & white\n",
		     cmd_name( argv ), infname ? infname : "stdin" );
	    exit( 1 );
	}
	if ( rle_dflt_hdr.ncolors < 3 )
	{
	    fprintf( stderr, "%s: %s is not RGB",
		     cmd_name( argv ), infname ? infname : "stdin" );
	    exit( 1 );
	}
	if ( rle_dflt_hdr.alpha )
	    RLE_SET_BIT( rle_dflt_hdr, RLE_ALPHA );

	out_hdr = rle_dflt_hdr;
	if ( rle_cnt == 0 )
	    outfile = rle_open_f(cmd_name( argv ), outfname, "w");
	rle_dflt_hdr.xmax -= rle_dflt_hdr.xmin;
	rle_dflt_hdr.xmin = 0;
	nrow = rle_dflt_hdr.xmax + 1;
	buffer = (unsigned char *)malloc( nrow );
	rle_row_alloc( &rle_dflt_hdr, &scan );
	if ( rle_dflt_hdr.alpha )
	{
	    outscan[0] = scan[-1];
	}
	outscan[1] = buffer;
	/* If 3 channel output, reference the line 3 times */
	if ( tflag )
	    outscan[2] = outscan[3] = buffer;

	if ( ! tflag )
	    out_hdr.ncolors = 1;
	else
	    out_hdr.ncolors = 3;

	if ( rle_dflt_hdr.background != 0 )
	{
	    rle_pixel r, g, b, o;

	    out_hdr.bg_color = (int *)malloc( sizeof( int ) );
	    r = rle_dflt_hdr.bg_color[0];
	    g = rle_dflt_hdr.bg_color[1];
	    b = rle_dflt_hdr.bg_color[2];
	    rgb_to_bw( &r, &g, &b, &o, 1 );
	    out_hdr.bg_color[0] = o;
	}

	rle_addhist( argv, &rle_dflt_hdr, &out_hdr );

	out_hdr.rle_file = outfile;

	rle_put_setup( &out_hdr );

	while ( rle_getrow( &rle_dflt_hdr, scan ) <= rle_dflt_hdr.ymax )
	{
	    rgb_to_bw( scan[0], scan[1], scan[2], buffer, nrow );
	    rle_putrow( &outscan[1], nrow, &out_hdr );
	}
	rle_puteof( &out_hdr );

	rle_row_free( &rle_dflt_hdr, scan );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
