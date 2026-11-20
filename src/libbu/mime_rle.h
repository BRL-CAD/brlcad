/* M I M E _ R L E . H
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
#ifndef BU_MIME_RLE_H
#define BU_MIME_RLE_H
#include "common.h"
#include <cstdint>
#include <string>
#include "bu/mime_runtime.h"

/* Only the fixed Utah header is observed here. Variable header blocks and
 * scanline data remain the decoder's responsibility. Byte-swapped files
 * are a reader compatibility feature; standard storage is little-endian. */
namespace mime_rle {
constexpr size_t signature_size = 2;
constexpr size_t fixed_header_size = 15;
constexpr unsigned alpha_flag = 0x04;

struct profile {
    bool identified = false, header_known = false, big_endian = false;
    unsigned flags = 0, depth = 0, colors = 0, map_colors = 0, map_length_bits = 0;

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "rle.variant", "utah") ||
	    !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "byte-order", big_endian ? "big-endian" : "little-endian")) return false;
	if (!header_known) return true;
	return bu_mime_traits_add(&traits, "bit-depth", std::to_string(depth).c_str()) &&
	    bu_mime_traits_add(&traits, "rle.color-channels", std::to_string(colors).c_str()) &&
	    bu_mime_traits_add(&traits, "rle.colormap-channels", std::to_string(map_colors).c_str()) &&
	    bu_mime_traits_add(&traits, "rle.colormap-length-bits", std::to_string(map_length_bits).c_str()) &&
	    bu_mime_traits_add(&traits, "rle.alpha", flags & alpha_flag ? "true" : "false");
    }

    bool matches(const bu_mime_traits *requested, const bu_mime_traits *supplemental = NULL) const
    {
	bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = append_traits(observed) && bu_mime_traits_match(&observed, supplemental, requested);
	bu_mime_traits_free(&observed);
	return valid;
    }
};

inline profile inspect(const unsigned char *data, size_t size)
{
    profile result;
    if (!data || size < signature_size) return result;
    if (data[0] == 0x52 && data[1] == 0xcc) result.identified = true;
    else if (data[0] == 0xcc && data[1] == 0x52) result.identified = result.big_endian = true;
    if (!result.identified || size < fixed_header_size) return result;
    /* The magic and four coordinate/dimension words precede five bytes. */
    const unsigned char *fields = data + signature_size + 4 * sizeof(uint16_t);
    result.flags = fields[0];
    result.colors = fields[1];
    result.depth = fields[2];
    result.map_colors = fields[3];
    result.map_length_bits = fields[4];
    result.header_known = true;
    return result;
}
} // namespace mime_rle
#endif
