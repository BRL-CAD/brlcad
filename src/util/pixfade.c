/*                       P I X F A D E . C
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
/** @file util/pixfade.c
 *
 * Fade a picture
 *
 * pixfade will darken a pix by a certain percentage or do an integer
 * max pixel value.  It runs in two modes, truncate which will cut any
 * channel greater than param to param, and scale which will change
 * a channel to param percent of its original value (limited by 0-255)
 *
 * Inputs:
 *	-m	integer max value
 *	-f	fraction to fade
 *	-p	percentage of fade (fraction = percentage/100)
 *      -s      squaresize of input pixfile
 *      -w      width of input pixfile
 *      -n      height of input pixfile
 *	file	a picture file (if not given, use STDIN)
 *
 * Output:
 *	STDOUT	the faded picture.
 *
 * Calls:
 *	get_args
 *
 * Method:
 *	straight-forward.
 *
 */
#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"

#include "icv.h"

#define PIXFADE_PERCENT_MAX 100.0
#define PIXFADE_FRACTION_MAX 1.0
#define PIXFADE_CHANNEL_MAX 255
#define PIXFADE_DEFAULT_SIZE 512

struct pixfade_args {
    struct pixfade_args *self;
    int help;
    double multiplier;
    int maximum;
    int width;
    int height;
    const char *output;
};

static struct pixfade_args *
pixfade_args_from_storage(void *storage)
{
    return storage ? *(struct pixfade_args **)storage : NULL;
}

static int
pixfade_multiplier(struct bu_vls *msg, const char *arg, void *storage,
	int percentage)
{
    struct pixfade_args *args = pixfade_args_from_storage(storage);
    double maximum = percentage ? PIXFADE_PERCENT_MAX : PIXFADE_FRACTION_MAX;
    double value;

    if (!bu_arg_double_range_from_str(msg, &value, arg, 0.0, maximum))
	return -1;
    if (args)
	args->multiplier = percentage ? value / PIXFADE_PERCENT_MAX : value;
    return 0;
}

static int
pixfade_percentage(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixfade_multiplier(msg, arg, storage, 1);
}

static int
pixfade_fraction(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixfade_multiplier(msg, arg, storage, 0);
}

static int
pixfade_maximum(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixfade_args *args = pixfade_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_from_str(msg, &value, arg, 10))
	return -1;
    if (args) {
	if (value < 0)
	    value = 0;
	else if (value > PIXFADE_CHANNEL_MAX)
	    value = PIXFADE_CHANNEL_MAX;
	args->maximum = value;
    }
    return 0;
}

static int
pixfade_size(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pixfade_args *args = pixfade_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static int
pixfade_dimension(struct bu_vls *msg, const char *arg, void *storage,
	int set_width)
{
    struct pixfade_args *args = pixfade_args_from_storage(storage);
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (set_width)
	    args->width = value;
	else
	    args->height = value;
    }
    return 0;
}

static int
pixfade_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixfade_dimension(msg, arg, storage, 1);
}

static int
pixfade_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return pixfade_dimension(msg, arg, storage, 0);
}

static const struct bu_arg_option pixfade_options[] = {
    BU_ARG_FLAG("h", "help", struct pixfade_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("p", "percentage", struct pixfade_args, self,
	pixfade_percentage, "percentage", "Scale channels by a percentage"),
    BU_ARG_CUSTOM("f", "fraction", struct pixfade_args, self,
	pixfade_fraction, "fraction", "Scale channels by a fraction"),
    BU_ARG_CUSTOM("m", "maximum", struct pixfade_args, self,
	pixfade_maximum, "level", "Clamp channels to a maximum level"),
    BU_ARG_CUSTOM("s", "size", struct pixfade_args, self, pixfade_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pixfade_args, self, pixfade_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pixfade_args, self, pixfade_height,
	"pixels", "Set the input image height"),
    BU_ARG_VALUE("o", "output", struct pixfade_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.pix",
	"Write PIX output to a file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pixfade_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pixfade_schema = BU_ARG_SCHEMA(
    "pixfade", "Fade or clamp a PIX image", pixfade_options,
    pixfade_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pixfade_grammar = {
    &pixfade_schema, NULL, NULL
};

static int
pixfade_ceiling(icv_image_t *img, int ceiling)
{
    double normalized_ceiling = (double)ceiling / PIXFADE_CHANNEL_MAX;
    size_t size;
    double *data;

    ICV_IMAGE_VAL_INT(img);

    size = img->height * img->width * img->channels;

    if (size == 0)
	return -1;

    data = img->data;

    while (size--) {
	if (*data > normalized_ceiling)
	    *data = normalized_ceiling;
	data++;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    struct pixfade_args args = {NULL, 0, 0.5, -1, PIXFADE_DEFAULT_SIZE,
	PIXFADE_DEFAULT_SIZE, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *input = NULL;
    icv_image_t *img = NULL;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pixfade_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pixfade_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pixfade_schema, "pixfade");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pixfade help");
	return 0;
    }

    if (operand_index < argc - 1)
	input = argv[operand_index + 1];
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "pixfade: no input file and standard input is a terminal\n");
    if (!args.output && isatty(fileno(stdout)))
	bu_exit(1, "pixfade cannot send binary output to a terminal\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    img = icv_read(input, BU_MIME_IMAGE_PIX, args.width, args.height);
    if (img == NULL)
	return 1;
    if (args.maximum < 0)
	icv_fade(img, args.multiplier);
    else
	pixfade_ceiling(img, args.maximum);
    icv_write(img, args.output, BU_MIME_IMAGE_PIX);
    if (!isatty(fileno(stdout)) && args.output) {
	icv_write(img, NULL, BU_MIME_IMAGE_PIX);
    }

    icv_destroy(img);
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
