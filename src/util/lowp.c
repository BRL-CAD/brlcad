/*                          L O W P . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file lowp.c
 *  Read 3 .pix files, and do a 3D low-pass filter on them.
 *  
 *  Mike Muuss, BRL.
 *
 *  $Revision$
 */
#include <stdio.h>

#define MAX_LINE	1024		/* Max pixels/line */
static int scanbytes;			/* # of bytes of scanline */

unsigned char *in1, *in2, *in3;

/* Output line */
unsigned char out1[MAX_LINE*3];

static int nlines;		/* Number of input lines */
static int pix_line;		/* Number of pixels/line */

char usage[] = "Usage: lowp file1.pix file2.pix file3.pix [width] > out.pix\n";

int infd1, infd2, infd3;		/* input fd's */

int
main(int argc, char **argv)
{
	static int x,y;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( (infd1 = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( (infd2 = open( argv[2], 0 )) < 0 )  {
		perror( argv[2] );
		exit(3);
	}
	if( (infd3 = open( argv[3], 0 )) < 0 )  {
		perror( argv[3] );
		exit(3);
	}
	if( argc == 5 )
		nlines = atoi(argv[4] );

	pix_line = nlines;	/* Square pictures */
	scanbytes = nlines * pix_line * 3;
	in1 = (unsigned char  *) malloc( scanbytes );
	in2 = (unsigned char  *) malloc( scanbytes );
	in3 = (unsigned char  *) malloc( scanbytes );
	if( read( infd1, in1, scanbytes ) != scanbytes )
		exit(0);
	if( read( infd2, in2, scanbytes ) != scanbytes )
		exit(0);
	if( read( infd3, in3, scanbytes ) != scanbytes )
		exit(0);

	/* First and last are black */
	bzero( out1, pix_line*3 );
	write( 1, out1, pix_line*3 );

	for( y=1; y < nlines-2; y++ )  {
		static unsigned char *op;

		op = out1+3;

		/* do (width-2)*3 times, borders are black */
		for( x=3; x < (pix_line-2)*3; x++ )  {
			register int i;
			register unsigned char *a, *b, *c;

			i = (y*pix_line)*3+x;
			a = in1+i;
			b = in2+i;
			c = in3+i;
			i = pix_line*3;
			*op++ = ( (int)
a[-i-3]    + a[-i  ]*3  + a[-i+3]    +
a[  -3]*3  + a[   0]*5  + a[   3]*3  +
a[ i-3]    + a[ i  ]*3  + a[ i+3]    +

b[-i-3]*3  + b[-i  ]*5  + b[-i+3]*3  +
b[  -3]*5  + b[   0]*10 + b[   3]*5  +
b[ i-3]*3  + b[ i  ]*5  + b[ i+3]*3  +

c[-i-3]    + c[-i  ]*3  + c[-i+3]    +
c[  -3]*3  + c[   0]*5  + c[   3]*3  +
c[ i-3]    + c[ i  ]*3  + c[ i+3]
				) / 84;
		}
		write( 1, out1, pix_line*3 );
	}

	/* First and last are black */
	bzero( out1, pix_line*3 );
	write( 1, out1, pix_line*3 );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
