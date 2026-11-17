/*                       B W S C A L E . C
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
/** @file util/bwscale.c
 *
 * Scale a black and white picture.
 *
 * To scale up, we use bilinear interpolation.
 * To scale down, we assume "square pixels" and preserve the
 * amount of light energy per unit area.
 *
 * This is a buffered version that can handle files of
 * almost arbitrary size.
 * Note: this buffer code also appears in bwcrop.c
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/file.h"
#include "bu/str.h"


#define MAXBUFBYTES BU_PAGE_SIZE*BU_PAGE_SIZE	/* max bytes to malloc in buffer space */

unsigned char *outbuf;
unsigned char *buffer;
ssize_t scanlen;		/* length of infile (and buffer) scanlines */
ssize_t buflines;		/* Number of lines held in buffer */
b_off_t buf_start = -1000;	/* First line in buffer */

ssize_t bufy;				/* y coordinate in buffer */
FILE *buffp;
static const char *file_name;

int rflag = 0;
int inx = 512;
int iny = 512;
int outx = 512;
int outy = 512;


struct bwscale_args {
    struct bwscale_args *self;
    int help;
    int replicate;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
};

static struct bwscale_args *
bwscale_args_from_storage(void *storage)
{
    return storage ? *(struct bwscale_args **)storage : NULL;
}

static int
bwscale_square(struct bu_vls *msg, const char *arg, void *storage, int output)
{
    struct bwscale_args *args = bwscale_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (output)
	    args->output_width = args->output_height = value;
	else
	    args->input_width = args->input_height = value;
    }
    return 0;
}

static int
bwscale_input_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return bwscale_square(msg, arg, storage, 0);
}

static int
bwscale_output_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return bwscale_square(msg, arg, storage, 1);
}

