/* M I M E _ N O A A _ G R I D . H
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
#ifndef BU_MIME_NOAA_GRID_H
#define BU_MIME_NOAA_GRID_H
#include "common.h"
#include <cstdint>
#include <limits>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"

namespace mime_noaa_grid {
constexpr const char *type = "image/vnd.noaa.ngs-b-grid";
constexpr size_t header_size = 52, marker_size = 4, coordinates_offset = marker_size;
constexpr size_t rows_offset = 36, columns_offset = 40, kind_offset = 44;
constexpr uint32_t header_record_size = header_size - 2 * marker_size;

struct profile {
    bool identified = false, little = false, steps_within_one_degree = true;
    int64_t kind = 0;
    uint32_t rows = 0, columns = 0;
    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified || !bu_mime_traits_add(&traits, "encoding", "binary") ||
	    !bu_mime_traits_add(&traits, "noaa-grid.layout", "b-records") ||
	    !bu_mime_traits_add(&traits, "noaa-grid.steps-within-one-degree", steps_within_one_degree ? "true" : "false") ||
	    !bu_mime_traits_add(&traits, "byte-order", little ? "little-endian" : "big-endian") ||
	    !bu_mime_traits_add(&traits, "noaa-grid.kind", std::to_string(kind).c_str()) ||
	    !bu_mime_traits_add(&traits, "noaa-grid.rows", std::to_string(rows).c_str()) ||
	    !bu_mime_traits_add(&traits, "noaa-grid.columns", std::to_string(columns).c_str())) return false;
	/* Kind -1 is a separate, encoded height representation. Future kinds
	 * retain their literal code without inheriting an older sample layout. */
	if (kind < 0 || kind > 2) return true;
	return bu_mime_traits_add(&traits, "bit-depth", kind == 2 ? "16" : "32") &&
	    bu_mime_traits_add(&traits, "sample-format", kind == 1 ? "ieee754" : "signed-integer");
    }
};

inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < header_size) return out;
    for (bool little : {false, true}) {
	const auto word = [&](size_t at) {return little ? mime_binary::little_u32(data + at) : mime_binary::big_u32(data + at);};
	if (word(0) != header_record_size || word(header_size - marker_size) != header_record_size) continue;
	const uint32_t rows = word(rows_offset), columns = word(columns_offset);
	if (!rows || !columns || rows > std::numeric_limits<int32_t>::max() || columns > std::numeric_limits<int32_t>::max()) continue;
	bool valid = true;
	bool steps_within_one_degree = true;
	for (size_t i = 0; i < 4; ++i) {
	    const unsigned char *value = data + coordinates_offset + i * sizeof(uint64_t);
	    const uint64_t bits = little ? mime_binary::little_u64(value) : mime_binary::big_u64(value);
	    constexpr uint64_t sign = UINT64_C(0x8000000000000000), exponent = UINT64_C(0x7ff0000000000000);
	    /* IEEE positive magnitudes sort in numeric order. Check finite
	     * coordinates and positive steps without host floating conversion. */
	    const uint64_t magnitude = bits & ~sign;
	    constexpr uint64_t one_degree = UINT64_C(0x3ff0000000000000);
	    const uint64_t limit = i == 0 ? UINT64_C(0x4056800000000000) : UINT64_C(0x4076800000000000); // 90 and 360 degrees.
	    valid = valid && (bits & exponent) != exponent && (i < 2 ? magnitude <= limit : !(bits & sign) && magnitude);
	    /* This source predicate lets readers state their resolution limit
	     * without narrowing recognition of the record grammar itself. */
	    if (i >= 2 && magnitude > one_degree) steps_within_one_degree = false;
	}
	if (!valid) continue;
	out.identified = true;
	out.little = little;
	out.steps_within_one_degree = steps_within_one_degree;
	out.rows = rows;
	out.columns = columns;
	const uint32_t kind = word(kind_offset);
	out.kind = kind <= std::numeric_limits<int32_t>::max() ? int64_t(kind) : int64_t(kind) - (INT64_C(1) << 32);
	break;
    }
    return out;
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
