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
 * rle_error.c - Error message stuff for URT.
 * 
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Mon Mar  2 1992
 * Copyright (c) 1992, University of Michigan
 */
#ifndef lint
static char rcs_id[] = "$Header$";
#endif

#include "rle.h"

/*****************************************************************
 * TAG( rle_alloc_error )
 * 
 * Print memory allocation error message and exit.
 * Inputs:
 * 	pgm:		Name of this program.
 * 	name:		Name of memory trying to be allocated.
 * Outputs:
 * 	Prints message and exits.
 *
 * Returns int because it's used in a conditional expression.
 */
int
rle_alloc_error( pgm, name )
CONST_DECL char *pgm, *name;
{
    if ( name )
	fprintf( stderr, "%s: memory allocation failed.\n", pgm );
    else
	fprintf( stderr, "%s: memory allocation failed (no space for %s).\n",
		 pgm, name );

    exit( RLE_NO_SPACE );

    /* Will some compilers bitch about this because they know exit
     * doesn't return??
     */
    return 0;
}

/*****************************************************************
 * TAG( rle_get_error )
 * 
 * Print an error message for the return code from rle_get_setup
 * Inputs:
 * 	code:		The return code from rle_get_setup.
 *	pgmname:	Name of this program (argv[0]).
 *	fname:		Name of the input file.
 * Outputs:
 * 	Prints an error message on standard output.
 *	Returns code.
 */
int
rle_get_error( code, pgmname, fname )
int code;
CONST_DECL char *pgmname;
CONST_DECL char *fname;
{
    if (! fname || strcmp( fname, "-" ) == 0 )
	fname = "Standard Input";

    switch( code )
    {
    case RLE_SUCCESS:		/* success */
	break;

    case RLE_NOT_RLE:		/* Not an RLE file */
	fprintf( stderr, "%s: %s is not an RLE file\n",
		 pgmname, fname );
	break;

    case RLE_NO_SPACE:			/* malloc failed */
	fprintf( stderr,
		 "%s: Malloc failed reading header of file %s\n",
		 pgmname, fname );
	break;

    case RLE_EMPTY:
	fprintf( stderr, "%s: %s is an empty file\n",
		 pgmname, fname );
	break;

    case RLE_EOF:
	fprintf( stderr,
		 "%s: RLE header of %s is incomplete (premature EOF)\n",
		 pgmname, fname );
	break;

    default:
	fprintf( stderr, "%s: Error encountered reading header of %s\n",
		 pgmname, fname );
	break;
    }
    return code;
}


