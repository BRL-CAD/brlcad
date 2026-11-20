/* M I M E _ B L E N D E R . H
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
#ifndef BU_MIME_BLENDER_H
#define BU_MIME_BLENDER_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime.h"

namespace mime_blender {
inline constexpr const char *type = "application/x-blender";
inline constexpr char signature[] = "BLENDER";
constexpr size_t legacy_header_size = 12, modern_header_size = 17;

struct profile {
    bool identified = false, incomplete = false, valid = false;
    std::string format, file_version, pointer_width, byte_order;
    bool append_traits(bu_mime_traits &traits, const char *compression = "none") const
    {
	if (!identified) return false;
	if (!bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "compression", compression)) return false;
	if (!valid) return true;
	/* The last two decimal digits encode the minor file version. The
	 * block layout has its own revision, independent of this number. */
	const unsigned version = static_cast<unsigned>(std::stoul(file_version));
	const std::string revision = std::to_string(version / 100) + "." + std::to_string(version % 100);
	return bu_mime_traits_add(&traits, "version", revision.c_str()) &&
	    bu_mime_traits_add(&traits, "blender.file-version", file_version.c_str()) &&
	    bu_mime_traits_add(&traits, "blender.format-version", format.c_str()) &&
	    bu_mime_traits_add(&traits, "blender.pointer-width", pointer_width.c_str()) &&
	    bu_mime_traits_add(&traits, "byte-order", byte_order.c_str());
    }
};

inline bool digits(const unsigned char *data, size_t size)
{
    return std::all_of(data, data + size, [](unsigned char c) {return c >= '0' && c <= '9';});
}

inline profile inspect(const bu_mime_probe_input &input)
{
    profile result;
    const size_t magic_size = sizeof(signature) - 1;
    const size_t available = std::min(input.size, magic_size);
    if (!available || memcmp(input.data, signature, available)) return result;
    result.incomplete = !input.complete;
    if (input.size < magic_size) return result;
    result.identified = true;
    if (input.size <= magic_size) return result;
    const bool legacy = input.data[magic_size] == '_' || input.data[magic_size] == '-';
    const size_t size = legacy ? legacy_header_size : modern_header_size;
    if (!legacy && input.data[magic_size] != '1') {result.incomplete = false; return result;}
    if (input.size < size) return result;
    result.incomplete = false;
    const size_t version_at = legacy ? 9 : 13;
    const size_t version_size = legacy ? 3 : 4;
    const size_t endian_at = legacy ? 8 : 12;
    if (!digits(input.data + version_at, version_size)) return result;
    if (legacy) {
	if (input.data[endian_at] != 'v' && input.data[endian_at] != 'V') return result;
	result.format = "0";
	result.pointer_width = input.data[magic_size] == '_' ? "32" : "64";
    } else {
	/* Only format 1 defines this extended header and its fixed layout.
	 * A future revision retains identity without borrowing these facts. */
	if (memcmp(input.data + magic_size, "17-01v", sizeof("17-01v") - 1)) return result;
	result.format = "1";
	result.pointer_width = "64";
    }
    result.file_version.assign(reinterpret_cast<const char *>(input.data + version_at), version_size);
    result.byte_order = input.data[endian_at] == 'v' ? "little-endian" : "big-endian";
    result.valid = true;
    return result;
}
}
#endif
