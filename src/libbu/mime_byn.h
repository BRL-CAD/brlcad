/* M I M E _ B Y N . H
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
#ifndef BU_MIME_BYN_H
#define BU_MIME_BYN_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_byn {
constexpr const char *type = "image/vnd.nrcan.byn";
constexpr size_t header_size = 80, factor_offset = 24, sample_size_offset = 32, byte_order_offset = 48;
constexpr size_t latitude_step = 16, longitude_step = 18, global = 20, data_type = 22, vertical_datum = 34;
constexpr size_t description = 40, subtype = 42, datum = 44, ellipsoid = 46, boundary_scale = 50;
constexpr size_t first_reserved = 36, first_reserved_end = 40, last_reserved = 78;

struct profile {
    bool identified = false, little = false, unit_scale = false;
    unsigned int sample_size = 0;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "byn.header-byte-order", "little-endian") &&
	    bu_mime_traits_add(&traits, "byte-order", little ? "little-endian" : "big-endian") &&
	    bu_mime_traits_add(&traits, "bit-depth", std::to_string(sample_size * 8).c_str()) &&
	    bu_mime_traits_add(&traits, "sample-format", "signed-integer") &&
	    bu_mime_traits_add(&traits, "byn.unit-scale", unit_scale ? "true" : "false");
    }
};

inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < header_size) return out;
    const auto word = [&](size_t at) {return mime_binary::little_u16(data + at);};
    out.sample_size = word(sample_size_offset);
    const unsigned int order = word(byte_order_offset);
    /* BYN has no magic. Use its defined header ranges and reserved bytes
     * as heuristic evidence, independently of its shared .err suffix. */
    if ((out.sample_size != 2 && out.sample_size != 4) || order > 1 ||
	word(latitude_step) == 0 || word(latitude_step) > std::numeric_limits<int16_t>::max() ||
	word(longitude_step) == 0 || word(longitude_step) > std::numeric_limits<int16_t>::max() ||
	word(global) > 1 || word(data_type) > 9 || word(vertical_datum) > 3 || word(description) > 3 ||
	word(subtype) > 9 || word(datum) > 1 || word(ellipsoid) > 7 || word(boundary_scale) > 1 ||
	std::any_of(data + first_reserved, data + first_reserved_end, [](unsigned char c) {return c != 0;}) || word(last_reserved)) return out;
    out.identified = true;
    out.little = order == 1;
    /* Compare the IEEE wire value for one without host floating-point
     * conversion. The current terrain adapter does not apply band scales. */
    constexpr unsigned char one[] = {0, 0, 0, 0, 0, 0, 0xf0, 0x3f};
    out.unit_scale = !memcmp(data + factor_offset, one, sizeof(one));
    return out;
}
}
#endif