static const struct bu_arg_option bwscale_options[] = {
    BU_ARG_FLAG("h", "help", struct bwscale_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("r", "replicate", struct bwscale_args, replicate,
	"Use nearest-neighbor replication when enlarging"),
    BU_ARG_CUSTOM("s", "input-size", struct bwscale_args, self,
	bwscale_input_square, "pixels", "Set both input dimensions"),
    BU_ARG_INTEGER_RANGE("w", "input-width", struct bwscale_args, input_width,
	1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_INTEGER_RANGE("n", "input-height", struct bwscale_args, input_height,
	1, 1, 1, INT_MAX, "scanlines", "Set the input height"),
    BU_ARG_CUSTOM("S", "output-size", struct bwscale_args, self,
	bwscale_output_square, "pixels", "Set both output dimensions"),
    BU_ARG_INTEGER_RANGE("W", "output-width", struct bwscale_args, output_width,
	1, 1, 1, INT_MAX, "pixels", "Set the output width"),
    BU_ARG_INTEGER_RANGE("N", "output-height", struct bwscale_args, output_height,
	1, 1, 1, INT_MAX, "scanlines", "Set the output height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwscale_file_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand bwscale_legacy_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1, "BW input file", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("input-width", 1, 1,
	1, 1, 1, INT_MAX, "Input width", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("input-height", 1, 1,
	1, 1, 1, INT_MAX, "Input height", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("output-width", 1, 1,
	1, 1, 1, INT_MAX, "Output width", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("output-height", 1, 1,
	1, 1, 1, INT_MAX, "Output height", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwscale_file_schema = BU_ARG_SCHEMA(
    "bwscale", "Scale a BW image", bwscale_options, bwscale_file_operands,
    BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_schema bwscale_legacy_schema = BU_ARG_SCHEMA(
    "bwscale", "Scale a BW image using positional dimensions",
    bwscale_options, bwscale_legacy_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_form bwscale_form_rows[] = {
    BU_ARG_FORM("options", "Specify dimensions with options",
	&bwscale_file_schema, NULL),
    BU_ARG_FORM("legacy", "Specify all dimensions positionally",
	&bwscale_legacy_schema, NULL),
    BU_ARG_FORM_NULL
};

static const struct bu_arg_form *
bwscale_form_select(const struct bu_arg_forms *forms, size_t argc,
	const char * const *argv, void *UNUSED(context))
{
    int value;
    size_t i;

    if (!forms || !forms->forms || !argv)
	return NULL;
    if (argc < 6)
	return &forms->forms[0];
    for (i = argc - 4; i < argc; i++) {
	if (!bu_arg_integer_range_from_str(NULL, &value, argv[i], 10, 1, INT_MAX))
	    return &forms->forms[0];
    }
    return &forms->forms[1];
}

static const struct bu_arg_forms bwscale_forms = BU_ARG_FORMS(
    "bwscale", "Scale a BW image", bwscale_form_rows, bwscale_form_select);

static const struct bu_arg_grammar bwscale_grammar = {
    NULL, NULL, &bwscale_forms
};

/****** THIS PROBABLY SHOULD BE ELSEWHERE *******/

/* ceiling and floor functions for positive numbers */
#define CEILING(x)	(((x) > (int)(x)) ? (int)(x)+1 : (int)(x))
#define FLOOR(x)	((int)(x))
#define MIN(x, y)	(((x) > (y)) ? (y) : (x))


static double
interpolation_step(int input_size, int output_size)
{
    if (output_size == 1)
	return 0.0;

    return (double)(input_size - 1) / (double)(output_size - 1);
}


/*
 * Load the buffer with scan lines centered around
 * the given y coordinate.
 */
static void
fill_buffer(int y)
{
    size_t ret;

    buf_start = y - buflines/2;
    if (buf_start < 0) buf_start = 0;

    if (bu_fseek(buffp, buf_start * scanlen, 0) < 0) {
	fprintf(stderr, "bwscale: Can't seek to input pixel!\n");
	/* bu_exit (3, NULL); */
    }
    ret = fread(buffer, scanlen, buflines, buffp);
    if (ret != (size_t)buflines)
	perror("fread");
}


/*
 * Nearest Neighbor Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
ninterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, input_x, input_y, j;
    double x, y;
    double xstep, ystep;
    unsigned char *op, *lp;
    size_t ret;

    xstep = interpolation_step(ix, ox);
    ystep = interpolation_step(iy, oy);

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	y = (j == oy - 1) ? (double)(iy - 1) : j * ystep;
	input_y = (int)(y + 0.5);
	/*
	 * Make sure we have this row in the buffer.
	 */
	bufy = input_y - buf_start;
	if (bufy < 0 || bufy >= buflines) {
	    fill_buffer(input_y);
	    bufy = input_y - buf_start;
	}

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = (i == ox - 1) ? (double)(ix - 1) : i * xstep;
	    input_x = (int)(x + 0.5);
	    lp = &buffer[bufy*scanlen+input_x];
	    *op++ = lp[0];
	}

	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
}


/*
 * Bilinear Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
binterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j;
    int next_bufy, xoffset;
    double x, y, dx, dy, mid1, mid2;
    double xstep, ystep;
    unsigned char *op, *up, *lp;

    xstep = interpolation_step(ix, ox);
    ystep = interpolation_step(iy, oy);

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	size_t ret;
	y = (j == oy - 1) ? (double)(iy - 1) : j * ystep;
	/*
	 * Make sure we have this row (and the one after it)
	 * in the buffer
	 */
	bufy = (int)y - buf_start;
	if (bufy < 0 || bufy >= buflines || ((int)y < iy - 1 && bufy >= buflines - 1)) {
	    fill_buffer((int)y);
	    bufy = (int)y - buf_start;
	}
	next_bufy = ((int)y < iy - 1) ? bufy + 1 : bufy;

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = (i == ox - 1) ? (double)(ix - 1) : i * xstep;
	    dx = x - (int)x;
	    dy = y - (int)y;
	    xoffset = ((int)x < ix - 1) ? 1 : 0;

	    /* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

	    lp = &buffer[bufy*scanlen+(int)x];
	    up = &buffer[next_bufy*scanlen+(int)x];

	    mid1 = lp[0] + dx * ((double)lp[xoffset] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[xoffset] - (double)up[0]);

	    *op++ = mid1 + dy * (mid2 - mid1);
	}

	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
}


/*
 * Scale a file of pixels to a different size.
 *
 * To scale down we make a square pixel assumption.
 * We will preserve the amount of light energy per unit area.
 * To scale up we use bilinear interpolation.
 */
int
scale(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j, k, l;
    double pxlen, pylen;			/* # old pixels per new pixel */
    double xstart, xend, ystart, yend;	/* edges of new pixel in old coordinates */
    double xdist, ydist;			/* length of new pixel sides in old coord */
    double sum;
    unsigned char *op;

    pxlen = (double)ix / (double)ox;
    pylen = (double)iy / (double)oy;
    if ((pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0)) {
	fprintf(stderr, "bwscale: can't stretch one way and compress another!\n");
	return -1;
    }
    if (pxlen < 1.0 || pylen < 1.0) {
	/* scale up */
	if (rflag) {
	    /* nearest neighbor interpolate */
	    ninterp(ofp, ix, iy, ox, oy);
	} else {
	    /* bilinear interpolate */
	    binterp(ofp, ix, iy, ox, oy);
	}
	return 0;
    }

    /* for each output pixel */
    for (j = 0; j < oy; j++) {
	size_t ret;

	ystart = j * pylen;
	yend = ystart + pylen;
	op = outbuf;
	for (i = 0; i < ox; i++) {
	    xstart = i * pxlen;
	    xend = xstart + pxlen;
	    sum = 0.0;
	    /*
	     * For each pixel of the original falling
	     * inside this new pixel.
	     */
	    for (l = FLOOR(ystart); l < CEILING(yend); l++) {

		/* Make sure we have this row in the buffer */
		bufy = l - buf_start;
		if (bufy < 0 || bufy >= buflines) {
		    fill_buffer(l);
		    bufy = l - buf_start;
		}

		/* Compute height of this row */
		if ((double)l < ystart)
		    ydist = CEILING(ystart) - ystart;
		else
		    ydist = MIN(1.0, yend - (double)l);

		for (k = FLOOR(xstart); k < CEILING(xend); k++) {
		    /* Compute width of column */
		    if ((double)k < xstart)
			xdist = CEILING(xstart) - xstart;
		    else
			xdist = MIN(1.0, xend - (double)k);

		    /* Add this pixels contribution */
		    /* sum += old[l][k] * xdist * ydist; */
		    sum += buffer[bufy * scanlen + k] * xdist * ydist;
		}
	    }
	    *op++ = (int)(sum / (pxlen * pylen));
	    if (op > (outbuf+scanlen))
		bu_bomb("unexpected buffer overrun");
	}
	ret = fwrite(outbuf, 1, ox, ofp);
	if (ret != (size_t)ox)
	    perror("fwrite");
    }
    return 1;
}


/*
 * Determine max number of lines to buffer.
 * and malloc space for it.
 * XXX - CHECK FILE SIZE
 */
void
init_buffer(void)
{
    ssize_t max;

    /* See how many we could buffer */
    max = MAXBUFBYTES / scanlen;

    /*
     * TODO: We really should see how big the input file is to decide
     * if we should buffer less than our max.
     */
    if (max > BU_PAGE_SIZE)
	max = BU_PAGE_SIZE;

    if (max < iny)
	buflines = max;
    else
	buflines = iny;

    buf_start = (-buflines);
    buffer = (unsigned char *)bu_calloc(buflines, scanlen, "buffer");
}

static int
get_args(int argc, char **argv, struct bwscale_args *args)
{
    const struct bu_arg_form *selected = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_forms_parse(&bwscale_forms, args, &msg,
	argc, (const char **)argv, NULL, &selected);

    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;

    if (selected == &bwscale_form_rows[1]) {
	file_name = argv[operand_index + 1];
	if (!bu_arg_integer_range_from_str(NULL, &args->input_width,
		argv[operand_index + 2], 10, 1, INT_MAX) ||
	    !bu_arg_integer_range_from_str(NULL, &args->input_height,
		argv[operand_index + 3], 10, 1, INT_MAX) ||
	    !bu_arg_integer_range_from_str(NULL, &args->output_width,
		argv[operand_index + 4], 10, 1, INT_MAX) ||
	    !bu_arg_integer_range_from_str(NULL, &args->output_height,
		argv[operand_index + 5], 10, 1, INT_MAX))
	    return 0;
    } else if (argc - operand_index - 1 == 1) {
	file_name = argv[operand_index + 1];
    } else {
	file_name = "-";
    }

    rflag = args->replicate;
    inx = args->input_width;
    iny = args->input_height;
    outx = args->output_width;
    outy = args->output_height;

    if (BU_STR_EQUAL(file_name, "-")) {
	if (isatty(fileno(stdin)))
	    return 0;
	buffp = stdin;
    } else if ((buffp = fopen(file_name, "rb")) == NULL) {
	bu_log("bwscale: cannot open \"%s\" for reading\n", file_name);
	return 0;
    }

    return 1;
}

int
main(int argc, char **argv)
{
    struct bwscale_args args = {NULL, 0, 0, 512, 512, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    introspection = bu_arg_introspect(&msg, &bwscale_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args) || isatty(fileno(stdout))) {
	char *help = bu_arg_forms_help(&bwscale_forms, "bwscale");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "bwscale help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_forms_help(&bwscale_forms, "bwscale");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwscale help");
	return 0;
    }

    if (inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0) {
	bu_exit(2, "bwscale: bad size\n");
    }

    /* See how many lines we can buffer */
    scanlen = inx;
    init_buffer();

    i = (inx < outx) ? outx : inx;
    outbuf = (unsigned char *)bu_malloc(i, "outbuf");

    /* Here we go */
    scale(stdout, inx, iny, outx, outy);

    bu_free(outbuf, (const char *)buffer);
    bu_free(buffer, (const char *)buffer);
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
