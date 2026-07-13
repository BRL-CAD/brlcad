/*                         J T _ R E A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include "jt_parser.h"

#include "bu/log.h"
#include "bu/mime.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "wdb.h"

static int
jt_can_read(const char *path)
{
    return jt::has_jt_signature(path) ? 1 : 0;
}

static int
jt_read(struct gcv_context *context, const struct gcv_opts *options,
	const void *UNUSED(options_data), const char *source_path)
{
    jt::File file;
    std::string error;
    if (!file.load(source_path, error)) {
	bu_log("JT import failed: %s: %s\n", source_path, error.c_str());
	return 0;
    }

    std::vector<jt::Mesh> meshes;
    for (const jt::TocEntry &entry : file.toc()) {
	if (entry.type() < 6 || entry.type() > 16) continue;
	jt::Segment segment;
	std::vector<jt::Element> elements;
	if (!file.segment(entry, segment, error) || !file.elements(segment, elements, error)) {
	    bu_log("JT import failed: %s: %s\n", source_path, error.c_str());
	    return 0;
	}
	for (const jt::Element &element : elements) {
	    if (!file.is_legacy_mesh(element)) continue;
	    jt::Mesh mesh;
	    if (!file.legacy_mesh(element, mesh, error)) {
		bu_log("JT import failed: %s: %s\n", source_path, error.c_str());
		return 0;
	    }
	    meshes.push_back(std::move(mesh));
	}
    }

    struct rt_wdb *wdb = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    mk_id_units(wdb, "Conversion from JT format", "mm");
    if (meshes.empty()) {
	bu_log("JT import failed: %s: no supported renderable mesh geometry found\n", source_path);
	return 0;
    }

    struct wmember all = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&all.l);
    for (size_t i = 0; i < meshes.size(); ++i) {
	std::string name = "jt_mesh_" + std::to_string(i + 1) + ".bot";
	jt::Mesh &mesh = meshes[i];
	for (double &coordinate : mesh.vertices) coordinate *= options->scale_factor;
	if (mk_bot(wdb, name.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
		mesh.vertices.size() / 3, mesh.faces.size() / 3, mesh.vertices.data(), mesh.faces.data(), NULL, NULL) != 0) {
	    bu_log("JT import failed: cannot create %s\n", name.c_str());
	    return 0;
	}
	mk_addmember(name.c_str(), &all.l, NULL, WMOP_UNION);
    }
    mk_lcomb(wdb, "all", &all, 0, NULL, NULL, NULL, 0);
    return 1;
}

extern "C" {
static const struct gcv_filter gcv_conv_jt_read = {
    "JT Reader", GCV_FILTER_READ, BU_MIME_MODEL_JT, jt_can_read, NULL, NULL, jt_read
};

static const struct gcv_filter * const filters[] = {&gcv_conv_jt_read, NULL};
const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
}
}
