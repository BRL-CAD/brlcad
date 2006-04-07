/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notices are 
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
 * graytorle.c - Create an RLE image from gray pixels.
 * 
 * Author:	Michael J. Banks
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jun 22 1988
 * Copyright (c) 1988, University of Utah
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

typedef FILE	*FILPTR;

/*
 * usage : graytorle xsize ysize [-h hdrsize] [-o outfile] [-a] files ...
 *
 *    xsize,ysize	Size of input files.
 *    -h hdrsize	Input file header to discard.
 *    -o outfile	Output file name.
 *    -a		Uses first input file as alpha channel.
 */

int
main(argc, argv)
int  argc;
char *argv[];
{
    int			hflag = 0;	/* Header size flag */
    int 		aflag = 0;	/* Alpha channel flag. */
    int			stdin_used = 0;
    char 	       *oname = NULL;	/* Output file name. */
    FILE **inpfil;		/* Input file pointers. */
    int xsize, ysize;		/* Image size. */
    int hsize;			/* Image header size. */
    int oflag;	        	/* Output file flag. */
    int files;			/* Number of files. */
    char **fname;		/* List of input file names. */
    rle_hdr out_hdr;
    rle_pixel **outrow;		/* Output buffer. */
    int i, row;
    char *trash;

    if (! scanargs( argc,argv,
	"% a%- h%-hdrsize!d o%-outfile!s xsize!d ysize!d files!*s\n(\
\tConvert one or more raw images to an RLE image.\n\
\t-a\tFirst input file is alpha channel.  Normally first is red.\n\
\t-h\tSpecify number of bytes to discard from input header.\n\
\txsize ysize\tSize of input images.\n\
\tTry rawtorle for more complex situations.)",
	    &aflag,
	    &hflag, &hsize,
	    &oflag, &oname,
	    &xsize, &ysize,
	    &files, &fname ))
	exit( -1 );

    out_hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &out_hdr, cmd_name( argv ), oname, 0 );

    /* 
     * Get enough file pointers for all input files that are specified,
     * and try to open them.
     */

    inpfil = (FILE **)malloc( sizeof( FILE * ) * files );
    for ( i=0; i<files; i++ )
    {
	inpfil[i] = rle_open_f( "graytorle", fname[i], "r" );
	if ( inpfil[i] == stdin )
	{
	    if ( stdin_used )
	    {
		fprintf( stderr,
		      "%s: Can't use standard input for more than one file\n",
			 out_hdr.cmd );
		exit( -1 );
	    }
	    stdin_used++;
	}
    }

    /* Throw away file headers. */

    if ( hflag && (hsize > 0) ) 
    {
	trash = (char *)malloc( hsize );

	for ( i=0; i<files; i++ )
	    fread( trash, 1, hsize, inpfil[i] );

	free( trash );
    }

    /* Adjust alpha channnel flag to use as index. */

    if ( aflag ) 
	aflag = 1;
    else
	aflag = 0;

    /* Initialize the_hdr and allocate image row storage. */

    out_hdr.alpha = aflag;
    out_hdr.ncolors = files - aflag;
    out_hdr.xmax = xsize - 1;
    out_hdr.ymax = ysize - 1;
    out_hdr.rle_file = rle_open_f(out_hdr.cmd, oname, "w");
    for ( i = -aflag; i<out_hdr.ncolors; i++)
	RLE_SET_BIT( out_hdr, i );
    rle_addhist( argv, (rle_hdr *)NULL, &out_hdr );
    rle_put_setup( &out_hdr );

    if (rle_row_alloc( &out_hdr, &outrow ))
    {
	fprintf(stderr, "%s: Ran out of heap space!!\n",
		out_hdr.cmd );
	exit(-2);
    }

    /* Combine rows and write to output file.  Adjust for alpha. */

    for ( row=0; row<ysize; row++)
    {
	for ( i = -aflag; i<files-aflag; i++ )
	    fread( outrow[i], 1, xsize, inpfil[i+aflag] );
	rle_putrow( outrow, xsize, &out_hdr );
    }
    rle_puteof( &out_hdr );
}

