/* M I M E _ A S S B I N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef BU_MIME_ASSBIN_H
#define BU_MIME_ASSBIN_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime.h"
#include "mime_binary.h"

namespace mime_assbin {
inline constexpr const char *type = "model/vnd.assimp.binary";
inline constexpr char signature[] = "ASSIMP.binary-dump.";
constexpr size_t version_at = 44, revision_at = 52, flags_at = 56, shortened_at = 60, compression_at = 62;
constexpr size_t fields_size = 64, header_size = 512;
/* Public ASSIMP_CFLAGS_DOUBLE_SUPPORT, kept independent of the SDK. */
constexpr uint32_t double_support = 0x20;

struct profile {
    const unsigned char *data = NULL;
    size_t size = 0;
    bool identified = false, incomplete = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "encoding", "binary")) return false;
	if (size < revision_at) return true;
	const uint32_t major = mime_binary::little_u32(data + version_at);
	const uint32_t minor = mime_binary::little_u32(data + version_at + sizeof(uint32_t));
	if (!bu_mime_traits_add(&traits, "assimp.major-word-le", std::to_string(major).c_str()) ||
	    !bu_mime_traits_add(&traits, "assimp.minor-word-le", std::to_string(minor).c_str())) return false;
	/* The specification is little endian, but SDK dumps use native words.
	 * Only known 1.0 headers distinguish those representations. */
	const bool big = mime_binary::big_u32(data + version_at) == 1 &&
	    mime_binary::big_u32(data + version_at + sizeof(uint32_t)) == 0;
	if (!(major == 1 && minor == 0) && !big) return true;
	if (!bu_mime_traits_add(&traits, "version", "1.0") ||
	    !bu_mime_traits_add(&traits, "byte-order", big ? "big-endian" : "little-endian")) return false;
	const auto word = [&](size_t at) {return big ? mime_binary::big_u32(data + at) : mime_binary::little_u32(data + at);};
	const auto short_word = [&](size_t at) {return big ? mime_binary::big_u16(data + at) : mime_binary::little_u16(data + at);};
	if (size >= flags_at && !bu_mime_traits_add(&traits, "assimp.producer-revision",
	    std::to_string(word(revision_at)).c_str())) return false;
	if (size >= shortened_at) {
	    const uint32_t flags = word(flags_at);
	    if (!bu_mime_traits_add(&traits, "assimp.compile-flags", std::to_string(flags).c_str()) ||
		!bu_mime_traits_add(&traits, "assimp.real-width", flags & double_support ? "64" : "32")) return false;
	}
	if (size >= compression_at) {
	    const uint16_t shortened = short_word(shortened_at);
	    if (!bu_mime_traits_add(&traits, "assimp.shortened-flag", std::to_string(shortened).c_str()) ||
		!bu_mime_traits_add(&traits, "assimp.shortened", shortened ? "true" : "false")) return false;
	}
	if (size >= fields_size) {
	    const uint16_t compressed = short_word(compression_at);
	    if (!bu_mime_traits_add(&traits, "assimp.compression-flag", std::to_string(compressed).c_str()) ||
		!bu_mime_traits_add(&traits, "compression", compressed ? "zlib" : "none")) return false;
	}
	return true;
    }
};

inline profile inspect(const bu_mime_probe_input &input)
{
    profile result;
    const size_t length = sizeof(signature) - 1;
    const size_t available = std::min(input.size, length);
    if (!available || memcmp(input.data, signature, available)) return result;
    result.data = input.data;
    result.size = input.size;
    result.identified = input.size >= length;
    result.incomplete = input.size < fields_size && !input.complete;
    return result;
}
}
#endif
