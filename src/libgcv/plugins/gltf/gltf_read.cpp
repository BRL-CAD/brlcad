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
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "gcv/api.h"
#include "vmath.h"
#include "rt/geom.h"
#include "rt/object_io.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/joint.h"
#include "raytrace.h"
#include "wdb.h"

#include "gltf_data.h"
#include "gltf_read_util.h"

struct conversion_state
{
    std::string input_file; /* name of the input file */
    struct rt_wdb *fd_out;  /* Resulting BRL-CAD file */
    struct wmember scene;
    std::set<std::string> object_names;
    std::vector<std::string> link_names;
    std::string world_link;
    bool has_native_joints;
};
#define CONVERSION_STATE_ZERO {"", NULL, WMEMBER_INIT_ZERO, {}, {}, "", false}

static constexpr double gltf_meters_to_millimeters = 1000.0;

static std::string
gltf_string(tg3_str value)
{
    return value.data ? std::string(value.data, value.len) : std::string();
}

static bool
import_object_description(struct conversion_state *state,
    const tg3_value *description)
{
    const tg3_value *name_value = brlcad_gltf::value_object_get(description, "name");
    const tg3_value *data_value = brlcad_gltf::value_object_get(description, "data");
    if (!name_value || name_value->type != TG3_VALUE_STRING || !data_value) {
	bu_log("Invalid BRL-CAD object metadata in glTF node\n");
	return false;
    }
    std::string name = gltf_string(name_value->string_val);
    if (name.empty()) {
	bu_log("BRL-CAD object metadata has no object name\n");
	return false;
    }
    if (!state->object_names.insert(name).second) {
	bu_log("Duplicate BRL-CAD object metadata for '%s'\n", name.c_str());
	return false;
    }
    if (db_lookup(state->fd_out->dbip, name.c_str(), LOOKUP_QUIET)) {
	bu_log("Cannot import glTF object '%s': name already exists\n", name.c_str());
	return false;
    }
    struct rt_data_node *data = brlcad_gltf::data_from_value(data_value);
    struct rt_data_context context = RT_DATA_CONTEXT_INIT;
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    context.media_type = "model/gltf+json";
    context.dbip = state->fd_out->dbip;
    context.local2base = state->fd_out->dbip->dbi_local2base;
    context.base2local = state->fd_out->dbip->dbi_base2local;
    const char *failure = nullptr;
    if (!data)
	failure = "decode neutral data";
    else if (rt_obj_data_import(&internal, data, &context) != BRLCAD_OK)
	failure = "import neutral data";
    else if (wdb_put_internal(state->fd_out, name.c_str(), &internal, 1.0) < 0)
	failure = "write database object";
    if (failure) {
	bu_log("Unable to import glTF object '%s': %s\n", name.c_str(), failure);
	rt_data_free(data);
	rt_db_free_internal(&internal);
	return false;
    }
    if (internal.idb_type == ID_JOINT)
	state->has_native_joints = true;
    rt_data_free(data);
    rt_db_free_internal(&internal);
    return true;
}

static bool
import_objects(struct conversion_state *state, const tg3_node &node)
{
    static const char *object_extras_name = "BRL-CAD_object";
    static const char *legacy_joint_extras_name = "BRL-CAD_joint";
    const tg3_value *description = brlcad_gltf::value_object_get(node.ext.extras,
	object_extras_name);

    if (!description)
	description = brlcad_gltf::value_object_get(node.ext.extras,
	    legacy_joint_extras_name);
    if (!description)
	return true;
    return import_object_description(state, description);
}

static const tg3_value *
object_description(const tg3_value *extras)
{
    static const char *object_extras_name = "BRL-CAD_object";
    return brlcad_gltf::value_object_get(extras, object_extras_name);
}

static std::string
unique_animation_name(struct conversion_state *state, const tg3_str &requested,
    uint32_t animation_index)
{
    std::string base = gltf_string(requested);
    if (base.empty())
	base = "animation_" + std::to_string(animation_index);
    std::string candidate = base;
    size_t suffix = 1;
    while (state->object_names.find(candidate) != state->object_names.end() ||
	db_lookup(state->fd_out->dbip, candidate.c_str(), LOOKUP_QUIET))
	candidate = base + "_" + std::to_string(suffix++);
    return candidate;
}

