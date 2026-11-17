/*                      P I X E M B E D . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2026 United States Government as represented by
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
/** @file util/pixembed.c
 *
 * Embed a smaller pix file in a larger space, replicating the boundary
 * pixels to fill out the borders, and output as a pix file.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"


unsigned char *obuf;

int scanlen;			/* length of infile (and buffer) scanlines */

FILE *buffp;

size_t xin = 512;
size_t yin = 512;
size_t xout = 512;
size_t yout = 512;

size_t border_inset = 0;	/* Sometimes border pixels are bad */

size_t inbase;

void load_buffer(void), write_buffer(void);

struct pixembed_args {
    struct pixembed_args *self;
    int help;
    size_t border_inset;
    size_t input_width;
    size_t input_height;
    size_t output_width;
    size_t output_height;
};

enum pixembed_dimension {
    PIXEMBED_INPUT_SIZE,
    PIXEMBED_INPUT_WIDTH,
    PIXEMBED_INPUT_HEIGHT,
    PIXEMBED_OUTPUT_SIZE,
    PIXEMBED_OUTPUT_WIDTH,
    PIXEMBED_OUTPUT_HEIGHT
};

static int
pixembed_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pixembed_dimension dimension)
{
    struct pixembed_args *args = storage ?
	*(struct pixembed_args **)storage : NULL;
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (!args)
	return 0;

    if (dimension == PIXEMBED_INPUT_SIZE ||
	dimension == PIXEMBED_INPUT_WIDTH)
	args->input_width = value;
    if (dimension == PIXEMBED_INPUT_SIZE ||
	dimension == PIXEMBED_INPUT_HEIGHT)
	args->input_height = value;
    if (dimension == PIXEMBED_OUTPUT_SIZE ||
	dimension == PIXEMBED_OUTPUT_WIDTH)
	args->output_width = value;
    if (dimension == PIXEMBED_OUTPUT_SIZE ||
	dimension == PIXEMBED_OUTPUT_HEIGHT)
	args->output_height = value;
    return 0;
}

#define PIXEMBED_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pixembed_dimension(msg, arg, storage, _dimension); \
    }

PIXEMBED_DIMENSION_CALLBACK(pixembed_input_size, PIXEMBED_INPUT_SIZE)
PIXEMBED_DIMENSION_CALLBACK(pixembed_input_width, PIXEMBED_INPUT_WIDTH)
PIXEMBED_DIMENSION_CALLBACK(pixembed_input_height, PIXEMBED_INPUT_HEIGHT)
PIXEMBED_DIMENSION_CALLBACK(pixembed_output_size, PIXEMBED_OUTPUT_SIZE)
PIXEMBED_DIMENSION_CALLBACK(pixembed_output_width, PIXEMBED_OUTPUT_WIDTH)
PIXEMBED_DIMENSION_CALLBACK(pixembed_output_height, PIXEMBED_OUTPUT_HEIGHT)

static int
pixembed_border_inset(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixembed_args *args = storage ?
	*(struct pixembed_args **)storage : NULL;
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 0, SIZE_MAX))
	return -1;
    if (args)
	args->border_inset = value;
    return 0;
}

