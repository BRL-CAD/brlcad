/* M I M E _ S U N . H
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
#ifndef BU_MIME_SUN_H
#define BU_MIME_SUN_H
#include "common.h"
#include <cstdint>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_sun {
constexpr unsigned char signature[] = {0x59, 0xa6, 0x6a, 0x95};
enum field {magic, width, height, depth, raster_length, raster_type, map_type, map_length, field_count};
constexpr size_t fixed_header_size = field_count * sizeof(uint32_t);

/* A borrowed header view observes declarations without claiming that raster
 * data or colormaps are implemented, present or valid. */
class header {
    const unsigned char *data;
    size_t size;
    bool has_field(field index) const
    {
	const size_t word_index = static_cast<size_t>(index);
	return word_index < static_cast<size_t>(field_count) && word_index < size / sizeof(uint32_t);
    }
public:
    header(const unsigned char *bytes, size_t length) : data(bytes), size(length) {}
    bool identified() const
    {
	return data && size >= sizeof(signature) && !memcmp(data, signature, sizeof(signature));
    }
    bool complete() const {return identified() && size >= fixed_header_size;}
    uint32_t word(field index) const
    {
	if (!identified() || !has_field(index)) return 0;
	const unsigned char *p = data + static_cast<size_t>(index) * sizeof(uint32_t);
	return mime_binary::big_u32(p);
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified() || !bu_mime_traits_add(&traits, "encoding", "binary")) return false;
	const struct {const char *key; field index;} fields[] = {
	    {"bit-depth", depth}, {"sun.raster-type", raster_type}, {"sun.map-type", map_type}
	};
	for (const auto &item : fields)
	    if (has_field(item.index) &&
		!bu_mime_traits_add(&traits, item.key, std::to_string(word(item.index)).c_str())) return false;
	return true;
    }
    bool matches(const bu_mime_traits *requested, const bu_mime_traits *supplemental = NULL) const
    {
	bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = append_traits(observed) && bu_mime_traits_match(&observed, supplemental, requested);
	bu_mime_traits_free(&observed);
	return valid;
    }
};
} // namespace mime_sun
#endif
