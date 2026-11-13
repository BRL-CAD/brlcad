/*                     P I X C O L O R S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file util/pixcolors.c
 *
 * Count the number of different pixel values in a PIX format image.
 *
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"


#define RGB_CHANNEL_COUNT 3
#define PIX_COLOR_BITS 24
#define PIX_COLOR_COUNT ((size_t)1 << PIX_COLOR_BITS)
#define COLOR_SET_BYTES (PIX_COLOR_COUNT / CHAR_BIT)
#define PIXEL_BUFFER_COUNT 1024

struct pixcolors_args {
    int help;
    int verbose;
};

static const struct bu_arg_option pixcolors_options[] = {
    BU_ARG_FLAG("h", "help", struct pixcolors_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct pixcolors_args, verbose,
	"List each distinct RGB color"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixcolors_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"PIX input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixcolors_schema = BU_ARG_SCHEMA(
    "pixcolors", "Count or list distinct colors in a BRL-CAD PIX image",
    pixcolors_options, pixcolors_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixcolors_grammar = {
    &pixcolors_schema, NULL, NULL
};


static int
count_colors(FILE *input, unsigned char *color_set, int verbose)
{
    unsigned char pixels[PIXEL_BUFFER_COUNT * RGB_CHANNEL_COUNT];
    size_t bytes;
    size_t color_count = 0;

    while ((bytes = fread(pixels, 1, sizeof(pixels), input)) > 0) {
	if (bytes % RGB_CHANNEL_COUNT) {
	    bu_flog(stderr, "pixcolors: input ends with an incomplete pixel\n");
	    return 0;
	}
	for (size_t offset = 0; offset < bytes; offset += RGB_CHANNEL_COUNT) {
	    unsigned int color = (unsigned int)pixels[offset] |
		((unsigned int)pixels[offset + 1] << 8) |
		((unsigned int)pixels[offset + 2] << 16);
	    size_t byte_index = color / CHAR_BIT;
	    unsigned char mask = (unsigned char)(1u << (color % CHAR_BIT));
	    if (!(color_set[byte_index] & mask)) {
		color_set[byte_index] |= mask;
		color_count++;
	    }
	}
    }
    if (ferror(input)) {
	bu_flog(stderr, "pixcolors: error reading input\n");
	return 0;
    }

    printf("%zu\n", color_count);
    if (verbose) {
	for (size_t color = 0; color < PIX_COLOR_COUNT; color++) {
	    unsigned char mask = (unsigned char)(1u << (color % CHAR_BIT));
	    if (color_set[color / CHAR_BIT] & mask) {
		printf("%3u %3u %3u\n",
		    (unsigned int)(color & 0xff),
		    (unsigned int)((color >> 8) & 0xff),
		    (unsigned int)((color >> 16) & 0xff));
	    }
	}
    }
    if (ferror(stdout) || fflush(stdout) == EOF) {
	bu_flog(stderr, "pixcolors: error writing output\n");
	return 0;
    }
    return 1;
}


int
main(int argc, char **argv)
{
    FILE *input = stdin;
    struct pixcolors_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixcolors_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixcolors_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixcolors_schema, "pixcolors");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixcolors help");
	return 0;
    }

    int operand_count = argc - operand_index - 1;
    if (operand_count == 1) {
	const char *path = argv[operand_index + 1];
	input = fopen(path, "rb");
	if (!input)
	    bu_exit(1, "pixcolors: cannot open '%s' for reading\n", path);
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "pixcolors requires an input file or redirected input\n");
    }

    setmode(fileno(input), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    unsigned char *color_set = (unsigned char *)bu_calloc(
	COLOR_SET_BYTES, 1, "pixcolors color set");
    int success = count_colors(input, color_set, args.verbose);
    bu_free(color_set, "pixcolors color set");

    if (input != stdin && fclose(input) == EOF) {
	bu_flog(stderr, "pixcolors: error closing input\n");
	success = 0;
    }

    return success ? 0 : 1;
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
