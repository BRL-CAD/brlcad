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

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "gcv/api.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

#include "gltf_read_util.h"

struct conversion_state
{
    struct rt_wdb *fd_out;
    std::vector<std::vector<std::string>> mesh_bots;
    struct wmember scene;
};


static bool
add_mesh(struct conversion_state &state, const tg3_model &model,
    int32_t node_index, wmember &region)
{
    int32_t mesh_index = model.nodes[node_index].mesh;
    if (mesh_index < 0 || static_cast<uint32_t>(mesh_index) >= model.meshes_count) {
	bu_log("glTF node %d has invalid mesh index %d\n", node_index, mesh_index);
	return false;
    }

    const tg3_mesh &mesh = model.meshes[mesh_index];
    auto &bot_names = state.mesh_bots[mesh_index];
    if (bot_names.empty()) {
	std::string shape_name = brlcad_gltf::to_string(mesh.name);
	if (shape_name.empty())
	    shape_name = "shape" + std::to_string(node_index);

	for (uint32_t i = 0; i < mesh.primitives_count; ++i) {
	    std::vector<fastf_t> vertices;
	    std::vector<int> faces;
	    if (!brlcad_gltf::append_triangle_primitive(model, mesh_index, i, vertices, faces))
		return false;

	    std::string bot_name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
		shape_name + std::to_string(i), mesh_index);
	    if (mk_bot(state.fd_out, bot_name.c_str(), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
		    vertices.size() / ELEMENTS_PER_POINT, faces.size() / brlcad_gltf::TRIANGLE_VERTICES,
		    vertices.data(), faces.data(), NULL, NULL) < 0) {
		bu_log("Unable to write glTF mesh %d primitive %u\n", mesh_index, i);
		return false;
	    }
	    bot_names.push_back(bot_name);
	}
    }

    /* Each node supplies its own placement of the referenced mesh. */
    for (const auto &bot_name : bot_names) {
	if (!mk_addmember(bot_name.c_str(), &region.l, NULL, WMOP_UNION))
	    return false;
    }
    return true;
}


static bool
handle_node(struct conversion_state &state, const tg3_model &model,
    int32_t node_index, struct wmember &regions, std::set<int32_t> &active_nodes)
{
    brlcad_gltf::active_node active(model, node_index, active_nodes);
    const tg3_node *node = active.get();
    if (!node)
	return false;
    if (node->children_count == 0 && node->mesh == TG3_INDEX_NONE)
	return true;

    struct wmember region;
    BU_LIST_INIT(&region.l);
    struct wmember_list_guard {
	struct bu_list *head;
	~wmember_list_guard() { if (head) mk_freemembers(head); }
    } region_guard = {&region.l};

    if (node->mesh != TG3_INDEX_NONE && !add_mesh(state, model, node_index, region))
	return false;
    for (uint32_t i = 0; i < node->children_count; ++i) {
	if (!handle_node(state, model, node->children[i], region, active_nodes))
	    return false;
    }
    if (BU_LIST_IS_EMPTY(&region.l))
	return true;

    const std::string region_name = brlcad_gltf::unique_object_name(state.fd_out->dbip,
	"Region_" + std::to_string(node_index), node_index);
    int comb_result = mk_lrcomb(state.fd_out, region_name.c_str(), &region,
	node->children_count == 0 ? 1 : 0, NULL, NULL, NULL, 0, 0, 0, 100, 0);
    region_guard.head = NULL;
    if (comb_result < 0)
	return false;

    mat_t matrix;
    brlcad_gltf::node_transform(matrix, *node);
    return mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION) != NULL;
}


static bool
convert_from_gltf(struct conversion_state &state, const tg3_model &model)
{
    if (model.scenes_count == 0) {
	bu_log("glTF input does not define a scene\n");
	return false;
    }
    const tg3_scene *scene = brlcad_gltf::default_scene(model);
    if (!scene)
	return false;

    std::set<int32_t> active_nodes;
    for (uint32_t i = 0; i < scene->nodes_count; ++i) {
	if (!handle_node(state, model, scene->nodes[i], state.scene, active_nodes))
	    return false;
    }
    return true;
}


static int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options),
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

    struct conversion_state state = {};
    state.fd_out = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
    state.mesh_bots.resize(model->meshes_count);
    BU_LIST_INIT(&state.scene.l);

    std::string title = "glTF conversion from " + std::string(source_path);
    if (mk_id(state.fd_out, title.c_str()) < 0)
	return 0;
    if (!convert_from_gltf(state, *model.get())) {
	mk_freemembers(&state.scene.l);
	return 0;
    }

    const std::string scene_name = brlcad_gltf::unique_object_name(context->dbip, "all", 0);
    if (mk_lcomb(state.fd_out, scene_name.c_str(), &state.scene, 0, NULL, NULL, NULL, 0) < 0)
	return 0;

    return 1;
}


static int
gltf_can_read(const char *UNUSED(data))
{
    /* FIXME */
    return 0;
}

extern "C"
{
    static const struct gcv_filter gcv_conv_gltf_read = {
	"GLTF Reader", GCV_FILTER_READ, BU_MIME_MODEL_GLTF, gltf_can_read,
	NULL, NULL, gltf_read
    };

    static const struct gcv_filter * const filters[] = { &gcv_conv_gltf_read, NULL, NULL };

    const struct gcv_plugin gcv_plugin_info_s = { filters };

    COMPILER_DLLEXPORT const struct gcv_plugin *
	gcv_plugin_info() { return &gcv_plugin_info_s; }
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
