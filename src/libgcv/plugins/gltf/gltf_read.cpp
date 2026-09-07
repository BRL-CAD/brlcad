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
#include <cstring>
#include <set>
#include <stdlib.h>
#include <vector>

#include "tiny_gltf.h"

#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/units.h"
#include "gcv/api.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

struct conversion_state
{
	std::string input_file;	/* name of the input file */
	struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
	struct wmember scene;
};
#define CONVERSION_STATE_ZERO {"", NULL, WMEMBER_INIT_ZERO}


static bool
accessor_data(const tinygltf::Model &model, int accessor_index,
	const tinygltf::Accessor **accessor_out, const unsigned char **data_out,
	size_t *stride_out)
{
	if (accessor_index < 0 || (size_t)accessor_index >= model.accessors.size())
		return false;

	const tinygltf::Accessor &accessor = model.accessors[accessor_index];
	if (accessor.bufferView < 0 ||
		(size_t)accessor.bufferView >= model.bufferViews.size())
		return false;

	const tinygltf::BufferView &view = model.bufferViews[accessor.bufferView];
	if (view.buffer < 0 || (size_t)view.buffer >= model.buffers.size())
		return false;

	const tinygltf::Buffer &buffer = model.buffers[view.buffer];
	int stride = accessor.ByteStride(view);
	int component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
	int component_count = tinygltf::GetNumComponentsInType(accessor.type);
	if (stride <= 0 || component_size <= 0 || component_count <= 0)
		return false;

	size_t element_size = (size_t)component_size * (size_t)component_count;
	if ((size_t)stride < element_size || view.byteOffset > buffer.data.size() ||
		view.byteLength > buffer.data.size() - view.byteOffset ||
		accessor.byteOffset > view.byteLength)
		return false;

	size_t available = view.byteLength - accessor.byteOffset;
	if (accessor.count > 0 &&
		(available < element_size ||
		 accessor.count - 1 > (available - element_size) / (size_t)stride))
		return false;

	*accessor_out = &accessor;
	*data_out = buffer.data.data() + view.byteOffset + accessor.byteOffset;
	*stride_out = (size_t)stride;
	return true;
}


static bool
read_index(const unsigned char *data, int component_type, size_t *index)
{
	switch (component_type) {
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
			*index = *data;
			return true;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			*index = (size_t)data[0] | ((size_t)data[1] << 8);
			return true;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			*index = (size_t)data[0] | ((size_t)data[1] << 8) |
				((size_t)data[2] << 16) | ((size_t)data[3] << 24);
			return true;
		default:
			return false;
	}
}


static float
read_float(const unsigned char *data)
{
	uint32_t bits = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}


