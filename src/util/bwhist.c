/*                        B W H I S T . C
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
/** @file util/bwhist.c
 *
 * Display, and optionally dump to tty, a histogram of a
 * black and white file.  Black is top of screen, white bottom.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "dm.h"


#define FRAMEBUFFER_DIMENSION 512
#define HISTOGRAM_BINS 256
#define INPUT_BUFFER_SIZE 512
#define RGB_CHANNEL_COUNT 3

struct bwhist_args {
    int help;
    int verbose;
};

static const struct bu_arg_option bwhist_options[] = {
    BU_ARG_FLAG("h", "help", struct bwhist_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("v", "verbose", struct bwhist_args, verbose,
	"Print counts and relative frequencies"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bwhist_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bwhist_schema = BU_ARG_SCHEMA(
    "bwhist", "Display a BW intensity histogram on a framebuffer",
    bwhist_options, bwhist_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bwhist_grammar = {
    &bwhist_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    long histogram[HISTOGRAM_BINS] = {0};
    unsigned char input[INPUT_BUFFER_SIZE];
    unsigned char white[RGB_CHANNEL_COUNT * FRAMEBUFFER_DIMENSION];
    FILE *input_file = stdin;
    struct bwhist_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bwhist_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bwhist_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bwhist_schema, "bwhist");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bwhist help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	const char *path = argv[operand_index + 1];
	input_file = fopen(path, "rb");
	if (!input_file)
	    bu_exit(1, "bwhist: cannot open '%s' for reading\n", path);
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "bwhist requires an input file or redirected input\n");
    }

    setmode(fileno(input_file), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    size_t count;
    while ((count = fread(input, 1, sizeof(input), input_file)) > 0) {
	for (size_t index = 0; index < count; index++)
	    histogram[input[index]]++;
    }
    if (ferror(input_file))
	bu_exit(1, "bwhist: error reading input\n");
    if (input_file != stdin && fclose(input_file) == EOF)
	bu_exit(1, "bwhist: error closing input\n");

    long maximum = 1;
    for (size_t index = 0; index < HISTOGRAM_BINS; index++) {
	if (histogram[index] > maximum)
	    maximum = histogram[index];
    }
    double scale = (FRAMEBUFFER_DIMENSION - 1) / (double)maximum;

    printf("Full screen = %ld pixels\n", maximum);
    memset(white, 255, sizeof(white));

    struct fb *framebuffer = fb_open(NULL, FRAMEBUFFER_DIMENSION,
	FRAMEBUFFER_DIMENSION);
    if (!framebuffer)
	bu_exit(12, "bwhist: framebuffer open failed\n");

    for (size_t index = 0; index < HISTOGRAM_BINS; index++) {
	int value = (int)(histogram[index] * scale);
	if (value == 0 && histogram[index] != 0)
	    value = 1;
	fb_write(framebuffer, 0, (int)(2 * index), white, value);
	fb_write(framebuffer, 0, (int)(2 * index + 1), white, value);
	if (args.verbose) {
	    printf("%3zu: %10ld (%10f)\n", index, histogram[index],
		(double)histogram[index] / (double)maximum);
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