static bool
import_standard_animation(struct conversion_state *state, const tg3_model &model,
    const tg3_animation &source, uint32_t animation_index)
{
    point_t origin = VINIT_ZERO;
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    OBJ[ID_ANIMATION].ft_make(&OBJ[ID_ANIMATION], &internal, nullptr, origin, 1.0);
    auto *animation = static_cast<struct rt_animation_internal *>(internal.idb_ptr);
    if (!animation || !source.channels_count) {
	rt_db_free_internal(&internal);
	return false;
    }
    animation->channel_count = source.channels_count;
    animation->channels = static_cast<struct rt_animation_channel *>(bu_calloc(
	animation->channel_count, sizeof(struct rt_animation_channel),
	"glTF animation channels"));
    for (size_t channel_index = 0; channel_index < animation->channel_count;
	++channel_index)
	bu_vls_init(&animation->channels[channel_index].target_link);

    for (size_t channel_index = 0; channel_index < animation->channel_count;
	++channel_index) {
	const tg3_animation_channel &source_channel = source.channels[channel_index];
	if (source_channel.sampler < 0 ||
	    static_cast<uint32_t>(source_channel.sampler) >= source.samplers_count ||
	    source_channel.target.node < 0 ||
	    static_cast<uint32_t>(source_channel.target.node) >= model.nodes_count) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	if (model.nodes[source_channel.target.node].has_matrix) {
	    bu_log("glTF animation channel %zu targets a matrix node\n",
		channel_index);
	    rt_db_free_internal(&internal);
	    return false;
	}
	const tg3_animation_sampler &source_sampler =
	    source.samplers[source_channel.sampler];
	struct rt_animation_channel &channel = animation->channels[channel_index];
	std::string path = gltf_string(source_channel.target.path);
	std::string interpolation = gltf_string(source_sampler.interpolation);
	std::string link = state->link_names[static_cast<size_t>(
	    source_channel.target.node)];
	if (interpolation.empty())
	    interpolation = "LINEAR";
	if (link.empty() || path == "weights" ||
	    rt_animation_target_from_name(path.c_str(), &channel.target) != BRLCAD_OK ||
	    rt_animation_interpolation_from_name(interpolation.c_str(),
		&channel.interpolation) != BRLCAD_OK) {
	    bu_log("Unsupported glTF animation channel %zu\n", channel_index);
	    rt_db_free_internal(&internal);
	    return false;
	}
	bu_vls_strcpy(&channel.target_link, link.c_str());

	brlcad_gltf::accessor_view times;
	brlcad_gltf::accessor_view values;
	if (!brlcad_gltf::get_accessor_view(model, source_sampler.input, times) ||
	    !brlcad_gltf::get_accessor_view(model, source_sampler.output, values) ||
	    times.accessor->component_type != TG3_COMPONENT_TYPE_FLOAT ||
	    times.accessor->type != TG3_TYPE_SCALAR || !times.accessor->count ||
	    values.accessor->component_type != TG3_COMPONENT_TYPE_FLOAT) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	size_t components = rt_animation_channel_components(&channel);
	size_t tuple_count = channel.interpolation == RT_ANIMATION_CUBICSPLINE ? 3 : 1;
	if (times.accessor->count > SIZE_MAX ||
	    times.accessor->count > UINT64_MAX / tuple_count) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	channel.keyframe_count = static_cast<size_t>(times.accessor->count);
	if ((components == 3 && values.accessor->type != TG3_TYPE_VEC3) ||
	    (components == 4 && values.accessor->type != TG3_TYPE_VEC4) ||
	    values.accessor->count != times.accessor->count * tuple_count) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	channel.times = static_cast<fastf_t *>(bu_malloc(
	    channel.keyframe_count * sizeof(fastf_t), "glTF animation times"));
	size_t value_count = rt_animation_channel_value_count(&channel);
	channel.values = static_cast<fastf_t *>(bu_malloc(
	    value_count * sizeof(fastf_t), "glTF animation values"));
	for (size_t key = 0; key < channel.keyframe_count; ++key) {
	    double value;
	    if (!brlcad_gltf::read_coordinate(times.data + key * times.stride,
		    times.accessor->component_type, value)) {
		rt_db_free_internal(&internal);
		return false;
	    }
	    channel.times[key] = value;
	}
	const size_t component_bytes = sizeof(float);
	for (size_t tuple = 0; tuple < values.accessor->count; ++tuple) {
	    for (size_t component = 0; component < components; ++component) {
		double value;
		if (!brlcad_gltf::read_coordinate(values.data + tuple * values.stride +
			component * component_bytes,
			values.accessor->component_type, value)) {
		    rt_db_free_internal(&internal);
		    return false;
		}
		if (channel.target == RT_ANIMATION_TRANSLATION)
		    value *= gltf_meters_to_millimeters;
		channel.values[tuple * components + component] = value;
	    }
	}
    }
    if (rt_animation_validate(animation, nullptr) != 0) {
	rt_db_free_internal(&internal);
	return false;
    }
    std::string name = unique_animation_name(state, source.name, animation_index);
    if (wdb_put_internal(state->fd_out, name.c_str(), &internal, 1.0) < 0) {
	rt_db_free_internal(&internal);
	return false;
    }
    state->object_names.insert(name);
    rt_db_free_internal(&internal);
    return true;
}

