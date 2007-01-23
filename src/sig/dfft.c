/*                          D F F T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file dfft.c
 *
 *  Compute FFT's of a stream of doubles (Real data).
 *
 *  Presently 512 point spectrum only.
 *  Which means we need a 1K data segment to get spectrum at midpoint.
 */
#include "common.h"


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "./complex.h"

#define	MAXFFT	4096
#define	MAXOUT	2048		/* MAXFFT/2 XXX (Actually + 1) */

double	data[MAXFFT];		/* Data buffer: 2*Points in spectrum */

double	mindB = -120.0;
int	lflag = 0;
int	cflag = 0;
int	phase = 0;
int	linear_output = 0;
int	ascii_output = 0;
int	normalize_output = 0;

double	cbfilter[27];
void	cbweights(double *filter, int window, int points);
double	cbsum;
void	fftdisp(double *dat, int N);
void	fftmag2(double *mags, double *dat, int N);
void	fftphase(double *dat, int N);
void	rfft();
void	LintoLog(double *in, double *out, int num);

char usage[] = "\
Usage: dfft [options] [width (1024)] < doubles > 512logmags\n\
  -d dB  minimum dB (default 120)\n\
  -l     log frequency scale\n\
  -c     critical band filter (3rd octave)\n\
  -N	 normalized PSD to max magnitude\n\
  -L	 linear output (no dB mag)\n\
  -A     ascii output\n\
";

int main(int argc, char **argv)
{
	int	i, n;
	int	L;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	while( argc > 1 ) {
		if( strcmp(argv[1], "-d") == 0 ) {
			mindB = -atof( argv[2] );
			argc--;
			argv++;
		} else if( strcmp(argv[1], "-c") == 0 ) {
			cflag++;
		} else if( strcmp(argv[1], "-l") == 0 ) {
			lflag++;
		} else if( strcmp(argv[1], "-p") == 0 ) {
			phase++;
		} else if( strcmp(argv[1], "-L") == 0 ) {
			linear_output++;
		} else if( strcmp(argv[1], "-A") == 0 ) {
			ascii_output++;
		} else if( strcmp(argv[1], "-N") == 0 ) {
			normalize_output++;
		} else
			break;
		argc--;
		argv++;
	}

	L = (argc > 1) ? atoi(argv[1]) : 1024;
	if( L > MAXFFT ) {
		fprintf( stderr, "dfft: can't go over %d\n", MAXFFT );
		exit( 2 );
	}

	/* Calculate Critical Band filter weights */
	if (cflag) {
		cbweights(&cbfilter[0], L, 19);
		cbsum = 0.0;
		for (i = 0; i < 19; i++)
			cbsum += cbfilter[i];
	}

	while ((n = fread(data, sizeof(*data), L, stdin)) > 0) {
		if (n != L) {
			fprintf( stderr, "dfft: warning - partial record, adding %d zeros\n", L-n );
			bzero( (char *)&data[n], L-n);
		}

		/* Do a spectrum */
		/* if (L == 256) rfft256(data); etc. XXX */
		rfft(data, L);

		/* Put it on screen */
		if (phase)
			fftphase(data, L);
		else
			fftdisp(data, L);
	}

	return 0;
}

void
fftdisp(double *dat, int N)
{
	int	i, j;
	double	mags[MAXOUT];

	/* Periodogram scaling */
	for( i = 0; i < N; i++ )
		dat[i] /= (double)N;

	fftmag2( mags, dat, N );

	/* Interp to Log freq scale */
	if( lflag ) {
		double	logout[MAXOUT+1];

		LintoLog( mags, logout, N/2 );
		/* put result back in mags */
		for( i = 0; i < N/2; i++ )
			mags[i] = logout[i];
	}

	/* Critical Band Filter */
	if( cflag ) {
		double	sum;
		double	tmp[MAXOUT];

		/* save working copy */
		for( i = 0; i < N/2; i++ )
			tmp[i] = mags[i];

		/* filter it */
		for( i = 0+9; i < N/2-9; i++ ) {
			sum = 0.0;
			for( j = -9; j <= 9; j++ )
				sum += tmp[i+j] * cbfilter[j+9];
			mags[i] = sum / cbsum;
		}
	}

	if (normalize_output) {
		double max = mags[1];		/* XXX or [0] ? */
		for (i = 1; i < N/2; i++) {
			if (mags[i] > max)
				max = mags[i];
		}
		if (linear_output) {
			for (i = 0; i < N/2; i++) {
				mags[i] /= max;
			}
		} else {
			for (i = 0; i < N/2; i++) {
				mags[i] -= max;
				if (mags[i] < mindB)
					mags[i] = mindB;
			}
		}
	}

	if (ascii_output) {
		for (i = 0; i < N/2; i++) {
			printf("%g %g\n", i/(double)N, mags[i]);
		}
	} else {
#if 0
		/* normalize dB range from 0 to 1 */
		value = (dB/mindB) + 1.0;
		if( value < 0 ) value = 0;
		else if( value > 1.0 ) value = 1.0;
#endif
		fwrite( mags, sizeof(*mags), N/2, stdout );
	}
}

void
fftmag2(double *mags, double *dat, int N)
{
	int	i;
	double	value, dB;

	/* DC */
	mags[0] = dat[0]*dat[0];

	/* Normal */
	for( i = 1; i < N/2; i++ ) {
		mags[i] = dat[i]*dat[i] + dat[N-i]*dat[N-i];
	}

	/* Nyquist */
	mags[N/2] = dat[N/2]*dat[N/2];


	if (linear_output) {
#if 0
		for (i = 0; i <= N/2; i++) {
			mags[i] = sqrt(mags[i]);	/*XXX?*/
		}
#endif
		;
	} else {
		/* Log output */
		for (i = 0; i <= N/2; i++) {
			value = mags[i];
			if( value > 1.0e-18 )
				dB = 10*log10(value);
			else
				dB = -180.0;
			mags[i] = dB;
		}
	}
}

void
fftphase(double *dat, int N)
{
	int	i;
	double	value, out[MAXFFT];

	for( i = 0; i < N; i++ )
		dat[i] /= (double)N;

	for( i = 1; i < N/2; i++ ) {
		value = atan2( dat[N-i], dat[i] );
		out[i] = value / PI;
	}
	/* DC */
	out[i] = 0;

	fwrite( out, sizeof(*out), N/2, stdout );
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
