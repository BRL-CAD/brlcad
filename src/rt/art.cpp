/*                     A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2019 United States Government as represented by
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
/** @file rt/art.cpp
 *
 * Once you have appleseed installed, run BRL-CAD's CMake with APPLESEED_ROOT
 * set to enable this program:
 *
 * cmake .. -DAPPLESEED_ROOT=/path/to/appleseed
 *
 * (the appleseed root path should contain bin, lib and include directories)
 *
 * On Linux, if using the prebuilt binary you'll need to set LD_LIBRARY_PATH:
 * export LD_LIBRARY_PATH=/path/to/appleseed/lib
 *
 * Possibly useful info here:
 * https://forum.appleseedhq.net/t/api-documentation/91
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wpedantic"
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#  pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wclass-memaccess"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif
// appleseed.renderer headers.
#include "renderer/api/frame.h"
#include "renderer/api/lighting.h"
#include "renderer/api/log.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/utility.h"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#include "vmath.h"		/* vector math macros */
#include "raytrace.h"		/* librt interface definitions */


int
main(int argc, char **argv)
{
    if (argc < 3) {
	bu_exit(1, "Usage: %s model.g objects...\n", argv[0]);
    }


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
