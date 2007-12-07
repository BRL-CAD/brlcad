/*
 * rlecat.c - Concatenate RLE files.
 *
 * Author:	Spencer W. Thomas
 * 		EECS Dept.
 * 		University of Michigan
 * Date:	Mon Nov  5 1990
 * Copyright (c) 1990, University of Michigan
 */
#ifndef lint
static char rcsid[] = "$Header$";
#endif
/*
rlecat()		Make a tag.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "rle.h"
#include "rle_raw.h"

#ifdef USE_PROTOTYPES
static void rep_file(rle_hdr *, rle_hdr *, int);
#else
static void rep_file();
#endif

/*
 * rlecat
 *
 * Concatenate RLE files, adding title comments, and/or repeating files.
 *
 * Usage:
 *  	rlecat [-c] [-n repeat-count] [-o outfile] [files...]
 * Inputs:
 *  	-c: 	"Collated"  Multiple input images will be repeated
 *  	    	(see -n) in sequence 1 2 3 ... 1 2 3 ...  The default
 *  	    	is uncolllated, 1 1 ... 2 2 ... 3 3 ...
 *
 *  	-n repeat-count:
 *  	    	Repeat each input image 'repeat-count' times.  The -c
 *  	    	flag controls the ordering of the repeats.  Repeating
 *  	    	creates a temporary file in the "/tmp" directory that
 *  	    	is the size of a single "repeat unit".  In collated
 *  	    	mode, a repeat unit consists of the concatenation of
 *  	    	all the input images, otherwise it is just a single
 *  	    	image.
 *
 *  	files:	Input file names.  If none specified, input will be
 *  	    	read from the standard input.
 *
 * Outputs:
 *  	-o outfile:
 *  	    	Output file.  If not specified, output is written to
 *  	    	the standard output.
 *
 * Algorithm:
 *  	Reads each input image, adds a 'title' comment to it based on
 *  	the input file name (no title is added to images read from the
 *  	standard input), and writes it to the output.  If a repeat
 *  	count is specified, the image is written to a temporary file,
 *  	and this is repeatedly copied to the output according to the
 *  	repeat count.  If the collation flag (-c) is specified, then
 *  	all images will be read before starting to repeat.
 */
