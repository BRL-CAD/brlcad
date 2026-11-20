/*                   G L T F _ S C E N E . C P P
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
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "bu/str.h"
#include "bn/qmath.h"
#include "bg/trimesh.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"

#include "gltf_read_util.h"
#include "gltf_scene.h"
#include "../mime_probe.h"
#include "mime_gltf.h"

namespace brlcad_gltf {

struct conversion_state
{
    struct rt_wdb *fd_out;  /* Resulting BRL-CAD file */
    struct wmember scene;
    fastf_t coordinate_scale;
    std::vector<std::string> mesh_regions;
};

struct member_list_guard {
    struct bu_list *head;
    ~member_list_guard() { if (head) mk_freemembers(head); }
};

static std::string
unique_name(struct conversion_state *state, const std::string &requested)
{
    bu_vls scrubbed = BU_VLS_INIT_ZERO;
    bu_vls_strcat(&scrubbed, requested.c_str());
    bu_vls_simplify(&scrubbed, ".-", "_", "_");
    std::string base = bu_vls_strlen(&scrubbed) ? bu_vls_cstr(&scrubbed) : "gltf";
    bu_vls_free(&scrubbed);
    std::string name = base;
    size_t suffix = 0;
    while (db_lookup(state->fd_out->dbip, name.c_str(), LOOKUP_QUIET))
	name = base + "_" + std::to_string(++suffix);
    return name;
}

static bool
node_matrix(mat_t matrix, const tg3_node &node, fastf_t coordinate_scale)
{
    if (node.has_matrix) {
	/* glTF stores columns; BRL-CAD stores rows. */
	for (size_t row = 0; row < 4; ++row)
	    for (size_t column = 0; column < 4; ++column)
		matrix[row * 4 + column] = static_cast<fastf_t>(node.matrix[column * 4 + row]);
    } else {
	quat_t rotation;
	/* BRL-CAD's quaternion convention gives the inverse active rotation. */
	for (size_t axis = 0; axis < 3; ++axis)
	    rotation[axis] = -static_cast<fastf_t>(node.rotation[axis]);
	rotation[W] = static_cast<fastf_t>(node.rotation[3]);
	fastf_t magnitude = QMAGNITUDE(rotation);
	if (!std::isfinite(magnitude) || magnitude < VDIVIDE_TOL) {
	    bu_log("glTF node has an invalid rotation quaternion\n");
	    return false;
	}
	quat_quat2mat(matrix, rotation);
	/* T * R * S scales the columns of R before adding translation. */
	for (size_t row = 0; row < 3; ++row) {
	    for (size_t column = 0; column < 3; ++column)
		matrix[row * 4 + column] *= static_cast<fastf_t>(node.scale[column]);
	    matrix[row * 4 + 3] = static_cast<fastf_t>(node.translation[row]);
	}
    }
    MAT_DELTAS_MUL(matrix, coordinate_scale, coordinate_scale, coordinate_scale);
    for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i) {
	if (!std::isfinite(matrix[i])) {
	    bu_log("glTF node has a non-finite transform\n");
	    return false;
	}
    }
    if (!ZERO(matrix[12]) || !ZERO(matrix[13]) || !ZERO(matrix[14]) || !EQUAL(matrix[15], 1.0)) {
	bu_log("glTF node transform is not affine\n");
	return false;
    }
    return true;
}


/* These limits describe the native geometry reader, independently of
 * broader SDK backends. Large headers may defer complete-set validation. */
static const bu_mime_trait gltf_json_traits[] = {
    BU_MIME_TRAIT_STRING("version", "2.0"),
    BU_MIME_TRAIT_SET("gltf.required-extensions", NULL, 0)
};
static const bu_mime_trait gltf_binary_traits[] = {
    BU_MIME_TRAIT_STRING("version", "2.0"),
    BU_MIME_TRAIT_SET("gltf.required-extensions", NULL, 0),
    BU_MIME_TRAIT_STRING("gltf.container-version", "2")
};
static const bu_mime_capability gltf_types[] = {
    {"model/gltf+json", gltf_json_traits, sizeof(gltf_json_traits) / sizeof(gltf_json_traits[0]), 0, BU_MIME_DEFER_INPUT_VALIDATION},
    {"model/gltf-binary", gltf_binary_traits, sizeof(gltf_binary_traits) / sizeof(gltf_binary_traits[0]), 0, BU_MIME_DEFER_INPUT_VALIDATION}
};

const bu_mime_capability *
reader_capabilities(size_t &count)
{
    count = sizeof(gltf_types) / sizeof(gltf_types[0]);
    return gltf_types;
}

