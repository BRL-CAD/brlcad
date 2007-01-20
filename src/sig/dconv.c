/*                         D C O N V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file dconv.c
 *
 *  Fast FFT based convolution
 *
 *  This uses the overlap-save method to achieve a linear convolution
 *  (straight FFT's give you a circular convolution).
 *  An M-point kernel is convolved with N-point sequences (in xform space).
 *  The first M-1 points are incorrect, while the remaining points yield
 *  a true linear convolution.  Thus the first M-1 points of each xform
 *  are thrown away, while the last M-1 points of each input section
 *  are saved for the next xform.
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"

#define	MAXM	4096

void	rfft256();
void	rfft();
void	irfft256();
void	irfft();

double	savebuffer[MAXM-1];
double	xbuf[2*MAXM];
double	ibuf[2*MAXM];		/* impulse response */

void	mult(double *o, double *b, int n);

static char usage[] = "\
Usage: dconv filter < doubles > doubles\n\
 XXX Warning: kernal size must be 2^i - 1\n";

int main(int argc, char **argv)
{
	int	i;
	int	N, M, L;
	FILE	*fp;

	M = 128;	/* kernel size */
	N = 2*M;	/* input sub-section length (fft size) */
	L = N - M + 1;	/* number of "good" points per section */

	if( argc != 2 || isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

#ifdef never
	/* prepare the kernel(!) */
	/* this is either the direct complex response,
	 *  or the FT(impulse resp)
	 */
	for( i = 0; i < N; i++ ) {
		if( i <= N/2 )
			ibuf[i] = 1.0;	/* Real part */
		else
			ibuf[i] = 0.0;	/* Imag part */
	}
#endif /* never */

	if( (fp = fopen( argv[1], "r" )) == NULL ) {
		fprintf( stderr, "dconv: can't open \"%s\"\n", argv[1] );
		exit( 2 );
	}
	if( (M = fread( ibuf, sizeof(*ibuf), 2*MAXM, fp )) == 0 ) {
		fprintf( stderr, "dconv: problem reading filter file\n" );
		exit( 3 );
	}
	fclose( fp );
	if( M > MAXM ) {
		fprintf( stderr, "dconv: only compiled for up to %d sized filter kernels\n", MAXM );
		exit( 4 );
	}
/*XXX HACK HACK HACK HACK XXX*/
/* Assume M = 2^i - 1 */
M += 1;
	N = 2*M;	/* input sub-section length (fft size) */
	L = N - M + 1;	/* number of "good" points per section */

	if( N == 256 )
		rfft256( ibuf );
	else
		rfft( ibuf, N );

	while( (i = fread(&xbuf[M-1], sizeof(*xbuf), L, stdin)) > 0 ) {
		if( i < L ) {
			/* pad the end with zero's */
			bzero( (char *)&xbuf[M-1+i], (L-i)*sizeof(*savebuffer) );
		}
		bcopy( savebuffer, xbuf, (M-1)*sizeof(*savebuffer) );
		bcopy( &xbuf[L], savebuffer, (M-1)*sizeof(*savebuffer) );

		/*xform( xbuf, N );*/
		if( N == 256 )
			rfft256( xbuf );
		else
			rfft( xbuf, N );

		/* Mult */
		mult( xbuf, ibuf, N );

		/*invxform( xbuf, N );*/
		if( N == 256 )
			irfft256( xbuf );
		else
			irfft( xbuf, N );

		fwrite( &xbuf[M-1], sizeof(*xbuf), L, stdout );
	}

	return 0;
}

/*
 *  Multiply two "real valued" spectra of length n
 *  and put the result in the first.
 *  The order is: [Re(0),Re(1)...Re(N/2),Im(N/2-1),...,Im(1)]
 *    so for: 0 < i < n/2, (x[i],x[n-i]) is a complex pair.
 */
void
mult(double *o, double *b, int n)
{
	int	i;
	double	r;

	/* do DC and Nyquist components */
	o[0] *= b[0];
	o[n/2] *= b[n/2];

	for( i = 1; i < n/2; i++ ) {
		r = o[i] * b[i] - o[n-i] * b[n-i];
		o[n-i] = o[i] * b[n-i] + o[n-i] * b[i];
		o[i] = r;
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
