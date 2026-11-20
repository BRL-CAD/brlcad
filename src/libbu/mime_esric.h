/* M I M E _ E S R I C . H
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
#ifndef BU_MIME_ESRIC_H
#define BU_MIME_ESRIC_H
#include "common.h"
#include <istream>
#include <string>
#include <utility>
#include "bu/mime_runtime.h"
#include "mime_json.h"
#include "mime_text.h"

namespace mime_esric {
using mime_json::json;
inline constexpr const char *xml_type = "application/vnd.esri.compact-cache+xml";
inline constexpr const char *json_type = "application/vnd.esri.compact-cache+json";
inline constexpr const char *zip_type = "application/vnd.esri.tile-package+zip";
inline constexpr const char *compact_v2 = "esriMapCacheStorageModeCompactV2";
struct profile {
    std::string storage, packet, rows, columns, tile_format, package_version;
    bool identified = false, consistent = true, incomplete = false, version_declared = false;
    bool append_fields(bu_mime_traits &traits) const
    {
	const std::pair<const char *, const std::string *> fields[] = {
	    {"esri-cache.storage-format", &storage}, {"esri-cache.packet-size", &packet},
	    {"esri-cache.tile-rows", &rows}, {"esri-cache.tile-columns", &columns},
	    {"esri-cache.tile-format", &tile_format}, {"esri-cache.package-version", &package_version}
	};
	for (const auto &field : fields)
	    if (!field.second->empty() && !bu_mime_traits_add(&traits, field.first, field.second->c_str())) return false;
	return true;
    }
    bool append_traits(bu_mime_traits &traits, bool archive = false) const
    {
	if (!identified) return false;
	if (!consistent) return true;
	return bu_mime_traits_add(&traits, "encoding", archive ? "binary" : "utf-8") &&
	    bu_mime_traits_add(&traits, "container", archive ? "zip" : "none") &&
	    bu_mime_traits_add(&traits, "esri-cache.package-version-declared", version_declared ? "true" : "false") &&
	    append_fields(traits) && (archive ? bu_mime_traits_add(&traits, "esri-cache.metadata-encoding", "utf-8") :
	    bu_mime_traits_add(&traits, "text.code-unit-width", "8"));
    }
};
inline std::string string(const json &object, const char *key)
{
    const auto field = object.find(key);
    if (field == object.end() || !field->is_string()) return {};
    const auto value = field->get<std::string>();
    return value.find('\0') == std::string::npos ? value : "";
}
inline std::string integer(const json &object, const char *key)
{
    const auto field = object.find(key);
    return field != object.end() && field->is_number_unsigned() ? field->dump() : "";
}
inline profile inspect(const json &object)
{
    profile out;
    if (!object.is_object() || string(object, "tileBundlesPath").empty()) return out;
    const auto storage = object.find("storageInfo"), tiles = object.find("tileInfo"), image = object.find("tileImageInfo");
    if (storage == object.end() || !storage->is_object() || tiles == object.end() || !tiles->is_object() ||
	image == object.end() || !image->is_object() || string(*storage, "storageFormat").empty()) return out;
    out.identified = true;
    out.storage = string(*storage, "storageFormat");
    out.packet = integer(*storage, "packetSize");
    out.rows = integer(*tiles, "rows");
    out.columns = integer(*tiles, "cols");
    out.tile_format = string(*image, "format");
    const auto version = object.find("version");
    out.version_declared = version != object.end();
    if (out.version_declared) {
	/* The owner specifies a JSON number. Preserve its value rather than
	 * mistaking the producing application's release for this revision. */
	out.consistent = version->is_number();
	if (version->is_number()) out.package_version = version->dump();
    }
    return out;
}
class declarations {
    mime_json::declarations<2> fields;
public:
    json::parser_callback_t callback()
    {
	return fields.callback([](int depth, const auto &parents, const std::string &key) {
	    if (depth == 1) return key == "version" || key == "tileBundlesPath" || key == "storageInfo" || key == "tileInfo" || key == "tileImageInfo";
	    if (depth != 2) return false;
	    if (parents[1] == "storageInfo") return key == "storageFormat" || key == "packetSize";
	    if (parents[1] == "tileInfo") return key == "rows" || key == "cols";
	    return parents[1] == "tileImageInfo" && key == "format";
	});
    }
    profile finish(const json &object) const
    {
	profile out = inspect(object);
	out.consistent = out.consistent && fields.unique();
	return out;
    }
};
inline profile inspect(std::istream &stream)
{
    declarations fields;
    return fields.finish(json::parse(stream, fields.callback(), false));
}
inline profile inspect(const unsigned char *data, size_t size, bool whole)
{
    const auto *begin = mime_json::object_begin(data, size);
    if (!begin) return {};
    declarations fields;
    const auto object = json::parse(begin, data + size, fields.callback(), false);
    if (object.is_discarded()) {profile out; out.incomplete = !whole; return out;}
    return fields.finish(object);
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