static const struct bu_arg_option pixembed_options[] = {
    BU_ARG_FLAG("h", "help", struct pixembed_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("b", "border-inset", struct pixembed_args, self,
	pixembed_border_inset, "pixels", "Inset the replicated border sample"),
    BU_ARG_CUSTOM("s", "input-size", struct pixembed_args, self,
	pixembed_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct pixembed_args, self,
	pixembed_input_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "input-height", struct pixembed_args, self,
	pixembed_input_height, "pixels", "Set the input image height"),
    BU_ARG_CUSTOM("S", "output-size", struct pixembed_args, self,
	pixembed_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_CUSTOM("W", "output-width", struct pixembed_args, self,
	pixembed_output_width, "pixels", "Set the output image width"),
    BU_ARG_CUSTOM("N", "output-height", struct pixembed_args, self,
	pixembed_output_height, "pixels", "Set the output image height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixembed_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file; standard input when omitted", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixembed_schema = BU_ARG_SCHEMA(
    "pixembed", "Embed a PIX image in a larger frame",
    pixembed_options, pixembed_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixembed_grammar = {
    &pixembed_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    size_t bottom_margin;
    size_t interior_rows;
    size_t top_margin;
    size_t i;
    size_t y;
    struct pixembed_args args = {NULL, 0, 0, 512, 512, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    args.self = &args;
    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixembed_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixembed_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixembed_schema, "pixembed");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixembed help");
	return 0;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixembed cannot send PIX output to a terminal\n");

    xin = args.input_width;
    yin = args.input_height;
    xout = args.output_width;
    yout = args.output_height;
    border_inset = args.border_inset;

    if (operand_index < argc - 1) {
	const char *path = argv[operand_index + 1];
	buffp = fopen(path, "rb");
	if (!buffp)
	    bu_exit(1, "pixembed: cannot open \"%s\" for reading: %s\n",
		path, strerror(errno));
    } else {
	if (isatty(fileno(stdin)))
	    bu_exit(1, "pixembed requires an input file or redirected input\n");
	buffp = stdin;
    }

    setmode(fileno(buffp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (xout < xin || yout < yin) {
	fprintf(stderr, "pixembed: output size must be at least the input size\n");
	bu_exit (3, NULL);
    }

    if (border_inset > (xin - 1) / 2 || border_inset > (yin - 1) / 2) {
	fprintf(stderr, "pixembed: border inset must leave at least one input row and column\n");
	bu_exit (4, NULL);
    }

    inbase = (xout - xin) / 2;
    bottom_margin = (yout - yin) / 2;
    top_margin = yout - yin - bottom_margin;
    interior_rows = yin - 2 * border_inset;

    /* Allocate storage for one output line */
    scanlen = 3*xout;
    obuf = (unsigned char *)bu_malloc(scanlen, "obuf");

    /* Discard suspect border rows, then replicate the first usable row. */
    for (i = 0; i < border_inset; i++)
	load_buffer();
    load_buffer();

    for (y = 0; y < bottom_margin + border_inset + 1; y++)
	write_buffer();

    for (y = 1; y < interior_rows; y++) {
	load_buffer();
	write_buffer();
    }

    /* Replicate the last usable row through the discarded edge and margin. */
    for (y = 0; y < top_margin + border_inset; y++)
	write_buffer();

    if (buffp != stdin && fclose(buffp) == EOF)
	bu_exit(1, "pixembed: error closing input\n");
    bu_free(obuf, "obuf");

    return 0;
}


/*
 * Read one input scanline into the middle of the output scanline,
 * and duplicate the border pixels.
 */
void
load_buffer(void)
{
    size_t ret;
    unsigned char r, g, b;
    unsigned char *cp;
    size_t i;

    ret = fread(obuf + inbase*3, 3, xin, buffp);
    if (ret != xin) {
	perror("pixembed fread");
	bu_exit (7, NULL);
    }
    r = obuf[(inbase+border_inset)*3+0];
    g = obuf[(inbase+border_inset)*3+1];
    b = obuf[(inbase+border_inset)*3+2];

    cp = obuf;
    for (i=0; i<inbase+border_inset; i++) {
	*cp++ = r;
	*cp++ = g;
	*cp++ = b;
    }

    r = obuf[(xin+inbase-1-border_inset)*3+0];
    g = obuf[(xin+inbase-1-border_inset)*3+1];
    b = obuf[(xin+inbase-1-border_inset)*3+2];
    cp = &obuf[(xin+inbase+0-border_inset)*3+0];
    for (i=xin+inbase-border_inset; i<xout; i++) {
	*cp++ = r;
	*cp++ = g;
	*cp++ = b;
    }

}


/*
 * Write the buffer to stdout, with error checking.
 */
void
write_buffer(void)
{
    if (fwrite(obuf, 3, xout, stdout) != xout) {
	perror("pixembed stdout fwrite");
	bu_exit (8, NULL);
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