bool
profile_valid(const tg3_model &model, const char *source_path, const char *selected_type,
    const bu_mime_traits *assertions, gcv_context *context)
{
    bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    bool valid = mime_gltf::file_traits(source_path, selected_type, observed);
    const char *type = bu_mime_traits_get(&observed, "gltf.container-version") ? "model/gltf-binary" : "model/gltf+json";
    std::string version = brlcad_gltf::to_string(model.asset.version);
    std::string minimum = brlcad_gltf::to_string(model.asset.min_version);
    if (minimum.empty()) minimum = version;
    int comparison = 0;
    if (!minimum.empty() && (!bu_mime_version_compare(minimum.c_str(), "2.0", &comparison) || comparison > 0)) {
	bu_log("GLTF Reader does not support minimum version %s\n", minimum.c_str());
	valid = false;
    }
    valid = valid && (context ?
	gcv_probe_file_traits_match(context, source_path, type, &observed, assertions) :
	gcv_probe_traits_match(type, &observed, assertions)) &&
	bu_mime_traits_resolve(&resolved, &observed, assertions);
    bool supported = false;
    for (const auto &capability : gltf_types)
	if (BU_STR_EQUAL(capability.mime_type, type) && bu_mime_capability_match(&capability, &resolved, BU_MIME_INPUT))
	    supported = true;
    valid = valid && supported;
    if (!valid) {
	bu_vls message = BU_VLS_INIT_ZERO;
	bu_mime_traits_describe(&observed, &message);
	bu_log("GLTF Reader cannot convert the declared profile:%s\n", bu_vls_cstr(&message));
	bu_vls_free(&message);
    }
    bu_mime_traits_free(&observed);
    bu_mime_traits_free(&resolved);
    return valid;
}

std::vector<gcv_conversion_loss>
scene_losses(const tg3_model &model)
{
    std::set<int32_t> meshes;
    std::set<int32_t> nodes;
    size_t skins = 0;
    size_t cameras = 0;
    size_t lights = 0;
    if (!model.scenes_count) {
	for (uint32_t i = 0; i < model.meshes_count; ++i)
	    meshes.insert(static_cast<int32_t>(i));
    } else {
	int32_t selected = model.default_scene >= 0 ? model.default_scene : 0;
	if (static_cast<uint32_t>(selected) >= model.scenes_count)
	    return {};
	const tg3_scene &scene = model.scenes[selected];
	std::vector<int32_t> pending;
	for (uint32_t root = 0; root < scene.nodes_count; ++root)
	    pending.push_back(scene.nodes[root]);
	while (!pending.empty()) {
	    int32_t index = pending.back();
	    pending.pop_back();
	    if (index < 0 || static_cast<uint32_t>(index) >= model.nodes_count || !nodes.insert(index).second)
		continue;
	    const tg3_node &node = model.nodes[index];
	    if (node.mesh != TG3_INDEX_NONE) meshes.insert(node.mesh);
	    if (node.skin != TG3_INDEX_NONE) ++skins;
	    if (node.camera != TG3_INDEX_NONE) ++cameras;
	    if (node.light != TG3_INDEX_NONE) ++lights;
	    for (uint32_t child = 0; child < node.children_count; ++child)
		pending.push_back(node.children[child]);
	}
    }
    size_t morph_targets = 0;
    size_t attributes = 0;
    std::set<int32_t> materials;
    for (int32_t index : meshes) {
	if (index < 0 || static_cast<uint32_t>(index) >= model.meshes_count)
	    continue;
	const tg3_mesh &mesh = model.meshes[index];
	for (uint32_t i = 0; i < mesh.primitives_count; ++i) {
	    const tg3_primitive &primitive = mesh.primitives[i];
	    if (primitive.material != TG3_INDEX_NONE) materials.insert(primitive.material);
	    morph_targets += primitive.targets_count;
	    for (uint32_t attribute = 0; attribute < primitive.attributes_count; ++attribute)
		if (!tg3_str_equals_cstr(primitive.attributes[attribute].key, "POSITION")) ++attributes;
	}
    }
    size_t animations = 0;
    for (uint32_t i = 0; i < model.animations_count; ++i) {
	const tg3_animation &animation = model.animations[i];
	for (uint32_t channel = 0; channel < animation.channels_count; ++channel) {
	    if (nodes.count(animation.channels[channel].target.node)) {
		++animations;
		break;
	    }
	}
    }
    std::vector<gcv_conversion_loss> losses;
    if (skins) losses.push_back({GCV_LOSS_GEOMETRY, "gltf.skinning", skins, "Skin deformation is not evaluated"});
    if (morph_targets) losses.push_back({GCV_LOSS_GEOMETRY, "gltf.morph-targets", morph_targets, "Morph targets and their weights are not evaluated"});
    if (!materials.empty()) losses.push_back({GCV_LOSS_APPEARANCE, "gltf.materials", materials.size(), "Materials and their textures are not converted"});
    if (attributes) losses.push_back({GCV_LOSS_APPEARANCE, "gltf.vertex-attributes", attributes, "Non-position vertex attributes are not converted"});
    if (cameras) losses.push_back({GCV_LOSS_APPEARANCE, "gltf.cameras", cameras, "Scene cameras are not converted"});
    if (lights) losses.push_back({GCV_LOSS_APPEARANCE, "gltf.lights", lights, "Scene lights are not converted"});
    if (animations) losses.push_back({GCV_LOSS_BEHAVIOR, "gltf.animations", animations, "Animation is not converted; static node transforms are used"});
    return losses;
}

