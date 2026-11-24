/*                    D O U B L E - A S C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2026 United States Government as represented by
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
/** @file util/double-asc.c
 *
 * Take a stream of IEEE doubles and make them ASCII
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bio.h"

#include "vmath.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/num.h"
#include "bu/str.h"
#include "bu/cv.h"
#include "bu/snooze.h"
#include "bn.h"
#include "dm.h"


#define DOUBLE_ASC_BUFFER_BYTES ((size_t)1 << 16)

static const char *file_name;
static char *format = 0;
static int infd;

static int fileinput = 0;		/* Input from file (vs. pipe)? */
static int autosize = 0;		/* Try to autosize input? */

static long int file_width = 512L;	/* default input width */
static long int file_height = 512L;	/* default input height */

static int make_cells = 0;		/* Insert cell coords in output? */
static int d_per_l = 1;		/* doubles per line of output */

struct double_asc_args {
    struct double_asc_args *self;
    int help;
    int autosize;
    int make_cells;
    long width;
    long height;
    int depth;
    const char *format;
};

enum double_asc_dimension {
    DOUBLE_ASC_WIDTH = -1,
    DOUBLE_ASC_SIZE,
    DOUBLE_ASC_HEIGHT
};

static struct double_asc_args *
double_asc_args_from_storage(void *storage)
{
    return storage ? *(struct double_asc_args **)storage : NULL;
}

static int
double_asc_autosize(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    struct double_asc_args *args = double_asc_args_from_storage(storage);
    if (args)
	args->autosize = 1;
    return 0;
}

static int
double_asc_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum double_asc_dimension dimension)
{
    long value;
    struct double_asc_args *args = double_asc_args_from_storage(storage);

    if (!bu_arg_long_range_from_str(msg, &value, arg, 10, 1, LONG_MAX))
	return -1;
    if (args) {
	switch (dimension) {
	    case DOUBLE_ASC_WIDTH:
		args->width = value;
		break;
	    case DOUBLE_ASC_SIZE:
		args->width = value;
		args->height = value;
		break;
	    case DOUBLE_ASC_HEIGHT:
		args->height = value;
		break;
	}
	args->autosize = 0;
    }
    return 0;
}

static int
double_asc_size(struct bu_vls *msg, const char *arg, void *storage)
{
    return double_asc_dimension(msg, arg, storage, DOUBLE_ASC_SIZE);
}

static int
double_asc_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return double_asc_dimension(msg, arg, storage, DOUBLE_ASC_WIDTH);
}

static int
double_asc_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return double_asc_dimension(msg, arg, storage, DOUBLE_ASC_HEIGHT);
}

