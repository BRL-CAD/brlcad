/*                      T I L E _ A R G S . H
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

#ifndef BRLCAD_SRC_UTIL_TILE_ARGS_H
#define BRLCAD_SRC_UTIL_TILE_ARGS_H

#include "bu/arg.h"

struct tile_args {
    struct tile_args *self;
    int help;
    size_t input_width;
    size_t input_height;
    size_t output_width;
    size_t output_height;
    int first_frame;
    size_t dimension_limit;
};

extern const struct bu_arg_option tile_options[];

#endif /* BRLCAD_SRC_UTIL_TILE_ARGS_H */
