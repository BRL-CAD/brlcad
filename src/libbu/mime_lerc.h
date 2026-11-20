/* M I M E _ L E R C . H
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
#ifndef BU_MIME_LERC_H
#define BU_MIME_LERC_H
#include "common.h"
#include <cstdint>
#include <cstring>
#include <istream>
#include <limits>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_lerc {
constexpr const char *type = "image/vnd.esri.lerc";
constexpr char lerc1_signature[] = "CntZImage ";
constexpr char lerc2_signature[] = "Lerc2 ";
constexpr size_t largest_header_size = 90;
/* MRF writes the historical decoder padding after each LERC tile. */
constexpr size_t mrf_padding_bytes = 3;

inline bool single_blob_extent(uint64_t blob_size, uint64_t source_size)
{
    return blob_size && source_size >= blob_size &&
	(source_size - blob_size == 0 || source_size - blob_size == mrf_padding_bytes);
}
constexpr size_t lerc1_mask_header = 34, band_header_size = 16;
constexpr unsigned int lerc1_revision = 11, lerc1_cntz_type = 8;
struct sample_type {const char *name, *bits, *kind;};
/* The array order is the LERC2 wire datatype enumeration. */
constexpr sample_type sample_types[] = {
    {"int8", "8", "signed-integer"}, {"uint8", "8", "unsigned-integer"},
    {"int16", "16", "signed-integer"}, {"uint16", "16", "unsigned-integer"},
    {"int32", "32", "signed-integer"}, {"uint32", "32", "unsigned-integer"},
    {"float32", "32", "ieee754"}, {"float64", "64", "ieee754"}
};
constexpr size_t lerc1_data_header = lerc1_mask_header + band_header_size;

struct profile {
    bool identified = false, incomplete = false, header_known = false, single_blob = false;
    unsigned int revision = 0;
    int data_type = -1;
    uint32_t depth = 0, image_type = 0;
    uint64_t blob_size = 0, data_header_offset = 0, trailing_bytes = 0;
    std::string codec;

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	if (!bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "lerc.first-blob.codec", codec.c_str())) return false;
	if (revision && !bu_mime_traits_add(&traits, "lerc.first-blob.version", std::to_string(revision).c_str())) return false;
	if (!header_known) return true;
	if (!bu_mime_traits_add(&traits, "byte-order", "little-endian")) return false;
	if (single_blob && !bu_mime_traits_add(&traits, "lerc.trailing-bytes", std::to_string(trailing_bytes).c_str())) return false;
	if (codec == "lerc1")
	    return bu_mime_traits_add(&traits, "lerc.first-blob.image-type", std::to_string(image_type).c_str()) &&
		(!single_blob || bu_mime_traits_add(&traits, "lerc.blob-count", "1"));
	const auto &sample = sample_types[data_type];
	if (!bu_mime_traits_add(&traits, "lerc.first-blob.data-type", sample.name) ||
	    !bu_mime_traits_add(&traits, "lerc.first-blob.depth", std::to_string(depth).c_str())) return false;
	if (!single_blob) return true;
	/* A bounded first header cannot establish every concatenated band's
	 * type. Publish global sample facts only for one complete blob. */
	return bu_mime_traits_add(&traits, "lerc.blob-count", "1") &&
	    bu_mime_traits_add(&traits, "bit-depth", sample.bits) && bu_mime_traits_add(&traits, "sample-format", sample.kind);
    }
};

inline void lerc1_extent(profile &out, const unsigned char *data)
{
    const uint32_t rows = mime_binary::little_u32(data), columns = mime_binary::little_u32(data + sizeof(uint32_t));
    const uint32_t bytes = mime_binary::little_u32(data + 2 * sizeof(uint32_t));
    constexpr uint32_t largest_integer = std::numeric_limits<int32_t>::max();
    if (rows && rows <= largest_integer && columns && columns <= largest_integer && bytes <= largest_integer)
	out.blob_size = out.data_header_offset + band_header_size + bytes;
}

inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data) return out;
    size_t signature_size = 0;
    if (size >= sizeof(lerc1_signature) - 1 && !memcmp(data, lerc1_signature, sizeof(lerc1_signature) - 1)) {
	out.codec = "lerc1";
	signature_size = sizeof(lerc1_signature) - 1;
    } else if (size >= sizeof(lerc2_signature) - 1 && !memcmp(data, lerc2_signature, sizeof(lerc2_signature) - 1)) {
	out.codec = "lerc2";
	signature_size = sizeof(lerc2_signature) - 1;
    } else return out;
    out.identified = true;
    out.incomplete = !complete;
    if (size < signature_size + sizeof(uint32_t)) return out;
    const uint32_t revision = mime_binary::little_u32(data + signature_size);
    if (!revision || revision > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) { out.incomplete = false; return out; }
    out.revision = revision;
    /* Keep future revisions observable without applying an old layout. */
    if (out.codec == "lerc1") {
	if (revision != lerc1_revision) { out.incomplete = false; return out; }
	if (size < lerc1_data_header) return out;
	out.image_type = mime_binary::little_u32(data + signature_size + sizeof(uint32_t));
	if (out.image_type != lerc1_cntz_type) { out.incomplete = false; return out; }
	const uint32_t rows = mime_binary::little_u32(data + signature_size + 2 * sizeof(uint32_t));
	const uint32_t columns = mime_binary::little_u32(data + signature_size + 3 * sizeof(uint32_t));
	const uint32_t mask_bytes = mime_binary::little_u32(data + lerc1_mask_header + 2 * sizeof(uint32_t));
	if (!rows || !columns || rows > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
	    columns > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
	    mime_binary::little_u32(data + lerc1_mask_header) ||
	    mime_binary::little_u32(data + lerc1_mask_header + sizeof(uint32_t))) { out.incomplete = false; return out; }
	if (mask_bytes > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) { out.incomplete = false; return out; }
	out.header_known = true;
	out.data_header_offset = lerc1_data_header + static_cast<uint64_t>(mask_bytes);
	if (out.data_header_offset <= size && size - static_cast<size_t>(out.data_header_offset) >= band_header_size)
	    lerc1_extent(out, data + static_cast<size_t>(out.data_header_offset));
    } else {
	constexpr unsigned int latest_layout = 6;
	if (revision > latest_layout) { out.incomplete = false; return out; }
	const size_t fields = signature_size + sizeof(uint32_t) + (revision >= 3 ? sizeof(uint32_t) : 0);
	const size_t dimension_fields = revision >= 4 ? 3 : 2;
	const size_t blob_offset = fields + (dimension_fields + 2) * sizeof(uint32_t);
	const size_t type_offset = blob_offset + sizeof(uint32_t);
	constexpr size_t wire_double_size = 8;
	const size_t header_size = type_offset + sizeof(uint32_t) +
	    (revision >= 6 ? 2 * sizeof(uint32_t) + 5 * wire_double_size : 3 * wire_double_size);
	if (size < header_size) return out;
	const uint32_t datatype = mime_binary::little_u32(data + type_offset);
	out.depth = revision >= 4 ? mime_binary::little_u32(data + fields + 2 * sizeof(uint32_t)) : 1;
	const uint32_t bytes = mime_binary::little_u32(data + blob_offset);
	const uint32_t rows = mime_binary::little_u32(data + fields), columns = mime_binary::little_u32(data + fields + sizeof(uint32_t));
	if (!rows || !columns || rows > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
	    columns > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
	    datatype >= sizeof(sample_types) / sizeof(sample_types[0]) || !out.depth || out.depth > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
	    bytes < header_size || bytes > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
	    out.incomplete = false;
	    return out;
	}
	out.data_type = static_cast<int>(datatype);
	out.blob_size = bytes;
	out.header_known = true;
    }
    out.single_blob = complete && single_blob_extent(out.blob_size, size);
    if (out.single_blob) out.trailing_bytes = size - out.blob_size;
    out.incomplete = !complete && (!out.header_known || (out.codec == "lerc1" && !out.blob_size));
    return out;
}

inline profile inspect(std::istream &stream)
{
    unsigned char prefix[largest_header_size];
    stream.read(reinterpret_cast<char *>(prefix), sizeof(prefix));
    auto out = inspect(prefix, static_cast<size_t>(stream.gcount()), false);
    if (!out.identified || !out.header_known || stream.bad()) return out;
    stream.clear();
    stream.seekg(0, std::ios::end);
    const auto end = stream.tellg();
    if (!stream || end < 0) return out;
    const auto size = static_cast<uint64_t>(end);
    if (out.codec == "lerc1" && !out.blob_size && out.data_header_offset <= size && size - out.data_header_offset >= band_header_size) {
	stream.seekg(static_cast<std::streamoff>(out.data_header_offset));
	unsigned char band[band_header_size];
	stream.read(reinterpret_cast<char *>(band), sizeof(band));
	if (stream) lerc1_extent(out, band);
    }
    out.single_blob = single_blob_extent(out.blob_size, size);
    if (out.single_blob) out.trailing_bytes = size - out.blob_size;
    out.incomplete = false;
    return out;
}
} // namespace mime_lerc
#endif
