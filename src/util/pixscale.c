/*                      P I X S C A L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file pixscale.c
 *
 *  Scale an RGB pix file.
 *
 *  To scale up, we use bilinear interpolation.
 *  To scale down, we assume "square pixels" and preserve the
 *  amount of light energy per unit area.
 *
 *  This is a buffered version that can handle files of
 *  almost arbitrary size.
 *
 *  Note: This is a simple extension to bwcrop.  Improvements made
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int	bu_getopt(int, char *const *, const char *);
extern char	*bu_optarg;
extern int	bu_optind;

#define	MAXBUFBYTES	3*1024*1024	/* max bytes to malloc in buffer space */

unsigned char	*outbuf;
unsigned char	*buffer;
int	scanlen;			/* length of infile (and buffer) scanlines */
int	buflines;			/* Number of lines held in buffer */
int	buf_start = -1000;		/* First line in buffer */

int	bufy;				/* y coordinate in buffer */
FILE	*buffp;
static char	*file_name;

int	rflag = 0;
int	inx = 512;
int	iny = 512;
int	outx = 512;
int	outy = 512;

void	init_buffer(int scanlen), fill_buffer(int y), binterp(FILE *ofp, int ix, int iy, int ox, int oy), ninterp(FILE *ofp, int ix, int iy, int ox, int oy);

static	char usage[] = "\
Usage: pixscale [-h] [-r] [-s squareinsize] [-w inwidth] [-n inheight]\n\
	[-S squareoutsize] [-W outwidth] [-N outheight] [in.pix] > out.pix\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "rhs:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'r':
			/* pixel replication */
			rflag = 1;
			break;
		case 'h':
			/* high-res */
			inx = iny = 1024;
			break;
		case 'S':
			/* square size */
			outx = outy = atoi(bu_optarg);
			break;
		case 's':
			/* square size */
			inx = iny = atoi(bu_optarg);
			break;
		case 'W':
			outx = atoi(bu_optarg);
			break;
		case 'w':
			inx = atoi(bu_optarg);
			break;
		case 'N':
			outy = atoi(bu_optarg);
			break;
		case 'n':
			iny = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	/* XXX - backward compatability hack */
	if( bu_optind+5 == argc ) {
		file_name = argv[bu_optind++];
		if( (buffp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixscale: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		inx = atoi(argv[bu_optind++]);
		iny = atoi(argv[bu_optind++]);
		outx = atoi(argv[bu_optind++]);
		outy = atoi(argv[bu_optind++]);
		return(1);
	}
	if( bu_optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		buffp = stdin;
	} else {
		file_name = argv[bu_optind];
		if( (buffp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"pixscale: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "pixscale: excess argument(s) ignored\n" );

	return(1);		/* OK */
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
int
scale(FILE *ofp, int ix, int iy, int ox, int oy)
{
	int	i, j, k, l;
	double	pxlen, pylen;			/* # old pixels per new pixel */
	double	xstart, xend, ystart, yend;	/* edges of new pixel in old coordinates */
	double	xdist, ydist;			/* length of new pixel sides in old coord */
	double	sumr, sumg, sumb;
	unsigned char *op;

	if( ix == ox )
		pxlen = 1.0;
	else
		pxlen = (double)ix / (double)ox;
	if( iy == oy )
		pylen = 1.0;
	else
		pylen = (double)iy / (double)oy;
	if ( (pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0) ) {
		fprintf( stderr, "pixscale: can't stretch one way and compress another!\n" );
		return( -1 );
	}
	if( pxlen < 1.0 || pylen < 1.0 ) {
		if( rflag ) {
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
			*op++ = (int)(sumr / (pxlen * pylen));
			*op++ = (int)(sumg / (pxlen * pylen));
			*op++ = (int)(sumb / (pxlen * pylen));
		}
		fwrite( outbuf, 3, ox, ofp );
	}
	return( 1 );
}

int
main(int argc, char **argv)
{
	int i;

	if ( !get_args( argc, argv ) || isatty(fileno(stdout)) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0 ) {
		fprintf( stderr, "pixscale: bad size\n" );
		exit( 2 );
	}

	/* See how many lines we can buffer */
	scanlen = 3 * inx;
	init_buffer( scanlen );
	if (inx < outx) i = outx * 3;
	else i = inx * 3;

	if( (outbuf = malloc(i)) == NULL )
		exit( 4 );

	/* Here we go */
	i = scale( stdout, inx, iny, outx, outy );
	free( outbuf );
	free( buffer );
	return( 0 );
}

/*
 * Determine max number of lines to buffer.
 *  and malloc space for it.
 *  XXX - CHECK FILE SIZE
 */
void
init_buffer(int scanlen)
{
	int	max;

	/* See how many we could buffer */
	max = MAXBUFBYTES / scanlen;

	/*
	 * Do a max of 512.  We really should see how big
	 * the input file is to decide if we should buffer
	 * less than our max.
	 */
	if( max > 4096 ) max = 4096;

	buflines = max;
	buf_start = (-buflines);
	buffer = malloc( buflines * scanlen );
}

/*
 * Load the buffer with scan lines centered around
 * the given y coordinate.
 */
void
fill_buffer(int y)
{
	static int	file_pos = 0;

	buf_start = y - buflines/2;
	if( buf_start < 0 ) buf_start = 0;

	if( file_pos != buf_start * scanlen )  {
		if( fseek( buffp, buf_start * scanlen, 0 ) < 0 ) {
			fprintf( stderr, "pixscale: Can't seek to input pixel! y=%d\n", y );
			exit( 3 );
		}
		file_pos = buf_start * scanlen;
	}
	fread( buffer, scanlen, buflines, buffp );
	file_pos += buflines * scanlen;
}


/*
 * Bilinear Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
binterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
	int	i, j;
	double	x, y, dx, dy, mid1, mid2;
	double	xstep, ystep;
	register unsigned char *op, *up, *lp;

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

			lp = &buffer[bufy*scanlen+(int)x*3];
			up = &buffer[(bufy+1)*scanlen+(int)x*3];

			/* Red */
			mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
			mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
			*op++ = mid1 + dy * (mid2 - mid1);
			lp++; up++;

			/* Green */
			mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
			mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
			*op++ = mid1 + dy * (mid2 - mid1);
			lp++; up++;

			/* Blue */
			mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
			mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
			*op++ = mid1 + dy * (mid2 - mid1);
		}

		(void) fwrite( outbuf, 3, ox, ofp );
	}
}

/*
 * Nearest Neighbor Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
ninterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
	int	i, j;
	double	x, y;
	double	xstep, ystep;
	unsigned char *op, *lp;

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
			lp = &buffer[bufy*scanlen+(int)x*3];
			*op++ = lp[0];
			*op++ = lp[1];
			*op++ = lp[2];
		}

		(void) fwrite( outbuf, 3, ox, ofp );
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
