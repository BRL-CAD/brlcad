/*
 * rleaddeof.c - Add an EOF opcode to RLE files without one.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Michigan
 * Date:	February, 1990
 * Copyright (c) 1990, The Regents of the University of Michigan
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( main )
 *
 * Usage:
 *     rleaddeof [-o outfile] [infile]
 * Inputs:
 *	infile:		If specified, input will be read from here,
 *			otherwise, input will be read from stdin.
 * Outputs:
 *	-o outfile:	Writes the output image to this file with an
 *			EOF opcode appended. Default is stdout.
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
    char * infname = NULL, *outfname = NULL;
    int oflag = 0;
    FILE *outfile = stdout;
    rle_hdr 	in_hdr, out_hdr;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% o%-outfile!s infile%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    /* Open input file */
    in_hdr.rle_file = rle_open_f( cmd_name( argv ), infname, "r" );
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );
    rle_names( &out_hdr, out_hdr.cmd, outfname, 0 );
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	/* Open output after first successful read. */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( "rleaddeof", outfname, "w" );

	/* Copy input to output file */
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	out_hdr.rle_file = outfile;
	rle_put_setup( &out_hdr );

	rle_cp( &in_hdr, &out_hdr );
    }

    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	rle_get_error( rle_err, cmd_name( argv ), infname );


    exit( 0 );
}
