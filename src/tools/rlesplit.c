/* 
 * rlesplit.c - Split concatenated RLE files into separate files.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon May  4 1987
 * Copyright (c) 1987, University of Utah
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "rle.h"
#include "rle_code.h"
#include "rle_raw.h"

/*****************************************************************
 * TAG( main )
 * 
 * Usage:
 *     rlesplit [-n number [digits]] [-p prefix] [rlefile]
 * Inputs:
 *	-n number:	If specified, output file numbering will start
 *			with this value (see below).  Otherwise, numbering
 *			starts at 1.
 *
 * 	digits:		The number of digits to be used in the numeric
 * 			portion of the output file names.  Defaults to 3.
 * 			All numbers will be leading zero filled.
 * 
 * 	-p prefix:	If specified, output files will be named
 *			"prefix.#.rle".  If not specified, and
 *			an rlefile is specified, then output files
 *			will be "rlefileroot.#.rle", where
 *			"rlefileroot" is rlefile with any ".rle" suffix
 *			stripped off.  If no arguments are specified,
 *			output files will be "#.rle".  In any case, "#"
 *			represents a sequentially increasing number.
 *
 *	infile:	If specified, input will be read from here,
 *			otherwise, input will be read from stdin.
 * Outputs:
 * 	Writes each rle image in the input stream to an output file
 *	whose name is generated as above.
 * Assumptions:
 * 	Each RLE image in the input stream must be terminated with
 *	an EOF opcode.
 * Algorithm:
 *	[None]
 */

int
main(int argc, char **argv)
{
    register const char * cp, * slashp;
    int num = 1, oflag = 0, digits = 3;
    int rle_err, ynext, y;
    const char * infname = NULL, * format = "%s%0*d.rle";
    char * prefix = "";
    char filebuf[BUFSIZ];
    rle_hdr in_hdr, out_hdr;
    rle_op ** scan;
    int * nraw;

    if ( scanargs( argc, argv, "% n%-number!ddigits%d o%-prefix!s infile%s",
		   &num, &num, &digits, &oflag, &prefix, &infname ) == 0 )
	exit( 1 );

    /* Open input file */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");

    if ( oflag || infname )
    {
	format = "%s.%0*d.rle";
	if ( !oflag )
	{
	    /* Strip ".rle" suffix from input file name */
	    /* Avoid strrchr, rindex problem */
	    for ( cp = infname; *cp; cp++ )
		;		/* find end of name */
	    /* Look for last slash */
	    for ( slashp = cp - 1; *slashp != '/' && slashp > infname; )
		slashp--;
	    if ( *slashp == '/' )
		slashp++;
	    /* Look for last dot */
	    while ( *--cp != '.' && cp > infname )
		;		/* find last . */
	    if ( strcmp( cp, ".rle" ) != 0 )
		cp = infname + strlen( infname );
	    /* Make null full string buffer */
	    prefix = (char *)calloc( cp - slashp + 1, 1 );
	    /* Copy everything but suffix */
	    strncpy( prefix, infname, cp - slashp );
	}
    }

    while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
    {
	/* Copy input to output file */
	sprintf( filebuf, format, prefix, digits, num++ );
	fprintf( stderr, "%s: Creating %s\n", cmd_name( argv ), filebuf );
	out_hdr = in_hdr;
        out_hdr.rle_file = rle_open_f(cmd_name( argv ), filebuf, "w");
	rle_addhist( argv, &in_hdr, &out_hdr );

	rle_put_setup( &out_hdr );

	rle_raw_alloc( &in_hdr, &scan, &nraw );


	for ( y = in_hdr.ymin;
	      (ynext = rle_getraw( &in_hdr, scan, nraw )) != 32768;
	      y = ynext )
	{
	    if ( ynext - y > 1 )
		rle_skiprow( &out_hdr, ynext - y );
	    rle_putraw( scan, nraw, &out_hdr );
	    rle_freeraw( &in_hdr, scan, nraw );
	}
	rle_puteof( &out_hdr );
	fclose( out_hdr.rle_file );

	rle_raw_free( &in_hdr, scan, nraw );
    }

    if ( rle_err != RLE_EOF && rle_err != RLE_EMPTY )
	rle_get_error( rle_err, argv[0], infname );

    exit( 0 );
}
