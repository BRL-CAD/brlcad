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
#include <cstdio>
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
#include "rt/primitives/joint.h"

#include "gltf_data.h"

namespace {

static const char *joint_extras_name = "BRL-CAD_joint";
static constexpr double millimeters_to_gltf_meters = 0.001;

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

struct node_record {
    std::string name;
    std::vector<int32_t> children;
    std::unique_ptr<brlcad_gltf::data_value> extras;
    tg3_node node = {};

    void finish()
    {
	node.name.data = name.c_str();
	node.name.len = static_cast<uint32_t>(name.size());
	node.camera = TG3_INDEX_NONE;
	node.skin = TG3_INDEX_NONE;
	node.mesh = TG3_INDEX_NONE;
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

static std::unique_ptr<brlcad_gltf::data_value>
joint_extras(const joint_record &record, const struct rt_data_context &context)
{
    struct rt_data_node *joint_data = nullptr;
    struct rt_data_node *envelope = rt_data_object();
    struct rt_data_node *description = rt_data_object();
    struct rt_data_node *name = rt_data_string(record.name.c_str());

    if (!envelope || !description || !name ||
	rt_obj_data_export(&joint_data, &record.internal, &context) != BRLCAD_OK)
	goto fail;
    if (rt_data_object_set(description, "name", name) != BRLCAD_OK)
	goto fail;
    name = nullptr;
    if (rt_data_object_set(description, "data", joint_data) != BRLCAD_OK)
	goto fail;
    joint_data = nullptr;
    if (rt_data_object_set(envelope, joint_extras_name, description) != BRLCAD_OK)
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
    std::vector<node_record> &nodes, std::vector<int32_t> &roots)
{
    std::map<std::string, int32_t> indices;
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
	child_node.extras = joint_extras(*record, context);
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
    std::vector<node_record> nodes;
    std::vector<int32_t> roots;
    tg3_write_options write_options;
    tg3_asset asset = {};
    tg3_scene scene = {};
    tg3_writer *writer = nullptr;
    tg3_error_code error = TG3_OK;
    FILE *stream = nullptr;

    if (!context || !context->dbip || !path || !load_joints(context->dbip, joints) ||
	!build_nodes(context->dbip, joints, nodes, roots)) {
	bu_log("glTF articulation export requires a valid persistent joint graph\n");
	return 0;
    }
    stream = fopen(path, "wb");
    if (!stream)
	return 0;
    tg3_write_options_init(&write_options);
    write_options.pretty_print = 1;
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
    for (const node_record &node : nodes) {
	if (error == TG3_OK)
	    error = tg3_writer_add_node(writer, &node.node);
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

