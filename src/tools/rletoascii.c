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
main(int argc, char **argv)
{
    FILE   *infile;		/* Input file pointer. */
    char   *infnam = NULL;	/* Input file name. */
    int nrow, nscan, i, row, index, rflag=0;
    unsigned char ** scan;
    unsigned char * buffer;
    char *asciistr = NULL;
    int numchars;

    if (! scanargs( argc,argv,
	"% S%-ascii_string!s r%- infile%s", &i, &asciistr, &rflag, &infnam ))
	exit( -1 );

    /* If an input file is specified, open it. Otherwise use stdin. */

    if ( infnam != NULL )
    {
	if ( (infile = fopen( infnam, "r" )) == NULL )
	{
	    perror( infnam );
	    exit( -1 );
	}
    }
    else
	infile = stdin;

    /* Read header information. */

    rle_dflt_hdr.rle_file = infile;
    rle_get_setup_ok( &rle_dflt_hdr, "rletoascii", infnam );
    if ( (rle_dflt_hdr.ncolors != 3 ) && (rle_dflt_hdr.ncolors != 1) ) {
	fprintf( stderr, "%s must have 3 or 1 channels, not %d\n",
		    infnam ? infnam : "stdin", rle_dflt_hdr.ncolors );
	exit( 0 );
    }
    RLE_CLR_BIT( rle_dflt_hdr, RLE_ALPHA );
    rle_dflt_hdr.xmax -= rle_dflt_hdr.xmin;
    rle_dflt_hdr.xmin = 0;
    nrow = rle_dflt_hdr.xmax + 1;
    nscan = (rle_dflt_hdr.ymax - rle_dflt_hdr.ymin + 1);
    buffer = (unsigned char *)malloc( nrow );
    scan = (unsigned char **) malloc( rle_dflt_hdr.ncolors * 
				      sizeof( unsigned char * ) );
    for ( i = 0; i < rle_dflt_hdr.ncolors; i++ )
	scan[i] = (unsigned char *)malloc( nrow );

    if ( asciistr == NULL ) {
	asciistr = (char *) malloc( 1 + strlen( default_asciistr ) );
	strcpy(asciistr, default_asciistr);
    }
    numchars = strlen(asciistr);

    /* Read .rle file, dumping ascii as we go. */

    for (row=0; (row<nscan); row++)
    {
	if (rle_dflt_hdr.ncolors == 3) {
	    rle_getrow( &rle_dflt_hdr, scan );
	    rgb_to_bw( scan[0], scan[1], scan[2], buffer, nrow );
	}
	else {
	    rle_getrow( &rle_dflt_hdr, &buffer );
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
    return 0;
}
