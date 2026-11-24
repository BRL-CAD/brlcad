/*                      P I X S C A L E . C
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
/** @file util/pixscale.c
 *
 * Scale an RGB pix file.
 *
 * To scale up, we use bilinear interpolation.
 * To scale down, we assume "square pixels" and preserve the
 * amount of light energy per unit area.
 *
 * This is a buffered version that can handle files of
 * almost arbitrary size.
 *
 * Note: This is a simple extension to bwcrop.  Improvements made
 * there should be incorporated here.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/file.h"
#include "bu/str.h"


#define MAXBUFBYTES 3*1024*1024	/* max bytes to malloc in buffer space */
#define INTERPOLATION_SCANLINES 2

unsigned char *outbuf;
unsigned char *buffer;
ssize_t scanlen;		/* length of infile (and buffer) scanlines */
ssize_t buflines;		/* Number of lines held in buffer */

static b_off_t buf_start = -1000;	/* First line in buffer */

static ssize_t bufloaded;	/* Number of valid lines held in buffer */

static b_off_t next_scanline;	/* Next unread input scanline */

ssize_t bufy;				/* y coordinate in buffer */
FILE *buffp;
static const char *file_name;

int rflag = 0;
int inx = 512;
int iny = 512;
int outx = 512;
int outy = 512;


struct pixscale_args {
    struct pixscale_args *self;
    int help;
    int replicate;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
};

static struct pixscale_args *
pixscale_args_from_storage(void *storage)
{
    return storage ? *(struct pixscale_args **)storage : NULL;
}

static int
pixscale_square(struct bu_vls *msg, const char *arg, void *storage, int output)
{
    struct pixscale_args *args = pixscale_args_from_storage(storage);
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
pixscale_input_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixscale_square(msg, arg, storage, 0);
}

static int
pixscale_output_square(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixscale_square(msg, arg, storage, 1);
}

