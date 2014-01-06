/*                          D F F T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
 * Compute FFT's of a stream of doubles (Real data).
 *
 * Presently 512 point spectrum only, which means we need a 1K data
 * segment to get spectrum at midpoint.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"

#define MAXFFT 4096


/* functions specified elsewhere (these probably belong in a header) */
void cbweights(double *filter, int window, int points); /* in butter.c */
void rfft(double *dat, int N); /* in dfft.c */
void LintoLog(double *in, double *out, int num); /* in interp.c */


/* FIXME: these need to get shoved into a struct and passed */
static double mindB = -120.0;
static int lflag = 0;
static int cflag = 0;
static int linear_output = 0;
static int ascii_output = 0;
static int normalize_output = 0;
static double cbfilter[27];


static void
fftmag2(double *mags, double *dat, int N)
{
    int i;
    double value, dB;

    /* DC */
    mags[0] = dat[0]*dat[0];

    /* Normal */
    for (i = 1; i < N/2; i++) {
	mags[i] = dat[i]*dat[i] + dat[N-i]*dat[N-i];
    }

    /* Nyquist */
    mags[N/2] = dat[N/2]*dat[N/2];


    if (!linear_output) {
	/* Log output */
	for (i = 0; i <= N/2; i++) {
	    value = mags[i];
	    if (value > 1.0e-18)
		dB = 10*log10(value);
	    else
		dB = -180.0;
	    mags[i] = dB;
	}
    }
}


static void
fftdisp(double *dat, int N, double cbsum)
{
    int i, j;
    double mags[MAXFFT];
    size_t ret;

    /* Periodogram scaling */
    for (i = 0; i < N; i++)
	dat[i] /= (double)N;

    fftmag2(mags, dat, N);

    /* Interp to Log freq scale */
    if (lflag) {
	double logout[MAXFFT];

	LintoLog(mags, logout, N/2);
	/* put result back in mags */
	for (i = 0; i < N/2; i++)
	    mags[i] = logout[i];
    }

    /* Critical Band Filter */
    if (cflag) {
	double sum;
	double tmp[MAXFFT];

	/* save working copy */
	for (i = 0; i < N/2; i++)
	    tmp[i] = mags[i];

	/* filter it */
	for (i = 0+9; i < N/2-9; i++) {
	    sum = 0.0;
	    for (j = -9; j <= 9; j++)
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
	ret = fwrite(mags, sizeof(*mags), N/2, stdout);
	if (ret != (size_t)(N/2))
	    perror("fwrite");
    }
}


static void
fftphase(double *dat, int N)
{
    int i;
    double value, out[MAXFFT];
    size_t ret;

    for (i = 0; i < N; i++)
	dat[i] /= (double)N;

    for (i = 1; i < N/2; i++) {
	value = atan2(dat[N-i], dat[i]);
	out[i] = value * M_1_PI;
    }
    /* DC */
    out[i] = 0;

    ret = fwrite(out, sizeof(*out), N/2, stdout);
    if (ret != (size_t)(N/2))
	perror("fwrite");
}


int
main(int argc, char **argv)
{
    static const char usage[] =
	"Usage: dfft [options] [width (1024)] < doubles > 512logmags\n"
	"\n"
	"Options:\n"
	"  -d dB  minimum dB (default 120)\n"
	"  -l     log frequency scale\n"
	"  -c     critical band filter (3rd octave)\n"
	"  -p     phase\n"
	"  -N     normalized PSD to max magnitude\n"
	"  -L     linear output (no dB mag)\n"
	"  -A     ascii output\n"
	;

    static const char optstring[] = "d:clpLANh?";

    int i, n, c;
    int L = 1024;
    double cbsum = 0.0;
    int phase = 0;
    double data[MAXFFT];		/* Data buffer: 2*Points in spectrum */

    while ((c = bu_getopt(argc, argv, optstring)) != -1) {
	switch (c) {
	    case 'd': mindB = -atof(bu_optarg); break;
	    case 'c': cflag++; break;
	    case 'l': lflag++; break;
	    case 'p': phase++; break;
	    case 'L': linear_output++; break;
	    case 'A': ascii_output++; break;
	    case 'N': normalize_output++; break;
	    default:  bu_exit(1, "%s", usage);
	}
    }

    if (isatty(STDIN_FILENO) || isatty(STDOUT_FILENO))
	bu_exit(1, "%s", usage);

    /* Calculate Critical Band filter weights */
    if (cflag) {
	cbweights(&cbfilter[0], L, 19);
	cbsum = 0.0;
	for (i = 0; i < 19; i++)
	    cbsum += cbfilter[i];
    }

    while ((n = fread(data, sizeof(*data), L, stdin)) > 0) {
	if (n != L) {
	    fprintf(stderr, "dfft: warning - partial record, adding %d zeros\n", L-n);
	    memset((char *)&data[n], 0, L-n);
	}

	/* Do a spectrum */
	/* if (L == 256) rfft256(data); etc. XXX */
	rfft(data, L);

	/* Put it on screen */
	if (phase)
	    fftphase(data, L);
	else
	    fftdisp(data, L, cbsum);
    }

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
