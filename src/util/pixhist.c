/*                       P I X H I S T . C
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
/** @file util/pixhist.c
 *
 * Display a color histogram of a pix file.
 * 0 is top of screen, 255 bottom.
 *
 */

#include "common.h"

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"


#define FRAMEBUFFER_DIMENSION 512
#define HISTOGRAM_BINS 256
#define INPUT_PIXEL_COUNT 2048
#define RGB_CHANNEL_COUNT 3

struct pixhist_args {
    int help;
    int verbose;
};

static const struct bu_arg_option pixhist_options[] = {
    BU_ARG_FLAG("h", "help", struct pixhist_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct pixhist_args, verbose,
	"Print per-channel histogram counts"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixhist_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixhist_schema = BU_ARG_SCHEMA(
    "pixhist", "Display PIX channel histograms on a framebuffer",
    pixhist_options, pixhist_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixhist_grammar = {
    &pixhist_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    long red_histogram[HISTOGRAM_BINS] = {0};
    long green_histogram[HISTOGRAM_BINS] = {0};
    long blue_histogram[HISTOGRAM_BINS] = {0};
    unsigned char input[INPUT_PIXEL_COUNT * RGB_CHANNEL_COUNT];
    unsigned char line[FRAMEBUFFER_DIMENSION * RGB_CHANNEL_COUNT] = {0};
    FILE *input_file = stdin;
    struct pixhist_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixhist_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixhist_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixhist_schema, "pixhist");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixhist help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	const char *path = argv[operand_index + 1];
	input_file = fopen(path, "rb");
	if (!input_file)
	    bu_exit(1, "pixhist: cannot open '%s' for reading\n", path);
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "pixhist requires an input file or redirected input\n");
    }

    setmode(fileno(input_file), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    size_t bytes;
    while ((bytes = fread(input, 1, sizeof(input), input_file)) > 0) {
	if (bytes % RGB_CHANNEL_COUNT)
	    bu_exit(1, "pixhist: input ends with an incomplete pixel\n");
	for (size_t offset = 0; offset < bytes; offset += RGB_CHANNEL_COUNT) {
	    red_histogram[input[offset + RED]]++;
	    green_histogram[input[offset + GRN]]++;
	    blue_histogram[input[offset + BLU]]++;
	}
    }
    if (ferror(input_file))
	bu_exit(1, "pixhist: error reading input\n");
    if (input_file != stdin && fclose(input_file) == EOF)
	bu_exit(1, "pixhist: error closing input\n");

    long maximum = 1;
    for (size_t index = 0; index < HISTOGRAM_BINS; index++) {
	if (red_histogram[index] > maximum)
	    maximum = red_histogram[index];
	if (green_histogram[index] > maximum)
	    maximum = green_histogram[index];
	if (blue_histogram[index] > maximum)
	    maximum = blue_histogram[index];
    }
    double scale = (FRAMEBUFFER_DIMENSION - 1) / (double)maximum;
    bu_log("Max bin count=%ld.  %g count/pixel\n", maximum, scale);

    struct fb *framebuffer = fb_open(NULL, FRAMEBUFFER_DIMENSION,
	FRAMEBUFFER_DIMENSION);
    if (!framebuffer)
	bu_exit(12, "pixhist: framebuffer open failed\n");

    for (size_t index = 0; index < HISTOGRAM_BINS; index++) {
	int red_level = (int)(red_histogram[index] * scale);
	int green_level = (int)(green_histogram[index] * scale);
	int blue_level = (int)(blue_histogram[index] * scale);
	for (int pixel = 0; pixel < FRAMEBUFFER_DIMENSION; pixel++) {
	    line[pixel * RGB_CHANNEL_COUNT + RED] = pixel < red_level ? 255 : 0;
	    line[pixel * RGB_CHANNEL_COUNT + GRN] = pixel < green_level ? 255 : 0;
	    line[pixel * RGB_CHANNEL_COUNT + BLU] = pixel < blue_level ? 255 : 0;
	}
	int line_length = red_level;
	if (green_level > line_length)
	    line_length = green_level;
	if (blue_level > line_length)
	    line_length = blue_level;

	fb_write(framebuffer, 0, (int)(2 * index), line, line_length);
	fb_write(framebuffer, 0, (int)(2 * index + 1), line, line_length);
	if (args.verbose) {
	    bu_log("%3zu: %10ld %10ld %10ld (%d)\n", index,
		red_histogram[index], green_histogram[index],
		blue_histogram[index], line_length);
	}
    }

    fb_close(framebuffer);
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
