/*             G L T F _ R E A D _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file gltf_read_util.h
 *
 * Shared geometry and scene handling for the native glTF readers.
 */

#ifndef BRLCAD_GLTF_READ_UTIL_H
#define BRLCAD_GLTF_READ_UTIL_H

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "tiny_gltf_v3.h"

#include "vmath.h"
#include "bn/qmath.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "rt/db_io.h"

namespace brlcad_gltf {

constexpr uint64_t MAX_FILE_BYTES = 1ULL << 30;
constexpr uint64_t MAX_PARSE_MEMORY_BYTES = MAX_FILE_BYTES;
constexpr size_t TRIANGLE_VERTICES = 3;

struct accessor_view {
    const tg3_accessor *accessor = nullptr;
    const uint8_t *data = nullptr;
    size_t stride = 0;
    std::shared_ptr<std::vector<uint8_t>> storage;
};

inline bool read_index(const uint8_t *data, int32_t component_type, size_t &index);

inline std::string
to_string(tg3_str value)
{
    return value.data ? std::string(value.data, value.len) : std::string();
}

inline std::string
unique_object_name(const struct db_i *dbip, std::string base, uint32_t object_index)
{
    /* A glTF name can repeat and can contain NUL, which terminates a .g name. */
    std::replace(base.begin(), base.end(), '\0', '_');
    if (!db_lookup(dbip, base.c_str(), LOOKUP_QUIET))
	return base;

    base += "_" + std::to_string(object_index);
    std::string name = base;
    for (size_t suffix = 1; db_lookup(dbip, name.c_str(), LOOKUP_QUIET); ++suffix)
	name = base + "_" + std::to_string(suffix);
    return name;
}

inline const tg3_scene *
default_scene(const tg3_model &model)
{
    int32_t index = model.default_scene >= 0 ? model.default_scene : 0;
    if (static_cast<uint32_t>(index) >= model.scenes_count) {
	bu_log("glTF default scene index %d is invalid\n", index);
	return nullptr;
    }
    return &model.scenes[index];
}

class active_node {
public:
    active_node(const tg3_model &model, int32_t index, std::set<int32_t> &path)
	: path_(path), index_(index)
    {
	if (index < 0 || static_cast<uint32_t>(index) >= model.nodes_count) {
	    bu_log("glTF node index %d is invalid\n", index);
	    return;
	}
	if (!path_.insert(index).second) {
	    bu_log("glTF node hierarchy contains a cycle at node %d\n", index);
	    return;
	}
	node_ = &model.nodes[index];
    }

    ~active_node()
    {
	if (node_)
	    path_.erase(index_);
    }

    active_node(const active_node &) = delete;
    active_node &operator=(const active_node &) = delete;

