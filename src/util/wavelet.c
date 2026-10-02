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
 * -p n Reconstruct a low-pass result from an average image size n
 * -n number of scanlines
 * -w width of dataset
 * -S level/limit of transform ...
 * -W (size of avg img in transformed data output)
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/malloc.h"
#include "bu/exit.h"
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
#define LOW_PASS 2

/* declarations to support use of bu_getopt() system call */
const char *options = "W:S:s:w:n:t:#:D:p:12drR:h?";

const char *progname = "(noname)";
int img_space=1;
int debug;
size_t width = 512;
size_t height = 512;
size_t channels = 3;
int value_type = CHAR;
size_t value_size = sizeof(char);
size_t avg_size = 0;
size_t limit = 0;
static size_t lowpass_size = 0;
int decomp_recon;
static int avg_size_specified;
static int limit_specified;

static int
parse_size_arg(const char *arg, size_t *out_value, const char *label, int allow_zero)
{
    return bu_opt_scan_size_t_range(arg, out_value, allow_zero ? 0 : 1, SIZE_MAX, label);
}


void
usage(const char *s)
{
    if (s) (void)fputs(s, stderr);

    bu_exit(1,
	    "Usage:\n\
	%s {-d | -r | -p average_size} [-2] [-t datatype] [-# channels]\n\
	[-w width] [-n scanlines] [-s number_of_samples] [-W output_size]\n\
	[-S output_size] [-R average_size] [-D level]\n\
	< datastream > wavelets\n", progname);
}


static void
set_operation(int operation)
{
    if (decomp_recon != 0 && decomp_recon != operation)
	usage("Specify only one of -d, -r, or -p.\n");

    decomp_recon = operation;
}


