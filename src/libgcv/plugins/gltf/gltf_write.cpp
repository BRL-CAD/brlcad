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
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
static constexpr size_t gltf_normal_components = 3;
static constexpr size_t gltf_uv_components = 2;
static constexpr size_t bot_uv_components = 3;

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
    std::deque<std::vector<tg3_str_int_pair>> attributes;
    std::vector<tg3_primitive> primitives;
    tg3_mesh mesh = {};

    void add_primitive()
    {
	attributes.emplace_back();
	primitives.emplace_back();
	primitives.back().material = TG3_INDEX_NONE;
	primitives.back().mode = TG3_MODE_TRIANGLES;
    }

    void add_attribute(const std::string &attribute_name, int32_t accessor)
    {
	attribute_names.push_back(attribute_name);
	const std::string &stored = attribute_names.back();
	tg3_str_int_pair attribute = {};
	attribute.key = {stored.c_str(), static_cast<uint32_t>(stored.size())};
	attribute.value = accessor;
	attributes.back().push_back(attribute);
    }

    void finish()
    {
	for (size_t index = 0; index < primitives.size(); ++index) {
	    primitives[index].attributes = attributes[index].data();
	    primitives[index].attributes_count =
		static_cast<uint32_t>(attributes[index].size());
	}
	mesh.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	mesh.primitives = primitives.data();
	mesh.primitives_count = static_cast<uint32_t>(primitives.size());
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

struct material_output {
    std::string name;
    std::string alpha_mode;
    tg3_material material = {};

    void finish()
    {
	material.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	material.alpha_mode = {alpha_mode.c_str(),
	    static_cast<uint32_t>(alpha_mode.size())};
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
    std::vector<material_output> materials;
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

struct scene_record {
    std::string name;
    std::vector<int32_t> roots;
    tg3_scene scene = {};

    void finish()
    {
	scene.name = {name.c_str(), static_cast<uint32_t>(name.size())};
	scene.nodes = roots.empty() ? nullptr : roots.data();
	scene.nodes_count = static_cast<uint32_t>(roots.size());
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
load_bots(struct db_i *dbip, std::vector<std::unique_ptr<bot_record>> &records)
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
	if (bot->skin && rt_bot_skin_validate(bot, nullptr) != BRLCAD_OK) {
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
		if (!std::isfinite(value) || value < 0.0 ||
		    value > std::numeric_limits<float>::max())
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

static bool attribute_value(struct db_i *dbip, struct directory *directory,
    const char *name, std::string &value);

static bool
add_bot_primitive(gltf_resources &resources, const struct rt_bot_internal *bot,
    mesh_record &mesh, int32_t material_index)
{
    const struct rt_bot_skin *skin = bot ? bot->skin : nullptr;
    if (!bot || !bot->num_vertices || !bot->num_faces ||
	bot->num_vertices > UINT32_MAX || bot->num_faces > UINT32_MAX ||
	(skin && skin->joint_count > UINT16_MAX))
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

    mesh.add_primitive();
    mesh.primitives.back().indices = index_accessor;
    mesh.primitives.back().material = material_index;
    mesh.add_attribute("POSITION", position_accessor);
    if (bot->bot_flags & RT_BOT_HAS_SURFACE_NORMALS) {
	if (!bot->normals || !bot->face_normals ||
	    bot->num_normals != bot->num_vertices ||
	    bot->num_face_normals != bot->num_faces)
	    return false;
	for (size_t index = 0; index < index_count; ++index) {
	    if (bot->face_normals[index] != bot->faces[index])
		return false;
	}
	std::vector<float> normals(bot->num_vertices * gltf_normal_components);
	for (size_t component = 0; component < normals.size(); ++component) {
	    double value = bot->normals[component];
	    if (!std::isfinite(value) ||
		std::fabs(value) > std::numeric_limits<float>::max())
		return false;
	    normals[component] = static_cast<float>(value);
	}
	int32_t normal_accessor;
	if (!add_accessor(resources, normals.data(), normals.size() * sizeof(float),
		sizeof(float), TG3_TARGET_ARRAY_BUFFER, TG3_COMPONENT_TYPE_FLOAT,
		bot->num_vertices, TG3_TYPE_VEC3, normal_accessor))
	    return false;
	mesh.add_attribute("NORMAL", normal_accessor);
    }
    if (bot->bot_flags & RT_BOT_HAS_TEXTURE_UVS) {
	if (!bot->uvs || !bot->face_uvs || bot->num_uvs != bot->num_vertices ||
	    bot->num_face_uvs != bot->num_faces)
	    return false;
	for (size_t index = 0; index < index_count; ++index) {
	    if (bot->face_uvs[index] != bot->faces[index])
		return false;
	}
	std::vector<float> uvs(bot->num_vertices * gltf_uv_components);
	for (size_t vertex = 0; vertex < bot->num_vertices; ++vertex) {
	    for (size_t component = 0; component < gltf_uv_components;
		component++) {
		double value = bot->uvs[vertex * bot_uv_components + component];
		if (!std::isfinite(value) ||
		    std::fabs(value) > std::numeric_limits<float>::max())
		    return false;
		uvs[vertex * gltf_uv_components + component] =
		    static_cast<float>(value);
	    }
	}
	int32_t uv_accessor;
	if (!add_accessor(resources, uvs.data(), uvs.size() * sizeof(float),
		sizeof(float), TG3_TARGET_ARRAY_BUFFER, TG3_COMPONENT_TYPE_FLOAT,
		bot->num_vertices, TG3_TYPE_VEC2, uv_accessor))
	    return false;
	mesh.add_attribute("TEXCOORD_0", uv_accessor);
    }
    if (skin) {
	size_t max_influences = 0;
	for (size_t vertex = 0; vertex < bot->num_vertices; vertex++) {
	    max_influences = std::max(max_influences,
		static_cast<size_t>(skin->vertex_offsets[vertex + 1] -
		    skin->vertex_offsets[vertex]));
	}
	size_t influence_sets = (max_influences + gltf_influences_per_set - 1) /
	    gltf_influences_per_set;
	for (size_t set = 0; set < influence_sets; set++) {
	    std::vector<uint16_t> joint_values(
		bot->num_vertices * gltf_influences_per_set, 0);
	    std::vector<float> weight_values(
		bot->num_vertices * gltf_influences_per_set, 0.0f);
	    for (size_t vertex = 0; vertex < bot->num_vertices; vertex++) {
		uint32_t first = skin->vertex_offsets[vertex];
		uint32_t end = skin->vertex_offsets[vertex + 1];
		for (size_t component = 0; component < gltf_influences_per_set;
		    component++) {
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
    }
    return true;
}

static bool
bot_material_index(struct db_i *dbip, const bot_record &record,
    const std::map<std::string, int32_t> &materials, int32_t &index)
{
    struct directory *directory = db_lookup(dbip, record.name.c_str(), LOOKUP_QUIET);
    std::string material_name;
    index = TG3_INDEX_NONE;
    if (!directory)
	return false;
    if (!attribute_value(dbip, directory, "material_name", material_name))
	return true;
    auto material = materials.find(material_name);
    if (material == materials.end())
	return false;
    index = material->second;
    return true;
}

static bool
add_inverse_bind_accessor(gltf_resources &resources,
    const struct rt_bot_skin *skin, int32_t &inverse_bind_accessor)
{
    if (!skin)
	return false;
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

struct combination_leaf {
    std::string name;
    std::array<fastf_t, ELEMENTS_PER_MAT> matrix;
};

static bool
attribute_value(struct db_i *dbip, struct directory *directory,
    const char *name, std::string &value)
{
    struct bu_attribute_value_set attributes;
    bu_avs_init_empty(&attributes);
    if (db5_get_attributes(dbip, &attributes, directory)) {
	bu_avs_free(&attributes);
	return false;
    }
    const char *stored = bu_avs_get(&attributes, name);
    bool found = stored != nullptr;
    value = stored ? stored : "";
    bu_avs_free(&attributes);
    return found;
}

static bool
parse_index(const std::string &text, int32_t &index)
{
    char *end = nullptr;
    errno = 0;
    long value = std::strtol(text.c_str(), &end, 10);
    if (errno || !end || end == text.c_str() || *end || value < 0 ||
	value > INT32_MAX)
	return false;
    index = static_cast<int32_t>(value);
    return true;
}

static bool
parse_values(const char *text, double *values, size_t count)
{
    if (!text || !values)
	return false;
    const char *cursor = text;
    for (size_t index = 0; index < count; ++index) {
	while (std::isspace(static_cast<unsigned char>(*cursor)))
	    ++cursor;
	char *end = nullptr;
	errno = 0;
	double value = std::strtod(cursor, &end);
	if (errno || end == cursor || !std::isfinite(value))
	    return false;
	values[index] = value;
	cursor = end;
    }
    while (std::isspace(static_cast<unsigned char>(*cursor)))
	++cursor;
    return !*cursor;
}

static bool
load_gltf_materials(struct db_i *dbip, gltf_resources &resources,
    std::map<std::string, int32_t> &indices)
{
    std::map<int32_t, struct directory *> directories;
    struct directory *directory;
    FOR_ALL_DIRECTORY_START(directory, dbip) {
	if (directory->d_major_type != DB5_MAJORTYPE_BRLCAD ||
	    directory->d_minor_type != ID_MATERIAL)
	    continue;
	std::string index_text;
	int32_t source_index;
	if (!attribute_value(dbip, directory, "gltf::material_index", index_text))
	    continue;
	if (!parse_index(index_text, source_index) ||
	    !directories.emplace(source_index, directory).second)
	    return false;
    } FOR_ALL_DIRECTORY_END;
    for (const auto &entry : directories) {
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	if (rt_db_get_internal(&internal, entry.second, dbip, nullptr) < 0 ||
	    internal.idb_type != ID_MATERIAL) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	const auto *material = static_cast<const struct rt_material_internal *>(
	    internal.idb_ptr);
	material_output output;
	output.name = bu_vls_cstr(&material->name);
	const struct bu_attribute_value_set &optical = material->opticalProperties;
	bool valid = parse_values(bu_avs_get(&optical,
	    "gltf::base_color_factor"),
	    output.material.pbr_metallic_roughness.base_color_factor, 4) &&
	    parse_values(bu_avs_get(&optical, "gltf::metallic_factor"),
		&output.material.pbr_metallic_roughness.metallic_factor, 1) &&
	    parse_values(bu_avs_get(&optical, "gltf::roughness_factor"),
		&output.material.pbr_metallic_roughness.roughness_factor, 1) &&
	    parse_values(bu_avs_get(&optical, "gltf::emissive_factor"),
		output.material.emissive_factor, 3) &&
	    parse_values(bu_avs_get(&optical, "gltf::alpha_cutoff"),
		&output.material.alpha_cutoff, 1);
	const char *alpha_mode = bu_avs_get(&optical, "gltf::alpha_mode");
	const char *double_sided = bu_avs_get(&optical, "gltf::double_sided");
	if (alpha_mode)
	    output.alpha_mode = alpha_mode;
	if (!valid || !double_sided ||
	    (!BU_STR_EQUAL(double_sided, "0") &&
	     !BU_STR_EQUAL(double_sided, "1"))) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	output.material.double_sided = BU_STR_EQUAL(double_sided, "1");
	output.material.pbr_metallic_roughness.base_color_texture.index =
	    TG3_INDEX_NONE;
	output.material.pbr_metallic_roughness.metallic_roughness_texture.index =
	    TG3_INDEX_NONE;
	output.material.normal_texture.index = TG3_INDEX_NONE;
	output.material.normal_texture.scale = 1.0;
	output.material.occlusion_texture.index = TG3_INDEX_NONE;
	output.material.occlusion_texture.strength = 1.0;
	output.material.emissive_texture.index = TG3_INDEX_NONE;
	if (resources.materials.size() >= static_cast<size_t>(INT32_MAX)) {
	    rt_db_free_internal(&internal);
	    return false;
	}
	resources.materials.push_back(std::move(output));
	indices.emplace(entry.second->d_namep,
	    static_cast<int32_t>(resources.materials.size() - 1));
	rt_db_free_internal(&internal);
    }
    for (material_output &material : resources.materials)
	material.finish();
    return true;
}

static bool
collect_union_leaves(const union tree *tree,
    std::vector<combination_leaf> &leaves)
{
    if (!tree)
	return true;
    if (tree->tr_op == OP_UNION)
	return collect_union_leaves(tree->tr_b.tb_left, leaves) &&
	    collect_union_leaves(tree->tr_b.tb_right, leaves);
    if (tree->tr_op != OP_DB_LEAF || !tree->tr_l.tl_name)
	return false;
    combination_leaf leaf;
    leaf.name = tree->tr_l.tl_name;
    mat_t identity;
    MAT_IDN(identity);
    const fastf_t *matrix = tree->tr_l.tl_mat ? tree->tr_l.tl_mat : identity;
    std::copy(matrix, matrix + ELEMENTS_PER_MAT, leaf.matrix.begin());
    leaves.push_back(std::move(leaf));
    return true;
}

static bool
combination_leaves(struct db_i *dbip, struct directory *directory,
    std::vector<combination_leaf> &leaves)
{
    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, directory, dbip, nullptr) < 0 ||
	internal.idb_type != ID_COMBINATION) {
	rt_db_free_internal(&internal);
	return false;
    }
    const auto *combination = static_cast<const struct rt_comb_internal *>(
	internal.idb_ptr);
    bool result = collect_union_leaves(combination->tree, leaves);
    rt_db_free_internal(&internal);
    return result;
}

static bool
identity_matrix(const std::array<fastf_t, ELEMENTS_PER_MAT> &matrix)
{
    mat_t identity;
    MAT_IDN(identity);
    for (size_t index = 0; index < ELEMENTS_PER_MAT; ++index) {
	if (!EQUAL(matrix[index], identity[index]))
	    return false;
    }
    return true;
}

static bool
same_skin_binding(const struct rt_bot_skin *first,
    const struct rt_bot_skin *second)
{
    if (!first || !second || first->joint_count != second->joint_count)
	return first == second;
    for (size_t joint = 0; joint < first->joint_count; ++joint) {
	if (!BU_STR_EQUAL(bu_vls_cstr(&first->joint_links[joint]),
		bu_vls_cstr(&second->joint_links[joint])))
	    return false;
	for (size_t element = 0; element < ELEMENTS_PER_MAT; ++element) {
	    if (!EQUAL(first->inverse_bind_matrices[joint][element],
		    second->inverse_bind_matrices[joint][element]))
		return false;
	}
    }
    return true;
}

static bool
same_bot_mesh(const struct rt_bot_internal *first,
    const struct rt_bot_internal *second)
{
    if (!first || !second || first->mode != second->mode ||
	first->orientation != second->orientation ||
	first->bot_flags != second->bot_flags ||
	first->num_vertices != second->num_vertices ||
	first->num_faces != second->num_faces ||
	first->num_normals != second->num_normals ||
	first->num_face_normals != second->num_face_normals ||
	first->num_uvs != second->num_uvs ||
	first->num_face_uvs != second->num_face_uvs)
	return false;
    if ((first->num_vertices && (!first->vertices || !second->vertices)) ||
	(first->num_faces && (!first->faces || !second->faces)) ||
	(first->num_normals && (!first->normals || !second->normals)) ||
	(first->num_face_normals &&
	    (!first->face_normals || !second->face_normals)) ||
	(first->num_uvs && (!first->uvs || !second->uvs)) ||
	(first->num_face_uvs && (!first->face_uvs || !second->face_uvs)))
	return false;
    for (size_t index = 0; index < first->num_vertices * 3; ++index) {
	if (!EQUAL(first->vertices[index], second->vertices[index]))
	    return false;
    }
    for (size_t index = 0; index < first->num_faces * 3; ++index) {
	if (first->faces[index] != second->faces[index])
	    return false;
    }
    for (size_t index = 0; index < first->num_normals * 3; ++index) {
	if (!EQUAL(first->normals[index], second->normals[index]))
	    return false;
    }
    for (size_t index = 0; index < first->num_face_normals * 3; ++index) {
	if (first->face_normals[index] != second->face_normals[index])
	    return false;
    }
    for (size_t index = 0; index < first->num_uvs * bot_uv_components; ++index) {
	if (!EQUAL(first->uvs[index], second->uvs[index]))
	    return false;
    }
    for (size_t index = 0; index < first->num_face_uvs * 3; ++index) {
	if (first->face_uvs[index] != second->face_uvs[index])
	    return false;
    }
    const struct rt_bot_skin *first_skin = first->skin;
    const struct rt_bot_skin *second_skin = second->skin;
    if (!first_skin || !second_skin)
	return first_skin == second_skin;
    if (first_skin->joint_count != second_skin->joint_count ||
	first_skin->influence_count != second_skin->influence_count)
	return false;
    if (!first_skin->vertex_offsets || !second_skin->vertex_offsets ||
	(first_skin->influence_count &&
	    (!first_skin->joint_indices || !second_skin->joint_indices ||
	     !first_skin->weights || !second_skin->weights)))
	return false;
    for (size_t index = 0; index <= first->num_vertices; ++index) {
	if (first_skin->vertex_offsets[index] != second_skin->vertex_offsets[index])
	    return false;
    }
    for (size_t index = 0; index < first_skin->influence_count; ++index) {
	if (first_skin->joint_indices[index] != second_skin->joint_indices[index] ||
	    !EQUAL(first_skin->weights[index], second_skin->weights[index]))
	    return false;
    }
    return true;
}

static bool
same_bot_meshes(struct db_i *dbip,
    const std::vector<const bot_record *> &first,
    const std::vector<const bot_record *> &second,
    const std::map<std::string, int32_t> &materials)
{
    if (first.size() != second.size())
	return false;
    for (size_t index = 0; index < first.size(); ++index) {
	const auto *first_bot = static_cast<const struct rt_bot_internal *>(
	    first[index]->internal.idb_ptr);
	const auto *second_bot = static_cast<const struct rt_bot_internal *>(
	    second[index]->internal.idb_ptr);
	int32_t first_material;
	int32_t second_material;
	if (!same_bot_mesh(first_bot, second_bot) ||
	    !bot_material_index(dbip, *first[index], materials, first_material) ||
	    !bot_material_index(dbip, *second[index], materials, second_material) ||
	    first_material != second_material)
	    return false;
    }
    return true;
}

static bool
set_node_matrix(node_record &node,
    const std::array<fastf_t, ELEMENTS_PER_MAT> &matrix)
{
    double converted[ELEMENTS_PER_MAT];
    matrix_to_gltf(converted, matrix.data());
    if (node.node.has_matrix) {
	for (size_t element = 0; element < ELEMENTS_PER_MAT; ++element) {
	    if (std::fabs(node.node.matrix[element] - converted[element]) > SMALL_FASTF)
		return false;
	}
	return true;
    }
    node.node.has_matrix = 1;
    std::copy(converted, converted + ELEMENTS_PER_MAT, node.node.matrix);
    return true;
}

static bool
build_manifest_nodes(struct db_i *dbip,
    const std::vector<std::unique_ptr<joint_record>> &joints,
    const std::vector<std::unique_ptr<bot_record>> &bots,
    std::vector<node_record> &nodes, std::vector<scene_record> &scenes,
    int32_t &default_scene, std::map<std::string, int32_t> &indices,
    const std::map<std::string, int32_t> &material_indices,
    gltf_resources &resources, bool &found)
{
    std::map<int32_t, struct directory *> node_directories;
    std::map<int32_t, struct directory *> scene_directories;
    std::map<std::string, const bot_record *> bot_records;
    std::map<std::string, int32_t> object_indices;
    std::map<int32_t, std::pair<int32_t, std::vector<const bot_record *>>> mesh_outputs;
    std::map<int32_t, std::pair<int32_t, const struct rt_bot_skin *>> skin_outputs;
    struct directory *directory;

    found = false;
    FOR_ALL_DIRECTORY_START(directory, dbip) {
	std::string kind;
	if (!attribute_value(dbip, directory, "gltf::kind", kind))
	    continue;
	const char *index_name = nullptr;
	std::map<int32_t, struct directory *> *destination = nullptr;
	if (kind == "node") {
	    index_name = "gltf::node_index";
	    destination = &node_directories;
	} else if (kind == "scene") {
	    index_name = "gltf::scene_index";
	    destination = &scene_directories;
	} else {
	    continue;
	}
	found = true;
	std::string index_text;
	int32_t index;
	if (!attribute_value(dbip, directory, index_name, index_text) ||
	    !parse_index(index_text, index) || !destination->emplace(index, directory).second)
	    return false;
    } FOR_ALL_DIRECTORY_END;
    if (!found)
	return true;
    if (node_directories.empty() || scene_directories.empty())
	return false;

    nodes.resize(node_directories.size());
    size_t output_index = 0;
    for (const auto &entry : node_directories) {
	std::string link;
	if (!attribute_value(dbip, entry.second, "gltf::link", link) || link.empty() ||
	    output_index >= static_cast<size_t>(INT32_MAX))
	    return false;
	if (!attribute_value(dbip, entry.second, "gltf::name",
		nodes[output_index].name))
	    nodes[output_index].name = link;
	indices.emplace(link, static_cast<int32_t>(output_index));
	object_indices.emplace(entry.second->d_namep, static_cast<int32_t>(output_index));
	++output_index;
    }
    for (const auto &record : bots)
	bot_records.emplace(record->name, record.get());

    for (const auto &entry : node_directories) {
	int32_t node_index = object_indices[entry.second->d_namep];
	node_record &node = nodes[static_cast<size_t>(node_index)];
	std::vector<combination_leaf> leaves;
	std::vector<const bot_record *> node_bots;
	if (!combination_leaves(dbip, entry.second, leaves))
	    return false;
	for (const combination_leaf &leaf : leaves) {
	    auto child = object_indices.find(leaf.name);
	    if (child != object_indices.end()) {
		node.children.push_back(child->second);
		if (!set_node_matrix(nodes[static_cast<size_t>(child->second)], leaf.matrix))
		    return false;
		continue;
	    }
	    auto bot = bot_records.find(leaf.name);
	    if (bot == bot_records.end() || !identity_matrix(leaf.matrix))
		return false;
	    node_bots.push_back(bot->second);
	}
	if (!node_bots.empty()) {
	    const struct rt_bot_skin *skin = static_cast<const struct rt_bot_internal *>(
		node_bots.front()->internal.idb_ptr)->skin;
	    for (const bot_record *record : node_bots) {
		const auto *bot = static_cast<const struct rt_bot_internal *>(
		    record->internal.idb_ptr);
		if (!same_skin_binding(skin, bot->skin))
		    return false;
	    }
	    std::string mesh_index_text;
	    int32_t source_mesh = TG3_INDEX_NONE;
	    bool has_source_mesh = attribute_value(dbip, entry.second,
		"gltf::mesh_index", mesh_index_text);
	    if (has_source_mesh && !parse_index(mesh_index_text, source_mesh))
		return false;
	    auto existing_mesh = mesh_outputs.find(source_mesh);
	    if (has_source_mesh && existing_mesh != mesh_outputs.end()) {
		if (!same_bot_meshes(dbip, existing_mesh->second.second, node_bots,
			material_indices))
		    return false;
		node.mesh = existing_mesh->second.first;
	    } else {
		mesh_record mesh;
		if (!attribute_value(dbip, entry.second, "gltf::mesh_name", mesh.name))
		    mesh.name = node.name;
		for (const bot_record *record : node_bots) {
		    const auto *bot = static_cast<const struct rt_bot_internal *>(
			record->internal.idb_ptr);
		    int32_t material_index;
		    if (!bot_material_index(dbip, *record, material_indices,
			    material_index) ||
			!add_bot_primitive(resources, bot, mesh, material_index))
			return false;
		}
		if (resources.meshes.size() >= static_cast<size_t>(INT32_MAX))
		    return false;
		resources.meshes.push_back(std::move(mesh));
		node.mesh = static_cast<int32_t>(resources.meshes.size() - 1);
		if (has_source_mesh)
		    mesh_outputs.emplace(source_mesh,
			std::make_pair(node.mesh, node_bots));
	    }
	    if (skin) {
		std::string skin_index_text;
		int32_t source_skin = TG3_INDEX_NONE;
		bool has_source_skin = attribute_value(dbip, entry.second,
		    "gltf::skin_index", skin_index_text);
		if (has_source_skin && !parse_index(skin_index_text, source_skin))
		    return false;
		std::string skin_name;
		if (!attribute_value(dbip, entry.second, "gltf::skin_name", skin_name))
		    skin_name = node.name;
		int32_t skeleton_index = TG3_INDEX_NONE;
		std::string skeleton_link;
		if (attribute_value(dbip, entry.second, "gltf::skin_skeleton",
			skeleton_link)) {
		    auto skeleton = indices.find(skeleton_link);
		    if (skeleton == indices.end())
			return false;
		    skeleton_index = skeleton->second;
		}
		auto existing_skin = skin_outputs.find(source_skin);
		if (has_source_skin && existing_skin != skin_outputs.end()) {
		    const skin_output &stored = resources.skins[
			static_cast<size_t>(existing_skin->second.first)];
		    if (!same_skin_binding(existing_skin->second.second, skin) ||
			stored.name != skin_name ||
			stored.skin.skeleton != skeleton_index)
			return false;
		    node.skin = existing_skin->second.first;
		} else {
		    int32_t inverse_bind_accessor;
		    if (!add_inverse_bind_accessor(resources, skin,
			    inverse_bind_accessor))
			return false;
		    skin_output output;
		    output.name = skin_name;
		    output.skin.inverse_bind_matrices = inverse_bind_accessor;
		    output.skin.skeleton = skeleton_index;
		    for (size_t joint = 0; joint < skin->joint_count; ++joint) {
			auto link = indices.find(
			    bu_vls_cstr(&skin->joint_links[joint]));
			if (link == indices.end())
			    return false;
			output.joints.push_back(link->second);
		    }
		    if (resources.skins.size() >= static_cast<size_t>(INT32_MAX))
			return false;
		    resources.skins.push_back(std::move(output));
		    node.skin = static_cast<int32_t>(resources.skins.size() - 1);
		    if (has_source_skin)
			skin_outputs.emplace(source_skin,
			    std::make_pair(node.skin, skin));
		}
	    }
	}
    }

    struct rt_data_context context = RT_DATA_CONTEXT_INIT;
    context.media_type = "model/gltf+json";
    context.dbip = dbip;
    context.local2base = dbip->dbi_local2base;
    context.base2local = dbip->dbi_base2local;
    for (const auto &record : joints) {
	const auto *joint = static_cast<const struct rt_joint_internal *>(record->internal.idb_ptr);
	auto child = indices.find(bu_vls_cstr(&joint->child_link));
	if (child == indices.end())
	    continue;
	mat_t relative;
	if (rt_joint_relative_mat(relative, joint) != BRLCAD_OK)
	    return false;
	std::array<fastf_t, ELEMENTS_PER_MAT> matrix;
	std::copy(relative, relative + ELEMENTS_PER_MAT, matrix.begin());
	node_record &node = nodes[static_cast<size_t>(child->second)];
	if (!set_node_matrix(node, matrix))
	    return false;
	node.extras = object_extras(record->name, record->internal, context);
	if (!node.extras)
	    return false;
    }

    for (const auto &entry : scene_directories) {
	scene_record scene;
	attribute_value(dbip, entry.second, "gltf::name", scene.name);
	std::vector<combination_leaf> leaves;
	if (!combination_leaves(dbip, entry.second, leaves))
	    return false;
	for (const combination_leaf &leaf : leaves) {
	    auto root = object_indices.find(leaf.name);
	    if (root == object_indices.end() ||
		!set_node_matrix(nodes[static_cast<size_t>(root->second)], leaf.matrix))
		return false;
	    scene.roots.push_back(root->second);
	}
	scenes.push_back(std::move(scene));
    }
    struct directory *global = db_lookup(dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    std::string default_text;
    int32_t stored_default;
    default_scene = TG3_INDEX_NONE;
    if (global && attribute_value(dbip, global, "gltf::default_scene", default_text)) {
	if (!parse_index(default_text, stored_default))
	    return false;
	auto selected = scene_directories.find(stored_default);
	if (selected == scene_directories.end())
	    return false;
	default_scene = static_cast<int32_t>(std::distance(
	    scene_directories.begin(), selected));
    }

    std::vector<unsigned char> state(nodes.size(), 0);
    for (const scene_record &scene : scenes) {
	for (int32_t root : scene.roots) {
	    if (!visit_node(static_cast<size_t>(root), nodes, state))
		return false;
	}
    }
    if (std::find(state.begin(), state.end(), 0) != state.end())
	return false;
    for (mesh_record &mesh : resources.meshes)
	mesh.finish();
    for (skin_output &skin : resources.skins)
	skin.finish();
    for (node_record &node : nodes)
	node.finish();
    for (scene_record &scene : scenes)
	scene.finish();
    return true;
}

static bool
build_nodes(struct db_i *dbip, const std::vector<std::unique_ptr<joint_record>> &joints,
    const std::vector<std::unique_ptr<bot_record>> &bots,
    std::vector<node_record> &nodes, std::vector<int32_t> &roots,
    std::map<std::string, int32_t> &indices,
    const std::map<std::string, int32_t> &material_indices,
    gltf_resources &resources)
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
	mesh_record mesh;
	mesh.name = record->name;
	int32_t material_index;
	if (!bot_material_index(dbip, *record, material_indices, material_index) ||
	    !add_bot_primitive(resources, bot, mesh, material_index) ||
	    resources.meshes.size() >= static_cast<size_t>(INT32_MAX))
	    return false;
	resources.meshes.push_back(std::move(mesh));
	int32_t mesh_index = static_cast<int32_t>(resources.meshes.size() - 1);
	int32_t skin_index = TG3_INDEX_NONE;
	if (skin) {
	    int32_t inverse_bind_accessor;
	    if (!add_inverse_bind_accessor(resources, skin, inverse_bind_accessor))
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
	    if (resources.skins.size() >= static_cast<size_t>(INT32_MAX))
		return false;
	    resources.skins.push_back(std::move(output));
	    skin_index = static_cast<int32_t>(resources.skins.size() - 1);
	}
	if (nodes.size() >= static_cast<size_t>(INT32_MAX))
	    return false;
	node_record mesh_node;
	mesh_node.name = record->name;
	mesh_node.mesh = mesh_index;
	mesh_node.skin = skin_index;
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

static int
gltf_write(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(options_data), const char *path)
{
    std::vector<std::unique_ptr<joint_record>> joints;
    std::vector<std::unique_ptr<bot_record>> bots;
    std::vector<std::unique_ptr<animation_record>> animations;
    std::vector<node_record> nodes;
    std::vector<int32_t> roots;
    std::vector<scene_record> scenes;
    std::map<std::string, int32_t> node_indices;
    std::map<std::string, int32_t> material_indices;
    gltf_resources resources;
    struct rt_data_context data_context = RT_DATA_CONTEXT_INIT;
    tg3_write_options write_options;
    tg3_model model = {};
    tg3_error_stack errors;
    tg3_error_code error = TG3_OK;
    int32_t default_scene = TG3_INDEX_NONE;
    bool manifest_found = false;

    if (!context || !context->dbip || !path || !load_joints(context->dbip, joints) ||
	!load_bots(context->dbip, bots) || !load_animations(context->dbip, animations) ||
	!load_gltf_materials(context->dbip, resources, material_indices)) {
	bu_log("glTF articulation export requires a valid persistent joint graph\n");
	return 0;
    }
    if (!build_manifest_nodes(context->dbip, joints, bots, nodes, scenes,
	    default_scene, node_indices, material_indices, resources,
	    manifest_found)) {
	bu_log("Unable to preserve the glTF scene manifest\n");
	return 0;
    }
    if (!manifest_found) {
	if (!build_nodes(context->dbip, joints, bots, nodes, roots, node_indices,
		material_indices, resources)) {
	    bu_log("glTF articulation export requires a valid persistent joint graph\n");
	    return 0;
	}
	scene_record scene;
	scene.name = "BRL-CAD articulation";
	scene.roots = roots;
	scene.finish();
	scenes.push_back(std::move(scene));
	default_scene = 0;
    }
    data_context.media_type = "model/gltf+json";
    data_context.dbip = context->dbip;
    data_context.local2base = context->dbip->dbi_local2base;
    data_context.base2local = context->dbip->dbi_base2local;
    if (!build_animations(animations, node_indices, data_context, resources)) {
	bu_log("Unable to project animation channels into glTF\n");
	return 0;
    }
    tg3_write_options_init(&write_options);
    write_options.pretty_print = 1;
    write_options.embed_buffers = 1;
    resources.buffer.byte_length = resources.bytes.size();
    resources.buffer.data = {resources.bytes.data(), resources.bytes.size()};
    std::vector<tg3_mesh> output_meshes;
    std::vector<tg3_node> output_nodes;
    std::vector<tg3_skin> output_skins;
    std::vector<tg3_animation> output_animations;
    std::vector<tg3_material> output_materials;
    std::vector<tg3_scene> output_scenes;
    for (const mesh_record &mesh : resources.meshes)
	output_meshes.push_back(mesh.mesh);
    for (const node_record &node : nodes)
	output_nodes.push_back(node.node);
    for (const skin_output &skin : resources.skins)
	output_skins.push_back(skin.skin);
    for (const animation_output &animation : resources.animations)
	output_animations.push_back(animation.animation);
    for (const material_output &material : resources.materials)
	output_materials.push_back(material.material);
    for (const scene_record &scene : scenes)
	output_scenes.push_back(scene.scene);
    model.asset.version = {"2.0", 3};
    model.asset.generator = {"BRL-CAD libgcv", 14};
    model.default_scene = default_scene;
    model.buffers = resources.bytes.empty() ? nullptr : &resources.buffer;
    model.buffers_count = resources.bytes.empty() ? 0 : 1;
    model.buffer_views = resources.views.data();
    model.buffer_views_count = static_cast<uint32_t>(resources.views.size());
    model.accessors = resources.accessors.data();
    model.accessors_count = static_cast<uint32_t>(resources.accessors.size());
    model.meshes = output_meshes.data();
    model.meshes_count = static_cast<uint32_t>(output_meshes.size());
    model.nodes = output_nodes.data();
    model.nodes_count = static_cast<uint32_t>(output_nodes.size());
    model.skins = output_skins.data();
    model.skins_count = static_cast<uint32_t>(output_skins.size());
    model.animations = output_animations.data();
    model.animations_count = static_cast<uint32_t>(output_animations.size());
    model.materials = output_materials.data();
    model.materials_count = static_cast<uint32_t>(output_materials.size());
    model.scenes = output_scenes.data();
    model.scenes_count = static_cast<uint32_t>(output_scenes.size());
    tg3_error_stack_init(&errors);
    if (strlen(path) > UINT32_MAX)
	error = TG3_ERR_WRITE_FAILED;
    else
	error = tg3_write_to_file(&model, &errors, path,
	    static_cast<uint32_t>(strlen(path)), &write_options);
    for (uint32_t index = 0; index < errors.count; ++index) {
	const tg3_error_entry &entry = errors.entries[index];
	bu_log("glTF write error (%d): %s\n", static_cast<int>(entry.code),
	    entry.message ? entry.message : "no details");
    }
    tg3_error_stack_free(&errors);
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

