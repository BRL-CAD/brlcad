/*                      P I X H A L V E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
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
/** @file util/pixhalve.c
 *
 * Reduce the resolution of a .pix file by one half in each direction,
 * using a 5x5 pyramid filter.
 *
 * As this tool is used primarily for preparing images for NTSC
 * television, convert RGB to YUV, then apply different filter
 * kernels; use 3x3 for Y, 5x5 for U and V.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"

struct pixhalve_args {
    struct pixhalve_args *self;
    int help;
    int autosize;
    size_t file_width;
};

enum pixhalve_dimension {
    PIXHALVE_SIZE,
    PIXHALVE_WIDTH,
    PIXHALVE_HEIGHT
};

static struct pixhalve_args *
pixhalve_args_from_storage(void *storage)
{
    return storage ? *(struct pixhalve_args **)storage : NULL;
}

static int
pixhalve_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct pixhalve_args *args = pixhalve_args_from_storage(storage);

    if (args)
	args->autosize = 1;
    return 0;
}

static int
pixhalve_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pixhalve_dimension dimension)
{
    struct pixhalve_args *args = pixhalve_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args) {
	if (dimension != PIXHALVE_HEIGHT)
	    args->file_width = value;
	args->autosize = 0;
    }
    return 0;
}

#define PIXHALVE_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pixhalve_dimension(msg, arg, storage, _dimension); \
    }

PIXHALVE_DIMENSION_CALLBACK(pixhalve_size, PIXHALVE_SIZE)
PIXHALVE_DIMENSION_CALLBACK(pixhalve_width, PIXHALVE_WIDTH)
PIXHALVE_DIMENSION_CALLBACK(pixhalve_height, PIXHALVE_HEIGHT)

static const struct bu_arg_option pixhalve_options[] = {
    BU_ARG_FLAG("h", "help", struct pixhalve_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct pixhalve_args,
	self, pixhalve_autosize, "Determine dimensions from a named input file"),
    BU_ARG_CUSTOM("s", "size", struct pixhalve_args, self, pixhalve_size,
	"pixels", "Set the input width for a square image"),
    BU_ARG_CUSTOM("w", "width", struct pixhalve_args, self, pixhalve_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pixhalve_args, self,
	pixhalve_height, "pixels", "Accept the input height for compatibility"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixhalve_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixhalve_schema = BU_ARG_SCHEMA(
    "pixhalve", "Reduce a PIX image to half resolution",
    pixhalve_options, pixhalve_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixhalve_grammar = {
    &pixhalve_schema, NULL, NULL
};


/*
 * Unpack RGB byte triples into three separate arrays of integers.
 * The first and last pixels are replicated twice, to handle border effects.
 *
 * Updated version:  the outputs are Y U V values, not R G B.
 */
static void
separate(int *rop, int *gop, int *bop, unsigned char *cp, long int num)
/* Y */
/* U */
/* V */


{
    long int i;
    int r, g, b;

    r = cp[0];
    g = cp[1];
    b = cp[2];

#define YCONV(_r, _g, _b)	(_r * 0.299 + _g * 0.587 + _b * 0.144 + 0.9)
#define UCONV(_r, _g, _b)	(_r * -0.1686 + _g * -0.3311 + _b * 0.4997 + 0.9)
#define VCONV(_r, _g, _b)	(_r * 0.4998 + _g * -0.4185 + _b * -0.0813 + 0.9)

    rop[-1] = rop[-2] = YCONV(r, g, b);
    gop[-1] = gop[-2] = UCONV(r, g, b);
    bop[-1] = bop[-2] = VCONV(r, g, b);

    for (i = num-1; i >= 0; i--) {
	r = cp[0];
	g = cp[1];
	b = cp[2];
	cp += 3;
	*rop++ = YCONV(r, g, b);
	*gop++ = UCONV(r, g, b);
	*bop++ = VCONV(r, g, b);
    }

    r = cp[-3];
    g = cp[-2];
    b = cp[-1];

    *rop++ = YCONV(r, g, b);
    *gop++ = UCONV(r, g, b);
    *bop++ = VCONV(r, g, b);

    *rop++ = YCONV(r, g, b);
    *gop++ = UCONV(r, g, b);
    *bop++ = VCONV(r, g, b);
}


/*
 * Combine three separate arrays of integers into a buffer of
 * RGB byte triples
 */
