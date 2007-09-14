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
 * rletoascii.c - Take a RLE, make it black and white, dump it as ascii chars.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "rle.h"

typedef FILE	*FILPTR;

/*
 * usage : rletoascii [-S asciistring] [-r] [-o outfile] [infile]
 *
 *      asciistring		String of characters to use for output
 *      -r			reverse video
 *	infile			File to dump.  If none, uses stdin.
 *	-o outfile		Where to put the output.  Default stdout.
 */
/* Default string based on X font 6x13, assumes black on white.
 * @ = 24
 * B = 23
 * R = 22
 * * = 21
 * # = 20
 * $ = 19
 * P = 18
 * X = 17
 * 0 = 16
 * w = 15
 * o = 14
 * I = 13
 * c = 12
 * v = 11
 * : = 10
 * + = 9
 * ! = 8
 * ~ = 7
 * " = 6
 * . = 5
 * , = 4
 *   = 0
 */
static char default_asciistr[] = "@BR*#$PX0woIcv:+!~\"., ";

int
main(argc, argv)
int  argc;
char *argv[];
{
    char   *infnam = NULL;	/* Input file name. */
    rle_hdr hdr;
    int nrow, nscan, i, row, index, rflag=0;
    unsigned char ** scan;
    unsigned char * buffer;
    char *asciistr = NULL;
    int numchars;

    if (! scanargs( argc,argv,
		    "% S%-ascii_string!s r%- infile%s\n(\
\t-S\tSpecify set of characters for output (dark to light).\n\
\t-r\tReverse black & white -- white on black.)",
		    &i, &asciistr, &rflag, &infnam ))
	exit( -1 );

    /* Set up header. */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    rle_names( &hdr, cmd_name( argv ), infnam, 0 );
    hdr.rle_file = rle_open_f( hdr.cmd, infnam, "r" );

    /* Read header information. */

    rle_get_setup_ok( &hdr, "rletoascii", infnam );
    if ( hdr.ncolors == 0 )
    {
	fprintf( stderr, "%s: Image %s has no data.\n",
		 hdr.cmd, hdr.file_name );
	exit( 1 );
    }

    if ( hdr.ncolors > 3 ) {
	fprintf( stderr,
		 "%s: Only first 3 channels (out of %d) in %s are shown.\n",
		 hdr.cmd, hdr.ncolors, hdr.file_name );
	for ( i = 3; i < hdr.ncolors; i++ )
	    RLE_CLR_BIT( hdr, i );
    }
    RLE_CLR_BIT( hdr, RLE_ALPHA );
    hdr.xmax -= hdr.xmin;
    hdr.xmin = 0;
    nrow = hdr.xmax + 1;
    nscan = (hdr.ymax - hdr.ymin + 1);
    buffer = (unsigned char *)malloc( nrow );
    scan = (unsigned char **) malloc( hdr.ncolors *
				      sizeof( unsigned char * ) );
    for ( i = 0; i < hdr.ncolors; i++ )
	scan[i] = (unsigned char *)malloc( nrow );
    for ( ; i < 3; i++ )
	scan[i] = scan[i-1];

    if ( asciistr == NULL ) {
	asciistr = (char *) malloc( 1 + strlen( default_asciistr ) );
	strcpy(asciistr, default_asciistr);
    }
    numchars = strlen(asciistr);

    /* Read .rle file, dumping ascii as we go. */

    for (row=0; (row<nscan); row++)
    {
	if (hdr.ncolors > 1) {
	    rle_getrow( &hdr, scan );
	    rgb_to_bw( scan[0], scan[1], scan[2], buffer, nrow );
	}
	else {
	    rle_getrow( &hdr, &buffer );
	}
	for (i=0; i < nrow; i++) {
	    index = ((int)(buffer[i]) * numchars) >> 8;
	    if (rflag)
		putchar(asciistr[numchars - (index + 1)]);
	    else
		putchar(asciistr[index]);
	}
	printf("\n");
    }
}
