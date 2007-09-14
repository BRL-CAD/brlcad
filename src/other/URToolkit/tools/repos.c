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
 * repos.c - Reposition RLE image.
 *
 * Author:	Rod Bogart & John W. Peterson
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sat Jun 21 1986
 * Copyright (c) 1986, University of Utah
 *
 */
#ifndef lint
static char rcs_ident[] = "$Header$";
#endif

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

char buffer[4096];

int
main(argc, argv)
int	argc;
char	*argv[];
{
    int xlen, ylen, xpos, ypos, posflag=0;
    int oflag = 0, incflag=0, xinc, yinc;
    char * infilename = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    rle_hdr	in_hdr, out_hdr;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if (scanargs(argc, argv,
		 "% p%-xpos!dypos!d P%-xinc!dyinc!d o%-outfile!s infile%s",
                 &posflag, &xpos, &ypos, &incflag, &xinc, &yinc,
		 &oflag, &out_fname, &infilename) == 0)
    {
	exit(-1);
    }

    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infilename, "r" );
    rle_names( &in_hdr, cmd_name( argv ), infilename, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;
	rle_addhist( argv, &in_hdr, &out_hdr );

	xlen = in_hdr.xmax - in_hdr.xmin;
	ylen = in_hdr.ymax - in_hdr.ymin;

	if (posflag)
	{
	    out_hdr.xmin = xpos;
	    out_hdr.ymin = ypos;
	    out_hdr.xmax = xpos + xlen;
	    out_hdr.ymax = ypos + ylen;
	}
	else if (incflag)
	{
	    out_hdr.xmin = in_hdr.xmin + xinc;
	    out_hdr.ymin = in_hdr.ymin + yinc;
	    out_hdr.xmax = in_hdr.xmax + xinc;
	    out_hdr.ymax = in_hdr.ymax + yinc;
	}
	else
	{
	    /* nothing specified on command line, assume -p 0 0 */
	    out_hdr.xmin = 0;
	    out_hdr.ymin = 0;
	    out_hdr.xmax = xlen;
	    out_hdr.ymax = ylen;
	}

	if ((out_hdr.xmin < 0) || (out_hdr.ymin < 0))
	{
	    fprintf(stderr, "Negative boundaries are not allowed!!!\n");
	    exit( -1 );
	}

	rle_put_setup( &out_hdr );

	rle_cp( &in_hdr, &out_hdr );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infilename );

    exit( 0 );
}
