/*                   G L T F _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2021 United States Government as represented by
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
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "gcv/api.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


HIDDEN int
gltf_read(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data), const char *UNUSED(source_path))
{
    return 1;
}


HIDDEN int
gltf_can_read(const char *UNUSED(data))
{
    /* FIXME */
    return 0;
}


static const struct gcv_filter gcv_conv_gltf_read = {
    "GLTF Reader", GCV_FILTER_READ, BU_MIME_MODEL_GLTF, gltf_can_read,
    NULL, NULL, gltf_read
};


static const struct gcv_filter * const filters[] = {&gcv_conv_gltf_read, NULL, NULL};

const struct gcv_plugin gcv_plugin_info_s = { filters };

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
