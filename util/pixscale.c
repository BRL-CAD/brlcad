/*
 *		P I X S C A L E . C
 *
 * Scale an RGB pix file.
 *
 * To scale up, we use bilinear interpolation.
 * To scale down, we assume "square pixels" and preserve the
 * amount of light energy per unit area.
 *
 * This is a buffered version that can handle files of
 * almost arbitrary size.
 *
 * Note: This is a simple extension to bwcrop.  Improvements made
 *  there should be incorporated here.
 *
 *  Author -
 *	Phillip Dykstra
 * 	23 Sep 1986
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
#include <stdio.h>

unsigned char *malloc();

#define	MAXBUFBYTES	1024*1024	/* max bytes to malloc in buffer space */

unsigned char	*buffer;
int	scanlen;			/* length of infile (and buffer) scanlines */
int	buflines;			/* Number of lines held in buffer */
int	buf_start = -1000;		/* First line in buffer */

int	bufy;				/* y coordinate in buffer */
FILE	*buffp;

char	*Usage = "usage: pixscale infile.pix inx iny outx outy >out.pix\n";

main( argc, argv )
int argc; char **argv;
{
	int	i, j;
	int	inx, iny, outx, outy;
	unsigned char linebuf[512];

	if( argc != 6 ) {
		fprintf( stderr, Usage );
		exit( 1 );
	}
	inx = atoi( argv[2] );
	iny = atoi( argv[3] );
	outx = atoi( argv[4] );
	outy = atoi( argv[5] );

	if( (buffp = fopen( argv[1], "r" )) == NULL ) {
		fprintf( stderr, "pixscale: can't open \"%s\"\n", argv[1] );
		exit( 2 );
	}

	/* See how many lines we can buffer */
	scanlen = 3 * inx;
	init_buffer( scanlen );

	/* Here we go */
	scale( stdin, inx, iny, stdout, outx, outy );
}

/*
 * Determine max number of lines to buffer.
 *  and malloc space for it.
 *  XXX - CHECK FILE SIZE
 */
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
fill_buffer( y )
int y;
{
	int	i;

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
scale( ifp, ix, iy, ofp, ox, oy )
FILE *ifp, *ofp;
int	ix, iy, ox, oy;
{
	int	i, j, k, l;
	double	pxlen, pylen;			/* # old pixels per new pixel */
	double	xstart, xend, ystart, yend;	/* edges of new pixel in old coordinates */
	double	xdist, ydist;			/* length of new pixel sides in old coord */
	double	sumr, sumg, sumb;
	unsigned char outpixel;

	pxlen = (double)ix / (double)ox;
	pylen = (double)iy / (double)oy;
	if ( (pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0) ) {
		fprintf( stderr, "pixscale: can't stretch one way and compress another!\n" );
		return( -1 );
	}
	if( pxlen < 1.0 || pylen < 1.0 ) {
		/* bilinear interpolate */
		binterp( ifp, ix, iy, ofp, ox, oy );
		return( 0 );
	}

	/* for each output pixel */
	for( j = 0; j < oy; j++ ) {
		ystart = j * pylen;
		yend = ystart + pylen;
		for( i = 0; i < ox; i++ ) {
			xstart = i * pxlen;
			xend = xstart + pxlen;
			sumr = sumg = sumb = 0.0;
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
					sumr += buffer[bufy * scanlen + 3*k] * xdist * ydist;
					sumg += buffer[bufy * scanlen + 3*k+1] * xdist * ydist;
					sumb += buffer[bufy * scanlen + 3*k+2] * xdist * ydist;
				}
			}
			outpixel = (int)(sumr / (pxlen * pylen));
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );
			outpixel = (int)(sumg / (pxlen * pylen));
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );
			outpixel = (int)(sumb / (pxlen * pylen));
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );
		}
	}
	return( 1 );
}

/*
 * Bilinear Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
binterp( ifp, ix, iy, ofp, ox, oy )
FILE *ifp, *ofp;
int	ix, iy, ox, oy;
{
	int	i, j;
	double	x, y, dx, dy, mid1, mid2;
	double	xstep, ystep;
	unsigned char outpixel;

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

		for( i = 0; i < ox; i++ ) {
			x = i * xstep;
			dx = x - (int)x;
			dy = y - (int)y;

			/* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

			/* Red */
			mid1 = (1.0 - dx) * buffer[bufy*scanlen+(int)x*3] + dx * buffer[bufy*scanlen+(int)(x+1)*3];
			mid2 = (1.0 - dx) * buffer[(bufy+1)*scanlen+(int)x*3] + dx * buffer[(bufy+1)*scanlen+(int)(x+1)*3];
			outpixel = (1.0 - dy) * mid1 + dy * mid2;
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );

			/* Green */
			mid1 = (1.0 - dx) * buffer[bufy*scanlen+(int)x*3+1] + dx * buffer[bufy*scanlen+(int)(x+1)*3+1];
			mid2 = (1.0 - dx) * buffer[(bufy+1)*scanlen+(int)x*3+1] + dx * buffer[(bufy+1)*scanlen+(int)(x+1)*3+1];
			outpixel = (1.0 - dy) * mid1 + dy * mid2;
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );

			/* Blue */
			mid1 = (1.0 - dx) * buffer[bufy*scanlen+(int)x*3+2] + dx * buffer[bufy*scanlen+(int)(x+1)*3+2];
			mid2 = (1.0 - dx) * buffer[(bufy+1)*scanlen+(int)x*3+2] + dx * buffer[(bufy+1)*scanlen+(int)(x+1)*3+2];
			outpixel = (1.0 - dy) * mid1 + dy * mid2;
			fwrite( &outpixel, sizeof(outpixel), 1, ofp );
		}
	}
}
