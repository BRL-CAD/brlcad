/* 
 * rleaddeof.c - Add an EOF opcode to RLE files without one.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Michigan
 * Date:	February, 1990
 * Copyright (c) 1990, The Regents of the University of Michigan
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#include "machine.h"
#include "rle.h"
#include "rle_code.h"
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
int
main(int argc, char **argv)
{
    char * infname = NULL, *outfname = NULL;
    int oflag = 0;
    FILE *outfile = stdout;
    rle_hdr in_hdr, out_hdr;
    int rle_cnt, rle_err;

    if ( scanargs( argc, argv, "% o%-outfile!s infile%s",
		   &oflag, &outfname, &infname ) == 0 )
	exit( 1 );

    /* Open input file */
    in_hdr.rle_file = rle_open_f( "rleaddeof", infname, "r" );
    rle_cnt = 0;
    while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
    {
	/* Open output after first successful read. */
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( "rleaddeof", outfname, "w" );
	rle_cnt++;

	/* Copy input to output file */
	out_hdr = in_hdr;
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