    const tg3_node *get() const { return node_; }

private:
    std::set<int32_t> &path_;
    int32_t index_;
    const tg3_node *node_ = nullptr;
};

inline void
node_transform(mat_t matrix, const tg3_node &node)
{
    if (node.has_matrix) {
	/* glTF stores matrices by column. BRL-CAD stores them by row. */
	MAT_TRANSPOSE(matrix, node.matrix);
	return;
    }

    /* BRL-CAD's quaternion conversion uses the inverse rotation convention. */
    quat_t rotation;
    QCONJUGATE(rotation, node.rotation);
    quat_quat2mat(matrix, rotation);

    /* glTF composes T * R * S, so scale each column before translating. */
    for (size_t row = 0; row < ELEMENTS_PER_VECT; ++row) {
	for (size_t column = 0; column < ELEMENTS_PER_VECT; ++column)
	    matrix[row * ELEMENTS_PER_HVECT + column] *= node.scale[column];
    }
    MAT_DELTAS_VEC(matrix, node.translation);
}

inline int32_t
find_attribute(const tg3_primitive &primitive, const char *name)
{
    for (uint32_t i = 0; i < primitive.attributes_count; ++i) {
	if (tg3_str_equals_cstr(primitive.attributes[i].key, name))
	    return primitive.attributes[i].value;
    }
    return TG3_INDEX_NONE;
}

inline bool
is_supported_position_component(int32_t component_type)
{
    return component_type == TG3_COMPONENT_TYPE_FLOAT ||
	component_type == TG3_COMPONENT_TYPE_DOUBLE;
}

inline void
warn_legacy_double_position(uint32_t mesh_number, uint32_t primitive_number)
{
    bu_log("Warning: glTF mesh %u primitive %u uses legacy non-standard DOUBLE "
	"(componentType 5130) POSITION data; importing it for recovery. "
	"Re-export the model to convert it to conforming FLOAT data.\n",
	mesh_number, primitive_number);
}

inline bool
get_accessor_view(const tg3_model &model, int32_t accessor_index, accessor_view &result)
{
    if (accessor_index < 0 || static_cast<uint32_t>(accessor_index) >= model.accessors_count)
	return false;

    const tg3_accessor &accessor = model.accessors[accessor_index];
    int32_t component_size = tg3_component_size(accessor.component_type);
    int32_t component_count = tg3_num_components(accessor.type);
    if (component_size <= 0 || component_count <= 0)
	return false;
    uint64_t element_size = static_cast<uint64_t>(component_size) *
	static_cast<uint64_t>(component_count);
    if (accessor.count > SIZE_MAX || element_size > SIZE_MAX ||
	accessor.count > MAX_PARSE_MEMORY_BYTES / element_size)
	return false;
    result = accessor_view();
    result.accessor = &accessor;

    const tg3_buffer_view *base_view = nullptr;
    const tg3_buffer *base_buffer = nullptr;
    int32_t stride = 0;
    if (accessor.buffer_view >= 0) {
	if (static_cast<uint32_t>(accessor.buffer_view) >= model.buffer_views_count)
	    return false;
	base_view = &model.buffer_views[accessor.buffer_view];
	if (base_view->buffer < 0 ||
	    static_cast<uint32_t>(base_view->buffer) >= model.buffers_count)
	    return false;
	base_buffer = &model.buffers[base_view->buffer];
	stride = tg3_accessor_byte_stride(&accessor, base_view);
	if (stride <= 0 || static_cast<uint64_t>(stride) < element_size ||
	    !base_buffer->data.data ||
	    base_view->byte_offset > base_buffer->data.count ||
	    base_view->byte_length > base_buffer->data.count - base_view->byte_offset ||
	    accessor.byte_offset > base_view->byte_length)
	    return false;
	uint64_t available = base_view->byte_length - accessor.byte_offset;
	if (accessor.count > 0 &&
	    (available < element_size || accessor.count - 1 >
	    (available - element_size) / static_cast<uint64_t>(stride)))
	    return false;
    } else if (!accessor.sparse.is_sparse) {
	return false;
    }

    if (!accessor.sparse.is_sparse) {
	uint64_t data_offset = base_view->byte_offset + accessor.byte_offset;
	if (data_offset > std::numeric_limits<size_t>::max())
	    return false;
	result.data = base_buffer->data.data + static_cast<size_t>(data_offset);
	result.stride = static_cast<size_t>(stride);
	return true;
    }

    const tg3_accessor_sparse &sparse = accessor.sparse;
    if (sparse.count > accessor.count || sparse.indices.buffer_view < 0 ||
	sparse.values.buffer_view < 0 ||
	static_cast<uint32_t>(sparse.indices.buffer_view) >= model.buffer_views_count ||
	static_cast<uint32_t>(sparse.values.buffer_view) >= model.buffer_views_count)
	return false;
    const tg3_buffer_view &index_view = model.buffer_views[sparse.indices.buffer_view];
    const tg3_buffer_view &value_view = model.buffer_views[sparse.values.buffer_view];
    if (index_view.buffer < 0 || value_view.buffer < 0 ||
	static_cast<uint32_t>(index_view.buffer) >= model.buffers_count ||
	static_cast<uint32_t>(value_view.buffer) >= model.buffers_count)
	return false;
    const tg3_buffer &index_buffer = model.buffers[index_view.buffer];
    const tg3_buffer &value_buffer = model.buffers[value_view.buffer];
    int32_t index_size = tg3_component_size(sparse.indices.component_type);
    if (index_size <= 0 || !index_buffer.data.data || !value_buffer.data.data ||
	index_view.byte_offset > index_buffer.data.count ||
	index_view.byte_length > index_buffer.data.count - index_view.byte_offset ||
	value_view.byte_offset > value_buffer.data.count ||
	value_view.byte_length > value_buffer.data.count - value_view.byte_offset ||
	sparse.indices.byte_offset > index_view.byte_length ||
	sparse.values.byte_offset > value_view.byte_length ||
	sparse.count > (index_view.byte_length - sparse.indices.byte_offset) /
	    static_cast<uint64_t>(index_size) ||
	sparse.count > (value_view.byte_length - sparse.values.byte_offset) /
	    element_size)
	return false;

    size_t byte_count = static_cast<size_t>(accessor.count * element_size);
    result.storage = std::make_shared<std::vector<uint8_t>>(byte_count, 0);
    if (base_view) {
	const uint8_t *base = base_buffer->data.data +
	    static_cast<size_t>(base_view->byte_offset + accessor.byte_offset);
	for (size_t index = 0; index < static_cast<size_t>(accessor.count); ++index)
	    std::memcpy(result.storage->data() + index * static_cast<size_t>(element_size),
		base + index * static_cast<size_t>(stride),
		static_cast<size_t>(element_size));
    }
    const uint8_t *sparse_indices = index_buffer.data.data +
	static_cast<size_t>(index_view.byte_offset + sparse.indices.byte_offset);
    const uint8_t *sparse_values = value_buffer.data.data +
	static_cast<size_t>(value_view.byte_offset + sparse.values.byte_offset);
    size_t previous = 0;
    for (size_t sparse_index = 0; sparse_index < static_cast<size_t>(sparse.count);
	sparse_index++) {
	size_t index;
	if (!read_index(sparse_indices + sparse_index * static_cast<size_t>(index_size),
		sparse.indices.component_type, index) || index >= accessor.count ||
	    (sparse_index && index <= previous))
	    return false;
	previous = index;
	std::memcpy(result.storage->data() + index * static_cast<size_t>(element_size),
	    sparse_values + sparse_index * static_cast<size_t>(element_size),
	    static_cast<size_t>(element_size));
    }
    result.data = result.storage->data();
    result.stride = static_cast<size_t>(element_size);
    return true;
}

inline bool
read_index(const uint8_t *data, int32_t component_type, size_t &index)
{
    switch (component_type) {
	case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
	    index = data[0];
	    return true;
	case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
	    index = static_cast<size_t>(data[0]) |
		(static_cast<size_t>(data[1]) << 8);
	    return true;
	case TG3_COMPONENT_TYPE_UNSIGNED_INT:
	    index = static_cast<size_t>(data[0]) |
		(static_cast<size_t>(data[1]) << 8) |
		(static_cast<size_t>(data[2]) << 16) |
		(static_cast<size_t>(data[3]) << 24);
	    return true;
	default:
	    return false;
    }
}

inline bool
read_coordinate(const uint8_t *data, int32_t component_type, double &value)
{
    if (component_type == TG3_COMPONENT_TYPE_FLOAT) {
	uint32_t bits = static_cast<uint32_t>(data[0]) |
	    (static_cast<uint32_t>(data[1]) << 8) |
	    (static_cast<uint32_t>(data[2]) << 16) |
	    (static_cast<uint32_t>(data[3]) << 24);
	float coordinate;
	std::memcpy(&coordinate, &bits, sizeof(coordinate));
	value = coordinate;
	return true;
    }
    if (component_type == TG3_COMPONENT_TYPE_DOUBLE) {
	uint64_t bits = static_cast<uint64_t>(data[0]) |
	    (static_cast<uint64_t>(data[1]) << 8) |
	    (static_cast<uint64_t>(data[2]) << 16) |
	    (static_cast<uint64_t>(data[3]) << 24) |
	    (static_cast<uint64_t>(data[4]) << 32) |
	    (static_cast<uint64_t>(data[5]) << 40) |
	    (static_cast<uint64_t>(data[6]) << 48) |
	    (static_cast<uint64_t>(data[7]) << 56);
	std::memcpy(&value, &bits, sizeof(value));
	return true;
    }
    return false;
}

inline bool
append_triangle_primitive(const tg3_model &model, uint32_t mesh_number,
    uint32_t primitive_number, std::vector<fastf_t> &vertices, std::vector<int> &faces)
{
    const tg3_primitive &primitive = model.meshes[mesh_number].primitives[primitive_number];
    if (primitive.mode != TG3_MODE_TRIANGLES) {
	bu_log("glTF mesh %u primitive %u is not a triangle mesh\n", mesh_number, primitive_number);
	return false;
    }

    accessor_view positions;
    if (!get_accessor_view(model, find_attribute(primitive, "POSITION"), positions) ||
	positions.accessor->type != TG3_TYPE_VEC3 ||
	!is_supported_position_component(positions.accessor->component_type)) {
	bu_log("glTF mesh %u primitive %u has invalid POSITION data\n", mesh_number, primitive_number);
	return false;
    }
    if (positions.accessor->component_type == TG3_COMPONENT_TYPE_DOUBLE)
	warn_legacy_double_position(mesh_number, primitive_number);

    size_t first_vertex = vertices.size() / ELEMENTS_PER_POINT;
    if (positions.accessor->count > static_cast<size_t>(INT_MAX) - first_vertex ||
	positions.accessor->count > (vertices.max_size() - vertices.size()) / ELEMENTS_PER_POINT) {
	bu_log("glTF mesh %u has too many vertices\n", mesh_number);
	return false;
    }
    size_t vertex_count = static_cast<size_t>(positions.accessor->count);

    accessor_view indices;
    uint64_t index_count = vertex_count;
    if (primitive.indices != TG3_INDEX_NONE) {
	if (!get_accessor_view(model, primitive.indices, indices) ||
	    indices.accessor->type != TG3_TYPE_SCALAR) {
	    bu_log("glTF mesh %u primitive %u has invalid index data\n", mesh_number, primitive_number);
	    return false;
	}
	index_count = indices.accessor->count;
    }
    if (index_count == 0 || index_count % TRIANGLE_VERTICES != 0 ||
	index_count > faces.max_size() - faces.size()) {
	bu_log("glTF mesh %u primitive %u has an invalid triangle index count\n",
	    mesh_number, primitive_number);
	return false;
    }

    const size_t first_index = faces.size();
    faces.resize(first_index + static_cast<size_t>(index_count));
    for (size_t i = 0; i < index_count; ++i) {
	size_t index = i;
	if (indices.accessor &&
	    !read_index(indices.data + i * indices.stride, indices.accessor->component_type, index)) {
	    bu_log("glTF mesh %u primitive %u uses an unsupported index type\n", mesh_number, primitive_number);
	    return false;
	}
	if (index >= vertex_count) {
	    bu_log("glTF mesh %u primitive %u contains an invalid vertex index\n", mesh_number, primitive_number);
	    return false;
	}
	faces[first_index + i] = static_cast<int>(first_vertex + index);
    }

    const int32_t component_type = positions.accessor->component_type;
    const int32_t component_size = tg3_component_size(component_type);
    vertices.resize((first_vertex + vertex_count) * ELEMENTS_PER_POINT);
    for (size_t i = 0; i < vertex_count; ++i) {
	for (size_t axis = 0; axis < ELEMENTS_PER_POINT; ++axis) {
	    double coordinate;
	    if (!read_coordinate(positions.data + i * positions.stride + axis * component_size,
		    component_type, coordinate))
		return false;
	    vertices[(first_vertex + i) * ELEMENTS_PER_POINT + axis] = static_cast<fastf_t>(coordinate);
	}
    }
    return true;
}

inline void
log_errors(const tg3_error_stack &errors)
{
    for (uint32_t i = 0; i < errors.count; ++i) {
	const tg3_error_entry &entry = errors.entries[i];
	const char *severity = entry.severity == TG3_SEVERITY_ERROR ? "error" :
	    (entry.severity == TG3_SEVERITY_WARNING ? "warning" : "info");
	bu_log("glTF %s (%d)%s%s: %s\n", severity, static_cast<int>(entry.code),
	    entry.json_path ? " at " : "", entry.json_path ? entry.json_path : "",
	    entry.message ? entry.message : "no details");
    }
}

class confined_file_reader {
public:
    explicit confined_file_reader(const char *source_path)
    {
	char *resolved = bu_file_realpath(source_path, nullptr);
	if (!resolved)
	    return;

	source_path_ = resolved;
	bu_free(resolved, "resolved glTF input path");
	std::string::size_type separator = source_path_.find_last_of("/\\");
	if (separator != std::string::npos)
	    root_directory_ = source_path_.substr(0, separator + 1);
    }

