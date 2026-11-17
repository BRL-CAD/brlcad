/*                      T I L E _ A R G S . C
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
#include <stdint.h>

#include "tile_args.h"

enum tile_dimension {
    TILE_INPUT_SIZE,
    TILE_INPUT_WIDTH,
    TILE_INPUT_HEIGHT,
    TILE_OUTPUT_SIZE,
    TILE_OUTPUT_WIDTH,
    TILE_OUTPUT_HEIGHT
};

static int
tile_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum tile_dimension dimension)
{
    struct tile_args *args = storage ? *(struct tile_args **)storage : NULL;
    size_t value;
    size_t maximum = args ? args->dimension_limit : SIZE_MAX;

    if (!bu_arg_size_t_range_from_str(msg, &value, arg, 10, 1, maximum))
	return -1;
    if (!args)
	return 0;

    if (dimension == TILE_INPUT_SIZE || dimension == TILE_INPUT_WIDTH)
	args->input_width = value;
    if (dimension == TILE_INPUT_SIZE || dimension == TILE_INPUT_HEIGHT)
	args->input_height = value;
    if (dimension == TILE_OUTPUT_SIZE || dimension == TILE_OUTPUT_WIDTH)
	args->output_width = value;
    if (dimension == TILE_OUTPUT_SIZE || dimension == TILE_OUTPUT_HEIGHT)
	args->output_height = value;
    return 0;
}

#define TILE_DIMENSION_CALLBACK(_name, _dimension) \
    static int _name(struct bu_vls *msg, const char *arg, void *storage) \
    { \
	return tile_dimension(msg, arg, storage, _dimension); \
    }

TILE_DIMENSION_CALLBACK(tile_input_size, TILE_INPUT_SIZE)
TILE_DIMENSION_CALLBACK(tile_input_width, TILE_INPUT_WIDTH)
TILE_DIMENSION_CALLBACK(tile_input_height, TILE_INPUT_HEIGHT)
TILE_DIMENSION_CALLBACK(tile_output_size, TILE_OUTPUT_SIZE)
TILE_DIMENSION_CALLBACK(tile_output_width, TILE_OUTPUT_WIDTH)
TILE_DIMENSION_CALLBACK(tile_output_height, TILE_OUTPUT_HEIGHT)

const struct bu_arg_option tile_options[] = {
    BU_ARG_FLAG("h", "help", struct tile_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("s", "input-size", struct tile_args, self,
	tile_input_size, "pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "input-width", struct tile_args, self,
	tile_input_width, "pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "input-height", struct tile_args, self,
	tile_input_height, "pixels", "Set the input image height"),
    BU_ARG_CUSTOM("S", "output-size", struct tile_args, self,
	tile_output_size, "pixels", "Set both output dimensions"),
    BU_ARG_CUSTOM("W", "output-width", struct tile_args, self,
	tile_output_width, "pixels", "Set the output image width"),
    BU_ARG_CUSTOM("N", "output-height", struct tile_args, self,
	tile_output_height, "pixels", "Set the output image height"),
    BU_ARG_INTEGER_RANGE("o", "start-frame", struct tile_args, first_frame,
	10, 0, 1, INT_MAX, "integer", "Set the first frame number"),
    BU_ARG_OPTION_NULL
};
