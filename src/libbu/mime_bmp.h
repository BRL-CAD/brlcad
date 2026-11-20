/* M I M E _ B M P . H
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
#ifndef BU_MIME_BMP_H
#define BU_MIME_BMP_H
#include "common.h"
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"
namespace mime_bmp {
constexpr unsigned char signature[] = {'B', 'M'};
constexpr size_t file_header_size = 14;
enum dib_header_size {core = 12, os2_short = 16, info = 40, rgb_masks = 52,
    alpha_mask = 56, os2_full = 64, v4 = 108, v5 = 124};
constexpr size_t largest_header_size = file_header_size + v5;
constexpr size_t core_depth_offset = 10;
constexpr size_t info_depth_offset = 14;
constexpr size_t info_compression_offset = 16;
struct profile {
    bool identified = false, size_known = false, depth_known = false, compression_known = false, incomplete = false;
    uint32_t header_size = 0, depth = 0, compression = 0;
    bool known_layout() const
    {
	return header_size == core || header_size == os2_short || header_size == info || header_size == rgb_masks ||
	    header_size == alpha_mask || header_size == os2_full || header_size == v4 || header_size == v5;
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    (!size_known || bu_mime_traits_add(&traits, "bmp.header-size", std::to_string(header_size).c_str())) &&
	    (!depth_known || bu_mime_traits_add(&traits, "bit-depth", std::to_string(depth).c_str())) &&
	    (!compression_known || bu_mime_traits_add(&traits, "bmp.compression-code", std::to_string(compression).c_str()));
    }
};
inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < sizeof(signature) || memcmp(data, signature, sizeof(signature))) return out;
    out.identified = true;
    out.incomplete = true;
    if (size < file_header_size + sizeof(uint32_t)) return out;
    out.size_known = true;
    out.header_size = mime_binary::little_u32(data + file_header_size);
    out.incomplete = size - file_header_size < out.header_size;
    if (!out.known_layout()) return out;
    /* CORE stores two-octet dimensions; INFO and its extended forms use
     * four-octet dimensions before their plane/depth fields. */
    const size_t depth_offset = file_header_size + (out.header_size == core ? core_depth_offset : info_depth_offset);
    if (size >= depth_offset + sizeof(uint16_t)) {
	out.depth_known = true;
	out.depth = mime_binary::little_u16(data + depth_offset);
    }
    const size_t compression_offset = file_header_size + info_compression_offset;
    if (out.header_size >= info && size >= compression_offset + sizeof(uint32_t)) {
	out.compression_known = true;
	out.compression = mime_binary::little_u32(data + compression_offset);
    }
    return out;
}
} // namespace mime_bmp
#endif
