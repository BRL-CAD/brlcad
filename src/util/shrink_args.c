/*                    S H R I N K _ A R G S . C
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

#include "shrink_args.h"

enum shrink_dimension {
    SHRINK_SIZE,
    SHRINK_WIDTH,
    SHRINK_HEIGHT
};

static int
shrink_dimension(struct bu_vls *msg, const char *arg, void *storage,
	enum shrink_dimension dimension)
{
    struct shrink_args *args = storage ? *(struct shrink_args **)storage : NULL;
    int value;

    if (!bu_arg_integer_range_from_str(msg, &value, arg, 10, 1, INT_MAX))
	return -1;
    if (args) {
	if (dimension != SHRINK_HEIGHT)
	    args->width = value;
	if (dimension != SHRINK_WIDTH)
	    args->height = value;
    }
    return 0;
}

static int
shrink_size(struct bu_vls *msg, const char *arg, void *storage)
{
    return shrink_dimension(msg, arg, storage, SHRINK_SIZE);
}

static int
shrink_width(struct bu_vls *msg, const char *arg, void *storage)
{
    return shrink_dimension(msg, arg, storage, SHRINK_WIDTH);
}

static int
shrink_height(struct bu_vls *msg, const char *arg, void *storage)
{
    return shrink_dimension(msg, arg, storage, SHRINK_HEIGHT);
}

static int
shrink_undersample(struct bu_vls *UNUSED(msg), const char *UNUSED(arg),
	void *storage)
{
    if (storage) {
	struct shrink_args *args = *(struct shrink_args **)storage;
	if (args)
	    args->method = SHRINK_METHOD_UNDERSAMPLE;
    }
    return 0;
}

const struct bu_arg_option shrink_options[] = {
    BU_ARG_FLAG("h", "help", struct shrink_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM_FLAG("u", "undersample", "undersample", struct shrink_args,
	self, shrink_undersample, "Use nearest-neighbor undersampling"),
    BU_ARG_CUSTOM("s", "size", struct shrink_args, self, shrink_size,
	"pixels", "Set both input dimensions"),
    BU_ARG_CUSTOM("w", "width", struct shrink_args, self, shrink_width,
	"pixels", "Set the input image width"),
    BU_ARG_CUSTOM("n", "height", struct shrink_args, self, shrink_height,
	"pixels", "Set the input image height"),
    BU_ARG_INTEGER_RANGE("f", "factor", struct shrink_args, factor,
	1, 2, 1, INT_MAX, "integer", "Set the shrink factor"),
    BU_ARG_OPTION_NULL
};
