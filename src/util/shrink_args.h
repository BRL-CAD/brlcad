/*                    S H R I N K _ A R G S . H
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

#ifndef BRLCAD_SRC_UTIL_SHRINK_ARGS_H
#define BRLCAD_SRC_UTIL_SHRINK_ARGS_H

#include "bu/arg.h"

enum shrink_method {
    SHRINK_METHOD_BOXCAR = 1,
    SHRINK_METHOD_UNDERSAMPLE
};

struct shrink_args {
    struct shrink_args *self;
    int help;
    int width;
    int height;
    int factor;
    int method;
};

extern const struct bu_arg_option shrink_options[];

#endif /* BRLCAD_SRC_UTIL_SHRINK_ARGS_H */