static bool
import_animations(struct conversion_state *state, const tg3_model &model)
{
    for (uint32_t index = 0; index < model.animations_count; ++index) {
	const tg3_animation &animation = model.animations[index];
	const tg3_value *description = object_description(animation.ext.extras);
	if (description) {
	    if (!import_object_description(state, description))
		return false;
	    continue;
	}
	if (!import_standard_animation(state, model, animation, index))
	    return false;
    }
    return true;
}


static bool
read_skin_weight(const uint8_t *data, const tg3_accessor &accessor, double &weight)
{
    if (accessor.component_type == TG3_COMPONENT_TYPE_FLOAT)
	return brlcad_gltf::read_coordinate(data, accessor.component_type, weight);
    if (!accessor.normalized)
	return false;
    size_t value;
    if (!brlcad_gltf::read_index(data, accessor.component_type, value))
	return false;
    if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)
	weight = static_cast<double>(value) / UINT8_MAX;
    else if (accessor.component_type == TG3_COMPONENT_TYPE_UNSIGNED_SHORT)
	weight = static_cast<double>(value) / UINT16_MAX;
    else
	return false;
    return true;
}

static bool
attach_skin(struct conversion_state *state, const tg3_model &model,
    const tg3_primitive &primitive, int32_t skin_index,
    const std::string &bot_name, size_t vertex_count)
{
    if (skin_index == TG3_INDEX_NONE)
	return true;
    if (skin_index < 0 || static_cast<uint32_t>(skin_index) >= model.skins_count)
	return false;
    const tg3_skin &gltf_skin = model.skins[skin_index];
    if (!gltf_skin.joints_count || gltf_skin.joints_count > UINT16_MAX)
	return false;

    std::vector<brlcad_gltf::accessor_view> joint_sets;
    std::vector<brlcad_gltf::accessor_view> weight_sets;
    for (size_t set = 0;; ++set) {
	std::string joint_name = "JOINTS_" + std::to_string(set);
	std::string weight_name = "WEIGHTS_" + std::to_string(set);
	int32_t joint_index = brlcad_gltf::find_attribute(primitive, joint_name.c_str());
	int32_t weight_index = brlcad_gltf::find_attribute(primitive, weight_name.c_str());
	if (joint_index == TG3_INDEX_NONE && weight_index == TG3_INDEX_NONE)
	    break;
	if (joint_index == TG3_INDEX_NONE || weight_index == TG3_INDEX_NONE)
	    return false;
	brlcad_gltf::accessor_view joints;
	brlcad_gltf::accessor_view weights;
	if (!brlcad_gltf::get_accessor_view(model, joint_index, joints) ||
	    !brlcad_gltf::get_accessor_view(model, weight_index, weights) ||
	    joints.accessor->type != TG3_TYPE_VEC4 ||
	    weights.accessor->type != TG3_TYPE_VEC4 ||
	    joints.accessor->count != vertex_count ||
	    weights.accessor->count != vertex_count)
	    return false;
	joint_sets.push_back(joints);
	weight_sets.push_back(weights);
    }
    if (joint_sets.empty())
	return false;

    std::vector<uint32_t> offsets(vertex_count + 1, 0);
    std::vector<uint32_t> joint_indices;
    std::vector<fastf_t> weights;
    for (size_t vertex = 0; vertex < vertex_count; ++vertex) {
	double sum = 0.0;
	for (size_t set = 0; set < joint_sets.size(); ++set) {
	    int32_t joint_component_size = tg3_component_size(
		joint_sets[set].accessor->component_type);
	    int32_t weight_component_size = tg3_component_size(
		weight_sets[set].accessor->component_type);
	    for (size_t component = 0; component < 4; ++component) {
		size_t joint;
		double weight;
		const uint8_t *joint_data = joint_sets[set].data +
		    vertex * joint_sets[set].stride + component * joint_component_size;
		const uint8_t *weight_data = weight_sets[set].data +
		    vertex * weight_sets[set].stride + component * weight_component_size;
		if (!brlcad_gltf::read_index(joint_data,
			joint_sets[set].accessor->component_type, joint) ||
		    !read_skin_weight(weight_data, *weight_sets[set].accessor, weight) ||
		    joint >= gltf_skin.joints_count || !std::isfinite(weight) || weight < 0.0)
		    return false;
		if (weight > 0.0) {
		    joint_indices.push_back(static_cast<uint32_t>(joint));
		    weights.push_back(static_cast<fastf_t>(weight));
		    sum += weight;
		}
	    }
	}
	if (!(sum > 0.0) || joint_indices.size() > UINT32_MAX)
	    return false;
	for (size_t influence = offsets[vertex]; influence < weights.size(); ++influence)
	    weights[influence] /= sum;
	offsets[vertex + 1] = static_cast<uint32_t>(joint_indices.size());
    }

    struct directory *directory = db_lookup(state->fd_out->dbip,
	bot_name.c_str(), LOOKUP_QUIET);
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (!directory || rt_db_get_internal(&internal, directory,
	    state->fd_out->dbip, nullptr) < 0 || internal.idb_type != ID_BOT) {
	rt_db_free_internal(&internal);
	return false;
    }
    auto *bot = static_cast<struct rt_bot_internal *>(internal.idb_ptr);
    BU_ALLOC(bot->skin, struct rt_bot_skin);
    bot->bot_flags |= RT_BOT_HAS_SKIN;
    bot->skin->joint_count = gltf_skin.joints_count;
    bot->skin->joint_links = static_cast<struct bu_vls *>(bu_calloc(
	bot->skin->joint_count, sizeof(struct bu_vls), "glTF skin joint links"));
    bot->skin->inverse_bind_matrices = static_cast<mat_t *>(bu_malloc(
	bot->skin->joint_count * sizeof(mat_t), "glTF inverse bind matrices"));
    for (size_t joint = 0; joint < bot->skin->joint_count; ++joint) {
	int32_t node_index = gltf_skin.joints[joint];
	if (node_index < 0 || static_cast<uint32_t>(node_index) >= model.nodes_count)
	    goto fail;
	std::string link = state->link_names[static_cast<size_t>(node_index)];
	bu_vls_init(&bot->skin->joint_links[joint]);
	bu_vls_strcpy(&bot->skin->joint_links[joint], link.c_str());
	MAT_IDN(bot->skin->inverse_bind_matrices[joint]);
    }
    if (gltf_skin.inverse_bind_matrices != TG3_INDEX_NONE) {
	brlcad_gltf::accessor_view matrices;
	if (!brlcad_gltf::get_accessor_view(model, gltf_skin.inverse_bind_matrices, matrices) ||
	    matrices.accessor->type != TG3_TYPE_MAT4 ||
	    matrices.accessor->count != bot->skin->joint_count ||
	    matrices.accessor->component_type != TG3_COMPONENT_TYPE_FLOAT)
	    goto fail;
	for (size_t joint = 0; joint < bot->skin->joint_count; ++joint) {
	    for (size_t row = 0; row < 4; ++row) {
		for (size_t column = 0; column < 4; ++column) {
		    double value;
		    const uint8_t *element = matrices.data + joint * matrices.stride +
			(column * 4 + row) * sizeof(float);
		    if (!brlcad_gltf::read_coordinate(element,
			    matrices.accessor->component_type, value))
			goto fail;
		    if (column == 3 && row < 3)
			value *= gltf_meters_to_millimeters;
		    bot->skin->inverse_bind_matrices[joint][row * 4 + column] = value;
		}
	    }
	}
    }
    bot->skin->influence_count = weights.size();
    bot->skin->vertex_offsets = static_cast<uint32_t *>(bu_malloc(
	offsets.size() * sizeof(uint32_t), "glTF skin vertex offsets"));
    bot->skin->joint_indices = static_cast<uint32_t *>(bu_malloc(
	joint_indices.size() * sizeof(uint32_t), "glTF skin joint indices"));
    bot->skin->weights = static_cast<fastf_t *>(bu_malloc(
	weights.size() * sizeof(fastf_t), "glTF skin weights"));
    memcpy(bot->skin->vertex_offsets, offsets.data(), offsets.size() * sizeof(uint32_t));
    memcpy(bot->skin->joint_indices, joint_indices.data(),
	joint_indices.size() * sizeof(uint32_t));
    memcpy(bot->skin->weights, weights.data(), weights.size() * sizeof(fastf_t));
    if (rt_bot_skin_validate(bot, nullptr) != BRLCAD_OK ||
	wdb_put_internal(state->fd_out, bot_name.c_str(), &internal, 1.0) < 0)
	goto fail;
    rt_db_free_internal(&internal);
    return true;

fail:
    rt_db_free_internal(&internal);
    return false;
}

