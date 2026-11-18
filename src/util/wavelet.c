/*                       W A V E L E T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @file util/wavelet.c
 *
 * This program performs a wavelet transformation on data.
 * Transformations possible are decompositions and reconstructions.
 * Currently, only the Haar wavelet is supported.
 *
 * Options
 * -D decompose
 * -R reconstruct
 * -1 one-dimensional transform
 * -2 two-dimensional transform
 * -# n n-elements/channels per sample (e.g. 3 for a pix file)
 * -t[cdfils]	data type
 * -D level debug
 * -s squaresize of original image/dataset (power of 2)
 * -R n Restart with average image size n
 * -n number of scanlines
 * -w width of dataset
 * -S level/limit of transform ...
 * -W (size of avg img in transformed data output)
 *
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn.h"
#include "dm.h"


#define CHAR	1
#define SHORT	2
#define INT	3
#define LONG	4
#define FLOAT	5
#define DOUBLE	6

#define DECOMPOSE 1
#define RECONSTRUCT -1

int img_space=1;
int debug;
size_t width = 512;
size_t height = 512;
size_t channels = 3;
int value_type = CHAR;
size_t value_size = sizeof(char);
size_t avg_size = 0;
size_t limit = 0;
int decomp_recon;

struct wavelet_args {
    struct wavelet_args *self;
    int help;
    int one_dimensional;
    int two_dimensional;
    int decompose;
    int reconstruct;
    const char *data_type;
    int debug;
    int width;
    int height;
    int channels;
    int avg_size;
    int limit;
};

static struct wavelet_args *
wavelet_args_from_storage(void *storage)
{
    return storage ? *(struct wavelet_args **)storage : NULL;
}

static int
is_power_of_two(size_t value)
{
    return value && !(value & (value - 1));
}

static int
wavelet_configuration_valid(void)
{
    if (!is_power_of_two(width)) {
	bu_log("%s: width must be a power of two\n", bu_getprogname());
	return 0;
    }
    if (img_space == 2 && width != height) {
	bu_log("%s: a two-dimensional transform requires equal width and height\n",
	       bu_getprogname());
	return 0;
    }
    if (!is_power_of_two(limit) || limit > width) {
	bu_log("%s: transform limit must be a power of two no greater than the width\n",
	       bu_getprogname());
	return 0;
    }
    if (decomp_recon == RECONSTRUCT &&
	(!is_power_of_two(avg_size) || avg_size >= limit)) {
	bu_log("%s: average size must be a power of two smaller than the transform limit\n",
	       bu_getprogname());
	return 0;
    }
    return 1;
}

static int
wavelet_square_size(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    struct wavelet_args *args = wavelet_args_from_storage(storage);
    int value;

    if (argc != 1 ||
	!bu_arg_integer_range_from_str(msg, &value, argv[0], 10, 1, INT_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static const struct bu_arg_value_keyword wavelet_type_values[] = {
    {"c", BU_ARG_ALIASES("char"), "C char"},
    {"s", BU_ARG_ALIASES("short"), "C short"},
    {"i", BU_ARG_ALIASES("int"), "C int"},
    {"l", BU_ARG_ALIASES("long"), "C long"},
    {"f", BU_ARG_ALIASES("float"), "C float"},
    {"d", BU_ARG_ALIASES("double"), "C double"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option wavelet_options[] = {
    BU_ARG_FLAG("h", "help", struct wavelet_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("1", "one-dimensional", struct wavelet_args,
	one_dimensional,
	"Transform each scanline horizontally and independently (default)"),
    BU_ARG_FLAG("2", "two-dimensional", struct wavelet_args,
	two_dimensional, "Transform a square dataset horizontally and vertically"),
    BU_ARG_FLAG("d", "decompose", struct wavelet_args, decompose,
	"Decompose samples into average and detail coefficients"),
    BU_ARG_FLAG("r", "reconstruct", struct wavelet_args, reconstruct,
	"Reconstruct samples from average and detail coefficients"),
    BU_ARG_KEYWORD_VALUES("t", "data-type", struct wavelet_args, data_type,
	"type", "Set the sample and arithmetic C data type (default: c/char)",
	wavelet_type_values),
    BU_ARG_INTEGER_RANGE("#", "channels", struct wavelet_args, channels,
	1, 1, 1, INT_MAX, "count",
	"Set interleaved values per sample (default: 3, as in RGB)"),
    BU_ARG_INTEGER_RANGE("w", "width", struct wavelet_args, width,
	1, 1, 1, INT_MAX, "samples",
	"Set samples per scanline (default: 512)"),
    BU_ARG_INTEGER_RANGE("n", "height", struct wavelet_args, height,
	1, 1, 1, INT_MAX, "scanlines",
	"Set scanline count (default: 512)"),
    BU_ARG_OPTION_SHAPED("s", "size", "size", struct wavelet_args, self,
	BU_ARG_VALUE_INTEGER, "samples", "Set equal square width and height",
	BU_ARG_REQUIRED, NULL, wavelet_square_size),
    BU_ARG_INTEGER_RANGE("W", "output-size", struct wavelet_args, limit,
	1, 1, 1, INT_MAX, "samples",
	"Stop decomposition at this power-of-two average width"),
    BU_ARG_INTEGER_RANGE("S", "square-output-size", struct wavelet_args, limit,
	1, 1, 1, INT_MAX, "samples",
	"Stop a two-dimensional transform at this power-of-two average size"),
    BU_ARG_INTEGER_RANGE("R", "average-size", struct wavelet_args, avg_size,
	1, 1, 1, INT_MAX, "samples",
	"Resume reconstruction from this power-of-two average image size"),
    BU_ARG_VALUE("D", "debug", struct wavelet_args, debug,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "level",
	"Print data type, dimensions, channels, and transform limit diagnostics"),
    BU_ARG_OPTION_NULL
};

static const char * const wavelet_operations[] = {
    "decompose", "reconstruct", NULL
};

static const char * const wavelet_dimensions[] = {
    "one-dimensional", "two-dimensional", NULL
};

static const char * const wavelet_average_requirement[] = {
    "average-size", "reconstruct", NULL
};

static const struct bu_arg_constraint wavelet_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	wavelet_operations, 1, 1, "select exactly one transform operation"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	wavelet_dimensions, 0, 1, "select at most one transform dimension"),
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_REQUIRES,
	BU_ARG_CONDITION_ALWAYS, wavelet_average_requirement, 0, 0,
	"--average-size requires --reconstruct"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema wavelet_schema = BU_ARG_SCHEMA(
    "wavelet", "Perform a Haar wavelet decomposition or reconstruction",
    wavelet_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, wavelet_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar wavelet_grammar = {
    &wavelet_schema, NULL, NULL
};

static int
wavelet_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&wavelet_schema, "wavelet");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "wavelet help");
    return status;
}

static void
wavelet_set_data_type(const char *type)
{
    if (BU_STR_EQUAL(type, "s")) {
	value_type = SHORT;
	value_size = sizeof(short);
    } else if (BU_STR_EQUAL(type, "i")) {
	value_type = INT;
	value_size = sizeof(int);
    } else if (BU_STR_EQUAL(type, "l")) {
	value_type = LONG;
	value_size = sizeof(long);
    } else if (BU_STR_EQUAL(type, "f")) {
	value_type = FLOAT;
	value_size = sizeof(float);
    } else if (BU_STR_EQUAL(type, "d")) {
	value_type = DOUBLE;
	value_size = sizeof(double);
    } else {
	value_type = CHAR;
	value_size = sizeof(char);
    }
}


void
wlt_decompose_1d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t i, n;
    size_t sample_size;	/* size of data type x #values/sample */
    size_t scanline_size;	/* # bytes in a scanline */

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size, "wavelet buf");
    tbuf = bu_malloc(scanline_size >> 1, "wavelet buf");

    if (debug)
	fprintf(stderr, "1D decompose:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu limit:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    for (i=0; i < height; i++) {

	n = fread(buf, sample_size, width, stdin);
	if (n != width) {
	    bu_exit(1, "read failed line %zu got %zu not %zu\n", i, n, width);
	}

	switch (value_type) {
	    case DOUBLE:
		bn_wlt_haar_1d_double_decompose((double *)tbuf, (double *)buf, width, channels, limit);
		break;
	    case FLOAT:
		bn_wlt_haar_1d_float_decompose((float *)tbuf, (float *)buf, width, channels, limit);
		break;
	    case CHAR:
		bn_wlt_haar_1d_char_decompose((char *)tbuf, (char *)buf, width, channels, limit);
		break;
	    case SHORT:
		bn_wlt_haar_1d_short_decompose((short int *)tbuf, (short int *)buf, width, channels, limit);
		break;
	    case INT:
		bn_wlt_haar_1d_int_decompose((int *)tbuf, (int *)buf, width, channels, limit);
		break;
	    case LONG:
		bn_wlt_haar_1d_long_decompose((long int *)tbuf, (long int *)buf, width, channels, limit);
		break;
	}

	ret = fwrite(buf, sample_size, width, stdout);
	if (ret == 0) {
	    perror("fwrite");
	    break;
	}
    }
}


