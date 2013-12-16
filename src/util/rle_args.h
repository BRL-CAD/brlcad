/*                       R L E _ A R G S . H
 * BRL-CAD
 *
 * Copyright (c) 1986-2013 United States Government as represented by
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

#include <stdlib.h>

#include "rle.h"

#ifndef UTIL_RLE_ARGS_H
#define UTIL_RLE_ARGS_H

extern int
get_args(int argc, char **argv, rle_hdr *outrle, FILE** infp, char** infile, int **background, size_t* file_width, size_t* file_height);

#endif /* UTIL_RLE_ARGS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