static bool
generate_geometry(struct conversion_state *state, const tg3_model &model,
    int32_t mesh_number, int32_t skin_index, const std::string &shape_name,
    wmember &region)
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
		vertices[vertex * 3 + axis] = static_cast<fastf_t>(
		    coordinate * gltf_meters_to_millimeters);
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

	std::string bot_name = mesh.primitives_count == 1 ? shape_name :
	    shape_name + std::to_string(primitive_number);
	if (mk_bot(state->fd_out, bot_name.c_str(), RT_BOT_SOLID,
		RT_BOT_UNORIENTED, 0, vertex_count, index_count / 3,
		vertices.data(), faces.data(), NULL, NULL) < 0 ||
	    !attach_skin(state, model, primitive, skin_index, bot_name, vertex_count) ||
	    !mk_addmember(bot_name.c_str(), &region.l, NULL, WMOP_UNION)) {
	    bu_log("Unable to write glTF primitive %u\n", primitive_number);
	    return false;
	}
    }

    return true;
}


static bool
node_matrix(mat_t matrix, const tg3_node &node)
{
    if (node.has_matrix) {
	for (size_t row = 0; row < 4; ++row) {
	    for (size_t column = 0; column < 4; ++column) {
		double entry = node.matrix[column * 4 + row];
		if (!std::isfinite(entry))
		    return false;
		matrix[row * 4 + column] = static_cast<fastf_t>(entry);
	    }
	}
	matrix[MDX] *= gltf_meters_to_millimeters;
	matrix[MDY] *= gltf_meters_to_millimeters;
	matrix[MDZ] *= gltf_meters_to_millimeters;
	return true;
    }

    const double x = node.rotation[X];
    const double y = node.rotation[Y];
    const double z = node.rotation[Z];
    const double w = node.rotation[W];
    const double magnitude = std::sqrt(x * x + y * y + z * z + w * w);
    if (!std::isfinite(magnitude) || ZERO(magnitude))
	return false;
    const double qx = x / magnitude;
    const double qy = y / magnitude;
    const double qz = z / magnitude;
    const double qw = w / magnitude;
    const double sx = node.scale[X];
    const double sy = node.scale[Y];
    const double sz = node.scale[Z];
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sz) ||
	!std::isfinite(node.translation[X]) ||
	!std::isfinite(node.translation[Y]) ||
	!std::isfinite(node.translation[Z]))
	return false;

    MAT_IDN(matrix);
    matrix[0] = static_cast<fastf_t>((1.0 - 2.0 * (qy * qy + qz * qz)) * sx);
    matrix[1] = static_cast<fastf_t>(2.0 * (qx * qy - qz * qw) * sy);
    matrix[2] = static_cast<fastf_t>(2.0 * (qx * qz + qy * qw) * sz);
    matrix[4] = static_cast<fastf_t>(2.0 * (qx * qy + qz * qw) * sx);
    matrix[5] = static_cast<fastf_t>((1.0 - 2.0 * (qx * qx + qz * qz)) * sy);
    matrix[6] = static_cast<fastf_t>(2.0 * (qy * qz - qx * qw) * sz);
    matrix[8] = static_cast<fastf_t>(2.0 * (qx * qz - qy * qw) * sx);
    matrix[9] = static_cast<fastf_t>(2.0 * (qy * qz + qx * qw) * sy);
    matrix[10] = static_cast<fastf_t>((1.0 - 2.0 * (qx * qx + qy * qy)) * sz);
    matrix[MDX] = static_cast<fastf_t>(
	node.translation[X] * gltf_meters_to_millimeters);
    matrix[MDY] = static_cast<fastf_t>(
	node.translation[Y] * gltf_meters_to_millimeters);
    matrix[MDZ] = static_cast<fastf_t>(
	node.translation[Z] * gltf_meters_to_millimeters);
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
    if (!import_objects(state, node))
	return false;
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
	if (!generate_geometry(state, model, node.mesh, node.skin, shape_name, region))
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

    mat_t matrix;
    if (!node_matrix(matrix, node)) {
	bu_log("glTF node %d has an invalid transform\n", node_index);
	return false;
    }

    return mk_addmember(region_name.c_str(), &regions.l, matrix, WMOP_UNION) != NULL;
}

