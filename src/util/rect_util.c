/*                      R E C T _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "common.h"

#include <limits.h>

#include "bio.h"

#include "bu/arg.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "icv.h"

#include "rect_util.h"

struct rect_args {
    struct rect_args *self;
    int help;
    int input_width;
    int input_height;
    int output_width;
    int output_height;
    int x_origin;
    int y_origin;
    const char *output_file;
};

enum rect_dimension {
    RECT_INPUT_SIZE,
    RECT_INPUT_WIDTH,
    RECT_INPUT_HEIGHT,
    RECT_OUTPUT_SIZE,
    RECT_OUTPUT_WIDTH,
    RECT_OUTPUT_HEIGHT
};

static int
rect_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum rect_dimension dimension)
{
    struct rect_args *args = storage ? *(struct rect_args **)storage : NULL;
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (!args)
	return 0;

    switch (dimension) {
	case RECT_INPUT_SIZE:
	    args->input_width = value;
	    args->input_height = value;
	    break;
	case RECT_INPUT_WIDTH:
	    args->input_width = value;
	    break;
	case RECT_INPUT_HEIGHT:
	    args->input_height = value;
	    break;
	case RECT_OUTPUT_SIZE:
	    args->output_width = value;
	    args->output_height = value;
	    break;
	case RECT_OUTPUT_WIDTH:
	    args->output_width = value;
	    break;
	case RECT_OUTPUT_HEIGHT:
	    args->output_height = value;
	    break;
    }
    return 0;
}

#define RECT_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return rect_dimension(msg, arg, storage, _dimension); \
    }

RECT_DIMENSION_CALLBACK(rect_input_size, RECT_INPUT_SIZE)
RECT_DIMENSION_CALLBACK(rect_input_width, RECT_INPUT_WIDTH)
RECT_DIMENSION_CALLBACK(rect_input_height, RECT_INPUT_HEIGHT)
RECT_DIMENSION_CALLBACK(rect_output_size, RECT_OUTPUT_SIZE)
RECT_DIMENSION_CALLBACK(rect_output_width, RECT_OUTPUT_WIDTH)
RECT_DIMENSION_CALLBACK(rect_output_height, RECT_OUTPUT_HEIGHT)

static const struct bu_arg_option rect_options[] = {
    BU_ARG_FLAG("h", "help", struct rect_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("s", "input-size", struct rect_args, self,
	rect_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct rect_args, self,
	rect_input_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "input-height", struct rect_args, self,
	rect_input_height, "pixels", "Set the input image height"),
    BU_ARG_CUSTOM("S", "output-size", struct rect_args, self,
	rect_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_CUSTOM("W", "output-width", struct rect_args, self,
	rect_output_width, "pixels", "Set the output rectangle width"),
    BU_ARG_CUSTOM("N", "output-height", struct rect_args, self,
	rect_output_height, "pixels", "Set the output rectangle height"),
    BU_ARG_INTEGER_RANGE("x", "x-origin", struct rect_args, x_origin,
	1, 0, 1, INT_MAX, "pixels", "Set the rectangle x origin"),
    BU_ARG_INTEGER_RANGE("y", "y-origin", struct rect_args, y_origin,
	1, 0, 1, INT_MAX, "pixels", "Set the rectangle y origin"),
    BU_ARG_VALUE("o", "output", struct rect_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Write output to a file"),
    BU_ARG_OPTION_NULL
};

int
rect_main(int argc, char **argv, const char *name, const char *description,
	enum bu_mime_image_t image_type)
{
    const struct bu_arg_operand operands[] = {
	BU_ARG_OPERAND("input", BU_ARG_VALUE_FILE, 0, 1,
	    "Input image file (default: standard input)", NULL),
	BU_ARG_OPERAND_NULL
    };
    const struct bu_arg_schema schema = BU_ARG_SCHEMA(
	name, description, rect_options, operands, BU_ARG_PARSE_OPTIONS_FIRST,
	BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));
    const struct bu_arg_grammar grammar = {&schema, NULL, NULL};
    struct rect_args args = {NULL, 0, 512, 512, 0, 0, 0, 0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    icv_image_t *image;
    const char *input_file = NULL;
    args.self = &args;

    int introspection = bu_arg_introspect(&msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    int operand_index = bu_arg_schema_parse_complete(&schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0)
	bu_exit(1, "%s", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&schema, name);
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "rectangle extractor help");
	return 0;
    }
    if (argc - operand_index - 1 == 1)
	input_file = argv[operand_index + 1];
    else if (isatty(fileno(stdin)))
	bu_exit(1, "%s requires an input file or redirected input\n", name);

    image = icv_read(input_file, image_type, args.input_width,
	args.input_height);
    if (!image)
	return 1;
    if (icv_crop_rect(image, args.x_origin, args.y_origin,
	    args.output_width, args.output_height) != 0) {
	icv_destroy(image);
	return 1;
    }
    if (icv_write(image, args.output_file, image_type) != 0) {
	icv_destroy(image);
	return 1;
    }
    if (!isatty(fileno(stdout)) && args.output_file
	&& icv_write(image, NULL, image_type) != 0) {
	icv_destroy(image);
	return 1;
    }

    icv_destroy(image);
    return 0;
}
