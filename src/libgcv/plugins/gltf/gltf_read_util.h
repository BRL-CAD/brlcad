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
 * Shared checked access to TinyGLTF v3 models.
 */

#ifndef BRLCAD_GLTF_READ_UTIL_H
#define BRLCAD_GLTF_READ_UTIL_H

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include "tiny_gltf_v3.h"

#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"

namespace brlcad_gltf {

constexpr uint64_t MAX_FILE_BYTES = 1ULL << 30;
constexpr uint64_t MAX_PARSE_MEMORY_BYTES = MAX_FILE_BYTES;

struct accessor_view {
    const tg3_accessor *accessor = nullptr;
    const uint8_t *data = nullptr;
    size_t stride = 0;
};

inline std::string
to_string(tg3_str value)
{
    return value.data ? std::string(value.data, value.len) : std::string();
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
    if (accessor.sparse.is_sparse || accessor.buffer_view < 0 ||
	static_cast<uint32_t>(accessor.buffer_view) >= model.buffer_views_count)
	return false;

    const tg3_buffer_view &view = model.buffer_views[accessor.buffer_view];
    if (view.buffer < 0 || static_cast<uint32_t>(view.buffer) >= model.buffers_count)
	return false;

    const tg3_buffer &buffer = model.buffers[view.buffer];
    int32_t stride = tg3_accessor_byte_stride(&accessor, &view);
    int32_t component_size = tg3_component_size(accessor.component_type);
    int32_t component_count = tg3_num_components(accessor.type);
    if (stride <= 0 || component_size <= 0 || component_count <= 0 || !buffer.data.data)
	return false;

    uint64_t element_size = static_cast<uint64_t>(component_size) *
	static_cast<uint64_t>(component_count);
    if (static_cast<uint64_t>(stride) < element_size ||
	view.byte_offset > buffer.data.count ||
	view.byte_length > buffer.data.count - view.byte_offset ||
	accessor.byte_offset > view.byte_length)
	return false;

    uint64_t available = view.byte_length - accessor.byte_offset;
    if (accessor.count > 0 &&
	(available < element_size ||
	 accessor.count - 1 > (available - element_size) / static_cast<uint64_t>(stride)))
	return false;

    uint64_t data_offset = view.byte_offset + accessor.byte_offset;
    if (data_offset > std::numeric_limits<size_t>::max())
	return false;

    result.accessor = &accessor;
    result.data = buffer.data.data + static_cast<size_t>(data_offset);
    result.stride = static_cast<size_t>(stride);
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
