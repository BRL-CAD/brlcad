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

#include <climits>
#include <cstdint>
#include <limits>
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
    std::string input_file; /* name of the input file */
    struct rt_wdb *fd_out;  /* Resulting BRL-CAD file */
    struct wmember scene;
};
#define CONVERSION_STATE_ZERO {"", NULL, WMEMBER_INIT_ZERO}


static bool
generate_geometry(struct conversion_state *state, const tg3_model &model,
    int32_t mesh_number, const std::string &shape_name, wmember &region)
{
    if (mesh_number < 0 || static_cast<uint32_t>(mesh_number) >= model.meshes_count) {
	bu_log("glTF mesh index %d is invalid\n", mesh_number);
	return false;
    }

    const tg3_mesh &mesh = model.meshes[mesh_number];
    for (uint32_t primitive_number = 0; primitive_number < mesh.primitives_count;
	++primitive_number) {
	const tg3_primitive &primitive = mesh.primitives[primitive_number];
	if (primitive.mode != TG3_MODE_TRIANGLES) {
	    bu_log("glTF primitive %u is not a triangle mesh\n", primitive_number);
	    return false;
	}

	int32_t position_index = brlcad_gltf::find_attribute(primitive, "POSITION");
	if (position_index == TG3_INDEX_NONE) {
	    bu_log("glTF primitive %u has no POSITION accessor\n", primitive_number);
	    return false;
	}

	brlcad_gltf::accessor_view positions;
	if (!brlcad_gltf::get_accessor_view(model, position_index, positions) ||
	    positions.accessor->type != TG3_TYPE_VEC3 ||
	    !brlcad_gltf::is_supported_position_component(
		positions.accessor->component_type)) {
	    bu_log("glTF primitive %u has invalid POSITION data\n", primitive_number);
	    return false;
	}
	if (positions.accessor->component_type == TG3_COMPONENT_TYPE_DOUBLE)
	    brlcad_gltf::warn_legacy_double_position(
		static_cast<uint32_t>(mesh_number), primitive_number);

	std::vector<fastf_t> vertices;
	if (positions.accessor->count > vertices.max_size() / 3) {
	    bu_log("glTF primitive %u has too many vertices\n", primitive_number);
	    return false;
	}
	size_t vertex_count = static_cast<size_t>(positions.accessor->count);
	vertices.resize(vertex_count * 3);
	int32_t component_size = tg3_component_size(positions.accessor->component_type);
	for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
	    for (size_t axis = 0; axis < 3; ++axis) {
		double coordinate;
		if (!brlcad_gltf::read_coordinate(
			positions.data + vertex * positions.stride + axis * component_size,
			positions.accessor->component_type, coordinate))
		    return false;
		vertices[vertex * 3 + axis] = static_cast<fastf_t>(coordinate);
	    }
	}

	brlcad_gltf::accessor_view indices;
	const tg3_accessor *index_accessor = nullptr;
	size_t index_count = vertex_count;
	if (primitive.indices != TG3_INDEX_NONE) {
	    if (!brlcad_gltf::get_accessor_view(model, primitive.indices, indices) ||
		indices.accessor->type != TG3_TYPE_SCALAR ||
		indices.accessor->count > std::numeric_limits<size_t>::max()) {
		bu_log("glTF primitive %u has invalid index data\n", primitive_number);
		return false;
	    }
	    index_accessor = indices.accessor;
	    index_count = static_cast<size_t>(index_accessor->count);
	}

	if (index_count == 0 || index_count % 3 != 0) {
	    bu_log("glTF primitive %u does not contain complete triangles\n", primitive_number);
	    return false;
	}

	std::vector<int> faces(index_count);
	for (size_t i = 0; i < index_count; ++i) {
	    size_t index = i;
	    if (index_accessor &&
		!brlcad_gltf::read_index(indices.data + i * indices.stride,
		    index_accessor->component_type, index)) {
		bu_log("glTF primitive %u uses an unsupported index type\n", primitive_number);
		return false;
	    }
	    if (index >= vertex_count || index > INT_MAX) {
		bu_log("glTF primitive %u contains an invalid vertex index\n", primitive_number);
		return false;
	    }
	    faces[i] = static_cast<int>(index);
	}

	std::string bot_name = shape_name + std::to_string(primitive_number);
	if (mk_bot(state->fd_out, bot_name.c_str(), RT_BOT_SOLID,
		RT_BOT_UNORIENTED, 0, vertex_count, index_count / 3,
		vertices.data(), faces.data(), NULL, NULL) < 0 ||
	    !mk_addmember(bot_name.c_str(), &region.l, NULL, WMOP_UNION)) {
	    bu_log("Unable to write glTF primitive %u\n", primitive_number);
	    return false;
	}
    }

    return true;
}


