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
 * rlesetbg.c - set the background color (based on repos)
 * 
 * Author:	John W. Peterson & Rod Bogart
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sat Jun 21 1986
 * Copyright (c) 1986, University of Utah
 * 
 */
#ifndef lint
static char rcs_ident[] = "$Id$";
#endif

#include <stdio.h>
#include "rle.h"

char buffer[4096];

void
main(argc, argv)
int	argc;
char	*argv[];
{
    int *new_back = NULL, nback = 0;
    int overlay_flag = 0, backcolor_flag = 0, oflag = 0;
    rle_hdr 	in_hdr, out_hdr;
    char * fname = NULL, *out_fname = NULL;
    FILE *outfile = stdout;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if (scanargs(argc, argv,
		 "% DO%- c%-bgcolor!*d o%-outfile!s infile%s",
		 &overlay_flag, &backcolor_flag,
		 &nback, &new_back,
		 &oflag, &out_fname, &fname ) == 0)
    {
	exit(-1);
    }
    if (overlay_flag == 2 && backcolor_flag)
    {
	fprintf(stderr, "%s: Delete or new color, but not both\n",
		cmd_name( argv ));
	exit(-1);
    }
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), fname, "r");
    rle_names( &in_hdr, cmd_name( argv ), fname, 0 );
    rle_names( &out_hdr, in_hdr.cmd, out_fname, 0 );

    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	if ( backcolor_flag && nback < in_hdr.ncolors )
	{
	    fprintf( stderr, "%s: Need %d colors, only %d supplied.\n",
		     in_hdr.ncolors, nback );
	    exit(-1);
	}
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( cmd_name( argv ), out_fname, "w" );
	out_hdr.rle_file = outfile;

	rle_addhist( argv, &in_hdr, &out_hdr );

	out_hdr.background = 2 - overlay_flag;
	if ( backcolor_flag != 0 )
	    out_hdr.bg_color = new_back;

	rle_put_setup( &out_hdr );

	rle_cp( &in_hdr, &out_hdr );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), fname );

    exit( 0 );
}