void
wlt_decompose_2d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size * height, "wavelet buf");
    tbuf = bu_malloc(scanline_size, "wavelet buf");

    if (debug)
	fprintf(stderr, "2D decompose:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu limit:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    switch (value_type) {
	case DOUBLE:
	    bn_wlt_haar_2d_double_decompose((double *)tbuf, (double *)buf, width, channels, limit);
	    break;
	case FLOAT:
	    bn_wlt_haar_2d_float_decompose((float *)tbuf, (float *)buf, width, channels, limit);
	    break;
	case CHAR:
	    bn_wlt_haar_2d_char_decompose((char *)tbuf, (char *)buf, width, channels, limit);
	    break;
	case SHORT:
	    bn_wlt_haar_2d_short_decompose((short int *)tbuf, (short int *)buf, width, channels, limit);
	    break;
	case INT:
	    bn_wlt_haar_2d_int_decompose((int *)tbuf, (int *)buf, width, channels, limit);
	    break;
	case LONG:
	    bn_wlt_haar_2d_long_decompose((long int *)tbuf, (long int *)buf, width, channels, limit);
	    break;
    }
    ret = fwrite(buf, scanline_size, width, stdout);
    if (ret == 0) {
	perror("fwrite");
    }
}


void
wlt_reconstruct_1d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t i, n;
    size_t sample_size;	/* size of data type x #values/sample */
    size_t scanline_size;	/* # bytes in a scanline */

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size, "wavelet buf");
    tbuf = bu_malloc(scanline_size >> 1, "wavelet buf");

    if (debug)
	fprintf(stderr, "1D reconstruct:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu limit:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);


    for (i=0; i < height; i++) {


	n = fread(buf, sample_size, width, stdin);
	if (n != width) {
	    bu_exit(-1, "read failed line %zu got %zu not %zu\n", i, n, width);
	}

	switch (value_type) {
	    case DOUBLE:
		bn_wlt_haar_1d_double_reconstruct((double *)tbuf, (double *)buf, width, channels, avg_size, limit);
		break;
	    case FLOAT:
		bn_wlt_haar_1d_float_reconstruct((float *)tbuf, (float *)buf, width, channels, avg_size, limit);
		break;
	    case CHAR:
		bn_wlt_haar_1d_char_reconstruct((char *)tbuf, (char *)buf, width, channels, avg_size, limit);
		break;
	    case SHORT:
		bn_wlt_haar_1d_short_reconstruct((short int *)tbuf, (short int *)buf, width, channels, avg_size, limit);
		break;
	    case INT:
		bn_wlt_haar_1d_int_reconstruct((int *)tbuf, (int *)buf, width, channels, avg_size, limit);
		break;
	    case LONG:
		bn_wlt_haar_1d_long_reconstruct((long int *)tbuf, (long int *)buf, width, channels, avg_size, limit);
		break;
	}

	ret = fwrite(buf, sample_size, width, stdout);
	if (ret == 0) {
	    perror("fwrite");
	    break;
	}
    }
}