static bool
handle_node(struct conversion_state *state, const tg3_model &model,
    int32_t node_index, struct wmember &regions, std::set<int32_t> &active_nodes)
{
    if (node_index < 0 || static_cast<uint32_t>(node_index) >= model.nodes_count) {
	bu_log("glTF node index %d is invalid\n", node_index);
	return false;
    }
    if (!active_nodes.insert(node_index).second) {
	bu_log("glTF node hierarchy contains a cycle at node %d\n", node_index);
	return false;
    }
    struct active_node_guard {
	std::set<int32_t> &nodes;
	int32_t index;
	~active_node_guard() { nodes.erase(index); }
    } guard = {active_nodes, node_index};

    const tg3_node &node = model.nodes[node_index];
    if (node.children_count == 0 && node.mesh == TG3_INDEX_NONE)
	return true;

    std::string region_name = "Region_" + std::to_string(node_index);
    struct wmember region;
    BU_LIST_INIT(&region.l);
    struct wmember_list_guard {
	struct bu_list *head;
	~wmember_list_guard() { if (head) mk_freemembers(head); }
    } region_guard = {&region.l};

    if (node.mesh != TG3_INDEX_NONE) {
	if (node.mesh < 0 || static_cast<uint32_t>(node.mesh) >= model.meshes_count) {
	    bu_log("glTF node %d has invalid mesh index %d\n", node_index, node.mesh);
	    return false;
	}
	const tg3_mesh &mesh = model.meshes[node.mesh];
	std::string shape_name = brlcad_gltf::to_string(mesh.name);
	if (shape_name.empty())
	    shape_name = "shape" + std::to_string(node_index);
	if (!generate_geometry(state, model, node.mesh, shape_name, region))
	    return false;
    }
    for (uint32_t i = 0; i < node.children_count; ++i) {
	if (!handle_node(state, model, node.children[i], region, active_nodes))
	    return false;
    }

    if (BU_LIST_IS_EMPTY(&region.l))
	return true;

    int comb_result = mk_lrcomb(state->fd_out, region_name.c_str(), &region,
	node.children_count == 0 ? 1 : 0, NULL, NULL, NULL, 0, 0, 0, 100, 0);
    region_guard.head = NULL;
    if (comb_result < 0)
	return false;

    fastf_t matrix[16];
    fastf_t *member_matrix = NULL;
    if (node.has_matrix) {
	for (size_t i = 0; i < 16; ++i)
	    matrix[i] = static_cast<fastf_t>(node.matrix[i]);
	member_matrix = matrix;
    } else if (!ZERO(node.translation[0]) || !ZERO(node.translation[1]) ||
	!ZERO(node.translation[2])) {
	MAT_IDN(matrix);
	matrix[3] = static_cast<fastf_t>(node.translation[0]);
	matrix[7] = static_cast<fastf_t>(node.translation[1]);
	matrix[11] = static_cast<fastf_t>(node.translation[2]);
	member_matrix = matrix;
    }

    return mk_addmember(region_name.c_str(), &regions.l, member_matrix, WMOP_UNION) != NULL;
}


static bool
convert_from_gltf(struct conversion_state *state, const tg3_model &model)
{
    if (model.scenes_count == 0) {
	bu_log("glTF input does not define a scene\n");
	return false;
    }
    int32_t scene_index = model.default_scene >= 0 ? model.default_scene : 0;
    if (scene_index < 0 || static_cast<uint32_t>(scene_index) >= model.scenes_count) {
	bu_log("glTF default scene index %d is invalid\n", scene_index);
	return false;
    }

    const tg3_scene &scene = model.scenes[scene_index];
    std::set<int32_t> active_nodes;
    for (uint32_t i = 0; i < scene.nodes_count; ++i) {
	if (!handle_node(state, model, scene.nodes[i], state->scene, active_nodes))
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
	return -1;
    }

    struct conversion_state state = CONVERSION_STATE_ZERO;
    state.input_file = source_path;
    state.fd_out = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
    BU_LIST_INIT(&state.scene.l);

    std::string title = "glTF conversion from " + state.input_file;
    mk_id(state.fd_out, title.c_str());
    if (!convert_from_gltf(&state, *model.get())) {
	mk_freemembers(&state.scene.l);
	return -1;
    }

    if (mk_lcomb(state.fd_out, "all", &state.scene, 0, NULL, NULL, NULL, 0) < 0)
	return -1;

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