static bool
generate_geometry(struct conversion_state *state, const tg3_model &model,
    int32_t mesh_number, wmember &region)
{
    if (mesh_number < 0 || static_cast<uint32_t>(mesh_number) >= model.meshes_count) {
	bu_log("glTF mesh index %d is invalid\n", mesh_number);
	return false;
    }

    const tg3_mesh &mesh = model.meshes[mesh_number];
    auto &mesh_region = state->mesh_regions[mesh_number];
    if (!mesh_region.empty())
	return mk_addmember(mesh_region.c_str(), &region.l, NULL, WMOP_UNION) != NULL;
    struct wmember primitives;
    BU_LIST_INIT(&primitives.l);
    member_list_guard primitives_guard = {&primitives.l};
    std::string shape_name = brlcad_gltf::to_string(mesh.name);
    if (shape_name.empty())
	shape_name = "gltf_mesh_" + std::to_string(mesh_number);
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
		vertices[vertex * 3 + axis] = static_cast<fastf_t>(coordinate) * state->coordinate_scale;
		if (!std::isfinite(vertices[vertex * 3 + axis])) {
		    bu_log("glTF primitive %u has a non-finite position\n", primitive_number);
		    return false;
		}
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

	std::string bot_name = unique_name(state, shape_name + "_" + std::to_string(primitive_number) + ".s");
	int bot_mode = bg_trimesh_manifold_closed(vertex_count, index_count / 3,
	    vertices.data(), faces.data()) ? RT_BOT_SOLID : RT_BOT_SURFACE;
	if (mk_bot(state->fd_out, bot_name.c_str(), bot_mode,
		RT_BOT_CCW, 0, vertex_count, index_count / 3,
		vertices.data(), faces.data(), NULL, NULL) < 0 ||
	    !mk_addmember(bot_name.c_str(), &primitives.l, NULL, WMOP_UNION)) {
	    bu_log("Unable to write glTF primitive %u\n", primitive_number);
	    return false;
	}
    }

    if (BU_LIST_IS_EMPTY(&primitives.l)) {
	bu_log("glTF mesh %d contains no triangle geometry\n", mesh_number);
	return false;
    }
    mesh_region = unique_name(state, shape_name + ".r");
    int result = mk_lcomb(state->fd_out, mesh_region.c_str(), &primitives, 1, NULL, NULL, NULL, 0);
    primitives_guard.head = NULL;
    return result >= 0 && mk_addmember(mesh_region.c_str(), &region.l, NULL, WMOP_UNION) != NULL;
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

    mat_t matrix;
    if (!node_matrix(matrix, node, state->coordinate_scale))
	return false;

    struct wmember region;
    BU_LIST_INIT(&region.l);
    member_list_guard region_guard = {&region.l};

    if (node.mesh != TG3_INDEX_NONE && !generate_geometry(state, model, node.mesh, region))
	return false;
    for (uint32_t i = 0; i < node.children_count; ++i) {
	if (!handle_node(state, model, node.children[i], region, active_nodes))
	    return false;
    }

    if (BU_LIST_IS_EMPTY(&region.l))
	return true;

    std::string region_name = unique_name(state, "Region_" + std::to_string(node_index));
    int comb_result = mk_lcomb(state->fd_out, region_name.c_str(), &region, 0, NULL, NULL, NULL, 0);
    region_guard.head = NULL;
    if (comb_result < 0)
	return false;

    return mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION) != NULL;
}


static bool
convert_from_gltf(struct conversion_state *state, const tg3_model &model)
{
    if (model.scenes_count == 0) {
	bu_log("glTF input has no scene; importing the mesh library in local coordinates\n");
	for (uint32_t i = 0; i < model.meshes_count; ++i)
	    if (!generate_geometry(state, model, static_cast<int32_t>(i), state->scene))
		return false;
	return true;
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


bool
convert_scene(struct rt_wdb *database, const tg3_model &model, fastf_t coordinate_scale)
{
    if (!database || !std::isfinite(coordinate_scale) || coordinate_scale <= 0)
	return false;
    struct conversion_state state = {};
    state.fd_out = database;
    state.coordinate_scale = coordinate_scale;
    state.mesh_regions.resize(model.meshes_count);
    BU_LIST_INIT(&state.scene.l);
    member_list_guard scene_guard = {&state.scene.l};
    if (!convert_from_gltf(&state, model))
	return false;
    if (BU_LIST_IS_EMPTY(&state.scene.l)) {
	bu_log("glTF input contains no triangle geometry in the selected scene\n");
	return false;
    }
    std::string name = unique_name(&state, "all");
    int result = mk_lcomb(database, name.c_str(), &state.scene, 0, NULL, NULL, NULL, 0);
    scene_guard.head = NULL;
    return result >= 0;
}

} // namespace brlcad_gltf

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
