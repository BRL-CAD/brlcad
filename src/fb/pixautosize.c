/*                   P I X A U T O S I Z E . C
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
 *
 */
/** @file pixautosize.c
 *
 * Program to determine if a given file is one of the "standard" sizes
 * as known by the framebuffer library.
 *
 * Used by pixinfo.sh to determine size of .pix and .bw files.
 *
 */

#include "common.h"

#include <limits.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn.h"
#include "dm.h"


struct pixautosize_args {
    int help;
    int bytes_per_sample;
    const char *file_name;
    int file_length;
};

static const struct bu_arg_option pixautosize_options[] = {
    BU_ARG_FLAG("h", "help", struct pixautosize_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_INTEGER_RANGE("b", "bytes-per-sample", struct pixautosize_args,
	bytes_per_sample, 1, 1, 1, INT_MAX, "bytes",
	"Set the number of bytes in each image sample"),
    BU_ARG_VALUE("f", "file", struct pixautosize_args, file_name,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "path",
	"Determine dimensions from a file's length"),
    BU_ARG_INTEGER_RANGE("l", "file-length", struct pixautosize_args,
	file_length, 1, 1, 1, INT_MAX, "bytes",
	"Determine dimensions from an explicit byte count"),
    BU_ARG_OPTION_NULL
};

static const char * const pixautosize_sources[] = {
    "file", "file-length", NULL
};

static const struct bu_arg_constraint pixautosize_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	pixautosize_sources, 1, 1, "select exactly one image-size source"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema pixautosize_schema = BU_ARG_SCHEMA(
    "pixautosize", "Determine standard image dimensions",
    pixautosize_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, pixautosize_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar pixautosize_grammar = {
    &pixautosize_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    int ret = 0;
    int nsamp;
    size_t width = 0;
    size_t height = 0;
    struct pixautosize_args args = {0, 3, NULL, 0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixautosize_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&pixautosize_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixautosize_schema, "pixautosize");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixautosize help");
	return 0;
    }

    if (args.file_name) {
	if (!fb_common_file_size(&width, &height, args.file_name,
		args.bytes_per_sample)) {
	    bu_log("pixautosize: unable to autosize file '%s'\n", args.file_name);
	    ret = 1;		/* ERROR */
	}
    } else {
	nsamp = args.file_length / args.bytes_per_sample;
	if (!fb_common_image_size(&width, &height, nsamp)) {
	    bu_log("pixautosize: unable to autosize nsamples=%d\n", nsamp);
	    ret = 2;		/* ERROR */
	}
    }

    /*
     * Whether or not an error message was printed to stderr above,
     * print out the width and height on stdout.
     * They will be zero on error.
     */
    printf("WIDTH=%lu; HEIGHT=%lu\n", (unsigned long)width, (unsigned long)height);
    return ret;
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