static void
combine(unsigned char *cp, int *rip, int *gip, int *bip, long int num)
{
    long int i;

#define RCONV(_y, _u, _v)	(_y + 1.4026 * _v)
#define GCONV(_y, _u, _v)	(_y - 0.3444 * _u - 0.7144 * _v)
#define BCONV(_y, _u, _v)	(_y + 1.7730 * _u)

#define CLIP(_v)	(((_v) <= 0) ? 0 : (((_v) >= 255) ? 255 : (_v)))

    for (i = num-1; i >= 0; i--) {
	int y, u, v;
	int r, g, b;

	y = *rip++;
	u = *gip++;
	v = *bip++;

	r = RCONV(y, u, v);
	g = GCONV(y, u, v);
	b = BCONV(y, u, v);

	*cp++ = CLIP(r);
	*cp++ = CLIP(g);
	*cp++ = CLIP(b);
    }
}


/*
 * Ripple all the scanlines down by one.
 *
 * Barrel shift all the pointers down, with [0] going back to the top.
 */
static void
ripple(int **array, int num)
{
    int i;
    int *temp;

    temp = array[0];
    for (i=0; i < num-1; i++)
	array[i] = array[i+1];
    array[num-1] = temp;
}


/*
 * Apply a 5x5 image pyramid to the input scanline, taking every other
 * input position to make an output.
 *
 * Code is arranged so as to vectorize, on machines that can.
 */
static void
filter5(int *op, int **lines, int num)
{
    int i;
    int *a, *b, *c, *d, *e;

    a = lines[0];
    b = lines[1];
    c = lines[2];
    d = lines[3];
    e = lines[4];

#ifdef VECTORIZE
    /* This version vectorizes */
    for (i=0; i < num; i++) {
	j = i*2;
	op[i] = (
	    a[j+0] + 2*a[j+1] + 4*a[j+2] + 2*a[j+3] +   a[j+4] +
	    2*b[j+0] + 4*b[j+1] + 8*b[j+2] + 4*b[j+3] + 2*b[j+4] +
	    4*c[j+0] + 8*c[j+1] +16*c[j+2] + 8*c[j+3] + 4*c[j+4] +
	    2*d[j+0] + 4*d[j+1] + 8*d[j+2] + 4*d[j+3] + 2*d[j+4] +
	    e[j+0] + 2*e[j+1] + 4*e[j+2] + 2*e[j+3] +   e[j+4]
	    ) / 100;
    }
#else
    /* This version is better for non-vectorizing machines */
    for (i=0; i < num; i++) {
	op[i] = (
	    a[0] + 2*a[1] + 4*a[2] + 2*a[3] +   a[4] +
	    2*b[0] + 4*b[1] + 8*b[2] + 4*b[3] + 2*b[4] +
	    4*c[0] + 8*c[1] +16*c[2] + 8*c[3] + 4*c[4] +
	    2*d[0] + 4*d[1] + 8*d[2] + 4*d[3] + 2*d[4] +
	    e[0] + 2*e[1] + 4*e[2] + 2*e[3] +   e[4]
	    ) / 100;
	a += 2;
	b += 2;
	c += 2;
	d += 2;
	e += 2;
    }
#endif
}


/*
 * Apply a 3x3 image pyramid to the input scanline, taking every other
 * input position to make an output.
 *
 * The filter coefficients are positioned so as to align the center
 * of the filter with the same center used in filter5().
 */
static void
filter3(int *op, int **lines, int num)
{
    int i;
    int *b, *c, *d;

    b = lines[1];
    c = lines[2];
    d = lines[3];

#ifdef VECTORIZE
    /* This version vectorizes */
    for (i=0; i < num; i++) {
	j = i*2;
	op[i] = (
	    b[j+1] + 2*b[j+2] +   b[j+3] +
	    2*c[j+1] + 4*c[j+2] + 2*c[j+3] +
	    d[j+1] + 2*d[j+2] +   d[j+3]
	    ) / 16;
    }
#else
    /* This version is better for non-vectorizing machines */
    for (i=0; i < num; i++) {
	op[i] = (
	    b[1] + 2*b[2] +   b[3] +
	    2*c[1] + 4*c[2] + 2*c[3] +
	    d[1] + 2*d[2] +   d[3]
	    ) / 16;
	b += 2;
	c += 2;
	d += 2;
    }
#endif
}