void
wlt_reconstruct_2d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size * height, "wavelet buf");
    tbuf = bu_malloc(scanline_size, "wavelet buf");

    if (debug)
	fprintf(stderr, "2D reconstruct:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu limit:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)limit);

    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    switch (value_type) {
	case DOUBLE:
	    bn_wlt_haar_2d_double_reconstruct((double *)tbuf, (double *)buf, width, channels, avg_size, limit);
	    break;
	case FLOAT:
	    bn_wlt_haar_2d_float_reconstruct((float *)tbuf, (float *)buf, width, channels, avg_size, limit);
	    break;
	case CHAR:
	    bn_wlt_haar_2d_char_reconstruct((char *)tbuf, (char *)buf, width, channels, avg_size, limit);
	    break;
	case SHORT:
	    bn_wlt_haar_2d_short_reconstruct((short int *)tbuf, (short int *)buf, width, channels, avg_size, limit);
	    break;
	case INT:
	    bn_wlt_haar_2d_int_reconstruct((int *)tbuf, (int *)buf, width, channels, avg_size, limit);
	    break;
	case LONG:
	    bn_wlt_haar_2d_long_reconstruct((long int *)tbuf, (long int *)buf, width, channels, avg_size, limit);
	    break;
    }
    ret = fwrite(buf, scanline_size, width, stdout);
    if (ret == 0) {
	perror("fwrite");
    }
}


