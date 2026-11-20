/* M I M E _ G E O P A C K A G E . H
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
#ifndef BU_MIME_GEOPACKAGE_H
#define BU_MIME_GEOPACKAGE_H
#include "common.h"
#include <cstring>
#include <string>
#include "bu/mime_runtime.h"
#include "mime_binary.h"
namespace mime_geopackage {
constexpr char sqlite_signature[] = "SQLite format 3";
constexpr size_t application_id_offset = 68;
constexpr size_t user_version_offset = 60;
constexpr size_t header_size = 100;
constexpr uint32_t major_version_scale = 10000;
constexpr uint32_t minor_version_scale = 100;
struct profile {
    std::string application_id, version;
    bool identified = false;
    uint32_t user_version = 0;
    bool append_traits(bu_mime_traits &traits) const
    {
	return identified && bu_mime_traits_add(&traits, "encoding", "binary") &&
	    bu_mime_traits_add(&traits, "container", "sqlite3") &&
	    bu_mime_traits_add(&traits, "geopackage.application-id", application_id.c_str()) &&
	    bu_mime_traits_add(&traits, "geopackage.user-version", std::to_string(user_version).c_str()) &&
	    (version.empty() || bu_mime_traits_add(&traits, "version", version.c_str()));
    }
};
inline profile inspect(const unsigned char *data, size_t size)
{
    profile out;
    if (!data || size < application_id_offset + sizeof(uint32_t) ||
	memcmp(data, sqlite_signature, sizeof(sqlite_signature))) return out;
    const char *identifier = reinterpret_cast<const char *>(data + application_id_offset);
    if (!memcmp(identifier, "GPKG", sizeof(uint32_t))) out.application_id = "GPKG";
    else if (!memcmp(identifier, "GP10", sizeof(uint32_t))) {out.application_id = "GP10"; out.version = "1.0";}
    else if (!memcmp(identifier, "GP11", sizeof(uint32_t))) {out.application_id = "GP11"; out.version = "1.1";}
    else return out;
    out.identified = true;
    out.user_version = mime_binary::big_u32(data + user_version_offset);
    /* Modern five-digit user_version encodes major, minor and patch. The
     * registered version parameter contains major/minor only. Legacy IDs
     * establish their family independently of this later field. */
    if (out.application_id == "GPKG" && out.user_version >= major_version_scale &&
	out.user_version < 10 * major_version_scale)
	out.version = std::to_string(out.user_version / major_version_scale) + "." +
	    std::to_string((out.user_version / minor_version_scale) % minor_version_scale);
    return out;
}
} // namespace mime_geopackage
#endif