static bool
generate_geometry(struct conversion_state *state, const tinygltf::Model &model,
	int mesh_number, const std::string &shape_name, wmember &region)
{
	if (mesh_number < 0 || (size_t)mesh_number >= model.meshes.size()) {
		bu_log("glTF mesh index %d is invalid\n", mesh_number);
		return false;
	}

	const tinygltf::Mesh &mesh = model.meshes[mesh_number];
	for (size_t j = 0; j < mesh.primitives.size(); j++) {
		const tinygltf::Primitive &primitive = mesh.primitives[j];
		if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
			bu_log("glTF primitive %zu is not a triangle mesh\n", j);
			return false;
		}

		auto position_it = primitive.attributes.find("POSITION");
		if (position_it == primitive.attributes.end()) {
			bu_log("glTF primitive %zu has no POSITION accessor\n", j);
			return false;
		}

		const tinygltf::Accessor *position_accessor = NULL;
		const unsigned char *position_data = NULL;
		size_t position_stride = 0;
		if (!accessor_data(model, position_it->second, &position_accessor,
			&position_data, &position_stride) ||
			position_accessor->type != TINYGLTF_TYPE_VEC3 ||
			position_accessor->componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
			bu_log("glTF primitive %zu has invalid POSITION data\n", j);
			return false;
		}

		if (position_accessor->count > SIZE_MAX / 3) {
			bu_log("glTF primitive %zu has too many vertices\n", j);
			return false;
		}
		std::vector<fastf_t> vertices(position_accessor->count * 3);
		for (size_t i = 0; i < position_accessor->count; i++) {
			for (size_t axis = 0; axis < 3; axis++) {
				vertices[i * 3 + axis] = (fastf_t)read_float(
					position_data + i * position_stride + axis * sizeof(float));
			}
		}

		const tinygltf::Accessor *index_accessor = NULL;
		const unsigned char *index_data = NULL;
		size_t index_stride = 0;
		size_t index_count = position_accessor->count;
		if (primitive.indices >= 0) {
			if (!accessor_data(model, primitive.indices, &index_accessor,
				&index_data, &index_stride) ||
				index_accessor->type != TINYGLTF_TYPE_SCALAR) {
				bu_log("glTF primitive %zu has invalid index data\n", j);
				return false;
			}
			index_count = index_accessor->count;
		}

		if (index_count == 0 || index_count % 3 != 0) {
			bu_log("glTF primitive %zu does not contain complete triangles\n", j);
			return false;
		}

		std::vector<int> faces(index_count);
		for (size_t i = 0; i < index_count; i++) {
			size_t index = i;
			if (index_accessor &&
				!read_index(index_data + i * index_stride,
					index_accessor->componentType, &index)) {
				bu_log("glTF primitive %zu uses an unsupported index type\n", j);
				return false;
			}
			if (index >= position_accessor->count || index > INT_MAX) {
				bu_log("glTF primitive %zu contains an invalid vertex index\n", j);
				return false;
			}
			faces[i] = (int)index;
		}

		std::string bot_name = shape_name + std::to_string(j);
		if (mk_bot(state->fd_out, bot_name.c_str(), RT_BOT_SOLID,
			RT_BOT_UNORIENTED, 0, position_accessor->count,
			index_count / 3, vertices.data(), faces.data(), NULL, NULL) < 0 ||
			!mk_addmember(bot_name.c_str(), &region.l, NULL, WMOP_UNION)) {
			bu_log("Unable to write glTF primitive %zu\n", j);
			return false;
		}
	}

	return true;
}


static bool
handle_node(struct conversion_state *state, const tinygltf::Model &model,
	int node_index, struct wmember &regions, std::set<int> &active_nodes)
{
	if (node_index < 0 || (size_t)node_index >= model.nodes.size()) {
		bu_log("glTF node index %d is invalid\n", node_index);
		return false;
	}
	if (!active_nodes.insert(node_index).second) {
		bu_log("glTF node hierarchy contains a cycle at node %d\n", node_index);
		return false;
	}
	struct active_node_guard {
		std::set<int> &nodes;
		int index;
		~active_node_guard() { nodes.erase(index); }
	} guard = {active_nodes, node_index};

	const tinygltf::Node &node = model.nodes[node_index];
	if (!node.translation.empty() && node.translation.size() != 3) {
		bu_log("glTF node %d has invalid translation data\n", node_index);
		return false;
	}
	if (!node.matrix.empty() && node.matrix.size() != 16) {
		bu_log("glTF node %d has invalid matrix data\n", node_index);
		return false;
	}
	if (node.children.empty() && node.mesh == -1)
		return true;

	std::string region_name = "Region_" + std::to_string(node_index);
	struct wmember region;
	BU_LIST_INIT(&region.l);
	struct wmember_list_guard {
		struct bu_list *head;
		~wmember_list_guard() { if (head) mk_freemembers(head); }
	} region_guard = {&region.l};

	int mesh_number = node.mesh;
	if (mesh_number != -1) {
		if ((size_t)mesh_number >= model.meshes.size()) {
			bu_log("glTF node %d has invalid mesh index %d\n", node_index, mesh_number);
			return false;
		}
		const tinygltf::Mesh &mesh = model.meshes[mesh_number];
		std::string shape_name = mesh.name;
		if (mesh.name.empty()) {
			shape_name = "shape" + std::to_string(node_index);
		}
		if (!generate_geometry(state, model, mesh_number, shape_name, region)) {
			return false;
		}
	}
	for (size_t i = 0; i < node.children.size(); i++) {
		if (!handle_node(state, model, node.children[i], region, active_nodes)) {
			return false;
		}
	}

	if (BU_LIST_IS_EMPTY(&region.l)) {
		return true;
	}
	int comb_result = mk_lrcomb(state->fd_out, region_name.c_str(), &region,
		node.children.empty() ? 1 : 0, NULL, NULL, NULL,
		0, 0, 0, 100, 0);
	region_guard.head = NULL;
	if (comb_result < 0) {
		return false;
	}

	if (!node.translation.empty()) {
		fastf_t matrix[16] = {0};
		matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
		matrix[3] = (fastf_t)node.translation[0];
		matrix[7] = (fastf_t)node.translation[1];
		matrix[11] = (fastf_t)node.translation[2];
		if (!mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION)) {
			return false;
		}
	} else if (!node.matrix.empty()) {
		fastf_t matrix[16];
		for (int i = 0; i < 16; i++)
			matrix[i] = (fastf_t)node.matrix[i];
		if (!mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION)) {
			return false;
		}
	} else if (!mk_addmember(region_name.c_str(), &regions.l, NULL, WMOP_UNION)) {
		return false;
	}
	return true;
}

