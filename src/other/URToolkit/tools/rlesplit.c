/*
 * rlesplit.c - Split concatenated RLE files into separate files.
 *
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Mon May  4 1987
 * Copyright (c) 1987, University of Utah
 */
#ifndef lint
char rcsid[] = "$Header$";
#endif
/*
rlesplit()			Tag the file.
*/

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"
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
 * 	-t:		Use TITLE comment as output file name.
 * 			Obviously, these should be distinct!
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
main( argc, argv )
int argc;
char **argv;
{
    register CONST_DECL char * cp, * slashp;
    int num = 1, oflag = 0, digits = 3, tflag = 0;
    int rle_err;
    CONST_DECL char *infname = NULL, *format = "%s%0*d.rle";
    static char nullpref[] = "";
    char *prefix = nullpref;
    char *title;
    char filebuf[BUFSIZ];
    rle_hdr 	in_hdr, out_hdr;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv,
		   "% n%-number!ddigits%d o%-prefix!s t%- infile%s\n(\
\tSplit a multi-image file into separate files.\n\
\t-n\tSpecify number assigned to first image, optionally specify\n\
\t\tnumber of digits used for numbering (default 3).\n\
\t-o\tSpecify output filename prefix.  If not specified, input\n\
\t\tfile name (without .rle) will be used.  Output file names are\n\
\t\tof the form prefix.number.rle.\n\
\t-t\tUse TITLE (or title) comment as file name.  They should be distinct.)",
		   &num, &num, &digits, &oflag, &prefix, &tflag,
		   &infname ) == 0 )
	exit( 1 );

    /* Open input file */
    in_hdr.rle_file = rle_open_f(cmd_name( argv ), infname, "r");
    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );

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
	    RLE_CHECK_ALLOC( in_hdr.cmd, prefix, 0 );
	    /* Copy everything but suffix */
	    strncpy( prefix, infname, cp - slashp );
	}
    }

    while ( (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS )
    {
	/* Copy input to output file */
	if ( tflag &&
	     ( (title=rle_getcom( "TITLE", &in_hdr)) ||
	       (title=rle_getcom( "title", &in_hdr)) ) &&
	     *title != '\0' )
	    sprintf( filebuf, "%s%s", prefix, title );
	else
	    sprintf( filebuf, format, prefix, digits, num++ );

	fprintf( stderr, "%s: Creating %s\n", cmd_name( argv ), filebuf );
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	rle_names( &out_hdr, out_hdr.cmd, filebuf, 0 );
        out_hdr.rle_file = rle_open_f(out_hdr.cmd, filebuf, "w");
	rle_addhist( argv, &in_hdr, &out_hdr );

	rle_put_setup( &out_hdr );

	/* Copy the image to the output file. */
	rle_cp( &in_hdr, &out_hdr );

	rle_close_f( out_hdr.rle_file );
    }

    if ( rle_err != RLE_EOF && rle_err != RLE_EMPTY )
	rle_get_error( rle_err, cmd_name( argv ), infname );

    exit( 0 );
}
