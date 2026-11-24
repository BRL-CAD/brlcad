/*                     P I X F I L T E R . C
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
/** @file util/pixfilter.c
 *
 * Filters a color pix file with an arbitrary 3x3 kernel.
 * Leaves the outer rows untouched.  Allows an alternate divisor and
 * offset to be given.
 *
 */

#include "common.h"

#include <limits.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define MAXLINE (8*1024)
#define DEFAULT_WIDTH 512
#define KERNEL_ELEMENT_COUNT 9
static unsigned char line1[3*MAXLINE], line2[3*MAXLINE], line3[3*MAXLINE], obuf[3*MAXLINE];

/* The filter kernels */
struct pixfilter_kernel {
    const char *name;
    const char *uname;		/* What is needed to recognize it */
    int kern[9];
    int kerndiv;	/* Divisor for kernel */
    int kernoffset;	/* To be added to result */
};

static const struct pixfilter_kernel pixfilter_kernels[] = {
    { "Low Pass", "lo", {3, 5, 3, 5, 10, 5, 3, 5, 3}, 42, 0 },
    { "Laplacian", "la", {-1, -1, -1, -1, 8, -1, -1, -1, -1}, 16, 128 },
    { "High Pass", "hi", {-1, -2, -1, -2, 13, -2, -1, -2, -1}, 1, 0 },
    { "Horizontal Gradient", "hg", {1, 0, -1, 1, 0, -1, 1, 0, -1}, 6, 128},
    { "Vertical Gradient", "vg", {1, 1, 1, 0, 0, 0, -1, -1, -1}, 6, 128 },
    { "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1}, 9, 0 },
    { NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0 },
};


struct pixfilter_args {
    struct pixfilter_args *self;
    int help;
    int verbose;
    const char *filter;
    int divisor;
    int divisor_set;
    int offset;
    int offset_set;
    int width;
    int height;
};

enum pixfilter_dimension {
    PIXFILTER_SIZE,
    PIXFILTER_WIDTH,
    PIXFILTER_HEIGHT
};

static struct pixfilter_args *
pixfilter_args_from_storage(void *storage)
{
    return storage ? *(struct pixfilter_args **)storage : NULL;
}

static int
pixfilter_divisor(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixfilter_args *args = pixfilter_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (value == 0) {
	if (msg)
	    bu_vls_printf(msg, "divisor must not be zero\n");
	return -1;
    }
    if (args) {
	args->divisor = value;
	args->divisor_set = 1;
    }
    return 0;
}

static int
pixfilter_offset(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixfilter_args *args = pixfilter_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (args) {
	args->offset = value;
	args->offset_set = 1;
    }
    return 0;
}

static int
pixfilter_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pixfilter_dimension dimension)
{
    struct pixfilter_args *args = pixfilter_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == PIXFILTER_SIZE)
	    args->width = args->height = value;
	else if (dimension == PIXFILTER_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define PIXFILTER_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pixfilter_dimension(msg, arg, storage, _dimension); \
    }

PIXFILTER_DIMENSION_CALLBACK(pixfilter_size, PIXFILTER_SIZE)
PIXFILTER_DIMENSION_CALLBACK(pixfilter_width, PIXFILTER_WIDTH)
PIXFILTER_DIMENSION_CALLBACK(pixfilter_height, PIXFILTER_HEIGHT)

