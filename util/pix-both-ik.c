/*
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

static int pix_line;		/* Number of pixels/line */

char usage[] = "Usage: pix-both-ik [-h] file.pix [width] [fr_offset] [fr_count]\n";

main(argc, argv)
int argc;
char **argv;
{
	static int infd;
	static int nlines;		/* Square:  nlines, npixels/line */
	static int frame_offset;
	static int frame_count;

	if( argc < 2 )  {
		fprintf(stderr,"%s", usage);
		exit(1);
	}

	nlines = 512;
	if( strcmp( argv[1], "-h" ) == 0 )  {
		nlines = 1024;
		argc--; argv++;
	}
	if( (infd = open( argv[1], 0 )) < 0 )  {
		perror( argv[1] );
		exit(3);
	}
	if( argc >= 3 )
		nlines = atoi(argv[2] );
	frame_offset = 0;
	if( argc >= 4 )
		frame_offset = atoi(argv[3]);
	frame_count = 1;
	if( argc >= 5 )
		frame_count = atoi(argv[4]);
	if( nlines > 512 )
		ikhires = 1;

	scanbytes = nlines * 3;
	pix_line = nlines;		/* square */

	(void)lseek( infd, (long)frame_offset*scanbytes*nlines, 0 );

	ikopen();
	ikclear();

	while(frame_count-- > 0)  {
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

doline( out )
register unsigned char *out;
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

avgline( out, i1, i2 )
register unsigned char *out;
register unsigned char *i1;
register unsigned char *i2;
{
	register int i;

	for( i = (pix_line)*2; i > 0; i-- )  {
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		*out++ = (((int)(*i1++)) + (*i2++))>>1;
		out++; i1++; i2++;	/* alpha byte */
	}
}

lowp( op, a, b, c )
register unsigned char *op, *a, *b, *c;
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

ck_diag( a1, l1, a2 )
register unsigned char *a1, *l1, *a2;
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
