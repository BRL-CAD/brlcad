/*                    P I X 3 F I L T E R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/pix3filter.c
 *
 * Filters a color pix file set with an arbitrary 3x3x3 kernel.
 * Leaves the outer rows untouched.
 * Allows an alternate divisor and offset to be given.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"


#define MAXLINE (8*1024)
#define DEFAULT_WIDTH 512
unsigned char l11[3*MAXLINE], l12[3*MAXLINE], l13[3*MAXLINE];
unsigned char l21[3*MAXLINE], l22[3*MAXLINE], l23[3*MAXLINE];
unsigned char l31[3*MAXLINE], l32[3*MAXLINE], l33[3*MAXLINE];
unsigned char obuf[3*MAXLINE];
unsigned char *topold, *middleold, *bottomold, *temp;
unsigned char *topcur, *middlecur, *bottomcur;
unsigned char *topnew, *middlenew, *bottomnew;

/* The filter kernels */
struct kernels {
    const char *name;
    const char *uname;		/* What is needed to recognize it */
    int kern[27];
    int kerndiv;	/* Divisor for kernel */
    int kernoffset;	/* To be added to result */
} kernel[] = {
    { "Low Pass", "lo", {1, 3, 1, 3, 5, 3, 1, 3, 1,
			 3, 5, 3, 5, 10, 5, 3, 5, 3,
			 1, 3, 1, 3, 5, 3, 1, 3, 1}, 84, 0 },
    { "High Pass", "hi", {-1, -2, -1, -2, -4, -2, -1, -2, -1,
			  -2, -4, -2, -4, 56, -4, -2, -4, -2,
			  -1, -2, -1, -2, -4, -2, -1, -2, -1}, 1, 0},
    { "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1,
			      1, 1, 1, 1, 27, 1, 1, 1, 1,
			      1, 1, 1, 1, 1, 1, 1, 1, 1}, 53, 0},
    { "Animation Smear", "s", {1, 1, 1, 1, 1, 1, 1, 1, 1,
			       1, 1, 1, 1, 1, 1, 1, 1, 1,
			       2, 2, 2, 2, 35, 2, 2, 2, 2}, 69, 0},
    { NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0 },
};


int *kern;
int kerndiv;
int kernoffset;
int width = DEFAULT_WIDTH;
int height = DEFAULT_WIDTH;
int verbose = 0;
FILE *oldfp, *curfp, *newfp;

struct pix3filter_args {
    struct pix3filter_args *self;
    int help;
    int verbose;
    const char *filter;
    int divisor;
    int offset;
    int width;
    int height;
};

static struct pix3filter_args *
pix3filter_args_from_storage(void *storage)
{
    return storage ? *(struct pix3filter_args **)storage : NULL;
}

static int
pix3filter_square(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pix3filter_args *args = pix3filter_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, MAXLINE))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static int
pix3filter_divisor(struct bu_vls *msg, const char *arg, void *storage)
{
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (value == 0) {
	if (msg)
	    bu_vls_printf(msg, "divisor must not be zero\n");
	return -1;
    }
    if (storage)
	*(int *)storage = value;
    return 0;
}

