/*                       G D A L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gdal.cpp
 *
 * Geospatial Data Abstraction Library plugin for libgcv.
 *
 */


#include "common.h"
#include "vmath.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include <gdal.h>

#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"

HIDDEN int
gdal_read(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(gcv_options),
	               const void *UNUSED(options_data), const char *UNUSED(dest_path))
{
    return 1;
}

extern "C"
{
    struct gcv_filter gcv_conv_gdal_read =
    {"GDAL Reader", GCV_FILTER_READ, BU_MIME_MODEL_AUTO, NULL, NULL, gdal_read};

    static const struct gcv_filter * const filters[] = {&gcv_conv_gdal_read, NULL};
    const struct gcv_plugin gcv_plugin_info_s = { filters };
    GCV_EXPORT const struct gcv_plugin *gcv_plugin_info(){return &gcv_plugin_info_s;}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
