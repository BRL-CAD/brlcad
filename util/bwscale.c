/*
 *		B W S C A L E . C
 *
 *  Scale a black and white picture.
 *
 *  To scale up, we use bilinear interpolation.
 *  To scale down, we assume "square pixels" and preserve the
 *  amount of light energy per unit area.
 *
 *  This is a buffered version that can handle files of
 *  almost arbitrary size.
 *  Note: this buffer code also appears in bwcrop.c
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

#include <stdio.h>

unsigned char *malloc();

#define	MAXBUFBYTES	1024*1024	/* max bytes to malloc in buffer space */

unsigned char	*outbuf;
unsigned char	*buffer;
int	scanlen;			/* length of infile (and buffer) scanlines */
int	buflines;			/* Number of lines held in buffer */
int	buf_start = -1000;		/* First line in buffer */

int	bufy;				/* y coordinate in buffer */
FILE	*buffp;
int	nflag = 0;

void	init_buffer(), fill_buffer(), binterp(), ninterp();

static char usage[] = "\
Usage: bwscale [-n] infile.bw inx iny outx outy >out.bw\n";

main( argc, argv )
int argc; char **argv;
{
	int	inx, iny, outx, outy;

	while( argc > 1 ) {
		if( strcmp(argv[1],"-n") == 0 ) {
			nflag++;
		} else
			break;
		argc--;
		argv++;
	}

	if( argc != 6 ) {
		fprintf( stderr, usage );
		exit( 1 );
	}
	inx = atoi( argv[2] );
	iny = atoi( argv[3] );
	outx = atoi( argv[4] );
	outy = atoi( argv[5] );
	if( inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0 ) {
		fprintf( stderr, "bwscale: bad size\n" );
		exit( 2 );
	}

	if( (buffp = fopen( argv[1], "r" )) == NULL ) {
		fprintf( stderr, "bwscale: can't open \"%s\"\n", argv[1] );
		exit( 3 );
	}

	/* See how many lines we can buffer */
	scanlen = inx;
	init_buffer( scanlen );
	if( (outbuf = malloc(scanlen)) == NULL )
		exit( 4 );

	/* Here we go */
	scale( stdout, inx, iny, outx, outy );
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
	buffer = malloc( buflines * scanlen );
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

	fseek( buffp, buf_start * scanlen, 0 );
	fread( buffer, scanlen, buflines, buffp );
}

/****** THIS PROBABLY SHOULD BE ELSEWHERE *******/

/* ceiling and floor functions for positive numbers */
#define	CEILING(x)	(((x) > (int)(x)) ? (int)(x)+1 : (int)(x))
#define	FLOOR(x)	((int)(x))
#define	MIN(x,y)	(((x) > (y)) ? (y) : (x))

/*
 * Scale a file of pixels to a different size.
 *
 * To scale down we make a square pixel assumption.
 * We will preserve the amount of light energy per unit area.
 * To scale up we use bilinear interpolation.
 */
scale( ofp, ix, iy, ox, oy )
FILE	*ofp;
int	ix, iy, ox, oy;
{
	int	i, j, k, l;
	double	pxlen, pylen;			/* # old pixels per new pixel */
	double	xstart, xend, ystart, yend;	/* edges of new pixel in old coordinates */
	double	xdist, ydist;			/* length of new pixel sides in old coord */
	double	sum;
	unsigned char *op;

	pxlen = (double)ix / (double)ox;
	pylen = (double)iy / (double)oy;
	if ( (pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0) ) {
		fprintf( stderr, "bwscale: can't stretch one way and compress another!\n" );
		return( -1 );
	}
	if( pxlen < 1.0 || pylen < 1.0 ) {
		if( nflag ) {
			/* nearest neighbor interpolate */
			ninterp( ofp, ix, iy, ox, oy );
		} else {
			/* bilinear interpolate */
			binterp( ofp, ix, iy, ox, oy );
		}
		return( 0 );
	}

	/* for each output pixel */
	for( j = 0; j < oy; j++ ) {
		ystart = j * pylen;
		yend = ystart + pylen;
		op = outbuf;
		for( i = 0; i < ox; i++ ) {
			xstart = i * pxlen;
			xend = xstart + pxlen;
			sum = 0.0;
			/*
			 * For each pixel of the original falling
			 *  inside this new pixel.
			 */
			for( l = FLOOR(ystart); l < CEILING(yend); l++ ) {

				/* Make sure we have this row in the buffer */
				bufy = l - buf_start;
				if( bufy < 0 || bufy >= buflines ) {
					fill_buffer( l );
					bufy = l - buf_start;
				}

				/* Compute height of this row */
				if( (double)l < ystart )
					ydist = CEILING(ystart) - ystart;
				else
					ydist = MIN( 1.0, yend - (double)l );

				for( k = FLOOR(xstart); k < CEILING(xend); k++ ) {
					/* Compute width of column */
					if( (double)k < xstart )
						xdist = CEILING(xstart) - xstart;
					else
						xdist = MIN( 1.0, xend - (double)k );

					/* Add this pixels contribution */
					/* sum += old[l][k] * xdist * ydist; */
					sum += buffer[bufy * scanlen + k] * xdist * ydist;
				}
			}
			*op++ = (int)(sum / (pxlen * pylen));
		}
		(void) fwrite( outbuf, 1, ox, ofp );
	}
	return( 1 );
}

/*
 * Bilinear Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
binterp( ofp, ix, iy, ox, oy )
FILE	*ofp;
int	ix, iy, ox, oy;
{
	int	i, j;
	double	x, y, dx, dy, mid1, mid2;
	double	xstep, ystep;
	unsigned char *op, *up, *lp;

	xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
	ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

	/* For each output pixel */
	for( j = 0; j < oy; j++ ) {
		y = j * ystep;
		/*
		 * Make sure we have this row (and the one after it)
		 * in the buffer
		 */
		bufy = (int)y - buf_start;
		if( bufy < 0 || bufy >= buflines-1 ) {
			fill_buffer( (int)y );
			bufy = (int)y - buf_start;
		}

		op = outbuf;

		for( i = 0; i < ox; i++ ) {
			x = i * xstep;
			dx = x - (int)x;
			dy = y - (int)y;

			/* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

			lp = &buffer[bufy*scanlen+(int)x];
			up = &buffer[(bufy+1)*scanlen+(int)x];

			mid1 = lp[0] + dx * ((double)lp[1] - (double)lp[0]);
			mid2 = up[0] + dx * ((double)up[1] - (double)up[0]);
			*op++ = mid1 + dy * (mid2 - mid1);
		}

		(void) fwrite( outbuf, 1, ox, ofp );
	}
}

/*
 * Nearest Neighbor Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
ninterp( ofp, ix, iy, ox, oy )
FILE	*ofp;
int	ix, iy, ox, oy;
{
	int	i, j;
	double	x, y, dx, dy, mid1, mid2;
	double	xstep, ystep;
	unsigned char *op, *up, *lp;

	xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
	ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

	/* For each output pixel */
	for( j = 0; j < oy; j++ ) {
		y = j * ystep;
		/*
		 * Make sure we have this row (and the one after it)
		 * in the buffer
		 */
		bufy = (int)y - buf_start;
		if( bufy < 0 || bufy >= buflines-1 ) {
			fill_buffer( (int)y );
			bufy = (int)y - buf_start;
		}

		op = outbuf;

		for( i = 0; i < ox; i++ ) {
			x = i * xstep;
			lp = &buffer[bufy*scanlen+(int)x];
			*op++ = lp[0];
		}

		(void) fwrite( outbuf, 1, ox, ofp );
	}
}
