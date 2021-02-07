/*                      P N G _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file png_read.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "gcv/api.h"
#include "bu/mime.h"
#include "wdb.h"

struct png_read_opts{
    int coloured;
};


static void create_opts(struct bu_opt_desc **opts_desc, void **dest_options_data)
{
    struct png_read_opts *opts_data;

    bu_log("VOL_PLUGIN: entered create_opts()\n");

    BU_ALLOC(opts_data, struct png_read_opts);
    *dest_options_data = opts_data;
    *opts_desc = (struct bu_opt_desc *)bu_calloc(3, sizeof(struct bu_opt_desc), "options_desc");

    opts_data->coloured = 0;

    BU_OPT((*opts_desc)[0], "c", "colored", NULL, bu_opt_bool,
	   &opts_data->coloured, "Check if it is coloured");
    BU_OPT_NULL((*opts_desc)[1]);
}


static void free_opts(void *options_data)
{
    bu_log("VOL_PLUGIN: entered free_opts()\n");

    bu_free(options_data, "options_data");
}


static int png_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *options_data, const char *source_path)
{
    const point_t center = {0.0, 0.0, 0.0};
    const fastf_t radius = 1.0;

    const struct png_read_opts *opts = (struct png_read_opts *)options_data;

    bu_log("importing from PNG file '%s'\n", source_path);
    bu_log("image is coloured: %s\n", opts->coloured ? "True" : "False");

    mk_id(context->dbip->dbi_wdbp, "GCV plugin test");

    mk_sph(context->dbip->dbi_wdbp, "test", center, radius);

    return 1;
}


HIDDEN int png_can_read(const char * data)
{
    bu_log("VOL_PLUGIN: entered png_can_read, data=%p\n", (void *)data);

    if (!data)
	return 0;
    return 1;
}


const struct gcv_filter gcv_conv_png_read = {
    "PNG Reader", GCV_FILTER_READ, (int)BU_MIME_IMAGE_PNG, BU_MIME_IMAGE, png_can_read,
    create_opts, free_opts, png_read
};


static const struct gcv_filter * const filters[] = {&gcv_conv_png_read, NULL};

const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info()
{
    return &gcv_plugin_info_s;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
