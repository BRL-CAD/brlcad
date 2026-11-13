/*                       P I X D I F F . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file util/pixdiff.c
 *
 * Highlight differences between two raw PIX or BW images.  Matching
 * PIX pixels provide dim grayscale context; matching BW samples are
 * written at half intensity.  Differing channels are highlighted.
 *
 * Comparison is independent of image resolution.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "bu/str.h"


#define PIX_CHANNELS 3
/* Buffered reads must end on a PIX pixel boundary. */
#define BUFFER_SIZE (8192 / PIX_CHANNELS * PIX_CHANNELS)
#define CONTEXT_DIVISOR 2
#define OFF_BY_ONE_INTENSITY 0xC0
#define OFF_BY_MANY_INTENSITY 0xFF
#define GRAYSCALE_RED_WEIGHT 22937
#define GRAYSCALE_GREEN_WEIGHT 36044
#define GRAYSCALE_BLUE_WEIGHT 6553
#define GRAYSCALE_SHIFT 17

struct pixdiff_args {
    int help;
    int bw_mode;
};

static const struct bu_arg_option pixdiff_options[] = {
    BU_ARG_FLAG("h", "help", struct pixdiff_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("b", "bw", struct pixdiff_args, bw_mode,
	"Compare one-byte BW samples instead of RGB pixels"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixdiff_operands[] = {
    BU_ARG_OPERAND("file1", BU_ARG_VALUE_FILE, 1, 1,
	"First input image, or - for standard input", NULL),
    BU_ARG_OPERAND("file2", BU_ARG_VALUE_FILE, 1, 1,
	"Second input image, or - for standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixdiff_schema = BU_ARG_SCHEMA(
    "pixdiff", "Highlight differences between two PIX or BW images",
    pixdiff_options, pixdiff_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixdiff_grammar = {
    &pixdiff_schema, NULL, NULL
};


static size_t
read_bytes(FILE *input, unsigned char *buffer, size_t capacity)
{
    size_t count = 0;

    while (count < capacity) {
	size_t n = fread(buffer + count, 1, capacity - count, input);
	if (n == 0) {
	    if (ferror(input))
		bu_exit(1, "pixdiff: input read error\n");
	    break;
	}
	count += n;
    }

    return count;
}


static void
write_byte_diff(int c1, int c2, int matching_intensity, FILE *output,
		size_t *offmany, size_t *off1, size_t *matching)
{
    if (c1 != c2) {
	int difference = abs(c1 - c2);
	if (difference > 1) {
	    fputc(OFF_BY_MANY_INTENSITY, output);
	    (*offmany)++;
	} else {
	    fputc(OFF_BY_ONE_INTENSITY, output);
	    (*off1)++;
	}
    } else {
	fputc(matching_intensity, output);
	(*matching)++;
    }
}


static void
write_pix_match(const unsigned char *pixel, FILE *output)
{
    long intensity = ((GRAYSCALE_RED_WEIGHT * pixel[0] +
		       GRAYSCALE_GREEN_WEIGHT * pixel[1] +
		       GRAYSCALE_BLUE_WEIGHT * pixel[2]) >> GRAYSCALE_SHIFT) / CONTEXT_DIVISOR;
    size_t channel;

    for (channel = 0; channel < PIX_CHANNELS; channel++)
	fputc((int)intensity, output);
}


int
main(int argc, char *argv[])
{
    size_t matching = 0;
    size_t off1 = 0;
    size_t offmany = 0;
    unsigned char buffer1[BUFFER_SIZE];
    unsigned char buffer2[BUFFER_SIZE];
    const char *file1;
    const char *file2;
    struct pixdiff_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    FILE *f1, *f2;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    int introspection = bu_arg_introspect(&msg, &pixdiff_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixdiff_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixdiff_schema, "pixdiff");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixdiff help");
	return 0;
    }

    if (isatty(fileno(stdout)))
	bu_exit(1, "pixdiff requires redirected binary output\n");

    file1 = argv[operand_index + 1];
    file2 = argv[operand_index + 2];

    if (BU_STR_EQUAL(file1, "-") && BU_STR_EQUAL(file2, "-")) {
	bu_exit(1, "pixdiff: standard input cannot supply both images\n");
    }

    if (BU_STR_EQUAL(file1, "-"))
	f1 = stdin;
    else if ((f1 = fopen(file1, "rb")) == NULL) {
	perror(file1);
	return 1;
    }
    if (BU_STR_EQUAL(file2, "-"))
	f2 = stdin;
    else if ((f2 = fopen(file2, "rb")) == NULL) {
	perror(file2);
	if (f1 != stdin)
	    fclose(f1);
	return 1;
    }

    while (1) {
	size_t count1 = read_bytes(f1, buffer1, sizeof(buffer1));
	size_t count2 = read_bytes(f2, buffer2, sizeof(buffer2));
	size_t i;

	if (count1 != count2)
	    bu_exit(1, "pixdiff: input sizes differ\n");
	if (count1 == 0)
	    break;
	if (!args.bw_mode && count1 % PIX_CHANNELS != 0)
	    bu_exit(1, "pixdiff: input contains an incomplete PIX pixel\n");

	if (args.bw_mode) {
	    for (i = 0; i < count1; i++)
		write_byte_diff(buffer1[i], buffer2[i], buffer1[i] / CONTEXT_DIVISOR,
				stdout, &offmany, &off1, &matching);
	} else {
	    for (i = 0; i < count1; i += PIX_CHANNELS) {
		if (buffer1[i] == buffer2[i] &&
		    buffer1[i + 1] == buffer2[i + 1] &&
		    buffer1[i + 2] == buffer2[i + 2]) {
		    write_pix_match(buffer1 + i, stdout);
		    matching += PIX_CHANNELS;
		} else {
		    size_t channel;
		    for (channel = 0; channel < PIX_CHANNELS; channel++)
			write_byte_diff(buffer1[i + channel], buffer2[i + channel], 0,
					stdout, &offmany, &off1, &matching);
		}
	    }
	}
    }

    if (fflush(stdout) == EOF)
	bu_exit(1, "pixdiff: output write error\n");

    if (f1 != stdin)
	fclose(f1);
    if (f2 != stdin)
	fclose(f2);

    fprintf(stderr,
	    "pixdiff bytes: %7zu matching, %7zu off by 1, %7zu off by many\n",
	    matching, off1, offmany);

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