static const struct bu_arg_value_keyword pixfilter_filter_values[] = {
    {"lo", NULL, "Low-pass filter"},
    {"la", NULL, "Laplacian filter"},
    {"hi", NULL, "High-pass filter"},
    {"hg", NULL, "Horizontal gradient"},
    {"vg", NULL, "Vertical gradient"},
    {"b", NULL, "Boxcar average"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option pixfilter_options[] = {
    BU_ARG_FLAG("h", "help", struct pixfilter_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("v", "verbose", struct pixfilter_args, verbose,
	BU_ARG_VALUE_FLAG, "Print kernel and result-range diagnostics"),
    BU_ARG_KEYWORD_VALUES("f", "filter", struct pixfilter_args, filter,
	"type", "Select the filter kernel", pixfilter_filter_values),
    BU_ARG_CUSTOM("d", "divisor", struct pixfilter_args, self,
	pixfilter_divisor, "integer", "Override the kernel divisor"),
    BU_ARG_CUSTOM("o", "offset", struct pixfilter_args, self,
	pixfilter_offset, "integer", "Override the kernel offset"),
    BU_ARG_CUSTOM("s", "size", struct pixfilter_args, self, pixfilter_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pixfilter_args, self,
	pixfilter_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pixfilter_args, self,
	pixfilter_height, "pixels", "Set the input image height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixfilter_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixfilter_schema = BU_ARG_SCHEMA(
    "pixfilter", "Apply a 3x3 kernel to a PIX image", pixfilter_options,
    pixfilter_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixfilter_grammar = {
    &pixfilter_schema, NULL, NULL
};

static const struct pixfilter_kernel *
pixfilter_kernel(const char *name)
{
    size_t i;

    for (i = 0; pixfilter_kernels[i].name; i++) {
	if (BU_STR_EQUAL(name, pixfilter_kernels[i].uname))
	    return &pixfilter_kernels[i];
    }
    return NULL;
}


int
main(int argc, char **argv)
{
    struct pixfilter_args args = {NULL, 0, 0, "lo", 0, 0, 0, 0,
	DEFAULT_WIDTH, DEFAULT_WIDTH};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const struct pixfilter_kernel *filter = NULL;
    const char *input = NULL;
    const int *kern = NULL;
    FILE *infp = stdin;
    unsigned char *top = NULL;
    unsigned char *middle = NULL;
    unsigned char *bottom = NULL;
    unsigned char *temp = NULL;
    int kerndiv;
    int kernoffset;
    int width;
    int height;
    int verbose;
    int x, y, color;
    int value, r1, r2, r3;
    int max, min;
    size_t ret;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixfilter_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixfilter_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixfilter_schema, "pixfilter");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixfilter help");
	return 0;
    }

    if (operand_index < argc - 1)
	input = argv[operand_index + 1];
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "pixfilter: no input file and standard input is a terminal\n");
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixfilter cannot send binary output to a terminal\n");
    if (input && (infp = fopen(input, "rb")) == NULL)
	bu_exit(1, "pixfilter: cannot open \"%s\" for reading\n", input);

    filter = pixfilter_kernel(args.filter);
    if (!filter)
	bu_exit(1, "pixfilter: internal filter selection failure\n");
    kern = filter->kern;
    kerndiv = args.divisor_set ? args.divisor : filter->kerndiv;
    kernoffset = args.offset_set ? args.offset : filter->kernoffset;
    width = args.width;
    height = args.height;
    verbose = args.verbose;

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (width > MAXLINE) {
	bu_exit(1, "pixfilter: limited to scanlines of %d pixels\n", MAXLINE);
    }

    /*
     * Read in bottom and middle lines.
     * Write out bottom untouched.
     */
    bottom = &line1[0];
    middle = &line2[0];
    top    = &line3[0];

    ret = fread(bottom, sizeof(char), 3*width, infp);
    if (ret < (size_t)3*width)
	perror("fread");

    if (verbose) {
	for (x = 0; x < KERNEL_ELEMENT_COUNT; x++)
	    bu_flog(stderr, "kern[%d] = %d\n", x, kern[x]);
    }

    if (height == 1) {
	ret = fwrite(bottom, sizeof(char), 3*width, stdout);
	if (ret < (size_t)3*width)
	    perror("fwrite");
	return 0;
    }

    ret = fread(middle, sizeof(char), 3*width, infp);
    if (ret < (size_t)3*width)
	perror("fread");

    ret = fwrite(bottom, sizeof(char), 3*width, stdout);
    if (ret < (size_t)3*width)
	perror("fwrite");

    max = 0;
    min = 255;

    for (y = 1; y < height-1; y++) {
	/* read in top line */
	ret = fread(top, sizeof(char), 3*width, infp);
	if (ret < (size_t)3*width)
	    perror("fread");

	for (color = 0; color < 3; color++) {
	    obuf[0+color] = middle[0+color];
	    /* Filter a line */
	    for (x = 3+color; x < 3*(width-1); x += 3) {
		r1 = top[x-3] * kern[0] + top[x] * kern[1] + top[x+3] * kern[2];
		r2 = middle[x-3] * kern[3] + middle[x] * kern[4] + middle[x+3] * kern[5];
		r3 = bottom[x-3] * kern[6] + bottom[x] * kern[7] + bottom[x+3] * kern[8];
		value = (r1+r2+r3) / kerndiv + kernoffset;
		if (value > max) max = value;
		if (value < min) min = value;
		if (verbose && (value > 255 || value < 0)) {
		    bu_flog(stderr, "Value %d\n", value);
		    bu_flog(stderr, "r1=%d, r2=%d, r3=%d\n", r1, r2, r3);
		}
		if (value < 0)
		    obuf[x] = 0;
		else if (value > 255)
		    obuf[x] = 255;
		else
		    obuf[x] = value;
	    }
	    obuf[3*(width-1)+color] = middle[3*(width-1)+color];
	}
	ret = fwrite(obuf, sizeof(char), 3*width, stdout);
	if (ret < (size_t)3*width)
	    perror("fwrite");
	/* Adjust row pointers */
	temp = bottom;
	bottom = middle;
	middle = top;
	top = temp;
    }
    /* write out last line untouched */
    ret = fwrite(middle, sizeof(char), 3*width, stdout);
    if (ret < (size_t)3*width)
	perror("fwrite");

    /* Give advise on scaling factors */
    if (verbose)
	bu_flog(stderr, "Max = %d,  Min = %d\n", max, min);

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
