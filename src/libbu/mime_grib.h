/* M I M E _ G R I B . H
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
#ifndef BU_MIME_GRIB_H
#define BU_MIME_GRIB_H
#include "common.h"
#include <array>
#include <cstring>
#include <istream>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"
namespace mime_grib {
constexpr unsigned char signature[] = {'G', 'R', 'I', 'B'};
constexpr size_t edition_offset = 7;
constexpr size_t first_indicator_size = 8;
constexpr size_t second_indicator_size = 16;
constexpr size_t terminator_size = 4;
struct profile {
    bool identified = false, edition_known = false, header_known = false, incomplete = false;
    unsigned edition = 0;
    size_t offset = 0;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    (!edition_known || bu_mime_traits_add(&traits, "grib.first-edition", std::to_string(edition).c_str()));
    }
};
inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data) return out;
    for (size_t at = 0; at + sizeof(signature) <= size; ++at) {
	if (memcmp(data + at, signature, sizeof(signature))) continue;
	const size_t left = size - at;
	if (left < first_indicator_size) {
	    out.identified = out.incomplete = true; out.offset = at; return out;
	}
	const unsigned edition = data[at + edition_offset];
	bool valid = false;
	if (edition == 1) {
	    const unsigned char *length = data + at + sizeof(signature);
	    const uint32_t bytes = (uint32_t(length[0]) << 16) | (uint32_t(length[1]) << 8) | length[2];
	    valid = bytes >= first_indicator_size + terminator_size;
	} else if (edition == 2) {
	    if (left < second_indicator_size) {
		out.identified = out.edition_known = out.incomplete = true;
		out.edition = edition; out.offset = at; return out;
	    }
	    valid = mime_binary::big_u64(data + at + first_indicator_size) >= second_indicator_size + terminator_size;
	} else {
	    /* A binary indicator can declare a future edition without revealing
	     * its layout. A textual mention of GRIB in a preamble cannot. */
	    bool binary = false;
	    for (size_t i = sizeof(signature); i < first_indicator_size; ++i)
		binary = binary || data[at + i] == 0;
	    if (binary) {
		out.identified = out.edition_known = true;
		out.edition = edition; out.offset = at; return out;
	    }
	    if (at == 0) out.identified = true;
	}
	if (!valid) continue;
	out.identified = out.edition_known = out.header_known = true;
	out.edition = edition; out.offset = at; return out;
    }
    return out;
}
inline profile inspect(std::istream &stream)
{
    std::array<unsigned char, second_indicator_size> window = {};
    size_t size = 0, skipped = 0;
    for (int c = stream.get(); c >= 0; c = stream.get()) {
	if (size == window.size()) {
	    memmove(window.data(), window.data() + 1, window.size() - 1);
	    --size;
	    ++skipped;
	}
	window[size++] = static_cast<unsigned char>(c);
	auto out = inspect(window.data(), size);
	if ((out.header_known || (out.edition_known && out.edition != 1 && out.edition != 2)) && !stream.bad()) {
	    out.offset += skipped;
	    return out;
	}
    }
    return {};
}
} // namespace mime_grib
#endif
