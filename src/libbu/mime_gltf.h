/*          M I M E _ G L T F . H
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

#ifndef BU_MIME_GLTF_H
#define BU_MIME_GLTF_H
#include "common.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include "bu/mime_runtime.h"
#include "bu/str.h"
#include "json.hpp"

namespace mime_gltf {
inline constexpr size_t glb_header_size = 12, glb_json_offset = 20;
inline constexpr uint32_t json_chunk_type = 0x4e4f534a;
inline bool valid_container(uint32_t version, uint32_t chunk_type)
{
    return (version == 2 && chunk_type == json_chunk_type) || (version == 1 && chunk_type == 0);
}
/* Retain complete declarations from a bounded prefix. Missing arrays are
 * known empty only after the root closes; an unfinished array is unknown. */
class gltf_header : public nlohmann::json_sax<nlohmann::json> {
    static constexpr size_t maximum_prefix_depth = 64;
    const bool bounded_prefix;
    size_t depth = 0;
    size_t asset_depth = 0;
    size_t extensions_depth = 0, vrm_depth = 0;
    bool root_object = false;
    bool extensions_seen = false, vrm_ambiguous = false;
    size_t vrm_count = 0;
    bool vrm_version_seen = false;
    std::string property;

public:
    explicit gltf_header(bool prefix = true) : bounded_prefix(prefix) {}
    struct string_set {
	std::vector<std::string> values;
	bool known = false, seen = false, valid = true;
	size_t depth = 0;
    } required, used;
    std::string version;
    std::string minimum_version;
    std::string vrm_extension, vrm_version;
    bool vrm_complete = false;
    bool asset_complete = false;
    bool root_complete = false;
private:
    string_set *active_set = nullptr;
    string_set *root_set()
    {
	if (!root_object || depth != 1) return nullptr;
	if (property == "extensionsRequired") return &required;
	if (property == "extensionsUsed") return &used;
	return nullptr;
    }
    void mark_vrm_version()
    {
	if (!vrm_depth || depth != vrm_depth || property != "specVersion") return;
	if (vrm_version_seen) ++vrm_count;
	vrm_version_seen = true;
	vrm_version.clear();
    }
    void mark_property()
    {
	if (auto *set = root_set()) {set->seen = true; set->known = false;}
	if (root_object && depth == 1 && property == "extensions") {
	    if (extensions_seen) {vrm_ambiguous = true; vrm_complete = false;}
	    extensions_seen = true;
	}
	if (extensions_depth && depth == extensions_depth && (property == "VRM" || property == "VRMC_vrm")) {
	    ++vrm_count;
	    vrm_complete = false;
	}
	mark_vrm_version();
    }
    bool primitive()
    {
	mark_property();
	if (active_set) active_set->valid = false;
	property.clear();
	return true;
    }
public:
    bool null() override { return primitive(); }
    bool boolean(bool) override { return primitive(); }
    bool number_integer(number_integer_t) override { return primitive(); }
    bool number_unsigned(number_unsigned_t) override { return primitive(); }
    bool number_float(number_float_t, const string_t &) override { return primitive(); }
    bool binary(binary_t &) override { return primitive(); }
    bool string(string_t &value) override
    {
	mark_property();
	if (asset_depth && depth == asset_depth) {
	    if (property == "version") version = value;
	    else if (property == "minVersion") minimum_version = value;
	}
	if (vrm_depth && depth == vrm_depth && property == "specVersion") {
	    vrm_version = value;
	}
	if (active_set && depth == active_set->depth) {
	    if (value.empty()) active_set->valid = false;
	    active_set->values.push_back(value);
	}
	property.clear();
	return true;
    }
    bool start_object(size_t) override
    {
	mark_property();
	if (!depth) root_object = true;
	if (root_object && depth == 1 && property == "asset") asset_depth = depth + 1;
	if (root_object && depth == 1 && property == "extensions") extensions_depth = depth + 1;
	if (extensions_depth && depth == extensions_depth && (property == "VRM" || property == "VRMC_vrm")) {
	    vrm_extension = property;
	    vrm_depth = depth + 1;
	    vrm_version_seen = false;
	    vrm_version.clear();
	    vrm_complete = false;
	}
	if (active_set) active_set->valid = false;
	property.clear();
	return ++depth <= maximum_prefix_depth || !bounded_prefix;
    }
    bool key(string_t &value) override { property = value; return true; }
    bool end_object() override
    {
	if (depth == asset_depth) { asset_depth = 0; asset_complete = true; }
	if (depth == vrm_depth) {vrm_depth = 0; vrm_complete = !vrm_ambiguous && vrm_count == 1;}
	if (depth == extensions_depth) extensions_depth = 0;
	if (root_object && depth == 1) {
	    root_complete = true;
	    if (!required.seen) required.known = true;
	    if (!used.seen) used.known = true;
	}
	property.clear();
	--depth;
	return true;
    }
    bool start_array(size_t) override
    {
	if (active_set) active_set->valid = false;
	if (auto *set = root_set()) {
	    /* Repeated declarations cannot establish a unique complete set. */
	    set->valid = !set->seen;
	    set->seen = true;
	    set->known = false;
	    set->depth = depth + 1;
	    set->values.clear();
	    active_set = set;
	}
	mark_property();
	property.clear();
	return ++depth <= maximum_prefix_depth || !bounded_prefix;
    }
    bool end_array() override
    {
	if (active_set && depth == active_set->depth) {
	    active_set->known = active_set->valid;
	    active_set = nullptr;
	}
	property.clear();
	--depth;
	return true;
    }
    bool parse_error(size_t, const std::string &, const nlohmann::detail::exception &) override { return false; }

};

