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
 * rletogray.c - Splits an RLE file into one gray file per channel.
 * 
 * Author:	Michael J. Banks
 * 		Computer Science Dept.
 * 		University of Utah
 * Date:	Wed Jun 22 1988
 * Copyright (c) 1988, University of Utah
 */
static char rcsid[] = "$Header$";
/*
rletogray()			Tag the file.
*/

#include <stdio.h>
#include "rle.h"

typedef FILE	*FILPTR;

/*
 * usage : rletogray [-o outprefix] [infile]
 *
 *	-o outprefix		Specifies ouput file name prefix.
 *	infile			File to split.  If none, uses stdin.
 */

void
main(argc, argv)
int  argc;
char *argv[];
{
    char       *inpnam = NULL;	/* Input file name. */
    static char defpref[] = "out";	/* So it's not CONST. */
    char       *prefix = NULL;	/* Output file name prefix. */
    rle_hdr 	hdr;
    int 	aflag = 0;	/* Alpha channel flag. */
    int 	oflag = 0;	/* Output file name prefix flag. */
    register char * cp, * slashp;
    FILPTR     *outfil;		/* Output file pointers. */
    char    	outnam[BUFSIZ];	/* Output file name. */
    int 	files;		/* Number of output files. */
    int 	scans, rasts;	/* Number of scan lines. */
    rle_pixel **inprow;		/* Input buffer. */
    int 	i, row;

    if (! scanargs( argc,argv,
	"% o%-outprefix!s infile%s", &oflag, &prefix, &inpnam ))
	exit( -1 );

    /* Set up header and open input. */
    /* Initialize header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), inpnam, 0 );

    hdr.rle_file = rle_open_f(hdr.cmd, inpnam, "r");

    /* Read header information. */

    rle_get_setup_ok( &hdr, NULL, NULL );
    if ( hdr.alpha )
	aflag = 1;
    scans = hdr.ymax - hdr.ymin + 1;
    rasts = hdr.xmax - hdr.xmin + 1;
    files = aflag + hdr.ncolors;

    /* Figure out what we want to call the output files. */

    if ( !inpnam && !oflag )
	prefix = defpref;
    else if ( inpnam && !oflag )
    {
	/* Strip ".rle" suffix from input file name */
	/* Avoid strrchr, rindex problem */
	for ( cp = inpnam; *cp; cp++ )
	    ;		/* find end of name */
	/* Look for last slash */
	for ( slashp = cp - 1; *slashp != '/' && slashp > inpnam; )
	    slashp--;
	if ( *slashp == '/' )
	    slashp++;
	/* Look for last dot */
	while ( *--cp != '.' && cp > inpnam )
	    ;		/* find last . */
	if ( strcmp( cp, ".rle" ) != 0 )
	    cp = inpnam + strlen( inpnam );
	/* Make null full string buffer */
	prefix = (char *)calloc( cp - slashp + 1, 1 );
	/* Copy everything but suffix */
	strncpy( prefix, inpnam, cp - slashp );
    }

    /* 
     * Get enough file pointers for all output files that are necessary,
     * and try to open them.
     */

    outfil = (FILPTR *)malloc( sizeof( FILPTR ) * files );
    for ( i = -aflag;  i<files-aflag; i++ )
    {
	switch( i ) 
	{
	case -1:
	    sprintf( outnam, "%s.alpha", prefix );
	    break;

	case 0:
	    sprintf( outnam, "%s.red", prefix );
	    break;

	case 1:
	    sprintf( outnam, "%s.green", prefix );
	    break;

	case 2:
	    sprintf( outnam, "%s.blue", prefix );
	    break;

	default:
	    sprintf( outnam, "%s.%03d", prefix, i );
	    break;
	}


	if ( (outfil[i+aflag] = fopen( outnam, "w" )) == NULL )
	{
	    fprintf( stderr, "%s: Can't create %s: ", hdr.cmd, outnam );
	    perror( "" );
	    exit( -1 );
	}
    }

    /* Allocate input buffer. */

    if (rle_row_alloc( &hdr, &inprow ))
    {
	fprintf(stderr, "%s: Out of memory.\n", hdr.cmd);
	exit(-2);
    }

    /* Read .rle file, splitting into gray files. */

    for (row=0; (row<scans); row++)
    {
	rle_getrow( &hdr, inprow );
	for ( i = -aflag; i<files-aflag; i++ )
	    fwrite( inprow[i], 1, rasts, outfil[i+aflag] );
    }
}
