/*                   P I X - F I L T - I K . C
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
/** @file pix-filt-ik.c
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

/* Output lines interwoven:  originals & averages ones */
static char avg1[MAX_LINE*4];	/* Ikonas pixels */
static char out1[MAX_LINE*4];	/* Ikonas pixels */
static char avg2[MAX_LINE*4];	/* Ikonas pixels */
static char out2[MAX_LINE*4];	/* Ikonas pixels */

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

			ck_diag( a1, l1, a2 );

			clustwrite( a1, y*2-1, pix_line*2 );
			clustwrite( l1, y*2-2, pix_line*2 );
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

ck_diag(register unsigned char *a1, register unsigned char *l1, register unsigned char *a2)
{
	static int i;
	static int need_avg;

	a1 += 4;	/* 2nd pixel on avg line (diag from l1) */
	l1 += 4*2;	/* 2nd original pixel (third on line) */
	a2 += 4;	/* 2nd pixel on avg line (diag from l1) */
	for( i=(pix_line-2); i > 0; i--, a1+=(4), a2+=(4), l1+=(4) )  {
		register int d1, d2;
		/*
		 *  If pixel is significantly different
		 *  from BOTH pixels on either of the 2 diagonals,
		 *  then average it along that diagonal.
		 */
#define THRESH 20
		need_avg = 0;
		if( (d1 = a1[2*4] - l1[0]) < 0 )  d1 = (-d1);
		if( (d2 = a2[0] - l1[0]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( (d1 = a1[2*4+1] - l1[0+1]) < 0 )  d1 = (-d1);
		if( (d2 = a2[0+1] - l1[0+1]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( (d1 = a1[2*4+2] - l1[0+2]) < 0 )  d1 = (-d1);
		if( (d2 = a2[0+2] - l1[0+2]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( need_avg )  {
			l1[0] = (((int)a1[2*4+0]) + a2[0]+ l1[0])/3;
			l1[1] = (((int)a1[2*4+1]) + a2[1]+ l1[1])/3;
			l1[2] = (((int)a1[2*4+2]) + a2[2]+ l1[2])/3;
#ifdef DEBUG
			l1[0] = 255; l1[1] = 0; l1[2] = 0;
#endif
			continue;
		}

		need_avg = 0;
		if( (d1 = a1[0] - l1[0]) < 0 )  d1 = (-d1);
		if( (d2 = a2[2*4+0] - l1[0]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( (d1 = a1[1] - l1[0+1]) < 0 )  d1 = (-d1);
		if( (d2 = a2[2*4+1] - l1[0+1]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( (d1 = a1[2] - l1[0+2]) < 0 )  d1 = (-d1);
		if( (d2 = a2[2*4+2] - l1[0+2]) < 0 )  d2 = (-d2);
		if( d1 > THRESH && d2 > THRESH )  need_avg++;

		if( need_avg )  {
			l1[0] = (((int)a1[0]) + a2[2*4+0]+ l1[0])/3;
			l1[1] = (((int)a1[1]) + a2[2*4+1]+ l1[1])/3;
			l1[2] = (((int)a1[2]) + a2[2*4+2]+ l1[2])/3;
#ifdef DEBUG
			l1[0] = 0; l1[1] = 255; l1[2] = 0;
#endif
			continue;
		}
	}
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
