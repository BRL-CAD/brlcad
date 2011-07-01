/*                          D F F T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
#define MAXOUT 2048		/* MAXFFT/2 XXX (Actually + 1) */

double data[MAXFFT];		/* Data buffer: 2*Points in spectrum */

double mindB = -120.0;
int lflag = 0;
int cflag = 0;
int phase = 0;
int linear_output = 0;
int ascii_output = 0;
int normalize_output = 0;

double cbfilter[27];
void cbweights(double *filter, int window, int points);
double cbsum;
void fftdisp(double *dat, int N);
void fftmag2(double *mags, double *dat, int N);
void fftphase(double *dat, int N);
void rfft();
void LintoLog(double *in, double *out, int num);

static const char usage[] = "\
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
    int i, n, c;
    int L = 1024;

    if (isatty(STDIN_FILENO) || isatty(STDOUT_FILENO)) {
	bu_exit(1, "%s", usage);
    }

    while ((c = bu_getopt(argc, argv, "d:clpLANh")) != -1)
	switch (c) {
	    case 'd': mindB = -atof(bu_optarg); break;
	    case 'c': cflag++; break;
	    case 'l': lflag++; break;
	    case 'p': phase++; break;
	    case 'L': linear_output++; break;
	    case 'A': ascii_output++; break;
	    case 'N': normalize_output++; break;
	    case 'h': printf("%s", usage); return EXIT_SUCCESS;
	    case ':': printf("Missing argument to %c\n%s\n", c, usage); return EXIT_FAILURE;
	    case '?':
	    default:  printf("Unknown argument: %c\n%s\n", c, usage); return EXIT_FAILURE;
	}

    if (L > MAXFFT) {
	bu_exit(2, "dfft: can't go over %d\n", MAXFFT);
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
	    fftdisp(data, L);
    }

    return 0;
}

void
fftdisp(double *dat, int N)
{
    int i, j;
    double mags[MAXOUT];
    size_t ret;

    /* Periodogram scaling */
    for (i = 0; i < N; i++)
	dat[i] /= (double)N;

    fftmag2(mags, dat, N);

    /* Interp to Log freq scale */
    if (lflag) {
	double logout[MAXOUT+1];

	LintoLog(mags, logout, N/2);
	/* put result back in mags */
	for (i = 0; i < N/2; i++)
	    mags[i] = logout[i];
    }

    /* Critical Band Filter */
    if (cflag) {
	double sum;
	double tmp[MAXOUT];

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

#if 0
	/* normalize dB range from 0 to 1 */
	value = (dB/mindB) + 1.0;
	if (value < 0) value = 0;
	else if (value > 1.0) value = 1.0;
#endif

	ret = fwrite(mags, sizeof(*mags), N/2, stdout);
	if (ret != (size_t)(N/2))
	    perror("fwrite");
    }
}

void
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

void
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
