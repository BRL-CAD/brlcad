/* 
 * rle_open_f.c - Open a file with defaults.
 * 
 * Author : 	Jerry Winters 
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	11/14/89
 * Copyright (c) 1990, University of Michigan
 */

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"


/* 
 *  Purpose : Open a file for input or ouput as controled by the mode
 *  parameter.  If no file name is specified (ie. file_name is null) then
 *  a pointer to stdin or stdout will be returned.  The calling routine may
 *  call this routine with a file name of "-".  For this case rle_open_f
 *  will return a pointer to stdin or stdout depending on the mode.
 *    If the user specifies a non-null file name and an I/O error occurs
 *  when trying to open the file, rle_open_f will terminate execution with
 *  an appropiate error message.
 *
 *  parameters
 *   input:
 *     prog_name: 	name of the calling program.
 *     file_name : 	name of the file to open
 *     mode : 		either "r" for read or input file or "w" for write or
 *            		output file
 *
 *   output:
 *     a file pointer
 * 
 */
FILE *
rle_open_f_noexit( prog_name, file_name, mode ) 
char *prog_name, *file_name, *mode;
{
    FILE *fp;
    char *err_str;
    register char *cp;
    char *combuf;

#ifdef STDIO_NEEDS_BINARY
    char mode_string[32];	/* Should be enough. */

    /* Concatenate a 'b' onto the mode. */
    mode_string[0] = mode[0];
    mode_string[1] = 'b';
    strcpy( mode_string + 2, mode + 1 );
    mode = mode_string;
#endif

    if ( *mode == 'w' || *mode == 'a' )
	fp = stdout;     /* Set the default value */
    else
	fp = stdin;
    
    if ( file_name != NULL && strcmp( file_name, "-" ) != 0 )
    {
#ifndef	NO_OPEN_PIPES
	/*  Real file, not stdin or stdout.  If name ends in ".Z",
	 *  pipe from/to un/compress (depending on r/w mode).
	 *  
	 *  If it starts with "|", popen that command.
	 */

	cp = file_name + strlen( file_name ) - 2;
	/* Pipe case. */
	if ( *file_name == '|' )
	{
	    if ( (fp = popen( file_name + 1, mode )) == NULL )
	    {
		err_str = "%s: can't invoke <<%s>> for %s: ";
		goto err;
	    }
	}

	/* Compress case. */
	else if ( cp > file_name && *cp == '.' && *(cp + 1) == 'Z' )
	{
	    combuf = (char *)malloc( 20 + strlen( file_name ) );
	    if ( combuf == NULL )
	    {
		err_str = "%s: out of memory opening (compressed) %s for %s";
		goto err;
	    }

	    if ( *mode == 'w' )
		sprintf( combuf, "compress > %s", file_name );
	    else if ( *mode == 'a' )
		sprintf( combuf, "compress >> %s", file_name );
	    else
		sprintf( combuf, "compress -d < %s", file_name );

	    fp = popen( combuf, mode );
	    free( combuf );

	    if ( fp == NULL )
	    {
		err_str =
    "%s: can't invoke 'compress' program, trying to open %s for %s";
		goto err;
	    }
	}

	/* Ordinary, boring file case. */
	else
#endif /* !NO_OPEN_PIPES */
	    if ( (fp = fopen(file_name, mode)) == NULL )
	    {
		err_str = "%s: can't open %s for %s: ";
		goto err;
	    }
    }

    return fp;

err:
	fprintf( stderr, err_str,
		 prog_name, file_name,
		 (*mode == 'w') ? "output" :
		 (*mode == 'a') ? "append" :
		 "input" );
	perror( "" );
	return NULL;

}

FILE *
rle_open_f( prog_name, file_name, mode )
char *prog_name, *file_name, *mode;
{
    FILE *fp;

    if ( (fp = rle_open_f_noexit( prog_name, file_name, mode )) == NULL )
	exit( -1 );

    return fp;
}