static const struct bu_arg_option double_asc_options[] = {
    BU_ARG_FLAG("h", "help", struct double_asc_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("a", "autosize", "autosize", struct double_asc_args,
	self, double_asc_autosize, "Derive dimensions from a named input file"),
    BU_ARG_FLAG("c", "cells", struct double_asc_args, make_cells,
	"Prepend column and row coordinates to each output line"),
    BU_ARG_VALUE("f", "format", struct double_asc_args, format,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "format", "Set the output number format"),
    BU_ARG_CUSTOM("s", "size", struct double_asc_args, self, double_asc_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct double_asc_args, self, double_asc_width,
	"pixels", "Set the input width"),
    BU_ARG_CUSTOM("n", "height", struct double_asc_args, self, double_asc_height,
	"scanlines", "Set the input height"),
    BU_ARG_INTEGER_RANGE("#", "depth", struct double_asc_args, depth,
	1, 1, 1, INT_MAX / SIZEOF_NETWORK_DOUBLE, "count",
	"Set the doubles per output line"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand double_asc_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Network-order double input file (default: standard input)", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema double_asc_schema = BU_ARG_SCHEMA(
    "double-asc", "Convert network-order IEEE doubles to ASCII text",
    double_asc_options, double_asc_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar double_asc_grammar = {
    &double_asc_schema, NULL, NULL
};


int
get_args(int argc, char **argv)
{
    struct double_asc_args args = {NULL, 0, 0, 0, 512L, 512L, 1, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;
    int operand_index;
    char *ifname;
    args.self = &args;

    introspection = bu_arg_introspect(&msg, &double_asc_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : -1;
    }
    operand_index = bu_arg_schema_parse_complete(&double_asc_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_log("%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return -1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&double_asc_schema, "double-asc");
	if (help) {
	    bu_flog(stdout, "%s", help);
	    bu_free(help, "double-asc help");
	}
	return 0;
    }
    autosize = args.autosize;
    make_cells = args.make_cells;
    file_width = args.width;
    file_height = args.height;
    d_per_l = args.depth;
    format = bu_strdup(args.format ? args.format : " %g");
    if (operand_index == argc - 1) {
	file_name = "stdin";
	infd = fileno(stdin);
	setmode(fileno(stdin), O_BINARY);
	return 1;
    }
    file_name = argv[operand_index + 1];
    ifname = bu_file_realpath(file_name, NULL);
    infd = open(ifname, O_RDONLY|O_BINARY);
    bu_free(ifname, "resolved double input path");
    if (infd == -1) {
	bu_log("double-asc: cannot open '%s'\n", file_name);
	return -1;
    }
    fileinput = 1;

    return 1;
}


int
main (int argc, char **argv)
{
    unsigned char *buffer;
    unsigned char *bp;
    double *value;
    size_t sample_size;
    size_t lines_per_buffer;
    size_t buffer_size;
    int l_per_b;	/* buffer size (in output lines) */
    int line_nm;	/* number of current line */
    int num;		/* number of bytes read */
    int row, col;	/* coords within input stream */
    int arg_status;

    bu_setprogname(argv[0]);

    arg_status = get_args(argc, argv);
    if (arg_status <= 0) {
	if (format)
	    bu_free(format, "double-asc format");
	return arg_status < 0 ? 1 : 0;
    }

    bu_log("DEPRECATION WARNING:  This command is scheduled for removal.  Please contact the developers if you use this command.\n\n");
    bu_snooze(BU_SEC2USEC(1));

    /* autosize input? */
    if (fileinput && autosize) {
	size_t w, h;

	if (fb_common_file_size(&w, &h, file_name,
		(size_t)d_per_l * SIZEOF_NETWORK_DOUBLE)) {
	    file_width = (long)w;
	    file_height = (long)h;
	} else
	    bu_log("double-asc: unable to autosize\n");
    }
    bu_log("OK, file is %ld wide and %ld high\n", file_width, file_height);

    /*
     * Choose an input-buffer size as close as possible to 64 kbytes,
     * while still an integral multiple of the size of an output line.
     */
    sample_size = (size_t)d_per_l * SIZEOF_NETWORK_DOUBLE;
    lines_per_buffer = DOUBLE_ASC_BUFFER_BYTES / sample_size;
    if (lines_per_buffer == 0)
	lines_per_buffer = 1;
    buffer_size = lines_per_buffer * sample_size;

    buffer = (unsigned char *)bu_malloc(buffer_size, "double input buffer");
    value = (double *)bu_malloc((size_t)d_per_l * sizeof(double),
	"converted doubles");
    col = row = 0;
    while ((num = read(infd, buffer, (unsigned int)buffer_size)) > 0) {
	bp = buffer;
	l_per_b = num / (int)sample_size;
	for (line_nm = 0; line_nm < l_per_b; ++line_nm) {
	    if (make_cells)
		printf("%d %d", col, row);
	    bu_cv_ntohd((unsigned char *)value, bp, d_per_l);
	    bp += sample_size;
	    bu_num_print(value, (size_t)d_per_l, (size_t)d_per_l,
			 NULL, NULL, format, "", NULL, "\n");
	    if (++col % file_width == 0) {
		col = 0;
		++row;
	    }
	}
    }
    if (num < 0) {
	perror("double-asc");
	bu_free(value, "converted doubles");
	bu_free(buffer, "double input buffer");
	bu_free(format, "double-asc format");
	return 1;
    }
    bu_free(value, "converted doubles");
    bu_free(buffer, "double input buffer");
    bu_free(format, "double-asc format");
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