int
parse_args(int ac, char **av)
{
    int c;

    if ((progname=strrchr(*av, '/')))
	progname++;
    else
	progname = *av;


    /* Turn off bu_getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	if (bu_optopt == '?') c='h';
	switch (c) {
	    case '1': img_space=1; break;
	    case '2': img_space=2; break;
	    case 'd': set_operation(DECOMPOSE);
		break;
	    case 'r': set_operation(RECONSTRUCT);
		break;
	    case 'D':
		if (!bu_opt_scan_int(bu_optarg, &debug, "debug level"))
		    usage("");
		break;
	    case 'R':
		if (!parse_size_arg(bu_optarg, &avg_size, "average size", 0))
		    usage("");
		avg_size_specified = 1;
		break;
	    case 'p':
		if (!parse_size_arg(bu_optarg, &lowpass_size, "low-pass average size", 0))
		    usage("");
		set_operation(LOW_PASS);
		break;
	    case '#':
		if (!parse_size_arg(bu_optarg, &channels, "channel count", 0))
		    usage("");
		break;
	    case 't':
		switch (*bu_optarg) {
		    case 'c': value_type = CHAR;
			value_size = sizeof(char);
			break;
		    case 'd': value_type = DOUBLE;
			value_size = sizeof(double);
			break;
		    case 'f': value_type = FLOAT;
			value_size = sizeof(float);
			break;
		    case 'i': value_type = INT;
			value_size = sizeof(int);
			break;
		    case 'l': value_type = LONG;
			value_size = sizeof(long);
			break;
		    case 's': value_type = SHORT;
			value_size = sizeof(short);
			break;
		    default:
			usage("Data type must be c, s, i, l, f, or d.\n");
			break;
		}
		break;
	    case 'n':
		if (!parse_size_arg(bu_optarg, &height, "height", 0))
		    usage("");
		break;
	    case 'w':
		if (!parse_size_arg(bu_optarg, &width, "width", 0))
		    usage("");
		break;
	    case 's':
		if (!parse_size_arg(bu_optarg, &width, "size", 0))
		    usage("");
		height = width;
		break;
	    case 'W':
		if (!parse_size_arg(bu_optarg, &limit, "output limit", 1))
		    usage("");
		limit_specified = 1;
		break;
	    case 'S':
		if (!parse_size_arg(bu_optarg, &limit, "transform limit", 1))
		    usage("");
		limit_specified = 1;
		break;
	    case 'h':
		usage("");
		break;
	    default: fprintf(stderr, "Bad flag specified %c\n", bu_optopt);
		usage("");
		break;
	    }
	}

    return bu_optind;
}


static void
wlt_transform_1d(void *tbuf, void *buf, int operation, size_t average_size, size_t transform_limit)
{
    if (operation != DECOMPOSE && operation != RECONSTRUCT)
	bu_exit(1, "Unsupported one-dimensional wavelet operation.\n");

    switch (value_type) {
	case DOUBLE:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_double_decompose((double *)tbuf, (double *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_double_reconstruct((double *)tbuf, (double *)buf, width, channels, average_size, transform_limit);
	    break;
	case FLOAT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_float_decompose((float *)tbuf, (float *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_float_reconstruct((float *)tbuf, (float *)buf, width, channels, average_size, transform_limit);
	    break;
	case CHAR:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_char_decompose((char *)tbuf, (char *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_char_reconstruct((char *)tbuf, (char *)buf, width, channels, average_size, transform_limit);
	    break;
	case SHORT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_short_decompose((short int *)tbuf, (short int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_short_reconstruct((short int *)tbuf, (short int *)buf, width, channels, average_size, transform_limit);
	    break;
	case INT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_int_decompose((int *)tbuf, (int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_int_reconstruct((int *)tbuf, (int *)buf, width, channels, average_size, transform_limit);
	    break;
	case LONG:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_1d_long_decompose((long int *)tbuf, (long int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_1d_long_reconstruct((long int *)tbuf, (long int *)buf, width, channels, average_size, transform_limit);
	    break;
	default:
	    bu_exit(1, "Unsupported data type.\n");
    }
}


static void
wlt_transform_2d(void *tbuf, void *buf, int operation, size_t average_size, size_t transform_limit)
{
    if (operation != DECOMPOSE && operation != RECONSTRUCT)
	bu_exit(1, "Unsupported two-dimensional wavelet operation.\n");

    switch (value_type) {
	case DOUBLE:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_double_decompose((double *)tbuf, (double *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_double_reconstruct((double *)tbuf, (double *)buf, width, channels, average_size, transform_limit);
	    break;
	case FLOAT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_float_decompose((float *)tbuf, (float *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_float_reconstruct((float *)tbuf, (float *)buf, width, channels, average_size, transform_limit);
	    break;
	case CHAR:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_char_decompose((char *)tbuf, (char *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_char_reconstruct((char *)tbuf, (char *)buf, width, channels, average_size, transform_limit);
	    break;
	case SHORT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_short_decompose((short int *)tbuf, (short int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_short_reconstruct((short int *)tbuf, (short int *)buf, width, channels, average_size, transform_limit);
	    break;
	case INT:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_int_decompose((int *)tbuf, (int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_int_reconstruct((int *)tbuf, (int *)buf, width, channels, average_size, transform_limit);
	    break;
	case LONG:
	    if (operation == DECOMPOSE)
		bn_wlt_haar_2d_long_decompose((long int *)tbuf, (long int *)buf, width, channels, transform_limit);
	    else
		bn_wlt_haar_2d_long_reconstruct((long int *)tbuf, (long int *)buf, width, channels, average_size, transform_limit);
	    break;
	default:
	    bu_exit(1, "Unsupported data type.\n");
    }
}


static void
wlt_validate_square_image(void)
{
    if (width != height) {
	fprintf(stderr, "Two dimensional wavelet operations require a square image\n");
	bu_exit(1, "%zu x %zu image specified\n", width, height);
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

	wlt_transform_1d(tbuf, buf, DECOMPOSE, avg_size, limit);

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


    wlt_validate_square_image();

    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    wlt_transform_2d(tbuf, buf, DECOMPOSE, avg_size, limit);
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

	wlt_transform_1d(tbuf, buf, RECONSTRUCT, avg_size, limit);

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

    wlt_validate_square_image();

    if (fread(buf, scanline_size, height, stdin) != height) {
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);
    }

    wlt_transform_2d(tbuf, buf, RECONSTRUCT, avg_size, limit);
    ret = fwrite(buf, scanline_size, width, stdout);
    if (ret == 0) {
	perror("fwrite");
    }
}


static void
wlt_zero_values(void *buf, size_t first_value, size_t value_count)
{
    unsigned char *bytes = (unsigned char *)buf;

    memset(bytes + first_value * value_size, 0, value_count * value_size);
}


static void
wlt_discard_1d_details(void *buf, size_t retained_size)
{
    size_t retained_values = retained_size * channels;
    size_t total_values = width * channels;

    wlt_zero_values(buf, retained_values, total_values - retained_values);
}


static void
wlt_discard_2d_details(void *buf, size_t retained_size)
{
    size_t scanline_values = width * channels;
    size_t retained_values = retained_size * channels;
    size_t row;

    for (row = 0; row < retained_size; row++) {
	wlt_zero_values(buf, row * scanline_values + retained_values,
			scanline_values - retained_values);
    }

    wlt_zero_values(buf, retained_size * scanline_values,
		    (width - retained_size) * scanline_values);
}


static int
is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}


static void
validate_lowpass(void)
{
    if (avg_size_specified || limit_specified)
	usage("-p cannot be combined with -R, -S, or -W.\n");

    if (!is_power_of_two(width) || !is_power_of_two(lowpass_size))
	usage("-p requires power-of-two width and average size values.\n");

    if (lowpass_size >= width)
	usage("-p average size must be smaller than the input width.\n");
}


static void
wlt_lowpass_1d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t i, n;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    buf = bu_malloc(scanline_size, "wavelet low-pass buffer");
    tbuf = bu_malloc(scanline_size >> 1, "wavelet low-pass temporary buffer");

    if (debug)
	fprintf(stderr, "1D low-pass reconstruction:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu average_size:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)lowpass_size);

    for (i = 0; i < height; i++) {
	n = fread(buf, sample_size, width, stdin);
	if (n != width)
	    bu_exit(1, "read failed line %zu got %zu not %zu\n", i, n, width);

	wlt_transform_1d(tbuf, buf, DECOMPOSE, lowpass_size, lowpass_size);
	wlt_discard_1d_details(buf, lowpass_size);
	wlt_transform_1d(tbuf, buf, RECONSTRUCT, lowpass_size, width);

	ret = fwrite(buf, sample_size, width, stdout);
	if (ret != width)
	    bu_exit(1, "write failed line %zu got %zu not %zu\n", i, ret, width);
    }

    bu_free(tbuf, "wavelet low-pass temporary buffer");
    bu_free(buf, "wavelet low-pass buffer");
}


static void
wlt_lowpass_2d(void)
{
    size_t ret;
    void *buf;
    void *tbuf;
    size_t sample_size;
    size_t scanline_size;

    sample_size = value_size * channels;
    scanline_size = sample_size * width;

    wlt_validate_square_image();

    buf = bu_malloc(scanline_size * height, "wavelet low-pass buffer");
    tbuf = bu_malloc(scanline_size, "wavelet low-pass temporary buffer");

    if (debug)
	fprintf(stderr, "2D low-pass reconstruction:\n\tdatatype_size:%lu channels:%lu width:%lu height:%lu average_size:%lu\n", (long unsigned)value_size, (long unsigned)channels, (long unsigned)width, (long unsigned)height, (long unsigned)lowpass_size);

    if (fread(buf, scanline_size, height, stdin) != height)
	bu_exit(1, "read error getting %zu x %zu bytes\n", scanline_size, height);

    wlt_transform_2d(tbuf, buf, DECOMPOSE, lowpass_size, lowpass_size);
    wlt_discard_2d_details(buf, lowpass_size);
    wlt_transform_2d(tbuf, buf, RECONSTRUCT, lowpass_size, width);

    ret = fwrite(buf, scanline_size, height, stdout);
    if (ret != height)
	bu_exit(1, "write failed got %zu not %zu scanlines\n", ret, height);

    bu_free(tbuf, "wavelet low-pass temporary buffer");
    bu_free(buf, "wavelet low-pass buffer");
}


/*
 * Call parse_args to handle command line arguments first, then
 * process input.
 */
int
main(int ac, char **av)
{
    bu_setprogname(av[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* parse command flags, and make sure there are arguments
     * left over for processing.
     */
    if (parse_args(ac, av) < ac) usage("Excess arguments not supported.\n");

    if (isatty(fileno(stdout))) usage("Redirect input/output\n");

    if (!value_type)
	usage("Must specify data type\n");

    if (decomp_recon == DECOMPOSE) {
	/* set some defaults */
	if (avg_size == 0) avg_size = width;
	if (limit == 0) limit = 1;

	if (img_space == 1) wlt_decompose_1d();
	else wlt_decompose_2d();
	return 0;
    } else if (decomp_recon == RECONSTRUCT) {
	/* set some defaults */
	if (avg_size == 0) avg_size = 1;
	if (limit == 0) limit = width;

	if (img_space == 1) wlt_reconstruct_1d();
	else wlt_reconstruct_2d();
	return 0;
    } else if (decomp_recon == LOW_PASS) {
	validate_lowpass();

	if (img_space == 1) wlt_lowpass_1d();
	else wlt_lowpass_2d();
	return 0;
    }
    usage("must specify decompose (-d), reconstruct (-r), or low-pass reconstruction (-p)\n");
    return -1;
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
#undef LOW_PASS

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
