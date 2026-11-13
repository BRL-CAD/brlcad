/*                        B W - P N G . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2026 United States Government as represented by
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
/** @file util/bw-png.c
 *
 * Convert bw file to PNG (Portable Network Graphics) format
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include "png.h"

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"


#define BYTESPERPIXEL 1

struct bw_png_args {
    struct bw_png_args *self;
    int help;
    int autosize;
    long width;
    long height;
};

enum bw_png_dimension {
    BW_PNG_WIDTH = -1,
    BW_PNG_SIZE,
    BW_PNG_HEIGHT
};

static struct bw_png_args *
bw_png_args_from_storage(void *storage)
{
    return storage ? *(struct bw_png_args **)storage : NULL;
}

static int
bw_png_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct bw_png_args *args = bw_png_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
bw_png_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum bw_png_dimension dimension)
{
    long value;
    struct bw_png_args *args = bw_png_args_from_storage(storage);

    if (!bu_arg_long_range_from_str(msg, &value, arg, 10, 1, LONG_MAX))
	return -1;
    if (args) {
	switch (dimension) {
	    case BW_PNG_WIDTH:
		args->width = value;
		break;
	    case BW_PNG_SIZE:
		args->width = value;
		args->height = value;
		break;
	    case BW_PNG_HEIGHT:
		args->height = value;
		break;
	}
	args->autosize = 0;
    }
    return 0;
}

static int
bw_png_size(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_png_dimension(msg, arg, storage, BW_PNG_SIZE);
}

static int
bw_png_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_png_dimension(msg, arg, storage, BW_PNG_WIDTH);
}

static int
bw_png_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return bw_png_dimension(msg, arg, storage, BW_PNG_HEIGHT);
}

static const struct bu_arg_option bw_png_options[] = {
    BU_ARG_FLAG("h", "help", struct bw_png_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct bw_png_args,
	self, bw_png_autosize, "Derive dimensions from a named input file"),
    BU_ARG_CUSTOM("s", "size", struct bw_png_args, self, bw_png_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct bw_png_args, self, bw_png_width,
	"pixels", "Set the input width"),
    BU_ARG_CUSTOM("n", "height", struct bw_png_args, self, bw_png_height,
	"scanlines", "Set the input height"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand bw_png_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"BW input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema bw_png_schema = BU_ARG_SCHEMA(
    "bw-png", "Convert a BRL-CAD BW image to PNG",
    bw_png_options, bw_png_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar bw_png_grammar = {
    &bw_png_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    int i;
    unsigned char *scanbuf;
    unsigned char **rows;
    png_structp png_p;
    png_infop info_p;
    FILE *outfp = stdout;
    size_t row_size;
    size_t image_size;
    const char *file_name = "-";
    FILE *infp = stdin;
    int fileinput = 0;
    struct bw_png_args args = {NULL, 0, 0, 512, 512};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &bw_png_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&bw_png_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&bw_png_schema, "bw-png");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "bw-png help");
	return 0;
    }

    if (argc - operand_index - 1 == 1) {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp)
	    bu_exit(1, "bw-png: cannot open '%s' for reading\n", file_name);
	fileinput = 1;
    } else if (isatty(fileno(stdin))) {
	bu_exit(1, "bw-png requires an input file or redirected input\n");
    }

    /* autosize input? */
    if (fileinput && args.autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 1)) {
	    args.width = (long)w;
	    args.height = (long)h;
	} else {
	    fprintf(stderr, "bw-png: unable to autosize\n");
	}
    }

    if (args.width > (long)PNG_UINT_31_MAX ||
	args.height > (long)PNG_UINT_31_MAX)
	bu_exit(EXIT_FAILURE, "bw-png: image dimensions exceed PNG limits\n");
    row_size = (size_t)args.width * BYTESPERPIXEL;
    if ((size_t)args.height > SIZE_MAX / row_size)
	bu_exit(EXIT_FAILURE, "bw-png: image dimensions are too large\n");
    image_size = (size_t)args.height * row_size;

    setmode(fileno(outfp), O_BINARY);

    if (isatty(fileno(outfp))) {
	fprintf(stderr, "cv: trying to send binary output to terminal\n");
	return 5;
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p)
	bu_exit(EXIT_FAILURE, "Could not create PNG write structure\n");

    info_p = png_create_info_struct(png_p);
    if (!info_p)
	bu_exit(EXIT_FAILURE, "Could not create PNG info structure\n");

    /* allocate space for the image */
    scanbuf = (unsigned char *)bu_calloc(image_size, sizeof(unsigned char), "scanbuf");

    /* create array of pointers to rows for libpng */
    rows = (unsigned char **)bu_calloc(args.height, sizeof(unsigned char *), "rows");
    for (i=0; i<args.height; i++)
	rows[i] = scanbuf + ((size_t)(args.height-i-1) * row_size);

    /* read the bw file */
    if (fread(scanbuf, image_size, 1, infp) != 1)
	bu_exit(EXIT_FAILURE, "bw-png: Short read\n");

    png_init_io(png_p, outfp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, args.width, args.height, 8,
		 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    if (outfp != stdout)
	fclose(outfp);

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