int
main( argc, argv )
int argc;
char **argv;
{
    CONST_DECL char **infname = NULL,
    	       *outfname = NULL;
    CONST_DECL char *dash = "-";	/* Used to fake a request for stdin. */
    static char temp[] = "/tmp/rlecatXXXXXXXX";
    int 	cflag = 0,
    	    	nflag = 0,
    	    	rep_cnt = 0,
    	    	oflag = 0,
    	    	nfiles = 0;
    int		rle_cnt, rle_err, y, nskip;
    int	    	file_cnt;
    FILE       *outfile, *tmpfile = NULL;
    rle_hdr 	in_hdr, out_hdr;
    rle_hdr 	tmp_hdr;	/* Header for temp file for repeats. */
    char    	buf[BUFSIZ];	/* For building title comment. */
    rle_op    **rows;		/* Storage for input data. */
    int	       *n_op;		/* Number of ops per row. */

    in_hdr = *rle_hdr_init( NULL );
    out_hdr = *rle_hdr_init( NULL );

    if ( scanargs( argc, argv, "% c%- n%-repeat-count!d o%-outfile!s files%*s",
		   &cflag, &nflag, &rep_cnt,
		   &oflag, &outfname, &nfiles, &infname ) == 0 )
	exit( 1 );

    /* If no input, use standard input. */
    if ( nfiles == 0 )
    {
	nfiles = 1;
	infname = &dash;
    }

    /* Open the output file now, to make sure we can. */
    outfile = rle_open_f_noexit( cmd_name( argv ), outfname, "w" );

    /* If requesting repeats, create temp file. */
    if ( nflag )
    {
	if ( rep_cnt < 2 )
	    nflag = 0;		/* Not really repeating! */
	else
	{
	    mkstemp( temp );	/* Make a temporary file name */
	    tmpfile = rle_open_f( cmd_name( argv ), temp, "w+" );
	}
    }

    /* For each file, read it and write it. */
    rle_names( &out_hdr, cmd_name( argv ), outfname, 0 );
    for ( file_cnt = 0; file_cnt < nfiles; file_cnt++ )
    {
	/* Open the input file. */
	rle_names( &in_hdr, out_hdr.cmd, infname[file_cnt], 0 );
	in_hdr.rle_file =
	    rle_open_f_noexit( in_hdr.cmd, infname[file_cnt], "r" );
	if ( in_hdr.rle_file == NULL )
	    continue;

	/* Count the input images. */
	for ( rle_cnt = 0;
	      (rle_err = rle_get_setup( &in_hdr )) == RLE_SUCCESS;
	      rle_cnt++ )
	{
	    /* The output header is a copy of the input header.  The only
	     * difference is the FILE pointer.
	     */
	    (void)rle_hdr_cp( &in_hdr, &out_hdr );
	    out_hdr.rle_file = outfile;

	    /* Add to the history comment. */
	    rle_addhist( argv, &in_hdr, &out_hdr );

	    /* Build the title comment. */
	    if ( rle_getcom( "title", &in_hdr ) ||
		 rle_getcom( "TITLE", &in_hdr ) )
		;		/* Don't disturb existing title. */
	    else
		if ( in_hdr.rle_file != stdin )
		{
		    if ( rle_cnt == 0 )
			sprintf( buf, "TITLE=%s", infname[file_cnt] );
		    else
			sprintf( buf, "TITLE=%s(%d)", infname[file_cnt],
				 rle_cnt+1 );
		    rle_putcom( buf, &out_hdr );
		}

	    /* Write the output image header. */
	    rle_put_setup( &out_hdr );

	    if ( nflag )
	    {
		(void)rle_hdr_cp( &out_hdr, &tmp_hdr );
		tmp_hdr.rle_file = tmpfile;
		rle_put_setup( &tmp_hdr );
	    }

	    /* Allocate memory into which the image scanlines can be read.
	     * This should happen after the above adjustment, to minimize
	     * the amount of memory allocated.
	     */
	    if ( rle_raw_alloc( &in_hdr, &rows, &n_op ) < 0 )
		RLE_CHECK_ALLOC( cmd_name( argv ), 0, "image memory" );

	    /* Read the input image and copy it to the output file. */
	    y = in_hdr.ymin - 1;
	    while ( (nskip = rle_getraw( &in_hdr, rows, n_op )) != 32768 )
	    {
		nskip -= y;
		y += nskip;
		if ( nskip > 1 )
		    rle_skiprow( &out_hdr, nskip - 1 );

		/* Write the processed scanline. */
		rle_putraw( rows, n_op, &out_hdr );
		if ( nflag )
		{
		    if ( nskip > 1 )
			rle_skiprow( &tmp_hdr, nskip - 1 );
		    rle_putraw( rows, n_op, &tmp_hdr );
		}

		rle_freeraw( &in_hdr, rows, n_op );
	    }

	    /* Free memory. */
	    rle_raw_free( &in_hdr, rows, n_op );

	    /* Write an end-of-image code. */
	    rle_puteof( &out_hdr );
	    if ( nflag )
		rle_puteof( &tmp_hdr );

	    /* If not collating, do the repeats now. */
	    if ( !cflag && nflag )
		rep_file( &tmp_hdr, &out_hdr, rep_cnt );
	}

	/* Close the input file. */
	fclose( in_hdr.rle_file );

	/* Check for an error.  EOF or EMPTY is ok if at least one image
	 * has been read.  Otherwise, print an error message.
	 */
	if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	    rle_get_error( rle_err, in_hdr.cmd, in_hdr.file_name );
    }

    /* If collating, do the repeats here. */
    if ( cflag && nflag )
	rep_file( &tmp_hdr, &out_hdr, rep_cnt );

    /* If repeating, delete the temp file. */
    if ( nflag )
	unlink( temp );

    exit( 0 );			/* All ok. */
}

/*****************************************************************
 * TAG( rep_file )
 *
 * Copy an rle file repeatedly to the output file.
 * Inputs:
 * 	in_hdr:	    Header for the file to copy from.
 * 	rep_cnt:    Number of times to repeat + 1 (one copy has
 * 	    	    already been written at this point.)
 * Outputs:
 * 	out_hdr:    Header for the output file.
 * Assumptions:
 * 	in_hdr refers to a seekable file, file "cursor" is at the end
 * 	of the image(s) to be copied.
 * Algorithm:
 *  	Save current file position in nbytes.
 * 	Repeat rep_cnt-1 times:
 * 	    Rewind the input.
 * 	    Copy nbytes bytes from the input to the output.
 *  	Rewind the input.
 */
static void
rep_file( in_hdr, out_hdr, rep_cnt)
rle_hdr *in_hdr, *out_hdr;
int rep_cnt;
{
    long int pos = ftell( in_hdr->rle_file );
    char buf[BUFSIZ];
    int n, nr;

    while ( --rep_cnt > 0 )
    {
	rewind( in_hdr->rle_file );
	for ( n = 0;
	      n < pos && (nr = fread( buf, 1, BUFSIZ, in_hdr->rle_file )) > 0;
	      n += nr )
	{
	    if ( pos - n < nr )
		nr = pos - n;
	    fwrite( buf, 1, nr, out_hdr->rle_file );
	}
    }

    rewind( in_hdr->rle_file );
}
