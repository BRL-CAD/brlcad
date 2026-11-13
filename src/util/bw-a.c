/*                          B W - A . C
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
/** @file util/bw-a.c
 *
 *
 * Convert a bw file into an ASCII bitmap.
 *
 * The output of bw-a can be fed to the X11 program atobm to generate
 * a bitmap that X11 programs can use.
 *
 * It is assumed that the file is in inverse order.
 *
 * A translation sequence might be:
 *	pix-fb -w1280 -n960 -i -Fimage.pixi image.pix	# flip image.
 *	pix-bw image.pixi | halftone -R -S -w1280 -n960 | \
 *		bw-a -w1280 -n960 | atobm -name image.bm >image.bm
 *
 */

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"


struct bw_a_args {
    struct bw_a_args *self;
    int help;
    int autosize;
    long width;
    long height;
};

enum bw_a_dimension {
    BW_A_WIDTH,
    BW_A_SIZE,
    BW_A_HEIGHT
};

static struct bw_a_args *
bw_a_args_from_storage(void *storage)
{
    return storage ? *(struct bw_a_args **)storage : NULL;
}

static int
bw_a_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct bw_a_args *args = bw_a_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
bw_a_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum bw_a_dimension dimension)
{
    long value;
    struct bw_a_args *args = bw_a_args_from_storage(storage);

    if (!bu_arg_long_range_from_str(msg, &value, arg, 10, 1, LONG_MAX))
	return -1;
    if (args) {
	switch (dimension) {
	    case BW_A_WIDTH:
		args->width = value;
		break;
	    case BW_A_SIZE:
		args->width = value;
		args->height = value;
		break;
	    case BW_A_HEIGHT:
		args->height = value;
		break;
	}
	args->autosize = 0;
    }
    return 0;
}

static int
bw_a_size(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_a_dimension(msg, arg, storage, BW_A_SIZE);
}

static int
bw_a_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_a_dimension(msg, arg, storage, BW_A_WIDTH);
}

static int
bw_a_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_a_dimension(msg, arg, storage, BW_A_HEIGHT);
}

static const struct bu_arg_option bw_a_options[] = {
    BU_ARG_FLAG("h", "help", struct bw_a_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct bw_a_args,
	self, bw_a_autosize, "Derive the width from a named input file"),
    BU_ARG_CUSTOM("s", "size", struct bw_a_args, self, bw_a_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct bw_a_args, self, bw_a_width,
	"pixels", "Set the input width"),
    BU_ARG_CUSTOM("n", "height", struct bw_a_args, self, bw_a_height,
	"scanlines", "Set the nominal input height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bw_a_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bw_a_schema = BU_ARG_SCHEMA(
    "bw-a", "Convert a BRL-CAD BW image to an ASCII bitmap",
    bw_a_options, bw_a_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bw_a_grammar = {
    &bw_a_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    int c;
    long int cur_width = 0;
    const char *file_name = "-";
    FILE *infp = stdin;
    int fileinput = 0;
    struct bw_a_args args = {NULL, 0, 0, 512L, 512L};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bw_a_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bw_a_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bw_a_schema, "bw-a");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bw-a help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp)
	    bu_exit(1, "bw-a: cannot open '%s' for reading\n", file_name);
	fileinput = 1;
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "bw-a requires an input file or redirected input\n");
    }

    setmode(fileno(infp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* autosize the input? */
    if (fileinput && args.autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 1)) {
	    args.width = (long)w;
	} else {
	    fprintf(stderr, "bw-a: unable to autosize\n");
	}
    }

    while ((c=getc(infp)) != EOF) {
	if (c < 127) {
	    putchar('#');
	} else {
	    putchar('-');
	}
	if (++cur_width >= args.width) {
	    putchar('\n');
	    cur_width=0L;
	}
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