static bool
initialize_link_names(struct conversion_state *state, const tg3_model &model)
{
    std::set<std::string> used;

    state->link_names.resize(model.nodes_count);
    for (uint32_t node_index = 0; node_index < model.nodes_count; ++node_index) {
	std::string base = gltf_string(model.nodes[node_index].name);
	if (base.empty())
	    base = "node" + std::to_string(node_index);
	std::string candidate = base;
	size_t suffix = 1;
	while (!used.insert(candidate).second)
	    candidate = base + "_" + std::to_string(suffix++);
	state->link_names[node_index] = candidate;
    }
    state->world_link = "gltf_world";
    size_t suffix = 1;
    while (used.find(state->world_link) != used.end())
	state->world_link = "gltf_world_" + std::to_string(suffix++);
    return true;
}

static std::string
unique_object_name(struct conversion_state *state, const std::string &base_name)
{
    std::string base = base_name.empty() ? "object" : base_name;
    std::string candidate = base;
    size_t suffix = 1;
    while (state->object_names.find(candidate) != state->object_names.end() ||
	db_lookup(state->fd_out->dbip, candidate.c_str(), LOOKUP_QUIET))
	candidate = base + "_" + std::to_string(suffix++);
    return candidate;
}

static bool
import_standard_joint_graph(struct conversion_state *state, const tg3_model &model)
{
    if (state->has_native_joints)
	return true;
    std::vector<int32_t> parents(model.nodes_count, TG3_INDEX_NONE);
    std::vector<unsigned char> selected(model.nodes_count, 0);
    for (uint32_t parent = 0; parent < model.nodes_count; ++parent) {
	for (uint32_t child_index = 0;
	    child_index < model.nodes[parent].children_count; ++child_index) {
	    int32_t child = model.nodes[parent].children[child_index];
	    if (child < 0 || static_cast<uint32_t>(child) >= model.nodes_count ||
		parents[static_cast<size_t>(child)] != TG3_INDEX_NONE) {
		bu_log("glTF node hierarchy is not a tree at node %d\n", child);
		return false;
	    }
	    parents[static_cast<size_t>(child)] = static_cast<int32_t>(parent);
	}
    }
    for (uint32_t skin_index = 0; skin_index < model.skins_count; ++skin_index) {
	const tg3_skin &skin = model.skins[skin_index];
	for (uint32_t joint = 0; joint < skin.joints_count; ++joint) {
	    int32_t node = skin.joints[joint];
	    if (node < 0 || static_cast<uint32_t>(node) >= model.nodes_count)
		return false;
	    selected[static_cast<size_t>(node)] = 1;
	}
    }
    for (uint32_t animation_index = 0; animation_index < model.animations_count;
	++animation_index) {
	const tg3_animation &animation = model.animations[animation_index];
	for (uint32_t channel = 0; channel < animation.channels_count; ++channel) {
	    int32_t node = animation.channels[channel].target.node;
	    if (node < 0 || static_cast<uint32_t>(node) >= model.nodes_count)
		return false;
	    selected[static_cast<size_t>(node)] = 1;
	}
    }
    for (size_t node = 0; node < selected.size(); ++node) {
	if (!selected[node])
	    continue;
	int32_t ancestor = parents[node];
	for (size_t depth = 0; ancestor != TG3_INDEX_NONE; ++depth) {
	    if (depth >= model.nodes_count)
		return false;
	    selected[static_cast<size_t>(ancestor)] = 1;
	    ancestor = parents[static_cast<size_t>(ancestor)];
	}
    }
    for (size_t node_index = 0; node_index < selected.size(); ++node_index) {
	if (!selected[node_index])
	    continue;
	struct rt_db_internal internal;
	point_t origin = VINIT_ZERO;
	RT_DB_INTERNAL_INIT(&internal);
	if (OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], &internal, nullptr,
		origin, 1.0) != BRLCAD_OK)
	    return false;
	auto *joint = static_cast<struct rt_joint_internal *>(internal.idb_ptr);
	joint->type = RT_JOINT_FIXED;
	int32_t parent_index = parents[node_index];
	const std::string &parent_link = parent_index == TG3_INDEX_NONE ?
	    state->world_link : state->link_names[static_cast<size_t>(parent_index)];
	bu_vls_strcpy(&joint->parent_link, parent_link.c_str());
	bu_vls_strcpy(&joint->child_link, state->link_names[node_index].c_str());
	const tg3_node &node = model.nodes[node_index];
	if (node.has_matrix) {
	    if (!node_matrix(joint->parent_frame, node)) {
		rt_db_free_internal(&internal);
		return false;
	    }
	} else {
	    VSET(joint->pose_translation,
		node.translation[X] * gltf_meters_to_millimeters,
		node.translation[Y] * gltf_meters_to_millimeters,
		node.translation[Z] * gltf_meters_to_millimeters);
	    HSET(joint->pose_rotation, node.rotation[X], node.rotation[Y],
		node.rotation[Z], node.rotation[W]);
	    VSET(joint->pose_scale, node.scale[X], node.scale[Y], node.scale[Z]);
	}
	if (rt_joint_validate(joint, nullptr) != 0) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	std::string name = unique_object_name(state,
	    state->link_names[node_index] + ".joint");
	if (wdb_put_internal(state->fd_out, name.c_str(), &internal, 1.0) < 0) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	state->object_names.insert(name);
	rt_db_free_internal(&internal);
    }
    return true;
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

    if (!initialize_link_names(state, model))
	return false;
    const tg3_scene &scene = model.scenes[scene_index];
    std::set<int32_t> active_nodes;
    for (uint32_t i = 0; i < scene.nodes_count; ++i) {
	if (!handle_node(state, model, scene.nodes[i], state->scene, active_nodes))
	    return false;
    }
    return import_standard_joint_graph(state, model) &&
	import_animations(state, model);
}


