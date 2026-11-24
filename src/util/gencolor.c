/*                      G E N C O L O R . C
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
/** @file util/gencolor.c
 *
 * Output a pattern of bytes either forever or the requested
 * number of times.  It gets the byte values either from the
 * command line, or stdin.  Defaults to black.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"
#include "bu/app.h"
#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#define MAX_BYTES (128*1024)

int32_t count = -1;
int outputtype = 1; /* 1 for pix, 2 for bw */
int width = 512;
int height = 512;
int typeselected = 0 ; /* set to 1 if any option other than -r appears */
int setrcount = 0; /* set to 1 if -r is detected */

unsigned char buf[MAX_BYTES];

struct gencolor_args {
    struct gencolor_args *self;
    int help;
};

enum gencolor_dimension {
    GENCOLOR_SIZE,
    GENCOLOR_WIDTH,
    GENCOLOR_HEIGHT
};

static struct gencolor_args *
gencolor_args_from_storage(void *storage)
{
    return storage ? *(struct gencolor_args **)storage : NULL;
}

static int
gencolor_repeat(struct bu_vls *msg, const char *arg, void *UNUSED(storage))
{
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 0, INT32_MAX))
	return -1;
    count = (int32_t)value;
    setrcount = 1;
    return 0;
}

static int
gencolor_format(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage, int format)
{
    if (gencolor_args_from_storage(storage)) {
	outputtype = format;
	typeselected = 1;
    }
    return 0;
}

static int
gencolor_pix(struct bu_vls *msg, const char *arg, void *storage)
{
    return gencolor_format(msg, arg, storage, 1);
}

static int
gencolor_bw(struct bu_vls *msg, const char *arg, void *storage)
{
    return gencolor_format(msg, arg, storage, 2);
}

static int
gencolor_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum gencolor_dimension dimension)
{
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (gencolor_args_from_storage(storage)) {
	if (dimension != GENCOLOR_HEIGHT)
	    width = value;
	if (dimension != GENCOLOR_WIDTH)
	    height = value;
	typeselected = 1;
    }
    return 0;
}

#define GENCOLOR_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return gencolor_dimension(msg, arg, storage, _dimension); \
    }

GENCOLOR_DIMENSION_CALLBACK(gencolor_size, GENCOLOR_SIZE)
GENCOLOR_DIMENSION_CALLBACK(gencolor_width, GENCOLOR_WIDTH)
GENCOLOR_DIMENSION_CALLBACK(gencolor_height, GENCOLOR_HEIGHT)

static const struct bu_arg_option gencolor_options[] = {
    BU_ARG_FLAG("h", "help", struct gencolor_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("r", "repeat", struct gencolor_args, self,
	gencolor_repeat, "count", "Repeat the pattern a fixed number of times"),
    BU_ARG_CUSTOM_FLAG("p", "pix", "pix", struct gencolor_args, self,
	gencolor_pix, "Generate three-byte PIX pixels"),
    BU_ARG_CUSTOM_FLAG("b", "bw", "bw", struct gencolor_args, self,
	gencolor_bw, "Generate one-byte BW pixels"),
    BU_ARG_CUSTOM("s", "size", struct gencolor_args, self,
	gencolor_size, "pixels", "Set both image dimensions"),
    BU_ARG_ALIAS("S", NULL, "size", 1),
    BU_ARG_CUSTOM("w", "width", struct gencolor_args, self,
	gencolor_width, "pixels", "Set the image width"),
    BU_ARG_ALIAS("W", NULL, "width", 1),
    BU_ARG_CUSTOM("n", "height", struct gencolor_args, self,
	gencolor_height, "pixels", "Set the image height"),
    BU_ARG_ALIAS("N", NULL, "height", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand gencolor_operands[] = {
    BU_ARG_OPERAND_INTEGER_RANGE("byte", 0, MAX_BYTES - 1,
	1, 0, 1, UCHAR_MAX, "Pattern byte values", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema gencolor_schema = BU_ARG_SCHEMA(
    "gencolor", "Generate a repeated byte pattern",
    gencolor_options, gencolor_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar gencolor_grammar = {
    &gencolor_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    int i, len, times, bytes_in_buf, copies_per_buf;
    int remainder = 0;
    unsigned char *bp;
    struct gencolor_args args = {NULL, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    int introspection = bu_arg_introspect(&msg, &gencolor_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&gencolor_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&gencolor_schema, "gencolor");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "gencolor help");
	return 0;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "gencolor requires redirected standard output\n");
    if (argc == 1) {
	char *help = bu_arg_schema_help(&gencolor_schema, "gencolor");
	bu_flog(stderr, "%sWriting pattern to redirected standard output\n",
	    help ? help : "");
	bu_free(help, "gencolor implicit help");
    }

    int operand_count = argc - operand_index - 1;

    if (operand_count > 0) {
	/* get values from the command line */
	i = 0;
	while (i < operand_count) {
	    if (!bu_arg_unsigned_char_from_str(NULL, &buf[i],
		    argv[operand_index + i + 1], 10))
		bu_exit(1, "gencolor: internal operand validation failure\n");
	    i++;
	}
	len = i;
    } else if (!isatty(fileno(stdin))) {
	/* get values from stdin */
	len = fread((char *)buf, 1, MAX_BYTES, stdin);
	if (len <= 0)
	    bu_exit(2, "gencolor: no pattern bytes received on standard input\n");
    } else {
	/* assume black */
	buf[0] = 0;
	len = 1;
    }

/* If -r was used, ignore all other "-" options in favor of what -r provided;
 * if there were no "-" options at all, we have no arguments (other than the
 * color values), and we go to the infinite loop which IS documented.
 */
    if (!setrcount && typeselected) {
	count = width * height;
	if ( outputtype == 1 ) count = count * 3;
	remainder = count % len;
	count = count/len; /* e.g., len is 3 for RGB for a pix file */
    }

finishup:

    /*
     * Replicate the pattern as many times as it will fit
     * in the buffer.
     */
    copies_per_buf = 1;
    bytes_in_buf = len;
    bp = &buf[len];
    while ((MAX_BYTES - bytes_in_buf) >= len) {
	for (i = 0; i < len; i++)
	    *bp++ = buf[i];
	copies_per_buf++;
	bytes_in_buf += len;
    }

    if (count <= 0) {
	/* output forever */
	while (1) {
	    if (write(1, (char *)buf, bytes_in_buf) != bytes_in_buf) {
		perror("write");
		break;
	    }
	}
	return 1;
    }

    while (count > 0) {
	times = copies_per_buf > count ? count : copies_per_buf;
	if (write(1, (char *)buf, len * times) != len * times) {
	    perror("write");
	    return 1;
	}
	count -= times;
    }

    if (remainder > 0) {
	count = 1;
	len = remainder;
	remainder = 0;
	goto finishup;
    }

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
