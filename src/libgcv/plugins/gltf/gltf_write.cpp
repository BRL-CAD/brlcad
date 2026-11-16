/*                  G L T F _ W R I T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <map>
#include <memory>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "bio.h"
#include "bu/file.h"
#include "gcv/api.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/object_io.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/joint.h"

#include "gltf_data.h"

namespace {

static const char *object_extras_name = "BRL-CAD_object";
static constexpr double millimeters_to_gltf_meters = 0.001;
static constexpr size_t gltf_components_per_vertex = 3;
static constexpr size_t gltf_indices_per_face = 3;
static constexpr size_t gltf_influences_per_set = 4;

struct joint_record {
    std::string name;
    struct rt_db_internal internal;

    joint_record(const char *object_name, struct rt_db_internal &source) : name(object_name)
    {
	internal = source;
	source.idb_ptr = nullptr;
    }

    ~joint_record() { rt_db_free_internal(&internal); }
    joint_record(const joint_record &) = delete;
    joint_record &operator=(const joint_record &) = delete;
};

struct bot_record {
    std::string name;
    struct rt_db_internal internal;

    bot_record(const char *object_name, struct rt_db_internal &source) : name(object_name)
    {
	internal = source;
	source.idb_ptr = nullptr;
    }

    ~bot_record() { rt_db_free_internal(&internal); }
    bot_record(const bot_record &) = delete;
    bot_record &operator=(const bot_record &) = delete;
};

struct animation_record {
    std::string name;
    struct rt_db_internal internal;

    animation_record(const char *object_name, struct rt_db_internal &source) : name(object_name)
    {
	internal = source;
	source.idb_ptr = nullptr;
    }

    ~animation_record() { rt_db_free_internal(&internal); }
    animation_record(const animation_record &) = delete;
    animation_record &operator=(const animation_record &) = delete;
};

struct position_bounds {
    std::array<double, gltf_components_per_vertex> minimum;
    std::array<double, gltf_components_per_vertex> maximum;
};

struct scalar_bounds {
    std::array<double, 1> minimum;
    std::array<double, 1> maximum;
};

struct mesh_record {
    std::string name;
    std::deque<std::string> attribute_names;
    std::vector<tg3_str_int_pair> attributes;
    tg3_primitive primitive = {};
    tg3_mesh mesh = {};

    void add_attribute(const std::string &attribute_name, int32_t accessor)
    {
	attribute_names.push_back(attribute_name);
	const std::string &stored = attribute_names.back();
	tg3_str_int_pair attribute = {};
	attribute.key = {stored.c_str(), static_cast<uint32_t>(stored.size())};
	attribute.value = accessor;
	attributes.push_back(attribute);
    }

    void finish()
    {
	mesh.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	primitive.attributes = attributes.data();
	primitive.attributes_count = static_cast<uint32_t>(attributes.size());
	mesh.primitives = &primitive;
	mesh.primitives_count = 1;
    }
};

struct skin_output {
    std::string name;
    std::vector<int32_t> joints;
    tg3_skin skin = {};

    void finish()
    {
	skin.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	skin.joints = joints.data();
	skin.joints_count = static_cast<uint32_t>(joints.size());
    }
};

struct animation_output {
    std::string name;
    std::deque<std::string> paths;
    std::deque<std::string> interpolations;
    std::vector<tg3_animation_channel> channels;
    std::vector<tg3_animation_sampler> samplers;
    std::unique_ptr<brlcad_gltf::data_value> extras;
    tg3_animation animation = {};

    void finish()
    {
	for (size_t index = 0; index < channels.size(); ++index) {
	    channels[index].target.path = {paths[index].c_str(),
		static_cast<uint32_t>(paths[index].size())};
	    samplers[index].interpolation = {interpolations[index].c_str(),
		static_cast<uint32_t>(interpolations[index].size())};
	}
	animation.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	animation.channels = channels.data();
	animation.channels_count = static_cast<uint32_t>(channels.size());
	animation.samplers = samplers.data();
	animation.samplers_count = static_cast<uint32_t>(samplers.size());
	animation.ext.extras = extras ? extras->get() : nullptr;
    }
};

struct gltf_resources {
    std::vector<uint8_t> bytes;
    tg3_buffer buffer = {};
    std::vector<tg3_buffer_view> views;
    std::vector<tg3_accessor> accessors;
    std::deque<position_bounds> bounds;
    std::deque<scalar_bounds> scalar_limits;
    std::vector<mesh_record> meshes;
    std::vector<skin_output> skins;
    std::vector<animation_output> animations;
};

struct node_record {
    std::string name;
    std::vector<int32_t> children;
    std::unique_ptr<brlcad_gltf::data_value> extras;
    int32_t mesh = TG3_INDEX_NONE;
    int32_t skin = TG3_INDEX_NONE;
    tg3_node node = {};

    void finish()
    {
	node.name.data = name.c_str();
	node.name.len = static_cast<uint32_t>(name.size());
	node.camera = TG3_INDEX_NONE;
	node.skin = skin;
	node.mesh = mesh;
	node.light = TG3_INDEX_NONE;
	node.emitter = TG3_INDEX_NONE;
	node.children = children.empty() ? nullptr : children.data();
	node.children_count = static_cast<uint32_t>(children.size());
	node.rotation[W] = 1.0;
	node.scale[X] = 1.0;
	node.scale[Y] = 1.0;
	node.scale[Z] = 1.0;
	node.ext.extras = extras ? extras->get() : nullptr;
    }
};

static bool
load_joints(struct db_i *dbip, std::vector<std::unique_ptr<joint_record>> &records)
{
    std::vector<struct directory *> directories;
    struct directory *directory;

    FOR_ALL_DIRECTORY_START(directory, dbip) {
	if (directory->d_major_type == DB5_MAJORTYPE_BRLCAD &&
	    directory->d_minor_type == ID_JOINT)
	    directories.push_back(directory);
    } FOR_ALL_DIRECTORY_END;
    std::sort(directories.begin(), directories.end(),
	[](const struct directory *first, const struct directory *second) {
	    return std::string(first->d_namep) < std::string(second->d_namep);
	});
    records.reserve(directories.size());
    for (struct directory *entry : directories) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, dbip, nullptr) < 0 ||
	    internal.idb_type != ID_JOINT) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	records.emplace_back(new joint_record(entry->d_namep, internal));
    }
    return true;
}

static bool
load_skinned_bots(struct db_i *dbip, std::vector<std::unique_ptr<bot_record>> &records)
{
    std::vector<struct directory *> directories;
    struct directory *directory;

    FOR_ALL_DIRECTORY_START(directory, dbip) {
	if (directory->d_major_type == DB5_MAJORTYPE_BRLCAD &&
	    directory->d_minor_type == ID_BOT)
	    directories.push_back(directory);
    } FOR_ALL_DIRECTORY_END;
    std::sort(directories.begin(), directories.end(),
	[](const struct directory *first, const struct directory *second) {
	    return std::string(first->d_namep) < std::string(second->d_namep);
	});
    records.reserve(directories.size());
    for (struct directory *entry : directories) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, dbip, nullptr) < 0 ||
	    internal.idb_type != ID_BOT) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	const auto *bot = static_cast<const struct rt_bot_internal *>(internal.idb_ptr);
	if (!bot->skin) {
	    rt_db_free_internal(&internal);
	    continue;
	}
	if (rt_bot_skin_validate(bot, nullptr) != BRLCAD_OK) {
	    bu_log("glTF mesh '%s' has an invalid skin binding\n", entry->d_namep);
	    rt_db_free_internal(&internal);
	    return false;
	}
	records.emplace_back(new bot_record(entry->d_namep, internal));
    }
    return true;
}

static bool
load_animations(struct db_i *dbip, std::vector<std::unique_ptr<animation_record>> &records)
{
    std::vector<struct directory *> directories;
    struct directory *directory;

    FOR_ALL_DIRECTORY_START(directory, dbip) {
	if (directory->d_major_type == DB5_MAJORTYPE_BRLCAD &&
	    directory->d_minor_type == ID_ANIMATION)
	    directories.push_back(directory);
    } FOR_ALL_DIRECTORY_END;
    std::sort(directories.begin(), directories.end(),
	[](const struct directory *first, const struct directory *second) {
	    return std::string(first->d_namep) < std::string(second->d_namep);
	});
    records.reserve(directories.size());
    for (struct directory *entry : directories) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry, dbip, nullptr) < 0 ||
	    internal.idb_type != ID_ANIMATION) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	records.emplace_back(new animation_record(entry->d_namep, internal));
    }
    return true;
}

static std::unique_ptr<brlcad_gltf::data_value>
object_extras(const std::string &object_name, const struct rt_db_internal &internal,
    const struct rt_data_context &context)
{
    struct rt_data_node *joint_data = nullptr;
    struct rt_data_node *envelope = rt_data_object();
    struct rt_data_node *description = rt_data_object();
    struct rt_data_node *name = rt_data_string(object_name.c_str());

    if (!envelope || !description || !name ||
	rt_obj_data_export(&joint_data, &internal, &context) != BRLCAD_OK)
	goto fail;
    if (rt_data_object_set(description, "name", name) != BRLCAD_OK)
	goto fail;
    name = nullptr;
    if (rt_data_object_set(description, "data", joint_data) != BRLCAD_OK)
	goto fail;
    joint_data = nullptr;
    if (rt_data_object_set(envelope, object_extras_name, description) != BRLCAD_OK)
	goto fail;
    description = nullptr;
    {
	std::unique_ptr<brlcad_gltf::data_value> result =
	    brlcad_gltf::data_value::create(envelope);
	rt_data_free(envelope);
	return result;
    }

fail:
	rt_data_free(name);
	rt_data_free(joint_data);
	rt_data_free(description);
	rt_data_free(envelope);
	return nullptr;
}

static void
matrix_to_gltf(double output[16], const mat_t input)
{
    for (size_t row = 0; row < 4; ++row) {
	for (size_t column = 0; column < 4; ++column)
	    output[column * 4 + row] = input[row * 4 + column];
    }
    output[12] *= millimeters_to_gltf_meters;
    output[13] *= millimeters_to_gltf_meters;
    output[14] *= millimeters_to_gltf_meters;
}

static bool
append_buffer_data(gltf_resources &resources, const void *data, size_t byte_count,
    size_t alignment, uint64_t &offset)
{
    const size_t max_buffer_size = std::numeric_limits<uint32_t>::max();
    if ((!data && byte_count) || !alignment || resources.bytes.size() > max_buffer_size)
	return false;
    size_t remainder = resources.bytes.size() % alignment;
    size_t padding = remainder ? alignment - remainder : 0;
    if (padding > max_buffer_size - resources.bytes.size() ||
	byte_count > max_buffer_size - resources.bytes.size() - padding)
	return false;
    resources.bytes.resize(resources.bytes.size() + padding, 0);
    offset = resources.bytes.size();
    const auto *bytes = static_cast<const uint8_t *>(data);
    resources.bytes.insert(resources.bytes.end(), bytes, bytes + byte_count);
    return true;
}

static bool
add_accessor(gltf_resources &resources, const void *data, size_t byte_count,
    size_t alignment, int32_t target, int32_t component_type, uint64_t count,
    int32_t type, int32_t &accessor_index)
{
    uint64_t offset;
    if (!append_buffer_data(resources, data, byte_count, alignment, offset) ||
	resources.views.size() >= static_cast<size_t>(INT32_MAX) ||
	resources.accessors.size() >= static_cast<size_t>(INT32_MAX))
	return false;
    tg3_buffer_view view = {};
    view.buffer = 0;
    view.byte_offset = offset;
    view.byte_length = byte_count;
    view.target = target;
    resources.views.push_back(view);
    tg3_accessor accessor = {};
    accessor.buffer_view = static_cast<int32_t>(resources.views.size() - 1);
    accessor.component_type = component_type;
    accessor.count = count;
    accessor.type = type;
    resources.accessors.push_back(accessor);
    accessor_index = static_cast<int32_t>(resources.accessors.size() - 1);
    return true;
}

static bool
build_animations(const std::vector<std::unique_ptr<animation_record>> &records,
    const std::map<std::string, int32_t> &node_indices,
    const struct rt_data_context &context, gltf_resources &resources)
{
    resources.animations.reserve(records.size());
    for (const auto &record : records) {
	const auto *animation = static_cast<const struct rt_animation_internal *>(
	    record->internal.idb_ptr);
	if (rt_animation_validate(animation, nullptr) != 0)
	    return false;
	animation_output output;
	output.name = record->name;
	output.extras = object_extras(record->name, record->internal, context);
	if (!output.extras)
	    return false;
	output.channels.reserve(animation->channel_count);
	output.samplers.reserve(animation->channel_count);
	for (size_t channel_index = 0; channel_index < animation->channel_count;
	    ++channel_index) {
	    const struct rt_animation_channel &channel =
		animation->channels[channel_index];
	    auto node = node_indices.find(bu_vls_cstr(&channel.target_link));
	    const char *path = rt_animation_target_name(channel.target);
	    const char *interpolation =
		rt_animation_interpolation_name(channel.interpolation);
	    if (node == node_indices.end() || !path || !interpolation)
		return false;

	    std::vector<float> times(channel.keyframe_count);
	    for (size_t key = 0; key < channel.keyframe_count; ++key) {
		double value = channel.times[key];
		if (!std::isfinite(value) || value > std::numeric_limits<float>::max())
		    return false;
		times[key] = static_cast<float>(value);
	    }
	    size_t value_count = rt_animation_channel_value_count(&channel);
	    std::vector<float> values(value_count);
	    for (size_t value_index = 0; value_index < value_count; ++value_index) {
		double value = channel.values[value_index];
		if (channel.target == RT_ANIMATION_TRANSLATION)
		    value *= millimeters_to_gltf_meters;
		if (!std::isfinite(value) ||
		    std::fabs(value) > std::numeric_limits<float>::max())
		    return false;
		values[value_index] = static_cast<float>(value);
	    }

	    int32_t input_accessor;
	    int32_t output_accessor;
	    int32_t value_type = channel.target == RT_ANIMATION_ROTATION ?
		TG3_TYPE_VEC4 : TG3_TYPE_VEC3;
	    if (!add_accessor(resources, times.data(), times.size() * sizeof(float),
		    sizeof(float), 0, TG3_COMPONENT_TYPE_FLOAT, times.size(),
		    TG3_TYPE_SCALAR, input_accessor) ||
		!add_accessor(resources, values.data(), values.size() * sizeof(float),
		    sizeof(float), 0, TG3_COMPONENT_TYPE_FLOAT,
		    channel.interpolation == RT_ANIMATION_CUBICSPLINE ?
			channel.keyframe_count * 3 : channel.keyframe_count,
		    value_type, output_accessor))
		return false;
	    scalar_bounds limits = {{{times.front()}}, {{times.back()}}};
	    resources.scalar_limits.push_back(limits);
	    tg3_accessor &input = resources.accessors[static_cast<size_t>(input_accessor)];
	    input.min_values = resources.scalar_limits.back().minimum.data();
	    input.min_values_count = 1;
	    input.max_values = resources.scalar_limits.back().maximum.data();
	    input.max_values_count = 1;

	    output.paths.emplace_back(path);
	    output.interpolations.emplace_back(interpolation);
	    tg3_animation_sampler sampler = {};
	    sampler.input = input_accessor;
	    sampler.output = output_accessor;
	    output.samplers.push_back(sampler);
	    tg3_animation_channel gltf_channel = {};
	    gltf_channel.sampler = static_cast<int32_t>(output.samplers.size() - 1);
	    gltf_channel.target.node = node->second;
	    output.channels.push_back(gltf_channel);
	}
	resources.animations.push_back(std::move(output));
    }
    for (animation_output &animation : resources.animations)
	animation.finish();
    return true;
}

static bool
add_skin_mesh(gltf_resources &resources, const struct rt_bot_internal *bot,
    const std::string &name,
    int32_t &mesh_index, int32_t &inverse_bind_accessor)
{
    const struct rt_bot_skin *skin = bot ? bot->skin : nullptr;
    if (!bot || !skin || !bot->num_vertices || !bot->num_faces ||
	bot->num_vertices > UINT32_MAX || bot->num_faces > UINT32_MAX ||
	skin->joint_count > UINT16_MAX)
	return false;
    std::vector<float> positions(bot->num_vertices * gltf_components_per_vertex);
    position_bounds bounds;
    for (size_t vertex = 0; vertex < bot->num_vertices; vertex++) {
	for (size_t axis = 0; axis < gltf_components_per_vertex; axis++) {
	    double coordinate = bot->vertices[vertex * gltf_components_per_vertex + axis] *
		millimeters_to_gltf_meters;
	    if (!std::isfinite(coordinate) ||
		std::fabs(coordinate) > std::numeric_limits<float>::max())
		return false;
	    float stored = static_cast<float>(coordinate);
	    positions[vertex * gltf_components_per_vertex + axis] = stored;
	    if (!vertex)
		bounds.minimum[axis] = bounds.maximum[axis] = stored;
	    else {
		bounds.minimum[axis] = std::min(bounds.minimum[axis],
		    static_cast<double>(stored));
		bounds.maximum[axis] = std::max(bounds.maximum[axis],
		    static_cast<double>(stored));
	    }
	}
    }
    int32_t position_accessor;
    if (!add_accessor(resources, positions.data(), positions.size() * sizeof(float),
	    sizeof(float), TG3_TARGET_ARRAY_BUFFER, TG3_COMPONENT_TYPE_FLOAT,
	    bot->num_vertices, TG3_TYPE_VEC3, position_accessor))
	return false;
    resources.bounds.push_back(bounds);
    resources.accessors[static_cast<size_t>(position_accessor)].min_values =
	resources.bounds.back().minimum.data();
    resources.accessors[static_cast<size_t>(position_accessor)].min_values_count =
	gltf_components_per_vertex;
    resources.accessors[static_cast<size_t>(position_accessor)].max_values =
	resources.bounds.back().maximum.data();
    resources.accessors[static_cast<size_t>(position_accessor)].max_values_count =
	gltf_components_per_vertex;

    if (bot->num_faces > SIZE_MAX / gltf_indices_per_face)
	return false;
    size_t index_count = bot->num_faces * gltf_indices_per_face;
    std::vector<uint32_t> indices(index_count);
    for (size_t index = 0; index < index_count; index++) {
	if (bot->faces[index] < 0 ||
	    static_cast<size_t>(bot->faces[index]) >= bot->num_vertices)
	    return false;
	indices[index] = static_cast<uint32_t>(bot->faces[index]);
    }
    int32_t index_accessor;
    if (!add_accessor(resources, indices.data(), indices.size() * sizeof(uint32_t),
	    sizeof(uint32_t), TG3_TARGET_ELEMENT_ARRAY_BUFFER,
	    TG3_COMPONENT_TYPE_UNSIGNED_INT, index_count, TG3_TYPE_SCALAR,
	    index_accessor))
	return false;

    mesh_record mesh;
    mesh.name = name;
    mesh.primitive.indices = index_accessor;
    mesh.primitive.material = TG3_INDEX_NONE;
    mesh.primitive.mode = TG3_MODE_TRIANGLES;
    mesh.add_attribute("POSITION", position_accessor);
    size_t max_influences = 0;
    for (size_t vertex = 0; vertex < bot->num_vertices; vertex++) {
	max_influences = std::max(max_influences,
	    static_cast<size_t>(skin->vertex_offsets[vertex + 1] -
		skin->vertex_offsets[vertex]));
    }
    size_t influence_sets = (max_influences + gltf_influences_per_set - 1) /
	gltf_influences_per_set;
    for (size_t set = 0; set < influence_sets; set++) {
	std::vector<uint16_t> joint_values(bot->num_vertices * gltf_influences_per_set, 0);
	std::vector<float> weight_values(bot->num_vertices * gltf_influences_per_set, 0.0f);
	for (size_t vertex = 0; vertex < bot->num_vertices; vertex++) {
	    uint32_t first = skin->vertex_offsets[vertex];
	    uint32_t end = skin->vertex_offsets[vertex + 1];
	    for (size_t component = 0; component < gltf_influences_per_set; component++) {
		size_t influence = first + set * gltf_influences_per_set + component;
		if (influence >= end)
		    break;
		joint_values[vertex * gltf_influences_per_set + component] =
		    static_cast<uint16_t>(skin->joint_indices[influence]);
		weight_values[vertex * gltf_influences_per_set + component] =
		    static_cast<float>(skin->weights[influence]);
	    }
	}
	int32_t joint_accessor;
	int32_t weight_accessor;
	if (!add_accessor(resources, joint_values.data(),
		joint_values.size() * sizeof(uint16_t), sizeof(uint16_t),
		TG3_TARGET_ARRAY_BUFFER, TG3_COMPONENT_TYPE_UNSIGNED_SHORT,
		bot->num_vertices, TG3_TYPE_VEC4, joint_accessor) ||
	    !add_accessor(resources, weight_values.data(),
		weight_values.size() * sizeof(float), sizeof(float),
		TG3_TARGET_ARRAY_BUFFER, TG3_COMPONENT_TYPE_FLOAT,
		bot->num_vertices, TG3_TYPE_VEC4, weight_accessor))
	    return false;
	mesh.add_attribute("JOINTS_" + std::to_string(set), joint_accessor);
	mesh.add_attribute("WEIGHTS_" + std::to_string(set), weight_accessor);
    }
    if (resources.meshes.size() >= static_cast<size_t>(INT32_MAX))
	return false;
    resources.meshes.push_back(std::move(mesh));
    mesh_index = static_cast<int32_t>(resources.meshes.size() - 1);

    std::vector<float> inverse_binds(skin->joint_count * ELEMENTS_PER_MAT);
    for (size_t joint = 0; joint < skin->joint_count; joint++) {
	for (size_t row = 0; row < 4; row++) {
	    for (size_t column = 0; column < 4; column++) {
		double value = skin->inverse_bind_matrices[joint][row * 4 + column];
		if (column == 3 && row < 3)
		    value *= millimeters_to_gltf_meters;
		if (!std::isfinite(value) ||
		    std::fabs(value) > std::numeric_limits<float>::max())
		    return false;
		inverse_binds[joint * ELEMENTS_PER_MAT + column * 4 + row] =
		    static_cast<float>(value);
	    }
	}
    }
    return add_accessor(resources, inverse_binds.data(),
	inverse_binds.size() * sizeof(float), sizeof(float), 0,
	TG3_COMPONENT_TYPE_FLOAT, skin->joint_count, TG3_TYPE_MAT4,
	inverse_bind_accessor);
}

static bool
visit_node(size_t index, const std::vector<node_record> &nodes, std::vector<unsigned char> &state)
{
    if (state[index] == 1)
	return false;
    if (state[index] == 2)
	return true;
    state[index] = 1;
    for (int32_t child : nodes[index].children) {
	if (child < 0 || static_cast<size_t>(child) >= nodes.size() ||
	    !visit_node(static_cast<size_t>(child), nodes, state))
	    return false;
    }
    state[index] = 2;
    return true;
}

static bool
build_nodes(struct db_i *dbip, const std::vector<std::unique_ptr<joint_record>> &joints,
    const std::vector<std::unique_ptr<bot_record>> &bots,
    std::vector<node_record> &nodes, std::vector<int32_t> &roots,
    std::map<std::string, int32_t> &indices, gltf_resources &resources)
{
    std::map<std::string, size_t> incoming;
    struct rt_data_context context = RT_DATA_CONTEXT_INIT;

    for (const auto &record : joints) {
	const auto *joint = static_cast<const struct rt_joint_internal *>(record->internal.idb_ptr);
	RT_JOINT_CK_MAGIC(joint);
	if (rt_joint_validate(joint, nullptr) != 0)
	    return false;
	indices.emplace(bu_vls_cstr(&joint->parent_link), 0);
	indices.emplace(bu_vls_cstr(&joint->child_link), 0);
    }
    if (indices.empty())
	return false;
    nodes.resize(indices.size());
    int32_t next_index = 0;
    for (auto &entry : indices) {
	entry.second = next_index;
	nodes[static_cast<size_t>(next_index)].name = entry.first;
	++next_index;
    }
    context.media_type = "model/gltf+json";
    context.dbip = dbip;
    context.local2base = dbip->dbi_local2base;
    context.base2local = dbip->dbi_base2local;
    for (const auto &record : joints) {
	const auto *joint = static_cast<const struct rt_joint_internal *>(record->internal.idb_ptr);
	const std::string parent = bu_vls_cstr(&joint->parent_link);
	const std::string child = bu_vls_cstr(&joint->child_link);
	const int32_t parent_index = indices[parent];
	const int32_t child_index = indices[child];
	mat_t relative;
	if (parent == child || incoming.find(child) != incoming.end() ||
	    rt_joint_relative_mat(relative, joint) != BRLCAD_OK)
	    return false;
	incoming[child] = static_cast<size_t>(parent_index);
	nodes[static_cast<size_t>(parent_index)].children.push_back(child_index);
	node_record &child_node = nodes[static_cast<size_t>(child_index)];
	child_node.node.has_matrix = 1;
	matrix_to_gltf(child_node.node.matrix, relative);
    child_node.extras = object_extras(record->name, record->internal, context);
	if (!child_node.extras)
	    return false;
    }
    for (node_record &node : nodes)
	std::sort(node.children.begin(), node.children.end());
    for (size_t i = 0; i < nodes.size(); ++i) {
	if (incoming.find(nodes[i].name) == incoming.end())
	    roots.push_back(static_cast<int32_t>(i));
    }
    std::vector<unsigned char> state(nodes.size(), 0);
    for (int32_t root : roots) {
	if (!visit_node(static_cast<size_t>(root), nodes, state))
	    return false;
    }
    if (std::find(state.begin(), state.end(), 0) != state.end())
	return false;
    for (const auto &record : bots) {
	const auto *bot = static_cast<const struct rt_bot_internal *>(record->internal.idb_ptr);
	const struct rt_bot_skin *skin = bot->skin;
	int32_t mesh_index;
	int32_t inverse_bind_accessor;
	if (!add_skin_mesh(resources, bot, record->name,
		mesh_index, inverse_bind_accessor))
	    return false;
	skin_output output;
	output.name = record->name;
	output.skin.inverse_bind_matrices = inverse_bind_accessor;
	output.skin.skeleton = TG3_INDEX_NONE;
	output.joints.reserve(skin->joint_count);
	for (size_t joint = 0; joint < skin->joint_count; joint++) {
	    auto link = indices.find(bu_vls_cstr(&skin->joint_links[joint]));
	    if (link == indices.end()) {
		bu_log("glTF skin '%s' refers to unknown link '%s'\n",
		    record->name.c_str(), bu_vls_cstr(&skin->joint_links[joint]));
		return false;
	    }
	    output.joints.push_back(link->second);
	}
	if (resources.skins.size() >= static_cast<size_t>(INT32_MAX) ||
	    nodes.size() >= static_cast<size_t>(INT32_MAX))
	    return false;
	resources.skins.push_back(std::move(output));
	node_record mesh_node;
	mesh_node.name = record->name;
	mesh_node.mesh = mesh_index;
	mesh_node.skin = static_cast<int32_t>(resources.skins.size() - 1);
	nodes.push_back(std::move(mesh_node));
	roots.push_back(static_cast<int32_t>(nodes.size() - 1));
    }
    for (mesh_record &mesh : resources.meshes)
	mesh.finish();
    for (skin_output &skin : resources.skins)
	skin.finish();
    for (node_record &node : nodes)
	node.finish();
    return true;
}

static int32_t
write_chunk(const uint8_t *data, uint64_t size, void *user_data)
{
    FILE *stream = static_cast<FILE *>(user_data);
    if (!stream || size > std::numeric_limits<size_t>::max())
	return 0;
    return fwrite(data, 1, static_cast<size_t>(size), stream) == size;
}

static int
gltf_write(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(options_data), const char *path)
{
    std::vector<std::unique_ptr<joint_record>> joints;
    std::vector<std::unique_ptr<bot_record>> bots;
    std::vector<std::unique_ptr<animation_record>> animations;
    std::vector<node_record> nodes;
    std::vector<int32_t> roots;
    std::map<std::string, int32_t> node_indices;
    gltf_resources resources;
    struct rt_data_context data_context = RT_DATA_CONTEXT_INIT;
    tg3_write_options write_options;
    tg3_asset asset = {};
    tg3_scene scene = {};
    tg3_writer *writer = nullptr;
    tg3_error_code error = TG3_OK;
    FILE *stream = nullptr;

    if (!context || !context->dbip || !path || !load_joints(context->dbip, joints) ||
	!load_skinned_bots(context->dbip, bots) ||
	!load_animations(context->dbip, animations) ||
	!build_nodes(context->dbip, joints, bots, nodes, roots, node_indices, resources)) {
	bu_log("glTF articulation export requires a valid persistent joint graph\n");
	return 0;
    }
    data_context.media_type = "model/gltf+json";
    data_context.dbip = context->dbip;
    data_context.local2base = context->dbip->dbi_local2base;
    data_context.base2local = context->dbip->dbi_base2local;
    if (!build_animations(animations, node_indices, data_context, resources)) {
	bu_log("Unable to project animation channels into glTF\n");
	return 0;
    }
    stream = fopen(path, "wb");
    if (!stream)
	return 0;
    tg3_write_options_init(&write_options);
    write_options.pretty_print = 1;
    write_options.embed_buffers = 1;
    asset.version = {"2.0", 3};
    asset.generator = {"BRL-CAD libgcv", 14};
    scene.name = {"BRL-CAD articulation", 20};
    scene.nodes = roots.data();
    scene.nodes_count = static_cast<uint32_t>(roots.size());
    writer = tg3_writer_create(write_chunk, stream, &write_options);
    if (!writer)
	error = TG3_ERR_OUT_OF_MEMORY;
    if (error == TG3_OK)
	error = tg3_writer_begin(writer, &asset);
    if (error == TG3_OK && !resources.bytes.empty()) {
	resources.buffer.byte_length = resources.bytes.size();
	resources.buffer.data = {resources.bytes.data(), resources.bytes.size()};
	error = tg3_writer_add_buffer(writer, &resources.buffer);
    }
    for (const tg3_buffer_view &view : resources.views) {
	if (error == TG3_OK)
	    error = tg3_writer_add_buffer_view(writer, &view);
    }
    for (const tg3_accessor &accessor : resources.accessors) {
	if (error == TG3_OK)
	    error = tg3_writer_add_accessor(writer, &accessor);
    }
    for (const mesh_record &mesh : resources.meshes) {
	if (error == TG3_OK)
	    error = tg3_writer_add_mesh(writer, &mesh.mesh);
    }
    for (const node_record &node : nodes) {
	if (error == TG3_OK)
	    error = tg3_writer_add_node(writer, &node.node);
    }
    for (const skin_output &skin : resources.skins) {
	if (error == TG3_OK)
	    error = tg3_writer_add_skin(writer, &skin.skin);
    }
    for (const animation_output &animation : resources.animations) {
	if (error == TG3_OK)
	    error = tg3_writer_add_animation(writer, &animation.animation);
    }
    if (error == TG3_OK)
	error = tg3_writer_add_scene(writer, &scene);
    if (error == TG3_OK)
	error = tg3_writer_end(writer);
    tg3_writer_destroy(writer);
    if (fclose(stream) != 0)
	error = TG3_ERR_WRITE_FAILED;
    if (error != TG3_OK) {
	bu_log("Unable to write glTF articulation data (error %d)\n", static_cast<int>(error));
	bu_file_delete(path);
	return 0;
    }
    return 1;
}

} // namespace

extern "C" const struct gcv_filter gcv_conv_gltf_write = {
    "GLTF Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_GLTF, nullptr,
    nullptr, nullptr, gltf_write
};

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

