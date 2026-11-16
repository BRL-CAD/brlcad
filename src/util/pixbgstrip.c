/*                    P I X B G S T R I P . C
 * BRL-CAD
 *
 * Copyright (c) 1991-2026 United States Government as represented by
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
/** @file util/pixbgstrip.c
 *
 * Background Un-Maker
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"

struct pixbgstrip_args {
    struct pixbgstrip_args *self;
    int help;
    int autosize;
    size_t file_width;
    int threshold;
    int background_x_offset;
};

enum pixbgstrip_dimension {
    PIXBGSTRIP_SIZE,
    PIXBGSTRIP_WIDTH,
    PIXBGSTRIP_HEIGHT
};

static struct pixbgstrip_args *
pixbgstrip_args_from_storage(void *storage)
{
    return storage ? *(struct pixbgstrip_args **)storage : NULL;
}

static int
pixbgstrip_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct pixbgstrip_args *args = pixbgstrip_args_from_storage(storage);

    if (args)
	args->autosize = 1;
    return 0;
}

static int
pixbgstrip_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum pixbgstrip_dimension dimension)
{
    struct pixbgstrip_args *args = pixbgstrip_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args) {
	if (dimension != PIXBGSTRIP_HEIGHT)
	    args->file_width = value;
	args->autosize = 0;
    }
    return 0;
}

#define PIXBGSTRIP_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pixbgstrip_dimension(msg, arg, storage, _dimension); \
    }

PIXBGSTRIP_DIMENSION_CALLBACK(pixbgstrip_size, PIXBGSTRIP_SIZE)
PIXBGSTRIP_DIMENSION_CALLBACK(pixbgstrip_width, PIXBGSTRIP_WIDTH)
PIXBGSTRIP_DIMENSION_CALLBACK(pixbgstrip_height, PIXBGSTRIP_HEIGHT)

static const struct bu_arg_option pixbgstrip_options[] = {
    BU_ARG_FLAG("h", "help", struct pixbgstrip_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct pixbgstrip_args,
	self, pixbgstrip_autosize, "Determine dimensions from a named input file"),
    BU_ARG_CUSTOM("s", "size", struct pixbgstrip_args, self,
	pixbgstrip_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pixbgstrip_args, self,
	pixbgstrip_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pixbgstrip_args, self,
	pixbgstrip_height, "pixels", "Accept the input height for compatibility"),
    BU_ARG_INTEGER_RANGE("t", "threshold", struct pixbgstrip_args, threshold,
	1, 0, 1, INT_MAX, "difference", "Set the channel matching threshold"),
    BU_ARG_INTEGER_RANGE("x", "background-x", struct pixbgstrip_args,
	background_x_offset, 1, 0, 1, INT_MAX, "pixel",
	"Set the background sample X offset"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixbgstrip_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixbgstrip_schema = BU_ARG_SCHEMA(
    "pixbgstrip", "Replace a PIX background color with black",
    pixbgstrip_options, pixbgstrip_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixbgstrip_grammar = {
    &pixbgstrip_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    int r, g, b;
    size_t i;
    unsigned char *scanline;
    size_t scanbytes;
    const char *file_name;
    FILE *infp;
    int fileinput = 0;
    struct pixbgstrip_args args = {NULL, 0, 0, 512, 0, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixbgstrip_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixbgstrip_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixbgstrip_schema, "pixbgstrip");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixbgstrip help");
	return 0;
    }

    if (operand_index + 1 < argc) {
	file_name = argv[operand_index + 1];
	infp = fopen(file_name, "rb");
	if (!infp)
	    bu_exit(1, "pixbgstrip: cannot open \"%s\" for reading: %s\n",
		file_name, strerror(errno));
	fileinput = 1;
    } else {
	if (isatty(fileno(stdin)))
	    bu_exit(1, "pixbgstrip: no input file and standard input is a terminal\n");
	file_name = "-";
	infp = stdin;
    }
    if (isatty(fileno(stdout)))
	bu_exit(1, "pixbgstrip requires redirected standard output\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    /* autosize input? */
    if (fileinput && args.autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, file_name, 3)) {
	    args.file_width = w;
	} else {
	    fprintf(stderr, "pixbgstrip: unable to autosize\n");
	}
    }

    if ((size_t)args.background_x_offset >= args.file_width)
	bu_exit(1, "pixbgstrip: background x offset %d is outside width %zu\n",
	    args.background_x_offset, args.file_width);

    scanbytes = args.file_width * sizeof(RGBpixel);
    scanline = (unsigned char *)bu_malloc(scanbytes, "scanline");

    while (!feof(infp)) {
	size_t ret = fread(scanline, 1, scanbytes, infp);
	if (ret != scanbytes)
	    break;
	r = scanline[args.background_x_offset*3+0];
	g = scanline[args.background_x_offset*3+1];
	b = scanline[args.background_x_offset*3+2];
	for (i=0; i<args.file_width; i++) {
	    int diff;

	    diff = scanline[i*3+0] - r;
	    if (diff <= -args.threshold || diff >= args.threshold) continue;

	    diff = scanline[i*3+1] - g;
	    if (diff <= -args.threshold || diff >= args.threshold) continue;

	    diff = scanline[i*3+2] - b;
	    if (diff <= -args.threshold || diff >= args.threshold) continue;

	    /* Input pixel matches background, set to black */
	    scanline[i*3+0] =
		scanline[i*3+1] =
		scanline[i*3+2] = 0;
	}
	if (fwrite(scanline, 1, scanbytes, stdout) != scanbytes) {
	    perror("pixbgstrip: fwrite()");
	    bu_exit (1, NULL);
	}
    }
    bu_free(scanline, "scanline");
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