    bool valid() const
    {
	return !source_path_.empty() && !root_directory_.empty() &&
	    source_path_.size() <= std::numeric_limits<uint32_t>::max();
    }

    tg3_error_code parse(tg3_model *model, tg3_error_stack *errors,
	bool store_original_json = false)
    {
	if (!valid())
	    return TG3_ERR_FILE_NOT_FOUND;

	tg3_parse_options options;
	tg3_parse_options_init(&options);
	/* BRL-CAD formerly wrote non-standard DOUBLE accessors with -F. */
	options.strictness = TG3_PERMISSIVE;
	options.images_as_is = 1;
	options.skip_extras_values = store_original_json ? 0 : 1;
	options.store_original_json = store_original_json ? 1 : 0;
	options.validate_indices = 1;
	options.memory.memory_budget = MAX_PARSE_MEMORY_BYTES;
	options.memory.max_single_alloc = MAX_FILE_BYTES;
	options.max_external_file_size = MAX_FILE_BYTES;
	options.fs.read_file = read_file;
	options.fs.free_file = free_file;
	options.fs.user_data = this;

	return tg3_parse_file(model, errors, source_path_.c_str(),
	    static_cast<uint32_t>(source_path_.size()), &options);
    }

    bool read_resource(tg3_str uri, uint8_t **data, uint64_t *size)
    {
	const std::string path = root_directory_ + to_string(uri);
	return uri.len && path.size() <= std::numeric_limits<uint32_t>::max() &&
	    read_file(data, size, path.c_str(), static_cast<uint32_t>(path.size()), this);
    }

private:
    bool resolve_confined_path(std::string &result, const char *path,
	uint32_t path_length) const
    {
	if (!path || path_length == 0)
	    return false;

	std::string requested(path, path_length);
	if (requested.find('\0') != std::string::npos)
	    return false;
	char *resolved = bu_file_realpath(requested.c_str(), nullptr);
	if (!resolved)
	    return false;

	result = resolved;
	bu_free(resolved, "resolved glTF resource path");
	return result.compare(0, root_directory_.size(), root_directory_) == 0;
    }

