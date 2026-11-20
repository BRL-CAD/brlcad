/*                   G L T F _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2002-2026 United States Government as represented by
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

#include <string>

#include "bu/units.h"
#include "gcv/api.h"
#include "wdb.h"

#include "gltf_read_util.h"
#include "gltf_scene.h"

static int
gltf_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *UNUSED(options_data), const char *source_path)
{
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(source_path);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get());
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", source_path,
	    static_cast<int>(parse_result));
	return 0;
    }

    if (!brlcad_gltf::profile_valid(*model.get(), source_path, gcv_target_mime(context), gcv_target_request_traits(context), context))
	return 0;
    bool permitted = true;
    for (const auto &loss : brlcad_gltf::scene_losses(*model.get()))
	if (!gcv_report_loss(context, loss.kind, loss.feature, loss.count, loss.message))
	    permitted = false;
    if (!permitted)
	return 0;
    struct rt_wdb *database = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
    std::string title = "glTF conversion from " + std::string(source_path);
    if (!database || mk_id_units(database, title.c_str(), "mm") < 0)
	return 0;
    return brlcad_gltf::convert_scene(database, *model.get(),
	bu_units_conversion("m") * gcv_options->scale_factor) ? 1 : 0;
}


extern "C"
{
    static const struct gcv_filter gcv_conv_gltf_read = {
	"GLTF Reader", GCV_FILTER_READ, BU_MIME_MODEL_GLTF, NULL,
	NULL, NULL, gltf_read
    };

    static const struct gcv_conversion gcv_conv_gltf_read_execution = {"GLTF Reader", GCV_FILTER_READ, NULL, NULL, gltf_read};

    static const struct gcv_filter * const filters[] = {&gcv_conv_gltf_read, NULL};

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info() { return &gcv_plugin_info_s; }

    COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
    gcv_plugin_info_v2()
    {
	size_t count = 0;
	const bu_mime_capability *types = brlcad_gltf::reader_capabilities(count);
	static const gcv_handler handler = {&gcv_conv_gltf_read_execution, types,
	    count, BU_MIME_DEFAULT_PRIORITY, NULL, NULL};
	static const gcv_plugin_v2 plugin = {GCV_PLUGIN_ABI_VERSION, sizeof(gcv_plugin_v2), NULL, 0, &handler, 1};
	return &plugin;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
