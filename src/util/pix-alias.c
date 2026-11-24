/*                     P I X - A L I A S . C
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
/** @file util/pix-alias.c
 *
 * Convert BRL PIX format image files to ALIAS(tm) PIX format files.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"


#define ALIAS_BITPLANES 24
#define ALIAS_MAX_RUN_LENGTH 255
#define DEFAULT_IMAGE_DIMENSION 512
#define RGB_CHANNEL_COUNT 3

struct pix_alias_dimensions {
    int width;
    int height;
};

struct pix_alias_args {
    int help;
    struct pix_alias_dimensions dimensions;
};

static int
pix_alias_size_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    int size;
    if (!bu_arg_integer_range_from_str(msg, &size, arg, 0, 1, USHRT_MAX))
	return -1;
    if (storage) {
	struct pix_alias_dimensions *dimensions =
	    (struct pix_alias_dimensions *)storage;
	dimensions->width = size;
	dimensions->height = size;
    }
    return 0;
}

static const struct bu_arg_option pix_alias_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_alias_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("s", "size", struct pix_alias_args, dimensions,
	pix_alias_size_from_str, "pixels", "Set both input dimensions"),
    BU_ARG_INTEGER_RANGE("w", "width", struct pix_alias_args,
	dimensions.width, 1, 1, 1, USHRT_MAX, "pixels",
	"Set the input image width"),
    BU_ARG_INTEGER_RANGE("n", "height", struct pix_alias_args,
	dimensions.height, 1, 1, 1, USHRT_MAX, "scanlines",
	"Set the input image height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema pix_alias_schema = BU_ARG_SCHEMA(
    "pix-alias", "Convert a BRL-CAD PIX image to Alias PIX format",
    pix_alias_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_alias_grammar = {
    &pix_alias_schema, NULL, NULL
};


static int
write_u16(unsigned int value)
{
    return putchar((int)((value >> 8) & 0xff)) != EOF &&
	putchar((int)(value & 0xff)) != EOF;
}


static int
write_alias_image(int width, int height)
{
    size_t scanline_size = (size_t)width * RGB_CHANNEL_COUNT;
    if ((size_t)height > SIZE_MAX / scanline_size) {
	bu_flog(stderr, "pix-alias: image dimensions are too large\n");
	return 0;
    }
    size_t image_size = scanline_size * (size_t)height;
    unsigned char *image = (unsigned char *)bu_malloc(
	image_size, "pix-alias image");

    for (int scanline = height - 1; scanline >= 0; scanline--) {
	if (fread(image + (size_t)scanline * scanline_size, 1,
		scanline_size, stdin) != scanline_size) {
	    bu_flog(stderr, "pix-alias: error reading scanline %d\n", scanline);
	    bu_free(image, "pix-alias image");
	    return 0;
	}
    }

    if (!write_u16((unsigned int)width) ||
	!write_u16((unsigned int)height) ||
	!write_u16(0) ||
	!write_u16((unsigned int)(height - 1)) ||
	!write_u16(ALIAS_BITPLANES)) {
	bu_flog(stderr, "pix-alias: error writing header\n");
	bu_free(image, "pix-alias image");
	return 0;
    }

    for (size_t index = 0; index < image_size;) {
	size_t pixel_start = index;
	unsigned int run_length = 0;
	do {
	    index += RGB_CHANNEL_COUNT;
	    run_length++;
	} while (run_length < ALIAS_MAX_RUN_LENGTH && index < image_size &&
	    memcmp(image + index, image + pixel_start, RGB_CHANNEL_COUNT) == 0);

	if (putchar((int)run_length) == EOF ||
	    putchar(image[pixel_start + 2]) == EOF ||
	    putchar(image[pixel_start + 1]) == EOF ||
	    putchar(image[pixel_start]) == EOF) {
	    bu_flog(stderr, "pix-alias: error writing pixel data\n");
	    bu_free(image, "pix-alias image");
	    return 0;
	}
    }

    int success = !(ferror(stdout) || fflush(stdout) == EOF);
    if (!success)
	bu_flog(stderr, "pix-alias: error writing output\n");
    bu_free(image, "pix-alias image");
    return success;
}


int
main(int argc, char **argv)
{
    struct pix_alias_args args = {
	0, {DEFAULT_IMAGE_DIMENSION, DEFAULT_IMAGE_DIMENSION}
    };
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pix_alias_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pix_alias_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pix_alias_schema, "pix-alias");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix-alias help");
	return 0;
    }

    if (isatty(fileno(stdin)))
	bu_exit(1, "pix-alias requires redirected binary input\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    return write_alias_image(args.dimensions.width, args.dimensions.height) ?
	0 : 1;
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