    static int32_t read_file(uint8_t **out_data, uint64_t *out_size,
	const char *path, uint32_t path_length, void *user_data)
    {
	if (!out_data || !out_size || !user_data)
	    return 0;
	*out_data = nullptr;
	*out_size = 0;

	const auto *reader = static_cast<const confined_file_reader *>(user_data);
	std::string resolved;
	if (!reader->resolve_confined_path(resolved, path, path_length))
	    return 0;

	std::ifstream input(resolved, std::ios::binary | std::ios::ate);
	if (!input)
	    return 0;
	std::streamoff end = input.tellg();
	if (end < 0 || static_cast<uint64_t>(end) > MAX_FILE_BYTES)
	    return 0;

	uint64_t byte_count = static_cast<uint64_t>(end);
	if (byte_count > static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
	    return 0;
	input.seekg(0, std::ios::beg);
	uint8_t *data = static_cast<uint8_t *>(bu_malloc(
	    byte_count ? static_cast<size_t>(byte_count) : 1, "glTF input data"));
	if (byte_count > 0) {
	    input.read(reinterpret_cast<char *>(data), static_cast<std::streamsize>(byte_count));
	    if (input.gcount() != static_cast<std::streamsize>(byte_count)) {
		bu_free(data, "glTF input data");
		return 0;
	    }
	}

	*out_data = data;
	*out_size = byte_count;
	return 1;
    }

    static void free_file(uint8_t *data, uint64_t, void *)
    {
	bu_free(data, "glTF input data");
    }

    std::string source_path_;
    std::string root_directory_;
};

} // namespace brlcad_gltf

#endif // BRLCAD_GLTF_READ_UTIL_H

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
