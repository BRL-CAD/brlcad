/*                      P N G _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
 * Read vol data from a png image file.
 *
 */

#include "common.h"

#include "gcv/api.h"
#include "bu/mime.h"
#include "wdb.h"

struct png_read_opts{
    int coloured;
};


static const struct bu_cmd_option png_read_schema_options[] = {
    BU_CMD_BOOL("c", "colored", struct png_read_opts, coloured, "bool",
	"Specify whether the image is coloured."),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_schema png_read_schema = {
    "png-read", "PNG volume reader options.", png_read_schema_options, NULL,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static void
png_read_create_schema_opts(const struct bu_cmd_schema **schema, void **dest_options_data)
{
    struct png_read_opts *opts_data;

    BU_ALLOC(opts_data, struct png_read_opts);
    *dest_options_data = opts_data;

    opts_data->coloured = 0;
    *schema = &png_read_schema;
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
    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);

    bu_log("importing from PNG file '%s'\n", source_path);
    bu_log("image is coloured: %s\n", opts->coloured ? "True" : "False");

    mk_id(wdbp, "GCV plugin test");

    mk_sph(wdbp, "test", center, radius);

    return 1;
}


static int png_can_read(const char * data)
{
    bu_log("VOL_PLUGIN: entered png_can_read, data=%p\n", (void *)data);

    if (!data)
	return 0;
    return 1;
}


const struct gcv_filter gcv_conv_png_read = {
    "PNG Reader", GCV_FILTER_READ, BU_MIME_MODEL_UNKNOWN, png_can_read,
    NULL, free_opts, png_read
};


static const struct gcv_filter * const filters[] = {&gcv_conv_png_read, NULL};
static const struct gcv_filter_schema filter_schemas[] = {
    {&gcv_conv_png_read, png_read_create_schema_opts},
    {NULL, NULL}
};

const struct gcv_plugin gcv_plugin_info_s = {filters};
static const struct gcv_native_plugin gcv_plugin_native_info_s = {filter_schemas};

COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
}


COMPILER_DLLEXPORT const struct gcv_native_plugin *
gcv_plugin_native_info(void)
{
    return &gcv_plugin_native_info_s;
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
