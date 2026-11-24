/*                        P I X - B W . C
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
/** @file util/pix-bw.c
 *
 * Converts a RGB pix file into an 8-bit BW file.
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/exit.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/str.h"
#include "icv.h"

struct pix_bw_args {
    struct pix_bw_args *self;
    int help;
    int red;
    int green;
    int blue;
    double red_weight;
    double green_weight;
    double blue_weight;
    size_t width;
    size_t height;
    const char *output;
};

enum pix_bw_channel {
    PIX_BW_RED,
    PIX_BW_GREEN,
    PIX_BW_BLUE
};

static struct pix_bw_args *
pix_bw_args_from_storage(void *storage)
{
    return storage ? *(struct pix_bw_args **)storage : NULL;
}

static int
pix_bw_scheme(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pix_bw_args *args = pix_bw_args_from_storage(storage);
    double red_weight;
    double green_weight;
    double blue_weight;

    if (BU_STR_EQUAL(arg, "avg")) {
	red_weight = green_weight = blue_weight = 0.0;
    } else if (BU_STR_EQUAL(arg, "ntsc")) {
	red_weight = 0.299;
	green_weight = 0.587;
	blue_weight = 0.114;
    } else if (BU_STR_EQUAL(arg, "crt")) {
	red_weight = 0.26;
	green_weight = 0.66;
	blue_weight = 0.08;
    } else if (BU_STR_EQUAL(arg, "hdtv")) {
	red_weight = 0.2126;
	green_weight = 0.7152;
	blue_weight = 0.0722;
    } else if (BU_STR_EQUAL(arg, "hdr")) {
	red_weight = 0.2627;
	green_weight = 0.6780;
	blue_weight = 0.0593;
    } else {
	if (msg)
	    bu_vls_printf(msg, "invalid luminance scheme: %s\n", arg);
	return -1;
    }
    if (args) {
	args->red_weight = red_weight;
	args->green_weight = green_weight;
	args->blue_weight = blue_weight;
	args->red = args->green = args->blue = 1;
    }
    return 0;
}

static int
pix_bw_weight(struct bu_vls *msg, const char *arg, void *storage,
	enum pix_bw_channel channel)
{
    struct pix_bw_args *args = pix_bw_args_from_storage(storage);
    double value;

    if (!bu_arg_double_from_str(msg, &value, arg))
	return -1;
    if (args) {
	if (channel == PIX_BW_RED) {
	    args->red = 1;
	    args->red_weight = value;
	} else if (channel == PIX_BW_GREEN) {
	    args->green = 1;
	    args->green_weight = value;
	} else {
	    args->blue = 1;
	    args->blue_weight = value;
	}
    }
    return 0;
}

#define PIX_BW_WEIGHT_CALLBACK(_name, _channel) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return pix_bw_weight(msg, arg, storage, _channel); \
    }

PIX_BW_WEIGHT_CALLBACK(pix_bw_red_weight, PIX_BW_RED)
PIX_BW_WEIGHT_CALLBACK(pix_bw_green_weight, PIX_BW_GREEN)
PIX_BW_WEIGHT_CALLBACK(pix_bw_blue_weight, PIX_BW_BLUE)

static int
pix_bw_size(struct bu_vls *msg, const char *arg, void *storage)
{
    struct pix_bw_args *args = pix_bw_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
	return -1;
    if (args)
	args->width = args->height = value;
    return 0;
}

static int
pix_bw_dimension(struct bu_vls *msg, const char *arg, void *storage,
	int set_width)
{
    struct pix_bw_args *args = pix_bw_args_from_storage(storage);
    size_t value;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, SIZE_MAX))
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
pix_bw_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return pix_bw_dimension(msg, arg, storage, 1);
}

static int
pix_bw_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return pix_bw_dimension(msg, arg, storage, 0);
}

static const struct bu_arg_value_keyword pix_bw_scheme_values[] = {
    {"ntsc", NULL, "ITU-R BT.601-7 NTSC luminance"},
    {"crt", NULL, "Historic CRT luminance"},
    {"hdtv", NULL, "ITU-R BT.709 luminance"},
    {"hdr", NULL, "ITU-R BT.2020 luminance"},
    {"avg", NULL, "Equal channel weights"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option pix_bw_options[] = {
    BU_ARG_FLAG("h", "help", struct pix_bw_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    {"e", "encoding", "encoding", "scheme",
	"Select a luminance weighting scheme", BU_ARG_VALUE_CUSTOM,
	BU_ARG_STORAGE_OFFSET(struct pix_bw_args, self), pix_bw_scheme,
	NULL, NULL, NULL, 0, 0, NULL, BU_ARG_REQUIRED, NULL, NULL,
	pix_bw_scheme_values, BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_CUSTOM("R", "red-weight", struct pix_bw_args, self,
	pix_bw_red_weight, "weight", "Select and weight the red channel"),
    BU_ARG_CUSTOM("G", "green-weight", struct pix_bw_args, self,
	pix_bw_green_weight, "weight", "Select and weight the green channel"),
    BU_ARG_CUSTOM("B", "blue-weight", struct pix_bw_args, self,
	pix_bw_blue_weight, "weight", "Select and weight the blue channel"),
    BU_ARG_CUSTOM("s", "size", struct pix_bw_args, self, pix_bw_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct pix_bw_args, self, pix_bw_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct pix_bw_args, self, pix_bw_height,
	"pixels", "Set the input image height"),
    BU_ARG_VALUE("o", "output", struct pix_bw_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file.bw", "Write BW output to a file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand pix_bw_operands[] = {
    BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	"Input PIX file; omit to read standard input", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema pix_bw_schema = BU_ARG_SCHEMA(
    "pix-bw", "Convert a PIX image to BW",
    pix_bw_options, pix_bw_operands, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar pix_bw_grammar = {
    &pix_bw_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    icv_image_t *img;
    ICV_COLOR color;
    const char *input = NULL;
    struct pix_bw_args args = {NULL, 0, 0, 0, 0, 0.0, 0.0, 0.0,
	0, 0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    args.self = &args;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &pix_bw_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&pix_bw_schema, &args,
	&msg, argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&pix_bw_schema, "pix-bw");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "pix-bw help");
	return 0;
    }
    if (operand_index + 1 < argc)
	input = argv[operand_index + 1];
    if (input && !bu_file_exists(input, NULL))
	bu_exit(1, "pix-bw: input file does not exist: %s\n", input);
    if (!input && isatty(fileno(stdin)))
	bu_exit(1, "pix-bw cannot receive input from a terminal\n");
    if (!args.output && isatty(fileno(stdout)))
	bu_exit(1, "pix-bw cannot send binary output to a terminal\n");

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    img = icv_read(input, BU_MIME_IMAGE_PIX, args.width, args.height);

    if (img == NULL)
	return 1;

    if (args.red && args.green && args.blue)
	color = ICV_COLOR_RGB;
    else if (args.blue && args.green)
	color = ICV_COLOR_BG;
    else if (args.red && args.blue)
	color = ICV_COLOR_RB;
    else if (args.red && args.green)
	color = ICV_COLOR_RG;
    else if (args.red)
	color = ICV_COLOR_R;
    else if (args.blue)
	color = ICV_COLOR_B;
    else if (args.green)
	color = ICV_COLOR_G;
    else
	color = ICV_COLOR_RGB;
    	/* no color scheme specified; rweight,gweight,bweight have
	 * all remain zero, so weight the three colors equally.
	 */

    icv_rgb2gray(img, color, args.red_weight, args.green_weight,
	args.blue_weight);

    icv_write(img, args.output, BU_MIME_IMAGE_BW);

    int fno = fileno(stdout);
    if ((fno > 0) && !isatty(fno) && args.output) {
	icv_write(img, NULL, BU_MIME_IMAGE_BW);
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