static int
gltf_read(struct gcv_context *context, const struct gcv_opts *UNUSED(gcv_options),
    const void *UNUSED(options_data), const char *source_path)
{
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(source_path);
    tg3_error_code parse_result = reader.parse(model.get(), errors.get(), true);
    brlcad_gltf::log_errors(*errors.get());
    if (parse_result != TG3_OK) {
	bu_log("Failed to parse glTF input %s (error %d)\n", source_path,
	    static_cast<int>(parse_result));
	return 0;
    }

    struct conversion_state state = CONVERSION_STATE_ZERO;
    state.input_file = source_path;
    state.fd_out = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_DEFAULT);
    BU_LIST_INIT(&state.scene.l);

    std::string title = "glTF conversion from " + state.input_file;
    mk_id(state.fd_out, title.c_str());
    if (!convert_from_gltf(&state, *model.get())) {
	mk_freemembers(&state.scene.l);
	return 0;
    }

    if (!BU_LIST_IS_EMPTY(&state.scene.l) &&
	mk_lcomb(state.fd_out, "all", &state.scene, 0, NULL, NULL, NULL, 0) < 0)
	return 0;

    return 1;
}


static int
gltf_can_read(const char *path)
{
    if (!path)
	return 0;
    tinygltf3::Model model;
    tinygltf3::ErrorStack errors;
    brlcad_gltf::confined_file_reader reader(path);
    return reader.parse(model.get(), errors.get(), true) == TG3_OK;
}

extern "C"
{
    extern const struct gcv_filter gcv_conv_gltf_write;
    static const struct gcv_filter gcv_conv_gltf_read = {
	"GLTF Reader", GCV_FILTER_READ, BU_MIME_MODEL_GLTF, gltf_can_read,
	NULL, NULL, gltf_read
    };

    static const struct gcv_filter * const filters[] = {
	&gcv_conv_gltf_read, &gcv_conv_gltf_write, NULL
    };

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
