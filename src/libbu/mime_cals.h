/* M I M E _ C A L S . H
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
#ifndef BU_MIME_CALS_H
#define BU_MIME_CALS_H
#include "common.h"
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"

namespace mime_cals {
constexpr size_t record_size = 128;
constexpr size_t header_size = 2048;
constexpr const char *type = "application/cals-1840";

struct profile {
    bool identified = false;
    bool complete = false;
    bool consistent = true;
    std::string raster_type, orientation, dimensions;

    bool type_one() const
    {
	return complete && consistent && raster_type == "1" && !orientation.empty() && !dimensions.empty();
    }

    bool append_traits(bu_mime_traits &traits) const
    {
	if (!identified) return false;
	/* Later header records may contradict an earlier declaration. */
	if (!complete || !consistent || raster_type.empty()) return true;
	/* Later CALS layouts use dtype rather than this driver's rtype record. */
	if (!bu_mime_traits_add(&traits, "cals.header-layout", "rtype") ||
	    !bu_mime_traits_add(&traits, "cals.raster-type", raster_type.c_str())) return false;
	return !type_one() || (bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "bit-depth", "1") &&
	    bu_mime_traits_add(&traits, "compression", "ccitt-group4"));
    }
};

inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data) return out;
    const char *first_fields[] = {"srcdocid:", "rtype:"};
    for (const char *field : first_fields)
	if (size >= strlen(field) && !memcmp(data, field, strlen(field))) out.identified = true;
    if (!out.identified || size < header_size) return out;
    out.complete = true;
    for (size_t at = 0; at < header_size; at += record_size) {
	const std::string record(reinterpret_cast<const char *>(data + at), record_size);
	const size_t colon = record.find(':');
	if (colon == std::string::npos) continue;
	const std::string key = record.substr(0, colon);
	std::string *field = key == "rtype" ? &out.raster_type :
	    key == "rorient" ? &out.orientation : key == "rpelcnt" ? &out.dimensions : NULL;
	if (!field) continue;
	const size_t begin = record.find_first_not_of(' ', colon + 1);
	const size_t end = record.find_last_not_of(' ');
	const std::string value = begin == std::string::npos ? "" : record.substr(begin, end - begin + 1);
	if (value.empty() || (!field->empty() && *field != value)) out.consistent = false;
	if (key == "rtype" && value.find_first_not_of("0123456789") != std::string::npos) out.consistent = false;
	*field = value;
    }
    return out;
}
} // namespace mime_cals
#endif