inline bool version_string(const std::string &value)
{
    size_t dot = value.find('.');
    return dot != std::string::npos && dot && dot + 1 < value.size() &&
	value.find('.', dot + 1) == std::string::npos &&
	value.find_first_not_of("0123456789.") == std::string::npos;
}

inline uint32_t little_uint32(const unsigned char *data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
	(static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

inline bool append_traits(gltf_header &header, bu_mime_traits &traits, uint32_t container_version)
{
    bool added = bu_mime_traits_add(&traits, "encoding", container_version ? "binary" : "utf-8") &&
	(!version_string(header.version) || bu_mime_traits_add(&traits, "version", header.version.c_str()));
    if (container_version) added = added && bu_mime_traits_add(&traits, "byte-order", "little-endian");
    if (header.minimum_version.empty() && header.asset_complete) header.minimum_version = header.version;
    if (version_string(header.minimum_version))
	added = added && bu_mime_traits_add(&traits, "gltf.minimum-version", header.minimum_version.c_str());
    for (const auto &entry : {std::make_pair("gltf.required-extensions", &header.required),
	std::make_pair("gltf.used-extensions", &header.used)}) {
	if (!entry.second->known) continue;
	std::vector<const char *> members;
	for (const auto &member : entry.second->values) members.push_back(member.c_str());
	added = added && bu_mime_traits_add_set(&traits, entry.first, members.data(), members.size());
    }
    if (header.vrm_complete) {
	added = added && bu_mime_traits_add(&traits, "vrm.extension", header.vrm_extension.c_str());
	if (!header.vrm_version.empty()) added = added && bu_mime_traits_add(&traits, "vrm.version", header.vrm_version.c_str());
    }
    return added;
}

/* The decoder validates the complete document. Reuse the SAX observations
 * without retaining arbitrary extension trees or scanning a GLB BIN chunk. */
inline bool file_traits(const char *filename, const char *expected_type, bu_mime_traits &traits)
{
    std::ifstream stream(filename, std::ios::binary);
    if (!stream.is_open()) return false;
    unsigned char bytes[glb_json_offset];
    stream.read(reinterpret_cast<char *>(bytes), sizeof(bytes));
    size_t size = static_cast<size_t>(stream.gcount());
    if (stream.bad()) return false;
    bool binary = size >= 4 && !memcmp(bytes, "glTF", 4);
    const char *type = binary ? "model/gltf-binary" : "model/gltf+json";
    if (expected_type && !BU_STR_EQUAL(type, expected_type)) return false;
    uint32_t container_version = 0;
    if (binary) {
	if (size < sizeof(bytes)) return false;
	container_version = little_uint32(bytes + 4);
	uint32_t chunk_type = little_uint32(bytes + 16);
	if (!valid_container(container_version, chunk_type)) return false;
    }
    stream.clear();
    stream.seekg(binary ? glb_json_offset : 0);
    /* Full decoding has already applied its own document limits. */
    gltf_header header(false);
    if (!stream || !nlohmann::json::sax_parse(stream, &header, nlohmann::json::input_format_t::json, false) ||
	!header.root_complete || !version_string(header.version)) return false;
    if (binary && !bu_mime_traits_add(&traits, "gltf.container-version", std::to_string(container_version).c_str())) return false;
    return append_traits(header, traits, container_version);
}
}
#endif
