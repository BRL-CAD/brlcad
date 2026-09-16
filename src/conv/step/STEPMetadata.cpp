/*                   S T E P M E T A D A T A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPMetadata.h"

#include <cstdint>
#include <limits>

#include "raytrace.h"
#include "rt/nongeom.h"

namespace brlcad {
namespace step {

const char *const STEP_METADATA_OBJECT = "_STEP_METADATA";
const char *const STEP_METADATA_FORMAT = "brlcad-step-metadata-map-v1";
const char *const STEP_METADATA_OBJECT_ATTRIBUTE = "STEP::METADATA::OBJECT";
const char *const STEP_METADATA_FORMAT_ATTRIBUTE = "STEP::METADATA::FORMAT";
const char *const STEP_METADATA_RECORDS_ATTRIBUTE = "STEP::METADATA::RECORDS";

namespace {

const unsigned char metadata_magic[] = {
    'B', 'R', 'L', '-', 'C', 'A', 'D', ' ', 'S', 'T', 'E', 'P', 'M', 'E', 'T', 'A'
};
const uint32_t metadata_version = 1;

void
append_u32(std::vector<unsigned char> &bytes, uint32_t value)
{
    bytes.push_back(static_cast<unsigned char>((value >> 24) & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 16) & 0xff));
    bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    bytes.push_back(static_cast<unsigned char>(value & 0xff));
}

void
append_u64(std::vector<unsigned char> &bytes, uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8)
	bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

bool
read_u32(const unsigned char *bytes, size_t byte_count, size_t &offset,
    uint32_t &value)
{
    if (offset > byte_count || byte_count - offset < 4) return false;
    value = (static_cast<uint32_t>(bytes[offset]) << 24) |
	(static_cast<uint32_t>(bytes[offset + 1]) << 16) |
	(static_cast<uint32_t>(bytes[offset + 2]) << 8) |
	static_cast<uint32_t>(bytes[offset + 3]);
    offset += 4;
    return true;
}

bool
read_u64(const unsigned char *bytes, size_t byte_count, size_t &offset,
    uint64_t &value)
{
    if (offset > byte_count || byte_count - offset < 8) return false;
    value = 0;
    for (size_t i = 0; i < 8; ++i)
	value = (value << 8) | static_cast<uint64_t>(bytes[offset + i]);
    offset += 8;
    return true;
}

bool
append_size(size_t &total, size_t add)
{
    if (add > std::numeric_limits<size_t>::max() - total) return false;
    total += add;
    return true;
}

} // namespace

bool
EncodeSTEPMetadata(const std::map<std::string, std::string> &attributes,
    std::vector<unsigned char> &bytes, std::string &error)
{
    error.clear();
    size_t byte_count = sizeof(metadata_magic) + 4 + 8;
    for (const auto &attribute : attributes) {
	if (!append_size(byte_count, 16) ||
		!append_size(byte_count, attribute.first.size()) ||
		!append_size(byte_count, attribute.second.size())) {
	    error = "retained STEP metadata exceeds addressable memory";
	    return false;
	}
    }

    std::vector<unsigned char> encoded;
    encoded.reserve(byte_count);
    encoded.insert(encoded.end(), metadata_magic,
	metadata_magic + sizeof(metadata_magic));
    append_u32(encoded, metadata_version);
    append_u64(encoded, static_cast<uint64_t>(attributes.size()));
    for (const auto &attribute : attributes) {
	append_u64(encoded, static_cast<uint64_t>(attribute.first.size()));
	append_u64(encoded, static_cast<uint64_t>(attribute.second.size()));
	encoded.insert(encoded.end(), attribute.first.begin(), attribute.first.end());
	encoded.insert(encoded.end(), attribute.second.begin(), attribute.second.end());
    }
    bytes.swap(encoded);
    return true;
}

bool
DecodeSTEPMetadata(const unsigned char *bytes, size_t byte_count,
    std::map<std::string, std::string> &attributes, std::string &error)
{
    error.clear();
    if (!bytes || byte_count < sizeof(metadata_magic) + 12) {
	error = "retained STEP metadata payload is truncated";
	return false;
    }
    for (size_t i = 0; i < sizeof(metadata_magic); ++i) {
	if (bytes[i] != metadata_magic[i]) {
	    error = "retained STEP metadata has an unrecognized format";
	    return false;
	}
    }

    size_t offset = sizeof(metadata_magic);
    uint32_t version = 0;
    uint64_t record_count = 0;
    if (!read_u32(bytes, byte_count, offset, version) ||
	    !read_u64(bytes, byte_count, offset, record_count)) {
	error = "retained STEP metadata header is truncated";
	return false;
    }
    if (version != metadata_version) {
	error = "unsupported retained STEP metadata version " +
	    std::to_string(version);
	return false;
    }
    if (record_count > static_cast<uint64_t>((byte_count - offset) / 16)) {
	error = "retained STEP metadata record count is invalid";
	return false;
    }

    std::map<std::string, std::string> decoded;
    for (uint64_t i = 0; i < record_count; ++i) {
	uint64_t key_size = 0;
	uint64_t value_size = 0;
	if (!read_u64(bytes, byte_count, offset, key_size) ||
		!read_u64(bytes, byte_count, offset, value_size) ||
		key_size > static_cast<uint64_t>(byte_count - offset) ||
		value_size > static_cast<uint64_t>(byte_count - offset -
		    static_cast<size_t>(key_size))) {
	    error = "retained STEP metadata record is truncated";
	    return false;
	}
	const std::string key(reinterpret_cast<const char *>(bytes + offset),
	    static_cast<size_t>(key_size));
	offset += static_cast<size_t>(key_size);
	const std::string value(reinterpret_cast<const char *>(bytes + offset),
	    static_cast<size_t>(value_size));
	offset += static_cast<size_t>(value_size);
	if (!decoded.emplace(key, value).second) {
	    error = "retained STEP metadata contains a duplicate key";
	    return false;
	}
    }
    if (offset != byte_count) {
	error = "retained STEP metadata contains trailing data";
	return false;
    }
    attributes.swap(decoded);
    return true;
}

bool
ReadSTEPMetadata(struct db_i *database,
    std::map<std::string, std::string> &attributes, bool &found,
    std::string &error)
{
    found = false;
    error.clear();
    if (!database) {
	error = "no BRL-CAD database supplied for retained STEP metadata";
	return false;
    }
    struct directory *entry = db_lookup(database, STEP_METADATA_OBJECT,
	LOOKUP_QUIET);
    if (entry == RT_DIR_NULL) return true;
    found = true;

    struct rt_db_internal internal;
    RT_DB_INTERNAL_INIT(&internal);
    if (rt_db_get_internal(&internal, entry, database, bn_mat_identity) < 0) {
	error = "could not read retained STEP metadata object";
	return false;
    }
    if ((internal.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0 ||
	    !internal.idb_ptr) {
	error = "retained STEP metadata object is not an unsigned byte array";
	rt_db_free_internal(&internal);
	return false;
    }
    const struct rt_binunif_internal *binary =
	static_cast<const struct rt_binunif_internal *>(internal.idb_ptr);
    if (binary->type != DB5_MINORTYPE_BINU_8BITINT_U) {
	error = "retained STEP metadata object is not an unsigned byte array";
	rt_db_free_internal(&internal);
	return false;
    }
    const bool decoded = DecodeSTEPMetadata(binary->u.uint8, binary->count,
	attributes, error);
    rt_db_free_internal(&internal);
    return decoded;
}

bool
IsSTEPMetadataLocatorAttribute(const std::string &name)
{
    return name == STEP_METADATA_OBJECT_ATTRIBUTE ||
	name == STEP_METADATA_FORMAT_ATTRIBUTE ||
	name == STEP_METADATA_RECORDS_ATTRIBUTE;
}

} // namespace step
} // namespace brlcad