static const struct bu_arg_value_keyword pix3filter_filter_values[] = {
    {"lo", NULL, "Low-pass filter"},
    {"hi", NULL, "High-pass filter"},
    {"b", NULL, "Boxcar average"},
    {"s", NULL, "Animation smear"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option pix3filter_options[] = {
    BU_ARG_FLAG("h", "help", struct pix3filter_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct pix3filter_args, verbose,
	BU_ARG_VALUE_INTEGER, "Print kernel coefficients and result range diagnostics"),
    BU_ARG_KEYWORD_VALUES("f", "filter", struct pix3filter_args, filter,
	"type", "Select the convolution filter", pix3filter_filter_values),
    BU_ARG_CUSTOM("d", "divisor", struct pix3filter_args, divisor,
	pix3filter_divisor, "integer", "Override the nonzero built-in filter divisor"),
    BU_ARG_VALUE("o", "offset", struct pix3filter_args, offset,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "integer",
	"Override the built-in result offset"),
    BU_ARG_INTEGER_RANGE("w", "width", struct pix3filter_args, width,
	1, 1, 1, MAXLINE, "pixels", "Set the input width (default 512)"),
    BU_ARG_INTEGER_RANGE("n", "height", struct pix3filter_args, height,
	1, 1, 1, INT_MAX, "scanlines", "Set the input height (default 512)"),
    BU_ARG_CUSTOM("s", "size", struct pix3filter_args, self,
	pix3filter_square, "pixels", "Set both input dimensions"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pix3filter_operands[] = {
    BU_ARG_OPERAND("frame", BU_ARG_VALUE_FILE, 1, 3,
	"One numbered current frame or three consecutive frames", NULL),
    BU_ARG_OPERAND_NULL
};

static int
pix3filter_validate(const struct bu_arg_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_arg_validate_result *result)
{
    size_t frame_count;

    if (!result || cursor_arg < argc || result->state == BU_ARG_VALIDATE_INVALID)
	return 0;

    frame_count = bu_arg_schema_operand_count(schema, argc, argv);
    if (frame_count == 2) {
	result->state = BU_ARG_VALIDATE_INCOMPLETE;
	result->token_start = argc;
	result->token_end = argc;
	result->expected = BU_ARG_EXPECT_OPERAND;
	result->hint = "supply one numbered frame or three explicit frames";
    }
    return 0;
}

static const struct bu_arg_schema pix3filter_schema = BU_ARG_SCHEMA(
    "pix3filter", "Apply a 3x3x3 filter to three PIX frames",
    pix3filter_options, pix3filter_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, pix3filter_validate, NULL, NULL));

static const struct bu_arg_grammar pix3filter_grammar = {
    &pix3filter_schema, NULL, NULL
};

static int
select_filter(const char *str, int divisor_set, int offset_set)
{
    int i = 0;

    while (kernel[i].name != NULL && !BU_STR_EQUAL(str, kernel[i].uname))
	i++;
    if (kernel[i].name == NULL)
	return 0;

    kern = &kernel[i].kern[0];
    if (!divisor_set)
	kerndiv = kernel[i].kerndiv;
    if (!offset_set)
	kernoffset = kernel[i].kernoffset;
    return 1;
}

static int
open_explicit_frames(const char **operands)
{
    oldfp = fopen(operands[0], "rb");
    if (oldfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", operands[0]);
	return 0;
    }
    curfp = fopen(operands[1], "rb");
    if (curfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", operands[1]);
	fclose(oldfp);
	oldfp = NULL;
	return 0;
    }
    newfp = fopen(operands[2], "rb");
    if (newfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", operands[2]);
	fclose(curfp);
	fclose(oldfp);
	curfp = NULL;
	oldfp = NULL;
	return 0;
    }
    return 1;
}

static int
open_sequence_frames(const char *current)
{
    struct bu_vls path = BU_VLS_INIT_ZERO;
    const char *separator = strrchr(current, '.');
    int frame_number;
    size_t stem_length;

    if (!separator || !separator[1] ||
	!bu_arg_integer_from_str(NULL, &frame_number, separator + 1, 10)) {
	bu_log("pix3filter: no numeric frame suffix on %s\n", current);
	return 0;
    }
    stem_length = (size_t)(separator - current);
    curfp = fopen(current, "rb");
    if (curfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", current);
	return 0;
    }

    bu_vls_printf(&path, "%.*s.%d", (int)stem_length, current, frame_number - 1);
    oldfp = fopen(bu_vls_cstr(&path), "rb");
    if (oldfp == NULL && frame_number == 1)
	oldfp = fopen(current, "rb");
    if (oldfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", bu_vls_cstr(&path));
	fclose(curfp);
	curfp = NULL;
	bu_vls_free(&path);
	return 0;
    }

    bu_vls_sprintf(&path, "%.*s.%d", (int)stem_length, current, frame_number + 1);
    newfp = fopen(bu_vls_cstr(&path), "rb");
    if (newfp == NULL) {
	bu_log("pix3filter: cannot open \"%s\" for reading\n", bu_vls_cstr(&path));
	fclose(oldfp);
	fclose(curfp);
	oldfp = NULL;
	curfp = NULL;
	bu_vls_free(&path);
	return 0;
    }
    bu_vls_free(&path);
    return 1;
}

static int
get_args(int argc, char **argv, struct pix3filter_args *args)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_schema_parse_complete(&pix3filter_schema, args,
	&msg, argc - 1, (const char **)(argv + 1));
    int operand_count;
    int divisor_set;
    int offset_set;
    const char **operands;

    if (operand_index < 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;
    if (isatty(fileno(stdout))) {
	bu_log("pix3filter will not write pixel data to a terminal\n");
	return 0;
    }

    divisor_set = bu_arg_schema_option_present(&pix3filter_schema,
	(size_t)(argc - 1), (const char **)(argv + 1), "divisor");
    offset_set = bu_arg_schema_option_present(&pix3filter_schema,
	(size_t)(argc - 1), (const char **)(argv + 1), "offset");
    kerndiv = args->divisor;
    kernoffset = args->offset;
    if (!select_filter(args->filter, divisor_set, offset_set))
	return 0;

    width = args->width;
    height = args->height;
    verbose = args->verbose;
    operand_count = argc - operand_index - 1;
    operands = (const char **)(argv + operand_index + 1);
    return operand_count == 1 ? open_sequence_frames(operands[0]) :
	open_explicit_frames(operands);
}


int
main(int argc, char **argv)
{
    struct pix3filter_args args = {NULL, 0, 0, "lo", 84, 0,
	DEFAULT_WIDTH, DEFAULT_WIDTH};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    size_t ret;
    int x, y, color;
    int value, r1, r2, r3, r4, r5, r6, r7, r8, r9;
    int max, min;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &pix3filter_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args)) {
	char *help = bu_arg_schema_help(&pix3filter_schema, "pix3filter");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "pix3filter help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_schema_help(&pix3filter_schema, "pix3filter");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix3filter help");
	return 0;
    }

    setmode(fileno(stdout), O_BINARY);

/*
 * Set up the pointers to the working areas.
 */
    bottomold = l11;
    middleold = l12;
    topold    = l13;

    bottomcur = l21;
    middlecur = l22;
    topcur    = l23;

    bottomnew = l31;
    middlenew = l32;
    topnew    = l33;

/*
 * Read in the bottom and middle rows of the old picture.
 */
    ret = fread(bottomold, sizeof(char), 3*width, oldfp);
    if (ret == 0)
	perror("fread");
    ret = fread(middleold, sizeof(char), 3*width, oldfp);
    if (ret == 0)
	perror("fread");
    /*
     * Read in the bottom and middle rows of the current picture.
     */
    ret = fread(bottomcur, sizeof(char), 3*width, curfp);
    if (ret == 0)
	perror("fread");
    ret = fread(middlecur, sizeof(char), 3*width, curfp);
    if (ret == 0)
	perror("fread");
    /*
     * Read in the bottom and middle rows of the new picture.
     */
    ret = fread(bottomnew, sizeof(char), 3*width, newfp);
    if (ret == 0)
	perror("fread");
    ret = fread(middlenew, sizeof(char), 3*width, newfp);
    if (ret == 0)
	perror("fread");
    /*
     * Write out the bottom row.
     */
    ret = fwrite(bottomcur, sizeof(char), 3*width, stdout);
    if (ret == 0)
	perror("fwrite");

    if (verbose) {
	for (x = 0; x < 29; x++)
	    fprintf(stderr, "kern[%d] = %d\n", x, kern[x]);
    }

    max = 0;
    min = 255;

    for (y = 1; y < height-1; y++) {
	/* read in top lines */
	ret = fread(topold, sizeof(char), 3*width, oldfp);
	if (ret == 0)
	    perror("fread");
	ret = fread(topcur, sizeof(char), 3*width, curfp);
	if (ret == 0)
	    perror("fread");
	ret = fread(topnew, sizeof(char), 3*width, newfp);
	if (ret == 0)
	    perror("fread");

	for (color = 0; color < 3; color++) {
	    obuf[0+color] = middlecur[0+color];
	    /* Filter a line */
	    for (x = 3+color; x < 3*(width-1); x += 3) {
		r1 = topold[x-3] * kern[0] +
		    topold[x] * kern[1] +
		    topold[x+3] * kern[2];
		r2 = middleold[x-3] * kern[3] +
		    middleold[x] * kern[4] +
		    middleold[x+3] * kern[5];
		r3 = bottomold[x-3] * kern[6] +
		    bottomold[x] * kern[7] +
		    bottomold[x+3] * kern[8];
		r4 = topcur[x-3] * kern[9] +
		    topcur[x] * kern[10] +
		    topcur[x+3] * kern[11];
		r5 = middlecur[x-3] * kern[12] +
		    middlecur[x] * kern[13] +
		    middlecur[x+3] * kern[14];
		r6 = bottomcur[x-3] * kern[15] +
		    bottomcur[x] * kern[16] +
		    bottomcur[x+3] * kern[17];
		r7 = topnew[x-3] * kern[18] +
		    topnew[x] * kern[19] +
		    topnew[x+3] * kern[20];
		r8 = middlenew[x-3] * kern[21] +
		    middlenew[x] * kern[22] +
		    middlenew[x+3] * kern[23];
		r9 = bottomnew[x-3] * kern[24] +
		    bottomnew[x] * kern[25] +
		    bottomnew[x+3] * kern[26];
		value = (r1+r2+r3+r4+r5+r6+r7+r8+r9) /
		    kerndiv + kernoffset;
		if (value > max) max = value;
		if (value < min) min = value;
		if (verbose && (value > 255 || value < 0)) {
		    fprintf(stderr, "Value %d\n", value);
		    fprintf(stderr, "r1=%d, r2=%d, r3=%d\n", r1, r2, r3);
		}
		if (value < 0)
		    obuf[x] = 0;
		else if (value > 255)
		    obuf[x] = 255;
		else
		    obuf[x] = value;
	    }
	    obuf[3*(width-1)+color] = middlecur[3*(width-1)+color];
	}
	ret = fwrite(obuf, sizeof(char), 3*width, stdout);
	if (ret == 0)
	    perror("fwrite");

	/* Adjust row pointers */
	temp = bottomold;
	bottomold = middleold;
	middleold = topold;
	topold = temp;

	temp = bottomcur;
	bottomcur = middlecur;
	middlecur = topcur;
	topcur = temp;

	temp = bottomnew;
	bottomnew = middlenew;
	middlenew = topnew;
	topnew = temp;

    }
    /* write out last line untouched */
    ret = fwrite(topcur, sizeof(char), 3*width, stdout);
    if (ret == 0)
	perror("fwrite");

    /* Give advise on scaling factors */
    if (verbose)
	fprintf(stderr, "Max = %d,  Min = %d\n", max, min);

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
