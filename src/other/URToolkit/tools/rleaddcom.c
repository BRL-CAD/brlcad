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
 * rle_addcom.c - Add comment[s] to an RLE file.
 * 
 * Author:	Spencer W. Thomas
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Sun Jan 25 1987
 * Copyright (c) 1987, University of Utah
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "rle.h"
#include <sys/types.h>
#include <sys/param.h>			/* for MAXPATHLEN */
#include <sys/stat.h>

#ifndef MAXPATHLEN
# define MAXPATHLEN BUFSIZ
#endif

static char temp[] = "intoXXXXXXXX";
static char buf[MAXPATHLEN+1];

/*****************************************************************
 * TAG( main )
 * 
 * Add one or more comments to an RLE file.
 *
 * Usage:
 *	rle_addcom [-d] [-i] [-o outfile] infile comments ...
 * Inputs:
 *	-d:		Delete matching comments instead of adding any.
 *	-i:		Do it "in place" -- replace the input file.
 * 	outfile:	Modified file with comments (defaults to stdout).
 * 	infile:		File to add comments to.
 *	comments:	One or more strings.  Each will be inserted as
 *			a separate comment.  They will usually be of the
 *			form "name=value".
 * Outputs:
 * 	Writes modified RLE file to standard output.
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
    rle_hdr 	in_hdr, out_hdr;
    char * fname = NULL, * out_fname = NULL;
    char *i_fname = NULL;
    FILE *outfile = stdout;
    char ** comments = NULL;
    char *my_name = cmd_name( argv );
    int oflag = 0, iflag = 0, delflag = 0, ncomment = 0;
    int is_pipe = 0;
    register int j;
    int rle_cnt, rle_err;

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% d%- i%- o%-outfile!s infile!s comments!*s",
		   &delflag, &iflag, &oflag, &out_fname, &fname,
                   &ncomment, &comments ) == 0 )
	exit( 1 );

    if ( iflag )
    {
	register char *cp;

	if ( oflag )
	    i_fname = out_fname;
	else
	    i_fname = fname;

	/* Recognize rle_open_f special cases. */
	if ( strcmp( i_fname, "-" ) == 0 )
	{
	    fprintf( stderr,
		     "%s: Can't add comments \"in place\" to standard %s.\n",
		     my_name, oflag ? "output" : "input" );
	    exit( 1 );
	}
#ifndef NO_OPEN_PIPES
	if ( *i_fname == '|' )
	{
	    fprintf( stderr,
		     "%s: Can't add comments \"in place\" to piped %s.\n",
		     my_name, oflag ? "output" : "input" );
	    exit( 1 );
	}
#endif
	strcpy( buf, i_fname );
	if ( (cp = rindex( buf, '/' )) != NULL )
	{
	    *++cp = 0;
	    strcat( buf, temp );
	}
	else
	    strcpy( buf, temp );
	mktemp( buf );
#ifndef NO_OPEN_PIPES
	/* Compressed file special case. */
	cp = i_fname + strlen( i_fname ) - 2;
	if ( cp > i_fname && *cp == '.' && *(cp + 1) == 'Z' )
	{
	    strcat( buf, ".Z" );
	    is_pipe = 1;
	}
#endif
	out_fname = buf;
    }

    in_hdr.rle_file = rle_open_f(my_name, fname, "r");
    rle_names( &in_hdr, my_name, fname, 0 );
    rle_names( &out_hdr, out_hdr.cmd, out_fname, 0 );

    /* Read in header */
    for ( rle_cnt = 0;
	  (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	  rle_cnt++ )
    {
	(void)rle_hdr_cp( &in_hdr, &out_hdr );
	if ( rle_cnt == 0 )
	    outfile = rle_open_f( my_name, out_fname, "w" );
	out_hdr.rle_file = outfile;

	/* Copy input to the_hdr struct */
	for ( j = 0; j < ncomment; j++ )
	{
	    if ( ! delflag )
		rle_putcom( comments[j], &out_hdr );
	    else
		rle_delcom( comments[j], &out_hdr );
	}

	/* Start output file */
	rle_put_setup( &out_hdr );

	/* Copy rest of input to output */
	rle_cp( &in_hdr, &out_hdr );
    }
    /* Check for an error.  EOF or EMPTY is ok if at least one image
     * has been read.  Otherwise, print an error message.
     */
    if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
    {
	rle_get_error( rle_err, my_name, fname );
	rle_err = 1;
    }
    else
	rle_err = 0;

    if ( iflag )
    {
	if ( rle_err )
	    fprintf( stderr, "%s: %s not replaced\n",
		     my_name, i_fname );
	else
	{
#ifndef NO_OPEN_PIPES
	    /* Have to call pclose, else file may not exist yet! */
	    if ( is_pipe )
		pclose( outfile );
	    else
#endif
		fclose( outfile );
	    if ( rename( buf, i_fname ) < 0 )
	    {
		fprintf( stderr, "%s: rename failed: ", my_name );
		perror( "" );
		unlink( buf );	/* Get rid of temp file. */
	    }
	}
    }

    exit( 0 );
}

#ifdef NEED_RENAME
rename( file1, file2 )
char *file1, *file2;
{
    struct stat st;

    if ( stat(file2, &st) >= 0 && unlink(file2) < 0 )
	return -1;
    if ( link(file1, file2) < 0 )
	return -1;
    return unlink( file1 );
}
#endif
