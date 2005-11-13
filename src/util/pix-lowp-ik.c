/*                   P I X - L O W P - I K . C
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
/** @file pix-lowp-ik.c
 *  			P I X - I K . C
 *
 *  Dumb little program to take bottom-up pixel files and
 *  send them to the Ikonas.
 *
 *  Easier than hacking around with RT.
 *
 *  Mike Muuss, BRL, 05/05/84.
 *
 *  $Revision$
 */
#include <stdio.h>

extern int ikfd;
extern int ikhires;

#define MAX_LINE	1024		/* Max pixels/line */
static char scanline[MAX_LINE*3];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

/* Intermediate lines interwoven:  originals & averages ones */
static char avg1[MAX_LINE*4];	/* Ikonas pixels */
static char out1[MAX_LINE*4];	/* Ikonas pixels */
static char avg2[MAX_LINE*4];	/* Ikonas pixels */
static char out2[MAX_LINE*4];	/* Ikonas pixels */

/* Output lines */
static char o1[MAX_LINE*4];
static char o2[MAX_LINE*4];

static int nlines;		/* Number of input lines */
static int pix_line;		/* Number of pixels/line */

char usage[] = "Usage: pix-ik [-h] file.pix [width]\n";

int
main(int argc, char **argv)
{
	static int infd;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		ikhires = 1;
		nlines = 1024;
		argc--; argv++;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc == 3 )
		nlines = atoi(argv[2] );

	pix_line = nlines;	/* Square pictures */
	scanbytes = pix_line * 3;

	ikopen();
	load_map(1);		/* Standard map: linear */
	ikclear();

	while(1)  {
		register int y;
		register char *l1;
		register char *l2;
		char *a1, *a2;

		a1 = avg1;
		l1 = out1;
		a2 = avg2;
		l2 = out2;

		if( read( infd, (char *)scanline, scanbytes ) != scanbytes )
			exit(0);
		/* avg1 is same as l1 to prime things */
		doline( a1 );
		doline( l1 );

		for( y=nlines; y > 0; y-- )  {
			if( read( infd, (char *)scanline, scanbytes ) != scanbytes )
				exit(0);
			doline( l2 );
			avgline( a2, l1, l2 );

			lowp( o1, a1, l1, a2 );
			lowp( o2, l1, a2, l2 );
			clustwrite( o1, y*2-1, pix_line*2 );
			clustwrite( o2, y*2-2, pix_line*2 );
			/* Exchange l1 and l2, so current l2 is next l1 */
			/* Also exchange a1 and a2 */
			{
				register char *t;
				t = l2;
				l2 = l1;
				l1 = t;
				t = a2;
				a2 = a1;
				a1 = t;
			}
		}
	}
}

doline(register unsigned char *out)
{
	register unsigned char *in;
	register int i;

	in = (unsigned char *)scanline;
	/* Left-most pixel */
	*out++ = *in++;
	*out++ = *in++;
	*out++ = *in++;
	*out++ = 0;
	for( i=1; i<pix_line; i++ )  {
		/* Averaged in-between pixel */
		*out++ = (((int)in[-3])+in[0])>>1;
		*out++ = (((int)in[-2])+in[1])>>1;
		*out++ = (((int)in[-1])+in[2])>>1;
		*out++ = 0;
		/* Right hand pixel */
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 0;
	}
	/* Right most pixel repeated at end */
	*out++ = in[-3];
	*out++ = in[-2];
	*out++ = in[-1];
	*out++ = 0;
}

avgline(register unsigned char *out, register unsigned char *i1, register unsigned char *i2)
{
	register int i;

	for( i = (pix_line)*2; i > 0; i-- )  {
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		out++; i1++; i2++;	/* alpha byte */
	}
}

lowp(register unsigned char *op, register unsigned char *a, register unsigned char *b, register unsigned char *c)
{
	register int i;

	*op++ = 0;
	*op++ = 0;
	*op++ = 0;
	*op++ = 0;

	for( i=(pix_line*2)-2; i > 0; i-- )  {
		*op++ = (a[0*4]*3 + a[1*4]*5  + a[2*4]*3 +
			 b[0*4]*5 + b[1*4]*10 + b[2*4]*5 +
			 c[0*4]*3 + c[1*4]*5  + c[2*4]*3 ) / 42;
		a++; b++; c++;
		*op++ = (a[0*4]*3 + a[1*4]*5  + a[2*4]*3 +
			 b[0*4]*5 + b[1*4]*10 + b[2*4]*5 +
			 c[0*4]*3 + c[1*4]*5  + c[2*4]*3 ) / 42;
		a++; b++; c++;
		*op++ = (a[0*4]*3 + a[1*4]*5  + a[2*4]*3 +
			 b[0*4]*5 + b[1*4]*10 + b[2*4]*5 +
			 c[0*4]*3 + c[1*4]*5  + c[2*4]*3 ) / 42;
		a+=2; b+=2; c+=2;
		*op++ = 0;
	}

	*op++ = 0;
	*op++ = 0;
	*op++ = 0;
	*op++ = 0;
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