int
main(int argc, char *argv[])
{
    unsigned char *inbuf;
    unsigned char *outbuf;
    int *rout, *gout, *bout;
    int *rlines[5];
    int *glines[5];
    int *blines[5];
    const char *file_name;
    FILE *infp;
    int fileinput = 0;
    size_t out_width;
    size_t i;
    int eof_seen;
    struct pixhalve_args args = {NULL, 0, 0, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixhalve_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixhalve_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixhalve_schema, "pixhalve");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixhalve help");
	return 0;
    }

    if (operand_index + 1 < argc) {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp)
	    bu_exit(1, "pixhalve: cannot open \"%s\" for reading: %s\n",
		file_name, strerror(errno));
	fileinput = 1;
    } else {
	if (isatty(fileno(stdin)))
	    bu_exit(1, "pixhalve: no input file and standard input is a terminal\n");
	file_name = "-";
	infp = stdin;
    }

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* autosize input? */
    if (fileinput && args.autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    args.file_width = w;
	} else {
	    fprintf(stderr, "pixhalve: unable to autosize\n");
	}
    }
    out_width = args.file_width/2;

    /* Allocate 1-scanline input & output buffers */
    inbuf = (unsigned char *)bu_malloc(3*args.file_width+8, "inbuf");
    outbuf = (unsigned char *)bu_malloc(3*(out_width+2)+8, "outbuf");

    /* Allocate 5 integer arrays for each color */
    /* each width+2 elements wide */
    for (i=0; i<5; i++) {
	rlines[i] = (int *)bu_calloc((args.file_width+4)+1, sizeof(long), "rlines");
	glines[i] = (int *)bu_calloc((args.file_width+4)+1, sizeof(long), "glines");
	blines[i] = (int *)bu_calloc((args.file_width+4)+1, sizeof(long), "blines");
    }

    /* Allocate an integer array for each color, for output */
    rout = (int *)bu_malloc(out_width * sizeof(long) + 8, "rout");
    gout = (int *)bu_malloc(out_width * sizeof(long) + 8, "gout");
    bout = (int *)bu_malloc(out_width * sizeof(long) + 8, "bout");

    /*
     * Prime the pumps with 5 lines of image.
     * Repeat the bottom most line three times to generate a "fill"
     * line on the bottom.  This will have to be matched on the top.
     */
    if (fread(inbuf, 3, args.file_width, infp) != args.file_width) {
	perror(file_name);
	fprintf(stderr, "pixhalve:  fread error\n");
	bu_exit (1, NULL);
    }
    separate(&rlines[0][2], &glines[0][2], &blines[0][2], inbuf, args.file_width);
    separate(&rlines[1][2], &glines[1][2], &blines[1][2], inbuf, args.file_width);
    separate(&rlines[2][2], &glines[2][2], &blines[2][2], inbuf, args.file_width);
    for (i=3; i<5; i++) {
	if (fread(inbuf, 3, args.file_width, infp) != args.file_width) {
	    perror(file_name);
	    fprintf(stderr, "pixhalve:  fread error\n");
	    bu_exit (1, NULL);
	}
	separate(&rlines[i][2], &glines[i][2], &blines[i][2],
		 inbuf, args.file_width);
    }

    eof_seen = 0;
    for (;;) {
	filter3(rout, rlines, out_width);
	filter5(gout, glines, out_width);
	filter5(bout, blines, out_width);
	combine(outbuf, rout, gout, bout, out_width);
	if (fwrite((void*)outbuf, 3, out_width, stdout) != out_width) {
	    perror("stdout");
	    bu_exit (2, NULL);
	}

	/* Ripple down two scanlines, and acquire two more */
	if (fread(inbuf, 3, args.file_width, infp) != args.file_width) {
	    if (eof_seen >= 2) break;
	    /* EOF, repeat last line 2x for final output line */
	    eof_seen++;
	    /* Fall through */
	}
	ripple(rlines, 5);
	ripple(glines, 5);
	ripple(blines, 5);
	separate(&rlines[4][2], &glines[4][2], &blines[4][2],
		 inbuf, args.file_width);

	if (fread(inbuf, 3, args.file_width, infp) != args.file_width) {
	    if (eof_seen >= 2) break;
	    /* EOF, repeat last line 2x for final output line */
	    eof_seen++;
	    /* Fall through */
	}
	ripple(rlines, 5);
	ripple(glines, 5);
	ripple(blines, 5);
	separate(&rlines[4][2], &glines[4][2], &blines[4][2],
		 inbuf, args.file_width);
    }

    bu_free(inbuf, "inbuf");
    bu_free(outbuf, "outbuf");

    for (i=0; i<5; i++) {
	bu_free(rlines[i], "rlines");
	bu_free(glines[i], "glines");
	bu_free(blines[i], "blines");
    }

    bu_free(rout, "rout");
    bu_free(gout, "gout");
    bu_free(bout, "bout");

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
