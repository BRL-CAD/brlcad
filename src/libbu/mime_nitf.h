/* M I M E _ N I T F . H
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
#ifndef BU_MIME_NITF_H
#define BU_MIME_NITF_H
#include "common.h"
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
namespace mime_nitf {
constexpr size_t signature_size = 4;
constexpr size_t header_size = 9;
struct profile {
    std::string standard, header_version, version;
    bool identified = false, complete = false;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "nitf.standard", standard.c_str()) &&
	    (header_version.empty() || bu_mime_traits_add(&traits, "nitf.header-version", header_version.c_str())) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str()));
    }
};
inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < signature_size) return out;
    if (!memcmp(data, "NITF", signature_size)) out.standard = "nitf";
    else if (!memcmp(data, "NSIF", signature_size)) out.standard = "nsif";
    else return out;
    out.identified = true;
    if (size < header_size) return out;
    const auto digit = [](unsigned char c) {return c >= '0' && c <= '9';};
    const unsigned char *v = data + signature_size;
    if (!digit(v[0]) || !digit(v[1]) || v[2] != '.' || !digit(v[3]) || !digit(v[4])) return out;
    out.header_version.assign(reinterpret_cast<const char *>(v), header_size - signature_size);
    out.complete = true;
    /* FHDR codes are identifiers, rather than decimal version arithmetic. */
    if (out.standard == "nitf") {
	if (out.header_version == "01.00") out.version = "1.0";
	else if (out.header_version == "01.10") out.version = "1.1";
	else if (out.header_version == "02.00") out.version = "2.0";
	else if (out.header_version == "02.10") out.version = "2.1";
    } else if (out.header_version == "01.00") out.version = "1.0";
    return out;
}
} // namespace mime_nitf
#endif
