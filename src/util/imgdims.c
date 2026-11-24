/*                       I M G D I M S . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2026 United States Government as represented by
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
/** @file util/imgdims.c
 *
 * Guess the dimensions of an image
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "vmath.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn.h"
#include "dm.h"


#define NONE -1
#define BELIEVE_NAME 0
#define BELIEVE_STAT 1
#define DFLT_PIXEL_SIZE 3

struct imgdims_args {
    int help;
    int how;
    int bytes_per_pixel;
};

static int
imgdims_believe_name(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage)
	*(int *)storage = BELIEVE_NAME;
    return 0;
}

static int
imgdims_believe_stat(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage)
	*(int *)storage = BELIEVE_STAT;
    return 0;
}

static const struct bu_arg_option imgdims_options[] = {
    BU_ARG_FLAG("h", "help", struct imgdims_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("n", "believe-name", "believe-name",
	struct imgdims_args, how, imgdims_believe_name,
	"Prefer dimensions encoded in the file name"),
    BU_ARG_CUSTOM_FLAG("s", "believe-stat", "believe-stat",
	struct imgdims_args, how, imgdims_believe_stat,
	"Use the file byte size"),
    BU_ARG_INTEGER_RANGE("#", "bytes-per-pixel", struct imgdims_args,
	bytes_per_pixel, 1, 1, 1, INT_MAX, "bytes",
	"Set the number of bytes per pixel"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand imgdims_operands[] = {
    BU_ARG_OPERAND("file-or-byte-count", BU_ARG_VALUE_STRING, 1, 1,
	"Image file name or positive byte count", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema imgdims_schema = BU_ARG_SCHEMA(
    "imgdims", "Guess the dimensions of an image",
    imgdims_options, imgdims_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar imgdims_grammar = {
    &imgdims_schema, NULL, NULL
};

static int
parse_positive_int_arg(const char *buf, int *np, const char *label)
{
    char *end = NULL;
    long int value;

    errno = 0;
    value = strtol(buf, &end, 10);
    if (errno != 0 || end == buf || *end != '\0' || value <= 0 || value > INT_MAX) {
	bu_log("imgdims: invalid %s '%s'\n", label, buf);
	return 0;
    }

    *np = (int)value;
    return 1;
}


static int grab_number (const char *buf, int *np)
{
    const char *bp;

    for (bp = buf; *bp != '\0'; ++bp)
	if (!isdigit((int)*bp))
	    return 0;

    return parse_positive_int_arg(buf, np, "byte count");
}


static int pixel_size (const char *buf)
{
    char *ep;
    struct assoc {
	const char *ext;
	int size;
    }			*ap;
    static struct assoc a_tbl[] = {
	{"bw", 1},
	{"pix", 3},
	{0, 0}
    };

    if ((ep = strrchr(buf, '.')) == NULL)
	return DFLT_PIXEL_SIZE;

    ++ep;

    for (ap = a_tbl; ap->ext; ++ap)
	if (BU_STR_EQUAL(ep, ap->ext))
	    return ap->size;

    return DFLT_PIXEL_SIZE;
}


int
main (int argc, char **argv)
{
    char *argument;		/* file name or size */
    int nm_bytes = -1;
    int nm_pixels = 0;
    size_t width;
    size_t height;
    struct stat stat_buf;
    struct imgdims_args args = {0, NONE, -1};
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &imgdims_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&imgdims_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&imgdims_schema, "imgdims");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "imgdims help");
	return 0;
    }

    argument = bu_file_realpath(argv[operand_index + 1], NULL);
    if (args.how != NONE && !bu_file_exists(argument, NULL)) {
	bu_log("image file [%s] does not exist\n", argument);
	bu_free(argument, "argument realpath");
	bu_exit(1, NULL);
    }

    if ((stat(argument, &stat_buf) != 0)
	&& (!grab_number(argv[operand_index + 1], &nm_bytes)))
    {
	bu_log("Cannot find file '%s'\n", argument);
	bu_free(argument, "argument realpath");
	return 1;
    }

    /*
     * If the user specified a file, determine its size.
     */
    if (nm_bytes == -1) {
	if ((args.how == BELIEVE_NAME)
	    && fb_common_name_size(&width, &height, argument))
	    goto done;

	nm_bytes = (int)stat_buf.st_size;
	if (args.bytes_per_pixel == -1)
	    args.bytes_per_pixel = pixel_size(argument);

    }

    bu_free(argument, "argument realpath");

    if (args.bytes_per_pixel == -1)
	args.bytes_per_pixel = DFLT_PIXEL_SIZE;

    if (nm_bytes % args.bytes_per_pixel != 0)
	bu_exit (1, "Image size (%d bytes) is not a multiple of pixel size (%d bytes)\n", nm_bytes, args.bytes_per_pixel);

    nm_pixels = nm_bytes / args.bytes_per_pixel;

    if (!fb_common_image_size(&width, &height, nm_pixels))
	bu_exit (0, NULL);

done:
    bu_flog(stdout, "%zu %zu\n", width, height);
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
