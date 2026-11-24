/*                   P I X I N T E R P 2 X . C
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
/** @file util/pixinterp2x.c
 *
 * Read a .pix file of a given resolution, and produce one with
 * twice as many pixels by interpolating between the pixels.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


FILE *infp;

size_t file_width = 512;
size_t file_height = 512;

size_t inbytes;			/* # bytes of one input scanline */
size_t outbytes;		/* # bytes of one output scanline */
size_t outsize;			/* size of output buffer */
unsigned char *outbuf;		/* ptr to output image buffer */
void widen_line(unsigned char *cp, int y);

void interp_lines(int out, int i1, int i2);

struct pixinterp2x_args {
    struct pixinterp2x_args *self;
    int help;
    size_t width;
    size_t height;
};

enum pixinterp2x_dimension {
    PIXINTERP2X_SIZE,
    PIXINTERP2X_WIDTH,
    PIXINTERP2X_HEIGHT
};


static int
pixinterp2x_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pixinterp2x_dimension dimension)
{
    struct pixinterp2x_args *args = storage ? *(struct pixinterp2x_args **)storage : NULL;
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args) {
	if (dimension != PIXINTERP2X_HEIGHT)
	    args->width = value;
	if (dimension != PIXINTERP2X_WIDTH)
	    args->height = value;
    }
    return 0;
}


static int
pixinterp2x_size(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixinterp2x_dimension(msg, arg, storage, PIXINTERP2X_SIZE);
}


static int
pixinterp2x_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixinterp2x_dimension(msg, arg, storage, PIXINTERP2X_WIDTH);
}


static int
pixinterp2x_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixinterp2x_dimension(msg, arg, storage, PIXINTERP2X_HEIGHT);
}


static const struct bu_arg_option pixinterp2x_options[] = {
    BU_ARG_FLAG("h", "help", struct pixinterp2x_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("s", "size", struct pixinterp2x_args, self,
	pixinterp2x_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pixinterp2x_args, self,
	pixinterp2x_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pixinterp2x_args, self,
	pixinterp2x_height, "pixels", "Set the input image height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixinterp2x_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixinterp2x_schema = BU_ARG_SCHEMA(
    "pixinterp2x", "Double a PIX image using bilinear interpolation",
    pixinterp2x_options, pixinterp2x_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixinterp2x_grammar = {
    &pixinterp2x_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    size_t iny, outy;
    unsigned char *inbuf;
    size_t ret;
    struct pixinterp2x_args args = {NULL, 0, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    infp = stdin;
    int introspection = bu_arg_introspect(&msg, &pixinterp2x_grammar,
	argc - 1, (const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixinterp2x_schema,
	&args, &msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixinterp2x_schema, "pixinterp2x");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixinterp2x help");
	return 0;
    }
    if (argc - operand_index - 1 == 1) {
	if ((infp = fopen(argv[operand_index + 1], "rb")) == NULL)
	    bu_exit(1, "pixinterp2x: cannot open '%s' for reading\n",
		argv[operand_index + 1]);
    }
    if (isatty(fileno(infp)) || isatty(fileno(stdout)))
	bu_exit(1, "pixinterp2x requires redirected input and output\n");

    file_width = args.width;
    file_height = args.height;

    inbytes = file_width * 3;	/* bytes/ input line */
    inbuf = (unsigned char *)malloc(inbytes);

    outbytes = inbytes * 2;		/* bytes/ output line */
    outsize = file_width * file_height * 4 * 3;
    if ((outbuf = (unsigned char *)malloc(outsize)) == (unsigned char *)0) {
	fprintf(stderr, "pixinterp2x:  unable to malloc buffer\n");
	bu_exit (1, NULL);
    }

    outy = -2;
    for (iny = 0; iny < file_height; iny++) {
	ret = fread((char *)inbuf, 1, inbytes, infp);
	if (ret != inbytes) {
	    fprintf(stderr, "pixinterp2x fread error\n");
	    break;
	}

	outy += 2;
	/* outy is line we will write on */
	widen_line(inbuf, outy);
	if (outy == 0)
	    widen_line(inbuf, ++outy);
	else
	    interp_lines(outy-1, outy, outy-2);
    }
    ret = write(1, (char *)outbuf, outsize);
    bu_free(inbuf, "inbuf alloc from malloc");
    if (ret != outsize) {
	perror("pixinterp2x write");
	bu_exit (1, NULL);
    }

    return 0;
}


void
widen_line(unsigned char *cp, int y)
{
    unsigned char *op;
    size_t i;

    op = (unsigned char *)outbuf + (y * outbytes);
    /* Replicate first pixel */
    *op++ = cp[0];
    *op++ = cp[1];
    *op++ = cp[2];
    /* Prep process by copying first pixel */
    *op++ = *cp++;
    *op++ = *cp++;
    *op++ = *cp++;
    for (i=0; i<inbytes; i+=3) {
	/* Average previous pixel with current pixel */
	*op++ = ((int)cp[-3+0] + (int)cp[0])>>1;
	*op++ = ((int)cp[-3+1] + (int)cp[1])>>1;
	*op++ = ((int)cp[-3+2] + (int)cp[2])>>1;
	/* Copy pixel */
	*op++ = *cp++;
	*op++ = *cp++;
	*op++ = *cp++;
    }
}


void
interp_lines(int out, int i1, int i2)
{
    unsigned char *a, *b;	/* inputs */
    unsigned char *op;
    size_t i;

    a = (unsigned char *)outbuf + (i1 * outbytes);
    b = (unsigned char *)outbuf + (i2 * outbytes);
    op = (unsigned char *)outbuf + (out * outbytes);

    for (i=0; i<outbytes; i+=3) {
	*op++ = ((int)*a++ + (int)*b++)>>1;
	*op++ = ((int)*a++ + (int)*b++)>>1;
	*op++ = ((int)*a++ + (int)*b++)>>1;
    }
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