static const struct bu_arg_option pixscale_options[] = {
    BU_ARG_FLAG("h", "help", struct pixscale_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("r", "replicate", struct pixscale_args, replicate,
	"Use nearest-neighbor replication when enlarging"),
    BU_ARG_CUSTOM("s", "input-size", struct pixscale_args, self,
	pixscale_input_square, "pixels", "Set both input dimensions"),
    BU_ARG_INTEGER_RANGE("w", "input-width", struct pixscale_args, input_width,
	1, 1, 1, INT_MAX, "pixels", "Set the input width"),
    BU_ARG_INTEGER_RANGE("n", "input-height", struct pixscale_args, input_height,
	1, 1, 1, INT_MAX, "scanlines", "Set the input height"),
    BU_ARG_CUSTOM("S", "output-size", struct pixscale_args, self,
	pixscale_output_square, "pixels", "Set both output dimensions"),
    BU_ARG_INTEGER_RANGE("W", "output-width", struct pixscale_args, output_width,
	1, 1, 1, INT_MAX, "pixels", "Set the output width"),
    BU_ARG_INTEGER_RANGE("N", "output-height", struct pixscale_args, output_height,
	1, 1, 1, INT_MAX, "scanlines", "Set the output height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixscale_file_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand pixscale_legacy_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 1, 1, "PIX input file", NULL),
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

static const struct bu_arg_schema pixscale_file_schema = BU_ARG_SCHEMA(
    "pixscale", "Scale a PIX image", pixscale_options, pixscale_file_operands,
    BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_schema pixscale_legacy_schema = BU_ARG_SCHEMA(
    "pixscale", "Scale a PIX image using positional dimensions",
    pixscale_options, pixscale_legacy_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_form pixscale_form_rows[] = {
    BU_ARG_FORM("options", "Specify dimensions with options",
	&pixscale_file_schema, NULL),
    BU_ARG_FORM("legacy", "Specify all dimensions positionally",
	&pixscale_legacy_schema, NULL),
    BU_ARG_FORM_NULL
};

static const struct bu_arg_form *
pixscale_form_select(const struct bu_arg_forms *forms, size_t argc,
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

static const struct bu_arg_forms pixscale_forms = BU_ARG_FORMS(
    "pixscale", "Scale a PIX image", pixscale_form_rows, pixscale_form_select);

static const struct bu_arg_grammar pixscale_grammar = {
    NULL, NULL, &pixscale_forms
};

/****** THIS PROBABLY SHOULD BE ELSEWHERE *******/

/* ceiling and floor functions for positive numbers */
#define CEILING(x)	(((x) > (int)(x)) ? (int)(x)+1 : (int)(x))
#define FLOOR(x)	((int)(x))
#define MIN(x, y)	(((x) > (y)) ? (y) : (x))


static size_t
read_scanlines(unsigned char *destination, size_t count)
{
    size_t ret = fread(destination, (size_t)scanlen, count, buffp);

    if (ret < count && ferror(buffp))
	bu_exit(3, "pixscale: error reading input: %s\n", strerror(errno));

    return ret;
}


/* Refill without seeking so files and standard input behave alike. */
void
fill_buffer(int y)
{
    size_t keep = 0;

    if ((b_off_t)y < buf_start)
	bu_exit(3, "pixscale: cannot read input scanlines out of order\n");

    if (bufloaded > 0 && (b_off_t)y < buf_start + bufloaded) {
	keep = (size_t)(buf_start + bufloaded - y);
	memmove(buffer, buffer + ((b_off_t)y - buf_start) * scanlen, keep * (size_t)scanlen);
    } else {
	while (next_scanline < (b_off_t)y) {
	    size_t skip = (size_t)((b_off_t)y - next_scanline);
	    size_t chunk = (skip < (size_t)buflines) ? skip : (size_t)buflines;
	    size_t ret = read_scanlines(buffer, chunk);

	    next_scanline += (b_off_t)ret;
	    if (ret != chunk)
		bu_exit(3, "pixscale: input ends before scanline %d\n", y);
	}
    }

    buf_start = y;
    bufloaded = (ssize_t)(keep + read_scanlines(buffer + keep * (size_t)scanlen,
	(size_t)buflines - keep));
    next_scanline += (b_off_t)(bufloaded - (ssize_t)keep);
}


static void
buffer_scanlines(int y, size_t count)
{
    b_off_t offset = (b_off_t)y - buf_start;

    if (offset < 0 || offset + (b_off_t)count > bufloaded) {
	fill_buffer(y);
	offset = (b_off_t)y - buf_start;
    }

    if (offset < 0 || offset + (b_off_t)count > bufloaded)
	bu_exit(3, "pixscale: input ends before scanline %d\n", y + (int)count - 1);

    bufy = (ssize_t)offset;
}


/*
 * Nearest Neighbor Interpolate a file of pixels.
 *
 * This version preserves the outside pixels and interps inside only.
 */
void
ninterp(FILE *ofp, int ix, int iy, int ox, int oy)
{
    int i, j;
    double x, y;
    double xstep, ystep;
    unsigned char *op, *lp;
    size_t ret;

    xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
    ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	y = j * ystep;
	/*
	 * Make sure we have this row (and the one after it)
	 * in the buffer
	 */
	buffer_scanlines((int)y, 1);

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    lp = &buffer[bufy*scanlen+(int)x*3];
	    *op++ = lp[0];
	    *op++ = lp[1];
	    *op++ = lp[2];
	}

	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
    double x, y, dx, dy, mid1, mid2;
    double xstep, ystep;
    unsigned char *op, *up, *lp;
    size_t ret;

    xstep = (double)(ix - 1) / (double)ox - 1.0e-6;
    ystep = (double)(iy - 1) / (double)oy - 1.0e-6;

    /* For each output pixel */
    for (j = 0; j < oy; j++) {
	y = j * ystep;
	/*
	 * Make sure we have this row (and the one after it)
	 * in the buffer
	 */
	buffer_scanlines((int)y, INTERPOLATION_SCANLINES);

	op = outbuf;

	for (i = 0; i < ox; i++) {
	    x = i * xstep;
	    dx = x - (int)x;
	    dy = y - (int)y;

	    /* Note: (1-a)*foo + a*bar = foo + a*(bar-foo) */

	    lp = &buffer[bufy*scanlen+(int)x*3];
	    up = &buffer[(bufy+1)*scanlen+(int)x*3];

	    /* Red */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	    lp++; up++;

	    /* Green */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	    lp++; up++;

	    /* Blue */
	    mid1 = lp[0] + dx * ((double)lp[3] - (double)lp[0]);
	    mid2 = up[0] + dx * ((double)up[3] - (double)up[0]);
	    *op++ = mid1 + dy * (mid2 - mid1);
	}

	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
    double sumr, sumg, sumb;
    unsigned char *op;
    size_t ret;

    if (ix == ox)
	pxlen = 1.0;
    else
	pxlen = (double)ix / (double)ox;
    if (iy == oy)
	pylen = 1.0;
    else
	pylen = (double)iy / (double)oy;
    if ((pxlen < 1.0 && pylen > 1.0) || (pxlen > 1.0 && pylen < 1.0)) {
	bu_log("pixscale: can't stretch one way and compress another!\n");
	return -1;
    }
    if (pxlen < 1.0 || pylen < 1.0) {
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
	ystart = j * pylen;
	yend = ystart + pylen;
	op = outbuf;
	for (i = 0; i < ox; i++) {
	    xstart = i * pxlen;
	    xend = xstart + pxlen;
	    sumr = sumg = sumb = 0.0;
	    /*
	     * For each pixel of the original falling
	     * inside this new pixel.
	     */
	    for (l = FLOOR(ystart); l < CEILING(yend); l++) {

		/* Make sure we have this row in the buffer */
		buffer_scanlines(l, 1);

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
		    sumr += buffer[bufy * scanlen + 3*k] * xdist * ydist;
		    sumg += buffer[bufy * scanlen + 3*k+1] * xdist * ydist;
		    sumb += buffer[bufy * scanlen + 3*k+2] * xdist * ydist;
		}
	    }
	    *op++ = (int)(sumr / (pxlen * pylen));
	    *op++ = (int)(sumg / (pxlen * pylen));
	    *op++ = (int)(sumb / (pxlen * pylen));
	}
	ret = fwrite(outbuf, 3, ox, ofp);
	if (ret < (size_t)ox)
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
     * Do a max of page size.  We really should see how big the input
     * file is to decide if we should buffer less than our max.
     */
    if (max > BU_PAGE_SIZE)
	max = BU_PAGE_SIZE;

    if (iny > 1 && max < INTERPOLATION_SCANLINES)
	max = INTERPOLATION_SCANLINES;
    else if (max < 1)
	max = 1;

    if (max < iny)
	buflines = max;
    else
	buflines = iny;

    buf_start = 0;
    bufloaded = 0;
    next_scanline = 0;
    buffer = (unsigned char *)bu_malloc(buflines * scanlen, "buffer");
}


static int
get_args(int argc, char **argv, struct pixscale_args *args)
{
    const struct bu_arg_form *selected = NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_forms_parse(&pixscale_forms, args, &msg,
	argc, (const char **)argv, NULL, &selected);

    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 0;
    }
    bu_vls_free(&msg);
    if (args->help)
	return 1;

    if (selected == &pixscale_form_rows[1]) {
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
	bu_log("pixscale: cannot open \"%s\" for reading\n", file_name);
	return 0;
    }

    return 1;
}


int
main(int argc, char **argv)
{
    struct pixscale_args args = {NULL, 0, 0, 512, 512, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int i;
    int introspection;

    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    introspection = bu_arg_introspect(&msg, &pixscale_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    bu_vls_free(&msg);

    if (!get_args(argc, argv, &args) || isatty(fileno(stdout))) {
	char *help = bu_arg_forms_help(&pixscale_forms, "pixscale");
	bu_flog(stderr, "%s", help ? help : "");
	bu_free(help, "pixscale help");
	return 1;
    }
    if (args.help) {
	char *help = bu_arg_forms_help(&pixscale_forms, "pixscale");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixscale help");
	return 0;
    }

    if (inx <= 0 || iny <= 0 || outx <= 0 || outy <= 0) {
	bu_exit(2, "pixscale: bad size\n");
    }

    /* See how many lines we can buffer */
    scanlen = 3 * inx;
    init_buffer();
    if (inx < outx) i = outx * 3;
    else i = inx * 3;

    outbuf = (unsigned char *)bu_malloc(i, "outbuf");

    /* Here we go */
    scale(stdout, inx, iny, outx, outy);
    bu_free(outbuf, "outbuf");
    bu_free(buffer, "buffer");
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
