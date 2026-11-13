/*                          D W I N . C
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
/** @file dwin.c
 *
 * Extract sliding windows of double values.
 * Apply window functions if desired.
 *
 */

#include "common.h"

#include <limits.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"


/* external functions */
void hamwin(double *data, int length); /* in hamwin.c */
void coswin(double *data, int length, double percent); /* in coswin.c */


#define START_IN_BUFFER (xform_start < buf_start+buf_num)
#define END_NOT_IN_BUFFER (xform_end >= buf_start+buf_num)
#define BSIZE 16*1024
#define DEFAULT_WIDTH 1024

struct dwin_args {
    int help;
    int cosine;
    int hamming;
    int bartlett;
    int bias;
    int end_aligned;
    int midpoint_aligned;
};

static const struct bu_arg_option dwin_options[] = {
    BU_ARG_FLAG("h", "help", struct dwin_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("w", "cosine", struct dwin_args, cosine,
	"Apply an 80-percent split-cosine window"),
    BU_ARG_FLAG("H", "hamming", struct dwin_args, hamming,
	"Apply a Hamming window"),
    BU_ARG_FLAG("b", "bartlett", struct dwin_args, bartlett,
	"Apply a Bartlett triangular window"),
    BU_ARG_FLAG("B", "bias", struct dwin_args, bias,
	"Apply a half-triangular bias window"),
    BU_ARG_FLAG("e", "end-aligned", struct dwin_args, end_aligned,
	"Align the first window's end with the first input sample"),
    BU_ARG_FLAG("m", "midpoint-aligned", struct dwin_args, midpoint_aligned,
	"Align the first window's midpoint with the first input sample"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand dwin_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("width", 0, 1, 1, 1, 1, BSIZE,
	"Samples per output window (default: 1024)", NULL),
    BU_ARG_OPERAND("step", BU_ARG_VALUE_NUMBER, 0, 1,
	"Positive advance in samples, or a width fraction below 1", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema dwin_schema = BU_ARG_SCHEMA(
    "dwin", "Extract and optionally taper sliding windows of doubles",
    dwin_options, dwin_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar dwin_grammar = {&dwin_schema, NULL, NULL};


/*
 * Buffering stuff
 */
static double buf[BSIZE];		/* input data buffer */
static double temp[BSIZE];		/* windowed data buffer */
static int input_sample = 0;	/* The *next* input sample ("file pointer") */

static int buf_start = 0;		/* sample number in buf[0] */
static int buf_num = 0;		/* number of samples currently in buffer */
static int buf_index = 0;		/* buffer offset for current window */


/*
 * Move input pointer to sample n.
 * Since we may be reading from a pipe, we actually
 * read and discard the samples.
 * Can only seek forward.
 */
static void
seek_sample(int n)
{
    double foo;
    size_t ret;

    fprintf(stderr, "seeking sample %d\n", n);
    while (input_sample < n) {
	ret = fread(&foo, sizeof(foo), 1, stdin);
	if (ret != 1)
	    perror("fread");
	input_sample++;
    }
}


/*
 * Fill the data buffer from the current input location.
 */
static void
fill_buffer(void)
{
    int n, num_to_read;

    num_to_read = BSIZE - buf_num;

#ifdef DEBUG
    fprintf(stderr, "fillbuffer: buf_start = %d, buf_num = %d, numtoread = %d, buf_index = %d\n",
	    buf_start, buf_num, num_to_read, buf_index);
#endif /* DEBUG */
    n = fread(&buf[buf_num], sizeof(*buf), num_to_read, stdin);
    if (n == 0) {
	/*fprintf(stderr, "EOF\n");*/
	memset((char *)&buf[buf_num], 0, sizeof(*buf)*num_to_read);
	return;
    }
    input_sample += n;
    buf_num += n;
    if (n < num_to_read) {
	memset((char *)&buf[buf_num], 0, sizeof(*buf)*(num_to_read-n));
	clearerr(stdin);	/* XXX HACK */
    }

#ifdef DEBUG
    fprintf(stderr, "filled buffer now has %d samples, [%d (%d)].  Input at %d\n", buf_num, buf_start, buf_start+buf_num-1, input_sample);
#endif /* DEBUG */
}


/* Bias window (half triangle) */
static void
biaswin(double *data, int L)
{
    int i;

    for (i = 0; i < L; i++) {
	data[i] *= (double)(L-i)/(double)L;
    }
}


/* Bartlett window (triangle) */
static void
bartwin(double *data, int L)
{
    int i;

    for (i = 0; i < L/2; i++) {
	data[i] *= (double)i/(L/2.0);
    }
    for (i = L/2; i < L; i++) {
	data[i] *= (double)(L-i)/(L/2.0);
    }
}


int
main(int argc, char *argv[])
{
    int L = DEFAULT_WIDTH;
    int step = DEFAULT_WIDTH;
    size_t ret;
    int xform_start = 0;	/* current window start/end */
    int xform_end = 0;
    struct dwin_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &dwin_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    argc--; argv++;
    int operand_index = bu_arg_schema_parse_complete(&dwin_schema, &args,
	&msg, argc, (const char **)argv);
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&dwin_schema, "dwin");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "dwin help");
	return 0;
    }
    argc -= operand_index;
    argv += operand_index;

    int window_count = args.cosine + args.hamming + args.bartlett + args.bias;
    if (window_count > 1)
	bu_exit(1, "dwin: window function options are mutually exclusive\n");
    if (args.end_aligned && args.midpoint_aligned)
	bu_exit(1, "dwin: alignment options are mutually exclusive\n");
    if (argc && !bu_arg_integer_range_from_str(&msg, &L, argv[0], 10,
	1, BSIZE))
	bu_exit(1, "dwin: width: %s", bu_vls_cstr(&msg));
    step = L;
    if (argc > 1) {
	double parsed_step;
	if (!bu_arg_double_from_str(&msg, &parsed_step, argv[1]) ||
	    parsed_step <= 0.0)
	    bu_exit(1, "dwin: step must be a positive finite number\n");
	if (parsed_step < 1.0)
	    parsed_step *= L;
	if (parsed_step < 1.0 || parsed_step > INT_MAX)
	    bu_exit(1, "dwin: step resolves outside the integer range\n");
	step = (int)parsed_step;
    }

    if (isatty(fileno(stdin)) || isatty(fileno(stdout)))
	bu_exit(1, "dwin requires redirected binary input and output\n");

    /* compute xform start/end */
    if (args.end_aligned)
	xform_start = -L + 1;	/* one sample at end */
    else if (args.midpoint_aligned)
	xform_start = -L/2;	/* odd - center, even - just after */
    else
	xform_start = 0;
    xform_end = xform_start + L-1;

    /* initialize data buffer */
    memset((char *)buf, 0, BSIZE*sizeof(*buf));
    buf_start = -BSIZE;
    buf_num = BSIZE;
    buf_index = 0;

    while (!feof(stdin)) {
#ifdef DEBUG
	fprintf(stderr, "\nWant to xform [%d %d]\n", xform_start, xform_end);
	fprintf(stderr, "Buffer contains %d samples, from [%d (%d)]\n", buf_num, buf_start, buf_start+buf_num-1);
#endif /* DEBUG */
	if (START_IN_BUFFER) {
	    buf_index = xform_start - buf_start;
	    if (END_NOT_IN_BUFFER) {
#ifdef DEBUG
		fprintf(stderr, "\tend isn't in buffer.\n");
#endif /* DEBUG */
		/* Move start to origin */
		memmove(&buf[0], &buf[buf_index], (buf_num-buf_index)*sizeof(*buf));
		buf_start = xform_start;
		buf_num -= buf_index;
		buf_index = 0;
		fill_buffer();
	    }
	} else {
#ifdef DEBUG
	    fprintf(stderr, "\tstart isn't in buffer.\n");
#endif /* DEBUG */
	    if (input_sample != xform_start)
		seek_sample(xform_start);
	    buf_start = xform_start;
	    buf_num = 0;
	    buf_index = 0;
	    fill_buffer();
	    if (feof(stdin))
		break;
	}

#ifdef DEBUG
	fprintf(stderr, "Did samples %d to %d (buf_index = %d)\n", xform_start, xform_end, buf_index);
#endif /* DEBUG */
	if (window_count) {
	    memcpy(temp, &buf[buf_index], L*sizeof(*temp));
	    if (args.hamming)
		hamwin(temp, L); /* Hamming window */
	    else if (args.bartlett)
		bartwin(temp, L); /* Bartlett window */
	    else if (args.bias)
		biaswin(temp, L); /* Bias window */
	    else
		coswin(temp, L, 0.80); /* 80% cosine window */
	    ret = fwrite(temp, sizeof(*temp), L, stdout);
	} else {
	    ret = fwrite(&buf[buf_index], sizeof(*buf), L, stdout);
	}
	if (ret != (size_t)L)
	    perror("fwrite");

	/* Bump out pointers */
	xform_start += step;
	xform_end = xform_start + L-1;
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
