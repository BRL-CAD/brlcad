/*                          D F F T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "fft.h"

#define MAXFFT BU_PAGE_SIZE
#define DEFAULT_FFT_WIDTH 1024


/* functions specified elsewhere (these probably belong in a header) */
void cbweights(double *filter, int window, int points); /* in butter.c */
void LintoLog(double *in, double *out, int num); /* in interp.c */


/* FIXME: these need to get shoved into a struct and passed */
static double mindB = -120.0;
static int lflag = 0;
static int cflag = 0;
static int linear_output = 0;
static int ascii_output = 0;
static int normalize_output = 0;
static double cbfilter[27];

struct dfft_args {
    int help;
    double minimum_db;
    int log_frequency;
    int critical_band;
    int phase;
    int normalize;
    int linear;
    int ascii;
};

static const struct bu_arg_option dfft_options[] = {
    BU_ARG_FLAG("h", "help", struct dfft_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_NUMBER_RANGE("d", "minimum-db", struct dfft_args, minimum_db,
	1, 0.0, 0, 0.0, "dB", "Set the normalized output floor in decibels"),
    BU_ARG_FLAG("l", "log-frequency", struct dfft_args, log_frequency,
	"Interpolate magnitudes onto a logarithmic frequency scale"),
    BU_ARG_FLAG("c", "critical-band", struct dfft_args, critical_band,
	"Apply a third-octave critical band filter"),
    BU_ARG_FLAG("p", "phase", struct dfft_args, phase,
	"Output phase instead of magnitude"),
    BU_ARG_FLAG("N", "normalize", struct dfft_args, normalize,
	"Normalize output to its peak magnitude"),
    BU_ARG_FLAG("L", "linear", struct dfft_args, linear,
	"Output linear rather than decibel magnitudes"),
    BU_ARG_FLAG("A", "ascii", struct dfft_args, ascii,
	"Write magnitude output as ASCII frequency/value pairs"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand dfft_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("width", 0, 1, 1, 8, 1, MAXFFT,
	"Power-of-two transform width (default: 1024)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema dfft_schema = BU_ARG_SCHEMA(
    "dfft", "Compute real fast Fourier transforms of a double stream",
    dfft_options, dfft_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar dfft_grammar = {&dfft_schema, NULL, NULL};


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
    out[0] = 0;

    ret = fwrite(out, sizeof(*out), N/2, stdout);
    if (ret != (size_t)(N/2))
	perror("fwrite");
}


int
main(int argc, char **argv)
{
    int i, n;
    int L = DEFAULT_FFT_WIDTH;
    double cbsum = 0.0;
    double data[MAXFFT];		/* Data buffer: 2*Points in spectrum */
    struct dfft_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.minimum_db = 120.0;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &dfft_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&dfft_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&dfft_schema, "dfft");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "dfft help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;

    if (argc && !bu_arg_integer_range_from_str(&msg, &L, argv[0], 10,
	8, MAXFFT))
	bu_exit(1, "dfft: width: %s", bu_vls_cstr(&msg));
    if ((L & (L - 1)) != 0)
	bu_exit(1, "dfft: width must be a power of two, got %d\n", L);

    mindB = -args.minimum_db;
    lflag = args.log_frequency;
    cflag = args.critical_band;
    linear_output = args.linear;
    ascii_output = args.ascii;
    normalize_output = args.normalize;

    if (isatty(STDIN_FILENO) || isatty(STDOUT_FILENO))
	bu_exit(1, "dfft requires redirected binary input and output\n");

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
	    memset((char *)&data[n], 0, (L-n) * sizeof(*data));
	}

	/* Do a spectrum */
	/* if (L == 256) rfft256(data); etc. XXX */
	rfft(data, L);

	/* Put it on screen */
	if (args.phase)
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
