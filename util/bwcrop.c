/*
 *		B W C R O P . C
 *
 * Crop Black and White files.
 *
 * Given four "corner points" in the input file, we produce an
 * output file of the requested size consisting of the nearest
 * pixels.  No filtering/interpolating is done.
 *
 * This can handle arbitrarily large files.
 * We keep a buffer of scan lines centered around the last
 * point that fell outside of the buffer.
 * Note: this buffer code is shared with bwscale.c
 *
 *  Author -
 *	Phillip Dykstra
 *	16 June 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"		/* For malloc */

#define	MAXBUFBYTES	1024*1024	/* max bytes to malloc in buffer space */

unsigned char	*buffer;
int	scanlen;			/* length of infile scanlines */
int	buflines;			/* Number of lines held in buffer */
int	buf_start = -1000;		/* First line in buffer */

float	xnum, ynum;			/* Number of pixels in new file */
float	ulx,uly,urx,ury,lrx,lry,llx,lly;	/* Corners of original file */

#define	round(x) ((int)(x+0.5))

void	init_buffer(), fill_buffer();

FILE	*ifp, *ofp;

static char usage[] = "\
Usage: bwcrop in.bw out.bw (I prompt!)\n\
   or  bwcrop in.bw out.bw inwidth outwidth outheight\n\
        ulx uly urx ury lrx lry llx lly\n";

main(argc, argv)
int argc; char **argv;
{
	float	row, col, x1, y1, x2, y2, x, y;
	int	yindex;
	char	value;

	if (argc < 3) {
		fprintf( stderr, usage );
		exit( 1 );
	}
	if ((ifp = fopen(argv[1], "r")) == NULL) {
		fprintf( stderr, "bwcrop: can't open %s\n", argv[1] );
		exit( 2 );
	}
	if ((ofp = fopen(argv[2], "w")) == NULL) {
		fprintf( stderr, "bwcrop: can't open %s\n", argv[1] );
		exit( 3 );
	}

	if( argc == 14 ) {
		scanlen = atoi( argv[3] );
		xnum = atoi( argv[4] );
		ynum = atoi( argv[5] );
		ulx = atoi( argv[6] );
		uly = atoi( argv[7] );
		urx = atoi( argv[8] );
		ury = atoi( argv[9] );
		lrx = atoi( argv[10] );
		lry = atoi( argv[11] );
		llx = atoi( argv[12] );
		lly = atoi( argv[13] );
	} else {
		/* Get info */
		printf("Scanline length in input file: ");
		scanf( "%d", &scanlen );
		if( scanlen <= 0 ) {
			fprintf(stderr,"bwcrop: scanlen = %d, don't be ridiculous\n", scanlen );
			exit( 4 );
		}
		printf("Line Length and Number of scan lines (in new file)?: ");
		scanf( "%f%f", &xnum, &ynum );
		printf("Upper left corner in input file (x,y)?: ");
		scanf( "%f%f", &ulx, &uly );
		printf("Upper right corner (x,y)?: ");
		scanf( "%f%f", &urx, &ury );
		printf("Lower right (x,y)?: ");
		scanf( "%f%f", &lrx, &lry );
		printf("Lower left (x,y)?: ");
		scanf( "%f%f", &llx, &lly );
	}

	/* See how many lines we can buffer */
	init_buffer( scanlen );

	/* Check for silly buffer syndrome */
	if( abs((int)(ury - uly)) > buflines/2 || abs((int)(lry - lly)) > buflines/2 ) {
		fprintf( stderr, "bwcrop: Warning: You are skewing enough in the y direction\n" );
		fprintf( stderr, "bwcrop: relative to my buffer size that I will exhibit silly\n" );
		fprintf( stderr, "bwcrop: buffer syndrome (two replacements per scanline).\n" );
		fprintf( stderr, "bwcrop: Recompile me or use a smarter algorithm.\n" );
	}

	/* Move all points */
	for (row = 0; row < ynum; row++) {
		/* calculate left point of row */
		x1 = ((ulx-llx)/(ynum-1)) * row + llx;
		y1 = ((uly-lly)/(ynum-1)) * row + lly;
		/* calculate right point of row */
		x2 = ((urx-lrx)/(ynum-1)) * row + lrx;
		y2 = ((ury-lry)/(ynum-1)) * row + lry;

		for (col = 0; col < xnum; col++) {
			/* calculate point along row */
			x = ((x2-x1)/(xnum-1)) * col + x1;
			y = ((y2-y1)/(xnum-1)) * col + y1;

			/* Make sure we are in the buffer */
			yindex = round(y) - buf_start;
			if( yindex < 0 || yindex >= buflines ) {
				fill_buffer( round(y) );
				yindex = round(y) - buf_start;
			}

			value = buffer[ yindex * scanlen + round(x) ];
			fwrite(&value, sizeof(value), 1, ofp);
		}
	}
	return (0);
}

/*
 * Determine max number of lines to buffer.
 *  and malloc space for it.
 *  XXX - CHECK FILE SIZE
 */
void
init_buffer( scanlen )
int scanlen;
{
	int	max;

	/* See how many we could buffer */
	max = MAXBUFBYTES / scanlen;

	/*
	 * Do a max of 512.  We really should see how big
	 * the input file is to decide if we should buffer
	 * less than our max.
	 */
	if( max > 512 ) max = 512;

	buflines = max;
	buffer = (unsigned char *)malloc( buflines * scanlen );
}

/*
 * Load the buffer with scan lines centered around
 * the given y coordinate.
 */
void
fill_buffer( y )
int y;
{
	buf_start = y - buflines/2;
	if( buf_start < 0 ) buf_start = 0;

	fseek( ifp, buf_start * scanlen, 0 );
	fread( buffer, scanlen, buflines, ifp );
}
