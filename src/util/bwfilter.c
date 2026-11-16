/*                      B W F I L T E R . C
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
/** @file util/bwfilter.c
 *
 * Filters a black and white file with an arbitrary 3x3 kernel.
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
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/log.h"
#include "bu/str.h"

#include "icv.h"
#include "vmath.h"

#define DEFAULT_IMAGE_DIMENSION 512
#define KERNEL_ELEMENT_COUNT 9

/* The filter kernels */
struct bwfilter_kernel {
    const char *name;
    const char *keyword;
    int kern[KERNEL_ELEMENT_COUNT];
    int divisor;
    int offset;
    ICV_FILTER filter;
};

static const struct bwfilter_kernel bwfilter_kernels[] = {
    { "Low Pass", "lo", {3, 5, 3, 5, 10, 5, 3, 5, 3}, 42, 0, ICV_FILTER_LOW_PASS },
    { "Laplacian", "la", {-1, -1, -1, -1, 8, -1, -1, -1, -1}, 16, 128, ICV_FILTER_LAPLACIAN },
    { "High Pass", "hi", {-1, -2, -1, -2, 13, -2, -1, -2, -1}, 1, 0, ICV_FILTER_HIGH_PASS },
    { "Horizontal Gradient", "hg", {1, 0, -1, 1, 0, -1, 1, 0, -1}, 6, 128, ICV_FILTER_HORIZONTAL_GRAD },
    { "Vertical Gradient", "vg", {1, 1, 1, 0, 0, 0, -1, -1, -1}, 6, 128, ICV_FILTER_VERTICAL_GRAD },
    { "Boxcar Average", "b", {1, 1, 1, 1, 1, 1, 1, 1, 1}, 9, 0, ICV_FILTER_BOXCAR_AVERAGE },
    { NULL, NULL, {0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0, ICV_FILTER_NULL },
};

struct bwfilter_args {
    struct bwfilter_args *self;
    int help;
    int verbose;
    const char *filter;
    int divisor;
    int divisor_set;
    int offset;
    int offset_set;
    int width;
    int height;
    const char *output;
};

enum bwfilter_dimension {
    BWFILTER_SIZE,
    BWFILTER_WIDTH,
    BWFILTER_HEIGHT
};

static struct bwfilter_args *
bwfilter_args_from_storage(void *storage)
{
    return storage ? *(struct bwfilter_args **)storage : NULL;
}

static int
bwfilter_divisor(struct bu_vls *msg, const char *arg, void *storage)
{
    struct bwfilter_args *args = bwfilter_args_from_storage(storage);
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
bwfilter_offset(struct bu_vls *msg, const char *arg, void *storage)
{
    struct bwfilter_args *args = bwfilter_args_from_storage(storage);
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
bwfilter_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum bwfilter_dimension dimension)
{
    struct bwfilter_args *args = bwfilter_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension == BWFILTER_SIZE)
	    args->width = args->height = value;
	else if (dimension == BWFILTER_WIDTH)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

#define BWFILTER_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return bwfilter_dimension(msg, arg, storage, _dimension); \
    }

BWFILTER_DIMENSION_CALLBACK(bwfilter_size, BWFILTER_SIZE)
BWFILTER_DIMENSION_CALLBACK(bwfilter_width, BWFILTER_WIDTH)
BWFILTER_DIMENSION_CALLBACK(bwfilter_height, BWFILTER_HEIGHT)

static const struct bu_arg_value_keyword bwfilter_filter_values[] = {
    {"lo", NULL, "Low-pass filter"},
    {"la", NULL, "Laplacian filter"},
    {"hi", NULL, "High-pass filter"},
    {"hg", NULL, "Horizontal gradient"},
    {"vg", NULL, "Vertical gradient"},
    {"b", NULL, "Boxcar average"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option bwfilter_options[] = {
    BU_ARG_FLAG("h", "help", struct bwfilter_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct bwfilter_args, verbose,
	"Print kernel and result-range diagnostics"),
    BU_ARG_KEYWORD_VALUES("f", "filter", struct bwfilter_args, filter,
	"type", "Select the filter kernel", bwfilter_filter_values),
    BU_ARG_CUSTOM("d", "divisor", struct bwfilter_args, self,
	bwfilter_divisor, "integer", "Override the kernel divisor"),
    BU_ARG_CUSTOM("O", "offset", struct bwfilter_args, self,
	bwfilter_offset, "integer", "Override the kernel offset"),
    BU_ARG_CUSTOM("s", "size", struct bwfilter_args, self, bwfilter_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct bwfilter_args, self,
	bwfilter_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct bwfilter_args, self,
	bwfilter_height, "pixels", "Set the input image height"),
    BU_ARG_VALUE("o", "output", struct bwfilter_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write output to a BW file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwfilter_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input BW file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwfilter_schema = BU_ARG_SCHEMA(
    "bwfilter", "Apply a 3x3 kernel to a BW image", bwfilter_options,
    bwfilter_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwfilter_grammar = {
    &bwfilter_schema, NULL, NULL
};

static const struct bwfilter_kernel *
bwfilter_kernel(const char *keyword)
{
    size_t i;

    for (i = 0; bwfilter_kernels[i].name; i++) {
	if (BU_STR_EQUAL(keyword, bwfilter_kernels[i].keyword))
	    return &bwfilter_kernels[i];
    }
    return NULL;
}


int
main(int argc, char **argv)
{
    struct bwfilter_args args = {NULL, 0, 0, "lo", 0, 0, 0, 0,
	DEFAULT_IMAGE_DIMENSION, DEFAULT_IMAGE_DIMENSION, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const struct bwfilter_kernel *filter;
    const char *input = NULL;
    icv_image_t *img;
    int status = 0;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bwfilter_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwfilter_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwfilter_schema, "bwfilter");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwfilter help");
	return 0;
    }

    if (operand_index < argc - 1)
	input = argv[operand_index + 1];
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "bwfilter requires an input file or redirected input\n");

    filter = bwfilter_kernel(args.filter);
    if (!filter)
	bu_exit(1, "bwfilter: internal filter selection failure\n");

    img = icv_read(input, BU_MIME_IMAGE_BW, args.width, args.height);
    if (img == NULL)
	return 1;

    icv_filter(img, filter->filter);

    if (args.offset_set || args.divisor_set) {
	icv_add_val(img, -ICV_CONV_8BIT(filter->offset));

	if (args.divisor_set) {
	    icv_multiply_val(img,
		(double)filter->divisor / (double)args.divisor);
	}

	icv_add_val(img,
	    ICV_CONV_8BIT(args.offset_set ? args.offset : filter->offset));
    }

    if (args.verbose) {
	double *max_d;
	double *min_d;
	int x;

	for (x = 0; x < KERNEL_ELEMENT_COUNT; x++)
	    bu_flog(stderr, "kern[%d] = %d\n", x, filter->kern[x]);

	max_d = icv_max(img);
	min_d = icv_min(img);
	bu_flog(stderr, "\n\tMax = %d, Min = %d\n",
	    (int)(*max_d * 255.0), (int)(*min_d * 255.0));
	bu_free(min_d, "bwfilter minimum");
	bu_free(max_d, "bwfilter maximum");
    }

    if (icv_write(img, args.output, BU_MIME_IMAGE_BW) != BRLCAD_OK)
	status = 1;

    /* If stdout is non-interactive as well as -o being specified, emit to both. */
    if (!isatty(fileno(stdout)) && args.output != NULL &&
	icv_write(img, NULL, BU_MIME_IMAGE_BW) != BRLCAD_OK)
	status = 1;

    icv_destroy(img);
    return status;
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