int
main(int argc, char **argv)
{
    struct wavelet_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;

    bu_setprogname(argv[0]);

    args.self = &args;
    args.data_type = "c";
    args.width = 512;
    args.height = 512;
    args.channels = 3;

    introspection = bu_arg_introspect(&msg, &wavelet_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    if (bu_arg_schema_parse_complete(&wavelet_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return wavelet_help(stdout, 0);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (isatty(fileno(stdout))) {
	bu_flog(stderr, "wavelet: standard output must be redirected\n");
	return wavelet_help(stderr, 1);
    }

    img_space = args.two_dimensional ? 2 : 1;
    decomp_recon = args.decompose ? DECOMPOSE : RECONSTRUCT;
    debug = args.debug;
    width = args.width;
    height = args.height;
    channels = args.channels;
    avg_size = args.avg_size;
    limit = args.limit;
    wavelet_set_data_type(args.data_type);

    if (decomp_recon == DECOMPOSE) {
	/* set some defaults */
	if (avg_size == 0) avg_size = width;
	if (limit == 0) limit = 1;
	if (!wavelet_configuration_valid())
	    return 1;

	if (img_space == 1) wlt_decompose_1d();
	else wlt_decompose_2d();
	return 0;
    } else if (decomp_recon == RECONSTRUCT) {
	/* set some defaults */
	if (avg_size == 0) avg_size = 1;
	if (limit == 0) limit = width;
	if (!wavelet_configuration_valid())
	    return 1;

	if (img_space == 1) wlt_reconstruct_1d();
	else wlt_reconstruct_2d();
	return 0;
    }
    return 1;
}


/* Undefine all local macros in case this file is ever incorporated into
 * Unity/jumbo builds.  CHAR, SHORT, INT, LONG, FLOAT, and DOUBLE conflict with
 * Windows SDK type names in winnt.h and must be cleaned up. */
#undef CHAR
#undef SHORT
#undef INT
#undef LONG
#undef FLOAT
#undef DOUBLE
#undef DECOMPOSE
#undef RECONSTRUCT

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
