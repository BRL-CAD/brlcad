/*                     P I X E L S W A P . C
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
/** @file util/pixelswap.c
 *
 * interchange pixel values in an image
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"


#define RGB_CHANNEL_COUNT 3
#define PIXEL_BUFFER_COUNT 32767

struct pixelswap_args {
    int help;
    int pixel_depth;
};

static const struct bu_arg_option pixelswap_options[] = {
    BU_ARG_FLAG("h", "help", struct pixelswap_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_INTEGER_RANGE("d", "depth", struct pixelswap_args, pixel_depth,
	1, RGB_CHANNEL_COUNT, 1, RGB_CHANNEL_COUNT, "bytes",
	"Bytes per pixel (only RGB depth 3 is supported)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixelswap_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("source-red", 1, 1, 1, 0, 1, 255,
	"Red component of the first color", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("source-green", 1, 1, 1, 0, 1, 255,
	"Green component of the first color", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("source-blue", 1, 1, 1, 0, 1, 255,
	"Blue component of the first color", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("replacement-red", 1, 1, 1, 0, 1, 255,
	"Red component of the second color", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("replacement-green", 1, 1, 1, 0, 1, 255,
	"Green component of the second color", NULL),
    BU_ARG_OPERAND_INTEGER_RANGE("replacement-blue", 1, 1, 1, 0, 1, 255,
	"Blue component of the second color", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixelswap_schema = BU_ARG_SCHEMA(
    "pixelswap", "Exchange two RGB colors in a BRL-CAD PIX stream",
    pixelswap_options, pixelswap_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixelswap_grammar = {
    &pixelswap_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    size_t bytes, pixel;
    unsigned char source[RGB_CHANNEL_COUNT];
    unsigned char replacement[RGB_CHANNEL_COUNT];
    unsigned char input[PIXEL_BUFFER_COUNT * RGB_CHANNEL_COUNT];
    unsigned char output[PIXEL_BUFFER_COUNT * RGB_CHANNEL_COUNT];
    struct pixelswap_args args = {0, RGB_CHANNEL_COUNT};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixelswap_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixelswap_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixelswap_schema, "pixelswap");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixelswap help");
	return 0;
    }

    const char **operands = (const char **)(argv + operand_index + 1);
    if (!bu_arg_unsigned_char_from_str(&msg, &source[0], operands[0], 10) ||
	!bu_arg_unsigned_char_from_str(&msg, &source[1], operands[1], 10) ||
	!bu_arg_unsigned_char_from_str(&msg, &source[2], operands[2], 10) ||
	!bu_arg_unsigned_char_from_str(&msg, &replacement[0], operands[3], 10) ||
	!bu_arg_unsigned_char_from_str(&msg, &replacement[1], operands[4], 10) ||
	!bu_arg_unsigned_char_from_str(&msg, &replacement[2], operands[5], 10))
	bu_exit(1, "pixelswap: color: %s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);

    if (isatty(fileno(stdout)) || isatty(fileno(stdin)))
	bu_exit(1, "pixelswap requires redirected binary input and output\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    while ((bytes = fread(input, 1, sizeof(input), stdin)) > 0) {
	if (bytes % RGB_CHANNEL_COUNT)
	    bu_exit(1, "pixelswap: input ends with an incomplete pixel\n");
	for (pixel = 0; pixel < bytes / RGB_CHANNEL_COUNT; pixel++) {
	    unsigned char *input_pixel = &input[pixel * RGB_CHANNEL_COUNT];
	    unsigned char *output_pixel = &output[pixel * RGB_CHANNEL_COUNT];
	    if (memcmp(input_pixel, source, RGB_CHANNEL_COUNT) == 0)
		memcpy(output_pixel, replacement, RGB_CHANNEL_COUNT);
	    else if (memcmp(input_pixel, replacement, RGB_CHANNEL_COUNT) == 0)
		memcpy(output_pixel, source, RGB_CHANNEL_COUNT);
	    else
		memcpy(output_pixel, input_pixel, RGB_CHANNEL_COUNT);
	}

	if (fwrite(output, 1, bytes, stdout) != bytes)
	    bu_exit(1, "pixelswap: error writing output\n");
    }
    if (ferror(stdin))
	bu_exit(1, "pixelswap: error reading input\n");

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
