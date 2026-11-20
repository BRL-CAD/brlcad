/* M I M E _ D W G . H
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
#ifndef BU_MIME_DWG_H
#define BU_MIME_DWG_H
#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"

namespace mime_dwg {
constexpr const char *type = "image/vnd.dwg";
constexpr size_t header_size = 6;

struct profile {
    bool identified = false, incomplete = false;
    std::string code;

    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "dwg.header-version", code.c_str()) &&
	    bu_mime_traits_add(&traits, "encoding", "binary");
    }
};

inline profile inspect(const unsigned char *data, size_t size, bool complete)
{
    profile out;
    if (!data) return out;
    /* Early identifiers are literal file codes, not decimal versions. */
    static const char *legacy[] = {"MC0.0", "AC1.2", "AC1.3", "AC1.40", "AC1.50", "AC2.10", "AC2.21", "AC2.22"};
    for (const char *code : legacy) {
	const size_t length = strlen(code);
	const size_t available = std::min(size, header_size);
	if (available && !memcmp(data, code, available)) {
	    if (size < header_size) { out.incomplete = !complete; return out; }
	    out.identified = true;
	    out.code.assign(code, length);
	    return out;
	}
    }
    if (!size || data[0] != 'A' || (size > 1 && data[1] != 'C')) return out;
    for (size_t at = 2; at < std::min(size, header_size); ++at)
	if (data[at] < '0' || data[at] > '9') return out;
    if (size < header_size) { out.incomplete = !complete; return out; }
    out.identified = true;
    out.code.assign(reinterpret_cast<const char *>(data), header_size);
    return out;
}
} // namespace mime_dwg
#endif