static bool
convert_from_gltf(struct conversion_state *state, const tinygltf::Model &model)
{
	if (model.scenes.empty()) {
		bu_log("glTF input does not define a scene\n");
		return false;
	}
	int scene_index = model.defaultScene >= 0 ? model.defaultScene : 0;
	if ((size_t)scene_index >= model.scenes.size()) {
		bu_log("glTF default scene index %d is invalid\n", scene_index);
		return false;
	}
	const tinygltf::Scene &scene = model.scenes[scene_index];
	std::set<int> active_nodes;
	//for each top level scene node
	for (size_t i = 0; i < scene.nodes.size(); i++)
	{
		if (!handle_node(state, model, scene.nodes[i], state->scene, active_nodes))
			return false;
	}
	return true;
}

static int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options), const void *UNUSED(options_data), const char *source_path)
{

	tinygltf::Model model;
	std::string err;
	std::string warn;
	std::string input_filename(source_path);
	struct bu_vls vext = BU_VLS_INIT_ZERO;
	bu_path_component(&vext, source_path, BU_PATH_EXT);
	std::string ext(bu_vls_cstr(&vext));
	bu_vls_free(&vext);

	bool ret = false;

	// getting undefined reference here for the class
	tinygltf::TinyGLTF gltf_ctx;

	if (ext.compare("glb") == 0) {
		std::cout << "File type: binary glTF" << std::endl;
		// assume binary glTF.
		ret = gltf_ctx.LoadBinaryFromFile(&model, &err, &warn,
			input_filename);
	}
	else {
		std::cout << "File type: ASCII glTF" << std::endl;
		// assume ascii glTF.
		ret =
			gltf_ctx.LoadASCIIFromFile(&model, &err, &warn, input_filename.c_str());
	}

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}


	struct conversion_state state = CONVERSION_STATE_ZERO;

	struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);

	//generate list to hold scene geometry
	BU_LIST_INIT(&state.scene.l);
	state.input_file = input_filename;
	state.fd_out = wdbp;

	std::string title = "gltf conversion from" + state.input_file;
	//set geometry title
	mk_id(state.fd_out, title.c_str());

	if (!convert_from_gltf(&state, model)) {
		mk_freemembers(&state.scene.l);
		return -1;
	}

	//combine all top level regions
	if (mk_lcomb(wdbp, "all", &state.scene, 0, NULL, NULL, NULL, 0) < 0)
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
